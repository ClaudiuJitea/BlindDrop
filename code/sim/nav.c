// code/sim/nav.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// GRID PATHING. Bots steer locally and have no route planner of their own,
// which is fine in the open: a fight keeps finding them. Two places it is not:
// LAST MAN STANDING's endgame (two survivors must cross a map to meet, and a
// straight walk at a rival behind a wall slides along that wall forever), and
// the large tactical maps, a town of corridors where almost every destination
// is round a corner.
//
// So: a 1 m grid over the arena, built lazily once per (seed, map). Each cell
// records whether a standing body fits there and at what LEVEL — always the
// floor on the open maps, which have no step-up (byte-for-byte the floor-only
// grid the hunt was measured on); the lowest level a body fits at on a stepped
// map, so stairs and raised platforms carry their own height. A move between
// neighbours climbs at most STEP_H and drops at most NAV_DROP. A breadth-first
// distance field from a goal cell is walked downhill, string-pulled to the
// farthest cell the body can reach in a straight line.
// ---------------------------------------------------------------------------
#define NAV_MAX 112                       // cells per side, 1 m: up to a 112 m map
#define NAV_MIN_LEAD 1.1f                 // > the investigation's 0.9 m arrival
#define NAV_DROP 2.6f                     // a ledge a body will step off
typedef struct {
  uint8_t  walk[NAV_MAX * NAV_MAX];
  float    lvl[NAV_MAX * NAV_MAX];
  uint32_t key;
  int      keymap, n, ok;
} nav_grid_t;
static nav_grid_t g_nav_all[SRV_LOBBIES];
#define g_nav (g_nav_all[g_lobby])

static float nav_cellsz(void) { return 2.0f * ARENA_HALF / (float)g_nav.n; }
static int nav_cell(float w) {
  int c = (int)floorf((w + ARENA_HALF) / nav_cellsz());
  return c < 0 ? 0 : c >= g_nav.n ? g_nav.n - 1 : c;
}
static float nav_centre(int c) { return -ARENA_HALF + ((float)c + 0.5f) * nav_cellsz(); }

static void nav_ready(void) {
  if (g_nav.ok && g_nav.key == g_map_seed && g_nav.keymap == g_map_id) return;
  int n = (int)(2.0f * ARENA_HALF + 0.5f);
  g_nav.n = n < 1 ? 1 : n > NAV_MAX ? NAV_MAX : n;
  for (int z = 0; z < g_nav.n; z++)
    for (int x = 0; x < g_nav.n; x++) {
      int i = z * g_nav.n + x;
      v3 c = {{nav_centre(x), 0.0f, nav_centre(z)}};
      g_nav.lvl[i] = 0.0f;
      g_nav.walk[i] = (uint8_t)aabb_fits(c, 0.45f, HEIGHT_STAND);
      if (g_nav.walk[i] || STEP_H <= 0.0f) continue;
      // Stepped map, floor blocked: the lowest box top under the centre that
      // a body fits on is the cell's level.
      float best = 1e9f;
      BP_BOX(k, c.x, c.z, c.x, c.z) {
        const solid_t *sd = &g_solids[k];
        if (c.x < sd->min.x || c.x > sd->max.x || c.z < sd->min.z || c.z > sd->max.z) continue;
        float y = sd->max.y;
        if (y >= best || y > (g_map_id == MAP_COMPOUND_ID ? 7.0f : 6.0f)) continue;
        if (aabb_fits((v3){{c.x, y + 0.001f, c.z}}, 0.45f, HEIGHT_STAND)) best = y;
      }
      if (best < 1e8f) { g_nav.walk[i] = 1; g_nav.lvl[i] = best; }
    }
  g_nav.key = g_map_seed;
  g_nav.keymap = g_map_id;
  g_nav.ok = 1;
}

// May a body step from cell a to neighbour b?
static int nav_step_ok(int a, int b) {
  if (!g_nav.walk[a] || !g_nav.walk[b]) return 0;
  float d = g_nav.lvl[b] - g_nav.lvl[a];
  return d <= STEP_H + 0.01f && -d <= NAV_DROP;
}
// A diagonal needs both orthogonal cells open and on its level (within a step),
// so a path never cuts a corner — or a stair edge — the body cannot.
static int nav_diag_ok(int cx, int cz, int x, int z) {
  int n = g_nav.n, c = cz * n + cx, a = cz * n + x, b = z * n + cx, d = z * n + x;
  if (!g_nav.walk[a] || !g_nav.walk[b]) return 0;
  float lo = g_nav.lvl[c], hi = lo;
  float l3[3] = {g_nav.lvl[a], g_nav.lvl[b], g_nav.lvl[d]};
  for (int k = 0; k < 3; k++) { lo = fminf(lo, l3[k]); hi = fmaxf(hi, l3[k]); }
  return hi - lo <= STEP_H + 0.01f;
}

