// code/harness/geometry_checks.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.

#include <unistd.h>

// --- figure geometry self-check (harness only) ------------------------------
// The two defects a box-stacked character model produces are invisible in a
// single screenshot but fatal in motion: a seam that opens into a hole at some
// joint angle, and two same-facing coplanar faces that flicker against each
// other. Both are structural, so both can be PROVEN absent instead of eyeballed:
//
//   closed + consistently wound: every triangle edge is shared by exactly one
//     other triangle, traversed in the opposite direction. A hole leaves an
//     edge used once; an inside-out triangle leaves a pair used twice in the
//     same direction. (Vertices shared between rings are bit-identical, so the
//     match is on exact float bits — no epsilon to tune.)
//   no z-fight risk: no two same-facing triangles are COPLANAR and overlapping.
//     Reported in two tiers, because depth precision at combat range is ~1e-7 m:
//     `zfight` = planes within 0.05 mm, i.e. genuinely fighting for the pixel
//     (what stacked coplanar boxes produce), and `near` = within 1 mm, an early
//     warning that two parts are grazing each other.
//   no shallow CROSSING: `cross`. The two tiers above test whether two surfaces
//     are PARALLEL and close. They are blind to the other half of the same
//     defect — two surfaces that are nearly parallel and actually pass THROUGH
//     each other — and blind by construction, not by tuning: `pd` is the worst
//     of the second triangle's three vertices, so a 90 mm barrel band whose far
//     end is 2.4 mm outside the fore-end it grazes measures 2.4 mm and falls out
//     of the 1 mm tier, however close the two surfaces are where they actually
//     meet. That is not a hypothetical. The sniper shipped with its barrel
//     crossing out through the top skin of its own fore-end at ~4 degrees,
//     31 mm behind the fore-end's nose, and both tiers read 0 the whole time.
//
//     Interpenetration is this model's whole placement discipline, so crossing
//     is not the defect — crossing SHALLOWLY is. Two surfaces meeting at a
//     steep angle leave an edge; two meeting at 4 degrees leave a seam whose
//     position is decided by facet phase, so a 12-gon and a 16-gon interleave
//     crest-through-flat for centimetres and the boundary renders as a ragged
//     speckle that swims when either part moves. Counted between 1.8 and 15
//     degrees: above that a crossing reads as the edge it is, and below it the
//     two planes are close enough to parallel that the pair is `zfight`/`near`
//     business and the intersection line is numerical noise anyway.
//
//     IT IS AN ADVISORY TIER LIKE `near`, NOT A ZERO GATE, and the reason is
//     geometric rather than a matter of tuning the numbers harder. Two curved
//     bodies that touch are TANGENT somewhere by construction — run a cylinder
//     through an equal cylinder and the surfaces are exactly parallel at the
//     point they graze — so a hand closed on a grip, a goggle lying on a skull
//     and a boot on its own sole all produce a handful of shallow pairs that
//     are correct. What the count is for is the DELTA: it moved 105 -> 95 on
//     the sniper's fore-end fix, and it is what pointed at the AR's gas tube
//     standing 0.6-1.2 mm out through the top of its own handguard for 130 mm.
//     `figv` prints each offender with its parts, angle, seam length and
//     position, which is the part that makes the number actionable.
//
// `figcheck N` walks N ticks of whatever the bot is doing and reports the worst
// tick, which turns "no holes, no flicker" into a regression test.
typedef struct { uint64_t lo, hi; int dir, tri; } fchk_edge_t;

static uint64_t fchk_vkey(const float *v) {
  uint64_t h = 1469598103934665603ull;
  for (int i = 0; i < 3; i++) {
    uint32_t b;
    memcpy(&b, &v[i], 4);
    if (b == 0x80000000u) b = 0;  // -0.0 and +0.0 are the same point
    h = (h ^ b) * 1099511628211ull;
  }
  return h;
}

