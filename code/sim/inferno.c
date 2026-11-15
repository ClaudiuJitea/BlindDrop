// code/sim/inferno.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// FIRE, from the incendiary grenade, the way CS2's incendiary/molotov works. The
// canister goes off the moment it strikes a floor and lays a POOL OF FIRE that
// runs out over the ground from where it broke in about a second, burns for
// seven, and dies back from its rim inward. Anyone standing in it burns — the
// thrower too. It does not climb walls, it pours down a step but not up one,
// and it stops at a wall the way water would.
//
// SMOKE PUTS IT OUT. A smoke cloud blooming over burning ground snuffs every
// cell the smoke reaches as the smoke reaches it, and an incendiary landing in a
// live cloud never catches where the cloud is: the CS2 counter-play.
//
// THE POOL IS A FLOOD FILL over a 0.5 m ground grid, the smoke's idea in 2D:
// from the cell the canister broke in, a Dijkstra walk steps to each neighbour
// whose floor it can reach (no wall between, a drop of at most FIRE_DROP, a
// rise of at most FIRE_RISE) until FIRE_BUDGET cells are spent or FIRE_DMAX
// metres walked. Each cell keeps its floor height and its walking distance, and
// that distance drives the spread (near cells first) and the dying (the rim
// first). One deterministic function of the break point and the static solids,
// so the server and every client build the same pool from the one EV_FLAME, and
// the snuffing comes from the EV_SMOKE every peer sees anyway.
// ---------------------------------------------------------------------------
#define FIRE_CELL    0.5f
#define FIRE_NX      17          // 8.5 m across
#define FIRE_N       (FIRE_NX * FIRE_NX)
#define FIRE_BUDGET  58          // 58 x 0.25 m^2 = 14.5 m^2, ~4.3 m across in the open
#define FIRE_DMAX    3.2f        // ...and no cell further than this, walked
#define FIRE_RISE    0.30f       // m it can climb between cells (a kerb, not a crate)
#define FIRE_DROP    1.40f       // m it can pour down
#define FIRE_SPREAD  1.10f       // s for the spread to reach the far rim
#define FIRE_LIFE    7.0f        // s the pool burns, as in CS2
#define FIRE_DIE     1.2f        // s over which it dies back, rim first
#define FIRE_DMG     9           // per burn tick...
#define FIRE_DMG_T   (TICK_HZ / 4)   // ...four times a second: 36 hp/s in the thick of it
#define MAX_INFERNOS    4

typedef struct {
  int      on, src;
  long     t0;                   // the tick it caught
  v3       at;                   // where it broke
  float    ox, oz;               // the grid's min corner
  float    dmax;                 // the farthest cell the fill reached, m
  int      ncell;
  uint8_t  dist[FIRE_N];         // walking distance in 1/32 m, 255 = no fire
  float    fy[FIRE_N];           // the floor height of each cell
  uint16_t out[FIRE_N];          // tick offset from t0 smoke snuffed it, 0 = burning
  uint16_t cells[FIRE_BUDGET];
} inferno_t;
static inferno_t g_infernos_all[SRV_LOBBIES][MAX_INFERNOS];
#define g_infernos (g_infernos_all[g_lobby])
static int g_inferno_last = -1;     // the slot the last pop filled (harness report)

// The floor under a cell's centre, searched from `top` down at most `reach`;
// 0 when there is none. The slab of air just over it must be free of solids (a
// cell under a crate is not a floor, it is the crate's inside).
static int inferno_floor(float x, float z, float top, float reach, float *fy) {
  v3 n;
  int si;
  float t = trace_world((v3){{x, top, z}}, (v3){{0, -1, 0}}, &n, &si);
  if (t > reach) return 0;
  float y = top - t;
  BP_BOX(i, x - 0.2f, z - 0.2f, x + 0.2f, z + 0.2f) {
    const solid_t *sd = &g_solids[i];
    if (sd->max.x > x - 0.2f && sd->min.x < x + 0.2f && sd->max.z > z - 0.2f && sd->min.z < z + 0.2f &&
        sd->max.y > y + 0.06f && sd->min.y < y + 0.45f) return 0;
  }
  *fy = y;
  return 1;
}