// The walkable cell nearest a world point, searching outward a few rings, at a
// level within reach of the point's own height — a body hugging a wall, or
// standing on a crate, sits over a blocked (or a different-level) cell.
static int nav_snap(v3 p) {
  int n = g_nav.n, cx = nav_cell(p.x), cz = nav_cell(p.z);
  for (int r = 0; r <= 4; r++)
    for (int dz = -r; dz <= r; dz++)
      for (int dx = -r; dx <= r; dx++) {
        if (abs(dx) != r && abs(dz) != r) continue;   // the ring only
        int x = cx + dx, z = cz + dz;
        if (x < 0 || z < 0 || x >= n || z >= n) continue;
        int i = z * n + x;
        if (g_nav.walk[i] && (STEP_H <= 0.0f || fabsf(g_nav.lvl[i] - p.y) <= 0.8f))
          return i;
      }
  return -1;
}

// BFS distance field to `goal`, over moves INTO the goal's direction (a cell's
// distance is the steps from it to the goal, so the edge tested is neighbour ->
// cell). 0xFFFF = no route.
static void nav_field(int goal, uint16_t *dist) {
  static int q[NAV_MAX * NAV_MAX];
  int n = g_nav.n;
  for (int i = 0; i < n * n; i++) dist[i] = 0xFFFF;
  int h = 0, t = 0;
  dist[goal] = 0;
  q[t++] = goal;
  while (h < t) {
    int c = q[h++], cx = c % n, cz = c / n;
    for (int dz = -1; dz <= 1; dz++)
      for (int dx = -1; dx <= 1; dx++) {
        if (!dx && !dz) continue;
        int x = cx + dx, z = cz + dz;
        if (x < 0 || z < 0 || x >= n || z >= n) continue;
        int nb = z * n + x;
        if (dist[nb] != 0xFFFF || !nav_step_ok(nb, c)) continue;
        if (dx && dz && !nav_diag_ok(cx, cz, x, z)) continue;
        dist[nb] = (uint16_t)(dist[c] + 1);
        q[t++] = nb;
      }
  }
}

// A few distance fields per arena, by goal cell: twenty bots heading for a
// handful of places (each other, a noise, a site) share them, and a field is a
// full BFS over up to 12,544 cells.
#define NAV_FIELDS 8
typedef struct { int goal; uint32_t key; int keymap; long tick; uint16_t d[NAV_MAX * NAV_MAX]; } nav_field_t;
static nav_field_t g_navf_all[SRV_LOBBIES][NAV_FIELDS];
static const uint16_t *nav_field_for(int goal) {
  nav_field_t *f = g_navf_all[g_lobby], *slot = &f[0];
  for (int k = 0; k < NAV_FIELDS; k++) {
    if (f[k].goal == goal && f[k].key == g_map_seed && f[k].keymap == g_map_id &&
        f[k].tick > 0) {
      f[k].tick = g_tick + 1;          // most recently used
      return f[k].d;
    }
    if (f[k].tick < slot->tick) slot = &f[k];
  }
  nav_field(goal, slot->d);
  slot->goal = goal; slot->key = g_map_seed; slot->keymap = g_map_id;
  slot->tick = g_tick + 1;
  return slot->d;
}