static int fchk_cmp(const void *a, const void *b) {
  const fchk_edge_t *x = a, *y = b;
  if (x->lo != y->lo) return x->lo < y->lo ? -1 : 1;
  if (x->hi != y->hi) return x->hi < y->hi ? -1 : 1;
  return 0;
}

// 2D separating-axis overlap for two triangles in a shared plane.
static int fchk_tri2d(const float *a, const float *b) {
  for (int t = 0; t < 2; t++) {
    const float *p = t ? b : a, *q = t ? a : b;
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      float ex = p[j * 2] - p[i * 2], ey = p[j * 2 + 1] - p[i * 2 + 1];
      float ax = -ey, ay = ex;  // edge normal
      float amin = 1e30f, amax = -1e30f, bmin = 1e30f, bmax = -1e30f;
      for (int k = 0; k < 3; k++) {
        float dp = ax * p[k * 2] + ay * p[k * 2 + 1];
        float dq = ax * q[k * 2] + ay * q[k * 2 + 1];
        amin = fminf(amin, dp); amax = fmaxf(amax, dp);
        bmin = fminf(bmin, dq); bmax = fmaxf(bmax, dq);
      }
      if (amax <= bmin + 1e-6f || bmax <= amin + 1e-6f) return 0;
    }
  }
  return 1;
}

typedef struct { int tris, degen, open, flip, dup, zfight, near, cross; } fchk_res_t;

// How shallow a crossing has to be to count, and how much shared seam it needs.
// 15 degrees: a facet pair that meets steeper than this leaves an edge the eye
// reads as an edge, whatever the tessellation does. The sniper's was 4. The
// seam floor keeps a corner that clips a neighbouring facet by a hair out of
// the count — below half a millimetre of shared line there is nothing to see
// and nothing to swim.
#define FCHK_CROSS_COS 0.9659f   // cos 15 deg
#define FCHK_CROSS_SEG 0.0005f   // 0.5 mm of intersection segment
// Triangles the crossing tier keeps a bounding radius for. The pair loop is
// O(n^2) already; this is what lets a pair be rejected in one subtraction.
#define FCHK_TMAX 32768

// Do two triangles share a vertex, bit-exactly? Neighbours inside one chain do,
// and their "intersection" is the edge they were built to share. (== on floats
// is the right test here for the same reason fchk_vkey hashes the bits: ring
// vertices are copied, not recomputed. +0.0 == -0.0 compares equal, which is
// also what fchk_vkey folds together.)
static int fchk_share(const v3 *a, const v3 *b) {
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      if (a[i].x == b[j].x && a[i].y == b[j].y && a[i].z == b[j].z) return 1;
  return 0;
}

// Where a triangle crosses the OTHER triangle's plane, as an interval along the
// two planes' line of intersection. `d` is each vertex's signed distance to that
// plane; the vertex alone on its own side is the apex of the two edges that
// pierce it. Returns 0 when the triangle stays on one side (or lies IN the
// plane — that is the coplanar case, and it belongs to `zfight`).
static int fchk_cross_iv(const v3 *v, const float *d, v3 dir,
                         float *lo, float *hi) {
  int a = -1;
  for (int i = 0; i < 3 && a < 0; i++) {
    int j = (i + 1) % 3, k = (i + 2) % 3;
    if ((d[i] > 0.0f && d[j] <= 0.0f && d[k] <= 0.0f) ||
        (d[i] < 0.0f && d[j] >= 0.0f && d[k] >= 0.0f)) a = i;
  }
  if (a < 0) return 0;
  int j = (a + 1) % 3, k = (a + 2) % 3;
  v3 p1 = v3_add(v[a], v3_scale(v3_sub(v[j], v[a]), d[a] / (d[a] - d[j])));
  v3 p2 = v3_add(v[a], v3_scale(v3_sub(v[k], v[a]), d[a] / (d[a] - d[k])));
  float s = v3_dot(p1, dir), t = v3_dot(p2, dir);
  *lo = fminf(s, t);
  *hi = fmaxf(s, t);
  return 1;
}
static int g_fchk_v;  // `figcheck N v`: also locate every offending triangle

