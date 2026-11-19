// code/world/prefabs.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// PREFABS — the architecture the tactical maps are dressed with. Everything is
// still boxes (the collision set IS the render mesh), so a building is
// assembled the way a set builder would: a core, a facade skin with real
// openings, trim, and roof furniture. Three rules shape every piece below.
//
//  1. NOTHING AT BODY HEIGHT LEAVES THE WALL PLANE. A box that sticks out 3 cm,
//     or a recess 12 cm deep, is a ledge a body sliding along the wall catches
//     on. So below 2.3 m a facade is flush — ground-floor windows and doors are
//     colour, not depth — and every relief (sills, shutters, balconies,
//     awnings, cornices) starts above head height.
//  2. NO TWO SAME-FACING FACES SHARE A PLANE WITH OVERLAP (`mapcheck`). Pieces
//     ABUT (a face meeting its neighbour's back face is fine) and trims are
//     sized so their tops never land on another piece's top.
//  3. STAIRS RISE 0.4 m A STEP, one STEP_H, so the player walks up them.
// Coordinates: x east, z south, y up; a facade is described along its wall
// (`a`), into the building from its outer plane (`t`, negative = outside).
// ---------------------------------------------------------------------------
#define PF_FLOOR 3.2f          // storey height
#define PF_SKIN  0.30f         // facade depth over the core
#define PF_RISE  0.40f         // one stair step == STEP_H's design step
#define PF_RUN   0.50f

static void bx(float x0, float y0, float z0, float x1, float y1, float z1, v3 c) {
  map_add((v3){{fminf(x0, x1), fminf(y0, y1), fminf(z0, z1)}},
          (v3){{fmaxf(x0, x1), fmaxf(y0, y1), fmaxf(z0, z1)}}, c);
}
static v3 rgb(float r, float g, float b) { return (v3){{r, g, b}}; }
static v3 tint(v3 c, float lo, float hi) { return col_tint(c, lo, hi); }

// A box on a facade. ax 0: the wall is z = plane, running along x; ax 1: the
// wall is x = plane, running along z. inw: +1/-1, the direction INTO the house.
static void fbox(int ax, float plane, int inw, float a0, float a1, float t0, float t1,
                 float y0, float y1, v3 c) {
  float d0 = plane + (float)inw * t0, d1 = plane + (float)inw * t1;
  if (ax == 0) bx(a0, y0, d0, a1, y1, d1, c);
  else         bx(d0, y0, a0, d1, y1, a1, c);
}

// THE PALETTE of one town: plasters, trims, glass, timber, accents.
typedef struct {
  v3 plaster[4], trim, glass, door, shutter[3], awning[4], roof, stone;
  int shutters, awnings, balconies, wide_windows;
} town_style_t;
static town_style_t g_town;
static int g_town_kind = -1;   // which town the grid is being dressed as
// The grid being dressed, so a facade can ask whether a street is really in
// front of each opening (NULL: a hand-placed landmark, which asks nobody).
static const char *const *g_mg_rows;
static int mg_point_open(float x, float z) {
  if (!g_mg_rows) return 1;
  const float cs = 2.0f * ARENA_HALF / 52.0f;
  int c = (int)floorf((x + ARENA_HALF) / cs), r = (int)floorf((z + ARENA_HALF) / cs);
  if (c < 0 || r < 0 || c >= 52 || r >= 52) return 0;
  // Clear AIR in front of a facade: floor, paint, or under a roof or catwalk.
  // Stairs, crates, walls and water have their own geometry there.
  return strchr(".ABTK", g_mg_rows[r][c]) != NULL;
}

static void town_style(int which) {
  town_style_t *s = &g_town;
  memset(s, 0, sizeof *s);
  s->glass = rgb(0.10f, 0.12f, 0.15f);
  s->door  = rgb(0.30f, 0.19f, 0.11f);
  if (which == 0) {          // CITADEL: an old desert town
    // Limewash in four pours: warm sand, bone, a soft terracotta and chalk
    // white. Pulled back from ochre so a clear sky does not turn the town orange.
    s->plaster[0] = rgb(0.82f, 0.72f, 0.56f); s->plaster[1] = rgb(0.88f, 0.83f, 0.73f);
    s->plaster[2] = rgb(0.78f, 0.60f, 0.46f); s->plaster[3] = rgb(0.92f, 0.89f, 0.83f);
    s->trim = rgb(0.62f, 0.50f, 0.36f); s->stone = rgb(0.58f, 0.50f, 0.40f);
    s->roof = rgb(0.70f, 0.60f, 0.46f);
    s->shutter[0] = rgb(0.16f, 0.36f, 0.52f); s->shutter[1] = rgb(0.20f, 0.44f, 0.36f);
    s->shutter[2] = rgb(0.52f, 0.30f, 0.18f);
    s->awning[0] = rgb(0.72f, 0.24f, 0.16f); s->awning[1] = rgb(0.18f, 0.38f, 0.58f);
    s->awning[2] = rgb(0.86f, 0.62f, 0.20f); s->awning[3] = rgb(0.30f, 0.52f, 0.30f);
    s->shutters = 1; s->awnings = 1; s->balconies = 1;
  } else if (which == 1) {   // HARBOR: brick and painted-steel waterfront
    s->plaster[0] = rgb(0.46f, 0.26f, 0.20f); s->plaster[1] = rgb(0.56f, 0.58f, 0.60f);
    s->plaster[2] = rgb(0.38f, 0.42f, 0.46f); s->plaster[3] = rgb(0.52f, 0.32f, 0.24f);
    s->trim = rgb(0.30f, 0.32f, 0.34f); s->stone = rgb(0.40f, 0.41f, 0.42f);
    s->roof = rgb(0.32f, 0.34f, 0.36f);
    s->shutter[0] = rgb(0.20f, 0.30f, 0.40f); s->shutter[1] = rgb(0.22f, 0.24f, 0.26f);
    s->shutter[2] = rgb(0.45f, 0.18f, 0.14f);
    s->awning[0] = rgb(0.16f, 0.30f, 0.46f); s->awning[1] = rgb(0.62f, 0.20f, 0.16f);
    s->awning[2] = rgb(0.26f, 0.28f, 0.30f); s->awning[3] = rgb(0.80f, 0.60f, 0.16f);
    s->awnings = 1; s->wide_windows = 1;
  } else {                   // REFINERY: concrete and corrugated sheds
    s->plaster[0] = rgb(0.66f, 0.64f, 0.60f); s->plaster[1] = rgb(0.58f, 0.60f, 0.62f);
    s->plaster[2] = rgb(0.72f, 0.70f, 0.64f); s->plaster[3] = rgb(0.52f, 0.54f, 0.52f);
    s->trim = rgb(0.80f, 0.62f, 0.14f); s->stone = rgb(0.46f, 0.46f, 0.44f);
    s->roof = rgb(0.44f, 0.45f, 0.46f);
    s->shutter[0] = rgb(0.30f, 0.32f, 0.34f); s->shutter[1] = s->shutter[0];
    s->shutter[2] = s->shutter[0];
    s->awning[0] = rgb(0.80f, 0.62f, 0.14f); s->awning[1] = rgb(0.40f, 0.42f, 0.44f);
    s->awning[2] = s->awning[0]; s->awning[3] = s->awning[1];
    s->wide_windows = 1;
  }
  // The materials behind the palette (see smat_classify). Registered AFTER the
  // theme's own entries, and the closest colour match wins either way.
  static const uint8_t PM[3][4] = {
    {SMAT_STUCCO,   SMAT_STUCCO, SMAT_STUCCO,   SMAT_STUCCO},   // limewashed render
    {SMAT_BRICK,    SMAT_CONCRETE, SMAT_CLAD,   SMAT_BRICK},    // warehouses, sheds
    {SMAT_CONCRETE, SMAT_CLAD,   SMAT_CONCRETE, SMAT_CLAD},     // precast, sheeting
  };
  for (int i = 0; i < 4; i++) smat_reg(s->plaster[i], PM[which][i]);
  smat_reg(s->stone, which == 0 ? SMAT_STONE : SMAT_CONCRETE);
  smat_reg(s->door, SMAT_WOOD);
  smat_reg(s->glass, SMAT_GLASS);                                 // every window
  if (which == 0) smat_reg(s->trim, SMAT_STUCCO);
  if (which == 0) smat_reg(rgb(0.24f, 0.46f, 0.52f), SMAT_COPPER);   // the dome
  if (which == 1) {                                               // the gantry
    smat_reg(rgb(0.86f, 0.64f, 0.14f), SMAT_HAZARD);
    smat_reg(rgb(0.28f, 0.30f, 0.32f), SMAT_STEEL);
  }
  if (which == 2) {
    smat_reg(rgb(0.74f, 0.72f, 0.68f), SMAT_CONCRETE);             // cooling tower
    smat_reg(s->trim, SMAT_HAZARD);                                // safety yellow
    smat_reg(rgb(0.30f, 0.31f, 0.32f), SMAT_STEEL);                // rack posts
    smat_reg(rgb(0.62f, 0.64f, 0.66f), SMAT_STEEL);                // the pipes
    smat_reg(rgb(0.52f, 0.26f, 0.18f), SMAT_STEEL);
    smat_reg(rgb(0.26f, 0.46f, 0.30f), SMAT_STEEL);
    smat_reg(rgb(0.78f, 0.78f, 0.76f), SMAT_STEEL);                // the column
    smat_reg(rgb(0.62f, 0.62f, 0.60f), SMAT_STEEL);
  }
}

