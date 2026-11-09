// code/world/solid_index.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// THE SOLID INDEX. Every collision sweep, sight line, trace and foot clamp in
// this file is a loop over ALL the solids, which was right at 30-160 boxes and
// is not at the thousands a dressed tactical map carries. So a map with more
// than BP_MIN solids gets a uniform 4 m grid over the floor plan, each cell
// listing the solids whose footprint touches it, and the hot loops iterate the
// candidates a region or a ray can touch instead.
//
// THE LOOPS DO NOT CHANGE. The iterator hands out candidate indices in
// ASCENDING order — a bitmap, scanned low to high — so a loop that breaks on
// its first hit, or prefers the lower index on a tie, decides exactly what the
// full scan would have; it simply never visits a solid that could not matter.
// At or below BP_MIN the iterator walks 0..n-1 itself, so every map that
// existed before it runs the same code path, bit for bit.
// ---------------------------------------------------------------------------
#define BP_CELL    4.0f
#define BP_MIN     300
#define BP_MAXC    32                    // cells per side: up to a 128 m map
#define BP_IDX_MAX 131072
#define BP_WORDS   (MAX_SOLIDS / 32)
typedef struct {
  int      ok, nc;
  float    org;                          // world coordinate of cell 0's edge
  uint32_t start[BP_MAXC * BP_MAXC + 1];
  uint16_t idx[BP_IDX_MAX];
} bp_grid_t;
static bp_grid_t g_bp_all[SRV_LOBBIES];
#define g_bp (g_bp_all[g_lobby])

static int bp_c(float w) {
  int c = (int)floorf((w - g_bp.org) / BP_CELL);
  return c < 0 ? 0 : c >= g_bp.nc ? g_bp.nc - 1 : c;
}

static void bp_build(void) {
  g_bp.ok = 0;
  if (g_num_solids <= BP_MIN) return;
  float ext = ARENA_HALF + 1.0f;
  g_bp.org = -ext;
  g_bp.nc = (int)ceilf(2.0f * ext / BP_CELL);
  if (g_bp.nc > BP_MAXC) return;
  int nc = g_bp.nc, total = 0;
  static uint32_t cnt[BP_MAXC * BP_MAXC];
  memset(cnt, 0, sizeof cnt);
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *sd = &g_solids[i];
    for (int z = bp_c(sd->min.z); z <= bp_c(sd->max.z); z++)
      for (int x = bp_c(sd->min.x); x <= bp_c(sd->max.x); x++) { cnt[z * nc + x]++; total++; }
  }
  if (total > BP_IDX_MAX) return;        // stays off: the full scans still work
  g_bp.start[0] = 0;
  for (int c = 0; c < nc * nc; c++) g_bp.start[c + 1] = g_bp.start[c] + cnt[c];
  memset(cnt, 0, sizeof cnt);
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *sd = &g_solids[i];
    for (int z = bp_c(sd->min.z); z <= bp_c(sd->max.z); z++)
      for (int x = bp_c(sd->min.x); x <= bp_c(sd->max.x); x++) {
        int c = z * nc + x;
        g_bp.idx[g_bp.start[c] + cnt[c]++] = (uint16_t)i;
      }
  }
  g_bp.ok = 1;
}

typedef struct { int full, n, wi; uint32_t cur; uint32_t w[BP_WORDS]; } bp_iter_t;