// Which authored PART a triangle belongs to. `figv` used to print bare triangle
// indices, and a bare index is not a diagnosis — identifying "3012" cost a round
// of commenting geometry out one piece at a time. `probe()` already records a
// float range per part for `figm`; this is the same table read the other way.
// --- tube-vs-mesh, for `vmtrig` --------------------------------------------
// The weapon publishes its thin parts as centrelines; the hand is a closed
// manifold in the scene batch. These answer "how far is this point from the
// hand's SURFACE, and is it inside", which is the only question that can see a
// 6.8 mm wire buried in a 24 mm finger. See the note at the vmtrig command.
#define TRIG_PROBE_STEP 0.001f   // 1 mm along a centreline: finer than any
                                 // feature either mesh has

// Closest point on a triangle to p (Ericson, Real-Time Collision Detection).
static float pt_tri_dist(v3 p, v3 a, v3 b, v3 c) {
  v3 ab = v3_sub(b, a), ac = v3_sub(c, a), ap = v3_sub(p, a);
  float d1 = v3_dot(ab, ap), d2 = v3_dot(ac, ap);
  v3 q;
  if (d1 <= 0.0f && d2 <= 0.0f) { q = a; goto done; }
  {
    v3 bp = v3_sub(p, b);
    float d3 = v3_dot(ab, bp), d4 = v3_dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) { q = b; goto done; }
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
      q = v3_add(a, v3_scale(ab, d1 / (d1 - d3)));
      goto done;
    }
    v3 cp = v3_sub(p, c);
    float d5 = v3_dot(ab, cp), d6 = v3_dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) { q = c; goto done; }
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
      q = v3_add(a, v3_scale(ac, d2 / (d2 - d6)));
      goto done;
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
      q = v3_add(b, v3_scale(v3_sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
      goto done;
    }
    float den = 1.0f / (va + vb + vc);
    q = v3_add(a, v3_add(v3_scale(ab, vb * den), v3_scale(ac, vc * den)));
  }
done:
  return v3_len(v3_sub(p, q));
}

// Moller-Trumbore, forward hits only — used as a parity counter, so the ray's
// direction is deliberately not axis-aligned and shared by every sample.
static int ray_tri_hit(v3 o, v3 d, v3 a, v3 b, v3 c) {
  v3 e1 = v3_sub(b, a), e2 = v3_sub(c, a), pv = v3_cross(d, e2);
  float det = v3_dot(e1, pv);
  if (det > -1e-9f && det < 1e-9f) return 0;
  float inv = 1.0f / det;
  v3 tv = v3_sub(o, a);
  float u = v3_dot(tv, pv) * inv;
  if (u < 0.0f || u > 1.0f) return 0;
  v3 qv = v3_cross(tv, e1);
  float vv = v3_dot(d, qv) * inv;
  if (vv < 0.0f || u + vv > 1.0f) return 0;
  return v3_dot(e2, qv) * inv > 1e-7f;
}

#define SCENE_V(F) ((v3){{g_scene[F], g_scene[(F) + 1], g_scene[(F) + 2]}})

// Signed distance from p to the hand mesh named by the probe ranges in `rng`.
// Negative = inside.
static float trig_mesh_sd(v3 p, const int *rng, int nrng) {
  const v3 rd = {{0.5773f, 0.5771f, 0.5775f}};   // no axis-aligned edge cases
  float dmin = 1e9f;
  int cross = 0;
  for (int q = 0; q < nrng; q += 2)
    for (int f = rng[q]; f + 2 * VERT_FLOATS < rng[q + 1]; f += 3 * VERT_FLOATS) {
      v3 A = SCENE_V(f), B = SCENE_V(f + VERT_FLOATS), C = SCENE_V(f + 2 * VERT_FLOATS);
      float d = pt_tri_dist(p, A, B, C);
      if (d < dmin) dmin = d;
      cross += ray_tri_hit(p, rd, A, B, C);
    }
  return (cross & 1) ? -dmin : dmin;
}

