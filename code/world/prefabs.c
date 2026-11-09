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
    s->plaster[0] = rgb(0.80f, 0.68f, 0.50f); s->plaster[1] = rgb(0.86f, 0.80f, 0.68f);
    s->plaster[2] = rgb(0.74f, 0.55f, 0.38f); s->plaster[3] = rgb(0.90f, 0.86f, 0.78f);
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
      if (is_door) {   // a lintel band over the door, above head height
        fbox(ax, plane, inw, o0 - 0.15f, o1 + 0.15f, -0.08f, 0.0f, 2.3f, 2.5f, s->trim);
        if (s->awnings)
          fbox(ax, plane, inw, o0 - 0.5f, o1 + 0.5f, -1.3f, -0.08f, 2.62f, 2.72f,
               s->awning[rng_u32() % 4]);
      } else if (f == 0 && s->awnings && (rng_u32() % 4) == 0) {
        fbox(ax, plane, inw, o0 - 0.4f, o1 + 0.4f, -1.1f, 0.0f, 2.62f, 2.72f,
             s->awning[rng_u32() % 4]);
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
  // Roof furniture, one item per quadrant at most, clear of the parapet.
  if (roof_props) {
    float w = x1 - x0, d = z1 - z0;
    if (w > 5.0f && d > 5.0f) {
      uint32_t r = rng_u32();
      if (r & 1) {   // water tank on a plinth
        float tx = x0 + 1.0f, tz = z0 + 1.0f;
        bx(tx, H, tz, tx + 1.6f, H + 0.3f, tz + 1.6f, s->stone);
        bx(tx + 0.1f, H + 0.3f, tz + 0.1f, tx + 1.5f, H + 1.6f, tz + 1.5f,
           rgb(0.25f, 0.27f, 0.30f));
      }
      if (r & 2) {   // chimney / vent stack
        float tx = x1 - 1.6f, tz = z1 - 1.6f;
        bx(tx, H, tz, tx + 0.7f, H + 1.9f, tz + 0.7f, tint(wall, 0.8f, 0.9f));
      }
      if (r & 4) {   // stair hut
        float tx = x1 - 3.2f, tz = z0 + 1.0f;
        if (tx > x0 + 3.0f) bx(tx, H, tz, tx + 2.0f, H + 2.3f, tz + 1.8f, wall);
      }
    }
  }
}

// A straight stair from the ground up `rise` metres, along a wall: steps run
// from (a, t) in direction `dir` (+1/-1 along the wall), `w` wide outward from
// the wall plane. Solid to the ground — it reads as a masonry stair.
static void stair_up(int ax, float plane, int inw, float a, int dir, float w, float rise, v3 c) {
  int n = (int)(rise / PF_RISE + 0.5f);
  for (int k = 0; k < n; k++) {
    float s0 = a + (float)dir * (float)k * PF_RUN, s1 = s0 + (float)dir * PF_RUN;
    fbox(ax, plane, inw, fminf(s0, s1), fmaxf(s0, s1), -w, 0.0f, 0.0f,
         (float)(k + 1) * PF_RISE, tint(c, 0.97f, 1.03f));
  }
}

// A house you can stand on top of: a stair along its `stair_side` wall
// (which is left windowless) up to a gap in the parapet. Returns nothing; the
// roof is the reward.
static void house_climb(float x0, float z0, float x1, float z1, int floors, int win,
                        int doors, int stair_side, int dir, float start) {
  float H = (float)floors * PF_FLOOR;
  float len = H / PF_RISE * PF_RUN;
  int ax = (stair_side == SIDE_N || stair_side == SIDE_S) ? 0 : 1;
  float end = start + (float)dir * len;
  float plane = stair_side == SIDE_N ? z0 : stair_side == SIDE_S ? z1
              : stair_side == SIDE_W ? x0 : x1;
  int inw = (stair_side == SIDE_N || stair_side == SIDE_W) ? +1 : -1;
  float g0 = fminf(end, end - (float)dir * 1.4f), g1 = fmaxf(end, end - (float)dir * 1.4f);
  house(x0, z0, x1, z1, floors, win & ~stair_side, doors & ~stair_side,
        stair_side, g0, g1, 1);
  stair_up(ax, plane, inw, start, dir, 1.3f, H, g_town.stone);
}

// THE CLIMBING TOWER: a square core with a stair spiralling round it, one step
// a 0.4 m slab, a landing at each corner, ending on the core's roof — the
// lookout. Railing round the roof, open where the stair arrives. Above it,
// `cap` builds the crown (a lantern and spire, or nothing).
static void tower(float cx, float cz, float hc, float H, v3 core, v3 step, int cap,
                  int open_side) {
  const float W = 1.2f;
  bx(cx - hc, 0.0f, cz - hc, cx + hc, H, cz + hc, core);
  int nside = (int)((2.0f * hc) / PF_RUN);
  float run = 2.0f * hc / (float)nside;
  int total = (int)(H / PF_RISE + 0.5f);
  int k = 0;
  // Sides in order: south (running east), east (running north), north
  // (running west), west (running south); each corner gets a landing.
  for (int side = 0; k < total; side = (side + 1) % 4) {
    for (int i = 0; i < nside && k < total; i++, k++) {
      float top = (float)(k + 1) * PF_RISE, a = -hc + (float)i * run, b = a + run;
      switch (side) {
        case 0: bx(cx + a, top - PF_RISE, cz + hc, cx + b, top, cz + hc + W, step); break;
        case 1: bx(cx + hc, top - PF_RISE, cz - b, cx + hc + W, top, cz - a, step); break;
        case 2: bx(cx - b, top - PF_RISE, cz - hc - W, cx - a, top, cz - hc, step); break;
        default: bx(cx - hc - W, top - PF_RISE, cz + a, cx - hc, top, cz + b, step); break;
      }
    }
    if (k >= total) break;
    float top = (float)k * PF_RISE;   // landing at the last step's height
    switch (side) {
      case 0: bx(cx + hc, top - PF_RISE, cz + hc, cx + hc + W, top, cz + hc + W, step); break;
      case 1: bx(cx + hc, top - PF_RISE, cz - hc - W, cx + hc + W, top, cz - hc, step); break;
      case 2: bx(cx - hc - W, top - PF_RISE, cz - hc - W, cx - hc, top, cz - hc, step); break;
      default: bx(cx - hc - W, top - PF_RISE, cz + hc, cx - hc, top, cz + hc + W, step); break;
    }
  }
  // Railing round the lookout, a gap on the side the stair finished on.
  int last = ((total - 1) / nside) % 4;
  v3 rc = g_town.trim;
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
  prop_add(PROP_CAR, x, z, ax, 0, p, w);
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

