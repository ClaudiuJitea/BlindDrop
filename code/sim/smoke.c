// code/sim/smoke.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// SMOKE, the way Counter-Strike 2 does it. A smoke grenade does nothing until it
// comes to REST; then it pops and a cloud BLOOMS out of it in about a second and a
// half, FILLS THE SPACE it is in rather than standing there as a ball, holds for
// eighteen seconds and thins away from the edges inward. It blocks SIGHT (players
// and bots), never bullets. A round fired through it punches a short-lived hole
// along its line; a frag going off in or beside it blows a large part of it away
// for a few seconds before it rolls back in.
//
// THE CLOUD IS A VOXEL FLOOD FILL, which is what makes it volumetric instead of a
// sphere. From the cell the canister lies in, a Dijkstra walk over a 0.5 m grid
// spreads into every free cell — never into a solid, never diagonally round a
// corner — until a fixed VOLUME of cells is spent. In the open that volume is a
// dome about 3.4 m across the ground; down a corridor the same volume runs much
// further, and through a doorway it spills into the next room — the CS2
// behaviour of a smoke "covering more length in tight spaces". Each cell keeps
// its walking distance from the canister, and that one number drives the bloom
// (near cells first), the density (a dense core, a thinner rim) and the end of
// its life (the rim goes first).
//
// One deterministic function of the burst point and the static solids, so the
// server and every client build the same cloud from the one EV_SMOKE event, and
// nothing about the cloud itself rides the wire. The holes come from events every
// peer sees anyway (EV_FIRE, EV_BLAST), applied through the one ev_push hook.
// ---------------------------------------------------------------------------
#define SMK_CELL     0.5f
#define SMK_NX       30          // 15 m across...
#define SMK_NY       9           // ...4.5 m tall (1.5 m below the burst, 3 m above)...
#define SMK_NZ       30
#define SMK_N        (SMK_NX * SMK_NY * SMK_NZ)
#define SMK_BELOW    3           // cells of the grid under the burst cell
#define SMK_UP_MAX   5           // a smoke climbs at most 2.5 m over its canister
#define SMK_BUDGET   860         // the cloud's volume: 860 x 0.125 m^3 = 107 m^3
#define SMK_DU       32.0f       // distance units per metre (a uint8 holds 7.9 m)
#define SMK_DMAX     7.4f        // ...and no cell further than this, whatever is left
#define SMK_LIFE     18.0f       // s of full cover after the bloom, as in CS2
#define SMK_FADE     2.4f        // s over which it thins out, rim first
#define SMK_BLOOM    1.45f       // s for the bloom to reach the far rim
#define SMK_SIGMA    2.0f        // extinction per metre of full-density smoke
#define SMK_TAU_SEE  1.6f        // optical depth past which nobody sees through
// A ROUND'S TUNNEL IS SOFT. It used to switch whole 0.5 m cells off within a
// fixed radius, and end-on that is a SQUARE window with hard edges cut in the
// cloud. Now each cell is thinned by how close its centre lies to the round's
// line: gone inside SMK_HOLE_IN, thinning out to nothing at SMK_HOLE_OUT, with a
// little per-cell jitter so the rim is ragged rather than a grid. The core of
// the tunnel also holds longest, so it closes from its edges in.
#define SMK_HOLE_IN  0.22f       // m: fully torn open this close to the line
#define SMK_HOLE_OUT 0.85f       // m: ...and untouched past this
#define SMK_HOLE_T   1.10f       // s before the core of a bullet hole starts to close
#define SMK_BLAST_R  4.2f        // m: what a frag clears
#define SMK_BLAST_T  3.6f        // s: ...and how long the middle of it stays clear
#define SMK_REFILL   0.45f       // s for a cleared cell to roll back in
#define MAX_SMOKES   6
#define SMK_SHOTS    32          // the recent rounds through a cloud, for the renderer