// The worst clearance between a tube (a sampled centreline plus a radius) and
// the hand mesh. Negative = the tube is inside the hand.
static float trig_tube_clear(const v3 *ax, int nax, float r,
                             const int *rng, int nrng) {
  float worst = 1e9f;
  for (int e = 0; e + 1 < nax; e++) {
    float seg = v3_len(v3_sub(ax[e + 1], ax[e]));
    int ns = (int)(seg / TRIG_PROBE_STEP) + 1;
    for (int u = 0; u <= ns; u++) {
      float sd = trig_mesh_sd(v3_lerp(ax[e], ax[e + 1], (float)u / (float)ns),
                              rng, nrng);
      if (sd - r < worst) worst = sd - r;
    }
  }
  return worst;
}

static const char *fchk_part(int tri) {
  int f = tri * VERT_FLOATS * 3;
  for (int i = 0; i < g_nprobe; i++)
    if (g_probe[i].name && f >= g_probe[i].v0 && f < g_probe[i].v1)
      return g_probe[i].name;
  return "?";
}

// --- ragdoll contact analysis, on the RENDERED corpse ------------------------
// "The bodies float" and "the bodies are inside the wall" are the two complaints
// a screenshot is worst at: a body 8 cm off the ground is the same picture as a
// body on it from every angle that is not exactly side-on, and a body inside a
// wall is invisible precisely because it is inside the wall. Both are
// quantities, and both live in the GEOMETRY, not in the joints — the boot sole
// hangs 75 mm below the ankle node and the helmet stands 90 mm above the head
// node, so a settle that puts the lowest JOINT on the floor is not a settle that
// puts the body on the floor. This reads the scene buffer the figure builder
// just filled.
//   clr  the lowest rendered vertex's clearance above the surface actually
//        under IT (not under the pelvis). 0 = resting, + = floating, - = sunk.
//   pen  the deepest any vertex is inside a solid, in metres.
//   nin  how many vertices are more than RAGX_IN inside a solid.
#define RAGX_IN 0.010f
// `span` and `hgt` are the rendered figure's own bounding box: the longest
// horizontal extent and the vertical one. A man is 1.79 m tall and about 0.55
// deep, so a corpse lying flat measures span ~1.7-1.9 / hgt ~0.4-0.5, and a
// corpse folded into a ball measures span ~0.9 / hgt ~0.8. That difference is
// the whole "does it read as a body or as a lump" question, and it is the one
// thing a settled-corpse screenshot is genuinely bad at — every camera angle
// foreshortens one of the two.
// `hang` is the one number the two complaints actually share, and a whole-body
// minimum cannot see it: a corpse lying half on a platform and half out over the
// void has vertices touching concrete AND vertices 0.55 m up in the air, so
// `clr` reads 0.00 and the picture is a floating body. The figure's vertices are
// bucketed into RAGX_CELL columns and each column contributes its own lowest
// vertex's clearance; `hang` is the worst of those. A body lying on flat ground
// reads ~0.05 (the thickest part of the torso over its own footprint), a body
// half over a lip reads the lip's height.
#define RAGX_CELL 0.30f
#define RAGX_NCELL 256
typedef struct {
  float clr, bclr, pen, bpen, span, hgt, hang;
  int nin, nv, fclr, fbclr, fpen, fbpen;
} ragx_t;

// The surface a point at (x,z) would fall onto: the highest solid top at or
// below it, floor otherwise. One per vertex is O(verts x solids) and this is a
// harness-only proof, so it stays a plain scan rather than growing a grid.
static float ragx_support(v3 p) { return rag_support_eps(p, 1e-4f); }

