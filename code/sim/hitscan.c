// code/sim/hitscan.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Hitscan — ray vs AABB slab test against the solids plus the arena floor.
// Returns the nearest hit distance (or GUN_RANGE if nothing) and the face
// normal, which impact marks and particles align to.
// ---------------------------------------------------------------------------

#define GUN_RANGE 200.0f

static float ray_aabb(v3 o, v3 d, v3 mn, v3 mx, v3 *nrm) {
  float t0 = 0.0f, t1 = GUN_RANGE;
  int ax0 = -1;
  for (int a = 0; a < 3; a++) {
    if (d.e[a] > -1e-8f && d.e[a] < 1e-8f) {
      if (o.e[a] < mn.e[a] || o.e[a] > mx.e[a]) return -1.0f;
      continue;
    }
    float inv = 1.0f / d.e[a];
    float ta = (mn.e[a] - o.e[a]) * inv, tb = (mx.e[a] - o.e[a]) * inv;
    if (ta > tb) { float tmp = ta; ta = tb; tb = tmp; }
    if (ta > t0) { t0 = ta; ax0 = a; }
    if (tb < t1) t1 = tb;
    if (t0 > t1) return -1.0f;
  }
  if (ax0 < 0) return -1.0f;  // ray starts inside: no sensible entry face
  *nrm = (v3){{0, 0, 0}};
  nrm->e[ax0] = d.e[ax0] > 0 ? -1.0f : 1.0f;
  return t0;
}

// `solid` (optional) reports WHICH box the ray entered, -1 for the arena floor
// plane or for nothing at all. The impact mark needs it: a decal has to be
// clipped to the face it is stamped on, and re-deriving the box from the hit
// point afterwards would be a second copy of this decision that cannot be made
// exact — `at = o + dir*t` is not bit-exactly on the face plane, and two solids
// may legitimately share a face plane, so an epsilon one ULP out silently
// stamps the mark on the wrong box.
static float trace_world(v3 o, v3 d, v3 *nrm, int *solid) {
  float best = GUN_RANGE;
  int si = -1;
  BP_RAY(i, o, d, GUN_RANGE) {
    v3 n;
    float t = ray_aabb(o, d, g_solids[i].min, g_solids[i].max, &n);
    if (t > 0 && t < best) { best = t; *nrm = n; si = i; }
  }
  if (d.y < -1e-8f) {  // arena floor plane at y = 0
    float t = -o.y / d.y;
    if (t > 0 && t < best) {
      float hx = o.x + d.x * t, hz = o.z + d.z * t;
      if (hx > -ARENA_HALF && hx < ARENA_HALF && hz > -ARENA_HALF && hz < ARENA_HALF) {
        best = t;
        *nrm = (v3){{0, 1, 0}};
        si = -1;                     // a nearer floor un-claims the box
      }
    }
  }
  if (solid) *solid = si;
  return best;
}

// --- penetration -----------------------------------------------------------
// A round that meets material keeps going while the PATH LENGTH it has spent
// inside solids stays under the weapon's budget. Path length is the whole idea:
// the same 0.60 m cover wall costs 0.60 m through the flat and 0.08 m through
// its arris, so shooting the CORNER of cover is nearly free and shooting the
// middle of it is nearly all of the budget — which is exactly the skill the
// player is asking for, and it needs no new level geometry to exist.

// How far the ray runs before it leaves this box. ray_aabb deliberately reports
// NO hit for an origin inside a box (the lean probe and the third-person pivot
// cast both depend on that), so the exit needs its own slab walk: the SMALLEST
// far-side crossing over the three axes.
static float aabb_exit(v3 o, v3 d, v3 mn, v3 mx) {
  float t1 = 1e30f;
  for (int a = 0; a < 3; a++) {
    if (d.e[a] > -1e-8f && d.e[a] < 1e-8f) continue;
    float inv = 1.0f / d.e[a];
    float ta = (mn.e[a] - o.e[a]) * inv, tb = (mx.e[a] - o.e[a]) * inv;
    float hi = ta > tb ? ta : tb;
    if (hi < t1) t1 = hi;
  }
  return t1 < 0.0f ? 0.0f : t1;
}