typedef struct {
  int      on, src;
  long     t0;                   // the tick it popped
  v3       burst;                // where the canister lay
  v3       o;                    // the grid's min corner
  float    dmax;                 // the farthest cell the fill reached, m
  int      ncell;
  uint8_t  dist[SMK_N];          // walking distance in 1/SMK_DU m, 255 = no smoke
  // TWO KINDS OF HOLE, kept apart: what a frag blew away (clear/hole) and what
  // rounds tore (bclear/bhole). The sight test reads both, per cell. The
  // renderer reads only the blast per cell and cuts the rounds' tunnels per
  // PUFF from `shot` below — at the exact distance of each puff from each line —
  // because a hole cut per 0.5 m cell, looked at down its own length, is a
  // square window with hard edges.
  uint16_t clear[SMK_N];         // tick offset from t0 the cell is cleared until
  uint8_t  hole[SMK_N];          // ...and how much of it (255 = all of it)
  uint16_t bclear[SMK_N];
  uint8_t  bhole[SMK_N];
  struct { v3 a, b; float t; } shot[SMK_SHOTS];   // a round's line through the box, and when
  int      nshot;
  uint16_t cells[SMK_BUDGET];    // the filled cells, so nothing scans the empty grid
  // THE DENSITY, CACHED PER TICK. Every sight test marches tens of samples and
  // a firefight asks hundreds of them a tick (each bot, each candidate, each
  // drawn puff); the curve behind a cell's density (powf, two ramps) is paid
  // once per filled cell per tick instead, and a sample is one load.
  float    den[SMK_N];
  long     den_tick;
} smoke_t;
static smoke_t g_smokes_all[SRV_LOBBIES][MAX_SMOKES];
#define g_smokes (g_smokes_all[g_lobby])
static int g_smoke_last = -1;   // the slot the last pop filled (harness report)

static int smk_idx(int x, int y, int z) { return (y * SMK_NZ + z) * SMK_NX + x; }

// A cell is free when no solid overlaps it and it is not under the floor. The box
// is shrunk 2 cm so a cell that only TOUCHES a wall face is still air.
static int smk_cell_free(const smoke_t *s, int x, int y, int z) {
  v3 mn = {{s->o.x + (float)x * SMK_CELL + 0.02f, s->o.y + (float)y * SMK_CELL + 0.02f,
            s->o.z + (float)z * SMK_CELL + 0.02f}};
  v3 mx = {{mn.x + SMK_CELL - 0.04f, mn.y + SMK_CELL - 0.04f, mn.z + SMK_CELL - 0.04f}};
  if (mn.y < 0.0f) return 0;
  BP_BOX(i, mn.x, mn.z, mx.x, mx.z) {
    const solid_t *sd = &g_solids[i];
    if (sd->max.x > mn.x && sd->min.x < mx.x && sd->max.y > mn.y && sd->min.y < mx.y &&
        sd->max.z > mn.z && sd->min.z < mx.z) return 0;
  }
  return 1;
}