// The dropped rifle's magazine reaches further down than any part of a man, so
// a single minimum over the whole figure answers "is the RIFLE on the ground"
// and never "is the BODY on the ground" — which is the question the complaint
// was about. `bclr` is the same minimum with the weapon's own vertices left
// out.
static int ragx_is_gun(int f) {
  for (int i = 0; i < g_nprobe; i++)
    if (g_probe[i].name && f >= g_probe[i].v0 && f < g_probe[i].v1)
      return !strcmp(g_probe[i].name, "gun");
  return 0;
}

// THE FOLLOWED BODY, once (rag / ragx / ragsoak all needed it and two of them
// carried their own copy). After the entity->corpse handover the staged body
// is the youngest corpse this BOT owns; "youngest of any" is only the fallback
// for pre-owner staged scripts, and at 20 bots hard — a kill every ~32 ticks —
// it silently re-targeted the follow mid-observation.
static const corpse_t *harness_corpse_of(int bi) {
  const corpse_t *best = NULL;
  int owned = 0, life = -1;
  for (int c = 0; c < MAX_CORPSES; c++) {
    if (!g_corpses[c].used) continue;
    int m = g_corpses[c].owner == bi + 1;      // bot index -> entity id
    if (owned && !m) continue;
    if (m && !owned) { owned = 1; life = -1; }
    if (life < 0 || g_corpses[c].life > life) {
      life = g_corpses[c].life; best = &g_corpses[c];
    }
  }
  return best;
}

// ...and the probe-tier draw of one, also once. Deliberately NOT the live
// scene path: dissolve is forced off (a body mid-burst has holes in it, and
// the clearance/penetration questions are about the whole mesh) and the
// profile is pinned to CORPSE_K, the tier a player actually walks up to.
static void harness_draw_corpse(const corpse_t *cp) {
  g_fig_diss = 0.0f;
  g_fig_kcap = CORPSE_K;
  death_t dth = rag_death(&cp->rag, cp->slack, cp->slack, 1.0f);
  scene_draw_figure(&cp->anim, cp->pos, cp->yaw, &dth, 1.0f, 0.0f,
                    (v3){{1, 1, 1}}, cp->pal, cp->wpn, cp->gs);
  g_fig_kcap = 0;
}