// One facade: piers, and in each opening column the wall pieces, the window or
// door between them, and the relief above head height. `windows` 0 = blank.
static void facade(int ax, float plane, int inw, float a0, float a1, float H, int floors,
                   int windows, int door, v3 wall) {
  const town_style_t *s = &g_town;
  float L = a1 - a0, D = PF_SKIN;
  float pitch = s->wide_windows ? 3.4f : 2.7f, ww = s->wide_windows ? 2.0f : 1.1f;
  int n = windows ? (int)((L - 0.8f) / pitch) : 0;
  if (n < 1) { fbox(ax, plane, inw, a0, a1, 0.0f, D, 0.0f, H, wall); return; }
  float step = L / (float)n;
  float prev = a0;
  int door_i = door ? n / 2 : -1;
  for (int i = 0; i < n; i++) {
    float c = a0 + step * ((float)i + 0.5f);
    float o0 = c - ww * 0.5f, o1 = c + ww * 0.5f;
    if (door && i == door_i) { o0 = c - 0.65f; o1 = c + 0.65f; }
    // Only where a street is in front of the whole opening and its relief:
    // a facade that runs on past the end of a street meets the next house.
    int faces_street = 1;
    // 1.9 m to either side catches a house standing across the end of this
    // facade, whose own awnings and balconies would meet these in the corner.
    for (int q = 0; q < 5; q++) {
      float a = q == 0 ? c : q == 1 ? o0 - 1.9f : q == 2 ? o1 + 1.9f
              : q == 3 ? o0 - 0.65f : o1 + 0.65f;
      float d = plane - (float)inw * 0.7f;
      if (!(ax == 0 ? mg_point_open(a, d) : mg_point_open(d, a))) faces_street = 0;
    }
    if (!faces_street) continue;      // the pier simply runs on across it
    fbox(ax, plane, inw, prev, o0, 0.0f, D, 0.0f, H, wall);            // pier
    prev = o1;
    float top = 0.0f;
    for (int f = 0; f < floors; f++) {
      float base = (float)f * PF_FLOOR;
      int is_door = f == 0 && i == door_i;
      float sill = is_door ? 0.0f : base + 0.95f, head = base + (is_door ? 2.3f : 2.25f);
      if (sill > top + 0.01f) fbox(ax, plane, inw, o0, o1, 0.0f, D, top, sill, wall);
      // Ground floor flush (rule 1); upper floors set 12 cm into the wall.
      float t0 = f == 0 ? 0.0f : 0.12f;
      fbox(ax, plane, inw, o0, o1, t0, D, sill, head, is_door ? s->door : s->glass);
      top = head;
      if (is_door) {   // lintel band + framed canopy with end support brackets
        fbox(ax, plane, inw, o0 - 0.18f, o1 + 0.18f, -0.08f, 0.0f, 2.30f, 2.50f, s->trim);
        if (s->awnings) {
          fbox(ax, plane, inw, o0 - 0.35f, o1 + 0.35f, -1.05f, 0.0f, 2.60f, 2.70f,
               s->awning[rng_u32() % 4]);
          fbox(ax, plane, inw, o0 - 0.32f, o0 - 0.25f, -0.75f, 0.0f, 2.34f, 2.60f, s->trim);
          fbox(ax, plane, inw, o1 + 0.25f, o1 + 0.32f, -0.75f, 0.0f, 2.34f, 2.60f, s->trim);
        }
      } else if (f == 0 && s->awnings && (rng_u32() % 4) == 0) {
        fbox(ax, plane, inw, o0 - 0.30f, o1 + 0.30f, -0.95f, 0.0f, 2.60f, 2.70f,
             s->awning[rng_u32() % 4]);
        fbox(ax, plane, inw, o0 - 0.27f, o0 - 0.20f, -0.68f, 0.0f, 2.34f, 2.60f, s->trim);
        fbox(ax, plane, inw, o1 + 0.20f, o1 + 0.27f, -0.68f, 0.0f, 2.34f, 2.60f, s->trim);
      }
      if (f >= 1) {
        fbox(ax, plane, inw, o0 - 0.08f, o1 + 0.08f, -0.12f, 0.0f, sill - 0.08f, sill,
             s->trim);                                                  // sill
        if (s->shutters && (rng_u32() % 3) != 0) {
          v3 sc = s->shutter[rng_u32() % 3];
          fbox(ax, plane, inw, o0 - 0.44f, o0 - 0.04f, -0.05f, 0.0f, sill, head, sc);
          fbox(ax, plane, inw, o1 + 0.04f, o1 + 0.44f, -0.05f, 0.0f, sill, head, sc);
        }
        if (f == 1 && s->balconies && (rng_u32() % 3) == 0) {
          float y = base;
          fbox(ax, plane, inw, o0 - 0.6f, o1 + 0.6f, -1.1f, 0.0f, y - 0.15f, y, s->stone);
          fbox(ax, plane, inw, o0 - 0.6f, o1 + 0.6f, -1.1f, -1.04f, y, y + 0.95f, s->door);
          fbox(ax, plane, inw, o0 - 0.6f, o0 - 0.54f, -1.04f, 0.0f, y, y + 0.95f, s->door);
          fbox(ax, plane, inw, o1 + 0.54f, o1 + 0.6f, -1.04f, 0.0f, y, y + 0.95f, s->door);
        }
      }
    }
    if (H > top + 0.01f) fbox(ax, plane, inw, o0, o1, 0.0f, D, top, H, wall);
  }
  fbox(ax, plane, inw, prev, a1, 0.0f, D, 0.0f, H, wall);
}

// A side of a house for house(): which walls carry windows, and where a stair
// reaches the roof (the parapet leaves a gap there).
enum { SIDE_N = 1, SIDE_E = 2, SIDE_S = 4, SIDE_W = 8 };