// The fill. A binary heap of (distance, cell) with lazy deletion; every free test
// is cached in `fr` (0 unknown, 1 free, 2 blocked) so each cell asks the
// broadphase once.
static void smoke_fill(smoke_t *s, int sx, int sy, int sz) {
  static uint32_t heap[1 << 16];   // dist << 16 | cell
  static uint8_t fr[SMK_N];
  static uint16_t best[SMK_N];
  int hn = 0;
  memset(fr, 0, sizeof fr);
  for (int i = 0; i < SMK_N; i++) { best[i] = 0xFFFF; s->dist[i] = 255; s->clear[i] = 0; s->hole[i] = 0;
                                     s->bclear[i] = 0; s->bhole[i] = 0; }
  s->nshot = 0;
  #define SMK_FREE(x, y, z) \
    (fr[smk_idx(x, y, z)] ? fr[smk_idx(x, y, z)] == 1 \
                          : (fr[smk_idx(x, y, z)] = smk_cell_free(s, x, y, z) ? 1 : 2) == 1)
  #define SMK_PUSH(d, c) do { if (hn < (int)(sizeof heap / sizeof *heap)) {      \
      int k_ = hn++; heap[k_] = (uint32_t)(d) << 16 | (uint32_t)(c);             \
      while (k_ && heap[(k_ - 1) / 2] > heap[k_]) {                              \
        uint32_t t_ = heap[k_]; heap[k_] = heap[(k_ - 1) / 2]; heap[(k_ - 1) / 2] = t_; \
        k_ = (k_ - 1) / 2; } } } while (0)
  int c0 = smk_idx(sx, sy, sz);
  best[c0] = 0;
  SMK_PUSH(0, c0);
  int taken = 0;
  float dlim = SMK_DMAX * SMK_DU;
  while (hn && taken < SMK_BUDGET) {
    uint32_t top = heap[0];
    heap[0] = heap[--hn];
    for (int k = 0;;) {                                   // sift down
      int l = 2 * k + 1, r = l + 1, m = k;
      if (l < hn && heap[l] < heap[m]) m = l;
      if (r < hn && heap[r] < heap[m]) m = r;
      if (m == k) break;
      uint32_t t = heap[k]; heap[k] = heap[m]; heap[m] = t; k = m;
    }
    int c = (int)(top & 0xFFFF), d = (int)(top >> 16);
    if (d != best[c] || s->dist[c] != 255) continue;      // stale, or already taken
    s->dist[c] = (uint8_t)(d > 254 ? 254 : d);
    taken++;
    int x = c % SMK_NX, z = (c / SMK_NX) % SMK_NZ, y = c / (SMK_NX * SMK_NZ);
    for (int dy = -1; dy <= 1; dy++)
    for (int dz = -1; dz <= 1; dz++)
    for (int dx = -1; dx <= 1; dx++) {
      if (!dx && !dy && !dz) continue;
      int nx = x + dx, ny = y + dy, nz = z + dz;
      if (nx < 0 || ny < 0 || nz < 0 || nx >= SMK_NX || ny >= SMK_NY || nz >= SMK_NZ) continue;
      if (ny - sy > SMK_UP_MAX) continue;
      // Never round a corner: a diagonal step needs every cell of the little
      // block it crosses to be free, so a wall one cell thick seals.
      int ok = 1;
      for (int ay = 0; ay <= (dy != 0) && ok; ay++)
      for (int az = 0; az <= (dz != 0) && ok; az++)
      for (int ax = 0; ax <= (dx != 0) && ok; ax++)
        if (!SMK_FREE(x + ax * dx, y + ay * dy, z + az * dz)) ok = 0;
      if (!ok) continue;
      // Euclidean steps (1, sqrt2, sqrt3 cells), and climbing costs half again:
      // smoke spreads along the ground faster than it stacks, so an open-ground
      // cloud is a low wide dome and not a ball.
      float hs = (float)(dx * dx + dz * dz), vs = (float)(dy * dy) * 2.25f;
      int nd = d + (int)(sqrtf(hs + vs) * SMK_CELL * SMK_DU + 0.5f);
      if ((float)nd > dlim) continue;
      int nc = smk_idx(nx, ny, nz);
      if (s->dist[nc] != 255 || nd >= best[nc]) continue;
      best[nc] = (uint16_t)nd;
      SMK_PUSH(nd, nc);
    }
  }
  #undef SMK_FREE
  #undef SMK_PUSH
  s->ncell = 0;
  for (int i = 0; i < SMK_N && s->ncell < SMK_BUDGET; i++)
    if (s->dist[i] != 255) s->cells[s->ncell++] = (uint16_t)i;
  taken = s->ncell;
  int dm = 1;
  for (int i = 0; i < SMK_N; i++) if (s->dist[i] != 255 && s->dist[i] > dm) dm = s->dist[i];
  s->dmax = (float)dm / SMK_DU;
}