// ONE RULE, ONE NUMBER: a round crosses PEN_PATH of material and then stops.
// No falloff, no per-weapon budget — a man hit through 1.19 m of wall takes
// exactly what a man hit in the open takes, and at 1.21 m the round is in the
// wall. The reasoning is the shooter's side of it: at 7 m/s run speed a kill on
// somebody you cannot see is already the hardest shot in the game, and taxing
// it twice — once by having to guess the position, again by damage — meant the
// AR needed FIFTEEN rounds through a standard 0.60 m cover wall, i.e. it was
// not a mechanic, it was an animation.
//
// WHAT THIS COSTS, MEASURED, so nobody has to rediscover it: the sniper does
// 150 against 100 hp, so every wall it can now cross is a one-shot body kill,
// and over ten seeds 100% of the arena's non-shell solids are thinner than
// PEN_PATH on their narrow axis. Cover no longer stops a bolt gun; it only
// hides you. That was priced before it was chosen, not after.
//
// It is a PATH LENGTH and not a wall count, which is the one piece of the old
// model worth keeping: a 0.60 m slab costs 0.60 m through the flat, 0.08 m
// through its arris and 1.20 m at 60 degrees of obliquity — so the angle you
// take a wall at still decides whether the round arrives, and shooting the
// corner of cover is still the cheap way through.
#define PEN_PATH  1.20f    // metres of material ANY round may cross, full damage
#define PEN_EPS   0.0015f  // step past a face before re-testing containment
// The two below are COST bounds on the search, NOT rules of the mechanic, and
// they are sized so PEN_PATH is the only thing a player can ever run into.
// At 2 runs a round with budget left stopped because it met a THIRD wall, which
// is an invisible second rule on top of the one number above — and reachable
// now that the budget is 1.20 m rather than 0.62, since three 0.40 m crates fit
// inside it. Each unit is one pass over g_num_solids and shots are rare next to
// the LOS traces the AI runs every tick, so the headroom is close to free.
#define PEN_JOIN  8        // abutting solids one crossing may weld into one run
#define PEN_MAX   8        // separate runs one round may cross

// The far side of a CONTIGUOUS run of material entered at `o`, or -1 if the run
// contains something no round may cross. Solids abut and overlap all over this
// generator — a crate anchored to a wall, a jump step under a top box — and
// ray_aabb answers nothing for an origin already inside one, so exiting a single
// box and carrying on would hand the round everything stacked behind it for
// free. Walk instead: from the current point take the deepest exit of every
// solid containing it, step just past that, and repeat.
static float pen_span(v3 o, v3 d) {
  float t = 0.0f;
  for (int it = 0; it <= PEN_JOIN; it++) {   // <=: the last pass is the one
                                             // that finds open air
    v3 p = v3_add(o, v3_scale(d, t + PEN_EPS));
    float adv = t;
    BP_BOX(i, p.x, p.z, p.x, p.z) {
      const solid_t *s = &g_solids[i];
      if (p.x < s->min.x || p.x > s->max.x || p.y < s->min.y ||
          p.y > s->max.y || p.z < s->min.z || p.z > s->max.z) continue;
      if (!s->pen) return -1.0f;
      float ex = t + PEN_EPS + aabb_exit(p, d, s->min, s->max);
      if (ex > adv) adv = ex;
    }
    if (adv <= t) return t;                 // open air on the far side
    t = adv;
  }
  return -1.0f;   // still in material after PEN_JOIN welds: treat it as solid
}

// WHAT THE FALLOFF THAT USED TO LIVE HERE WAS FOR, and why the cliff that
// replaced it is not the trap that argument predicted. The old comment held
// that path length through a box is not monotone in the aim — that sliding
// toward an arris the chord first GROWS and only then shrinks — so a hard cut
// would leave a DEAD BAND: aim steps walking from a hit, through a stopped
// round, and back to a hit. It was worth believing and it did not reproduce.
// Swept with `aim`/`tap fire`, reading `bang`'s own pen= (seed 1337):
//   0.60 m cover wall, 17 aim points across it — monotone. Through at
//     pen 0.087/0.464/0.786/0.958/0.999/1.038/1.086/1.124/1.169, stuck from
//     1.209 on, and it never comes back.
//   1.40 m monolith, which is THICKER than PEN_PATH — its middle is solid at
//     every aim, and a corner band passes at pen 0.396 and 0.784.
// No pass-stop-pass anywhere in either. The second one is the better news: a
// block too thick to shoot through still has shootable CORNERS, so the arris
// skill the falloff was built to reward survives the cliff intact — it is a
// hard edge to find now instead of a gradient to feel out.

// The sim's tick rate. It lives up here, above the audio synth, because the
// synth is its second consumer: every reload sound derives its five event
// offsets from the weapon's reload_ticks, which is a duration in ticks. It
// used to be defined with the player sim 550 lines below, so the audio section
// carried `const float hz = 120.0f` — a private copy that a change to TICK_HZ
// would have desynchronised from the mechanic with no compiler error. That is
// the class of defect `parity` exists to catch everywhere else.
#define TICK_HZ        120
#define TICK_DT        (1.0f / TICK_HZ)