static ragx_t ragx_scan(int v0) {
  ragx_t r = {1e9f, 1e9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, -1, -1, -1, -1};
  v3 lo3 = {{1e9f, 1e9f, 1e9f}}, hi3 = {{-1e9f, -1e9f, -1e9f}};
  // Open-addressed column table: key is the (x,z) cell, value the lowest
  // clearance seen in it. A figure fills ~40 columns, so 256 slots never
  // saturate; if it ever did, the extra vertices fold into a neighbour and can
  // only UNDER-report, which is the safe direction for a proof.
  static int32_t ck[RAGX_NCELL];
  static float   cv[RAGX_NCELL];
  for (int i = 0; i < RAGX_NCELL; i++) { ck[i] = INT32_MIN; cv[i] = 1e9f; }
  for (int f = v0; f + VERT_FLOATS <= g_scene_floats; f += VERT_FLOATS) {
    v3 p = {{g_scene[f], g_scene[f + 1], g_scene[f + 2]}};
    r.nv++;
    for (int a = 0; a < 3; a++) {
      if (p.e[a] < lo3.e[a]) lo3.e[a] = p.e[a];
      if (p.e[a] > hi3.e[a]) hi3.e[a] = p.e[a];
    }
    float c = p.y - ragx_support(p);
    {
      // UNSIGNED all the way through: the arena spans negative coordinates, so
      // a cell index is routinely negative and `gx * 73856093` on a signed int
      // is overflow — UB, and UBSan says so on the first frame.
      uint32_t gx = (uint32_t)(int32_t)floorf(p.x / RAGX_CELL);
      uint32_t gz = (uint32_t)(int32_t)floorf(p.z / RAGX_CELL);
      int32_t key = (int32_t)(gx * 73856093u ^ gz * 19349663u);
      if (key == INT32_MIN) key = 0;
      int h = (int)((uint32_t)key % RAGX_NCELL);
      for (int t = 0; t < RAGX_NCELL; t++) {
        int q = (h + t) % RAGX_NCELL;
        if (ck[q] == INT32_MIN) { ck[q] = key; cv[q] = c; break; }
        if (ck[q] == key) { if (c < cv[q]) cv[q] = c; break; }
      }
    }
    if (c < r.clr) { r.clr = c; r.fclr = f; }
    if (c < r.bclr && !ragx_is_gun(f)) { r.bclr = c; r.fbclr = f; }
    for (int i = 0; i < g_num_solids; i++) {
      const solid_t *b = &g_solids[i];
      if (p.x <= b->min.x || p.x >= b->max.x || p.y <= b->min.y ||
          p.y >= b->max.y || p.z <= b->min.z || p.z >= b->max.z) continue;
      float d = fminf(fminf(p.x - b->min.x, b->max.x - p.x),
                fminf(fminf(p.y - b->min.y, b->max.y - p.y),
                      fminf(p.z - b->min.z, b->max.z - p.z)));
      if (d > r.pen) { r.pen = d; r.fpen = f; }
      // The rifle is 900 mm long and no joint radius bounds it, so it is always
      // the deepest thing in a wall and it buries the number the complaint is
      // actually about. `bpen` is the BODY's own worst penetration.
      if (d > r.bpen && !ragx_is_gun(f)) { r.bpen = d; r.fbpen = f; }
      if (d > RAGX_IN) { r.nin++; break; }
    }
  }
  if (r.clr > 1e8f) { r.clr = 0.0f; r.fclr = -1; }
  if (r.bclr > 1e8f) { r.bclr = r.clr; r.fbclr = r.fclr; }
  if (r.nv) {
    r.span = fmaxf(hi3.x - lo3.x, hi3.z - lo3.z);
    r.hgt = hi3.y - lo3.y;
    for (int i = 0; i < RAGX_NCELL; i++)
      if (ck[i] != INT32_MIN && cv[i] > r.hang) r.hang = cv[i];
  }
  return r;
}

// A float index back to the PART that emitted it. A bare vertex offset is not a
// diagnosis; "the worst sinking vertex is `outsole`" is one. Same probe table
// `figv` and `figm` read, so it costs nothing unless probes are on.
static const char *ragx_part(int f) {
  if (f < 0) return "-";
  for (int i = 0; i < g_nprobe; i++)
    if (g_probe[i].name && f >= g_probe[i].v0 && f < g_probe[i].v1)
      return g_probe[i].name;
  return "?";
}