// POP. Lays the grid round the canister so it sits in the middle of a cell, finds
// the nearest free cell to start from (a canister resting in a corner is touching
// two solids), and fills.
static void smoke_spawn(v3 at, int src) {
  int slot = 0;
  long oldest = 1L << 62;
  for (int i = 0; i < MAX_SMOKES; i++) {
    if (!g_smokes[i].on) { slot = i; oldest = -1; break; }
    if (g_smokes[i].t0 < oldest) { oldest = g_smokes[i].t0; slot = i; }
  }
  smoke_t *s = &g_smokes[slot];
  g_smoke_last = slot;
  s->on = 1;
  s->src = src;
  s->t0 = g_tick;
  s->burst = at;
  float by = fmaxf(at.y, 0.0f) + 0.20f;   // the canister's own height, off the floor
  s->o = (v3){{at.x - (SMK_NX / 2 + 0.5f) * SMK_CELL, floorf(by / SMK_CELL) * SMK_CELL - SMK_BELOW * SMK_CELL,
               at.z - (SMK_NZ / 2 + 0.5f) * SMK_CELL}};
  int sx = SMK_NX / 2, sy = SMK_BELOW, sz = SMK_NZ / 2;
  int best = -1;
  for (int r = 0; r <= 2 && best < 0; r++)
    for (int dy = 0; dy <= r && best < 0; dy++)
      for (int dz = -r; dz <= r && best < 0; dz++)
        for (int dx = -r; dx <= r && best < 0; dx++)
          if (smk_cell_free(s, sx + dx, sy + dy, sz + dz)) {
            sx += dx; sy += dy; sz += dz;
            best = 1;
          }
  if (best < 0) { s->on = 0; return; }    // buried in geometry: nothing to fill
  smoke_fill(s, sx, sy, sz);
  memset(s->den, 0, sizeof s->den);
  s->den_tick = -1;
}

// How much a hole still takes away at time t (0..1), given its strength and the
// time it starts to close: all of it while it holds, then rolling back in.
static float smoke_hole_left(float strength, float tc, float t) {
  return strength * (1.0f - f_clamp((t - tc) / SMK_REFILL, 0.0f, 1.0f));
}
static float smoke_hole_cell(uint16_t clr, uint8_t hl, float t) {
  return clr ? smoke_hole_left((float)hl / 255.0f, (float)clr / (float)TICK_HZ, t) : 0.0f;
}
// The blast's hole in cell c (bullets = 0) or the rounds' (bullets = 1).
static float smoke_hole_now(const smoke_t *s, int c, float t, int bullets) {
  return bullets ? smoke_hole_cell(s->bclear[c], s->bhole[c], t)
                 : smoke_hole_cell(s->clear[c], s->hole[c], t);
}
// Open (part of) a cell: never weaker than what is already open there.
static void smoke_hole_open(smoke_t *s, int c, float age, float strength, float hold, int bullets) {
  uint16_t *clr = bullets ? s->bclear : s->clear;
  uint8_t *hl = bullets ? s->bhole : s->hole;
  if (strength <= smoke_hole_cell(clr[c], hl[c], age)) return;
  hl[c] = (uint8_t)(f_clamp(strength, 0.0f, 1.0f) * 255.0f + 0.5f);
  clr[c] = (uint16_t)f_clamp((age + hold) * (float)TICK_HZ, 1.0f, 65000.0f);
}
// THE ROUNDS' TUNNELS AT A POINT (the renderer's, per puff): the same soft
// profile and timing the cells get, but measured from the point itself.
static float smoke_shot_hole(const smoke_t *s, v3 p, float t) {
  float best = 0.0f;
  for (int i = 0; i < s->nshot && i < SMK_SHOTS; i++) {
    float t0 = s->shot[i].t;
    if (t < t0 || t > t0 + SMK_HOLE_T + SMK_REFILL) continue;
    v3 a = s->shot[i].a, d = v3_sub(s->shot[i].b, a);
    float dl2 = v3_dot(d, d);
    float u = dl2 > 1e-6f ? f_clamp(v3_dot(v3_sub(p, a), d) / dl2, 0.0f, 1.0f) : 0.0f;
    float dd = v3_len(v3_sub(p, v3_add(a, v3_scale(d, u))));
    if (dd >= SMK_HOLE_OUT) continue;
    float w = 1.0f - smooth01(f_clamp((dd - SMK_HOLE_IN) / (SMK_HOLE_OUT - SMK_HOLE_IN), 0.0f, 1.0f));
    best = fmaxf(best, smoke_hole_left(w, t0 + SMK_HOLE_T * (0.35f + 0.65f * w), t));
  }
  return best;
}