// THE HOUSE. A core inset by the skin, four skins (N/S run the full length,
// E/W fit between them so no two skins overlap), a cornice above the top
// floor's windows and a parapet round a flat roof, with a gap at `gap_*` for a
// roof stair. `win` = sides with windows, `doors` = sides with a door.
static void house(float x0, float z0, float x1, float z1, int floors, int win, int doors,
                  int gap_side, float gap_a0, float gap_a1, int roof_props) {
  const town_style_t *s = &g_town;
  float H = (float)floors * PF_FLOOR, D = PF_SKIN;
  v3 wall = tint(s->plaster[rng_u32() % 4], 0.95f, 1.04f);
  bx(x0 + D, 0.0f, z0 + D, x1 - D, H, z1 - D, wall);                    // core
  int dn = doors & SIDE_N ? 1 : 0, ds = doors & SIDE_S ? 1 : 0;
  int de = doors & SIDE_E ? 1 : 0, dw = doors & SIDE_W ? 1 : 0;
  facade(0, z0, +1, x0, x1, H, floors, win & SIDE_N, dn, wall);
  facade(0, z1, -1, x0, x1, H, floors, win & SIDE_S, ds, wall);
  facade(1, x0, +1, z0 + D, z1 - D, H, floors, win & SIDE_W, dw, wall);
  facade(1, x1, -1, z0 + D, z1 - D, H, floors, win & SIDE_E, de, wall);
  // Cornice: N/S along the full length, E/W between them; outward only, and
  // only where nothing climbs past it.
  float c0 = H - 0.45f, c1 = H - 0.15f;
  // Street stretches only: a cornice over a party wall, or past the corner
  // where another house meets this one, would sit inside that house.
  for (int sd = 1; sd <= 8; sd <<= 1) {
    if (gap_side == sd || !(win & sd)) continue;
    int axz = sd == SIDE_N || sd == SIDE_S;
    float a0 = axz ? x0 : z0 + 0.2f, a1 = axz ? x1 : z1 - 0.2f;
    float pl = sd == SIDE_N ? z0 : sd == SIDE_S ? z1 : sd == SIDE_W ? x0 : x1;
    float out = (sd == SIDE_N || sd == SIDE_W) ? -1.0f : 1.0f;
    // Walk the side cell by cell (cells are where other houses begin), so a
    // run ends exactly where the street does.
    const float cs = 2.0f * ARENA_HALF / 52.0f;
    float run0 = -1.0f;
    for (float a = a0; a < a1 - 0.001f; ) {
      float cell_end = -ARENA_HALF + (floorf((a + ARENA_HALF) / cs + 1e-4f) + 1.0f) * cs;
      float e = fminf(cell_end, a1), mid = (a + e) * 0.5f;
      int open = axz ? mg_point_open(mid, pl + out * 0.7f) : mg_point_open(pl + out * 0.7f, mid);
      // A run that starts or stops against another building stops 12 cm
      // short: at an inside corner that building's cornice comes the other
      // way, and the two must meet, not cross.
      if (open && run0 < 0.0f) run0 = a > a0 + 0.001f ? a + 0.12f : a;
      if ((!open || e >= a1 - 0.001f) && run0 >= 0.0f) {
        float re = open ? e : a - 0.12f;
        if (re - run0 > 0.05f) {
          if (axz) bx(run0, c0, pl, re, c1, pl + out * 0.12f, s->trim);
          else     bx(pl, c0, run0, pl + out * 0.12f, c1, re, s->trim);
        }
        run0 = -1.0f;
      }
      a = e;
    }
  }
  // Parapet, split round the stair gap.
  const float PT = 0.25f, PH = 0.7f;
  struct { int side; float a0, a1; int ax; float p0, p1; } par[4] = {
    {SIDE_N, x0, x1, 0, z0, z0 + PT}, {SIDE_S, x0, x1, 0, z1 - PT, z1},
    {SIDE_W, z0 + PT, z1 - PT, 1, x0, x0 + PT}, {SIDE_E, z0 + PT, z1 - PT, 1, x1 - PT, x1}};
  for (int k = 0; k < 4; k++) {
    float a0 = par[k].a0, a1 = par[k].a1;
    float segs[2][2] = {{a0, a1}, {0, 0}};
    int ns = 1;
    if (par[k].side == gap_side) {
      segs[0][1] = fmaxf(a0, gap_a0); segs[1][0] = fminf(a1, gap_a1); segs[1][1] = a1;
      ns = 2;
    }
    for (int q = 0; q < ns; q++) {
      if (segs[q][1] - segs[q][0] < 0.2f) continue;
      if (par[k].ax == 0) bx(segs[q][0], H, par[k].p0, segs[q][1], H + PH, par[k].p1, s->trim);
      else                bx(par[k].p0, H, segs[q][0], par[k].p1, H + PH, segs[q][1], s->trim);
    }
  }
  // Roof furniture, one item per quadrant at most, clear of the parapet and stair entry.
  if (roof_props) {
    float w = x1 - x0, d = z1 - z0;
    if (w > 5.0f && d > 5.0f) {
      uint32_t r = rng_u32();
      float gx0 = 1e9f, gz0 = 1e9f, gx1 = -1e9f, gz1 = -1e9f;
      if (gap_side == SIDE_N) { gx0 = gap_a0 - 0.9f; gx1 = gap_a1 + 0.9f; gz0 = z0; gz1 = z0 + 1.85f; }
      if (gap_side == SIDE_S) { gx0 = gap_a0 - 0.9f; gx1 = gap_a1 + 0.9f; gz0 = z1 - 1.85f; gz1 = z1; }
      if (gap_side == SIDE_W) { gx0 = x0; gx1 = x0 + 1.85f; gz0 = gap_a0 - 0.9f; gz1 = gap_a1 + 0.9f; }
      if (gap_side == SIDE_E) { gx0 = x1 - 1.85f; gx1 = x1; gz0 = gap_a0 - 0.9f; gz1 = gap_a1 + 0.9f; }
      #define RP_FREE(rx0, rz0, rx1, rz1) \
        (!(gap_side && (rx0) < gx1 && (rx1) > gx0 && (rz0) < gz1 && (rz1) > gz0))
      if (r & 1) {   // water tank on a plinth
        float tx = x0 + 1.0f, tz = z0 + 1.0f;
        if (RP_FREE(tx, tz, tx + 1.6f, tz + 1.6f)) {
          bx(tx, H, tz, tx + 1.6f, H + 0.3f, tz + 1.6f, s->stone);
          bx(tx + 0.1f, H + 0.3f, tz + 0.1f, tx + 1.5f, H + 1.6f, tz + 1.5f,
             rgb(0.25f, 0.27f, 0.30f));
        }
      }
      if (r & 2) {   // chimney / vent stack
        float tx = x1 - 1.6f, tz = z1 - 1.6f;
        if (RP_FREE(tx, tz, tx + 0.7f, tz + 0.7f))
          bx(tx, H, tz, tx + 0.7f, H + 1.9f, tz + 0.7f, tint(wall, 0.8f, 0.9f));
      }
      if (r & 4) {   // stair hut
        float tx = x1 - 3.2f, tz = z0 + 1.0f;
        if (tx > x0 + 3.0f && RP_FREE(tx, tz, tx + 2.0f, tz + 1.8f))
          bx(tx, H, tz, tx + 2.0f, H + 2.3f, tz + 1.8f, wall);
      }
      #undef RP_FREE
    }
  }
}

// A RAILING along one open edge of a stair flight or a raised floor: a curb
// on the tread, posts at both ends and every ~1.2 m between, a mid rail and a
// handrail at 0.98 m. ax 0 runs along x with its depth band at z = d0..d0+0.14
// (ax 1: along z, x = d0..). Everything sits ON the floor at `y`, so it adds
// nothing a body walking the flight can catch on; the posts start on the curb
// and the rails pass between them, so no two same-facing faces meet.
static void edge_rail(int ax, float d0, float a0, float a1, float y, v3 curb, v3 iron) {
  if (a1 - a0 < 0.30f) return;
#define ER(u0, u1, y0, y1, e0, e1, c) \
  (ax == 0 ? bx(u0, y0, e0, u1, y1, e1, c) : bx(e0, y0, u0, e1, y1, u1, c))
  int mf = g_mat_force;
  ER(a0, a1, y, y + 0.16f, d0, d0 + 0.14f, curb);
  g_mat_force = SMAT_STEEL;
  float L = a1 - a0;
  int n = (int)(L / 1.2f) + 1;
  for (int i = 0; i <= n; i++) {
    float u = a0 + 0.05f + (L - 0.16f) * (float)i / (float)n;
    ER(u, u + 0.06f, y + 0.16f, y + 0.93f, d0 + 0.04f, d0 + 0.10f, iron);
  }
  ER(a0 + 0.02f, a1 - 0.02f, y + 0.93f, y + 0.98f, d0 + 0.03f, d0 + 0.11f, iron);
  ER(a0 + 0.03f, a1 - 0.03f, y + 0.52f, y + 0.55f, d0 + 0.055f, d0 + 0.085f, iron);
  g_mat_force = mf;
#undef ER
}