// Analyse whatever triangle soup currently sits in g_scene.
static fchk_res_t fchk_scene(void) {
  fchk_res_t r = {0};
  int nt = g_scene_floats / (VERT_FLOATS * 3);
  r.tris = nt;
  static fchk_edge_t ed[65536];
  static float trad[FCHK_TMAX];   // circumradius about the centroid, for the
                                  // crossing tier's one-subtraction reject
  int ne = 0;
  for (int i = 0; i < nt; i++) {
    const float *t = &g_scene[i * VERT_FLOATS * 3];
    v3 p0 = {{t[0], t[1], t[2]}};
    v3 p1 = {{t[VERT_FLOATS], t[VERT_FLOATS + 1], t[VERT_FLOATS + 2]}};
    v3 p2 = {{t[VERT_FLOATS * 2], t[VERT_FLOATS * 2 + 1], t[VERT_FLOATS * 2 + 2]}};
    if (i < FCHK_TMAX) {
      v3 c = v3_scale(v3_add(v3_add(p0, p1), p2), 1.0f / 3.0f);
      trad[i] = fmaxf(fmaxf(v3_len(v3_sub(p0, c)), v3_len(v3_sub(p1, c))),
                      v3_len(v3_sub(p2, c)));
    }
    v3 n = v3_cross(v3_sub(p1, p0), v3_sub(p2, p0));
    if (v3_len(n) < 1e-9f) {
      r.degen++;
      if (g_fchk_v) printf("  DEGEN  %s tri %d\n", fchk_part(i), i);
      if (i < FCHK_TMAX) trad[i] = -1.0f;   // degenerate: never a crossing pair
      continue;
    }
    uint64_t k[3] = {fchk_vkey(&t[0]), fchk_vkey(&t[VERT_FLOATS]),
                     fchk_vkey(&t[VERT_FLOATS * 2])};
    for (int e = 0; e < 3; e++) {
      uint64_t a = k[e], b = k[(e + 1) % 3];
      if (ne < (int)(sizeof ed / sizeof *ed))
        ed[ne++] = (fchk_edge_t){a < b ? a : b, a < b ? b : a, a < b ? 1 : -1, i};
    }
  }
  qsort(ed, ne, sizeof *ed, fchk_cmp);
  for (int i = 0; i < ne;) {
    int j = i;
    while (j < ne && !fchk_cmp(&ed[i], &ed[j])) j++;
    int n = j - i, sum = 0;
    for (int k2 = i; k2 < j; k2++) sum += ed[k2].dir;
    // Report the PART, not a bare index: an open edge is a hole in a specific
    // chain and the whole job is finding which one.
    if (n == 1 && g_fchk_v)
      printf("  OPEN   %s tri %d\n", fchk_part(ed[i].tri), ed[i].tri);
    if (n == 1) r.open++;
    else if (n > 2) r.dup++;
    else if (sum != 0) r.flip++;  // both halves wound the same way
    i = j;
  }
  // Coplanar overlap: same facing, planes within 1 mm, projections overlapping.
  for (int i = 0; i < nt; i++) {
    const float *ti = &g_scene[i * VERT_FLOATS * 3];
    v3 a0 = {{ti[0], ti[1], ti[2]}};
    v3 a1 = {{ti[VERT_FLOATS], ti[VERT_FLOATS + 1], ti[VERT_FLOATS + 2]}};
    v3 a2 = {{ti[VERT_FLOATS * 2], ti[VERT_FLOATS * 2 + 1], ti[VERT_FLOATS * 2 + 2]}};
    v3 na = v3_cross(v3_sub(a1, a0), v3_sub(a2, a0));
    if (v3_len(na) < 1e-9f) continue;
    na = v3_norm(na);
    v3 ca = v3_scale(v3_add(v3_add(a0, a1), a2), 1.0f / 3.0f);
    v3 u = v3_norm(v3_sub(a1, a0)), w = v3_cross(na, u);
    for (int j = i + 1; j < nt; j++) {
      const float *tj = &g_scene[j * VERT_FLOATS * 3];
      v3 b0 = {{tj[0], tj[1], tj[2]}};
      v3 b1 = {{tj[VERT_FLOATS], tj[VERT_FLOATS + 1], tj[VERT_FLOATS + 2]}};
      v3 b2 = {{tj[VERT_FLOATS * 2], tj[VERT_FLOATS * 2 + 1], tj[VERT_FLOATS * 2 + 2]}};
      v3 cb = v3_scale(v3_add(v3_add(b0, b1), b2), 1.0f / 3.0f);
      float cd = v3_len(v3_sub(cb, ca));
      if (cd > 0.45f) continue;
      v3 nb = v3_cross(v3_sub(b1, b0), v3_sub(b2, b0));
      if (v3_len(nb) < 1e-9f) continue;
      nb = v3_norm(nb);
      float ndot = v3_dot(na, nb);
      // SHALLOW CROSSING. Ordered cheapest-first, because everything below the
      // last test runs on a pair that already looks like the defect: the angle
      // is one dot product that is needed anyway, the reach test is one
      // subtraction against radii the prepass already measured, and only then
      // does a pair pay for two plane solves.
      if (fabsf(ndot) >= FCHK_CROSS_COS && fabsf(ndot) < 0.9995f &&
          i < FCHK_TMAX && j < FCHK_TMAX && cd <= trad[i] + trad[j]) {
        v3 av[3] = {a0, a1, a2}, bv[3] = {b0, b1, b2};
        if (!fchk_share(av, bv)) {
          float db[3], da[3];
          for (int k2 = 0; k2 < 3; k2++) {
            db[k2] = v3_dot(v3_sub(bv[k2], a0), na);   // B's vertices vs A
            da[k2] = v3_dot(v3_sub(av[k2], b0), nb);   // A's vertices vs B
          }
          v3 dir = v3_cross(na, nb);
          float dl = v3_len(dir);
          float alo, ahi, blo, bhi;
          // dl == 0 is exactly parallel, which cannot cross — that pair is
          // `zfight`/`near` territory and falls through to the code below.
          if (dl > 1e-7f) {
            dir = v3_scale(dir, 1.0f / dl);
            if (fchk_cross_iv(av, da, dir, &alo, &ahi) &&
                fchk_cross_iv(bv, db, dir, &blo, &bhi)) {
              float seg = fminf(ahi, bhi) - fmaxf(alo, blo);
              if (seg >= FCHK_CROSS_SEG) {
                r.cross++;
                if (g_fchk_v)
                  printf("  CROSS  %s/%s tri %d/%d ang=%.1fdeg seg=%.2fmm "
                         "at (%.3f %.3f %.3f)\n",
                         fchk_part(i), fchk_part(j), i, j,
                         (double)(acosf(fminf(fabsf(ndot), 1.0f)) / DEG2RAD),
                         (double)(seg * 1000.0f),
                         (double)ca.x, (double)ca.y, (double)ca.z);
              }
            }
          }
        }
      }
      if (ndot < 0.9995f) continue;                        // not same-facing
      float pd = fmaxf(fmaxf(fabsf(v3_dot(v3_sub(b0, a0), na)),
                             fabsf(v3_dot(v3_sub(b1, a0), na))),
                       fabsf(v3_dot(v3_sub(b2, a0), na)));
      if (pd > 0.001f) continue;
      float pa[6], pb[6];
      v3 av[3] = {a0, a1, a2}, bv[3] = {b0, b1, b2};
      for (int k2 = 0; k2 < 3; k2++) {
        pa[k2 * 2] = v3_dot(v3_sub(av[k2], ca), u);
        pa[k2 * 2 + 1] = v3_dot(v3_sub(av[k2], ca), w);
        pb[k2 * 2] = v3_dot(v3_sub(bv[k2], ca), u);
        pb[k2 * 2 + 1] = v3_dot(v3_sub(bv[k2], ca), w);
      }
      if (fchk_tri2d(pa, pb)) {
        if (pd < 0.00005f) r.zfight++; else r.near++;
        // Print the TIER and the measured gap, not just "zfight" for both: a
        // 0.02 mm pair and a 0.9 mm pair need different fixes, and the gap is
        // what says at which range the depth buffer stops separating them.
        if (g_fchk_v)
          printf("  %s %s/%s tri %d/%d gap=%.3fmm at (%.3f %.3f %.3f) "
                 "n=(%.2f %.2f %.2f)\n",
                 pd < 0.00005f ? "ZFIGHT" : "near  ",
                 fchk_part(i), fchk_part(j), i, j, (double)(pd * 1000.0f),
                 (double)ca.x, (double)ca.y, (double)ca.z,
                 (double)na.x, (double)na.y, (double)na.z);
      }
    }
  }
  return r;
}