// THE DENSITY OF ONE CELL at time `t` (s since the pop), 0..1. Bloom: cells come
// in by walking distance, the near ones first, each ramping over a quarter of a
// second. Life: SMK_LIFE of full cover, then the rim thins first and the core
// last. A hole: zero until its clear time, then rolling back in.
// `bullets` 0 leaves the rounds' holes out: the renderer cuts those per puff.
static float smoke_cell_dens_x(const smoke_t *s, int c, float t, int bullets) {
  int d8 = s->dist[c];
  if (d8 == 255) return 0.0f;
  float u = f_clamp((float)d8 / SMK_DU / s->dmax, 0.0f, 1.0f);
  float tb = 0.06f + SMK_BLOOM * powf(u, 1.4f);
  float g = f_clamp((t - tb) / 0.28f, 0.0f, 1.0f);
  if (g <= 0.0f) return 0.0f;
  float te = SMK_LIFE + SMK_BLOOM + SMK_FADE * (1.0f - u);
  float f = f_clamp((te - t) / 0.9f, 0.0f, 1.0f);
  float base = 1.0f - 0.50f * smooth01(f_clamp((u - 0.55f) / 0.45f, 0.0f, 1.0f));
  float k = base * g * g * (3.0f - 2.0f * g) * f;
  if (s->clear[c]) k *= 1.0f - smoke_hole_now(s, c, t, 0);
  if (bullets && s->bclear[c]) k *= 1.0f - smoke_hole_now(s, c, t, 1);
  return k;
}
static float smoke_cell_dens(const smoke_t *s, int c, float t) { return smoke_cell_dens_x(s, c, t, 1); }
static float smoke_age(const smoke_t *s) { return (float)(g_tick - s->t0) / (float)TICK_HZ; }
// The cached density of this tick (recomputed on the first ask after a tick).
static const float *smoke_den(smoke_t *s) {
  if (s->den_tick != g_tick) {
    float age = smoke_age(s);
    for (int k = 0; k < s->ncell; k++) s->den[s->cells[k]] = smoke_cell_dens(s, s->cells[k], age);
    s->den_tick = g_tick;
  }
  return s->den;
}
static int smoke_alive(const smoke_t *s) {
  return s->on && smoke_age(s) < SMK_LIFE + SMK_BLOOM + SMK_FADE + 1.0f;
}

// The cell under a world point, -1 outside the grid.
static int smoke_cell_at(const smoke_t *s, v3 p) {
  int x = (int)floorf((p.x - s->o.x) / SMK_CELL), y = (int)floorf((p.y - s->o.y) / SMK_CELL),
      z = (int)floorf((p.z - s->o.z) / SMK_CELL);
  if (x < 0 || y < 0 || z < 0 || x >= SMK_NX || y >= SMK_NY || z >= SMK_NZ) return -1;
  return smk_idx(x, y, z);
}
// Smoke density at a point (the densest cloud covering it).
static float smoke_density_at(v3 p) {
  float k = 0.0f;
  for (int i = 0; i < MAX_SMOKES; i++) {
    smoke_t *s = &g_smokes[i];
    if (!smoke_alive(s)) continue;
    int c = smoke_cell_at(s, p);
    if (c >= 0) k = fmaxf(k, smoke_den(s)[c]);
  }
  return k;
}