static void inferno_fill(inferno_t *f, int sx, int sz) {
  static uint16_t best[FIRE_N];
  static uint32_t heap[4096];
  int hn = 0;
  for (int i = 0; i < FIRE_N; i++) { best[i] = 0xFFFF; f->dist[i] = 255; f->out[i] = 0; f->fy[i] = 0.0f; }
  int c0 = sz * FIRE_NX + sx;
  best[c0] = 0;
  heap[hn++] = (uint32_t)c0;
  int taken = 0;
  float dlim = FIRE_DMAX * 32.0f;
  while (hn && taken < FIRE_BUDGET) {
    uint32_t top = heap[0];
    heap[0] = heap[--hn];
    for (int k = 0;;) {
      int l = 2 * k + 1, r = l + 1, m = k;
      if (l < hn && heap[l] < heap[m]) m = l;
      if (r < hn && heap[r] < heap[m]) m = r;
      if (m == k) break;
      uint32_t t = heap[k]; heap[k] = heap[m]; heap[m] = t; k = m;
    }
    int c = (int)(top & 0xFFFF), d = (int)(top >> 16);
    if (d != best[c] || f->dist[c] != 255) continue;
    f->dist[c] = (uint8_t)(d > 254 ? 254 : d);
    taken++;
    int x = c % FIRE_NX, z = c / FIRE_NX;
    float cy = f->fy[c];
    for (int dz = -1; dz <= 1; dz++)
    for (int dx = -1; dx <= 1; dx++) {
      if (!dx && !dz) continue;
      int nx = x + dx, nz = z + dz;
      if (nx < 0 || nz < 0 || nx >= FIRE_NX || nz >= FIRE_NX) continue;
      int nc = nz * FIRE_NX + nx;
      if (f->dist[nc] != 255) continue;
      int nd = d + (dx && dz ? 23 : 16);    // 0.5 m and its diagonal, in 1/32 m
      if ((float)nd > dlim || nd >= best[nc]) continue;
      float wx = f->ox + (nx + 0.5f) * FIRE_CELL, wz = f->oz + (nz + 0.5f) * FIRE_CELL;
      float ny;
      if (!inferno_floor(wx, wz, cy + FIRE_RISE + 0.05f, FIRE_RISE + 0.05f + FIRE_DROP, &ny)) continue;
      if (ny > cy + FIRE_RISE || ny < cy - FIRE_DROP) continue;
      // No wall between: a level trace a hand over the higher of the two floors.
      float hy = fmaxf(cy, ny) + 0.12f;
      v3 a = {{f->ox + (x + 0.5f) * FIRE_CELL, hy, f->oz + (z + 0.5f) * FIRE_CELL}};
      v3 dv = {{wx - a.x, 0, wz - a.z}};
      float dl = v3_len(dv);
      v3 wn;
      int ws;
      if (trace_world(a, v3_scale(dv, 1.0f / dl), &wn, &ws) < dl) continue;
      f->fy[nc] = ny;
      best[nc] = (uint16_t)nd;
      if (hn < (int)(sizeof heap / sizeof *heap)) {
        int k_ = hn++;
        heap[k_] = (uint32_t)nd << 16 | (uint32_t)nc;
        while (k_ && heap[(k_ - 1) / 2] > heap[k_]) {
          uint32_t t_ = heap[k_]; heap[k_] = heap[(k_ - 1) / 2]; heap[(k_ - 1) / 2] = t_;
          k_ = (k_ - 1) / 2;
        }
      }
    }
  }
  f->ncell = 0;
  int dm = 16;
  for (int i = 0; i < FIRE_N && f->ncell < FIRE_BUDGET; i++)
    if (f->dist[i] != 255) {
      f->cells[f->ncell++] = (uint16_t)i;
      if (f->dist[i] > dm) dm = f->dist[i];
    }
  f->dmax = (float)dm / 32.0f;
}