// A straight stair from the ground up `rise` metres, along a wall: `n - 1` steps
// of depth `run`, a flat `land` top landing at height `rise`, plus a stepped stone
// stringer curb with dark-steel posts, a mid rail and a handrail at ~0.97 m.
static void stair_up(int ax, float plane, int inw, float a, int dir, float w, float rise,
                     float run, float land, v3 c, v3 rail) {
  int n = (int)(rise / PF_RISE + 0.5f);
  if (n < 2) n = 2;
  const float SW = 0.14f, SH = 0.18f;
  v3 cap = v3_scale(rail, 1.06f);
  v3 iron = rgb(0.16f, 0.16f, 0.18f);
  for (int k = 0; k < n - 1; k++) {
    float s0 = a + (float)dir * (float)k * run, s1 = s0 + (float)dir * run;
    float u0 = fminf(s0, s1), u1 = fmaxf(s0, s1), top = (float)(k + 1) * PF_RISE;
    fbox(ax, plane, inw, u0, u1, -w, 0.0f, 0.0f, top, tint(c, 0.97f, 1.03f));
    fbox(ax, plane, inw, u0, u1, -(w + SW), -w, 0.0f, top + SH, rail);
    fbox(ax, plane, inw, u0 + 0.01f, u1 - 0.01f, -(w + SW + 0.01f), -(w - 0.01f),
         top + SH, top + SH + 0.04f, cap);
    fbox(ax, plane, inw, u0 + 0.04f, u0 + 0.10f, -(w + 0.10f), -(w + 0.04f),
         top + 0.22f, top + 0.92f, iron);
    fbox(ax, plane, inw, u0 + 0.01f, u1 - 0.01f, -(w + 0.11f), -(w + 0.03f),
         top + 0.92f, top + 0.97f, iron);
    fbox(ax, plane, inw, u0 + 0.01f, u1 - 0.01f, -(w + 0.09f), -(w + 0.05f),
         top + 0.55f, top + 0.58f, iron);
  }
  float ls0 = a + (float)dir * (float)(n - 1) * run, ls1 = ls0 + (float)dir * land;
  float lu0 = fminf(ls0, ls1), lu1 = fmaxf(ls0, ls1);
  fbox(ax, plane, inw, lu0, lu1, -w, 0.0f, 0.0f, rise, tint(c, 0.97f, 1.03f));
  fbox(ax, plane, inw, lu0, lu1, -(w + SW), -w, 0.0f, rise + SH, rail);
  fbox(ax, plane, inw, lu0 + 0.01f, lu1 - 0.01f, -(w + SW + 0.01f), -(w - 0.01f),
       rise + SH, rise + SH + 0.04f, cap);
  fbox(ax, plane, inw, lu0 + 0.05f, lu0 + 0.11f, -(w + 0.10f), -(w + 0.04f),
       rise + 0.22f, rise + 0.92f, iron);
  fbox(ax, plane, inw, lu1 - 0.11f, lu1 - 0.05f, -(w + 0.10f), -(w + 0.04f),
       rise + 0.22f, rise + 0.92f, iron);
  fbox(ax, plane, inw, lu0 + 0.01f, lu1 - 0.01f, -(w + 0.11f), -(w + 0.03f),
       rise + 0.92f, rise + 0.97f, iron);
  fbox(ax, plane, inw, lu0 + 0.01f, lu1 - 0.01f, -(w + 0.09f), -(w + 0.05f),
       rise + 0.55f, rise + 0.58f, iron);
  // End parapet at the far end of the top landing (guides climber into the roof gap).
  float es0 = ls1 - (float)dir * 0.20f, es1 = ls1;
  fbox(ax, plane, inw, fminf(es0, es1), fmaxf(es0, es1), -w, 0.0f, rise, rise + 0.65f, rail);
  fbox(ax, plane, inw, fminf(es0, es1) + 0.01f, fmaxf(es0, es1) - 0.01f, -w + 0.01f, -0.02f,
       rise + 0.65f, rise + 0.70f, cap);
}

// A house you can stand on top of: a stair along its `stair_side` wall
// (which is left windowless) up to a flat top landing aligned with a gap in
// the parapet. Returns nothing; the roof is the reward.
static void house_climb(float x0, float z0, float x1, float z1, int floors, int win,
                        int doors, int stair_side, int dir, float start) {
  float H = (float)floors * PF_FLOOR;
  int n = (int)(H / PF_RISE + 0.5f);
  if (n < 2) n = 2;
  int ax = (stair_side == SIDE_N || stair_side == SIDE_S) ? 0 : 1;
  float wa0 = ax == 0 ? x0 : z0, wa1 = ax == 0 ? x1 : z1;
  float avail = (dir > 0 ? (wa1 - start) : (start - wa0)) - 0.25f;
  float land = 1.45f;
  float run = fminf(PF_RUN, fmaxf(0.38f, (avail - land) / (float)(n - 1)));
  if ((float)(n - 1) * run + land > avail && avail > 1.8f)
    land = fmaxf(0.95f, avail - (float)(n - 1) * run);
  float ls0 = start + (float)dir * (float)(n - 1) * run;
  float clear_end = ls0 + (float)dir * (land - 0.20f);
  float plane = stair_side == SIDE_N ? z0 : stair_side == SIDE_S ? z1
              : stair_side == SIDE_W ? x0 : x1;
  int inw = (stair_side == SIDE_N || stair_side == SIDE_W) ? +1 : -1;
  float g0 = fminf(ls0, clear_end), g1 = fmaxf(ls0, clear_end);
  house(x0, z0, x1, z1, floors, win & ~stair_side, doors & ~stair_side,
        stair_side, g0, g1, 1);
  stair_up(ax, plane, inw, start, dir, 1.20f, H, run, land, g_town.stone, g_town.trim);
}