// Can a body walk the straight segment a->b? Swept as body-sized samples every
// 25 cm. On the open maps that is the floor test with a 0.5 m margin — the bots'
// wall-band steering shaves the wish inside it, and a line grazing a wall end at
// 0.36 m was measured to stall a bot on the corner it was routed round. On a
// stepped map every sample must also sit on a cell it can step to from the last,
// and the body is tested one step HIGHER than that cell's level, so a stair it
// will climb passes while a crate or a wall does not.
static int nav_clear(v3 a, v3 b) {
  float dx = b.x - a.x, dz = b.z - a.z, l = sqrtf(dx * dx + dz * dz);
  int ns = (int)(l / 0.25f) + 1;
  int prev = nav_snap(a);
  for (int i = 1; i <= ns; i++) {
    float t = (float)i / (float)ns;
    v3 p = {{a.x + dx * t, 0.0f, a.z + dz * t}};
    if (STEP_H <= 0.0f) {
      if (!aabb_fits(p, 0.50f, HEIGHT_STAND)) return 0;
      continue;
    }
    int c = nav_cell(p.z) * g_nav.n + nav_cell(p.x);
    if (prev < 0 || (c != prev && !nav_step_ok(prev, c))) return 0;
    p.y = g_nav.lvl[c] + STEP_H + 0.01f;
    if (!aabb_fits(p, 0.36f, HEIGHT_STAND - STEP_H)) return 0;
    prev = c;
  }
  return 1;
}

// Where a body at `from` should walk next toward the field's goal: the FARTHEST
// of the next `steps` downhill cells it can reach in a straight line, never
// inside the investigation's arrival radius (the hunt re-issues a point every
// quarter second, and a point under 0.9 m reads as "arrived" — measured, a bot
// at a wall end sat in that loop for a round). Returns 0 when there is no route.
static int nav_next(const uint16_t *dist, v3 from, int steps, v3 *out) {
  int n = g_nav.n, c = nav_snap(from);
  if (c < 0 || dist[c] == 0xFFFF) return 0;
  v3 first = {{0}}, pick = {{0}}, far1 = {{0}};
  int have = 0, have_far = 0;
  for (int k = 0; k < steps && dist[c] > 0; k++) {
    int cx = c % n, cz = c / n, best = c;
    for (int dz = -1; dz <= 1; dz++)
      for (int dx = -1; dx <= 1; dx++) {
        int x = cx + dx, z = cz + dz;
        if ((!dx && !dz) || x < 0 || z < 0 || x >= n || z >= n) continue;
        int nb = z * n + x;
        if (dist[nb] >= dist[best] || !nav_step_ok(c, nb)) continue;
        if (dx && dz && !nav_diag_ok(cx, cz, x, z)) continue;
        best = nb;
      }
    if (best == c) break;
    c = best;
    v3 cp = {{nav_centre(c % n), g_nav.lvl[c], nav_centre(c / n)}};
    float ddx = cp.x - from.x, ddz = cp.z - from.z;
    if (!have_far && ddx * ddx + ddz * ddz >= NAV_MIN_LEAD * NAV_MIN_LEAD) {
      far1 = cp; have_far = 1;
    }
    if (!have) { first = pick = cp; have = 1; }
    else if (nav_clear(from, cp)) pick = cp;
  }
  if (!have) {   // already in the goal cell
    *out = (v3){{nav_centre(c % n), g_nav.lvl[c], nav_centre(c / n)}};
    return 1;
  }
  v3 o = nav_clear(from, pick) ? pick : first;
  float odx = o.x - from.x, odz = o.z - from.z;
  if (odx * odx + odz * odz < NAV_MIN_LEAD * NAV_MIN_LEAD && have_far) o = far1;
  *out = o;
  return 1;
}


// THE ROUTE TO A POINT, for a bot on a routed map: the waypoint to steer at
// instead of the point itself. Cached per bot and re-planned every quarter
// second, when the destination moves, or when the waypoint is reached. Off a
// routed map it is the point itself, so every caller below is unchanged there.
static v3 bot_route(bot_t *b, v3 dest) {
  if (!map_routes()) return dest;
  float ddx = dest.x - b->route_dest.x, ddz = dest.z - b->route_dest.z;
  float wdx = b->route_wp.x - b->pos.x, wdz = b->route_wp.z - b->pos.z;
  if (b->route_t > 0 && ddx * ddx + ddz * ddz < 2.25f && wdx * wdx + wdz * wdz > 0.81f) {
    b->route_t--;
    return b->route_wp;
  }
  nav_ready();
  b->route_dest = dest;
  b->route_t = TICK_HZ / 4;
  b->route_wp = dest;
  if (nav_clear(b->pos, dest)) return dest;          // in sight: just go
  int goal = nav_snap(dest);
  if (goal < 0) return dest;
  v3 wp;
  if (nav_next(nav_field_for(goal), b->pos, 10, &wp)) b->route_wp = wp;
  return b->route_wp;
}