static float inferno_age(const inferno_t *f) { return (float)(g_tick - f->t0) / (float)TICK_HZ; }
static int inferno_alive(const inferno_t *f) { return f->on && inferno_age(f) < FIRE_LIFE + 0.6f; }
static float inferno_u(const inferno_t *f, int c) {
  return f_clamp((float)f->dist[c] / 32.0f / f->dmax, 0.0f, 1.0f);
}
// HOW HARD CELL c BURNS at time t (s since it caught), 0..1: catching as the
// spread reaches it, full, then dying back rim first — or snuffed by smoke.
static float inferno_cell_heat(const inferno_t *f, int c, float t) {
  if (f->dist[c] == 255) return 0.0f;
  float u = inferno_u(f, c);
  float tc = 0.04f + FIRE_SPREAD * powf(u, 1.15f);
  float g = f_clamp((t - tc) / 0.30f, 0.0f, 1.0f);
  if (g <= 0.0f) return 0.0f;
  float te = FIRE_LIFE - FIRE_DIE * u;
  float k = smooth01(g) * f_clamp((te - t) / 0.8f, 0.0f, 1.0f);
  if (f->out[c]) k *= f_clamp(1.0f - (t - (float)f->out[c] / (float)TICK_HZ) / 0.35f, 0.0f, 1.0f);
  return k * (1.0f - 0.25f * u);           // the rim burns lower than the heart
}
// The cell under a world point (its floor within reach of it), -1 when none.
static int inferno_cell_at(const inferno_t *f, v3 p) {
  int x = (int)floorf((p.x - f->ox) / FIRE_CELL), z = (int)floorf((p.z - f->oz) / FIRE_CELL);
  if (x < 0 || z < 0 || x >= FIRE_NX || z >= FIRE_NX) return -1;
  int c = z * FIRE_NX + x;
  if (f->dist[c] == 255 || p.y < f->fy[c] - 0.35f || p.y > f->fy[c] + 0.9f) return -1;
  return c;
}
// The heat at a point (the hottest pool covering it).
static float inferno_heat_at(v3 p) {
  float k = 0.0f;
  for (int i = 0; i < MAX_INFERNOS; i++) {
    const inferno_t *f = &g_infernos[i];
    if (!inferno_alive(f)) continue;
    int c = inferno_cell_at(f, p);
    if (c >= 0) k = fmaxf(k, inferno_cell_heat(f, c, inferno_age(f)));
  }
  return k;
}

// SMOKE SNUFFS FIRE: every burning cell of fire f that cloud s covers goes out
// when the smoke's bloom reaches it (or at once, if it already has).
static void inferno_snuff(inferno_t *f, const smoke_t *s) {
  for (int k = 0; k < f->ncell; k++) {
    int c = f->cells[k];
    int x = c % FIRE_NX, z = c / FIRE_NX;
    v3 p = {{f->ox + (x + 0.5f) * FIRE_CELL, f->fy[c] + 0.25f, f->oz + (z + 0.5f) * FIRE_CELL}};
    int sc = smoke_cell_at(s, p);
    if (sc < 0 || s->dist[sc] == 255) continue;
    float su = f_clamp((float)s->dist[sc] / SMK_DU / s->dmax, 0.0f, 1.0f);
    long when = s->t0 + (long)((0.06f + SMK_BLOOM * powf(su, 1.4f) + 0.15f) * (float)TICK_HZ);
    long off = when - f->t0;
    if (off < 1) off = 1;
    if (off > 65000) continue;
    if (!f->out[c] || f->out[c] > off) f->out[c] = (uint16_t)off;
  }
}

static void inferno_spawn(v3 at, int src) {
  int slot = 0;
  long oldest = 1L << 62;
  for (int i = 0; i < MAX_INFERNOS; i++) {
    if (!g_infernos[i].on) { slot = i; oldest = -1; break; }
    if (g_infernos[i].t0 < oldest) { oldest = g_infernos[i].t0; slot = i; }
  }
  inferno_t *f = &g_infernos[slot];
  g_inferno_last = slot;
  f->on = 1;
  f->src = src;
  f->t0 = g_tick;
  f->at = at;
  f->ox = at.x - (FIRE_NX / 2 + 0.5f) * FIRE_CELL;
  f->oz = at.z - (FIRE_NX / 2 + 0.5f) * FIRE_CELL;
  int sx = FIRE_NX / 2, sz = FIRE_NX / 2;
  float fy;
  if (!inferno_floor(at.x, at.z, at.y + 0.30f, 1.0f, &fy)) { f->on = 0; return; }
  f->fy[sz * FIRE_NX + sx] = fy;
  inferno_fill(f, sx, sz);
  if (f->ncell == 0) { f->on = 0; return; }
  // Landing in a live cloud: where the cloud is, it never catches.
  for (int i = 0; i < MAX_SMOKES; i++)
    if (smoke_alive(&g_smokes[i])) inferno_snuff(f, &g_smokes[i]);
}

// EV_FLAME: k = 1 the pool caught at a, k = 0 a wasted air burst at a.
static void inferno_on_event(const event_t *e) {
  if (e->type == EV_FLAME && e->k == 1) inferno_spawn(e->a, e->src);
  else if (e->type == EV_SMOKE && g_smoke_last >= 0 && g_smokes[g_smoke_last].on)
    for (int i = 0; i < MAX_INFERNOS; i++)
      if (inferno_alive(&g_infernos[i])) inferno_snuff(&g_infernos[i], &g_smokes[g_smoke_last]);
}
static void inferno_reset(void) {
  for (int i = 0; i < MAX_INFERNOS; i++) g_infernos[i].on = 0;
}