// THE CLIMBING TOWER: a square core with a stair spiralling round it, one step
// a 0.4 m slab, a landing at each corner, ending on the core's roof — the
// lookout. Railing round the roof, open where the stair arrives. Above it,
// `cap` builds the crown (a lantern and spire, or nothing).
static void tower(float cx, float cz, float hc, float H, v3 core, v3 step, int cap,
                  int open_side) {
  const float W = 1.08f, SW = 0.10f, SH = 0.84f;
  v3 rc = g_town.trim;
  bx(cx - hc, 0.0f, cz - hc, cx + hc, H, cz + hc, core);
  int nside = (int)((2.0f * hc) / PF_RUN);
  float run = 2.0f * hc / (float)nside;
  int total = (int)(H / PF_RISE + 0.5f);
  int k = 0;
  // Sides in order: south (running east), east (running north), north
  // (running west), west (running south); each corner gets a landing + outer rails.
  for (int side = 0; k < total; side = (side + 1) % 4) {
    for (int i = 0; i < nside && k < total; i++, k++) {
      float top = (float)(k + 1) * PF_RISE, bot = top - PF_RISE;
      float a = -hc + (float)i * run, b = a + run;
      switch (side) {
        case 0:
          bx(cx + a, bot, cz + hc, cx + b, top, cz + hc + W, step);
          bx(cx + a, bot, cz + hc + W, cx + b, top + SH, cz + hc + W + SW, rc);
          if (k == total - 1)
            bx(cx + b, bot, cz + hc, cx + b + SW, top + SH - 0.02f, cz + hc + W, rc);
          break;
        case 1:
          bx(cx + hc, bot, cz - b, cx + hc + W, top, cz - a, step);
          bx(cx + hc + W, bot, cz - b, cx + hc + W + SW, top + SH, cz - a, rc);
          if (k == total - 1)
            bx(cx + hc, bot, cz - b - SW, cx + hc + W, top + SH - 0.02f, cz - b, rc);
          break;
        case 2:
          bx(cx - b, bot, cz - hc - W, cx - a, top, cz - hc, step);
          bx(cx - b, bot, cz - hc - W - SW, cx - a, top + SH, cz - hc - W, rc);
          if (k == total - 1)
            bx(cx - b - SW, bot, cz - hc - W, cx - b, top + SH - 0.02f, cz - hc, rc);
          break;
        default:
          bx(cx - hc - W, bot, cz + a, cx - hc, top, cz + b, step);
          bx(cx - hc - W - SW, bot, cz + a, cx - hc - W, top + SH, cz + b, rc);
          if (k == total - 1)
            bx(cx - hc - W, bot, cz + b, cx - hc, top + SH - 0.02f, cz + b + SW, rc);
          break;
      }
    }
    if (k >= total) break;
    float top = (float)k * PF_RISE, bot = top - PF_RISE;   // corner landing
    switch (side) {
      case 0:
        bx(cx + hc, bot, cz + hc, cx + hc + W, top, cz + hc + W, step);
        bx(cx + hc, bot, cz + hc + W, cx + hc + W, top + SH, cz + hc + W + SW, rc);
        bx(cx + hc + W, bot, cz + hc, cx + hc + W + SW, top + SH, cz + hc + W + SW, rc);
        break;
      case 1:
        bx(cx + hc, bot, cz - hc - W, cx + hc + W, top, cz - hc, step);
        bx(cx + hc + W, bot, cz - hc - W, cx + hc + W + SW, top + SH, cz - hc, rc);
        bx(cx + hc, bot, cz - hc - W - SW, cx + hc + W + SW, top + SH, cz - hc - W, rc);
        break;
      case 2:
        bx(cx - hc - W, bot, cz - hc - W, cx - hc, top, cz - hc, step);
        bx(cx - hc - W, bot, cz - hc - W - SW, cx - hc, top + SH, cz - hc - W, rc);
        bx(cx - hc - W - SW, bot, cz - hc - W - SW, cx - hc - W, top + SH, cz - hc, rc);
        break;
      default:
        bx(cx - hc - W, bot, cz + hc, cx - hc, top, cz + hc + W, step);
        bx(cx - hc - W - SW, bot, cz + hc, cx - hc - W, top + SH, cz + hc + W, rc);
        bx(cx - hc - W - SW, bot, cz + hc + W, cx - hc, top + SH, cz + hc + W + SW, rc);
        break;
    }
  }
  // Railing round the lookout, a gap on the side the stair finished on.
  int last = ((total - 1) / nside) % 4;
  const float RT = 0.12f, RH = 1.0f;
  if (last != 2 && open_side != 2) bx(cx - hc, H, cz - hc, cx + hc, H + RH, cz - hc + RT, rc);
  if (last != 0 && open_side != 0) bx(cx - hc, H, cz + hc - RT, cx + hc, H + RH, cz + hc, rc);
  if (last != 3 && open_side != 3)
    bx(cx - hc, H, cz - hc + RT, cx - hc + RT, H + RH, cz + hc - RT, rc);
  if (last != 1 && open_side != 1)
    bx(cx + hc - RT, H, cz - hc + RT, cx + hc, H + RH, cz + hc - RT, rc);
  if (cap == 1) {   // lantern: four posts and a roof, then a spire
    float p = 0.22f, ph = 2.4f;
    bx(cx - hc + RT, H, cz - hc + RT, cx - hc + RT + p, H + ph, cz - hc + RT + p, core);
    bx(cx + hc - RT - p, H, cz - hc + RT, cx + hc - RT, H + ph, cz - hc + RT + p, core);
    bx(cx - hc + RT, H, cz + hc - RT - p, cx - hc + RT + p, H + ph, cz + hc - RT, core);
    bx(cx + hc - RT - p, H, cz + hc - RT - p, cx + hc - RT, H + ph, cz + hc - RT, core);
    bx(cx - hc - 0.2f, H + ph, cz - hc - 0.2f, cx + hc + 0.2f, H + ph + 0.3f, cz + hc + 0.2f, rc);
    bx(cx - hc * 0.6f, H + ph + 0.3f, cz - hc * 0.6f, cx + hc * 0.6f, H + ph + 1.2f,
       cz + hc * 0.6f, core);
    bx(cx - 0.12f, H + ph + 1.2f, cz - 0.12f, cx + 0.12f, H + ph + 3.0f, cz + 0.12f, rc);
  }
}

// An octagonal drum (or a dome, as a stack of them): each layer a centre strip
// and two tabs that do not overlap it, so the plan is an octagon and no two
// tops share a plane.
static void octa(float cx, float cz, float r, float y0, float y1, v3 c) {
  float k = r * 0.414f, m = r * 0.72f;
  bx(cx - r, y0, cz - k, cx + r, y1, cz + k, c);
  bx(cx - 0.82f * r, y0, cz - m, cx + 0.82f * r, y1, cz - k, c);
  bx(cx - 0.82f * r, y0, cz + k, cx + 0.82f * r, y1, cz + m, c);
  bx(cx - 0.5f * r, y0, cz - r, cx + 0.5f * r, y1, cz - m, c);
  bx(cx - 0.5f * r, y0, cz + m, cx + 0.5f * r, y1, cz + r, c);
}
static void dome(float cx, float cz, float R, float y0, v3 c) {
  static const float RS[6] = {1.0f, 0.97f, 0.9f, 0.78f, 0.6f, 0.34f};
  float dh = R * 0.2f;
  for (int i = 0; i < 6; i++)
    octa(cx, cz, R * RS[i], y0 + dh * (float)i, y0 + dh * (float)(i + 1), c);
  bx(cx - 0.1f, y0 + dh * 6.0f, cz - 0.1f, cx + 0.1f, y0 + dh * 6.0f + 1.4f, cz + 0.1f,
     g_town.trim);
}

static void palm(float x, float z, float h) {
  v3 tr = rgb(0.42f, 0.32f, 0.22f), lf = rgb(0.26f, 0.42f, 0.18f);
  float seg = h / 3.0f, lean = rng_range(-0.25f, 0.25f);
  int first = g_num_solids;   // the boxes stay the collision; prop_palm is the tree
  for (int i = 0; i < 3; i++) {
    float o = lean * (float)i;
    bx(x - 0.18f + o, seg * (float)i, z - 0.18f, x + 0.18f + o, seg * (float)(i + 1),
       z + 0.18f, tint(tr, 0.9f, 1.1f));
  }
  float tx = x + lean * 2.0f, t = h;
  bx(tx - 0.22f, t, z - 2.4f, tx + 0.22f, t + 0.08f, z, lf);
  bx(tx - 0.22f, t + 0.1f, z, tx + 0.22f, t + 0.18f, z + 2.4f, lf);
  bx(tx, t + 0.05f, z - 0.22f, tx + 2.4f, t + 0.13f, z + 0.22f, lf);
  bx(tx - 2.4f, t + 0.15f, z - 0.22f, tx, t + 0.23f, z + 0.22f, lf);
  bx(tx - 0.2f, t - 0.6f, z - 2.9f, tx + 0.2f, t - 0.05f, z - 2.4f, lf);
  bx(tx - 0.2f, t - 0.5f, z + 2.4f, tx + 0.2f, t + 0.05f, z + 2.9f, lf);
  bx(tx + 2.4f, t - 0.55f, z - 0.2f, tx + 2.9f, t, z + 0.2f, lf);
  bx(tx - 2.9f, t - 0.45f, z - 0.2f, tx - 2.4f, t + 0.1f, z + 0.2f, lf);
  solids_hide(first);
  prop_add(PROP_PALM, x, z, 0, 0, lf, (v3){{h, lean, 0.0f}});
}

// A car (ax 0 = facing along x). Body, cabin, four wheels proud of the body.
static void car(float x, float z, int ax) {
  static const v3 PAINT[5] = {{{0.62f, 0.12f, 0.10f}}, {{0.82f, 0.80f, 0.74f}},
                              {{0.16f, 0.30f, 0.48f}}, {{0.20f, 0.22f, 0.24f}},
                              {{0.72f, 0.58f, 0.24f}}};
  v3 p = PAINT[rng_u32() % 5], w = rgb(0.08f, 0.08f, 0.09f);
  float L = 2.1f, W = 0.95f;
  int first = g_num_solids;
#define CB(a0, b0, a1, b1, y0, y1, c) \
  (ax ? bx(x + (b0), y0, z + (a0), x + (b1), y1, z + (a1), c) \
      : bx(x + (a0), y0, z + (b0), x + (a1), y1, z + (b1), c))
  CB(-L, -W, L, W, 0.32f, 0.95f, p);
  CB(-1.1f, -0.85f, 0.9f, 0.85f, 0.95f, 1.5f, tint(p, 0.85f, 0.9f));
  CB(-1.05f, -0.86f, 0.85f, 0.86f, 1.05f, 1.42f, g_town.glass);
  CB(-1.55f, -W - 0.05f, -0.85f, -W + 0.25f, 0.0f, 0.62f, w);
  CB(0.85f, -W - 0.05f, 1.55f, -W + 0.25f, 0.0f, 0.62f, w);
  CB(-1.55f, W - 0.25f, -0.85f, W + 0.05f, 0.0f, 0.62f, w);
  CB(0.85f, W - 0.25f, 1.55f, W + 0.05f, 0.0f, 0.62f, w);
#undef CB
  solids_hide(first);
  prop_add(PROP_CAR, x, z, ax, CAR_SEDAN, p, w);
}