static void bp_mark_cell(bp_iter_t *it, int c) {
  for (uint32_t k = g_bp.start[c]; k < g_bp.start[c + 1]; k++) {
    unsigned i = g_bp.idx[k];
    it->w[i >> 5] |= 1u << (i & 31);
  }
}
static void bp_iter_init(bp_iter_t *it) {
  it->n = g_num_solids;
  it->full = !g_bp.ok;
  it->wi = 0;
  it->cur = 0;
  if (!it->full) memset(it->w, 0, sizeof(uint32_t) * (size_t)((it->n + 31) >> 5));
}
static bp_iter_t *bp_box(bp_iter_t *it, float x0, float z0, float x1, float z1) {
  bp_iter_init(it);
  if (it->full) return it;
  for (int z = bp_c(z0); z <= bp_c(z1); z++)
    for (int x = bp_c(x0); x <= bp_c(x1); x++) bp_mark_cell(it, z * g_bp.nc + x);
  it->cur = it->w[0];
  return it;
}
// The cells a ray's floor-plan shadow crosses over [0, len] (Amanatides-Woo).
static bp_iter_t *bp_ray(bp_iter_t *it, v3 o, v3 d, float len) {
  bp_iter_init(it);
  if (it->full) return it;
  int nc = g_bp.nc, x = bp_c(o.x), z = bp_c(o.z);
  int sx = d.x > 0 ? 1 : -1, sz = d.z > 0 ? 1 : -1;
  float ax = fabsf(d.x), az = fabsf(d.z);
  float nbx = g_bp.org + (float)(x + (sx > 0)) * BP_CELL;
  float nbz = g_bp.org + (float)(z + (sz > 0)) * BP_CELL;
  float tmx = ax > 1e-7f ? (nbx - o.x) / d.x : 1e30f;
  float tmz = az > 1e-7f ? (nbz - o.z) / d.z : 1e30f;
  float tdx = ax > 1e-7f ? BP_CELL / ax : 1e30f, tdz = az > 1e-7f ? BP_CELL / az : 1e30f;
  for (int guard = 0; guard < 4 * BP_MAXC; guard++) {
    bp_mark_cell(it, z * nc + x);
    float tn = fminf(tmx, tmz);
    if (tn > len) break;
    if (tmx < tmz) { x += sx; tmx += tdx; } else { z += sz; tmz += tdz; }
    if (x < 0 || z < 0 || x >= nc || z >= nc) break;
  }
  it->cur = it->w[0];
  return it;
}
static int bp_next(bp_iter_t *it, int *out) {
  if (it->full) {
    if (it->wi >= it->n) return 0;
    *out = it->wi++;
    return 1;
  }
  int nw = (it->n + 31) >> 5;
  while (!it->cur) {
    if (++it->wi >= nw) return 0;
    it->cur = it->w[it->wi];
  }
  int b = __builtin_ctz(it->cur);
  it->cur &= it->cur - 1;
  *out = (it->wi << 5) + b;
  return 1;
}
// `for` sugar: BP_BOX(i, x0, z0, x1, z1) { ... } visits candidate solid i.
// break/continue behave as in the plain loop they replace.
#define BP_LOOP(i, INIT) \
  for (bp_iter_t i##_it, *i##_p = (INIT); i##_p; i##_p = NULL) \
    for (int i; bp_next(i##_p, &i);)
#define BP_BOX(i, x0, z0, x1, z1) BP_LOOP(i, bp_box(&i##_it, x0, z0, x1, z1))
#define BP_RAY(i, o, d, len)      BP_LOOP(i, bp_ray(&i##_it, o, d, len))

// Perimeter = 4 wall bases + 4 accent bands, always solids 0..7.
#define PERIM_SOLIDS 8
// Wall-hugging corridor kept free of cover: guaranteed rotation routes and
// four long sniper sightlines, one per wall.
#define RING_W 4.5f

static int map_box_free(v3 mn, v3 mx, float gap) {
  // Reject boxes near either spawn zone (the map is 180-deg rotationally
  // symmetric, so the mirrored spawn must stay clear too) or overlapping
  // existing solids by less than `gap`.
  for (int s = -1; s <= 1; s += 2) {
    float sz = SPAWN_POS.z * (float)s;
    if (mx.x > SPAWN_POS.x - 4 && mn.x < SPAWN_POS.x + 4 &&
        mx.z > sz - 4 && mn.z < sz + 4) return 0;
  }
  for (int i = PERIM_SOLIDS; i < g_num_solids; i++) {
    const solid_t *s = &g_solids[i];
    if (mx.x > s->min.x - gap && mn.x < s->max.x + gap &&
        mx.z > s->min.z - gap && mn.z < s->max.z + gap &&
        mx.y > s->min.y && mn.y < s->max.y) return 0;
  }
  return 1;
}

// 180-deg rotational symmetry about the arena center: every element lands
// twice, once at (x,z) and once at (-x,-z). Symmetric cover = fair duels.
static int map_sym_free(v3 mn, v3 mx, float gap) {
  return map_box_free(mn, mx, gap) &&
         map_box_free((v3){{-mx.x, mn.y, -mx.z}}, (v3){{-mn.x, mx.y, -mn.z}}, gap);
}

static void map_add_sym(v3 mn, v3 mx, v3 col) {
  map_add(mn, mx, col);
  map_add((v3){{-mx.x, mn.y, -mx.z}}, (v3){{-mn.x, mx.y, -mn.z}}, col);
}

static v3 col_tint(v3 c, float lo, float hi) {
  float t = rng_range(lo, hi);
  return (v3){{c.x * t, c.y * t, c.z * t}};
}

// --- A LONG WALL IS NOT ONE BOX --------------------------------------------
// Every wall in this arena was a perfect rectangular slab, and a slab is the one
// silhouette a box arena cannot afford to repeat: at twenty metres the ONLY
// thing that reads about a wall is its top line against the sky, and eight
// identical horizontal lines is what makes a procedural arena look authored by
// a for-loop.
//
// THE DAMAGE IS REAL GEOMETRY, NOT A DECAL, and that is the load-bearing
// decision. `g_solids` is the collision set AND the render mesh, so a chip that
// exists only in the picture is cover that is not there, a sightline that is not
// there, and a death nobody can explain. Every piece below is a solid: a
// stepped-down section is a ledge you can actually stand on, a broken stub is a
// step you can actually climb, and a breach is a hole you can actually see and
// shoot through — because there is nothing in it, in both meanings at once.
//
// The pieces ABUT EXACTLY rather than overlapping. Two collinear boxes that
// overlap share BOTH of their long side planes, and `mapcheck` counts exactly
// that as the z-fighting it is; at zero overlap every pair falls out of its area
// test and the two coincident faces are back-to-back, so one of them is always
// culled. That is the same argument mapcheck's own comment makes, used forwards.
//
// Only about a third of the walls are damaged. If every wall is broken then none
// of them is: the break stops reading as something that HAPPENED and becomes
// what a wall is.
static void wall_piece(v3 mn, v3 mx, int ax, float a, float b,
                       float y0, float y1, v3 col) {
  v3 p0 = mn, p1 = mx;
  if (ax) { p0.x = a; p1.x = b; } else { p0.z = a; p1.z = b; }
  p0.y = y0; p1.y = y1;
  map_add_sym(p0, p1, col);
}

// Rubble at the foot of a break. Real solids at ankle-to-knee height: they are
// what makes the break read as a COLLAPSE rather than as a wall that was built
// short, and they are a step up onto the stub.
//
// EVERY BLOCK GOES THROUGH map_sym_free, and that is not a placement nicety, it
// is the coplanarity proof. `mapcheck` only counts two same-facing faces that
// share a plane AND overlap with real area, so a solid that interpenetrates
// NOTHING cannot contribute a pair at all. The first version placed three blocks
// from independent draws and let them pile into each other and into the wall:
// two overlapping blocks then share a top plane whenever two draws from a 0.18 m
// range land within a millimetre of each other, which is about one pair in two
// hundred — and that is exactly the 98-over-500-maps the gate reported. Rejecting
// the overlap removes the whole class rather than making it rarer.
static void wall_rubble(v3 mn, v3 mx, int ax, float at, float dir, int count,
                        float hcap, v3 col) {
  float H = ARENA_HALF;
  float cz = ax ? (mn.z + mx.z) * 0.5f : (mn.x + mx.x) * 0.5f;
  for (int k = 0; k < count; k++) {
    // THIS COLLISION HAS NO STEP-UP. move_axis is a per-axis AABB sweep and
    // nothing in the file auto-climbs: a 0.25 m block is not a kerb you walk
    // over, it is a wall you have to JUMP, and a scatter of them in open ground
    // is the clutter that gets removed from competitive maps for exactly that
    // reason. Measured over 12000 ticks at 20 bots hard, three seeds, the first
    // version took pin_max from 21/21/21 to 29/39/22 — outside the documented
    // 21-36 band — and cut flank_arrive/flanks from 0.105 to 0.04.
    //
    // So rubble is not scatter, it is a STEP. Same height class as the crate
    // clusters' own jump-up crates, and it stays inside the wall's OWN
    // thickness so it never becomes a free-standing obstacle in a lane: a
    // player running the wall passes it the way they pass the wall, and a
    // player who wants to be on top of the stub has something to climb.
    float rw = rng_range(0.22f, 0.34f), rd = rng_range(0.22f, 0.30f);
    // Capped by whatever it fell OFF. A rubble block taller than the stub it
    // sits under is not a collapse, it is a second wall — and on the low height
    // class (a 1.0 m wall breaks to a 0.4 m stub) an uncapped 0.62 would be
    // exactly that.
    float rh = rng_range(0.34f, 0.58f);
    if (rh > hcap) rh = hcap;
    if (rh < 0.18f) continue;
    float side = rng_range(-0.06f, 0.06f);
    // Slots along the run, spaced wider than two blocks can ever be. dir 0 is
    // the breach, whose spill sits in front of the hole rather than beyond an
    // end, so its blocks step sideways from the centre instead.
    // ...AND IT SITS ON THE WALL'S OWN LINE, immediately at the break, rather
    // than being thrown clear of it. A block a metre past the end of a wall is
    // a jump obstacle placed exactly where a body rounding that end runs, and
    // measured it cost 8-23 ticks of pin per seed. Where it is now it is the
    // last course of the wall, collapsed — it cannot obstruct anything the wall
    // did not already obstruct, and it is a step onto the stub above it.
    float slot = dir != 0.0f ? dir * (0.30f + rw + 0.34f * (float)k)
                             : ((float)k - 0.5f * (float)(count - 1)) * 0.86f;
    v3 p0, p1;
    if (ax) {
      p0 = (v3){{at + slot - rw, 0.0f, cz + side - rd}};
      p1 = (v3){{at + slot + rw, rh,   cz + side + rd}};
    } else {
      p0 = (v3){{cz + side - rd, 0.0f, at + slot - rw}};
      p1 = (v3){{cz + side + rd, rh,   at + slot + rw}};
    }
    // The same corridor bound the step crates get: the RING_W lane is documented
    // cover-free, and a knee-high block a bullet stops is still cover.
    if (p0.x < -(H - RING_W) || p0.z < -(H - RING_W) ||
        p1.x > H - RING_W || p1.z > H - RING_W) continue;
    if (!map_sym_free(p0, p1, 0.012f)) continue;
    map_add_sym(p0, p1, col_tint(col, 0.84f, 0.96f));
  }
}

static void map_add_wall_sym(v3 mn, v3 mx, v3 col, int ax) {
  float lo = ax ? mn.x : mn.z, hi = ax ? mx.x : mx.z;
  float len = hi - lo, h = mx.y;
  uint32_t roll = rng_u32();
  // A short wall has no room for a story, and a LOW wall has no room for a hole
  // with material above and below it.
  int kind = (len >= 3.2f && (roll % 3u) == 0u) ? (int)((roll >> 5) % 3u) : -1;
  if (kind == 1 && h < 1.8f) kind = 2;
  switch (kind) {
  case 0: {  // BROKEN END: the last stretch has come down to a climbable stub
    float bl = rng_range(0.9f, 1.6f);
    // A FRACTION of the wall, never an absolute height. Authored absolutely it
    // was 0.78-1.02 against a height class that starts at 0.95, i.e. a "broken
    // end" three centimetres below the wall it broke off — no silhouette event
    // at all on half the walls in the arena.
    float hs = h * rng_range(0.34f, 0.52f);
    // NOT `far`. windef.h defines `far` and `near` as macros, so a local of
    // either name compiles clean on gcc and fails only the Windows build.
    int hi_end = (int)((roll >> 9) & 1u);
    float cut = hi_end ? hi - bl : lo + bl;
    if (hi_end) {
      wall_piece(mn, mx, ax, lo, cut, 0.0f, h, col);
      wall_piece(mn, mx, ax, cut, hi, 0.0f, hs, col);
      wall_rubble(mn, mx, ax, hi, 1.0f, 1, hs * 0.82f, col);
    } else {
      wall_piece(mn, mx, ax, lo, cut, 0.0f, hs, col);
      wall_piece(mn, mx, ax, cut, hi, 0.0f, h, col);
      wall_rubble(mn, mx, ax, lo, -1.0f, 1, hs * 0.82f, col);
    }
  } break;
  case 1: {  // BREACH: material above and below, nothing between. A real hole.
    float bw = rng_range(0.85f, 1.45f);
    float mid = rng_range(lo + bw * 0.6f + 0.5f, hi - bw * 0.6f - 0.5f);
    float a = mid - bw * 0.5f, b = mid + bw * 0.5f;
    // The SILL is a ledge you can mount and the LINTEL leaves a port you can
    // fire through standing or crouched. Deliberately the one place in this map
    // where something sits at eye level: a wall top at eye level is an ambiguous
    // peek, a hole at eye level is an invitation, and they are not the same rule.
    float y0 = rng_range(0.72f, 0.94f);
    float y1 = y0 + rng_range(0.62f, 0.86f);
    if (y1 > h - 0.28f) y1 = h - 0.28f;
    if (y1 - y0 < 0.35f) { y0 = h * 0.34f; y1 = h * 0.72f; }
    wall_piece(mn, mx, ax, lo, a, 0.0f, h, col);
    // THE HOLE IS RAGGED, and it is ragged in the collision set as well as in
    // the picture. One sill and one lintel across the whole span drew a clean
    // rectangle — which reads as a window somebody cut, not as masonry that
    // came out. Two or three columns, each with its own sill and lintel, step
    // both edges. They abut exactly, so mapcheck still sees zero coplanar area,
    // and every step is a real ledge: the low column is where you rest a rifle.
    int cols = 2 + (int)((roll >> 13) & 1u);
    for (int c = 0; c < cols; c++) {
      float ca = a + (b - a) * (float)c / (float)cols;
      float cb = a + (b - a) * (float)(c + 1) / (float)cols;
      float jy0 = y0 * rng_range(0.80f, 1.14f);
      float jy1 = y1 + (h - y1) * rng_range(-0.42f, 0.34f);
      if (jy1 > h - 0.14f) jy1 = h - 0.14f;
      if (jy1 - jy0 < 0.30f) jy1 = jy0 + 0.30f;
      if (jy1 > h - 0.14f) { jy1 = h - 0.14f; jy0 = jy1 - 0.30f; }
      // THE LINTEL MAY NOT LAND ON THE EYE, and this is the wall-height rule
      // one boundary out. Curated wall TOPS are never eye-level because an
      // eye-level top is an ambiguous peek; a hole whose TOP is eye-level is
      // worse, because leaning through it drops the eye 87 mm (1.620 standing,
      // 1.533 leaning, measured with `lean`) and 8.7 cm is enough to SHUT the
      // window. That inverts the one thing leaning is for, and nothing on the
      // wall tells you which way it will go. Surveyed over 200 seeds the free
      // draw put 62 % of breach cells in [1.50, 1.70] with the histogram
      // peaking exactly on 1.6, and 27 % of them flipped open/shut on the lean
      // alone. So the lintel is pushed clear of the band: high enough to be an
      // unambiguous stand-and-shoot slot where the wall has the room, and
      // otherwise low enough to be an unambiguous crouch-only murder hole.
      // Which side of the band it goes to is a COIN, not the side the draw
      // happened to fall on: keyed off the draw, 79 % of cells came out
      // crouch-only, and a map where every hole is the same kind of hole has
      // traded one ambiguity for one monotony.
      if (jy1 > 1.48f && jy1 < 1.78f)
        jy1 = (h - 0.14f >= 1.82f && ((roll >> 17) & 1u)) ? 1.82f : 1.44f;
      if (jy1 - jy0 < 0.30f) jy0 = jy1 - 0.30f;
      if (jy0 < 0.34f) jy0 = 0.34f;
      if (jy1 - jy0 < 0.30f) jy1 = jy0 + 0.30f;
      wall_piece(mn, mx, ax, ca, cb, 0.0f, jy0, col);
      wall_piece(mn, mx, ax, ca, cb, jy1, h, col);
    }
    wall_piece(mn, mx, ax, b, hi, 0.0f, h, col);
    wall_rubble(mn, mx, ax, mid, 0.0f, 1, y0 * 0.72f, col);
  } break;
  case 2: {  // STEPPED TOP: one section has come down to a lower course
    float cut = rng_range(lo + 1.1f, hi - 1.1f);
    // The drop must clear the height bands the arena curates: a top that lands
    // between them is the awkward in-between the whole height table exists to
    // avoid, so the low course is a fixed FRACTION of the wall rather than a
    // free draw.
    float hs = h * rng_range(0.46f, 0.62f);
    int hi_end = (int)((roll >> 9) & 1u);   // see the note on `far` above
    wall_piece(mn, mx, ax, lo, cut, 0.0f, hi_end ? h : hs, col);
    wall_piece(mn, mx, ax, cut, hi, 0.0f, hi_end ? hs : h, col);
    wall_rubble(mn, mx, ax, cut, hi_end ? 1.0f : -1.0f, 1, hs * 0.82f, col);
  } break;
  default: map_add_sym(mn, mx, col); break;
  }
}

// Curated arena themes — light, high-value surfaces so the dark bot
// silhouettes always pop; muted accents, one saturated landmark color for the
// mid structure. Deliberately no greens: the bots wear olive.
// `surf` is the SURFACE TREATMENT, and it is a separate axis from the colours:
// a theme is a palette AND a story about the weather that made it. SAND is a
// dry windswept floor, so it gets aeolian ripples and mica glitter; SLATE is
// overcast after rain, so it gets standing water; QUARTZ is dry warm concrete
// with dust and grit. Keeping it here rather than deriving it from the palette
// is what stops a puddle appearing in a desert — the one failure that would
// make every other detail read as decoration rather than as a place.
typedef enum { SURF_GRIT = 0, SURF_WET = 1, SURF_SAND = 2 } surf_kind_t;
typedef struct {
  v3 ground, wall, band_ew, band_n, band_s, cover, crate_a, crate_b, mid;
  int surf;
} theme_t;

static const theme_t THEMES[] = {
  // QUARTZ — warm daylight concrete, teal landmark
  {.ground = {{0.60f, 0.585f, 0.55f}}, .wall = {{0.67f, 0.655f, 0.62f}},
   .band_ew = {{0.33f, 0.34f, 0.36f}}, .band_n = {{0.29f, 0.48f, 0.65f}},
   .band_s = {{0.80f, 0.48f, 0.20f}},  .cover = {{0.64f, 0.44f, 0.34f}},
   .crate_a = {{0.63f, 0.56f, 0.45f}}, .crate_b = {{0.53f, 0.51f, 0.48f}},
   .mid = {{0.15f, 0.47f, 0.49f}}, .surf = SURF_GRIT},
  // SLATE — cool overcast grays, signal-orange landmark
  {.ground = {{0.555f, 0.57f, 0.595f}}, .wall = {{0.63f, 0.65f, 0.67f}},
   .band_ew = {{0.30f, 0.32f, 0.34f}}, .band_n = {{0.30f, 0.52f, 0.72f}},
   .band_s = {{0.82f, 0.50f, 0.20f}},  .cover = {{0.44f, 0.50f, 0.60f}},
   .crate_a = {{0.61f, 0.59f, 0.56f}}, .crate_b = {{0.50f, 0.48f, 0.46f}},
   .mid = {{0.83f, 0.42f, 0.15f}}, .surf = SURF_WET},
  // SAND — desert light, deep-blue landmark
  {.ground = {{0.63f, 0.595f, 0.52f}}, .wall = {{0.71f, 0.67f, 0.585f}},
   .band_ew = {{0.36f, 0.34f, 0.30f}}, .band_n = {{0.27f, 0.46f, 0.66f}},
   .band_s = {{0.78f, 0.42f, 0.16f}},  .cover = {{0.62f, 0.40f, 0.30f}},
   .crate_a = {{0.66f, 0.585f, 0.45f}}, .crate_b = {{0.56f, 0.51f, 0.43f}},
   .mid = {{0.21f, 0.36f, 0.58f}}, .surf = SURF_SAND},
};

static theme_t g_theme_all[SRV_LOBBIES];
#define g_theme (g_theme_all[g_lobby])

// ONE wind direction per arena. Every weather cue in the frame reads it — the
// lee-side sand drifts baked into the floor, the lean of a grass tuft, the
// heading of the ripple field — and a place only reads as a place if they all
// agree. It is HASHED FROM THE SEED rather than drawn from g_rng: a draw here
// would advance the map stream and re-roll the layout of every recorded seed,
// which is a byte-gate failure for a cosmetic term.
static float g_wind_ang_all[SRV_LOBBIES];
#define g_wind_ang (g_wind_ang_all[g_lobby])
static v3 wind_dir(void) {
  return (v3){{cosf(g_wind_ang), 0.0f, sinf(g_wind_ang)}};
}

// Ripple strength / glitter strength / standing-water level, per treatment.
// The grit term in WORLD_FS is unconditional — every floor has loose material on
// it — so what this table decides is only what is SPECIAL about a floor.
static v3 surf_params(void) {
  switch (g_theme.surf) {
  case SURF_SAND: return (v3){{1.00f, 1.00f, 0.00f}};
  // A wet floor has no ripples (water flattens loose material) and no glitter
  // (the film buries the crystal faces that would catch the sun) — the two are
  // off because the surface is telling a different story, not to save the ALU.
  case SURF_WET:  return (v3){{0.00f, 0.00f, 0.30f}};
  // Dry concrete gets NO ripples. It is a hard floor with loose material on it,
  // not a loose floor, and at a third of the sand's amplitude the two themes
  // measured the same to within a tenth of an LSB of texture energy — a
  // treatment that is not distinguishable from the one it is supposed to differ
  // from is a treatment that costs ALU for nothing. What separates grit is the
  // unconditional gravel term and a weaker glitter.
  default:        return (v3){{0.00f, 0.22f, 0.00f}};
  }
}

// One human's gun-RNG stream for a given map seed. Client and server BOTH
// reset a slot to this on a JOIN (net_srv_admit / the first-sync branch): the
// map seed alone is not enough, because a previous occupant of the slot may
// have advanced the server's stream — the new client would then predict a
// different recoil path for every shot of the whole session.
static uint64_t rng_gun_seed(uint32_t seed, int h) {
  // Human 0's term vanishes: stream [0] is bit-identical to the historical
  // g_rng_gun, which is what keeps every spray artefact valid.
  return (uint64_t)(seed ^ 0x6A75u) * 2654435761u + 1 +
         (uint64_t)h * 0x9E3779B97F4A7C15u;
}