// Clip the segment a->b to a cloud's grid box; returns 0 when it misses.
static int smoke_clip(const smoke_t *s, v3 a, v3 b, float *t0, float *t1) {
  float lo = 0.0f, hi = 1.0f;
  const float mn[3] = {s->o.x, s->o.y, s->o.z};
  const float mx[3] = {s->o.x + SMK_NX * SMK_CELL, s->o.y + SMK_NY * SMK_CELL,
                       s->o.z + SMK_NZ * SMK_CELL};
  for (int k = 0; k < 3; k++) {
    float d = b.e[k] - a.e[k];
    if (fabsf(d) < 1e-7f) { if (a.e[k] < mn[k] || a.e[k] > mx[k]) return 0; continue; }
    float ta = (mn[k] - a.e[k]) / d, tb = (mx[k] - a.e[k]) / d;
    if (ta > tb) { float t = ta; ta = tb; tb = t; }
    lo = fmaxf(lo, ta);
    hi = fminf(hi, tb);
    if (lo > hi) return 0;
  }
  *t0 = lo; *t1 = hi;
  return 1;
}

// OPTICAL DEPTH along a->b through every cloud: metres of smoke times density
// times SMK_SIGMA. Marched at a quarter of a cell. This is the one sight test —
// bots, the HUD's markers and the renderer's own layering all read it.
static float smoke_tau_step(v3 a, v3 b, float step);
static float smoke_tau(v3 a, v3 b) { return smoke_tau_step(a, b, SMK_CELL * 0.25f); }
// ...at a chosen sample step: the renderer's layering and self-shadowing march a
// whole cell at a time (they average over a disc anyway), the sight test a quarter.
static float smoke_tau_step(v3 a, v3 b, float step) {
  float tau = 0.0f;
  for (int i = 0; i < MAX_SMOKES; i++) {
    smoke_t *s = &g_smokes[i];
    if (!smoke_alive(s)) continue;
    float t0, t1;
    if (!smoke_clip(s, a, b, &t0, &t1)) continue;
    v3 d = v3_sub(b, a);
    float len = v3_len(d) * (t1 - t0);
    if (len < 1e-4f) continue;
    const float *den = smoke_den(s);
    int n = (int)(len / step) + 1;
    float dt = (t1 - t0) / (float)n;
    for (int k = 0; k < n; k++) {
      v3 p = v3_add(a, v3_scale(d, t0 + dt * ((float)k + 0.5f)));
      int c = smoke_cell_at(s, p);
      if (c >= 0) tau += den[c] * (len / (float)n) * SMK_SIGMA;
    }
    if (tau > 8.0f) break;
  }
  return tau;
}
static int smoke_blocks(v3 a, v3 b) { return smoke_tau(a, b) > SMK_TAU_SEE; }