// A single-cab PICKUP in the same 4.2 m footprint, sat higher: a body block to
// the bed rails and the cab over its front half. `flip` faces it the other way.
// It draws its paint from g_rng exactly as car() does, so swapping one for the
// other leaves every later draw — and so the rest of the map — unchanged.
static void pickup_truck(float x, float z, int ax, int flip) {
  static const v3 PAINT[5] = {{{0.82f, 0.80f, 0.74f}}, {{0.62f, 0.12f, 0.10f}},
                              {{0.30f, 0.34f, 0.26f}}, {{0.16f, 0.30f, 0.48f}},
                              {{0.70f, 0.66f, 0.56f}}};
  v3 p = PAINT[rng_u32() % 5], w = rgb(0.08f, 0.08f, 0.09f);
  float L = 2.12f, W = 0.95f, s = flip ? -1.0f : 1.0f;
  int first = g_num_solids;
#define CB(a0, b0, a1, b1, y0, y1, c) \
  (ax ? bx(x + (b0), y0, z + fminf((a0) * s, (a1) * s), x + (b1), y1, z + fmaxf((a0) * s, (a1) * s), c) \
      : bx(x + fminf((a0) * s, (a1) * s), y0, z + (b0), x + fmaxf((a0) * s, (a1) * s), y1, z + (b1), c))
  CB(-L, -W, L, W, 0.42f, 1.00f, p);
  CB(-0.46f, -0.86f, 0.95f, 0.86f, 1.00f, 1.66f, p);
  CB(-1.60f, -W - 0.05f, -1.00f, -W + 0.25f, 0.0f, 0.72f, w);
  CB(1.02f, -W - 0.05f, 1.62f, -W + 0.25f, 0.0f, 0.72f, w);
  CB(-1.60f, W - 0.25f, -1.00f, W + 0.05f, 0.0f, 0.72f, w);
  CB(1.02f, W - 0.25f, 1.62f, W + 0.05f, 0.0f, 0.72f, w);
#undef CB
  solids_hide(first);
  prop_add(PROP_CAR, x, z, ax, CAR_PICKUP | (flip ? CAR_FLIP : 0), p, w);
}

// STREET FURNITURE: a Blender-built mesh (PM_*) over hidden collision boxes,
// `qt` quarter turns about y. Each box is given in the prop's own frame
// (a along its forward, b across it) and turned with it. No g_rng.
typedef struct { float a0, a1, b0, b1, y0, y1; } pbox_t;
static void pbox_world(const pbox_t *b, float x, float z, int qt, float *ox0, float *oz0,
                       float *ox1, float *oz1) {
  float c[4][2] = {{b->a0, b->b0}, {b->a1, b->b0}, {b->a0, b->b1}, {b->a1, b->b1}};
  float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
  for (int k = 0; k < 4; k++) {
    float a = c[k][0], bb = c[k][1], wx, wz;
    switch (qt & 3) {          // F = (1,0,0) turned qt times; L follows it
      case 0: wx = a;   wz = bb;  break;
      case 1: wx = -bb; wz = a;   break;
      case 2: wx = -a;  wz = -bb; break;
      default: wx = bb; wz = -a;  break;
    }
    x0 = fminf(x0, wx); x1 = fmaxf(x1, wx); z0 = fminf(z0, wz); z1 = fmaxf(z1, wz);
  }
  *ox0 = x + x0; *oz0 = z + z0; *ox1 = x + x1; *oz1 = z + z1;
}
static void street_prop_y(int id, float x, float y, float z, int qt, v3 paint, v3 paint2,
                          const pbox_t *bxs, int nb) {
  int first = g_num_solids;
  for (int i = 0; i < nb; i++) {
    float x0, z0, x1, z1;
    pbox_world(&bxs[i], x, z, qt, &x0, &z0, &x1, &z1);
    bx(x0, y + bxs[i].y0, z0, x1, y + bxs[i].y1, z1, paint);
  }
  solids_hide(first);
  prop_add_y(PROP_MESH, x, y, z, qt & 3, id, paint, paint2);
}
static void street_prop(int id, float x, float z, int qt, v3 paint, v3 paint2,
                        const pbox_t *bxs, int nb) {
  street_prop_y(id, x, 0.0f, z, qt, paint, paint2, bxs, nb);
}
static void genset(float x, float z, int qt, v3 paint, v3 paint2) {
  static const pbox_t B[] = {{-1.08f, 1.08f, -0.49f, 0.49f, 0.0f, 1.32f}};
  street_prop(PM_GENSET, x, z, qt, paint, paint2, B, 1);
}
static void forklift_prop(float x, float z, int qt, v3 paint, v3 paint2) {
  static const pbox_t B[] = {{-1.28f, 0.72f, -0.55f, 0.55f, 0.0f, 0.94f},
                             {-0.96f, 0.38f, -0.42f, 0.42f, 0.94f, 2.06f},
                             {0.72f, 0.92f, -0.42f, 0.42f, 0.10f, 2.06f},
                             {0.88f, 1.74f, -0.29f, 0.29f, 0.0f, 0.10f}};
  street_prop(PM_FORKLIFT, x, z, qt, paint, paint2, B, 4);
}
static void dumpster(float x, float z, int qt, v3 paint) {
  static const pbox_t B[] = {{-0.72f, 0.72f, -0.56f, 0.56f, 0.0f, 1.16f}};
  street_prop(PM_DUMPSTER, x, z, qt, paint, paint, B, 1);
}
static void motorbike(float x, float z, int qt, v3 paint) {
  static const pbox_t B[] = {{-0.92f, 0.92f, -0.22f, 0.22f, 0.0f, 1.02f}};
  street_prop(PM_MOTO, x, z, qt, paint, paint, B, 1);
}
static void cafe_set(float x, float z, int qt) {
  static const pbox_t B[] = {{-0.37f, 0.37f, -0.37f, 0.37f, 0.0f, 0.76f},       // table
                             {0.40f, 0.86f, -0.24f, 0.24f, 0.0f, 0.88f},        // chair, facing it
                             {-0.34f, 0.14f, 0.40f, 0.86f, 0.0f, 0.88f}};       // the other chair
  street_prop(PM_CAFE, x, z, qt, rgb(0.9f, 0.9f, 0.87f), rgb(0.9f, 0.9f, 0.87f), B, 3);
}
// A position hash for world-gen choices that must not draw from g_rng (which
// would re-roll the rest of the map after them).
static float wg_hash(float x, float z, uint32_t salt) {
  uint32_t h = (uint32_t)(int32_t)lrintf(x * 37.0f) * 73856093u ^
               (uint32_t)(int32_t)lrintf(z * 37.0f) * 19349663u ^ salt * 83492791u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return (float)(h & 0xFFFFFFu) / 16777216.0f;
}
static int wg_box_free(v3 mn, v3 mx) {
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *s = &g_solids[i];
    if (mn.x < s->max.x && mx.x > s->min.x && mn.y < s->max.y && mx.y > s->min.y &&
        mn.z < s->max.z && mx.z > s->min.z) return 0;
  }
  return 1;
}
// ROOFTOP WATER TANKS: a polyethylene tank on its steel stand, on the flat roofs
// of the taller houses. They are SOLID — a climbable roof is played on, and a
// tank a round passes through is cover that lies. Placed by hash, inset from
// the parapets, only where the whole stand is clear.
static void roof_tanks(int max_n) {
  static const v3 TC[3] = {{{0.10f, 0.10f, 0.11f}}, {{0.20f, 0.34f, 0.55f}}, {{0.84f, 0.84f, 0.80f}}};
  static const pbox_t SB[] = {{-0.62f, 0.68f, -0.84f, 0.84f, 0.0f, 1.56f}};
  int n0 = g_num_solids, n = 0;
  for (int i = 0; i < n0 && n < max_n; i++) {
    const solid_t *s = &g_solids[i];
    float sx = s->max.x - s->min.x, sz = s->max.z - s->min.z, top = s->max.y;
    // a house core from the ground, or the roof slab over one
    if (g_solid_hide[i] || sx < 3.6f || sz < 3.6f || top < 5.5f ||
        (s->min.y > 0.05f && s->max.y - s->min.y > 0.6f)) continue;
    float cx = (s->min.x + s->max.x) * 0.5f, cz = (s->min.z + s->max.z) * 0.5f;
    if (wg_hash(cx, cz, 11u) > 0.50f) continue;
    int solar = (g_town_kind == 0 || g_town_kind == 3) && sx >= 4.2f && sz >= 4.2f &&
                wg_hash(cx, cz, 16u) > 0.45f;
    float margin = solar ? 2.8f : 2.2f;
    float x = cx + (wg_hash(cx, cz, 12u) - 0.5f) * fmaxf(0.0f, sx - margin);
    float z = cz + (wg_hash(cx, cz, 13u) - 0.5f) * fmaxf(0.0f, sz - margin);
    int qt = (int)(wg_hash(cx, cz, 15u) * 4.0f) & 3;
    if (solar) {
      float x0, z0, x1, z1;
      pbox_world(&SB[0], x, z, qt, &x0, &z0, &x1, &z1);
      if (!wg_box_free((v3){{x0 - 0.04f, top + 0.001f, z0 - 0.04f}},
                       (v3){{x1 + 0.04f, top + 1.60f, z1 + 0.04f}})) continue;
      street_prop_y(PM_SOLAR, x, top, z, qt, rgb(0.84f, 0.84f, 0.80f),
                    rgb(0.18f, 0.34f, 0.52f), SB, 1);
    } else {
      v3 mn = {{x - 0.53f, top + 0.001f, z - 0.53f}}, mx = {{x + 0.53f, top + 1.80f, z + 0.53f}};
      if (!wg_box_free(mn, mx)) continue;
      v3 c = TC[(int)(wg_hash(cx, cz, 14u) * 3.0f) % 3];
      int first = g_num_solids;
      bx(x - 0.50f, top, z - 0.50f, x + 0.50f, top + 1.77f, z + 0.50f, c);
      solids_hide(first);
      prop_add_y(PROP_MESH, x, top, z, qt, PM_WTANK, c, c);
    }
    n++;
  }
}
// WALL UNITS: split-system AC condensers on brackets above head height on the
// street walls, and satellite dishes on the parapets. Render-only (above the
// 1.8 m gameplay band, on walls nobody stands in), so props without solids.
// Each needs bare wall behind it — never a window — and open air where it sits.
static int wg_inside(v3 p, v3 *col) {
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *s = &g_solids[i];
    if (p.x > s->min.x && p.x < s->max.x && p.y > s->min.y && p.y < s->max.y &&
        p.z > s->min.z && p.z < s->max.z) { if (col) *col = s->color; return 1; }
  }
  return 0;
}
static int wg_same(v3 a, v3 b) { return fabsf(a.x - b.x) + fabsf(a.y - b.y) + fabsf(a.z - b.z) < 1e-4f; }
static void wall_units(float p_ac, float p_dish) {
  const town_style_t *ts = &g_town;
  int n0 = g_num_solids;
  for (int i = 0; i < n0; i++) {
    const solid_t *s = &g_solids[i];
    v3 mn = s->min, mx = s->max;
    float sx = mx.x - mn.x, sy = mx.y - mn.y, sz = mx.z - mn.z;
    int skin = mn.y < 0.05f && sy > 3.6f && (fabsf(sx - PF_SKIN) < 0.01f || fabsf(sz - PF_SKIN) < 0.01f) &&
               fmaxf(sx, sz) > 2.2f;
    int parapet = wg_same(s->color, ts->trim) && fabsf(sy - 0.70f) < 0.02f && mn.y > 2.9f &&
                  (fabsf(sx - 0.25f) < 0.02f || fabsf(sz - 0.25f) < 0.02f) && fmaxf(sx, sz) > 2.0f;
    if (!skin && !parapet) continue;
    int ax = skin ? (fabsf(sx - PF_SKIN) < 0.01f ? 0 : 2) : (fabsf(sx - 0.25f) < 0.02f ? 0 : 2);
    float cx = (mn.x + mx.x) * 0.5f, cz = (mn.z + mx.z) * 0.5f;
    if (wg_hash(cx, cz, skin ? 21u : 31u) > (skin ? p_ac : p_dish)) continue;
    // which face is the street: probe past both
    float py = skin ? 1.5f : mn.y - 0.6f;
    v3 pp = ax == 0 ? (v3){{mx.x + 0.45f, py, cz}} : (v3){{cx, py, mx.z + 0.45f}};
    v3 pn = ax == 0 ? (v3){{mn.x - 0.45f, py, cz}} : (v3){{cx, py, mn.z - 0.45f}};
    int ip = wg_inside(pp, NULL), in = wg_inside(pn, NULL);
    if (ip == in) continue;
    int o = ip ? -1 : 1;
    float pl = ax == 0 ? (o > 0 ? mx.x : mn.x) : (o > 0 ? mx.z : mn.z);
    float u0 = ax == 0 ? mn.z : mn.x, u1 = ax == 0 ? mx.z : mx.x;
    if (u1 - u0 < 1.8f) continue;
    float u = u0 + 0.8f + wg_hash(cx, cz, 22u) * (u1 - u0 - 1.6f);
    float yb = skin ? (wg_hash(cx, cz, 23u) < 0.5f || sy < 6.0f ? 2.30f : 5.45f) : mn.y + 0.02f;
#define WP(t, du, y) (ax == 0 ? (v3){{pl + (float)o * (t), (y), u + (du)}} \
                              : (v3){{u + (du), (y), pl + (float)o * (t)}})
    int ok = 1;
    float hw = skin ? 0.46f : 0.40f, hh = skin ? 0.66f : 0.70f, dd = skin ? 0.46f : 0.95f;
    for (int a = 0; a < 3 && ok; a++)
      for (int b = 0; b < 3 && ok; b++)
        for (int c = 0; c < 3 && ok; c++)
          if (wg_inside(WP(0.04f + dd * 0.5f * (float)a, hw * (float)(b - 1), yb + 0.03f + hh * 0.5f * (float)c), NULL))
            ok = 0;
    for (int b = 0; b < 3 && ok; b++)
      for (int c = 0; c < 2 && ok; c++) {
        v3 wc;
        if (!wg_inside(WP(-0.06f, hw * (float)(b - 1), yb + 0.1f + 0.45f * (float)c), &wc) ||
            wg_same(wc, ts->glass)) ok = 0;
      }
    if (skin && ok && wg_inside(WP(0.08f, 0.43f, yb + 1.15f), NULL)) ok = 0;      // the pipe run
#undef WP
    // the outer ring's backs face out of the arena, where nobody looks
    float ox = ax == 0 ? pl + (float)o : u, oz = ax == 0 ? u : pl + (float)o;
    if (fabsf(ox) > ARENA_HALF - 0.5f || fabsf(oz) > ARENA_HALF - 0.5f) ok = 0;
    if (!ok) continue;
    // F = out of the wall: +x, +z, -x, -z are quarter turns 0..3 (prop_mesh)
    int qt = ax == 0 ? (o > 0 ? 0 : 2) : (o > 0 ? 1 : 3);
    float x = ax == 0 ? pl : u, z = ax == 0 ? u : pl;
    v3 c = rgb(0.86f, 0.84f, 0.78f);
    prop_add_y(PROP_MESH, x, yb, z, qt, skin ? PM_AC : PM_DISH, c, c);
  }
}
// THE YARD AND STREET KIT (tools/props_models.py): an IBC tote, a pallet of
// sacks, a bench, an electrical cabinet. Each faces +x (out from a wall) at
// qt 0, and its collision is the block-out of what a round would hit.
static void ibc_tote(float x, float z, int qt, v3 paint, v3 cap) {
  static const pbox_t B[] = {{-0.60f, 0.60f, -0.50f, 0.50f, 0.0f, 1.16f}};
  int mf = g_mat_force; g_mat_force = SMAT_STEEL;
  street_prop(PM_IBC, x, z, qt, paint, cap, B, 1);
  g_mat_force = mf;
}
static void sack_pallet(float x, float z, int qt, v3 paint) {
  static const pbox_t B[] = {{-0.60f, 0.60f, -0.50f, 0.50f, 0.0f, 0.80f}};
  int mf = g_mat_force; g_mat_force = SMAT_STUCCO;
  street_prop(PM_SACKS, x, z, qt, paint, paint, B, 1);
  g_mat_force = mf;
}
static void bench_prop(float x, float z, int qt, v3 slats) {
  static const pbox_t B[] = {{-0.28f, 0.32f, -0.82f, 0.82f, 0.0f, 0.49f},
                             {-0.32f, -0.18f, -0.82f, 0.82f, 0.49f, 0.90f}};
  int mf = g_mat_force; g_mat_force = SMAT_WOOD;
  street_prop(PM_BENCH, x, z, qt, slats, slats, B, 2);
  g_mat_force = mf;
}
static void elec_cabinet(float x, float z, int qt, v3 paint) {
  static const pbox_t B[] = {{-0.28f, 0.28f, -0.66f, 0.66f, 0.0f, 1.53f}};
  int mf = g_mat_force; g_mat_force = SMAT_STEEL;
  street_prop(PM_ELEC, x, z, qt, paint, v3_scale(paint, 0.9f), B, 1);
  g_mat_force = mf;
}
static void tyre_stack(float x, float z, int qt) {
  static const pbox_t B[] = {{-0.34f, 0.36f, -0.34f, 0.36f, 0.0f, 0.74f},
                             {0.38f, 0.72f, -0.30f, 0.38f, 0.0f, 0.66f}};
  street_prop(PM_TYRES, x, z, qt, rgb(0.06f, 0.06f, 0.06f), rgb(0.06f, 0.06f, 0.06f), B, 2);
}