// A ROUND THROUGH THE SMOKE tears a soft tunnel along its line (SMK_HOLE_IN /
// SMK_HOLE_OUT above): every smoky cell is thinned by its distance to the line,
// the core held for SMK_HOLE_T and the fringe for less, then it rolls back in.
static void smoke_bullet(v3 a, v3 b) {
  v3 d = v3_sub(b, a);
  float dl2 = v3_dot(d, d);
  if (dl2 < 1e-6f) return;
  for (int i = 0; i < MAX_SMOKES; i++) {
    smoke_t *s = &g_smokes[i];
    if (!smoke_alive(s)) continue;
    float t0, t1;
    if (!smoke_clip(s, a, b, &t0, &t1)) continue;
    float age = smoke_age(s);
    uint32_t salt = (uint32_t)g_tick * 2654435761u;
    int si = s->nshot++ % SMK_SHOTS;
    if (s->nshot >= 2 * SMK_SHOTS) s->nshot -= SMK_SHOTS;   // stays >= SMK_SHOTS: the ring is full
    s->shot[si].a = v3_add(a, v3_scale(d, t0));
    s->shot[si].b = v3_add(a, v3_scale(d, t1));
    s->shot[si].t = age;
    for (int k = 0; k < s->ncell; k++) {
      int c = s->cells[k];
      int x = c % SMK_NX, z = (c / SMK_NX) % SMK_NZ, y = c / (SMK_NX * SMK_NZ);
      v3 p = {{s->o.x + (x + 0.5f) * SMK_CELL, s->o.y + (y + 0.5f) * SMK_CELL,
               s->o.z + (z + 0.5f) * SMK_CELL}};
      float u = f_clamp(v3_dot(v3_sub(p, a), d) / dl2, t0, t1);
      float dd = v3_len(v3_sub(p, v3_add(a, v3_scale(d, u))));
      if (dd >= SMK_HOLE_OUT + 0.15f) continue;
      // Ragged rim: up to +-0.12 m of the cell's own (deterministic) noise.
      uint32_t h = ((uint32_t)c * 0x9E3779B1u) ^ salt;
      h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
      dd += ((float)(h & 1023) / 1023.0f - 0.5f) * 0.24f;
      float w = 1.0f - smooth01(f_clamp((dd - SMK_HOLE_IN) / (SMK_HOLE_OUT - SMK_HOLE_IN), 0.0f, 1.0f));
      if (w <= 0.02f) continue;
      smoke_hole_open(s, c, age, w, SMK_HOLE_T * (0.35f + 0.65f * w), 1);
    }
    s->den_tick = -1;   // the holes show this tick, not the next
  }
}

// A FRAG IN THE SMOKE blows a big part of it away: every cell within SMK_BLAST_R,
// the middle staying clear longest so the hole closes from its edges in.
static void smoke_blast(v3 at) {
  for (int i = 0; i < MAX_SMOKES; i++) {
    smoke_t *s = &g_smokes[i];
    if (!smoke_alive(s)) continue;
    float age = smoke_age(s);
    for (int k = 0; k < s->ncell; k++) {
      int c = s->cells[k];
      int x = c % SMK_NX, z = (c / SMK_NX) % SMK_NZ, y = c / (SMK_NX * SMK_NZ);
      v3 p = {{s->o.x + (x + 0.5f) * SMK_CELL, s->o.y + (y + 0.5f) * SMK_CELL,
               s->o.z + (z + 0.5f) * SMK_CELL}};
      float dd = v3_len(v3_sub(p, at));
      if (dd > SMK_BLAST_R) continue;
      float hold = SMK_BLAST_T * (1.0f - 0.55f * dd / SMK_BLAST_R);
      smoke_hole_open(s, c, age, 1.0f, hold, 0);
    }
    s->den_tick = -1;
  }
}

// The one hook (ev_push, so the server, an offline game and every client all
// run it on the same events): the pop builds the cloud, a shot and a blast tear it.
static void smoke_on_event(const event_t *e) {
  if (e->type == EV_SMOKE) smoke_spawn(e->a, e->src);
  else if (e->type == EV_BLAST) smoke_blast(e->a);
  else if (e->type == EV_FIRE && e->src >= 0) {
    // EV_FIRE carries the END of the round's line and its direction; the
    // muzzle is a - b * f, exactly as the fly-by crack reconstructs it.
    v3 md = v3_norm(e->b);
    float f = e->f > 0.0f ? e->f : GUN_RANGE;
    smoke_bullet(v3_sub(e->a, v3_scale(md, f)), e->a);
  }
}
static void smoke_reset(void) {
  for (int i = 0; i < MAX_SMOKES; i++) g_smokes[i].on = 0;
}