static void barrel(float x, float z, v3 c) {
  int first = g_num_solids;
  bx(x - 0.3f, 0.0f, z - 0.3f, x + 0.3f, 0.92f, z + 0.3f, c);
  bx(x - 0.32f, 0.42f, z - 0.32f, x + 0.32f, 0.5f, z + 0.32f, tint(c, 0.6f, 0.7f));
  solids_hide(first);
  prop_add(PROP_BARREL, x, z, 0, 0, c, c);
}

static void lamp(float x, float z, int ax, int dir) {
  v3 m = rgb(0.16f, 0.17f, 0.18f);
  bx(x - 0.07f, 0.0f, z - 0.07f, x + 0.07f, 4.2f, z + 0.07f, m);
  float d = (float)dir * 0.9f;
  if (ax == 0) bx(fminf(x, x + d), 3.98f, z - 0.05f, fmaxf(x, x + d), 4.08f, z + 0.05f, m);
  else         bx(x - 0.05f, 3.98f, fminf(z, z + d), x + 0.05f, 4.08f, fmaxf(z, z + d), m);
  float lx = ax == 0 ? x + d : x, lz = ax == 0 ? z : z + d;
  int head = g_num_solids;
  bx(lx - 0.18f, 3.72f, lz - 0.18f, lx + 0.18f, 3.98f, lz + 0.18f, rgb(0.95f, 0.86f, 0.62f));
  solids_hide(head);                        // the lantern is a prop (it glows)
  prop_add(PROP_LAMP, lx, lz, ax, dir, rgb(0.95f, 0.86f, 0.62f), m);
}

// A shipping container, 6.1 x 2.44 x 2.6, along x (ax 0) or z; `y` stacks it.
static void container(float x, float z, int ax, float y, v3 c) {
  float L = 3.05f, W = 1.22f, H = 2.6f;
  g_mat_force = SMAT_CONTAINER;
  if (ax == 0) bx(x - L, y, z - W, x + L, y + H, z + W, c);
  else         bx(x - W, y, z - L, x + W, y + H, z + L, c);
  g_mat_force = -1;
}

// Market stall (one copy; the open maps' stall() mirrors).
static void stall1(float cx, float cz, int ax, v3 awn) {
#define ST1(a0, b0, a1, b1, y0, y1, c) \
  (ax ? bx(cx + (b0), y0, cz + (a0), cx + (b1), y1, cz + (a1), c) \
      : bx(cx + (a0), y0, cz + (b0), cx + (a1), y1, cz + (b1), c))
  ST1(-1.20f, -0.40f, 1.20f, 0.40f, 0.0f, 1.0f, rgb(0.50f, 0.36f, 0.22f));
  ST1(-1.38f, 0.82f, -1.22f, 0.98f, 0.0f, 2.30f, rgb(0.34f, 0.24f, 0.15f));
  ST1(1.22f, 0.82f, 1.38f, 0.98f, 0.0f, 2.30f, rgb(0.34f, 0.24f, 0.15f));
  ST1(-1.50f, -0.70f, 1.50f, 1.30f, 2.30f, 2.42f, awn);
#undef ST1
}

// An arched gate across a street: two piers, the wall over the opening, and a
// stepped soffit that reads as an arch. Opening from a0..a1 along `ax`, wall
// thickness t centred on `p`.
static void arch_gate(int ax, float p, float a0, float a1, float wa0, float wa1, float H, v3 c) {
  float t = 0.9f, oh = 3.6f;
  float o = (a1 - a0);
#define AG(u0, u1, y0, y1) \
  (ax == 0 ? bx(u0, y0, p - t * 0.5f, u1, y1, p + t * 0.5f, c) \
           : bx(p - t * 0.5f, y0, u0, p + t * 0.5f, y1, u1, c))
  AG(wa0, a0, 0.0f, H);
  AG(a1, wa1, 0.0f, H);
  AG(a0 + o * 0.22f, a1 - o * 0.22f, oh, H);
  AG(a0, a0 + o * 0.22f, oh - 0.55f, H);
  AG(a1 - o * 0.22f, a1, oh - 0.55f, H);
#undef AG
  // crenellations along the top
  for (float u = wa0; u + 0.6f <= wa1; u += 1.2f)
    ax == 0 ? bx(u, H, p - t * 0.5f, u + 0.6f, H + 0.6f, p + t * 0.5f, c)
            : bx(p - t * 0.5f, H, u, p + t * 0.5f, H + 0.6f, u + 0.6f, c);
}

static void tank(float cx, float cz, float r, float h, v3 c) {
  octa(cx, cz, r, 0.0f, h, c);
  octa(cx, cz, r + 0.08f, h * 0.5f - 0.18f, h * 0.5f + 0.18f, tint(c, 0.62f, 0.68f));
  octa(cx, cz, r * 0.86f, h, h + 0.55f, tint(c, 0.9f, 0.95f));
  bx(cx - 0.3f, h + 0.55f, cz - 0.3f, cx + 0.3f, h + 1.1f, cz + 0.3f, g_town.trim);
}

static void truck(float x, float z, int ax, v3 c) {
  v3 cab = rgb(0.72f, 0.16f, 0.12f), blk = rgb(0.08f, 0.08f, 0.09f);
  int first = g_num_solids;
#define TK(a0, b0, a1, b1, y0, y1, cc) \
  (ax ? bx(x + (b0), y0, z + (a0), x + (b1), y1, z + (a1), cc) \
      : bx(x + (a0), y0, z + (b0), x + (a1), y1, z + (b1), cc))
  TK(-3.6f, -1.1f, 3.4f, 1.1f, 0.55f, 1.2f, rgb(0.20f, 0.21f, 0.22f));   // chassis
  TK(3.4f, -1.15f, 5.4f, 1.15f, 0.55f, 3.0f, cab);                       // cab
  TK(5.4f, -0.95f, 5.44f, 0.95f, 1.8f, 2.7f, g_town.glass);              // windscreen
  TK(-3.0f, -1.2f, -1.8f, -0.95f, 0.0f, 0.95f, blk);
  TK(-3.0f, 0.95f, -1.8f, 1.2f, 0.0f, 0.95f, blk);
  TK(3.8f, -1.2f, 5.0f, -0.95f, 0.0f, 0.95f, blk);
  TK(3.8f, 0.95f, 5.0f, 1.2f, 0.0f, 0.95f, blk);
#undef TK
  solids_hide(first);                       // drawn by mesh_props instead
  prop_add(PROP_TRUCK, x, z, ax, 0, cab, c);
  container(ax ? x : x - 0.1f, ax ? z - 0.1f : z, ax, 1.2f, c);
}

