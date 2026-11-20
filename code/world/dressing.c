// code/world/dressing.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// THE SMALL MAPS' DRESSING KIT. The 48 m maps were built from a dozen palette
// boxes each — a place you could fight in but not a place. These are the real
// objects that stand in those boxes' footprints (sandbags, HESCO, blast walls,
// crates, pallets, pipe runs, furnaces, a crane) and they obey the same three
// rules every map does: nothing taller than a step without a step up to it,
// no two same-facing faces sharing a plane (`mapcheck`), and — because these
// maps are 180-degree symmetric — every half-map piece is built through `s`,
// the rotation sign (x, z) -> (s x, s z). Nothing here draws from g_rng: a
// tint comes from a hash of the position, so dressing never re-rolls a map.
// ---------------------------------------------------------------------------
static void ladder_add(float x, float z, int side);
static float g_ks = 1.0f;   // the half being built: +1, or -1 for its mirror
#define KB(x0, y0, z0, x1, y1, z1, c) \
  bx(g_ks * (x0), y0, g_ks * (z0), g_ks * (x1), y1, g_ks * (z1), c)
static float khash(float x, float z, int salt) {
  uint32_t h = (uint32_t)(int32_t)(x * 97.0f) * 73856093u ^
               (uint32_t)(int32_t)(z * 89.0f) * 19349663u ^ (uint32_t)salt * 83492791u;
  h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
  return (float)(h >> 8) * (1.0f / 16777216.0f);
}
static v3 kt_(v3 c, float x, float z, int salt, float lo, float hi) {
  float t = lo + (hi - lo) * khash(x, z, salt);
  return (v3){{c.x * t, c.y * t, c.z * t}};
}
// Forced material for the boxes a statement adds.
#define KMAT(m, ...) do { int _pm = g_mat_force; g_mat_force = (m); __VA_ARGS__; g_mat_force = _pm; } while (0)

// A wooden crate: body, a lid band proud at the top, a skid under it.
static void k_crate(float x, float z, float hw, float hd, float h, v3 c) {
  KMAT(SMAT_WOOD, {
    KB(x - hw, 0.10f, z - hd, x + hw, h - 0.07f, z + hd, kt_(c, x, z, 1, 0.92f, 1.06f));
    KB(x - hw - 0.02f, h - 0.07f, z - hd - 0.02f, x + hw + 0.02f, h, z + hd + 0.02f,
       kt_(c, x, z, 2, 0.70f, 0.78f));
    KB(x - hw + 0.06f, 0.0f, z - hd + 0.06f, x + hw - 0.06f, 0.10f, z + hd - 0.06f,
       kt_(c, x, z, 3, 0.55f, 0.62f));
  });
}
// A stack of n pallets (1.2 x 1.0 each), slat gaps shown as a darker core.
static void k_pallets(float x, float z, int ax, int n) {
  v3 w = rgb(0.56f, 0.44f, 0.30f);
  float a = ax ? 0.5f : 0.6f, b = ax ? 0.6f : 0.5f;
  KMAT(SMAT_WOOD, for (int i = 0; i < n; i++) {
    float y = 0.144f * (float)i;
    KB(x - a, y, z - b, x + a, y + 0.024f, z + b, kt_(w, x, z, 10 + i, 0.88f, 1.08f));
    KB(x - a + 0.04f, y + 0.024f, z - b + 0.04f, x + a - 0.04f, y + 0.120f, z + b - 0.04f,
       kt_(w, x, z, 20 + i, 0.50f, 0.58f));
    KB(x - a, y + 0.120f, z - b, x + a, y + 0.144f, z + b, kt_(w, x, z, 30 + i, 0.90f, 1.10f));
  });
}
// SANDBAGS. A wall of bags laid in courses, alternate courses offset half a
// bag, each course stepped in 15 mm (a battered face), each bag its own shade.
// Along x (ax 0) from a0 to a1 centred on `c`, `rows` courses high.
static void k_sandbags(int ax, float c, float a0, float a1, float yb, int rows, float th) {
  v3 sb = rgb(0.62f, 0.56f, 0.42f);
  const float BL = 0.56f, BH = 0.155f;
  KMAT(SMAT_STUCCO, for (int r = 0; r < rows; r++) {
    float y0 = yb + BH * (float)r, y1 = y0 + BH;
    float d = th * 0.5f - 0.015f * (float)r;
    float off = (r & 1) ? BL * 0.5f : 0.0f;
    float u = a0;
    for (int k = 0; u < a1 - 0.01f; k++) {
      float v = k == 0 && off > 0.0f ? a0 + off : u + BL;
      if (v > a1) v = a1;
      float dj = 0.012f * (khash(u, c, r) - 0.5f);
      v3 col = kt_(sb, u, c + (float)r, r, 0.84f, 1.10f);
      if (ax == 0) KB(u, y0, c - d - dj, v, y1, c + d + dj, col);
      else         KB(c - d - dj, y0, u, c + d + dj, y1, v, col);
      u = v;
    }
  });
}
// HESCO: wire-mesh baskets of fill, 1.06 m square, `h` tall, a row of n along
// ax from (x, z). The fill is the collision; the corner posts and the top rim
// are proud by 15 mm, so the mesh reads at range as a grid of bays.
static void k_hesco(float x, float z, int ax, int n, float h) {
  const float U = 1.06f;
  v3 fill = rgb(0.66f, 0.60f, 0.47f), wire = rgb(0.34f, 0.35f, 0.33f);
  for (int i = 0; i < n; i++) {
    float cx = x + (ax ? 0.0f : U * (float)i), cz = z + (ax ? U * (float)i : 0.0f);
    float x0 = cx - U * 0.5f, x1 = cx + U * 0.5f, z0 = cz - U * 0.5f, z1 = cz + U * 0.5f;
    KMAT(SMAT_STUCCO, KB(x0, 0.0f, z0, x1, h, z1, kt_(fill, cx, cz, 5, 0.90f, 1.06f)));
    KMAT(SMAT_STEEL, {
      // posts on the two long faces at each bay end (the shared post of two
      // bays is drawn once: only the leading end of each bay gets one)
      float p = 0.03f;
      if (ax == 0) {
        KB(x0, 0.0f, z0 - 0.015f, x0 + p, h + 0.02f, z0, wire);
        KB(x0, 0.0f, z1, x0 + p, h + 0.02f, z1 + 0.015f, wire);
        if (i == n - 1) {
          KB(x1 - p, 0.0f, z0 - 0.015f, x1, h + 0.02f, z0, wire);
          KB(x1 - p, 0.0f, z1, x1, h + 0.02f, z1 + 0.015f, wire);
        }
        KB(x0, h, z0, x1, h + 0.03f, z0 + 0.03f, wire);
        KB(x0, h, z1 - 0.03f, x1, h + 0.03f, z1, wire);
      } else {
        KB(x0 - 0.015f, 0.0f, z0, x0, h + 0.02f, z0 + p, wire);
        KB(x1, 0.0f, z0, x1 + 0.015f, h + 0.02f, z0 + p, wire);
        if (i == n - 1) {
          KB(x0 - 0.015f, 0.0f, z1 - p, x0, h + 0.02f, z1, wire);
          KB(x1, 0.0f, z1 - p, x1 + 0.015f, h + 0.02f, z1, wire);
        }
        KB(x0, h, z0, x0 + 0.03f, h + 0.03f, z1, wire);
        KB(x1 - 0.03f, h, z0, x1, h + 0.03f, z1, wire);
      }
    });
  }
}
// JERSEY BARRIER: the stepped concrete profile (base, waist, crown), 0.82 m.
static void k_jersey(int ax, float c, float a0, float a1) {
  v3 cc = rgb(0.70f, 0.69f, 0.65f);
  for (float u = a0; u < a1 - 0.05f; u += 2.0f) {
    float v = fminf(u + 1.98f, a1);
    v3 t = kt_(cc, u, c, 7, 0.92f, 1.04f);
    KMAT(SMAT_CONCRETE, {
      if (ax == 0) {
        KB(u, 0.00f, c - 0.30f, v, 0.25f, c + 0.30f, t);
        KB(u, 0.25f, c - 0.20f, v, 0.55f, c + 0.20f, t);
        KB(u, 0.55f, c - 0.11f, v, 0.82f, c + 0.11f, t);
      } else {
        KB(c - 0.30f, 0.00f, u, c + 0.30f, 0.25f, v, t);
        KB(c - 0.20f, 0.25f, u, c + 0.20f, 0.55f, v, t);
        KB(c - 0.11f, 0.55f, u, c + 0.11f, 0.82f, v, t);
      }
    });
  }
}
// T-WALL blast barrier sections: a flared foot and a 0.3 m slab, h tall, each
// 1.5 m section its own pour (tint), a lifting eye on top. ax 0: along x.
static void k_twall(int ax, float c, float a0, float a1, float h) {
  v3 cc = rgb(0.72f, 0.70f, 0.66f), eye = rgb(0.30f, 0.30f, 0.30f);
  for (float u = a0; u < a1 - 0.05f; u += 1.5f) {
    float v = fminf(u + 1.5f, a1), m = 0.5f * (u + v);
    v3 t = kt_(cc, u, c, 9, 0.88f, 1.05f);
    KMAT(SMAT_CONCRETE, {
      if (ax == 0) {
        KB(u, 0.0f, c - 0.45f, v, 0.40f, c + 0.45f, t);
        KB(u, 0.40f, c - 0.15f, v, h, c + 0.15f, t);
      } else {
        KB(c - 0.45f, 0.0f, u, c + 0.45f, 0.40f, v, t);
        KB(c - 0.15f, 0.40f, u, c + 0.15f, h, v, t);
      }
    });
    KMAT(SMAT_STEEL, {
      if (ax == 0) KB(m - 0.08f, h, c - 0.03f, m + 0.08f, h + 0.10f, c + 0.03f, eye);
      else         KB(c - 0.03f, h, m - 0.08f, c + 0.03f, h + 0.10f, m + 0.08f, eye);
    });
  }
}
// A pipe run on a wall: `n` pipes stacked from y0, along ax at plane `c`
// (standing `inw` metres off it toward the room), with a bracket every 3 m.
static void k_pipes(int ax, float c, float inw, float off, float a0, float a1, float y0, int n,
                    v3 col) {
  v3 br = rgb(0.22f, 0.23f, 0.24f);
  float sgn = inw > 0.0f ? 1.0f : -1.0f;
  KMAT(SMAT_STEEL, {
    for (int i = 0; i < n; i++) {
      float y = y0 + 0.22f * (float)i, d0 = c + sgn * off, d1 = c + sgn * (off + 0.14f);
      v3 pc = kt_(col, (float)i, c, 11, 0.85f, 1.05f);
      if (ax == 0) KB(a0, y, fminf(d0, d1), a1, y + 0.14f, fmaxf(d0, d1), pc);
      else         KB(fminf(d0, d1), y, a0, fmaxf(d0, d1), y + 0.14f, a1, pc);
    }
    for (float u = a0 + 0.4f; u < a1 - 0.2f; u += 3.0f) {
      float yb0 = y0 - 0.08f, yb1 = y0 + 0.22f * (float)(n - 1) + 0.22f;
      float d0 = c, d1 = c + sgn * off;
      if (ax == 0) KB(u, yb0, fminf(d0, d1), u + 0.10f, yb1, fmaxf(d0, d1), br);
      else         KB(fminf(d0, d1), yb0, u, fmaxf(d0, d1), yb1, u + 0.10f, br);
    }
  });
}
// The arena shell, dressed: a pier every 6 m on the inside face of each wall
// (full height through the compass band, with a cap), so a 48 m wall reads as
// a built thing with a module instead of one extruded slab. Symmetric by
// construction; built once (not through g_ks).
static void k_shell_piers(v3 col) {
  const float H = ARENA_HALF;
  v3 cap = {{col.x * 0.86f, col.y * 0.86f, col.z * 0.86f}};
  KMAT(SMAT_CONCRETE, for (float u = -H + 3.0f; u <= H - 2.9f; u += 6.0f) {
    bx(u - 0.25f, 0.0f, -H, u + 0.25f, 3.30f, -H + 0.22f, col);          // north wall
    bx(u - 0.25f, 0.0f, H - 0.22f, u + 0.25f, 3.30f, H, col);            // south
    bx(-H, 0.0f, u - 0.25f, -H + 0.22f, 3.30f, u + 0.25f, col);          // west
    bx(H - 0.22f, 0.0f, u - 0.25f, H, 3.30f, u + 0.25f, col);            // east
    bx(u - 0.31f, 3.30f, -H - 0.02f, u + 0.31f, 3.42f, -H + 0.28f, cap);
    bx(u - 0.31f, 3.30f, H - 0.28f, u + 0.31f, 3.42f, H + 0.02f, cap);
    bx(-H - 0.02f, 3.30f, u - 0.31f, -H + 0.28f, 3.42f, u + 0.31f, cap);
    bx(H - 0.28f, 3.30f, u - 0.31f, H + 0.02f, 3.42f, u + 0.31f, cap);
  });
}
// Painted floor line (a 6 mm film; a step, not cover).
static void k_line(float x0, float z0, float x1, float z1, v3 c) {
  KMAT(SMAT_HAZARD, KB(x0, 0.0f, z0, x1, 0.006f, z1, c));
}
// The small maps borrow the town props (car, truck, tank), which read glass
// and trim from the town palette: give them a neutral one.
static void small_map_style(void) {
  memset(&g_town, 0, sizeof g_town);
  g_town.glass = rgb(0.10f, 0.12f, 0.15f);
  g_town.trim = rgb(0.30f, 0.31f, 0.32f);
  g_town.door = rgb(0.30f, 0.19f, 0.11f);
  g_ks = 1.0f;
}

// FOUNDRY — a steelworks yard under a storm sky. The layout is the one the map
// was balanced on (a gantry you stand on or run under, two furnace stacks, a
// shed down each side entered at the ends or the middle door, a conveyor and a
// container stack in the yards); what changed is that every block is now the
// thing it stood for, plus an overhead crane, roofs on the sheds, pipe runs,
// drums, a forklift and the yard's safety markings.
// THE STEELWORKS AROUND THE YARD. The Foundry was a walled box with mountains
// past the wall; a yard like this sits inside the works it serves. Everything
// here stands OUTSIDE the shell (|x| or |z| past H + 0.6), so none of it is
// reachable, none of it changes a sightline inside, and it is all built
// through g_ks with the rest of the half: south and east here, north and west
// as the mirror. What it gives the yard is a skyline to navigate by — the
// rolling-mill hall behind each end wall, the blast furnace and its stoves
// over each shed roof, chimneys, a water tower and silos at the corners.
#define FOUNDRY_YARD 24.0f   // the yard: the 48 m map the Foundry was, now the works' centre
static void k_octa(float x, float z, float r, float y0, float y1, v3 c) {
  octa(g_ks * x, g_ks * z, r, y0, y1, c);
}
// A round tower in courses: `n` sections from y0 to y1 tapering r0 -> r1,
// with a proud steel band at each joint (bands sit between section planes).
static void k_tower(float x, float z, float r0, float r1, float y0, float y1, int n, v3 c,
                    v3 band) {
  for (int i = 0; i < n; i++) {
    float t0 = (float)i / (float)n, t1 = (float)(i + 1) / (float)n;
    float r = r0 + (r1 - r0) * (t0 + t1) * 0.5f;
    float ya = y0 + (y1 - y0) * t0, yb = y0 + (y1 - y0) * t1;
    k_octa(x, z, r, ya, yb, kt_(c, x + (float)i, z, 40 + i, 0.90f, 1.06f));
    if (i > 0) KMAT(SMAT_STEEL, k_octa(x, z, r + 0.09f, ya - 0.13f, ya + 0.17f, band));
  }
}
static void foundry_backdrop_half(void) {
  const float H = ARENA_HALF, O = FOUNDRY_YARD + 0.6f;   // the yard wall's outer face
  v3 brick = rgb(0.47f, 0.25f, 0.19f), brick_d = rgb(0.36f, 0.19f, 0.15f);
  v3 steel = rgb(0.34f, 0.36f, 0.39f), dark = rgb(0.17f, 0.18f, 0.19f);
  v3 rust = rgb(0.44f, 0.28f, 0.20f), conc = rgb(0.62f, 0.61f, 0.58f);
  v3 glass = rgb(0.12f, 0.15f, 0.19f), yel = rgb(0.86f, 0.66f, 0.12f);
  const float HX = H - 0.1f;                 // the outer shell's inner face
  // ---- THE ROLLING-MILL HALL behind the south wall: a brick shed 44 m long,
  // pilastered every 4 m, a band of tall steel-framed windows, a cornice and
  // a glazed roof monitor along its ridge.
  {
    const float z0 = 27.0f, z1 = 41.0f, X = 22.0f, HH = 11.0f;
    // THE HALL IS A BUILDING YOU GO INTO: brick walls 0.5 m thick under a
    // ceiling slab, two doors in the front facing the yard's gate, one in each
    // end, and inside it the mill line, the coils and the hall crane.
    const float T = 0.5f, DH = 3.3f, WT_ = HH - 0.4f;
    KMAT(SMAT_BRICK, {
      KB(-X, 0.0f, z0, -13.6f, WT_, z0 + T, brick);                 // front, in three
      KB(-10.4f, 0.0f, z0, 6.4f, WT_, z0 + T, brick);
      KB(9.6f, 0.0f, z0, X, WT_, z0 + T, brick);
      KB(-13.6f, DH, z0, -10.4f, WT_, z0 + T, brick);               // ...and the lintels
      KB(6.4f, DH, z0, 9.6f, WT_, z0 + T, brick);
      KB(-X, 0.0f, z1 - T, X, WT_, z1, brick);                      // back
      KB(-X, 0.0f, z0 + T, -X + T, WT_, 31.4f, brick);              // west end, a door
      KB(-X, 0.0f, 34.6f, -X + T, WT_, z1 - T, brick);
      KB(-X, DH, 31.4f, -X + T, WT_, 34.6f, brick);
      KB(X - T, 0.0f, z0 + T, X, WT_, 31.4f, brick);                // east end, a door
      KB(X - T, 0.0f, 34.6f, X, WT_, z1 - T, brick);
      KB(X - T, DH, 31.4f, X, WT_, 34.6f, brick);
      KB(-X, WT_, z0, X, HH, z1, brick_d);                          // the ceiling slab
      for (float x = -X; x <= X + 0.01f; x += 4.0f)              // pilasters
        KB(x - 0.30f, 0.0f, z0 - 0.28f, x + 0.30f, HH + 0.25f, z0, brick_d);
      // cornice (flush at the back, where the roof stair climbs the wall)
      KB(-X - 0.45f, HH, z0 - 0.45f, X + 0.45f, HH + 0.55f, z1, brick_d);
      KB(-X + 0.2f, 3.35f, z0 - 0.12f, X - 0.2f, 3.60f, z0 - 0.005f, brick_d);              // string course
    });
    KMAT(SMAT_GLASS, for (float x = -X + 0.30f; x < X - 0.5f; x += 4.0f)
      KB(x + 0.45f, 5.0f, z0 - 0.04f, x + 3.15f, 9.8f, z0 - 0.002f, glass));
    KMAT(SMAT_STEEL, for (float x = -X + 0.30f; x < X - 0.5f; x += 4.0f) {
      KB(x + 1.70f, 4.95f, z0 - 0.08f, x + 1.90f, 9.85f, z0 - 0.010f, dark);             // mullion
      KB(x + 0.40f, 7.30f, z0 - 0.07f, x + 3.20f, 7.42f, z0 - 0.015f, dark);             // transom
      KB(x + 0.35f, 4.80f, z0 - 0.20f, x + 3.25f, 4.95f, z0 - 0.020f, rgb(0.55f, 0.53f, 0.50f));   // sill
    });
    // the roof: a slab, then the monitor with its own glazing and a cap
    KMAT(SMAT_CLAD, {
      KB(-X + 0.6f, HH + 0.55f, z0 + 3.0f, X - 0.6f, HH + 2.6f, z1 - 3.0f, rgb(0.36f, 0.38f, 0.41f));
      KB(-X + 0.3f, HH + 2.6f, z0 + 2.7f, X - 0.3f, HH + 2.85f, z1 - 2.7f, rgb(0.30f, 0.32f, 0.35f));
    });
    KMAT(SMAT_GLASS, KB(-X + 1.0f, HH + 0.75f, z0 + 2.97f, X - 1.0f, HH + 2.35f, z0 + 3.0f, glass));
    // INSIDE: two roll-stand pairs on their roller tables (the mill line, cover
    // down the hall's middle with the centre left open), finished coils along
    // the back wall, the hall crane's girder overhead, door frames in steel.
    KMAT(SMAT_STEEL, {
      const float mz0 = 34.4f, mz1 = 36.4f;
      for (int i = 0; i < 4; i++) {
        float hx = (i == 0) ? -15.0f : (i == 1) ? -11.0f : (i == 2) ? 4.0f : 8.0f;
        KB(hx - 0.55f, 0.0f, mz0, hx + 0.55f, 2.70f, mz1, rgb(0.30f, 0.33f, 0.38f));      // housing
        KB(hx - 0.62f, 2.70f, mz0 - 0.05f, hx + 0.62f, 2.95f, mz1 + 0.05f, yel);          // its cap
      }
      KB(-14.45f, 0.0f, 34.85f, -11.55f, 0.85f, 35.95f, dark);         // roller tables
      KB(4.55f, 0.0f, 34.85f, 7.45f, 0.85f, 35.95f, dark);
      KB(-21.0f, 0.0f, 34.85f, -15.55f, 0.85f, 35.95f, dark);
      KB(8.55f, 0.0f, 34.85f, 13.0f, 0.85f, 35.95f, dark);
      KB(-21.4f, 8.2f, 30.2f, 21.4f, 8.9f, 31.0f, yel);                // hall crane girder
      KB(-6.0f, 8.9f, 30.0f, -4.6f, 9.5f, 31.2f, rgb(0.24f, 0.25f, 0.27f));
      KB(-21.5f, 7.8f, z0 + T, -21.0f, 8.2f, z1 - T, dark);            // its runway beams
      KB(21.0f, 7.8f, z0 + T, 21.5f, 8.2f, z1 - T, dark);
    });
    for (int i = 0; i < 6; i++) {                                     // coils by the back wall
      float cx = (i < 2) ? -19.0f + 1.8f * (float)i : (i < 4) ? -2.0f + 1.8f * (float)(i - 2) : 15.0f + 1.8f * (float)(i - 4);
      KMAT(SMAT_STEEL, k_octa(cx, 39.4f, 0.80f, 0.0f, 1.40f, kt_(rgb(0.52f, 0.54f, 0.57f), cx, 39.4f, 80, 0.88f, 1.06f)));
    }
    // a gantry crane's runway over a stock yard at the hall's east end
    KMAT(SMAT_HAZARD, KB(X + 1.0f, 8.0f, z0 + 1.0f, X + 9.0f, 8.6f, z0 + 1.8f, yel));
    KMAT(SMAT_STEEL, {
      KB(X + 8.4f, 0.0f, z0 + 1.0f, X + 9.0f, 8.0f, z0 + 1.8f, steel);
      KB(X + 0.2f, 0.0f, z0 + 1.0f, X + 0.8f, 8.0f, z0 + 1.8f, steel);
    });
  }
  // ---- TWO BRICK CHIMNEYS behind the hall, 34 m, banded, sooted at the top.
  KMAT(SMAT_BRICK, {
    k_tower(-12.0f, 44.0f, 1.55f, 1.05f, 0.0f, 30.0f, 6, brick, dark);
    k_tower(12.0f, 44.0f, 1.55f, 1.05f, 0.0f, 34.0f, 7, brick, dark);
    k_octa(-12.0f, 44.0f, 1.18f, 30.0f, 31.2f, rgb(0.16f, 0.13f, 0.12f));
    k_octa(12.0f, 44.0f, 1.18f, 34.0f, 35.2f, rgb(0.16f, 0.13f, 0.12f));
  });
  // ---- THE WATER TOWER on the south-east corner: four legs, braces, a tank.
  KMAT(SMAT_STEEL, {
    const float cx = 36.0f, cz = 36.0f;
    for (int i = 0; i < 4; i++) {
      float lx = cx + ((i & 1) ? 2.2f : -2.2f), lz = cz + ((i & 2) ? 2.2f : -2.2f);
      KB(lx - 0.18f, 0.0f, lz - 0.18f, lx + 0.18f, 14.0f, lz + 0.18f, steel);
    }
    for (float y = 4.5f; y < 14.0f; y += 4.5f) {
      KB(cx - 2.15f, y, cz - 2.11f, cx + 2.15f, y + 0.14f, cz - 2.05f, dark);
      KB(cx - 2.15f, y, cz + 2.05f, cx + 2.15f, y + 0.14f, cz + 2.11f, dark);
      KB(cx - 2.11f, y + 0.20f, cz - 2.15f, cx - 2.05f, y + 0.34f, cz + 2.15f, dark);
      KB(cx + 2.05f, y + 0.20f, cz - 2.15f, cx + 2.11f, y + 0.34f, cz + 2.15f, dark);
    }
    k_octa(cx, cz, 3.1f, 14.0f, 19.0f, rgb(0.52f, 0.56f, 0.58f));
    k_octa(cx, cz, 3.22f, 16.4f, 16.6f, dark);
    k_octa(cx, cz, 2.3f, 19.0f, 19.7f, rgb(0.44f, 0.47f, 0.49f));
    k_octa(cx, cz, 1.2f, 19.7f, 20.3f, rgb(0.44f, 0.47f, 0.49f));
  });
  // ---- THE ORE SILOS on the south-west corner: three concrete bins on a
  // conveyor gallery.
  KMAT(SMAT_CONCRETE, for (int i = 0; i < 3; i++) {
    float cx = -42.0f + 5.4f * (float)i, cz = 34.0f;
    k_octa(cx, cz, 2.5f, 0.0f, 17.0f, kt_(conc, cx, cz, 60, 0.92f, 1.04f));
    k_octa(cx, cz, 1.7f, 17.0f, 18.0f, kt_(conc, cx, cz, 61, 0.86f, 0.94f));
  });
  KMAT(SMAT_CLAD, KB(-44.0f, 18.0f, 33.0f, -31.0f, 20.2f, 35.0f, rgb(0.58f, 0.50f, 0.38f)));
  // ---- THE BLAST FURNACE east of the east wall: hearth, bosh, a stack in
  // banded courses, the throat and four uptakes into the downcomer.
  {
    const float fx = 37.0f, fz = -3.0f;
    v3 shell = rgb(0.30f, 0.31f, 0.33f);
    KMAT(SMAT_STEEL, {
      k_octa(fx, fz, 4.6f, 0.0f, 7.0f, rust);                       // hearth
      k_octa(fx, fz, 5.0f, 7.0f, 10.5f, shell);                     // bosh
      k_octa(fx, fz, 6.4f, 9.2f, 10.0f, dark);                      // bustle main
      k_octa(fx, fz, 6.6f, 10.5f, 11.0f, steel);                    // working deck
    });
    KMAT(SMAT_STEEL, k_tower(fx, fz, 4.6f, 3.0f, 11.0f, 25.0f, 7, shell, dark));
    KMAT(SMAT_STEEL, {
      k_octa(fx, fz, 3.3f, 25.0f, 25.6f, steel);                    // top platform
      k_octa(fx, fz, 2.2f, 25.6f, 28.5f, rust);                     // throat, hopper
      for (int i = 0; i < 4; i++) {                                 // uptakes
        float ux = fx + ((i & 1) ? 1.4f : -1.4f), uz = fz + ((i & 2) ? 1.4f : -1.4f);
        KB(ux - 0.45f, 25.8f, uz - 0.45f, ux + 0.45f, 33.0f, uz + 0.45f, shell);
      }
      KB(fx - 2.0f, 33.0f, fz - 2.0f, fx + 2.0f, 34.2f, fz + 2.0f, shell);
      // the downcomer: over and down to the dust catcher on the north side
      KB(fx - 0.7f, 30.6f, fz - 11.0f, fx + 0.7f, 32.0f, fz - 1.9f, shell);
      KB(fx - 0.7f, 18.0f, fz - 11.0f, fx + 0.7f, 30.6f, fz - 9.6f, shell);
      k_octa(fx, fz - 10.3f, 2.4f, 6.0f, 18.0f, rgb(0.40f, 0.40f, 0.42f));   // dust catcher
      k_octa(fx, fz - 10.3f, 1.2f, 3.0f, 6.0f, rgb(0.40f, 0.40f, 0.42f));
      for (int i = 0; i < 4; i++) {
        float lx = fx + ((i & 1) ? 1.9f : -1.9f), lz = fz - 10.3f + ((i & 2) ? 1.9f : -1.9f);
        KB(lx - 0.16f, 0.0f, lz - 0.16f, lx + 0.16f, 6.4f, lz + 0.16f, dark);
      }
      // the cast house roof over the tap floor, toward the yard
      // (clear of the shed's back door at z -1.5..1.5)
      KB(fx - 11.0f, 0.0f, fz - 4.6f, fx - 10.6f, 6.5f, fz - 4.2f, steel);
      KB(fx - 11.0f, 0.0f, fz + 4.6f, fx - 10.6f, 6.5f, fz + 5.0f, steel);
    });
    KMAT(SMAT_CLAD, KB(fx - 11.4f, 6.5f, fz - 4.8f, fx - 4.0f, 6.85f, fz + 5.2f, rgb(0.42f, 0.44f, 0.47f)));
    // THE SKIP INCLINE: twin girders climbing from the stock house to the top
    // platform, built in steps (axis boxes), a box every 0.3 m of run.
    KMAT(SMAT_HAZARD, for (int i = 0; i < 26; i++) {
      const float st = 9.6f / 26.0f;
      float xa = fx + 13.0f - st * (float)(i + 1), xb = fx + 13.0f - st * (float)i;
      float y = 25.0f * (float)i / 26.0f;
      KB(xa, y, fz - 1.45f, xb, y + 1.25f, fz - 1.05f, yel);
      KB(xa, y, fz + 1.05f, xb, y + 1.25f, fz + 1.45f, yel);
    });
    KMAT(SMAT_CLAD, KB(fx + 12.6f, 0.0f, fz - 3.5f, HX, 4.5f, fz + 3.5f, rgb(0.46f, 0.36f, 0.28f)));
  }
  // ---- THREE HOT-BLAST STOVES in a row south of the furnace: tall domed
  // cylinders, and the hot-blast main across their shoulders to the bustle.
  KMAT(SMAT_STEEL, for (int i = 0; i < 3; i++) {
    float cx = 31.5f + 6.0f * (float)i, cz = 13.0f;
    v3 sc = kt_(rgb(0.56f, 0.58f, 0.60f), cx, cz, 70, 0.92f, 1.04f);
    k_octa(cx, cz, 2.5f, 0.0f, 22.0f, sc);
    k_octa(cx, cz, 2.62f, 7.0f, 7.25f, dark);
    k_octa(cx, cz, 2.62f, 14.5f, 14.75f, dark);
    k_octa(cx, cz, 2.0f, 22.0f, 23.4f, sc);
    k_octa(cx, cz, 1.1f, 23.4f, 24.3f, sc);
  });
  KMAT(SMAT_STEEL, {
    KB(29.0f, 11.6f, 9.9f, 46.0f, 12.8f, 11.1f, rust);              // hot-blast main
    KB(36.4f, 9.7f, 2.8f, 37.6f, 12.7f, 9.9f, rust);                 // into the bustle
  });
  // ---- A STACK OF PIPE BENT INTO THE SKY: the waste-gas chimney by the stoves.
  KMAT(SMAT_STEEL, {
    k_tower(46.0f, 18.0f, 1.0f, 0.8f, 0.0f, 40.0f, 8, rgb(0.62f, 0.62f, 0.60f), rgb(0.70f, 0.20f, 0.14f));
  });
  // ---- THE WAYS UP. Steel stairs (0.40 m risers: STEP_H walks them) to the
  // rolling-mill hall's roof, up its back wall (the cornice stops flush there
  // so nothing overhangs the climb), landing level with the roof; and to
  // the cast house roof at 6.85 m, from beside the shed's back door, the perch
  // that looks back over the yard wall. Built through g_ks like the rest
  // (stair_up takes world coordinates, so the half's sign goes in by hand).
  {
    v3 tread = rgb(0.32f, 0.34f, 0.37f), rl = rgb(0.86f, 0.66f, 0.12f);
    KMAT(SMAT_STEEL, {
      stair_up(0, g_ks * 41.0f, g_ks > 0.0f ? -1 : 1, g_ks * -7.0f, g_ks > 0.0f ? 1 : -1,
               1.60f, 11.60f, 0.50f, 1.45f, tread, rl);
      stair_up(0, g_ks * 2.2f, g_ks > 0.0f ? -1 : 1, g_ks * 34.8f, g_ks > 0.0f ? -1 : 1,
               1.20f, 6.80f, 0.50f, 1.45f, tread, rl);
    });
  }
  // ---- THE STOCK YARDS of the outer ring: cover between the buildings.
  KMAT(SMAT_STEEL, {
    static const float COILS[][2] = {{28.0f, 46.0f}, {29.8f, 46.0f}, {44.0f, 29.6f}, {44.0f, 31.4f}, {33.0f, 27.0f}};
    for (int i = 0; i < 5; i++)
      k_octa(COILS[i][0], COILS[i][1], 0.80f, 0.0f, 1.40f, kt_(rgb(0.52f, 0.54f, 0.57f), COILS[i][0], COILS[i][1], 81, 0.88f, 1.06f));
    // ingot stacks: three courses of cast pigs, each course turned
    static const float INGOTS[][2] = {{30.0f, -20.0f}, {45.5f, -20.5f}, {47.0f, 5.0f}};
    for (int i = 0; i < 3; i++) {
      float x = INGOTS[i][0], z = INGOTS[i][1];
      v3 ic = kt_(rgb(0.36f, 0.30f, 0.27f), x, z, 82, 0.9f, 1.05f);
      KB(x - 1.0f, 0.0f, z - 0.6f, x + 1.0f, 0.40f, z + 0.6f, ic);
      KB(x - 0.6f, 0.40f, z - 1.0f, x + 0.6f, 0.80f, z + 1.0f, ic);
      KB(x - 0.9f, 0.80f, z - 0.5f, x + 0.9f, 1.18f, z + 0.5f, ic);
    }
  });
  container(g_ks * -38.0f, g_ks * 46.5f, 0, 0.0f, rgb(0.24f, 0.42f, 0.30f));
  container(g_ks * -30.5f, g_ks * 44.0f, 1, 0.0f, rgb(0.58f, 0.30f, 0.18f));
  lamp(g_ks * 23.0f, g_ks * 26.2f, 1, g_ks > 0.0f ? 1 : -1);
  lamp(g_ks * 34.0f, g_ks * 20.0f, 0, g_ks > 0.0f ? -1 : 1);
  // ---- THE RAILS out of the yard continue past the east wall to the works.
  KMAT(SMAT_STEEL, {
    KB(O, 0.0f, 21.90f, HX, 0.016f, 21.98f, rgb(0.46f, 0.44f, 0.42f));
    KB(O, 0.0f, 23.33f, HX, 0.016f, 23.41f, rgb(0.46f, 0.44f, 0.42f));
  });
}

// THE YARD WALL. The Foundry's yard was the whole map, walled by the arena
// shell; it is now the centre of the works, and its wall has ways out: a gate
// in each end wall toward the rolling-mill hall (x 6..11 here, -11..-6 in the
// mirror), a door through each shed's back wall into the blast furnace's cast
// house (z -1.5..1.5), and the rail gate where the spur leaves (z 20.6..24).
static void foundry_yard_wall_half(void) {
  const float Y = FOUNDRY_YARD, WT = 0.6f, BH = 3.2f;
  const theme_t *t = &g_theme;
  v3 wc = t->wall, cap = {{t->wall.x * 0.86f, t->wall.y * 0.86f, t->wall.z * 0.86f}};
  v3 yel = rgb(0.86f, 0.66f, 0.12f), steel = rgb(0.30f, 0.32f, 0.35f);
  KMAT(SMAT_CONCRETE, {
    KB(-Y - WT, 0.0f, Y, 6.0f, BH, Y + WT, wc);                     // south wall, the gate gap
    KB(11.0f, 0.0f, Y, Y + WT, BH, Y + WT, wc);
    KB(Y, 0.0f, -Y, Y + WT, BH, -1.5f, wc);                         // east wall: door, rail gate
    KB(Y, 0.0f, 1.5f, Y + WT, BH, 20.6f, wc);
    KB(Y, 2.6f, -1.5f, Y + WT, BH, 1.5f, wc);                       // the door's lintel
    // piers on the yard face, clear of the openings
    for (float u = -21.0f; u <= 21.01f; u += 6.0f) {
      if (u > 5.0f && u < 12.0f) continue;
      KB(u - 0.25f, 0.0f, Y - 0.22f, u + 0.25f, 3.30f, Y, wc);
      KB(u - 0.31f, 3.30f, Y - 0.28f, u + 0.31f, 3.42f, Y + 0.02f, cap);
    }
  });
  // the gate's steel portal and its hazard-striped jambs; the rail gate's posts
  KMAT(SMAT_HAZARD, {
    KB(5.60f, 0.0f, Y - 0.12f, 6.05f, BH + 0.9f, Y + WT + 0.12f, yel);
    KB(10.95f, 0.0f, Y - 0.12f, 11.4f, BH + 0.9f, Y + WT + 0.12f, yel);
    KB(Y - 0.12f, 0.0f, 20.20f, Y + WT + 0.12f, BH + 0.6f, 20.65f, yel);
  });
  KMAT(SMAT_STEEL, {
    KB(5.6f, BH + 0.9f, Y - 0.16f, 11.4f, BH + 1.3f, Y + WT + 0.16f, steel);   // portal beam
    KB(Y - 0.08f, 0.0f, -1.62f, Y + WT + 0.08f, 2.62f, -1.46f, steel);         // door frame
    KB(Y - 0.08f, 0.0f, 1.46f, Y + WT + 0.08f, 2.62f, 1.62f, steel);
  });
}

static void foundry_half(void) {
  const theme_t *t = &g_theme;
  v3 steel = rgb(0.30f, 0.32f, 0.35f), dark = rgb(0.17f, 0.18f, 0.19f);
  v3 yel = rgb(0.86f, 0.66f, 0.12f), rust = rgb(0.46f, 0.31f, 0.22f);
  // GANTRY: legs, the steel stair (two treads with yellow nosings), a handrail
  // along this half's long edge of the deck (the stair ends stay open).
  KMAT(SMAT_STEEL, {
    KB(4.2f, 0.0f, 1.2f, 4.8f, 1.90f, 1.8f, steel);
    KB(4.2f, 0.0f, -1.8f, 4.8f, 1.90f, -1.2f, steel);
    KB(5.2f, 0.0f, -1.4f, 6.2f, 1.45f, 1.4f, rgb(0.36f, 0.38f, 0.40f));
    KB(6.2f, 0.0f, -1.4f, 7.2f, 0.72f, 1.4f, rgb(0.38f, 0.40f, 0.42f));
    KB(5.22f, 0.0f, -1.46f, 6.18f, 2.25f, -1.40f, dark);
    KB(5.22f, 0.0f, 1.40f, 6.18f, 2.25f, 1.46f, dark);
    KB(6.22f, 0.0f, -1.46f, 7.18f, 1.52f, -1.40f, dark);
    KB(6.22f, 0.0f, 1.40f, 7.18f, 1.52f, 1.46f, dark);
    for (float x = -4.6f; x <= 4.01f; x += 1.15f)
      KB(x, 2.20f, 1.90f, x + 0.06f, 3.15f, 1.96f, dark);
    KB(-4.63f, 3.15f, 1.89f, 4.08f, 3.21f, 1.97f, dark);
    KB(-4.63f, 2.68f, 1.91f, 4.08f, 2.72f, 1.95f, dark);
  });
  KMAT(SMAT_HAZARD, {
    KB(6.12f, 1.45f, -1.4f, 6.20f, 1.465f, 1.4f, yel);
    KB(7.12f, 0.72f, -1.4f, 7.20f, 0.735f, 1.4f, yel);
    KB(-5.0f, 2.20f, 1.80f, 5.0f, 2.215f, 1.88f, yel);   // deck edge stripe
  });
  // FURNACE: a round body in the map's signal colour, a steel collar, and a
  // chimney to 11 m banded twice — the landmark you navigate by.
  {
    float cx = g_ks * 10.0f, cz = g_ks * -10.0f;
    octa(cx, cz, 1.15f, 0.0f, 4.6f, t->mid);
    KMAT(SMAT_STEEL, {
      octa(cx, cz, 1.24f, 1.30f, 1.62f, steel);
      octa(cx, cz, 1.22f, 4.6f, 4.9f, steel);
      octa(cx, cz, 0.55f, 4.9f, 11.0f, dark);
      octa(cx, cz, 0.62f, 7.0f, 7.25f, steel);
      octa(cx, cz, 0.62f, 9.6f, 9.85f, steel);
      octa(cx, cz, 0.66f, 11.0f, 11.3f, steel);
    });
    // the tap hole and its glow, on the face toward the gantry
    KB(9.55f, 0.4f, -8.88f, 10.45f, 1.1f, -8.84f, rgb(1.0f, 0.46f, 0.10f));
  }
  // THE SHEDS: corrugated walls, the roof on four columns over the whole shed
  // to the shell wall, a gutter, pipe runs inside along the shell wall.
  KMAT(SMAT_CLAD, {
    KB(13.0f, 0.0f, -14.0f, 13.6f, 2.6f, -4.0f, t->wall);
    KB(13.0f, 0.0f, 4.0f, 13.6f, 2.6f, 14.0f, t->wall);
    KB(12.6f, 3.60f, -14.3f, 24.0f, 3.72f, 14.3f, rgb(0.40f, 0.43f, 0.46f));
  });
  KMAT(SMAT_STEEL, {
    KB(13.0f, 2.6f, -14.0f, 13.6f, 3.60f, -13.6f, steel);
    KB(13.0f, 2.6f, -4.4f, 13.6f, 3.60f, -4.0f, steel);
    KB(13.0f, 2.6f, 4.0f, 13.6f, 3.60f, 4.4f, steel);
    KB(13.0f, 2.6f, 13.6f, 13.6f, 3.60f, 14.0f, steel);
    KB(12.48f, 3.50f, -14.3f, 12.6f, 3.66f, 14.3f, dark);         // gutter
  });
  k_pipes(1, 24.0f, -1.0f, 0.06f, -13.8f, -1.8f, 1.75f, 3, rust);   // (the back door between)
  k_pipes(1, 24.0f, -1.0f, 0.06f, 1.8f, 13.8f, 1.75f, 3, rust);
  // ...and what holds the roof up, seen from inside (render-only: PROP_ROOF).
  prop_add(PROP_ROOF, g_ks * 18.3f, 0.0f, 0, g_ks < 0.0f ? -1 : 1,
           rgb(0.33f, 0.35f, 0.38f), rgb(0.20f, 0.21f, 0.22f));
  // the sheds' contents: crates, a pallet stack, drums
  k_crate(17.7f, -7.3f, 0.70f, 0.70f, 1.10f, t->crate_a);
  k_crate(18.7f, -7.3f, 0.30f, 0.40f, 0.55f, t->crate_b);
  k_crate(20.1f, 2.6f, 0.60f, 0.60f, 1.00f, t->crate_a);
  KMAT(SMAT_WOOD, KB(19.8f, 0.98f, 2.3f, 20.4f, 1.45f, 2.9f, kt_(t->crate_b, 20.1f, 2.6f, 4, 0.9f, 1.0f)));
  k_pallets(21.6f, -11.5f, 1, 5);
  k_jersey(0, 10.25f, 16.0f, 21.0f);
  // (clear of the other half's container stack, which ends at z -16.28)
  barrel(g_ks * 16.0f, g_ks * -15.45f, rgb(0.62f, 0.18f, 0.12f));
  barrel(g_ks * 16.75f, g_ks * -15.55f, rgb(0.20f, 0.30f, 0.50f));
  barrel(g_ks * 16.35f, g_ks * -14.85f, rgb(0.74f, 0.56f, 0.16f));
  barrel(g_ks * 22.8f, g_ks * 12.6f, rgb(0.62f, 0.18f, 0.12f));
  barrel(g_ks * 22.8f, g_ks * 11.9f, rgb(0.62f, 0.18f, 0.12f));
  // THE CONVEYOR: legs, a frame, the belt and its end drums. You can shoot
  // under it now; you cannot walk under it.
  KMAT(SMAT_STEEL, {
    KB(-8.0f, 0.78f, 15.0f, 2.0f, 0.96f, 15.6f, steel);
    for (float x = -7.8f; x < 2.0f; x += 2.4f) {
      KB(x, 0.0f, 15.05f, x + 0.1f, 0.78f, 15.15f, dark);
      KB(x, 0.0f, 15.45f, x + 0.1f, 0.78f, 15.55f, dark);
    }
    KB(-8.2f, 0.80f, 15.02f, -8.0f, 1.0f, 15.58f, dark);
    KB(2.0f, 0.80f, 15.02f, 2.2f, 1.0f, 15.58f, dark);
  });
  KB(-7.95f, 0.96f, 15.05f, 1.95f, 1.05f, 15.55f, rgb(0.10f, 0.10f, 0.11f));   // belt
  // THE CONTAINER STACK and the forklift working it.
  container(g_ks * -14.0f, g_ks * 17.5f, 0, 0.0f, rgb(0.20f, 0.36f, 0.52f));
  container(g_ks * -14.3f, g_ks * 17.5f, 0, 2.6f, rgb(0.62f, 0.24f, 0.16f));
  // ...the forklift working it (the Blender model, forks toward the stack)...
  forklift_prop(g_ks * -8.5f, g_ks * 20.3f, g_ks > 0.0f ? 3 : 1, rgb(0.86f, 0.64f, 0.12f),
                rgb(0.20f, 0.21f, 0.22f));
  // ...and IBC totes of quench oil against the shed's back wall.
  ibc_tote(g_ks * 23.1f, g_ks * -4.1f, g_ks > 0.0f ? 2 : 0, rgb(0.86f, 0.86f, 0.82f), rgb(0.15f, 0.30f, 0.55f));
  ibc_tote(g_ks * 23.1f, g_ks * -2.95f, g_ks > 0.0f ? 2 : 0, rgb(0.80f, 0.80f, 0.74f), rgb(0.30f, 0.30f, 0.32f));
  elec_cabinet(g_ks * 23.45f, g_ks * 7.5f, g_ks > 0.0f ? 2 : 0, rgb(0.55f, 0.58f, 0.55f));
  k_crate(2.6f, 5.6f, 0.60f, 0.60f, 1.10f, t->crate_a);
  k_crate(-6.5f, 7.0f, 0.50f, 1.00f, 1.00f, t->crate_b);
  // THE OVERHEAD CRANE: a column at each end of this half's runway, the
  // runway beam along the east side, and this half's bridge girder across the
  // whole yard with its trolley, hoist rope and hook block.
  KMAT(SMAT_STEEL, {
    KB(20.55f, 0.0f, -19.75f, 21.05f, 7.0f, -19.25f, steel);
    KB(20.55f, 0.0f, 19.25f, 21.05f, 7.0f, 19.75f, steel);
    KB(20.45f, 0.0f, -19.85f, 21.15f, 0.12f, -19.15f, dark);
    KB(20.45f, 0.0f, 19.15f, 21.15f, 0.12f, 19.85f, dark);
  });
  KMAT(SMAT_HAZARD, {
    KB(20.50f, 7.0f, -20.3f, 21.10f, 7.45f, 20.3f, yel);              // runway
    KB(-21.10f, 7.45f, -7.45f, 21.10f, 8.05f, -6.55f, yel);           // bridge
    KB(-3.6f, 8.05f, -7.5f, -2.4f, 8.55f, -6.5f, rgb(0.24f, 0.25f, 0.27f));   // trolley
  });
  KMAT(SMAT_STEEL, {
    KB(-3.02f, 4.1f, -7.02f, -2.98f, 7.45f, -6.98f, dark);            // rope
    KB(-3.18f, 3.75f, -7.15f, -2.82f, 4.1f, -6.85f, yel);             // hook block
  });
  // Safety lines: the walkway along each shed front and the gantry's apron.
  k_line(12.20f, -14.0f, 12.32f, 14.0f, yel);
  k_line(-5.6f, 2.55f, 4.1f, 2.67f, yel);
  lamp(g_ks * 12.3f, g_ks * -16.8f, 1, g_ks > 0.0f ? 1 : -1);
  // THE LADLE on the crane's hook: a refractory-lined steel ladle on its bail,
  // the melt glowing in it — hung high enough (2.5 m) to walk and jump under.
  KMAT(SMAT_STEEL, {
    k_octa(-3.0f, -7.0f, 0.78f, 2.50f, 2.70f, dark);
    k_octa(-3.0f, -7.0f, 0.92f, 2.70f, 3.42f, rgb(0.28f, 0.24f, 0.22f));
    k_octa(-3.0f, -7.0f, 1.00f, 3.20f, 3.38f, steel);                // lip band
    KB(-4.08f, 3.0f, -7.06f, -4.00f, 3.80f, -6.94f, dark);         // the bail
    KB(-2.00f, 3.0f, -7.06f, -1.92f, 3.80f, -6.94f, dark);
    KB(-4.08f, 3.80f, -7.06f, -1.92f, 3.88f, -6.94f, dark);
  });
  k_octa(-3.0f, -7.0f, 0.80f, 3.42f, 3.46f, rgb(1.0f, 0.52f, 0.12f));   // the melt
  // THE CRANE CAB: the operator's glazed pulpit under the east end of the
  // bridge girder, over the shed roof line.
  KMAT(SMAT_STEEL, KB(17.2f, 5.9f, -7.65f, 19.0f, 7.45f, -6.35f, rgb(0.24f, 0.25f, 0.27f)));
  KMAT(SMAT_GLASS, KB(16.0f, 6.35f, -7.45f, 17.2f, 7.45f, -6.55f, rgb(0.12f, 0.15f, 0.19f)));
  KMAT(SMAT_HAZARD, KB(15.96f, 6.20f, -7.50f, 17.2f, 6.35f, -6.50f, yel));
  // THE PIPE BRIDGE: a trussed bridge from shed roof to shed roof across the
  // yard at 5.6 m carrying the gas, steam and water mains; a post each end on
  // the shed's front wall line. Well above anything you can stand on.
  KMAT(SMAT_STEEL, {
    KB(-13.25f, 3.72f, 11.75f, -12.95f, 5.60f, 12.05f, steel);
    KB(12.95f, 3.72f, 11.75f, 13.25f, 5.60f, 12.05f, steel);
    KB(-13.4f, 5.60f, 11.45f, 13.4f, 5.78f, 12.55f, dark);           // bottom chord / deck
    KB(-13.4f, 6.62f, 11.45f, 13.4f, 6.74f, 11.55f, dark);           // top chords
    KB(-13.4f, 6.62f, 12.45f, 13.4f, 6.74f, 12.55f, dark);
    for (float x = -12.6f; x < 12.8f; x += 1.8f) {                   // verticals
      KB(x, 5.78f, 11.45f, x + 0.08f, 6.62f, 11.53f, dark);
      KB(x, 5.78f, 12.47f, x + 0.08f, 6.62f, 12.55f, dark);
    }
    KB(-13.3f, 5.80f, 11.60f, 13.3f, 6.14f, 11.94f, rgb(0.20f, 0.42f, 0.30f));   // water
    KB(-13.3f, 5.80f, 12.02f, 13.3f, 6.06f, 12.28f, rgb(0.62f, 0.62f, 0.60f));   // steam
    KB(-13.3f, 6.18f, 11.80f, 13.3f, 6.44f, 12.06f, yel);                       // gas
  });
  // THE TAP RUNNER: the furnace's tap hole drains into a glowing trough
  // let into the floor (paint-thin: it is a picture, not a step).
  KB(9.82f, 0.0f, -8.84f, 10.18f, 0.008f, -6.9f, rgb(1.0f, 0.46f, 0.10f));
  // THE RAIL SPUR along the south wall: two rails and their sleepers, flush
  // with the floor, a buffer stop at the yard end; they run on under the east
  // wall to the works outside.
  KMAT(SMAT_WOOD, for (float x = -5.4f; x < 23.6f; x += 0.70f)
    KB(x, 0.0f, 21.62f, x + 0.24f, 0.006f, 23.67f, rgb(0.30f, 0.25f, 0.20f)));
  KMAT(SMAT_STEEL, {
    KB(-5.6f, 0.0f, 21.90f, FOUNDRY_YARD + 0.6f, 0.016f, 21.98f, rgb(0.46f, 0.44f, 0.42f));
    KB(-5.6f, 0.0f, 23.33f, FOUNDRY_YARD + 0.6f, 0.016f, 23.41f, rgb(0.46f, 0.44f, 0.42f));
  });
  KMAT(SMAT_HAZARD, KB(-6.05f, 0.0f, 21.70f, -5.6f, 0.62f, 23.59f, yel));   // buffer stop
  foundry_yard_wall_half();
  foundry_backdrop_half();
}
static void map_foundry(void) {
  small_map_style();
  const theme_t *t = &g_theme;
  mk1(-5.0f, -2.0f, 5.0f, 2.0f, 1.90f, 2.20f, t->crate_b);   // the gantry deck
  k_shell_piers(t->wall);
  g_ks = 1.0f;  foundry_half();
  g_ks = -1.0f; foundry_half();
  g_ks = 1.0f;
}

// BAZAAR — a market quarter at golden hour. Four houses make a plus of streets
// through the fountain square, stalls crowd the streets, two roofs are reached
// by a stair. Now the houses are houses (render, doors, shuttered windows, a
// cornice, parapets, water tanks, rugs hung out), the streets are shaded by
// striped canopies, the square has palms and a real fountain, and the stalls
// carry their goods.
static void bazaar_half(void) {
  const theme_t *t = &g_theme;
  v3 rA = kt_(t->wall, 5.0f, 5.0f, 1, 0.92f, 0.98f);
  v3 rB = kt_(t->cover, -5.0f, 5.0f, 2, 0.95f, 1.05f);
  v3 door = rgb(0.32f, 0.20f, 0.12f), glass = rgb(0.10f, 0.12f, 0.15f);
  v3 shut = rgb(0.18f, 0.40f, 0.52f), shut2 = rgb(0.56f, 0.30f, 0.18f);
  v3 stone = rgb(0.62f, 0.56f, 0.46f);
  // HOUSE A (roof reachable) and HOUSE B.
  KMAT(SMAT_STUCCO, {
    KB(5.0f, 0.0f, 5.0f, 11.0f, 3.2f, 11.0f, rA);
    KB(-11.0f, 0.0f, 5.0f, -5.0f, 3.2f, 11.0f, rB);
    // cornices, abutting at the corners rather than overlapping
    v3 cA = kt_(rA, 1, 1, 3, 0.86f, 0.88f), cB = kt_(rB, 1, 1, 4, 0.86f, 0.88f);
    KB(4.9f, 2.92f, 4.9f, 11.1f, 3.06f, 5.0f, cA);
    KB(4.9f, 2.92f, 5.0f, 5.0f, 3.06f, 11.1f, cA);
    KB(11.0f, 2.92f, 5.0f, 11.1f, 3.06f, 11.1f, cA);
    KB(5.0f, 2.92f, 11.0f, 11.0f, 3.06f, 11.1f, cA);
    KB(-11.1f, 2.92f, 4.9f, -4.9f, 3.06f, 5.0f, cB);
    KB(-5.0f, 2.92f, 5.0f, -4.9f, 3.06f, 11.1f, cB);
    KB(-11.1f, 2.92f, 5.0f, -11.0f, 3.06f, 11.1f, cB);
    KB(-11.0f, 2.92f, 11.0f, -5.0f, 3.06f, 11.1f, cB);
    // parapets: A's north edge (the original), east and south; B all round
    KB(5.3f, 3.2f, 5.05f, 10.7f, 3.9f, 5.45f, t->crate_b);
    KB(10.6f, 3.2f, 5.45f, 11.0f, 3.7f, 11.0f, cA);
    KB(5.0f, 3.2f, 10.6f, 10.6f, 3.7f, 11.0f, cA);
    KB(-11.0f, 3.2f, 5.0f, -5.0f, 3.6f, 5.35f, cB);
    KB(-11.0f, 3.2f, 10.65f, -5.0f, 3.6f, 11.0f, cB);
    KB(-11.0f, 3.2f, 5.35f, -10.65f, 3.6f, 10.65f, cB);
    KB(-5.35f, 3.2f, 5.35f, -5.0f, 3.6f, 10.65f, cB);
  });
  // the roof stair, in stone: walkable 0.40 m steps + flat landing + sidebar
  v3 sb = v3_scale(stone, 0.86f);
  KMAT(SMAT_STONE, {
    for (int k = 0; k < 6; k++) {
      float z0 = 6.60f + 0.46f * (float)k, z1 = z0 + 0.46f;
      float top = 0.40f * (float)(k + 1);
      KB(3.70f, 0.0f, z0, 5.0f, top, z1, kt_(stone, 4, (float)k, 5, 0.94f, 1.00f));
      KB(3.52f, 0.0f, z0 + 0.01f, 3.70f, top + 0.76f, z1 - 0.01f, sb);
      // ...and an iron handrail on posts along its crown
      g_mat_force = SMAT_STEEL;
      KB(3.58f, top + 0.76f, z0 + 0.20f, 3.64f, top + 1.00f, z0 + 0.26f, rgb(0.16f, 0.16f, 0.18f));
      KB(3.56f, top + 1.00f, z0 + 0.02f, 3.66f, top + 1.05f, z1 - 0.02f, rgb(0.16f, 0.16f, 0.18f));
      g_mat_force = SMAT_STONE;
    }
    KB(3.70f, 0.0f, 9.36f, 5.0f, 2.80f, 10.55f, kt_(stone, 4, 9, 5, 0.92f, 0.96f));
    KB(3.52f, 0.0f, 9.37f, 3.70f, 3.56f, 10.55f, sb);
    KB(3.70f, 2.80f, 10.37f, 4.88f, 3.54f, 10.55f, sb);
  });
  // doors with lintels, windows with sills and shutters (flush colour below
  // head height, the way the town facades are), an AC box and a hung rug
  KMAT(SMAT_WOOD, {
    KB(4.97f, 0.0f, 5.45f, 5.0f, 2.2f, 6.40f, door);
    KB(-5.0f, 0.0f, 6.6f, -4.97f, 2.2f, 7.7f, door);
    KB(7.0f, 0.0f, 11.0f, 8.1f, 2.2f, 11.03f, door);
    KB(7.4f, 1.3f, 4.985f, 8.4f, 2.2f, 5.0f, glass);
    KB(6.92f, 1.3f, 4.975f, 7.4f, 2.2f, 5.0f, shut);
    KB(8.4f, 1.3f, 4.975f, 8.88f, 2.2f, 5.0f, shut);
    KB(-8.6f, 1.3f, 4.985f, -7.6f, 2.2f, 5.0f, glass);
    KB(-9.08f, 1.3f, 4.975f, -8.6f, 2.2f, 5.0f, shut2);
    KB(-7.6f, 1.3f, 4.975f, -7.12f, 2.2f, 5.0f, shut2);
    KB(11.0f, 1.3f, 7.4f, 11.015f, 2.2f, 8.4f, glass);
    KB(-11.015f, 1.3f, 8.0f, -11.0f, 2.2f, 9.0f, glass);
  });
  KMAT(SMAT_STONE, {
    KB(4.88f, 2.2f, 5.38f, 5.0f, 2.34f, 6.48f, stone);
    KB(-5.0f, 2.2f, 6.52f, -4.88f, 2.34f, 7.78f, stone);
    KB(7.3f, 1.20f, 4.92f, 8.5f, 1.30f, 5.0f, stone);
    KB(-8.7f, 1.20f, 4.92f, -7.5f, 1.30f, 5.0f, stone);
  });
  KMAT(SMAT_STEEL, KB(11.0f, 2.40f, 5.8f, 11.45f, 2.85f, 6.6f, rgb(0.74f, 0.74f, 0.72f)));
  KB(-6.6f, 0.9f, 10.99f, -5.4f, 2.6f, 11.02f, t->band_s);             // rug, back wall
  KB(-6.6f, 0.82f, 10.99f, -5.4f, 0.9f, 11.02f, kt_(t->band_s, 1, 1, 6, 0.6f, 0.65f));
  KB(-10.4f, 1.0f, 4.97f, -9.4f, 2.7f, 5.0f, t->band_n);               // rug, street
  // roof kit: a water tank on A, a dish on B
  KMAT(SMAT_STEEL, {
    octa(g_ks * 9.4f, g_ks * 9.4f, 0.55f, 3.2f, 4.3f, rgb(0.22f, 0.24f, 0.28f));
    KB(-8.3f, 3.2f, 8.3f, -8.1f, 3.9f, 8.5f, rgb(0.30f, 0.30f, 0.30f));
    KB(-8.6f, 3.9f, 8.1f, -7.8f, 4.5f, 8.18f, rgb(0.82f, 0.82f, 0.80f));
  });
  // CANOPIES over this half's two streets: striped cloth on poles from the
  // roofs, high enough (4.5 m) that nobody stands on them from a parapet.
  {
    v3 cl[3] = {t->band_s, rgb(0.86f, 0.82f, 0.72f), t->band_n};
    for (int i = 0; i < 3; i++) {
      float z0 = 6.0f + 1.6f * (float)i;
      KB(-5.0f, 4.56f, z0, 5.0f, 4.60f, z0 + 0.95f, cl[i]);
      KB(-5.0f, 3.2f, z0 + 0.43f, -4.9f, 4.56f, z0 + 0.53f, rgb(0.34f, 0.24f, 0.15f));
      KB(4.9f, 3.2f, z0 + 0.43f, 5.0f, 4.56f, z0 + 0.53f, rgb(0.34f, 0.24f, 0.15f));
      float x0 = 6.0f + 1.6f * (float)i;
      KB(x0, 4.62f, -5.0f, x0 + 0.95f, 4.66f, 5.0f, cl[(i + 1) % 3]);
    }
  }
  // the square: palms and pots
  palm(g_ks * 3.7f, g_ks * 3.7f, 5.0f);
  palm(g_ks * -3.7f, g_ks * 3.7f, 5.4f);
  KMAT(SMAT_BRICK, {
    octa(g_ks * 4.4f, g_ks * 5.6f, 0.28f, 0.0f, 0.55f, t->cover);
    octa(g_ks * 4.4f, g_ks * 5.6f, 0.32f, 0.55f, 0.64f, kt_(t->cover, 1, 1, 7, 0.8f, 0.85f));
    octa(g_ks * -4.4f, g_ks * 7.6f, 0.24f, 0.0f, 0.48f, t->cover);
  });
  // a handcart with sacks
  KMAT(SMAT_WOOD, {
    KB(13.6f, 0.45f, 3.6f, 15.2f, 0.70f, 4.5f, rgb(0.50f, 0.36f, 0.22f));
    KB(15.2f, 0.62f, 3.75f, 16.4f, 0.68f, 3.85f, rgb(0.40f, 0.28f, 0.17f));
    KB(15.2f, 0.62f, 4.25f, 16.4f, 0.68f, 4.35f, rgb(0.40f, 0.28f, 0.17f));
    KB(14.1f, 0.0f, 3.48f, 14.8f, 0.70f, 3.56f, rgb(0.24f, 0.17f, 0.11f));
    KB(14.1f, 0.0f, 4.54f, 14.8f, 0.70f, 4.62f, rgb(0.24f, 0.17f, 0.11f));
  });
  KMAT(SMAT_STUCCO, {
    KB(13.75f, 0.70f, 3.7f, 14.45f, 1.02f, 4.1f, rgb(0.78f, 0.72f, 0.58f));
    KB(14.5f, 0.70f, 3.8f, 15.1f, 0.98f, 4.4f, rgb(0.74f, 0.68f, 0.55f));
  });
  // the street barrier is a stone wall with a coping; the crates are crates
  KMAT(SMAT_STONE, {
    KB(12.5f, 0.0f, -3.0f, 13.0f, 0.9f, 3.0f, stone);
    KB(12.45f, 0.9f, -3.05f, 13.05f, 1.0f, 3.05f, kt_(stone, 1, 1, 8, 1.06f, 1.1f));
  });
  // A bench on each side of the square, facing the fountain, and a pallet
  // of grain sacks waiting by the stalls.
  bench_prop(0.0f, g_ks * 3.45f, g_ks > 0.0f ? 3 : 1, rgb(0.46f, 0.30f, 0.17f));
  sack_pallet(g_ks * 20.4f, g_ks * 4.4f, g_ks > 0.0f ? 0 : 2, rgb(0.62f, 0.55f, 0.40f));
  // House B's roof, now reached by a ladder up its west wall.
  ladder_add(g_ks * -11.0f, g_ks * 8.4f, g_ks > 0.0f ? SIDE_W : SIDE_E);
  k_crate(20.6f, -6.4f, 0.6f, 0.6f, 1.1f, t->crate_a);
  k_crate(21.3f, -4.9f, 0.4f, 0.4f, 0.6f, t->crate_b);
  lamp(g_ks * 4.4f, g_ks * -13.0f, 1, g_ks > 0.0f ? 1 : -1);
  lamp(g_ks * 13.0f, g_ks * 4.4f, 0, g_ks > 0.0f ? -1 : 1);
}
// THE BAZAAR'S OUTER STREET. The market filled the middle of the map and left
// a ring of bare sand to the arena wall, so the quarter ended in a void. Now a
// row of shopfront houses stands against the wall all the way round: each its
// own width, height and limewash, a door and a shuttered window at street
// level, a striped awning on every other front, upper windows on the tall
// ones, a cornice and a roof parapet. They are 2 m deep (the street is still
// 9 m wide) and nobody stands on them: no step reaches 3.6 m.
static void k_shopfront(int ax, float a0, float a1, float face, float back, int salt,
                        float hb, float hr) {
  // ax 0: the row runs along x, the front faces -z at z = face (back at z = back)
  static const float PL[6][3] = {{0.86f, 0.78f, 0.62f}, {0.90f, 0.86f, 0.76f}, {0.80f, 0.62f, 0.46f},
                                 {0.76f, 0.70f, 0.58f}, {0.88f, 0.72f, 0.52f}, {0.70f, 0.56f, 0.44f}};
  const theme_t *t = &g_theme;
  v3 door = rgb(0.32f, 0.20f, 0.12f), glass = rgb(0.10f, 0.12f, 0.15f);
  v3 shut[3] = {rgb(0.18f, 0.40f, 0.52f), rgb(0.56f, 0.30f, 0.18f), rgb(0.26f, 0.42f, 0.30f)};
  v3 awn[3] = {t->band_s, rgb(0.86f, 0.82f, 0.72f), t->band_n};
  float sg = face < back ? 1.0f : -1.0f;               // +1: the row lies on the + side of its face
  int k = 0;
  for (float u = a0; u < a1 - 0.5f; k++) {
    float w = 4.0f + 2.0f * khash(u, face, salt + k);
    if (u + w > a1 - 2.5f) w = a1 - u;
    float v = u + w;
    float h = hb + hr * khash(face, u, salt + 7 * k);
    int pi = (int)(6.0f * khash(u, (float)k, salt + 3)) % 6;
    v3 pc = kt_(rgb(PL[pi][0], PL[pi][1], PL[pi][2]), u, face, salt, 0.94f, 1.02f);
    v3 cc = v3_scale(pc, 0.86f);
    float m = 0.5f * (u + v), f0 = face, f1 = back;
    float fz0 = fminf(f0, f1), fz1 = fmaxf(f0, f1);
    float pr = face - sg * 0.02f;                       // the plane things stand proud to
#define SF(a_, y0_, b_, y1_, d0_, d1_, c_)                                           \
    do { float _d0 = face - sg * (d0_), _d1 = face - sg * (d1_);                     \
         if (ax == 0) KB(a_, y0_, fminf(_d0, _d1), b_, y1_, fmaxf(_d0, _d1), c_);     \
         else         KB(fminf(_d0, _d1), y0_, a_, fmaxf(_d0, _d1), y1_, b_, c_); } while (0)
    (void)pr;
    KMAT(SMAT_STUCCO, {
      if (ax == 0) KB(u, 0.0f, fz0, v, h, fz1, pc); else KB(fz0, 0.0f, u, fz1, h, v, pc);
      SF(u + 0.02f, h - 0.30f, v - 0.02f, h - 0.14f, 0.0f, 0.10f, cc);          // cornice
      SF(u + 0.02f, h, v - 0.02f, h + 0.45f, -0.05f, 0.25f, cc);               // parapet
      SF(u + 0.02f, 0.0f, v - 0.02f, 0.28f, 0.0f, 0.06f, v3_scale(pc, 0.78f));  // plinth
    });
    float dx = u + 0.7f + 0.8f * khash(u, h, salt + 11);
    if (dx + 1.25f > v) dx = u + 0.5f * (w - 1.05f);    // a narrow front: the door centred
    int win = dx + 3.12f < v;                          // ...and no window past its end
    KMAT(SMAT_WOOD, {
      SF(dx, 0.28f, dx + 1.05f, 2.25f, 0.06f, 0.075f, door);
      if (win) {
        SF(dx + 1.7f, 1.25f, dx + 2.6f, 2.15f, 0.0f, 0.015f, glass);
        SF(dx + 1.28f, 1.25f, dx + 1.7f, 2.15f, 0.0f, 0.025f, shut[k % 3]);
        SF(dx + 2.6f, 1.25f, dx + 3.02f, 2.15f, 0.0f, 0.025f, shut[k % 3]);
      }
      if (h > 4.6f) {                                   // upper floor windows
        SF(u + 0.8f, 3.05f, u + 1.6f, 3.95f, 0.0f, 0.015f, glass);
        if (w > 4.6f) SF(v - 1.6f, 3.05f, v - 0.8f, 3.95f, 0.0f, 0.015f, glass);
      }
      if (h > 7.6f) {                                   // ...and a third storey's
        SF(u + 0.8f, 6.20f, u + 1.6f, 7.10f, 0.0f, 0.015f, glass);
        if (w > 4.6f) SF(v - 1.6f, 6.20f, v - 0.8f, 7.10f, 0.0f, 0.015f, glass);
      }
    });
    KMAT(SMAT_STONE, SF(dx - 0.08f, 2.25f, dx + 1.13f, 2.40f, 0.0f, 0.10f, rgb(0.62f, 0.56f, 0.46f)));
    if (k & 1) {                                        // a striped awning, high and shallow
      v3 ac = awn[(k / 2) % 3];
      float ay = ax ? 2.70f : 2.62f;                   // (the two rows' awnings never share a plane)
      SF(u + 0.4f, ay, v - 0.4f, ay + 0.04f, 0.0f, 1.10f, ac);
      KMAT(SMAT_STEEL, {
        SF(u + 0.45f, 2.30f, u + 0.52f, ay, 1.00f, 1.07f, rgb(0.2f, 0.2f, 0.2f));
        SF(v - 0.52f, 2.30f, v - 0.45f, ay, 1.00f, 1.07f, rgb(0.2f, 0.2f, 0.2f));
      });
    }
    (void)m;
#undef SF
    u = v;
  }
}
// ---------------------------------------------------------------------------
// THE SOUK (0.8.7). The Bazaar was the 48 m market square and its outer street,
// walled in. It is now the heart of an 84 m market quarter: the shopfront row
// that stood against the old wall is pierced by passages, and behind it runs
// a ring of the old city in four layers, outward from the core:
//
//   23.9-27.5  THE ARCADE: a roofed hallway on timber beams with a skylight
//              slot in every bay, red and amber paper lanterns on the beams,
//              banners, and shops on both walls (dark shop mouths, sign
//              boards, counters with their goods — cover at waist height)
//   27.5-34.0  THE SHOP BLOCKS, split by cross-alleys strung with cloth; some
//              blocks have a stone stair up to a parapeted roof terrace
//   34.0-38.0  THE MARKET STREET under striped canopies and lanterns
//   38.0-41.9  THE OUTER HOUSES, two and three storeys, against the shell
//
// with a climbable lantern tower in each corner courtyard and a pagoda-crowned
// tower on the two long sides. Every piece is built in a BAND's own frame
// (a along the band, d outward from the core) and placed through g_ks, so the
// map keeps its 180-degree symmetry; the +z band runs the full width, the +x
// band fits between it and its mirror.
// ---------------------------------------------------------------------------
#define SK_IN  23.9f   // the inner row's back wall: the arcade's inner side
#define SK_A1  27.5f   // the arcade's outer side = the blocks' arcade face
#define SK_B1  34.0f   // the blocks' street face
#define SK_S1  38.0f   // the outer houses' face
#define SK_OUT 41.9f   // the outer houses' back (the shell's inner face is 42)
static int g_sk_ax;    // the band being built: 0 = a is x (the +z band), 1 = a is z
#define SKB(a0, y0, d0, a1, y1, d1, c)                                   \
  do { if (g_sk_ax == 0) KB(a0, y0, d0, a1, y1, d1, c);                 \
       else              KB(d0, y0, a0, d1, y1, a1, c); } while (0)
static const float SK_CLOTH[6][3] = {
  {0.90f, 0.62f, 0.18f}, {0.72f, 0.20f, 0.14f}, {0.20f, 0.28f, 0.58f},
  {0.16f, 0.48f, 0.46f}, {0.86f, 0.82f, 0.72f}, {0.66f, 0.20f, 0.42f}};
static v3 sk_cloth(float a, float d, int salt) {
  int i = (int)(6.0f * khash(a, d, salt)) % 6;
  return kt_(rgb(SK_CLOTH[i][0], SK_CLOTH[i][1], SK_CLOTH[i][2]), a, d, salt + 1, 0.90f, 1.04f);
}
// A paper lantern hung on a cord from `top`: a cap, the glowing body and a
// tassel, its bottom at 2.50 m so a body passes under it.
static void sk_lantern(float a, float d, float top, int salt) {
  v3 body = khash(a, d, salt) < 0.6f ? rgb(0.95f, 0.30f, 0.16f) : rgb(1.0f, 0.68f, 0.26f);
  v3 cap = rgb(0.16f, 0.12f, 0.10f);
  SKB(a - 0.015f, 2.98f, d - 0.015f, a + 0.015f, top, d + 0.015f, cap);           // the cord
  SKB(a - 0.12f, 2.92f, d - 0.12f, a + 0.12f, 2.98f, d + 0.12f, cap);             // cap
  KMAT(SMAT_LAMP, SKB(a - 0.17f, 2.60f, d - 0.17f, a + 0.17f, 2.92f, d + 0.17f, body));
  SKB(a - 0.10f, 2.54f, d - 0.10f, a + 0.10f, 2.60f, d + 0.10f, cap);             // foot
  SKB(a - 0.02f, 2.50f, d - 0.02f, a + 0.02f, 2.54f, d + 0.02f, body);            // tassel
}
// A SHOP in a wall at d = pl, its street on the `s` side (+1: toward larger
// d): the mouth (dark, 15 mm proud — rule 1, colour below head height), a
// timber lintel and a painted sign board above head height, and, if `ctr`, a
// counter in front with its goods: mounds of spice, bolts of cloth, crates.
static void sk_shop(float a0, float a1, float pl, float s, int ctr, int salt) {
  float m = 0.5f * (a0 + a1), w = fminf(2.4f, a1 - a0 - 0.6f) * 0.5f;
  if (w < 0.5f) return;
  v3 mouth = kt_(rgb(0.10f, 0.08f, 0.07f), m, pl, salt, 0.85f, 1.15f);
  v3 wood = kt_(rgb(0.36f, 0.24f, 0.15f), m, pl, salt + 1, 0.90f, 1.08f);
  KMAT(SMAT_WOOD, {
    SKB(m - w, 0.0f, pl + s * 0.002f, m + w, 2.28f, pl + s * 0.015f, mouth);
    SKB(m - w - 0.14f, 2.32f, pl, m + w + 0.14f, 2.44f, pl + s * 0.07f, wood);
    SKB(m - w + 0.05f, 2.52f, pl, m + w - 0.05f, 2.98f, pl + s * 0.09f, sk_cloth(m, pl, salt + 2));
    SKB(m - w + 0.18f, 2.62f, pl + s * 0.09f, m + w - 0.18f, 2.88f, pl + s * 0.10f,
       kt_(rgb(0.92f, 0.86f, 0.70f), m, pl, salt + 3, 0.9f, 1.0f));               // the lettering panel
  });
  if (!ctr) return;
  float c0 = m - w + 0.10f, c1 = m + w - 0.10f, dd = pl + s * 0.62f;
  KMAT(SMAT_WOOD, {
    SKB(c0, 0.0f, pl + s * 0.02f, c1, 0.92f, dd, wood);
    SKB(c0 - 0.03f, 0.92f, pl + s * 0.01f, c1 + 0.03f, 0.97f, dd + s * 0.03f, v3_scale(wood, 0.80f));
  });
  // the goods, one kind per shop
  int kind = (int)(4.0f * khash(m, pl, salt + 4)) % 4;
  float dc = pl + s * 0.32f;
  int n = (int)((c1 - c0) / 0.46f);
  for (int i = 0; i < n; i++) {
    float u = c0 + 0.23f + 0.46f * (float)i;
    v3 g = sk_cloth(u, pl, salt + 10 + i);
    if (kind == 0) {          // spice mounds in trays: a tray, a heap, its crown
      static const float SP[5][3] = {{0.86f, 0.54f, 0.12f}, {0.70f, 0.18f, 0.10f}, {0.92f, 0.76f, 0.20f},
                                     {0.40f, 0.46f, 0.18f}, {0.50f, 0.28f, 0.16f}};
      int k = (int)(5.0f * khash(u, pl, salt + 20)) % 5;
      v3 sp = rgb(SP[k][0], SP[k][1], SP[k][2]);
      KMAT(SMAT_WOOD, SKB(u - 0.19f, 0.97f, dc - 0.21f, u + 0.19f, 1.02f, dc + 0.21f, rgb(0.42f, 0.30f, 0.20f)));
      KMAT(SMAT_STUCCO, {
        SKB(u - 0.16f, 1.02f, dc - 0.18f, u + 0.16f, 1.10f, dc + 0.18f, sp);
        SKB(u - 0.10f, 1.10f, dc - 0.11f, u + 0.10f, 1.17f, dc + 0.11f, v3_scale(sp, 1.06f));
      });
    } else if (kind == 1) {   // bolts of cloth, stacked in pairs
      SKB(u - 0.20f, 0.97f, dc - 0.24f, u + 0.20f, 1.09f, dc + 0.24f, g);
      SKB(u - 0.17f, 1.09f, dc - 0.21f, u + 0.17f, 1.21f, dc + 0.21f, sk_cloth(u, dc, salt + 30 + i));
    } else if (kind == 2) {   // fruit crates
      static const float FR[4][3] = {{0.86f, 0.40f, 0.10f}, {0.64f, 0.70f, 0.16f}, {0.62f, 0.12f, 0.18f},
                                     {0.92f, 0.78f, 0.24f}};
      int k = (int)(4.0f * khash(u, pl, salt + 40)) % 4;
      KMAT(SMAT_WOOD, SKB(u - 0.20f, 0.97f, dc - 0.22f, u + 0.20f, 1.12f, dc + 0.22f, rgb(0.56f, 0.42f, 0.26f)));
      KMAT(SMAT_STUCCO, SKB(u - 0.17f, 1.12f, dc - 0.19f, u + 0.17f, 1.18f, dc + 0.19f, rgb(FR[k][0], FR[k][1], FR[k][2])));
    } else {                  // brass and copper ware
      v3 br = kt_(rgb(0.78f, 0.56f, 0.24f), u, pl, salt + 50, 0.85f, 1.10f);
      KMAT(SMAT_STEEL, {
        SKB(u - 0.12f, 0.97f, dc - 0.12f, u + 0.12f, 1.20f, dc + 0.12f, br);
        SKB(u - 0.06f, 1.20f, dc - 0.06f, u + 0.06f, 1.30f, dc + 0.06f, v3_scale(br, 0.9f));
      });
    }
  }
}
// A rug hung on a wall to dry: a border and a field 2 mm proud of it.
// A crate in the band frame (k_crate itself is x/z).
static void sk_crate(float a, float d, float hw, float h, v3 c) {
  if (g_sk_ax == 0) k_crate(a, d, hw, hw, h, c); else k_crate(d, a, hw, hw, h, c);
}
static void sk_rug(float a0, float a1, float y0, float y1, float pl, float s, int salt) {
  v3 b = sk_cloth(a0, pl, salt), f = sk_cloth(a1, pl, salt + 7);
  if (b.x == f.x) f = v3_scale(f, 0.62f);
  SKB(a0, y0, pl, a1, y1, pl + s * 0.012f, b);
  SKB(a0 + 0.10f, y0 + 0.12f, pl + s * 0.002f, a1 - 0.10f, y1 - 0.12f, pl + s * 0.016f, f);
}
// THE ARCADE over a0..a1: timber beams every 3.2 m resting on the two walls,
// a plank roof over 2.2 m of each bay and a skylight slot over the rest, a
// lantern on every other beam and a banner on the ones between. `nosh`:
// spans of a with no shops (the passages through the inner row), as pairs.
static void sk_arcade(float a0, float a1, float wall, const float *nosh, int nn, int salt) {
  v3 beam = rgb(0.30f, 0.21f, 0.14f), plank = rgb(0.44f, 0.32f, 0.21f);
  v3 pave = kt_(rgb(0.60f, 0.53f, 0.43f), a0, SK_IN, salt, 0.97f, 1.0f);
  KMAT(SMAT_STONE, SKB(a0 + 0.01f, 0.0f, SK_IN + 0.01f, a1 - 0.01f, 0.006f, SK_A1 - 0.01f, pave));
  int bay = 0;
  for (float u = a0 + 0.3f; u < a1 - 1.0f; u += 3.2f, bay++) {
    KMAT(SMAT_WOOD, {
      SKB(u, 3.26f, SK_IN, u + 0.16f, 3.40f, SK_A1, beam);
      if (u + 2.4f < a1) SKB(u - 0.02f, 3.40f, SK_IN, u + 2.2f, 3.48f, SK_A1,
                            kt_(plank, u, SK_IN, salt + 1, 0.88f, 1.08f));
    });
    float lc = u + 0.08f;
    if (bay & 1) sk_lantern(lc, 0.5f * (SK_IN + SK_A1), 3.26f, salt + 2);
    else SKB(lc - 0.45f, 2.62f, 25.68f, lc + 0.45f, 3.26f, 25.72f, sk_cloth(lc, 25.7f, salt + 3));
  }
  // shops along the inner wall (|a| < wall: past it the hall turns the
  // corner), a counter at every other one so the hall keeps 3 m; the outer
  // wall's shops belong to the blocks (sk_block), which know where they stand
  int k = 0;
  for (float u = -wall + 0.6f; u + 3.0f < wall; u += 3.6f, k++) {
    int skip = 0;
    for (int i = 0; i < nn; i++) if (u + 3.0f > nosh[2 * i] - 0.3f && u < nosh[2 * i + 1] + 0.3f) skip = 1;
    if (skip) continue;
    if (k % 4 == 3) sk_rug(u + 0.8f, u + 2.2f, 0.70f, 2.20f, SK_IN, 1.0f, salt + 300 + k);
    else sk_shop(u, u + 3.0f, SK_IN, 1.0f, (k & 1) == 0, salt + 100 + k);
  }
}
// A SHOP BLOCK a0..a1 between the arcade and the street, `h` tall: render,
// a cornice on its two long faces, a parapet with a gap where `stair` (-1 at
// a0, +1 at a1, 0 none) arrives, shops on both faces, awnings on the street
// face and windows up top on a two-storey block. Returns nothing.
static void sk_block(float a0, float a1, float h, int stair, int salt) {
  static const float PL[5][3] = {{0.86f, 0.78f, 0.62f}, {0.80f, 0.62f, 0.46f}, {0.90f, 0.86f, 0.76f},
                                 {0.76f, 0.64f, 0.50f}, {0.84f, 0.70f, 0.52f}};
  int pi = (int)(5.0f * khash(a0, a1, salt)) % 5;
  v3 pc = kt_(rgb(PL[pi][0], PL[pi][1], PL[pi][2]), a0, a1, salt + 1, 0.94f, 1.02f);
  v3 cc = v3_scale(pc, 0.86f);
  KMAT(SMAT_STUCCO, {
    SKB(a0, 0.0f, SK_A1, a1, h, SK_B1, pc);
    SKB(a0 + 0.02f, 0.0f, SK_A1 - 0.06f, a1 - 0.02f, 0.28f, SK_A1, v3_scale(pc, 0.78f));   // plinths
    SKB(a0 + 0.02f, 0.0f, SK_B1, a1 - 0.02f, 0.28f, SK_B1 + 0.06f, v3_scale(pc, 0.78f));
    SKB(a0, h - 0.30f, SK_A1 - 0.10f, a1, h - 0.14f, SK_A1, cc);                           // cornices
    SKB(a0, h - 0.30f, SK_B1, a1, h - 0.14f, SK_B1 + 0.10f, cc);
    // parapet: the long edges whole, the ends between them, the stair end split
    SKB(a0, h, SK_A1, a1, h + 0.60f, SK_A1 + 0.25f, cc);
    SKB(a0, h, SK_B1 - 0.25f, a1, h + 0.60f, SK_B1, cc);
  });
  const float RUN = 0.485f, LAND = 1.45f;
  int n = (int)(h / PF_RISE + 0.5f);
  float g0 = SK_A1 + 0.1f + (float)(n - 1) * RUN, g1 = g0 + LAND;     // the landing's span in d
  for (int e = -1; e <= 1; e += 2) {
    float x0 = e < 0 ? a0 : a1 - 0.25f, x1 = x0 + 0.25f;
    KMAT(SMAT_STUCCO, {
      if (stair == e) {
        if (g0 - (SK_A1 + 0.25f) > 0.2f) SKB(x0, h, SK_A1 + 0.25f, x1, h + 0.60f, g0, cc);
        if ((SK_B1 - 0.25f) - g1 > 0.2f) SKB(x0, h, g1, x1, h + 0.60f, SK_B1 - 0.25f, cc);
      } else {
        SKB(x0, h, SK_A1 + 0.25f, x1, h + 0.60f, SK_B1 - 0.25f, cc);
      }
    });
  }
  if (stair) {
    // The stair stands in the alley against the block's end wall, rising
    // from the arcade toward the street. stair_up's frame: a wall at
    // `plane` running along d, so its axis is the band's other one.
    float pl = stair < 0 ? a0 : a1;
    int inw = stair < 0 ? 1 : -1;
    v3 st = rgb(0.62f, 0.56f, 0.46f);
    int mf = g_mat_force; g_mat_force = SMAT_STONE;
    stair_up(1 - g_sk_ax, g_ks * pl, (int)g_ks * inw, g_ks * (SK_A1 + 0.1f), (int)g_ks, 1.20f, h,
             RUN, LAND, st, v3_scale(st, 0.86f));
    g_mat_force = mf;
  }
  // the faces: shops, awnings over the street, windows above
  int k = 0;
  for (float u = a0 + 0.4f; u + 2.8f <= a1 - 0.3f; u += 3.4f, k++) {
    sk_shop(u, u + 3.0f, SK_B1, 1.0f, k % 2 == 0, salt + 10 + k);
    if (k % 3 != 2) sk_shop(u + 0.2f, u + 3.2f, SK_A1, -1.0f, 0, salt + 40 + k);
    else sk_rug(u + 0.9f, u + 2.4f, 0.70f, 2.20f, SK_A1, -1.0f, salt + 40 + k);
    if (k & 1) {
      SKB(u + 0.1f, 3.04f, SK_B1, u + 2.9f, 3.08f, SK_B1 + 0.95f, sk_cloth(u, SK_B1, salt + 70 + k));
    }
    if (h > 5.0f) KMAT(SMAT_WOOD, {
      v3 gl = rgb(0.10f, 0.12f, 0.15f), sh = sk_cloth(u, h, salt + 80 + k);
      SKB(u + 1.0f, 3.70f, SK_B1, u + 2.0f, 4.60f, SK_B1 + 0.015f, gl);
      SKB(u + 0.55f, 3.70f, SK_B1, u + 0.98f, 4.60f, SK_B1 + 0.025f, sh);
      SKB(u + 2.02f, 3.70f, SK_B1, u + 2.45f, 4.60f, SK_B1 + 0.025f, sh);
      SKB(u + 1.0f, 3.70f, SK_A1 - 0.015f, u + 2.0f, 4.60f, SK_A1, gl);
    });
  }
}
// A CROSS-ALLEY a0..a1 through the blocks: if `lines` (a wall on both sides
// and no stair in it), cloth strung across at three heights; a crate or a
// barrel against a wall.
static void sk_alley(float a0, float a1, int lines, float pa, int salt) {
  for (int i = 0; i < 3 && lines; i++) {
    float d = SK_A1 + 1.6f + 1.9f * (float)i, y = 3.70f + 0.25f * (float)i;
    SKB(a0, y, d - 0.01f, a1, y + 0.02f, d + 0.01f, rgb(0.20f, 0.18f, 0.16f));                // the line
    for (float u = a0 + 0.35f; u + 0.6f < a1 - 0.2f; u += 0.95f)
      if (khash(u, d, salt + i) < 0.7f)
        SKB(u, y - 0.75f, d - 0.012f, u + 0.6f, y, d + 0.012f, sk_cloth(u, d, salt + 10 + i));  // washing
  }
  // the crate or barrel stands at a = pa (clear of any stair in the alley)
  if (khash(a0, a1, salt + 20) < 0.5f) sk_crate(pa, SK_A1 + 2.2f, 0.45f, 0.9f, rgb(0.56f, 0.42f, 0.26f));
  else if (g_sk_ax == 0) barrel(g_ks * pa, g_ks * (SK_B1 - 1.2f), rgb(0.30f, 0.36f, 0.46f));
  else barrel(g_ks * (SK_B1 - 1.2f), g_ks * pa, rgb(0.30f, 0.36f, 0.46f));
}
// THE MARKET STREET over a0..a1: striped canopies wall to wall, a lantern
// under every other one, goods against the outer houses.
static void sk_street(float a0, float a1, int salt) {
  v3 pave = kt_(rgb(0.58f, 0.51f, 0.41f), a0, SK_B1, salt, 0.97f, 1.0f);
  KMAT(SMAT_STONE, SKB(a0 + 0.01f, 0.0f, SK_B1 + 0.01f, a1 - 0.01f, 0.006f, SK_S1 - 0.01f, pave));
  int k = 0;
  for (float u = a0 + 0.2f; u + 2.6f < a1; u += 3.2f, k++) {
    v3 c = sk_cloth(u, SK_S1, salt + k);
    SKB(u, 3.22f, SK_B1 + 0.03f, u + 2.6f, 3.26f, SK_S1 - 0.03f, c);
    SKB(u, 3.18f, SK_B1 + 0.05f, u + 2.6f, 3.22f, SK_B1 + 0.33f, v3_scale(c, 0.7f));         // the hem
    if (k & 1) sk_lantern(u + 1.3f, 0.5f * (SK_B1 + SK_S1), 3.18f, salt + 50 + k);
    float h = khash(u, SK_S1, salt + 80);
    float gd = SK_S1 - 0.5f;
    if (h < 0.22f) sk_crate(u + 1.3f, gd, 0.40f, 0.8f, rgb(0.56f, 0.42f, 0.26f));
    else if (h < 0.36f) {
      if (g_sk_ax == 0) sack_pallet(g_ks * (u + 1.3f), g_ks * (gd - 0.05f), g_ks > 0 ? 0 : 2, rgb(0.62f, 0.55f, 0.40f));
      else sack_pallet(g_ks * (gd - 0.05f), g_ks * (u + 1.3f), g_ks > 0 ? 1 : 3, rgb(0.62f, 0.55f, 0.40f));
    } else if (h < 0.50f) {
      KMAT(SMAT_WOOD, {   // a basket stack: three woven drums
        SKB(u + 0.9f, 0.0f, gd - 0.30f, u + 1.5f, 0.55f, gd + 0.30f, rgb(0.62f, 0.48f, 0.28f));
        SKB(u + 1.55f, 0.0f, gd - 0.25f, u + 2.05f, 0.45f, gd + 0.25f, rgb(0.58f, 0.44f, 0.26f));
        SKB(u + 0.98f, 0.55f, gd - 0.22f, u + 1.42f, 0.90f, gd + 0.22f, rgb(0.66f, 0.52f, 0.30f));
      });
    }
  }
}
// A STONE LANTERN (the pagoda yard's): plinth, post, a glowing fire box, a
// cap and its finial. Built in the band frame.
static void sk_stone_lantern(float a, float d) {
  v3 st = rgb(0.56f, 0.54f, 0.50f);
  KMAT(SMAT_STONE, {
    SKB(a - 0.32f, 0.0f, d - 0.32f, a + 0.32f, 0.22f, d + 0.32f, st);
    SKB(a - 0.13f, 0.22f, d - 0.13f, a + 0.13f, 1.05f, d + 0.13f, st);
    SKB(a - 0.28f, 1.05f, d - 0.28f, a + 0.28f, 1.15f, d + 0.28f, st);
    SKB(a - 0.36f, 1.53f, d - 0.36f, a + 0.36f, 1.64f, d + 0.36f, st);
    SKB(a - 0.18f, 1.64f, d - 0.18f, a + 0.18f, 1.76f, d + 0.18f, st);
    SKB(a - 0.06f, 1.76f, d - 0.06f, a + 0.06f, 1.92f, d + 0.06f, st);
  });
  KMAT(SMAT_LAMP, SKB(a - 0.20f, 1.15f, d - 0.20f, a + 0.20f, 1.53f, d + 0.20f, rgb(1.0f, 0.70f, 0.32f)));
}
// THE PAGODA CROWN over a climbing tower's lookout at height H (world centre
// cx, cz, core half-width hc): red lacquered posts, three tiers of green tile
// roof with upturned corners, a glowing lantern room, a gilded spire.
static void sk_pagoda_crown(float cx, float cz, float hc, float H) {
  v3 lac = rgb(0.62f, 0.16f, 0.10f), tile = rgb(0.16f, 0.30f, 0.26f), gold = rgb(0.84f, 0.64f, 0.22f);
  const float RT = 0.12f, p = 0.20f, ph = 2.3f;
  int mf = g_mat_force; g_mat_force = SMAT_WOOD;
  bx(cx - hc + RT, H, cz - hc + RT, cx - hc + RT + p, H + ph, cz - hc + RT + p, lac);
  bx(cx + hc - RT - p, H, cz - hc + RT, cx + hc - RT, H + ph, cz - hc + RT + p, lac);
  bx(cx - hc + RT, H, cz + hc - RT - p, cx - hc + RT + p, H + ph, cz + hc - RT, lac);
  bx(cx + hc - RT - p, H, cz + hc - RT - p, cx + hc - RT, H + ph, cz + hc - RT, lac);
  float y = H + ph;
  static const float TW[3] = {1.70f, 1.25f, 0.85f}, CW[3] = {0.95f, 0.70f, 0.0f};
  for (int i = 0; i < 3; i++) {
    float w = hc + TW[i] - 1.0f + 0.6f;
    g_mat_force = SMAT_CLAD;
    bx(cx - w, y, cz - w, cx + w, y + 0.20f, cz + w, tile);                      // the tier
    for (int c = 0; c < 4; c++) {                                                 // upturned corners
      float sx = (c & 1) ? 1.0f : -1.0f, sz = (c & 2) ? 1.0f : -1.0f;
      bx(cx + sx * (w - 0.34f), y + 0.20f, cz + sz * (w - 0.34f), cx + sx * w, y + 0.42f, cz + sz * w,
         v3_scale(tile, 1.15f));
    }
    bx(cx - w * 0.62f, y + 0.20f, cz - w * 0.62f, cx + w * 0.62f, y + 0.34f, cz + w * 0.62f, v3_scale(tile, 0.85f));
    y += 0.34f;
    if (CW[i] > 0.0f) {                                                           // the room over it
      g_mat_force = SMAT_LAMP;
      bx(cx - CW[i], y, cz - CW[i], cx + CW[i], y + 0.62f, cz + CW[i], rgb(0.98f, 0.46f, 0.22f));
      y += 0.62f;
    }
  }
  g_mat_force = SMAT_STEEL;
  bx(cx - 0.10f, y, cz - 0.10f, cx + 0.10f, y + 1.8f, cz + 0.10f, gold);
  bx(cx - 0.22f, y + 0.5f, cz - 0.22f, cx + 0.22f, y + 0.62f, cz + 0.22f, gold);
  bx(cx - 0.18f, y + 0.9f, cz - 0.18f, cx + 0.18f, y + 1.0f, cz + 0.18f, gold);
  g_mat_force = mf;
}
// One band of the souk: a from a0 to a1 (the +z band runs -41.9..41.9 and
// ends in corner houses; the +x band runs between them, -23.9..23.9).
static void sk_band(int ax, int salt) {
  g_sk_ax = ax;
  v3 core = rgb(0.80f, 0.70f, 0.54f), step = rgb(0.64f, 0.58f, 0.48f);
  if (ax == 0) {
    // passages through the inner row: no shop is painted across them
    static const float NOSH[4] = {-14.0f, -11.0f, 11.0f, 14.0f};
    sk_arcade(-38.0f, 38.0f, SK_IN, NOSH, 2, salt);
    sk_block(-23.9f, -14.0f, 4.4f, -1, salt + 1);
    sk_alley(-14.0f, -11.0f, 1, -12.0f, salt + 2);
    sk_block(-11.0f, -4.0f, 6.2f, 0, salt + 3);
    sk_block(4.0f, 11.0f, 6.2f, 0, salt + 4);
    sk_alley(11.0f, 14.0f, 1, 13.0f, salt + 5);
    sk_block(14.0f, 23.9f, 4.4f, 1, salt + 6);
    sk_alley(-27.5f, -23.9f, 0, -26.8f, salt + 7);
    sk_alley(23.9f, 27.5f, 0, 26.6f, salt + 8);
    sk_alley(-38.0f, -34.0f, 0, -35.5f, salt + 9);
    sk_alley(34.0f, 38.0f, 0, 36.5f, salt + 10);
    sk_street(-38.0f, 38.0f, salt + 11);
    k_shopfront(0, -38.0f, 38.0f, SK_S1, SK_OUT, salt + 12, 5.2f, 3.6f);
    // the corner houses, their front on the alley that closes each street,
    // and a plain corner block behind the end of the outer row
    // (the corner houses stop 10 cm short of the street's end, so their
    // plinth never meets the outer row's in the corner; the block fills it)
    k_shopfront(1, SK_IN, SK_S1 - 0.1f, 38.0f, SK_OUT, salt + 13, 6.5f, 2.5f);
    k_shopfront(1, SK_IN, SK_S1 - 0.1f, -38.0f, -SK_OUT, salt + 14, 6.5f, 2.5f);
    KMAT(SMAT_STUCCO, {
      KB(38.0f, 0.0f, SK_S1 - 0.1f, SK_OUT, 8.6f, SK_OUT, kt_(core, 1, 1, salt + 15, 0.90f, 0.96f));
      KB(-SK_OUT, 0.0f, SK_S1 - 0.1f, -38.0f, 8.2f, SK_OUT, kt_(core, 2, 2, salt + 16, 0.90f, 0.96f));
    });
    // the corner towers, each in its courtyard, and the pagoda over the
    // yard in the middle of the side with its stone lanterns
    tower(g_ks * 30.75f, g_ks * 30.75f, 1.3f, 8.0f, core, step, 1, -1);
    tower(g_ks * -30.75f, g_ks * 30.75f, 1.3f, 8.0f, core, step, 1, -1);
    tower(0.0f, g_ks * 30.75f, 1.3f, 9.6f, kt_(core, 3, 3, salt + 17, 1.0f, 1.04f), step, 0, -1);
    sk_pagoda_crown(0.0f, g_ks * 30.75f, 1.3f, 9.6f);
    sk_stone_lantern(-3.3f, 28.2f);
    sk_stone_lantern(3.3f, 33.3f);
  } else {
    static const float NOSH[4] = {-14.0f, -11.0f, 11.0f, 14.0f};
    sk_arcade(-23.9f, 23.9f, SK_IN, NOSH, 2, salt);
    sk_block(-23.9f, -14.0f, 6.2f, 0, salt + 1);
    sk_alley(-14.0f, -11.0f, 1, -12.0f, salt + 2);
    sk_block(-11.0f, -1.5f, 4.4f, 0, salt + 3);
    sk_alley(-1.5f, 1.5f, 0, -0.8f, salt + 4);
    sk_block(1.5f, 11.0f, 4.4f, -1, salt + 5);
    sk_alley(11.0f, 14.0f, 1, 13.0f, salt + 6);
    sk_block(14.0f, 23.9f, 6.2f, 0, salt + 7);
    sk_street(-23.9f, 23.9f, salt + 8);
    k_shopfront(1, -23.9f, 23.9f, SK_S1, SK_OUT, salt + 9, 5.2f, 3.6f);
  }
}
static void map_bazaar(void) {
  small_map_style();
  const theme_t *t = &g_theme;
  // timber rails on the towers, and stone for their crowns
  g_town.trim = rgb(0.40f, 0.28f, 0.18f);
  g_town.stone = rgb(0.62f, 0.56f, 0.46f);
  // (no shell piers: the outer houses stand in front of the whole shell)
  // the old outer street's shopfronts, a row along each side of the core,
  // now pierced by the passages to the arcade behind them (the 180-degree
  // symmetry is kept by building each half's two rows through g_ks)
  for (int hh = 0; hh < 2; hh++) {
    g_ks = hh ? -1.0f : 1.0f;
    k_shopfront(0, -SK_IN, -14.0f, 22.0f, SK_IN, 300, 3.6f, 2.4f);   // the +z row, facing -z
    k_shopfront(0, -11.0f, 11.0f, 22.0f, SK_IN, 310, 3.6f, 2.4f);
    k_shopfront(0, 14.0f, SK_IN, 22.0f, SK_IN, 320, 3.6f, 2.4f);
    k_shopfront(1, -21.8f, -14.0f, 22.0f, SK_IN, 400, 3.6f, 2.4f);  // the +x row, facing -x
    k_shopfront(1, -11.0f, 11.0f, 22.0f, SK_IN, 410, 3.6f, 2.4f);
    k_shopfront(1, 14.0f, 21.8f, 22.0f, SK_IN, 420, 3.6f, 2.4f);
    sk_band(0, 500);
    sk_band(1, 600);
  }
  g_ks = 1.0f;
  // THE FOUNTAIN: an octagonal stone basin with water, a column, an upper
  // bowl and the blue spire the map is navigated by.
  v3 stone = rgb(0.66f, 0.60f, 0.50f);
  KMAT(SMAT_STONE, {
    octa(0.0f, 0.0f, 1.6f, 0.0f, 0.62f, stone);
    octa(0.0f, 0.0f, 0.36f, 0.64f, 1.60f, stone);
    octa(0.0f, 0.0f, 0.80f, 1.60f, 1.76f, stone);
  });
  octa(0.0f, 0.0f, 1.40f, 0.62f, 0.64f, rgb(0.10f, 0.22f, 0.26f));
  octa(0.0f, 0.0f, 0.62f, 1.76f, 1.78f, rgb(0.10f, 0.22f, 0.26f));
  octa(0.0f, 0.0f, 0.20f, 1.78f, 3.0f, t->mid);
  g_ks = 1.0f;  bazaar_half();
  g_ks = -1.0f; bazaar_half();
  g_ks = 1.0f;
  stall(-2.5f, 8.0f, 1, t->band_s);
  stall(8.0f, -2.5f, 0, t->band_n);
  stall(17.0f, 1.0f, 1, t->mid);
  stall(17.0f, 9.0f, 1, t->band_s);
  stall(15.0f, -16.0f, 0, t->band_n);
  stall(2.0f, 16.0f, 0, t->mid);
}

// OUTPOST — a forward operating base at high noon. Three lanes split by long
// walls with offset gaps, a tower in the middle, a gated bunker at each end, a
// climbable container in each side lane: long sightlines, built for the
// sniper. The walls are T-wall blast barriers now, the perimeter is lined with
// HESCO, the bunkers are sandbagged under camo nets, the tower is a sandbagged
// watchtower with a mast, and the base has its floodlights, generator, water
// tank and fuel.
// A railing in the half being built (edge_rail through the rotation sign).
static void k_rail(int ax, float d0, float a0, float a1, float y, v3 curb, v3 iron) {
  if (g_ks > 0.0f) edge_rail(ax, d0, a0, a1, y, curb, iron);
  else             edge_rail(ax, -d0 - 0.14f, -a1, -a0, y, curb, iron);
}
// A HESCO BASTION: one fill block (x0..x1, z0..z1, h tall) wired on its
// outside only, like the watchtower's — baskets built one by one would put
// their posts face to face inside the block.
static void k_bastion(float x0, float z0, float x1, float z1, float h) {
  v3 fill = rgb(0.66f, 0.60f, 0.47f), wire = rgb(0.34f, 0.35f, 0.33f);
  KMAT(SMAT_STUCCO, KB(x0, 0.0f, z0, x1, h, z1, kt_(fill, x0, z0, 5, 0.92f, 1.04f)));
  KMAT(SMAT_STEEL, {
    for (float u = x0; u <= x1 + 0.001f; u += 1.06f) {
      float u0 = u > x1 - 0.03f ? x1 - 0.03f : u;
      KB(u0, 0.0f, z0 - 0.015f, u0 + 0.03f, h + 0.02f, z0, wire);
      KB(u0, 0.0f, z1, u0 + 0.03f, h + 0.02f, z1 + 0.015f, wire);
    }
    for (float u = z0; u <= z1 + 0.001f; u += 1.06f) {
      float u0 = u > z1 - 0.03f ? z1 - 0.03f : u;
      KB(x0 - 0.015f, 0.0f, u0, x0, h + 0.02f, u0 + 0.03f, wire);
      KB(x1, 0.0f, u0, x1 + 0.015f, h + 0.02f, u0 + 0.03f, wire);
    }
  });
}
// A BARRACKS TENT: a canvas body on a timber floor, a ridged roof in four
// stepped courses with a little eave, a door on each end. Long axis along z.
static void k_tent(float x0, float z0, float x1, float z1, v3 canvas) {
  v3 dk = rgb(0.17f, 0.18f, 0.16f), fl = rgb(0.42f, 0.34f, 0.24f);
  float w = x1 - x0, cx = (x0 + x1) * 0.5f;
  KMAT(SMAT_WOOD, KB(x0 - 0.10f, 0.0f, z0 - 0.10f, x1 + 0.10f, 0.12f, z1 + 0.10f, fl));
  KMAT(SMAT_STUCCO, {
    KB(x0, 0.12f, z0, x1, 1.72f, z1, canvas);
    KB(x0 - 0.06f, 1.72f, z0 - 0.06f, x1 + 0.06f, 1.97f, z1 + 0.06f, kt_(canvas, x0, z0, 1, 0.90f, 0.94f));
    KB(cx - w * 0.36f, 1.97f, z0 - 0.03f, cx + w * 0.36f, 2.24f, z1 + 0.03f, kt_(canvas, x0, z0, 2, 0.94f, 0.98f));
    KB(cx - w * 0.22f, 2.24f, z0, cx + w * 0.22f, 2.47f, z1, kt_(canvas, x0, z0, 3, 0.97f, 1.0f));
    KB(cx - w * 0.09f, 2.47f, z0 + 0.02f, cx + w * 0.09f, 2.62f, z1 - 0.02f, kt_(canvas, x0, z0, 4, 0.86f, 0.9f));
  });
  KB(cx - 0.55f, 0.12f, z0 - 0.006f, cx + 0.55f, 1.62f, z0, dk);       // doors
  KB(cx - 0.55f, 0.12f, z1, cx + 0.55f, 1.62f, z1 + 0.006f, dk);
  for (float z = z0 + 1.2f; z < z1 - 0.6f; z += 1.6f)                   // window flaps
    for (int sd = 0; sd < 2; sd++) {
      float xa = sd ? x1 : x0 - 0.006f;
      KB(xa, 1.02f, z, xa + 0.006f, 1.42f, z + 0.70f, kt_(canvas, z, xa, 6, 0.55f, 0.6f));
    }
}

static void outpost_half(void) {
  const theme_t *t = &g_theme;
  v3 ply = rgb(0.56f, 0.48f, 0.36f), pole = rgb(0.40f, 0.36f, 0.30f);
  v3 olive = rgb(0.34f, 0.36f, 0.24f), dark = rgb(0.17f, 0.18f, 0.19f);
  // HESCO along this half's north and east shell walls.
  k_hesco(-23.45f, -23.45f, 0, 45, 2.2f);
  k_hesco(23.45f, -22.37f, 1, 43, 2.2f);
  // T-WALLS for the lane walls.
  k_twall(1, -8.0f, -12.0f, -4.0f, 2.8f);
  k_twall(1, -8.0f, 1.5f, 12.0f, 2.8f);
  // THE BUNKERS: two fill platforms either side of the gate, a sandbag
  // parapet along their front, crates as the step up at the outer ends, a
  // raised boom at the gate and a camo net over the lot.
  KMAT(SMAT_STUCCO, {
    KB(-6.0f, 0.0f, 16.6f, -1.0f, 1.6f, 21.0f, kt_(rgb(0.66f, 0.60f, 0.47f), 1, 1, 1, 0.95f, 1.0f));
    KB(1.0f, 0.0f, 16.6f, 6.0f, 1.6f, 21.0f, kt_(rgb(0.66f, 0.60f, 0.47f), 2, 2, 1, 0.95f, 1.0f));
  });
  KMAT(SMAT_STEEL, for (float x = -6.0f; x < 6.01f; x += 1.0f) {
    if (x > -1.01f && x < 1.01f) continue;
    KB(x, 0.0f, 16.585f, x + 0.03f, 1.58f, 16.6f, rgb(0.34f, 0.35f, 0.33f));
  });
  k_sandbags(0, 16.85f, -5.8f, -1.2f, 1.6f, 7, 0.46f);
  k_sandbags(0, 16.85f, 1.2f, 5.8f, 1.6f, 7, 0.46f);
  k_crate(6.5f, 19.0f, 0.5f, 1.0f, 0.8f, olive);
  for (int i = 0; i < 6; i++)
    KB(0.95f, 1.2f + 0.5f * (float)i, 16.95f, 1.07f, 1.7f + 0.5f * (float)i, 17.07f,
       (i & 1) ? rgb(0.86f, 0.84f, 0.80f) : rgb(0.72f, 0.16f, 0.12f));
  KMAT(SMAT_STEEL, KB(0.90f, 0.0f, 16.9f, 1.12f, 1.2f, 17.12f, dark));
  KMAT(SMAT_WOOD, {
    KB(-6.5f, 0.0f, 16.2f, -6.4f, 3.75f, 16.3f, pole);
    KB(6.4f, 0.0f, 16.2f, 6.5f, 3.75f, 16.3f, pole);
    KB(-6.5f, 1.6f, 21.4f, -6.4f, 3.75f, 21.5f, pole);
    KB(6.4f, 1.6f, 21.4f, 6.5f, 3.75f, 21.5f, pole);
  });
  for (int i = 0; i < 7; i++) {
    float z0 = 16.1f + 0.78f * (float)i;
    KB(-6.7f, 3.75f + 0.01f * (float)(i & 1), z0, 6.7f, 3.78f + 0.01f * (float)(i & 1), z0 + 0.62f,
       kt_(i & 1 ? rgb(0.44f, 0.42f, 0.30f) : rgb(0.36f, 0.38f, 0.26f), (float)i, 1, 12, 0.9f, 1.1f));
  }
  // middle-lane cover: a jersey barrier and ammo crates
  k_jersey(0, 8.25f, -3.0f, 1.0f);
  k_crate(3.6f, -5.4f, 0.6f, 0.6f, 1.15f, olive);
  k_crate(-3.9f, 13.6f, 0.6f, 0.6f, 1.0f, t->crate_a);
  // SIDE LANE: the container, a 10 ft box on it, crates as the steps up, a
  // sandbag wall, a crate pile and the fuel point.
  container(g_ks * 18.2f, g_ks * -9.0f, 1, 0.0f, rgb(0.48f, 0.42f, 0.28f));
  KMAT(SMAT_CONTAINER, KB(17.08f, 2.6f, -11.6f, 19.32f, 5.2f, -8.6f, rgb(0.34f, 0.38f, 0.26f)));
  k_crate(19.9f, -7.3f, 0.5f, 0.7f, 0.85f, olive);
  k_crate(19.9f, -8.7f, 0.5f, 0.7f, 1.70f, kt_(olive, 1, 1, 13, 0.9f, 0.95f));
  k_sandbags(1, 12.25f, -2.0f, 5.0f, 0.0f, 6, 0.5f);
  k_crate(14.6f, 12.6f, 0.6f, 0.6f, 1.1f, olive);
  k_crate(15.5f, 12.6f, 0.3f, 0.3f, 0.55f, t->crate_a);
  barrel(g_ks * 21.2f, g_ks * 0.3f, olive);
  barrel(g_ks * 21.9f, g_ks * 0.8f, olive);
  barrel(g_ks * 21.2f, g_ks * 1.05f, rgb(0.62f, 0.18f, 0.12f));
  // FLOODLIGHT on a mast, aimed at the middle.
  KMAT(SMAT_STEEL, {
    KB(20.3f, 0.0f, 20.3f, 20.5f, 6.4f, 20.5f, dark);
    KB(19.7f, 6.4f, 20.2f, 20.6f, 6.85f, 20.4f, dark);
  });
  KB(19.75f, 6.45f, 20.18f, 20.55f, 6.8f, 20.2f, rgb(1.0f, 0.96f, 0.84f));
  // GENERATOR with its exhaust, a water tank on legs.
  KMAT(SMAT_STEEL, {
    KB(-21.6f, 0.0f, 7.0f, -20.4f, 1.25f, 9.2f, olive);
    KB(-21.5f, 1.25f, 7.2f, -20.5f, 1.32f, 8.9f, dark);
    KB(-21.15f, 1.25f, 8.95f, -21.0f, 2.1f, 9.1f, dark);
  });
  KMAT(SMAT_STEEL, {
    KB(-21.4f, 0.0f, 12.6f, -21.3f, 1.2f, 12.7f, dark);
    KB(-19.9f, 0.0f, 12.6f, -19.8f, 1.2f, 12.7f, dark);
    KB(-21.4f, 0.0f, 14.1f, -21.3f, 1.2f, 14.2f, dark);
    KB(-19.9f, 0.0f, 14.1f, -19.8f, 1.2f, 14.2f, dark);
    octa(g_ks * -20.6f, g_ks * 13.4f, 0.9f, 1.2f, 2.6f, rgb(0.16f, 0.16f, 0.17f));
  });
  // THE NORTH BASTION: a 3 x 2 bay HESCO block in the corner, a sandbag
  // parapet on the side that looks down this half's side lane, rails round
  // the rest, and a ladder up its yard face — a sniper's nest the lane can
  // fight back at.
  k_bastion(15.0f, -21.6f, 18.18f, -19.48f, 2.2f);
  k_sandbags(0, -19.70f, 15.0f, 18.18f, 2.2f, 4, 0.40f);
  {
    v3 curb = rgb(0.56f, 0.52f, 0.42f), iron = rgb(0.24f, 0.25f, 0.24f);
    k_rail(1, 15.02f, -21.58f, -19.92f, 2.2f, curb, iron);
    k_rail(0, -21.58f, 15.18f, 18.0f, 2.2f, curb, iron);
  }
  ladder_add(g_ks * 18.18f, g_ks * -20.55f, g_ks > 0.0f ? SIDE_E : SIDE_W);
  // A BARRACKS TENT in the side lane: cover with a roof over it, and its two
  // ends are doors you can see through only by the gaps beside it.
  k_tent(15.7f, 0.6f, 19.5f, 5.6f, rgb(0.44f, 0.43f, 0.30f));
  k_crate(14.9f, 6.3f, 0.45f, 0.45f, 0.9f, olive);
  sack_pallet(g_ks * 17.6f, g_ks * -0.45f, g_ks > 0.0f ? 0 : 2, rgb(0.62f, 0.55f, 0.40f));
  ibc_tote(g_ks * -20.9f, g_ks * 10.6f, g_ks > 0.0f ? 0 : 2, rgb(0.86f, 0.86f, 0.82f), rgb(0.15f, 0.30f, 0.55f));
  (void)ply;
}
static void map_outpost(void) {
  small_map_style();
  const theme_t *t = &g_theme;
  // THE WATCHTOWER (built once, it stands on the origin): a 3 x 3 block of
  // HESCO, a sandbag parapet on it, four posts, a roof and the radio mast.
  // One fill block (nine baskets' worth) wired on its OUTSIDE only: baskets
  // built one by one would put their posts face to face inside the block.
  {
    const float R = 1.59f, Hh = 2.6f;
    v3 wire = rgb(0.34f, 0.35f, 0.33f);
    KMAT(SMAT_STUCCO, bx(-R, 0.0f, -R, R, Hh, R, rgb(0.66f, 0.60f, 0.47f)));
    KMAT(SMAT_STEEL, {
      for (int k = 0; k <= 3; k++) {
        float u = -R + 1.06f * (float)k;
        float u0 = k == 3 ? u - 0.03f : u, u1 = u0 + 0.03f;
        bx(u0, 0.0f, -R - 0.015f, u1, Hh + 0.02f, -R, wire);
        bx(u0, 0.0f, R, u1, Hh + 0.02f, R + 0.015f, wire);
        bx(-R - 0.015f, 0.0f, u0, -R, Hh + 0.02f, u1, wire);
        bx(R, 0.0f, u0, R + 0.015f, Hh + 0.02f, u1, wire);
      }
      // (no top rim: the sandbag parapet sits on that edge)
    });
  }
  k_sandbags(0, -1.36f, -1.59f, 1.59f, 2.6f, 4, 0.40f);
  g_ks = -1.0f;
  k_sandbags(0, -1.36f, -1.59f, 1.59f, 2.6f, 4, 0.40f);
  g_ks = 1.0f;
  k_sandbags(1, -1.36f, -1.16f, 1.16f, 2.6f, 4, 0.40f);
  g_ks = -1.0f;
  k_sandbags(1, -1.36f, -1.16f, 1.16f, 2.6f, 4, 0.40f);
  g_ks = 1.0f;
  v3 pole = rgb(0.40f, 0.36f, 0.30f), dark = rgb(0.17f, 0.18f, 0.19f);
  KMAT(SMAT_WOOD, {
    bx(-1.5f, 2.45f, -1.5f, -1.38f, 4.9f, -1.38f, pole);   // seated in the fill
    bx(1.38f, 2.45f, -1.5f, 1.5f, 4.9f, -1.38f, pole);
    bx(-1.5f, 2.45f, 1.38f, -1.38f, 4.9f, 1.5f, pole);
    bx(1.38f, 2.45f, 1.38f, 1.5f, 4.9f, 1.5f, pole);
    bx(-1.75f, 4.9f, -1.75f, 1.75f, 5.02f, 1.75f, rgb(0.50f, 0.44f, 0.32f));
  });
  bx(-1.8f, 5.02f, -1.8f, 1.8f, 5.06f, 1.8f, t->mid);
  KMAT(SMAT_STEEL, {
    bx(0.9f, 5.06f, 0.9f, 1.0f, 9.4f, 1.0f, dark);
    bx(0.6f, 7.6f, 0.93f, 1.3f, 7.65f, 0.97f, dark);
    bx(0.7f, 8.6f, 0.93f, 1.2f, 8.65f, 0.97f, dark);
  });
  g_ks = 1.0f;  outpost_half();
  g_ks = -1.0f; outpost_half();
  g_ks = 1.0f;
}

// Water: a dark sheet on the floor, and a quay railing along every edge that
// borders walkable ground — top rail above a jump (1.35 m), mid rail, posts.
static void water_rect(float x0, float z0, float x1, float z1) {
  bx(x0, 0.0f, z0, x1, 0.02f, z1, rgb(0.08f, 0.17f, 0.21f));
}
static void quay_rail(int ax, float plane, int inw, float a0, float a1) {
  v3 m = rgb(0.20f, 0.22f, 0.24f);
  fbox(ax, plane, inw, a0, a1, 0.0f, 0.10f, 1.25f, 1.35f, m);
  fbox(ax, plane, inw, a0, a1, 0.0f, 0.10f, 0.55f, 0.62f, m);
  for (float a = a0 + 0.2f; a + 0.12f <= a1; a += 2.0f)
    fbox(ax, plane, inw, a, a + 0.12f, 0.02f, 0.14f, 0.02f, 1.25f, m);
}

// A LADDER up a wall face. `side` names the face it hangs on (SIDE_N: the
// face looking -z, ...). The climb height is read off the world: the highest
// solid just inside the face at the ladder's line, i.e. the parapet's crown.
// Two rails and rungs stand 16 cm off the wall, which is also what keeps a
// climber's box clear of the cornice on the way up.
#define LADDER_HALF  0.30f   // rail to rail / 2
#define LADDER_OFF   0.19f   // rails' outer depth off the face (> 0.12f trim)
// Anything jutting past the rails into the climb column (an awning, a
// balcony) would stop a climber dead, so the ladder slides along the face to
// the nearest clear stretch, up to 3 m either way.
static int ladder_clear(float x, float z, float nx, float nz, float top) {
  float tx = -nz, tz = nx;
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *b = &g_solids[i];
    if (b->max.y < 0.25f || b->min.y > top) continue;
    float cx = (b->min.x + b->max.x) * 0.5f - x, cz = (b->min.z + b->max.z) * 0.5f - z;
    float hx = (b->max.x - b->min.x) * 0.5f, hz = (b->max.z - b->min.z) * 0.5f;
    float dc = cx * nx + cz * nz, dh = hx * fabsf(nx) + hz * fabsf(nz);
    float ac = cx * tx + cz * tz, ah = hx * fabsf(tx) + hz * fabsf(tz);
    if (dc + dh > LADDER_OFF + 0.02f && dc - dh < LADDER_OFF + 0.9f &&
        fabsf(ac) - ah < LADDER_HALF + 0.45f)
      return 0;
  }
  return 1;
}
static void ladder_add(float x, float z, int side) {
  if (g_nladders >= MAX_LADDERS) return;
  float nx = side == SIDE_E ? 1.0f : side == SIDE_W ? -1.0f : 0.0f;
  float nz = side == SIDE_S ? 1.0f : side == SIDE_N ? -1.0f : 0.0f;
  float tx = -nz, tz = nx;             // along the face
  float top = 0.0f;
  int ok = 0;
  for (int k = 0; k <= 12 && !ok; k++) {
    float off = (float)((k + 1) / 2) * 0.5f * ((k & 1) ? 1.0f : -1.0f);
    float cx = x + tx * off, cz = z + tz * off;
    float px = cx - nx * 0.12f, pz = cz - nz * 0.12f;
    top = 0.0f;
    for (int i = 0; i < g_num_solids; i++) {
      const solid_t *b = &g_solids[i];
      if (px >= b->min.x && px <= b->max.x && pz >= b->min.z && pz <= b->max.z &&
          b->max.y > top)
        top = b->max.y;
    }
    if (top >= 1.0f && ladder_clear(cx, cz, nx, nz, top)) { x = cx; z = cz; ok = 1; }
  }
  if (!ok) return;                     // no clear wall here: no ladder
  // A GAP IN THE PARAPET over the ladder, so the way down is as open as the
  // way up: the crown that set `top` is split round the ladder's span, and
  // the climb then ends at the roof itself.
  {
    float px = x - nx * 0.12f, pz = z - nz * 0.12f, gh = LADDER_HALF + 0.30f;
    int n0 = g_num_solids;
    for (int i = 0; i < n0; i++) {
      solid_t *b = &g_solids[i];
      if (fabsf(b->max.y - top) > 0.01f || b->max.y - b->min.y > 1.5f) continue;
      if (px < b->min.x || px > b->max.x || pz < b->min.z || pz > b->max.z) continue;
      solid_t hi = *b;
      if (fabsf(tx) > 0.5f) { b->max.x = x - gh; hi.min.x = x + gh; }
      else                  { b->max.z = z - gh; hi.min.z = z + gh; }
      int keep_lo = b->max.x - b->min.x > 0.05f && b->max.z - b->min.z > 0.05f;
      int keep_hi = hi.max.x - hi.min.x > 0.05f && hi.max.z - hi.min.z > 0.05f;
      if (!keep_lo && keep_hi) { *b = hi; keep_hi = 0; keep_lo = 1; }
      if (!keep_lo) b->max = b->min;   // degenerate: collides with nothing
      if (keep_hi) map_add(hi.min, hi.max, hi.color);
    }
    float t2 = 0.0f;
    for (int i = 0; i < g_num_solids; i++) {
      const solid_t *b = &g_solids[i];
      if (px >= b->min.x && px <= b->max.x && pz >= b->min.z && pz <= b->max.z &&
          b->max.y > t2)
        t2 = b->max.y;
    }
    if (t2 >= 1.0f) top = t2;
  }
  g_ladders[g_nladders++] = (ladder_t){x, z, nx, nz, top};
  v3 m = rgb(0.86f, 0.64f, 0.14f);   // safety yellow: findable from the street
  for (int r = -1; r <= 1; r += 2) {   // rails stand clear of 0.12f trim
    float ax = x + tx * LADDER_HALF * (float)r, az = z + tz * LADDER_HALF * (float)r;
    bx(ax - 0.03f * fabsf(tx) + nx * 0.135f, 0.02f, az - 0.03f * fabsf(tz) + nz * 0.135f,
       ax + 0.03f * fabsf(tx) + nx * LADDER_OFF, top + 0.45f,
       az + 0.03f * fabsf(tz) + nz * LADDER_OFF, m);
  }
  for (float y = 0.30f; y < top - 0.1f; y += 0.30f)   // rungs
    bx(x - tx * (LADDER_HALF - 0.03f) + nx * 0.145f, y, z - tz * (LADDER_HALF - 0.03f) + nz * 0.145f,
       x + tx * (LADDER_HALF - 0.03f) + nx * 0.180f, y + 0.04f, z + tz * (LADDER_HALF - 0.03f) + nz * 0.180f, m);
}

static void flare_stack(float x, float z, float h) {
  bx(x - 2.0f, 0.0f, z - 2.0f, x + 2.0f, 3.0f, z + 2.0f, g_town.stone);
  for (int i = 0; 3.0f + (float)i * 2.0f < h; i++) {
    float y0 = 3.0f + (float)i * 2.0f;
    bx(x - 0.6f, y0, z - 0.6f, x + 0.6f, fminf(y0 + 2.0f, h), z + 0.6f,
       (i & 1) ? rgb(0.86f, 0.86f, 0.84f) : rgb(0.72f, 0.16f, 0.12f));
  }
  bx(x - 0.8f, h, z - 0.8f, x + 0.8f, h + 0.5f, z + 0.8f, rgb(0.16f, 0.16f, 0.17f));
}

static void cooling_tower(float cx, float cz) {
  // Grounded catch basin + open air-intake colonnade + 13-ring hyperbolic shell.
  static const float R[13] = {
    7.45f, 7.00f, 6.62f, 6.32f, 6.08f, 5.92f, 5.82f,
    5.78f, 5.80f, 5.88f, 6.02f, 6.20f, 6.44f
  };
  v3 c = rgb(0.74f, 0.72f, 0.68f), base = rgb(0.54f, 0.53f, 0.50f), dark = rgb(0.26f, 0.27f, 0.29f);
  octa(cx, cz, 8.45f, 0.0f, 0.55f, base);                             // concrete apron
  octa(cx, cz, 6.20f, 0.55f, 0.65f, rgb(0.14f, 0.24f, 0.28f));        // intake water pool
  octa(cx, cz, 2.55f, 0.65f, 2.40f, dark);                            // central riser shaft
  static const float PIL[8][2] = {
    {6.85f, 1.35f}, {-6.85f, -1.35f}, {1.35f, -6.85f}, {-1.35f, 6.85f},
    {5.00f, 5.00f}, {-5.00f, 5.00f}, {5.00f, -5.00f}, {-5.00f, -5.00f}
  };
  for (int i = 0; i < 8; i++)
    bx(cx + PIL[i][0] - 0.26f, 0.55f, cz + PIL[i][1] - 0.26f,
       cx + PIL[i][0] + 0.26f, 2.40f, cz + PIL[i][1] + 0.26f, base);
  for (int i = 0; i < 13; i++) {
    float s = 0.96f + 0.015f * (float)(i & 3);
    octa(cx, cz, R[i], 2.40f + (float)i * 1.20f, 2.40f + (float)(i + 1) * 1.20f,
         (v3){{c.x * s, c.y * s, c.z * s}});
  }
  octa(cx, cz, 6.04f, 9.45f, 9.72f, rgb(0.46f, 0.46f, 0.44f));        // throat stiffening ring
  octa(cx, cz, 6.66f, 18.00f, 18.45f, rgb(0.36f, 0.36f, 0.36f));      // top deck rim
  octa(cx, cz, 5.45f, 18.45f, 18.60f, rgb(0.14f, 0.14f, 0.16f));      // dark exhaust opening
}

typedef struct { char ch; float y0, y1, inset; int col; } mapgrid_tile_t;
enum { MGC_WALL, MGC_COVER, MGC_CRATE_A, MGC_CRATE_B, MGC_MID, MGC_SITE_A, MGC_SITE_B };
static const mapgrid_tile_t MAPGRID_TILES[] = {
  {'#', 0.0f, 8.0f, 0.0f, MGC_WALL},    {'%', 0.0f, 5.0f, 0.0f, MGC_COVER},
  {'w', 0.0f, 3.2f, 0.0f, MGC_CRATE_B}, {'r', 3.4f, 3.9f, 0.0f, MGC_CRATE_B},
  {'^', 2.4f, 2.8f, 0.0f, MGC_COVER},
  {'1', 0.0f, 0.4f, 0.0f, MGC_CRATE_B}, {'2', 0.0f, 0.8f, 0.0f, MGC_CRATE_B},
  {'3', 0.0f, 1.2f, 0.0f, MGC_CRATE_B}, {'4', 0.0f, 1.6f, 0.0f, MGC_CRATE_B},
  {'5', 0.0f, 2.0f, 0.0f, MGC_CRATE_B}, {'6', 0.0f, 2.4f, 0.0f, MGC_CRATE_B},
  {'c', 0.0f, 1.1f, 0.15f, MGC_CRATE_A}, {'C', 0.0f, 2.2f, 0.15f, MGC_CRATE_A},
  {'=', 0.0f, 1.05f, 0.5f, MGC_COVER},
  {'A', 0.0f, 0.02f, 0.0f, MGC_SITE_A}, {'B', 0.0f, 0.02f, 0.0f, MGC_SITE_B},
};

// Is the grid cell (c, r) open ground — something a facade should face?
static int mg_open(const char *const rows[MAPGRID_N], int c, int r) {
  if (c < 0 || r < 0 || c >= MAPGRID_N || r >= MAPGRID_N) return 0;
  return strchr("#%HhOX", rows[r][c]) == NULL;
}
// Does the world-space side of a rect face open ground anywhere along it?
static int mg_side_open(const char *const rows[MAPGRID_N], float x0, float z0, float x1,
                        float z1, int side) {
  const float cs = 2.0f * ARENA_HALF / MAPGRID_N;
  int c0 = (int)floorf((x0 + ARENA_HALF) / cs + 0.01f), c1 = (int)ceilf((x1 + ARENA_HALF) / cs - 0.01f) - 1;
  int r0 = (int)floorf((z0 + ARENA_HALF) / cs + 0.01f), r1 = (int)ceilf((z1 + ARENA_HALF) / cs - 0.01f) - 1;
  if (side == SIDE_N || side == SIDE_S) {
    int r = side == SIDE_N ? r0 - 1 : r1 + 1;
    for (int c = c0; c <= c1; c++) if (mg_open(rows, c, r)) return 1;
  } else {
    int c = side == SIDE_W ? c0 - 1 : c1 + 1;
    for (int r = r0; r <= r1; r++) if (mg_open(rows, c, r)) return 1;
  }
  return 0;
}

static char mg_cell_ch(const char *const rows[MAPGRID_N], int c, int r) {
  if (c < 0 || r < 0 || c >= MAPGRID_N || r >= MAPGRID_N) return '#';
  return rows[r][c];
}

// A BLOCK OF TOWN: one merged '#' (tall) or '%' (low) rect, cut along its long
// axis into houses 8-13.5 m wide, each with its own height, plaster, windows on
// the sides that face a street and, often, a door on one of them. Thin strips
// (< 5.2 m) become buttressed masonry compound walls instead of thin houses.
static void town_block(const char *const rows[MAPGRID_N], float x0, float z0, float x1,
                       float z1, int tall) {
  if (fminf(x1 - x0, z1 - z0) < 5.2f) {
    float H = 3.8f;
    v3 wall = g_town.plaster[0], stone = g_town.stone, trim = g_town.trim;
    bx(x0, 0.0f, z0, x1, 0.45f, z1, stone);
    bx(x0 + 0.08f, 0.45f, z0 + 0.08f, x1 - 0.08f, H, z1 - 0.08f, wall);
    bx(x0 + 0.02f, H, z0 + 0.02f, x1 - 0.02f, H + 0.25f, z1 - 0.02f, trim);
    return;
  }
  int along_x = (x1 - x0) >= (z1 - z0);
  float a = along_x ? x0 : z0, end = along_x ? x1 : z1;
  while (end - a > 0.1f) {
    float w = rng_range(8.0f, 13.5f);
    if (end - (a + w) < 5.5f) w = end - a;          // no sliver at the end
    float hx0 = along_x ? a : x0, hx1 = along_x ? a + w : x1;
    float hz0 = along_x ? z0 : a, hz1 = along_x ? z1 : a + w;
    int win = 0;
    for (int sd = 1; sd <= 8; sd <<= 1)
      if (mg_side_open(rows, hx0, hz0, hx1, hz1, sd)) win |= sd;
    int floors = tall ? 2 + (int)(rng_u32() % 2) : 1 + (int)(rng_u32() % 2);
    if (fminf(hx1 - hx0, hz1 - hz0) < 6.0f) floors = 1;
    int doors = 0;
    if (win && (rng_u32() % 10) < 7) {
      for (int sd = 1; sd <= 8; sd <<= 1) if (win & sd) { doors = sd; break; }
    }
    house(hx0, hz0, hx1, hz1, floors, win, doors, 0, 0.0f, 0.0f, 1);
    a += w;
  }
}

// A CLIMBABLE HOUSE ('H' tall, 'h' low): one house, its stair on the longest
// side facing a clear street (checked at 1.0 m and 2.2 m outward), as many
// floors as that side has room to climb with a full top landing.
static void town_climb(const char *const rows[MAPGRID_N], float x0, float z0, float x1,
                       float z1, int tall) {
  int win = 0;
  for (int sd = 1; sd <= 8; sd <<= 1)
    if (mg_side_open(rows, x0, z0, x1, z1, sd)) win |= sd;
  int best = 0;
  float best_len = 0.0f, best_a = 0.0f;
  const float cs = 2.0f * ARENA_HALF / MAPGRID_N;
  for (int sd = 1; sd <= 8; sd <<= 1) {
    if (!(win & sd)) continue;
    int axz = sd == SIDE_N || sd == SIDE_S;
    float a0 = axz ? x0 : z0 + PF_SKIN, a1 = axz ? x1 : z1 - PF_SKIN;
    float pl = sd == SIDE_N ? z0 : sd == SIDE_S ? z1 : sd == SIDE_W ? x0 : x1;
    float out = (sd == SIDE_N || sd == SIDE_W) ? -1.0f : 1.0f;
    float run0 = -1.0f;
    for (float a = a0; a < a1 - 0.001f; ) {
      float ce = -ARENA_HALF + (floorf((a + ARENA_HALF) / cs + 1e-4f) + 1.0f) * cs;
      float e = fminf(ce, a1), m = (a + e) * 0.5f;
      int open = (axz ? (mg_point_open(m, pl + out * 1.0f) && mg_point_open(m, pl + out * 2.2f))
                      : (mg_point_open(pl + out * 1.0f, m) && mg_point_open(pl + out * 2.2f, m)));
      if (open && run0 < 0.0f) run0 = a;
      if ((!open || e >= a1 - 0.001f) && run0 >= 0.0f) {
        float re = open ? e : a;
        if (re - run0 > best_len) { best_len = re - run0; best = sd; best_a = run0; }
        run0 = -1.0f;
      }
      a = e;
    }
  }
  int floors = tall ? 3 : 1;
  while (floors > 1) {
    float min_span = ((float)(floors * PF_FLOOR / PF_RISE) - 1.0f) * 0.40f + 3.1f;
    if (best_len >= min_span) break;
    floors--;
  }
  float req = ((float)(floors * PF_FLOOR / PF_RISE) - 1.0f) * 0.40f + 3.1f;
  if (!best || best_len < req) {
    house(x0, z0, x1, z1, floors, win, 0, 0, 0.0f, 0.0f, 1);
    return;
  }
  int other = win & ~best;
  int doors = 0;
  for (int sd = 1; sd <= 8; sd <<= 1) if (other & sd) { doors = sd; break; }
  house_climb(x0, z0, x1, z1, floors, win, doors, best, +1, best_a + 1.4f);
}

static void map_grid(const char *const rows[MAPGRID_N]) {
  const theme_t *t = &g_theme;
  g_mg_rows = rows;
  const float cs = 2.0f * ARENA_HALF / MAPGRID_N;
  static uint8_t used[MAPGRID_N][MAPGRID_N];
  memset(used, 0, sizeof used);
  for (int r = 0; r < MAPGRID_N; r++)
    for (int c = 0; c < MAPGRID_N; c++) {
      char ch = rows[r][c];
      const mapgrid_tile_t *tl = NULL;
      for (size_t k = 0; k < sizeof MAPGRID_TILES / sizeof MAPGRID_TILES[0]; k++)
        if (MAPGRID_TILES[k].ch == ch) tl = &MAPGRID_TILES[k];
      int special = strchr("#%HhOWX", ch) != NULL || (ch == 'C' && (g_town_kind == 1 || g_town_kind == 3));
      if ((!tl && !special) || used[r][c]) continue;
      // Widest run along the row, then as many rows down as stay full.
      int w = 1, h = 1;
      while (c + w < MAPGRID_N && rows[r][c + w] == ch && !used[r][c + w]) w++;
      for (int ok = 1; ok && r + h < MAPGRID_N; ) {
        for (int k = 0; k < w; k++)
          if (rows[r + h][c + k] != ch || used[r + h][c + k]) { ok = 0; break; }
        if (ok) h++;
      }
      for (int y = r; y < r + h; y++)
        for (int x = c; x < c + w; x++) used[y][x] = 1;
      if (special) {
        float sx0 = -ARENA_HALF + (float)c * cs, sz0 = -ARENA_HALF + (float)r * cs;
        float sx1 = -ARENA_HALF + (float)(c + w) * cs, sz1 = -ARENA_HALF + (float)(r + h) * cs;
        switch (ch) {
          case '#': town_block(rows, sx0, sz0, sx1, sz1, 1); break;
          case '%': town_block(rows, sx0, sz0, sx1, sz1, 0); break;
          case 'H': town_climb(rows, sx0, sz0, sx1, sz1, 1); break;
          case 'h': town_climb(rows, sx0, sz0, sx1, sz1, 0); break;
          case 'O': {
            float rr = fminf(sx1 - sx0, sz1 - sz0) * 0.5f - 0.15f;
            tank((sx0 + sx1) * 0.5f, (sz0 + sz1) * 0.5f, rr, rng_range(6.5f, 8.5f),
                 tint(rgb(0.82f, 0.82f, 0.78f), 0.95f, 1.03f));
          } break;
          case 'W':
            water_rect(sx0, sz0, sx1, sz1);
            // a railing on each land edge, run by run along it
            for (int sd = 1; sd <= 8; sd <<= 1) {
              int n_ = (sd == SIDE_N || sd == SIDE_S) ? w : h;
              int run0 = -1;
              for (int k = 0; k <= n_; k++) {
                int open = 0;
                if (k < n_) {
                  int cc = sd == SIDE_W ? c - 1 : sd == SIDE_E ? c + w : c + k;
                  int rr2 = sd == SIDE_N ? r - 1 : sd == SIDE_S ? r + h : r + k;
                  open = mg_open(rows, cc, rr2) && rows[rr2][cc] != 'W';
                }
                if (open && run0 < 0) run0 = k;
                if (!open && run0 >= 0) {
                  float a0 = (sd == SIDE_N || sd == SIDE_S ? sx0 : sz0) + (float)run0 * cs;
                  float a1 = (sd == SIDE_N || sd == SIDE_S ? sx0 : sz0) + (float)k * cs;
                  if (sd == SIDE_E) quay_rail(1, sx1, +1, a0, a1);
                  else if (sd == SIDE_W) quay_rail(1, sx0, -1, a0, a1);
                  else if (sd == SIDE_N) quay_rail(0, sz0, -1, a0, a1);
                  else quay_rail(0, sz1, +1, a0, a1);
                  run0 = -1;
                }
              }
            }
            break;
          case 'C': {   // HARBOR: shipping containers, sometimes two high
            static const v3 CC[5] = {{{0.66f, 0.20f, 0.14f}}, {{0.16f, 0.34f, 0.56f}},
                                     {{0.22f, 0.46f, 0.30f}}, {{0.82f, 0.54f, 0.16f}},
                                     {{0.50f, 0.52f, 0.54f}}};
            int ax = (sx1 - sx0) >= (sz1 - sz0) ? 0 : 1;
            float cx = (sx0 + sx1) * 0.5f, cz = (sz0 + sz1) * 0.5f;
            container(cx, cz, ax, 0.0f, CC[rng_u32() % 5]);
            if (rng_u32() & 1) container(cx, cz, ax, 2.6f, CC[rng_u32() % 5]);
          } break;
          default: break;   // 'X': a landmark goes here by hand
        }
        continue;
      }
      float y1 = tl->y1;
      float x0 = -ARENA_HALF + (float)c * cs + tl->inset;
      float z0 = -ARENA_HALF + (float)r * cs + tl->inset;
      float x1 = -ARENA_HALF + (float)(c + w) * cs - tl->inset;
      float z1 = -ARENA_HALF + (float)(r + h) * cs - tl->inset;
      v3 col;
      switch (tl->col) {
        case MGC_WALL:    col = col_tint(t->wall, 0.86f, 1.0f); break;
        case MGC_COVER:   col = col_tint(t->cover, 0.92f, 1.04f); break;
        case MGC_CRATE_A: col = col_tint(t->crate_a, 0.9f, 1.06f); break;
        case MGC_MID:     col = t->mid; break;
        case MGC_SITE_A:  col = t->band_s; break;
        case MGC_SITE_B:  col = t->band_n; break;
        default:          col = col_tint(t->crate_b, 0.92f, 1.04f); break;
      }
      map_add((v3){{x0, tl->y0, z0}}, (v3){{x1, y1, z1}}, col);
      // Grid stairs ('1'..'6'): add stepped sidebars on lateral edges facing open street.
      if (ch >= '1' && ch <= '6') {
        int step_x = 0, step_z = 0;
        for (int rr2 = r; rr2 < r + h; rr2++)
          if (strchr("123456^", mg_cell_ch(rows, c - 1, rr2)) ||
              strchr("123456^", mg_cell_ch(rows, c + w, rr2))) step_x = 1;
        for (int cc2 = c; cc2 < c + w; cc2++)
          if (strchr("123456^", mg_cell_ch(rows, cc2, r - 1)) ||
              strchr("123456^", mg_cell_ch(rows, cc2, r + h))) step_z = 1;
        // The rails: a curb, posts, a mid rail and a handrail on each lateral
        // edge that drops to open street, one run per stretch of open cells.
        v3 curb = g_town.stone;
        v3 iron = g_town_kind == 2 ? g_town.trim : rgb(0.16f, 0.16f, 0.18f);
        int old_mf = g_mat_force;
        g_mat_force = SMAT_CONCRETE;
        if (step_x && !step_z) {
          for (int sd = 0; sd < 2; sd++) {
            int adj_r = sd == 0 ? r - 1 : r + h;
            float e0 = sd == 0 ? z0 + 0.02f : z1 - 0.16f;
            int run0 = -1;
            for (int k = 0; k <= w; k++) {
              int op = k < w && strchr(".AB", mg_cell_ch(rows, c + k, adj_r)) != NULL;
              if (op && run0 < 0) run0 = k;
              if (!op && run0 >= 0) {
                float u0 = -ARENA_HALF + (float)(c + run0) * cs;
                float u1 = -ARENA_HALF + (float)(c + k) * cs;
                edge_rail(0, e0, u0 + 0.02f, u1 - 0.02f, y1, curb, iron);
                run0 = -1;
              }
            }
          }
        }
        if (step_z && !step_x) {
          for (int sd = 0; sd < 2; sd++) {
            int adj_c = sd == 0 ? c - 1 : c + w;
            float e0 = sd == 0 ? x0 + 0.02f : x1 - 0.16f;
            int run0 = -1;
            for (int k = 0; k <= h; k++) {
              int op = k < h && strchr(".AB", mg_cell_ch(rows, adj_c, r + k)) != NULL;
              if (op && run0 < 0) run0 = k;
              if (!op && run0 >= 0) {
                float u0 = -ARENA_HALF + (float)(r + run0) * cs;
                float u1 = -ARENA_HALF + (float)(r + k) * cs;
                edge_rail(1, e0, u0 + 0.02f, u1 - 0.02f, y1, curb, iron);
                run0 = -1;
              }
            }
          }
        }
        g_mat_force = old_mf;
      }
      // Catwalks ('^'): steel handrails along open edges + corner/mid support legs.
      if (ch == '^') {
        v3 rc = rgb(0.28f, 0.30f, 0.32f), cc = rgb(0.24f, 0.26f, 0.28f);
        int old_mf = g_mat_force;
        g_mat_force = SMAT_STEEL;
        for (int sd = 1; sd <= 8; sd <<= 1) {
          int axz = sd == SIDE_N || sd == SIDE_S;
          int n_ = axz ? w : h;
          int run0 = -1;
          for (int k = 0; k <= n_; k++) {
            int op = 0;
            if (k < n_) {
              int cc2 = sd == SIDE_W ? c - 1 : sd == SIDE_E ? c + w : c + k;
              int rr2 = sd == SIDE_N ? r - 1 : sd == SIDE_S ? r + h : r + k;
              op = strchr("^6#%Hh", mg_cell_ch(rows, cc2, rr2)) == NULL;
            }
            if (op && run0 < 0) run0 = k;
            if (!op && run0 >= 0) {
              float a0 = (axz ? x0 : z0) + (float)run0 * cs + 0.11f;
              float a1 = (axz ? x0 : z0) + (float)k * cs - 0.11f;
              if (a1 - a0 > 0.25f) {
                float edge = sd == SIDE_N ? z0 : sd == SIDE_S ? z1 : sd == SIDE_W ? x0 : x1;
                float sgn = (sd == SIDE_N || sd == SIDE_W) ? 1.0f : -1.0f;
                float d0 = edge + sgn * 0.03f, d1 = edge + sgn * 0.09f;
                float p0 = edge + sgn * 0.035f, p1 = edge + sgn * 0.085f;
                if (axz) {
                  bx(a0, 3.22f, d0, a1, 3.28f, d1, rc);
                  bx(a0, 3.58f, d0, a1, 3.66f, d1, rc);
                } else {
                  bx(d0, 3.22f, a0, d1, 3.28f, a1, rc);
                  bx(d0, 3.58f, a0, d1, 3.66f, a1, rc);
                }
                float last_p = -1e9f;
                for (float p = a0 + 0.04f; p + 0.07f <= a1 - 0.04f; p += 1.60f) {
                  last_p = p;
                  if (axz) bx(p, y1, p0, p + 0.07f, 3.58f, p1, cc);
                  else     bx(p0, y1, p, p1, 3.58f, p + 0.07f, cc);
                }
                if (a1 - 0.11f - last_p > 0.35f) {
                  float p = a1 - 0.11f;
                  if (axz) bx(p, y1, p0, p + 0.07f, 3.58f, p1, cc);
                  else     bx(p0, y1, p, p1, 3.58f, p + 0.07f, cc);
                }
              }
              run0 = -1;
            }
          }
        }
        // Grounded corner legs where neither corner neighbour is a wall/stair6
        for (int iy = 0; iy < 2; iy++)
          for (int ix = 0; ix < 2; ix++) {
            int cc2 = ix ? c + w - 1 : c, rr2 = iy ? r + h - 1 : r;
            int dx = ix ? 1 : -1, dz = iy ? 1 : -1;
            if (strchr("#%Hh6", mg_cell_ch(rows, cc2 + dx, rr2)) != NULL ||
                strchr("#%Hh6", mg_cell_ch(rows, cc2, rr2 + dz)) != NULL) continue;
            float lx = ix ? x1 - 0.40f : x0 + 0.14f, lz = iy ? z1 - 0.40f : z0 + 0.14f;
            bx(lx, 0.0f, lz, lx + 0.26f, tl->y0, lz + 0.26f, cc);
          }
        if (x1 - x0 > 9.0f) {
          for (float mx = x0 + 8.0f; mx <= x1 - 4.0f; mx += 8.0f) {
            int mc = (int)floorf((mx + ARENA_HALF) / cs);
            if (strchr(".AB", mg_cell_ch(rows, mc, r - 1)) != NULL)
              bx(mx - 0.13f, 0.0f, z0 + 0.14f, mx + 0.13f, tl->y0, z0 + 0.40f, cc);
            if (strchr(".AB", mg_cell_ch(rows, mc, r + h)) != NULL)
              bx(mx - 0.13f, 0.0f, z1 - 0.40f, mx + 0.13f, tl->y0, z1 - 0.14f, cc);
          }
        }
        g_mat_force = old_mf;
      }
      // Canopy roofs ('r'): grounded support columns under exposed corners & spans.
      if (ch == 'r') {
        v3 pc = rgb(0.28f, 0.30f, 0.32f);
        int old_mf = g_mat_force;
        g_mat_force = SMAT_STEEL;
        for (int iy = 0; iy < 2; iy++)
          for (int ix = 0; ix < 2; ix++) {
            int cc2 = ix ? c + w - 1 : c, rr2 = iy ? r + h - 1 : r;
            int dx = ix ? 1 : -1, dz = iy ? 1 : -1;
            if (strchr("#%Hhw", mg_cell_ch(rows, cc2 + dx, rr2)) != NULL ||
                strchr("#%Hhw", mg_cell_ch(rows, cc2, rr2 + dz)) != NULL) continue;
            float px = ix ? x1 - 0.46f : x0 + 0.18f, pz = iy ? z1 - 0.46f : z0 + 0.18f;
            bx(px, 0.0f, pz, px + 0.28f, tl->y0, pz + 0.28f, pc);
          }
        if (x1 - x0 > 9.0f) {
          for (float mx = x0 + 8.0f; mx <= x1 - 4.0f; mx += 8.0f) {
            int mc = (int)floorf((mx + ARENA_HALF) / cs);
            if (strchr(".AB", mg_cell_ch(rows, mc, r - 1)) != NULL)
              bx(mx - 0.14f, 0.0f, z0 + 0.18f, mx + 0.14f, tl->y0, z0 + 0.46f, pc);
            if (strchr(".AB", mg_cell_ch(rows, mc, r + h)) != NULL)
              bx(mx - 0.14f, 0.0f, z1 - 0.46f, mx + 0.14f, tl->y0, z1 - 0.18f, pc);
          }
        }
        if (z1 - z0 > 9.0f) {
          for (float mz = z0 + 8.0f; mz <= z1 - 4.0f; mz += 8.0f) {
            int mr = (int)floorf((mz + ARENA_HALF) / cs);
            if (strchr(".AB", mg_cell_ch(rows, c - 1, mr)) != NULL)
              bx(x0 + 0.18f, 0.0f, mz - 0.14f, x0 + 0.46f, tl->y0, mz + 0.14f, pc);
            if (strchr(".AB", mg_cell_ch(rows, c + w, mr)) != NULL)
              bx(x1 - 0.46f, 0.0f, mz - 0.14f, x1 - 0.18f, tl->y0, mz + 0.14f, pc);
          }
        }
        g_mat_force = old_mf;
      }
    }
}
// THE LANDMARKS, by hand, on the cells the grids reserve ('X') or leave open.
// STREET SPANS — what makes a narrow street read as a TOWN: in the old towns a
// room carried across the street on stone corbels (a sabat), on the waterfront
// and in the plant a pipe bridge between the sheds. Found on the grid: a run
// of 1-3 open cells with a building on both sides, the street and both
// buildings running on past it. Everything is above 2.9 m, so it changes no
// walk and no jump; a span is built only where both walls stand tall enough
// to carry it, never near a ladder, and only through the facades' own small
// relief (sills, shutters, balcony rails), which it removes where it passes —
// anything bigger in the way and that street gets no span. No g_rng: the
// choice is a hash of the cell, so the rest of the map is unchanged.
static void solid_remove(int i) {
  for (int k = i; k + 1 < g_num_solids; k++) {
    g_solids[k] = g_solids[k + 1];
    g_solid_hide[k] = g_solid_hide[k + 1];
    g_solid_mat[k] = g_solid_mat[k + 1];
  }
  g_num_solids--;
}
// Clear a span's volume: 1 if only small relief was in it (now removed).
static int span_clear(v3 mn, v3 mx) {
  for (int pass = 0; pass < 2; pass++)
    for (int i = g_num_solids - 1; i >= 0; i--) {
      const solid_t *b = &g_solids[i];
      if (!(mn.x < b->max.x && mx.x > b->min.x && mn.y < b->max.y && mx.y > b->min.y &&
            mn.z < b->max.z && mx.z > b->min.z)) continue;
      v3 d = v3_sub(b->max, b->min);
      int relief = !g_solid_hide[i] && b->min.y > 2.2f && d.x * d.y * d.z < 1.6f &&
                   fminf(d.x, d.z) < 1.25f;
      if (!relief) return 0;
      if (pass == 1) solid_remove(i);
    }
  return 1;
}
static int span_wall_tall(float x, float z, float y) { return wg_inside((v3){{x, y, z}}, NULL); }
static void street_spans(const char *const rows[MAPGRID_N]) {
  const float cs = 2.0f * ARENA_HALF / MAPGRID_N;
  int room = g_town_kind == 0 || g_town_kind == 3;
  static int last[2][MAPGRID_N];
  for (int i = 0; i < MAPGRID_N; i++) last[0][i] = last[1][i] = -100;
  const char *bld = "#%Hh";
  for (int pass = 0; pass < 2; pass++)          // 0: span along x, 1: along z
    for (int r = 2; r < MAPGRID_N - 2; r++)
      for (int c = 1; c < MAPGRID_N - 1; c++) {
#define CH(u, v) (pass == 0 ? mg_cell_ch(rows, (u), (v)) : mg_cell_ch(rows, (v), (u)))
        // u runs ACROSS the street, v along it
        int u0 = c, v = r;
        if (!strchr(bld, CH(u0 - 1, v)) || !strchr(".AB", CH(u0, v))) continue;
        int u1 = u0;
        while (u1 - u0 < 4 && strchr(".AB", CH(u1 + 1, v))) u1++;
        if (u1 + 1 >= MAPGRID_N || !strchr(bld, CH(u1 + 1, v))) continue;
        int ok = 1;
        for (int dv = -1; dv <= 1 && ok; dv++) {
          if (!strchr(bld, CH(u0 - 1, v + dv)) || !strchr(bld, CH(u1 + 1, v + dv))) ok = 0;
          for (int u = u0; u <= u1 && ok; u++) if (!strchr(".AB", CH(u, v + dv))) ok = 0;
        }
        if (!ok || r - last[pass][u0] < 7) continue;
        float ua = -ARENA_HALF + (float)u0 * cs, ub = -ARENA_HALF + (float)(u1 + 1) * cs;
        float vc = -ARENA_HALF + ((float)v + 0.5f) * cs;
        float hsh = pass == 0 ? wg_hash(ua, vc, 41u) : wg_hash(vc, ua, 43u);
        if (hsh > (room ? 0.55f : 0.45f)) continue;
        for (int l = 0; l < g_nladders; l++) {
          float lu = pass == 0 ? g_ladders[l].x : g_ladders[l].z;
          float lv = pass == 0 ? g_ladders[l].z : g_ladders[l].x;
          if (lu > ua - 1.5f && lu < ub + 1.5f && fabsf(lv - vc) < 4.0f) ok = 0;
        }
        if (!ok) continue;
        // Both walls tall enough, probed 0.15 m inside each face at both edges
        // of the span's depth. Tall houses carry a room; a pair of compound
        // walls only an arch (or, in the plants, a low pipe bridge).
#define WT(u, vv, y) (pass == 0 ? span_wall_tall((u), (vv), (y)) : span_wall_tall((vv), (u), (y)))
#define WALLS(y, hd_) (WT(ua - 0.15f, vc - (hd_), y) && WT(ua - 0.15f, vc + (hd_), y) && \
                       WT(ub + 0.15f, vc - (hd_), y) && WT(ub + 0.15f, vc + (hd_), y))
        int kind = -1;          // 0 room, 1 arch, 2 high pipes, 3 low pipes
        if (room) kind = WALLS(6.05f, 1.25f) ? 0 : WALLS(3.86f, 0.55f) ? 1 : -1;
        else      kind = WALLS(5.00f, 0.80f) ? 2 : WALLS(3.95f, 0.80f) ? 3 : -1;
#undef WALLS
#undef WT
        if (kind < 0) continue;
#define SB(a0, a1, y0, y1, b0, b1, col) \
  (pass == 0 ? bx(a0, y0, b0, a1, y1, b1, col) : bx(b0, y0, a0, b1, y1, a1, col))
        v3 mn, mx;
        float hd = kind == 0 ? 1.25f : kind == 1 ? 0.55f : 0.80f;
        float ytop = kind == 0 ? 5.84f : kind == 1 ? 4.08f : kind == 2 ? 4.86f : 3.94f;
        float ybot = kind <= 1 ? 2.93f : kind == 2 ? 4.16f : 3.24f;
        if (pass == 0) { mn = (v3){{ua + 0.001f, ybot, vc - hd}}; mx = (v3){{ub - 0.001f, ytop, vc + hd}}; }
        else           { mn = (v3){{vc - hd, ybot, ua + 0.001f}}; mx = (v3){{vc + hd, ytop, ub - 0.001f}}; }
        if (!span_clear(mn, mx)) continue;
        int n0 = g_num_solids;
        (void)n0;
        last[pass][u0] = r;
        const town_style_t *ts = &g_town;
        float L = ub - ua, m = (ua + ub) * 0.5f;
        if (kind == 0) {
          v3 wall = tint(ts->plaster[(int)(hsh * 40.0f) & 3], 0.92f, 1.0f);
          SB(ua, ua + 0.42f, 2.93f, 3.43f, vc - 1.02f, vc + 1.02f, ts->stone);      // corbels
          SB(ub - 0.42f, ub, 2.93f, 3.43f, vc - 1.02f, vc + 1.02f, ts->stone);
          SB(ua, ub, 3.43f, 3.71f, vc - 1.24f, vc + 1.24f, ts->stone);              // the floor beam
          SB(ua, ub, 3.71f, 5.63f, vc - 1.09f, vc + 1.09f, wall);                   // the room
          SB(ua, ub, 5.63f, 5.83f, vc - 1.23f, vc + 1.23f, ts->trim);               // roof slab
          int nw = L > 5.0f ? 2 : 1;
          for (int k = 0; k < nw; k++) {                                          // windows
            float wc = nw == 1 ? m : ua + L * (0.28f + 0.44f * (float)k);
            SB(wc - 0.42f, wc + 0.42f, 4.21f, 5.17f, vc - 1.11f, vc + 1.11f, ts->glass);
            SB(wc - 0.50f, wc + 0.50f, 4.13f, 4.21f, vc - 1.16f, vc + 1.16f, ts->trim);
            if (ts->shutters) {
              v3 sc = ts->shutter[(int)(hsh * 97.0f) % 3];
              SB(wc - 0.86f, wc - 0.46f, 4.21f, 5.17f, vc - 1.13f, vc + 1.13f, sc);
              SB(wc + 0.46f, wc + 0.86f, 4.21f, 5.17f, vc - 1.13f, vc + 1.13f, sc);
            }
          }
        } else if (kind == 1) {
          // THE ARCH: springers on each wall, a stepped soffit that reads as
          // the curve, the beam, a coping and a keystone over the middle.
          v3 st = tint(ts->stone, 0.96f, 1.02f);
          float sp = fminf(0.62f, L * 0.16f);
          SB(ua, ua + sp, 2.93f, 3.33f, vc - 0.45f, vc + 0.45f, st);
          SB(ub - sp, ub, 2.93f, 3.33f, vc - 0.45f, vc + 0.45f, st);
          SB(ua, ua + sp * 2.0f, 3.33f, 3.58f, vc - 0.45f, vc + 0.45f, st);
          SB(ub - sp * 2.0f, ub, 3.33f, 3.58f, vc - 0.45f, vc + 0.45f, st);
          SB(ua, ub, 3.58f, 3.96f, vc - 0.45f, vc + 0.45f, st);
          SB(ua, ub, 3.96f, 4.07f, vc - 0.53f, vc + 0.53f, ts->trim);
          SB(m - 0.21f, m + 0.21f, 3.50f, 3.58f, vc - 0.49f, vc + 0.49f, ts->trim);
        } else {
          v3 stl = rgb(0.28f, 0.30f, 0.32f);
          static const v3 PC[4] = {{{0.62f, 0.64f, 0.66f}}, {{0.52f, 0.26f, 0.18f}},
                                   {{0.26f, 0.46f, 0.30f}}, {{0.80f, 0.62f, 0.14f}}};
          float o = kind == 2 ? 0.0f : -0.92f;     // the low bridge, under the wall tops
          int mf = g_mat_force;
          g_mat_force = SMAT_STEEL;
          SB(ua, ua + 0.06f, 4.18f + o, 4.52f + o, vc - 0.78f, vc + 0.78f, stl);   // wall plates
          SB(ub - 0.06f, ub, 4.18f + o, 4.52f + o, vc - 0.78f, vc + 0.78f, stl);
          SB(ua + 0.06f, ub - 0.06f, 4.40f + o, 4.54f + o, vc - 0.70f, vc - 0.56f, stl);
          SB(ua + 0.06f, ub - 0.06f, 4.40f + o, 4.54f + o, vc + 0.56f, vc + 0.70f, stl);
          int pc = (int)(hsh * 53.0f);
          SB(ua, ub, 4.54f + o, 4.82f + o, vc - 0.52f, vc - 0.24f, PC[pc & 3]);    // pipes
          SB(ua, ub, 4.54f + o, 4.74f + o, vc - 0.14f, vc + 0.06f, PC[(pc + 1) & 3]);
          SB(ua, ub, 4.54f + o, 4.85f + o, vc + 0.16f, vc + 0.47f, PC[(pc + 2) & 3]);
          g_mat_force = mf;
        }
#undef SB
#undef CH
      }
}

// STREET LIFE ALONG THE WALLS: the yard and street kit set back against a
// building, the way things collect in a lived-in place — sacks and benches in
// the old towns, IBC totes and pallets in the docks and the plant, a power
// cabinet anywhere. A spot is a plain floor cell with a house behind it and at
// least two more open cells in front (an alley stays an alley), never in front
// of a door or a window, never near a ladder, the whole footprint and a margin
// round it free, and 7 m from the last one. Hash-picked, no g_rng.
static void env_scatter(const char *const rows[MAPGRID_N], int max_n) {
  const float cs = 2.0f * ARENA_HALF / MAPGRID_N;
  static const int DC[4] = {-1, 0, 1, 0}, DR[4] = {0, -1, 0, 1};
  float px[64], pz[64];
  int n = 0, old = g_town_kind == 0 || g_town_kind == 3;
  for (int r = 1; r < MAPGRID_N - 1 && n < max_n; r++)
    for (int c = 1; c < MAPGRID_N - 1 && n < max_n; c++) {
      if (rows[r][c] != '.') continue;
      for (int d = 0; d < 4 && n < max_n; d++) {
        if (!strchr("#%", mg_cell_ch(rows, c + DC[d], r + DR[d]))) continue;
        if (!strchr(".AB", mg_cell_ch(rows, c - DC[d], r - DR[d])) ||
            !strchr(".AB", mg_cell_ch(rows, c - 2 * DC[d], r - 2 * DR[d]))) continue;
        float h = wg_hash((float)(c * 4 + d), (float)r, 61u);
        if (h > 0.22f) continue;
        // the wall plane and the outward normal (away from the wall)
        float cx = -ARENA_HALF + ((float)c + 0.5f) * cs, cz = -ARENA_HALF + ((float)r + 0.5f) * cs;
        float nx = (float)-DC[d], nz = (float)-DR[d];
        float wx = cx - nx * cs * 0.5f, wz = cz - nz * cs * 0.5f;    // on the wall plane
        float tx = -nz, tz = nx;
        float pick = wg_hash(cx, cz, 62u);
        int kind = old ? (pick < 0.36f ? 1 : pick < 0.70f ? 2 : 3) : (pick < 0.45f ? 0 : pick < 0.70f ? 1 : 3);
        static const float DEPTH[4] = {1.20f, 1.20f, 0.64f, 0.56f}, HALFW[4] = {0.50f, 0.50f, 0.82f, 0.66f};
        static const float HT[4] = {1.16f, 0.80f, 0.90f, 1.53f};
        float back = kind == 2 ? 0.10f : 0.06f;
        float ox = wx + nx * (back + DEPTH[kind] * 0.5f), oz = wz + nz * (back + DEPTH[kind] * 0.5f);
        int ok = 1;
        for (int k = 0; k < n && ok; k++)
          if (fabsf(px[k] - ox) + fabsf(pz[k] - oz) < 6.0f) ok = 0;
        for (int l = 0; l < g_nladders && ok; l++)
          if (fabsf(g_ladders[l].x - ox) + fabsf(g_ladders[l].z - oz) < 3.0f) ok = 0;
        if (!ok) continue;
        // footprint + margin clear (back edge stops short of the wall)
        float a0 = back - 0.02f, a1 = back + DEPTH[kind] + 0.35f, b = HALFW[kind] + 0.35f;
        v3 mn = {{fminf(wx + nx * a0 - tx * b, wx + nx * a1 + tx * b), 0.03f,
                  fminf(wz + nz * a0 - tz * b, wz + nz * a1 + tz * b)}};
        v3 mx = {{fmaxf(wx + nx * a0 - tx * b, wx + nx * a1 + tx * b), HT[kind] + 0.40f,
                  fmaxf(wz + nz * a0 - tz * b, wz + nz * a1 + tz * b)}};
        if (!wg_box_free(mn, mx)) continue;
        // bare wall behind it: no door, no window, and a wall at all
        for (int q = -1; q <= 1 && ok; q++)
          for (int yy = 0; yy < 2 && ok; yy++) {
            v3 wc;
            v3 pp = {{wx - nx * 0.05f + tx * (HALFW[kind] + 0.2f) * (float)q, 0.5f + 0.9f * (float)yy,
                      wz - nz * 0.05f + tz * (HALFW[kind] + 0.2f) * (float)q}};
            if (!wg_inside(pp, &wc) || wg_same(wc, g_town.door) || wg_same(wc, g_town.glass)) ok = 0;
          }
        if (!ok) continue;
        int qt = nx > 0.5f ? 0 : nz > 0.5f ? 1 : nx < -0.5f ? 2 : 3;
        float t2 = wg_hash(ox, oz, 63u);
        switch (kind) {
          case 0: ibc_tote(ox, oz, qt, rgb(0.86f, 0.86f, 0.82f),
                           t2 < 0.5f ? rgb(0.15f, 0.30f, 0.55f) : rgb(0.30f, 0.30f, 0.32f)); break;
          case 1: sack_pallet(ox, oz, qt, t2 < 0.33f ? rgb(0.62f, 0.55f, 0.40f)
                                         : t2 < 0.66f ? rgb(0.70f, 0.70f, 0.68f) : rgb(0.85f, 0.84f, 0.80f)); break;
          case 2: bench_prop(ox, oz, qt, t2 < 0.5f ? rgb(0.46f, 0.30f, 0.17f) : rgb(0.20f, 0.36f, 0.26f)); break;
          default: elec_cabinet(ox, oz, qt, t2 < 0.5f ? rgb(0.55f, 0.58f, 0.55f) : rgb(0.30f, 0.40f, 0.32f)); break;
        }
        px[n] = ox; pz[n] = oz; n++;
      }
    }
}

// SUPPLY STATION PLACEMENT (see supply_t, world/map.c). Run after a handmade
// map's build, from the solids alone. A spot is good when the station's back is
// against something solid (a wall, a container, cover) and its footprint plus a
// margin, the reach in front of it and the head room are all clear, it is on
// open floor (no water), away from the spawn zones and the ladders. Picked
// greedily for SPREAD (far from every station already placed) with a pull to
// the middle of the map, alternating health and grenades. The symmetric small
// maps pick in one half and mirror, so each side has the same supplies.
static const pbox_t SUP_BOX[2] = {{-0.30f, 0.30f, -0.46f, 0.46f, 0.0f, 1.70f},    // medical cabinet
                                  {-0.36f, 0.36f, -0.64f, 0.64f, 0.0f, 0.82f}};   // ammunition crate
static const char *const *sup_rows(void) {
  switch (g_map_id) {
    case 4: return MAP_CITADEL; case 5: return MAP_HARBOR;
    case 6: return MAP_REFINERY; case 7: return MAP_COMPOUND;
    default: return NULL;
  }
}
// A point inside VISIBLE architecture (a wall, a pier, a parapet) — not inside a
// prop's hidden block-out (only props hide their solids).
static int sup_wall_at(v3 p) {
  for (int i = 0; i < g_num_solids; i++) {
    if (g_solid_hide[i]) continue;
    const solid_t *sd = &g_solids[i];
    if (p.x > sd->min.x && p.x < sd->max.x && p.y > sd->min.y && p.y < sd->max.y &&
        p.z > sd->min.z && p.z < sd->max.z) return 1;
  }
  return 0;
}
// BACK TO A WALL. A station on the 1 m search grid at (x, z) facing qt looks up
// to SUP_REACH behind its back for a wall that spans its whole width (both
// edges and the middle, at two heights) and, when it finds one, steps back
// against it: *x, *z come back moved. 0 when there is no such wall.
// It used to accept ANY solid 0.25 m behind the back, and on a grid whose
// points rarely sit that close to a facade the something was usually a prop:
// a generator, an electrical box, a lamp post. The cabinet then stood flush
// against it and, from the front, the two read as one object grown into the
// other. A prop is a hidden block-out, so it no longer counts as a wall, and a
// post is narrower than the station, so neither does that.
#define SUP_REACH 0.80f
static int sup_snap_wall(float *x, float *z, int kind, int qt) {
  const pbox_t *b = &SUP_BOX[kind];
  float fx = qt == 0 ? 1.0f : qt == 2 ? -1.0f : 0.0f, fz = qt == 1 ? 1.0f : qt == 3 ? -1.0f : 0.0f;
  float lx = -fz, lz = fx;                       // the station's own left
  for (float d = 0.02f; d <= SUP_REACH + 1e-4f; d += 0.02f) {
    float bk = -b->a0 + d;
    int all = 1;
    for (int k = -1; k <= 1 && all; k++) {
      float w = k < 0 ? b->b0 + 0.04f : k > 0 ? b->b1 - 0.04f : 0.0f;
      float px = *x - fx * bk + lx * w, pz = *z - fz * bk + lz * w;
      all = sup_wall_at((v3){{px, 0.6f, pz}}) && sup_wall_at((v3){{px, 1.2f, pz}});
    }
    if (all) {
      float mv = fmaxf(d - 0.03f, 0.0f);
      *x -= fx * mv; *z -= fz * mv;
      return 1;
    }
  }
  return 0;
}
static int sup_spot_ok(float *px_, float *pz_, int kind, int qt) {
  float x = *px_, z = *pz_;
  const char *const *rows = sup_rows();
  const float H = ARENA_HALF;
  if (fabsf(x) > H - 2.0f || fabsf(z) > H - 2.0f) return 0;
  if (rows) {   // open floor, and nowhere near a spawn zone
    const float cs = 2.0f * H / MAPGRID_N;
    int c = (int)floorf((x + H) / cs), r = (int)floorf((z + H) / cs);
    if (!strchr(".ABr^", mg_cell_ch(rows, c, r))) return 0;   // open floor (roofed counts)
    for (int dr = -3; dr <= 3; dr++)
      for (int dc = -3; dc <= 3; dc++)
        if (strchr("TK", mg_cell_ch(rows, c + dc, r + dr))) return 0;
  } else {
    float d1 = hypotf(x - SPAWN_POS.x, z - SPAWN_POS.z), d2 = hypotf(x + SPAWN_POS.x, z + SPAWN_POS.z);
    if (fminf(d1, d2) < 6.0f) return 0;
  }
  for (int l = 0; l < g_nladders; l++)
    if (fabsf(g_ladders[l].x - x) + fabsf(g_ladders[l].z - z) < 3.0f) return 0;
  const pbox_t *b = &SUP_BOX[kind];
  if (!sup_snap_wall(&x, &z, kind, qt)) return 0;
  // the footprint with a margin at the sides and a reach in front, clear to 2.3 m
  pbox_t clr = {b->a0 + 0.04f, b->a1 + 1.30f, b->b0 - 0.45f, b->b1 + 0.45f, 0.03f, 2.30f};
  float x0, z0, x1, z1;
  pbox_world(&clr, x, z, qt, &x0, &z0, &x1, &z1);
  if (!wg_box_free((v3){{x0, clr.y0, z0}}, (v3){{x1, clr.y1, z1}})) return 0;
  // ...and A BREATH OF ROOM from every prop: no prop's block-out within 0.45 m
  // of the footprint, beside it, in front or behind, up to its own height.
  pbox_t room = {b->a0 - 0.45f, b->a1 + 0.45f, b->b0 - 0.45f, b->b1 + 0.45f, 0.03f, b->y1 + 0.30f};
  pbox_world(&room, x, z, qt, &x0, &z0, &x1, &z1);
  for (int i = 0; i < g_num_solids; i++) {
    if (!g_solid_hide[i]) continue;
    const solid_t *sd = &g_solids[i];
    if (sd->max.y - sd->min.y < 0.05f) continue;       // floor films and pads
    if (x0 < sd->max.x && x1 > sd->min.x && room.y0 < sd->max.y && room.y1 > sd->min.y &&
        z0 < sd->max.z && z1 > sd->min.z) return 0;
  }
  *px_ = x; *pz_ = z;
  return 1;
}
static void sup_add(float x, float z, int kind, int qt) {
  if (g_nsupply >= MAX_SUPPLY) return;
  int first = g_num_solids;
  const pbox_t *b = &SUP_BOX[kind];
  float x0, z0, x1, z1;
  pbox_world(b, x, z, qt, &x0, &z0, &x1, &z1);
  int mf = g_mat_force;
  g_mat_force = SMAT_STEEL;
  bx(x0, b->y0, z0, x1, b->y1, z1, kind == SUP_HEALTH ? rgb(0.86f, 0.87f, 0.85f) : rgb(0.30f, 0.34f, 0.22f));
  g_mat_force = mf;
  solids_hide(first);
  g_supply[g_nsupply] = (supply_t){x, z, kind, qt};
  g_supply_ready[g_nsupply] = 0;
  g_nsupply++;
}
static void supply_place(void) {
  const float H = ARENA_HALF;
  int sym = sup_rows() == NULL;              // the 48 m maps are 180-degree symmetric
  int per_kind = sym ? 1 : 3;                // per half on the small maps
  enum { NC = 7200 };   // 1600 filled up on kind 0 alone past ~60 m
  static float cx[NC], cz[NC], cmx[NC], cmz[NC];
  static int cq[NC], ck[NC];
  int nc = 0;
  for (int kind = 0; kind < 2; kind++)
    for (float z = -H + 2.0f; z <= H - 2.0f && nc < NC; z += 1.0f)
      for (float x = -H + 2.0f; x <= H - 2.0f && nc < NC; x += 1.0f) {
        if (sym && z < 1.0f) continue;       // one half; the mirror is checked with it
        for (int qt = 0; qt < 4 && nc < NC; qt++) {
          float sx = x, sz = z, mx_ = -x, mz_ = -z;
          if (!sup_spot_ok(&sx, &sz, kind, qt)) continue;
          if (sym && !sup_spot_ok(&mx_, &mz_, kind, (qt + 2) & 3)) continue;
          cx[nc] = sx; cz[nc] = sz; cmx[nc] = mx_; cmz[nc] = mz_; cq[nc] = qt; ck[nc] = kind; nc++;
          break;
        }
      }
  int want[2] = {per_kind, per_kind};
  for (int round = 0; round < 2 * per_kind; round++) {
    int kind = round & 1, best = -1;
    if (!want[kind]) continue;
    float bs = -1e9f;
    for (int i = 0; i < nc; i++) {
      if (ck[i] != kind) continue;
      float dmin = 1e9f;
      for (int j = 0; j < g_nsupply; j++) {
        float d = hypotf(cx[i] - g_supply[j].x, cz[i] - g_supply[j].z);
        if (g_supply[j].kind == kind) d *= 0.6f;   // like kinds spread further
        dmin = fminf(dmin, d);
      }
      if (sym) dmin = fminf(dmin, hypotf(cx[i] * 2.0f, cz[i] * 2.0f));   // vs its own mirror
      float raw = 1e9f;                                  // hard spacing, whatever the kind
      for (int j = 0; j < g_nsupply; j++)
        raw = fminf(raw, hypotf(cx[i] - g_supply[j].x, cz[i] - g_supply[j].z));
      if (sym) raw = fminf(raw, hypotf(cx[i] * 2.0f, cz[i] * 2.0f));
      if (raw < (sym ? 10.0f : 16.0f)) continue;
      float sc = fminf(dmin, H) - 0.35f * hypotf(cx[i], cz[i]);
      if (sc > bs) { bs = sc; best = i; }
    }
    if (best < 0) continue;
    // the spot may have been taken by a station placed since it was listed
    float bx_ = cx[best], bz_ = cz[best], bmx = cmx[best], bmz = cmz[best];
    if (!sup_spot_ok(&bx_, &bz_, kind, cq[best]) ||
        (sym && !sup_spot_ok(&bmx, &bmz, kind, (cq[best] + 2) & 3))) { ck[best] = -1; round--; continue; }
    sup_add(bx_, bz_, kind, cq[best]);
    if (sym) sup_add(bmx, bmz, kind, (cq[best] + 2) & 3);
    want[kind]--;
    ck[best] = -1;
  }
}

static void citadel_extras(void) {
  const town_style_t *s = &g_town;
  // The mosque: a hall with a drum and dome, and a minaret you climb.
  int bal = g_town.balconies;
  g_town.balconies = 0;
  house(12.0f, -16.0f, 24.0f, 6.0f, 2, SIDE_N | SIDE_S | SIDE_E | SIDE_W, SIDE_W, 0, 0.0f, 0.0f, 0);
  g_town.balconies = bal;
  octa(18.0f, -5.0f, 4.6f, 6.4f, 7.6f, s->plaster[3]);
  dome(18.0f, -5.0f, 4.3f, 7.6f, rgb(0.24f, 0.46f, 0.52f));
  tower(8.6f, -12.0f, 1.8f, 10.4f, s->plaster[3], s->stone, 1, -1);
  octa(9.0f, 1.0f, 1.0f, 0.0f, 0.9f, s->stone);                     // well
  street_prop(PM_WELL, 9.0f, 1.0f, 0, s->stone, s->stone, NULL, 0); // ...and its head
  palm(7.6f, 4.6f, 5.2f);
  // Arched gates at mid doors and long A doors.
  arch_gate(0, -23.0f, -2.0f, 2.0f, -6.0f, 6.0f, 6.0f, s->plaster[0]);
  arch_gate(0, 7.0f, -42.0f, -38.0f, -46.0f, -36.0f, 6.0f, s->plaster[2]);
  // Palms, market stalls, carts, lamps, barrels.
  palm(-22.0f, -16.0f, 5.6f); palm(-12.0f, 44.0f, 5.0f); palm(12.0f, 44.0f, 5.4f);
  palm(-4.0f, -38.0f, 5.0f);
  stall1(4.6f, 12.0f, 1, s->awning[0]); stall1(4.6f, 22.0f, 1, s->awning[1]);
  car(-30.0f, -44.5f, 0); pickup_truck(34.0f, -44.0f, 0, 1); car(-30.0f, 32.0f, 0);
  for (float z = -10.0f; z <= 26.0f; z += 12.0f) lamp(5.6f, z + 1.0f, 0, -1);
  barrel(-38.0f, -22.35f, s->door); barrel(-37.2f, -22.75f, s->door);
  barrel(20.0f, -34.0f, s->door);
  // STREET LIFE (no g_rng below: the map's layout stays the seed's). A second
  // pickup in the north square, bins against the walls, motorbikes, a workshop's
  // tyres, and a street cafe across from the stalls.
  pickup_truck(22.0f, -41.0f, 0, 0);
  dumpster(-12.0f, -49.4f, 0, rgb(0.22f, 0.36f, 0.24f));
  dumpster(-31.0f, 37.4f, 0, rgb(0.20f, 0.30f, 0.46f));
  dumpster(45.4f, -30.0f, 1, rgb(0.22f, 0.36f, 0.24f));
  motorbike(-45.2f, -12.0f, 1, rgb(0.62f, 0.10f, 0.08f));
  motorbike(15.0f, 41.5f, 0, rgb(0.14f, 0.24f, 0.46f));
  tyre_stack(-45.3f, 1.0f, 0);
  tyre_stack(40.6f, -46.5f, 1);
  cafe_set(-2.6f, 13.5f, 0);
  cafe_set(-4.25f, 22.5f, 3);
  genset(-37.8f, -18.5f, 1, rgb(0.76f, 0.58f, 0.22f), rgb(0.24f, 0.25f, 0.27f));
  street_spans(g_mg_rows);
  env_scatter(g_mg_rows, 14);
  roof_tanks(12);
  wall_units(0.26f, 0.14f);
}

static void harbor_extras(void) {
  const town_style_t *s = &g_town;
  // The lighthouse in the dock-side pocket.
  tower(-37.0f, 12.0f, 2.0f, 12.0f, rgb(0.90f, 0.90f, 0.88f), s->stone, 1, -1);
  // The gantry crane: a stair tower at the east end, a walkway beam at 10.4 m
  // across the container yard, a leg frame at the west end, a cab and a hook.
  v3 yel = rgb(0.86f, 0.64f, 0.14f), stl = rgb(0.28f, 0.30f, 0.32f);
  // (8.0 m, so the stair finishes on the north side and the west face, where
  // the beam meets the tower, has no flight under it.)
  tower(-13.5f, -33.0f, 1.8f, 8.0f, yel, stl, 0, 3);
  bx(-45.0f, 7.6f, -33.8f, -15.3f, 8.0f, -32.2f, yel);
  bx(-45.0f, 8.0f, -33.8f, -15.3f, 9.0f, -33.72f, stl);
  bx(-45.0f, 8.0f, -32.28f, -15.3f, 9.0f, -32.2f, stl);
  bx(-45.0f, 8.0f, -33.72f, -44.92f, 9.0f, -32.28f, stl);
  bx(-44.8f, 0.0f, -33.8f, -44.0f, 7.6f, -33.2f, yel);
  bx(-44.8f, 0.0f, -32.8f, -44.0f, 7.6f, -32.2f, yel);
  bx(-44.8f, 4.0f, -33.2f, -44.0f, 4.4f, -32.8f, yel);
  bx(-40.0f, 5.8f, -34.4f, -37.4f, 7.6f, -31.6f, rgb(0.82f, 0.80f, 0.74f));
  bx(-28.06f, 3.6f, -33.06f, -27.94f, 7.6f, -32.94f, stl);
  bx(-28.4f, 3.0f, -33.4f, -27.6f, 3.6f, -32.6f, yel);
  // Trucks in the east lane and at T, lamps along the dock, forklift & dockside genset.
  truck(41.0f, 20.0f, 1, rgb(0.16f, 0.34f, 0.56f));
  truck(-26.0f, 46.0f, 0, rgb(0.66f, 0.20f, 0.14f));
  forklift_prop(-19.5f, -25.0f, 0, yel, rgb(0.20f, 0.21f, 0.23f));
  genset(37.5f, 2.5f, 1, rgb(0.22f, 0.42f, 0.56f), rgb(0.18f, 0.20f, 0.22f));
  for (float z = -36.0f; z <= 40.0f; z += 12.0f) lamp(z < -4.0f ? -41.4f : -43.4f, z, 0, -1);
  barrel(36.0f, -2.0f, rgb(0.20f, 0.30f, 0.50f)); barrel(36.8f, -2.4f, rgb(0.62f, 0.20f, 0.14f));
  // Sniper roofs: ladders up five buildings, each on a face over open street —
  // over site A (north row), mid from both sides of the long west block, the
  // yard from the west block's east end, and the north approach.
  ladder_add(-20.0f, -38.0f, SIDE_S);
  ladder_add(20.0f, -40.0f, SIDE_N);
  ladder_add(-26.0f, -2.0f, SIDE_S);
  ladder_add(-10.0f, 12.0f, SIDE_E);
  ladder_add(30.0f, 10.0f, SIDE_N);
}

static void refinery_extras(void) {
  const town_style_t *s = &g_town;
  flare_stack(-11.0f, -35.0f, 24.0f);
  cooling_tower(-21.0f, 13.0f);
  // The distillation column: a stair tower to a 10.4 m deck, the column above.
  tower(24.0f, -11.0f, 1.8f, 10.4f, rgb(0.62f, 0.62f, 0.60f), rgb(0.30f, 0.31f, 0.32f), 0, -1);
  octa(24.0f, -11.0f, 1.0f, 10.4f, 19.0f, rgb(0.78f, 0.78f, 0.76f));
  for (float y = 12.0f; y < 19.0f; y += 2.4f)
    octa(24.0f, -11.0f, 1.12f, y, y + 0.3f, s->trim);
  // A pipe rack down mid (catwalk rails are built by map_grid on '^').
  v3 post = rgb(0.30f, 0.31f, 0.32f);
  static const float PZ[] = {-34.0f, -24.0f, -14.0f, 6.0f, 16.0f, 26.0f, 36.0f};
  for (size_t i = 0; i < sizeof PZ / sizeof PZ[0]; i++) {
    bx(-7.8f, 0.0f, PZ[i] - 0.2f, -7.4f, 4.6f, PZ[i] + 0.2f, post);
    bx(7.4f, 0.0f, PZ[i] - 0.2f, 7.8f, 4.6f, PZ[i] + 0.2f, post);
    bx(-7.8f, 4.6f, PZ[i] - 0.15f, 7.8f, 4.9f, PZ[i] + 0.15f, post);
  }
  bx(-4.2f, 4.9f, -38.0f, -3.75f, 5.35f, 38.0f, s->trim);
  bx(-1.4f, 4.9f, -38.0f, -0.8f, 5.5f, 38.0f, rgb(0.62f, 0.64f, 0.66f));
  bx(1.2f, 4.9f, -38.0f, 1.75f, 5.45f, 38.0f, rgb(0.52f, 0.26f, 0.18f));
  bx(3.6f, 4.9f, -38.0f, 4.0f, 5.3f, 38.0f, rgb(0.26f, 0.46f, 0.30f));
  for (float z = -30.0f; z <= 30.0f; z += 15.0f) lamp(-6.6f, z + 3.0f, 0, +1);
  barrel(-38.0f, -10.0f, rgb(0.20f, 0.30f, 0.50f)); barrel(-37.2f, -10.4f, rgb(0.20f, 0.30f, 0.50f));
  barrel(33.5f, -24.0f, rgb(0.72f, 0.16f, 0.12f)); barrel(34.3f, -24.4f, rgb(0.72f, 0.16f, 0.12f));
  car(20.0f, -46.0f, 0); car(-14.0f, 47.0f, 0);
  forklift_prop(29.5f, -18.0f, 2, rgb(0.84f, 0.58f, 0.12f), rgb(0.22f, 0.23f, 0.25f));
  genset(-37.5f, -14.5f, 0, rgb(0.28f, 0.48f, 0.34f), rgb(0.20f, 0.22f, 0.22f));
}

// The compound's landmarks. The HQ keeps the grid's 'X' cells; the towers,
// the helicopter, the lamps and the vehicles stand on open floor.
#define MAP_COMPOUND_ID 7
static void compound_extras(void) {
  const town_style_t *s = &g_town;
  // HQ: two storeys, the stair up its south wall to the roof (hostage three).
  int bal = g_town.balconies;
  g_town.balconies = 0;
  house_climb(-10.0f, -26.0f, 8.0f, -14.0f, 2, SIDE_N | SIDE_E | SIDE_S | SIDE_W,
              SIDE_N | SIDE_E, SIDE_S, +1, -9.4f);
  g_town.balconies = bal;
  v3 stl = rgb(0.28f, 0.30f, 0.32f);
  bx(-4.2f, 6.4f, -25.4f, -4.02f, 13.6f, -25.22f, stl);            // radio mast
  bx(-4.8f, 11.8f, -25.36f, -3.42f, 11.9f, -25.26f, stl);
  bx(-4.8f, 12.8f, -25.36f, -3.42f, 12.9f, -25.26f, stl);
  bx(0.0f, 6.4f, -25.5f, 2.4f, 6.9f, -24.9f, rgb(0.36f, 0.34f, 0.28f));     // roof bags
  // The corner watchtowers: a stair round a timber core, a lookout with a
  // roof on four posts. A sniper stands in each.
  v3 core = rgb(0.46f, 0.36f, 0.25f), step = s->stone;
  static const float TW[4][2] = {{-35.5f, -31.5f}, {35.5f, -31.5f}, {-35.5f, 27.5f}, {35.5f, 27.5f}};
  for (int i = 0; i < 4; i++) tower(TW[i][0], TW[i][1], 1.2f, 6.0f, core, step, 1, -1);
  // The old lookout in the wadi, outside the walls: the player's own perch.
  tower(-48.0f, 40.0f, 1.2f, 8.4f, s->plaster[2], s->stone, 0, -1);
  // The helicopter on the LZ, nose east. The rotor is drawn live (scene_build).
  v3 od = rgb(0.30f, 0.33f, 0.25f), dk = rgb(0.12f, 0.13f, 0.12f);
  float hx = 22.0f, hz = -45.0f;
  int heli0 = g_num_solids;   // the collision block-out; prop_heli is what shows
  bx(hx - 3.2f, 0.62f, hz - 1.2f, hx + 3.0f, 2.62f, hz + 1.2f, od);           // cabin
  bx(hx + 3.0f, 0.86f, hz - 0.95f, hx + 4.3f, 2.30f, hz + 0.95f, od);          // nose
  bx(hx + 2.4f, 1.70f, hz - 1.05f, hx + 4.1f, 2.48f, hz + 1.05f, s->glass);    // canopy
  bx(hx - 2.6f, 2.62f, hz - 0.8f, hx + 1.6f, 3.0f, hz + 0.8f, tint(od, 0.86f, 0.9f));  // engine
  bx(hx - 10.4f, 1.85f, hz - 0.28f, hx - 3.2f, 2.40f, hz + 0.28f, od);         // tail boom
  bx(hx - 10.9f, 2.40f, hz - 0.10f, hx - 9.7f, 4.40f, hz + 0.10f, od);         // fin
  bx(hx - 1.1f, 1.0f, hz - 1.22f, hx + 1.3f, 2.3f, hz - 1.2f, dk);             // open doors
  bx(hx - 1.1f, 1.0f, hz + 1.2f, hx + 1.3f, 2.3f, hz + 1.22f, dk);
  bx(hx - 0.22f, 3.0f, hz - 0.22f, hx + 0.22f, 3.45f, hz + 0.22f, stl);        // mast
  bx(hx - 3.4f, 0.0f, hz - 1.62f, hx + 3.6f, 0.10f, hz - 1.42f, stl);          // skids
  bx(hx - 3.4f, 0.0f, hz + 1.42f, hx + 3.6f, 0.10f, hz + 1.62f, stl);
  for (int k = 0; k < 2; k++) {
    float sx = k ? hx + 1.9f : hx - 2.1f;
    bx(sx, 0.10f, hz - 1.56f, sx + 0.14f, 0.62f, hz - 1.48f, stl);
    bx(sx, 0.10f, hz + 1.48f, sx + 0.14f, 0.62f, hz + 1.56f, stl);
  }
  solids_hide(heli0);
  prop_add(PROP_HELI, hx, hz, 0, 0, rgb(0.43f, 0.47f, 0.34f), dk);   // faded olive: the blocks keep od
  // The pad: a concrete apron with an H painted on it.
  bx(14.0f, 0.0f, -50.0f, 30.0f, 0.02f, -40.0f, rgb(0.36f, 0.35f, 0.33f));
  v3 wh = rgb(0.86f, 0.86f, 0.82f);
  bx(hx - 6.0f, 0.02f, hz + 2.4f, hx - 5.4f, 0.03f, hz + 4.4f, wh);
  bx(hx - 4.0f, 0.02f, hz + 2.4f, hx - 3.4f, 0.03f, hz + 4.4f, wh);
  bx(hx - 5.4f, 0.02f, hz + 3.15f, hx - 4.0f, 0.03f, hz + 3.65f, wh);
  // Lamps: the gates, the courtyard, the pen door, the motor pool, the LZ.
  lamp(-7.0f, 25.0f, 0, +1); lamp(7.0f, 25.0f, 0, -1);
  lamp(-14.0f, -11.0f, 0, +1); lamp(12.0f, -11.0f, 0, -1);
  lamp(-19.0f, -19.0f, 1, -1); lamp(18.0f, -6.0f, 1, +1);
  lamp(11.0f, -26.0f, 0, +1); lamp(42.0f, 12.0f, 1, +1);
  lamp(12.0f, -42.0f, 0, +1); lamp(-17.0f, 16.0f, 0, -1);
  // Vehicles: two jeeps in the motor pool, a supply truck by each outer gate.
  car(26.0f, -9.0f, 0); car(26.0f, -1.5f, 0);
  truck(-22.0f, -41.0f, 0, rgb(0.34f, 0.36f, 0.26f));
  truck(46.0f, -6.0f, 1, rgb(0.46f, 0.40f, 0.28f));
  // Fuel drums by the tanks and the generator, palms in the wadi and outside.
  v3 red = rgb(0.62f, 0.18f, 0.12f), blu = rgb(0.20f, 0.30f, 0.50f);
  barrel(17.0f, 16.0f, red); barrel(16.2f, 16.6f, red); barrel(17.4f, 16.9f, blu);
  barrel(-12.0f, -30.0f, blu); barrel(-11.2f, -30.4f, red);
  genset(-14.5f, -30.5f, 0, rgb(0.38f, 0.42f, 0.30f), rgb(0.24f, 0.26f, 0.20f));
  forklift_prop(16.5f, -8.5f, 0, rgb(0.78f, 0.56f, 0.16f), rgb(0.24f, 0.24f, 0.22f));
  palm(-46.0f, -20.0f, 5.0f); palm(46.0f, 28.0f, 5.4f); palm(-44.0f, 16.0f, 5.6f);
  palm(30.0f, 49.0f, 5.0f); palm(-10.0f, 47.0f, 5.2f); palm(10.0f, 49.0f, 4.8f);
  palm(-48.0f, 30.0f, 5.0f); palm(47.0f, -30.0f, 5.2f);
}

static void map_compound(void) {
  g_town_kind = 3; town_style(0); map_grid(MAP_COMPOUND); compound_extras();
  street_spans(MAP_COMPOUND); env_scatter(MAP_COMPOUND, 14); g_mg_rows = NULL;
}

static void map_citadel(void) {
  g_town_kind = 0; town_style(0); map_grid(MAP_CITADEL); citadel_extras(); g_mg_rows = NULL;
}
static void map_harbor(void) {
  g_town_kind = 1; town_style(1); map_grid(MAP_HARBOR); harbor_extras();
  street_spans(MAP_HARBOR); env_scatter(MAP_HARBOR, 14); g_mg_rows = NULL;
}
static void map_refinery(void) {
  g_town_kind = 2; town_style(2); map_grid(MAP_REFINERY); refinery_extras();
  street_spans(MAP_REFINERY); env_scatter(MAP_REFINERY, 14); g_mg_rows = NULL;
}

// The map table. `theme` indexes THEMES and `atm` the sky presets (ATM, below
// with the renderer): a handmade map has ONE look, because a place you learn
// is a place you recognise. ARENA keeps drawing both from its seed. `half` is
// the map's half-size and `step` its STEP_H; `route` turns on grid routing for
// the bots (bot_route), which a town of corridors cannot be played without and
// the open maps do not need.
typedef struct {
  const char *name, *key;
  int theme, atm;
  float half, step;
  int route;
  void (*build)(void);
} map_def_t;
static const map_def_t MAP_DEFS[] = {
  {"ARENA",    "arena",    -1, -1, 24.0f, 0.0f,  0, NULL},
  {"FOUNDRY",  "foundry",   1,  4, 52.0f, 0.45f, 1, map_foundry},  // SLATE, STORMFRONT: the yard + the works
  {"BAZAAR",   "bazaar",    2,  1, 42.0f, 0.45f, 1, map_bazaar},    // SAND, GOLDEN HOUR: the core + the souk
  {"OUTPOST",  "outpost",   0,  0, 24.0f, 0.0f,  0, map_outpost},   // QUARTZ, HIGH NOON
  {"CITADEL",  "citadel",   2,  6, 52.0f, 0.45f, 1, map_citadel},   // SAND, CLEAR AFTERNOON
  {"HARBOR",   "harbor",    1,  3, 52.0f, 0.45f, 1, map_harbor},    // SLATE, COLD DAWN
  {"REFINERY", "refinery",  0,  7, 52.0f, 0.45f, 1, map_refinery},  // QUARTZ, CRISP MORNING
  {"COMPOUND", "compound",  2,  5, 52.0f, 0.45f, 1, map_compound},  // SAND, BLUE HOUR
};
#define MAP_COUNT ((int)(sizeof MAP_DEFS / sizeof MAP_DEFS[0]))
#define MAP_ROTATE MAP_COUNT          // the MAP setting's extra value
static const char *const MAP_PICK_NAMES[MAP_COUNT + 1] = {
  "ARENA", "FOUNDRY", "BAZAAR", "OUTPOST", "CITADEL", "HARBOR", "REFINERY", "COMPOUND", "ROTATE"};
static int map_parse(const char *s) {
  for (int i = 0; i < MAP_COUNT; i++) if (!strcmp(s, MAP_DEFS[i].key)) return i;
  return !strcmp(s, "rotate") ? MAP_ROTATE : -1;
}
static const char *map_pick_key(int pick) {
  return pick >= 0 && pick < MAP_COUNT ? MAP_DEFS[pick].key : "rotate";
}
static int map_id_valid(int id) { return id >= 0 && id < MAP_COUNT ? id : 0; }
static int map_routes(void) { return MAP_DEFS[map_id_valid(g_map_id)].route; }

// Latch the MAP setting and decide this match's map. ROTATE steps through every
// map in table order — `advance` is 0 at session bring-up, so a rotation opens
// on ARENA, and 1 on each restart after it.
static void map_pick_apply(int advance) {
  int pick = g_cfg.sp_map;
  if (pick < 0 || pick > MAP_ROTATE) pick = 0;
  g_map_pick = pick;
  // HOSTAGE RESCUE is a mission, and its mission is on one map.
  if (g_cfg.sp_mode == GM_RESCUE && !g_home) { g_map_id = MAP_COMPOUND_ID; return; }
  if (pick < MAP_COUNT) g_map_id = pick;
  else g_map_id = advance ? (map_id_valid(g_map_id) + 1) % MAP_COUNT : 0;
}

static void map_generate_layout(uint32_t seed);
// Lay the arena out, then index it (bp_build). Every caller comes through here.
static void map_generate(uint32_t seed) {
  map_generate_layout(seed);
  bp_build();
}
static void map_generate_layout(uint32_t seed) {
  g_rng = (uint64_t)seed * 2654435761u + 1;
  for (int h = 0; h < (int)(sizeof g_rng_gun / sizeof g_rng_gun[0]); h++)
    g_rng_gun[h] = rng_gun_seed(seed, h);
  g_rng_bot = (uint64_t)(seed ^ 0xB07u) * 2654435761u + 1;
  g_num_solids = 0;
  g_nladders = 0;
  g_nprops = 0;
  g_nsupply = 0;
  g_theme = THEMES[rng_u32() % (sizeof THEMES / sizeof THEMES[0])];
  const map_def_t *md = &MAP_DEFS[map_id_valid(g_map_id)];
  if (md->theme >= 0) g_theme = THEMES[md->theme];   // the draw above still
                                                     // happens: same streams
  // What the theme's palette entries are made of. A town map registers its own
  // palette on top of these in town_style; the closest match wins, so a
  // plaster that happens to sit near a theme colour still reads as plaster.
  smat_reset();
  smat_reg(g_theme.wall, SMAT_CONCRETE);
  smat_reg(g_theme.cover, g_theme.surf == SURF_WET ? SMAT_CLAD : SMAT_BRICK);
  smat_reg(g_theme.crate_a, SMAT_WOOD);
  smat_reg(g_theme.crate_b, SMAT_CONCRETE);
  g_arena_half_all[g_lobby] = md->half;              // before the shell below
  g_step_h_all[g_lobby] = md->step;
  {  // see wind_dir(): hashed off the seed, never drawn from the map stream
    uint32_t wh = seed * 2246822519u;
    wh ^= wh >> 13; wh *= 3266489917u; wh ^= wh >> 16;
    g_wind_ang = (float)(wh >> 8) * (1.0f / 16777216.0f) * (2.0f * F_PI);
  }

  const float H = ARENA_HALF, WT = 0.6f, WH = 3.2f, BH = 2.75f;
  v3 wc = g_theme.wall;
  map_add((v3){{-H - WT, 0, -H - WT}}, (v3){{H + WT, BH, -H}}, wc);
  map_add((v3){{-H - WT, 0, H}},       (v3){{H + WT, BH, H + WT}}, wc);
  map_add((v3){{-H - WT, 0, -H}},      (v3){{-H, BH, H}}, wc);
  map_add((v3){{H, 0, -H}},            (v3){{H + WT, BH, H}}, wc);
  // Compass bands atop the walls: north cool, south warm, east/west dark —
  // instant orientation in a rotationally symmetric arena.
  map_add((v3){{-H - WT, BH, -H - WT}}, (v3){{H + WT, WH, -H}}, g_theme.band_n);
  map_add((v3){{-H - WT, BH, H}},       (v3){{H + WT, WH, H + WT}}, g_theme.band_s);
  map_add((v3){{-H - WT, BH, -H}},      (v3){{-H, WH, H}}, g_theme.band_ew);
  map_add((v3){{H, BH, -H}},            (v3){{H + WT, WH, H}}, g_theme.band_ew);
  // Everything added SO FAR is the arena shell, and a round may never cross it:
  // it is the edge of the world rather than cover, and the shell happens to be
  // the same 0.60 m thick as a cover wall, so thickness alone cannot tell them
  // apart. Marked here, next to the eight calls that build it, rather than
  // threaded through map_add as a parameter every other caller would pass 1 to.
  for (int i = 0; i < g_num_solids; i++) g_solids[i].pen = 0;
  if (md->build) { md->build(); supply_place(); return; }

  // Mid landmark, self-symmetric: the map's one saturated color anchor.
  switch (rng_u32() % 4) {
  case 0: {  // monolith: full block, splits mid into two lanes
    float mx_ = rng_range(2.0f, 3.2f), mz = rng_range(2.0f, 3.2f);
    float mh = rng_range(2.5f, 3.0f);
    map_add((v3){{-mx_, 0, -mz}}, (v3){{mx_, mh, mz}}, g_theme.mid);
  } break;
  case 1: {  // plaza: mountable low platform, pillar marks the center
    float pw = rng_range(3.4f, 4.4f);
    map_add((v3){{-pw, 0, -pw}}, (v3){{pw, 0.55f, pw}},
            col_tint(g_theme.crate_b, 0.97f, 1.03f));
    map_add((v3){{-0.7f, 0.55f, -0.7f}}, (v3){{0.7f, 3.0f, 0.7f}}, g_theme.mid);
  } break;
  case 2: {  // gate: two parallel walls, a fought-over corridor through mid
    float gx = rng_range(2.6f, 3.6f), gl = rng_range(3.5f, 5.0f);
    float gh = rng_range(2.1f, 2.5f);
    map_add_sym((v3){{gx - 0.35f, 0, -gl}}, (v3){{gx + 0.35f, gh, gl}}, g_theme.mid);
  } break;
  default: {  // four pillars: porous mid, peek duels
    float d = rng_range(2.6f, 3.8f), r = rng_range(0.45f, 0.6f);
    float h = rng_range(2.4f, 2.8f);
    map_add_sym((v3){{d - r, 0, d - r}},  (v3){{d + r, h, d + r}},  g_theme.mid);
    map_add_sym((v3){{d - r, 0, -d - r}}, (v3){{d + r, h, -d + r}}, g_theme.mid);
  } break;
  }

  // Long tactical walls. Heights are curated, never eye-level: LOW you shoot
  // over standing, HIGH blocks fully — no awkward in-between clutter.
  int nwalls = 3 + (int)(rng_u32() % 2);
  for (int i = 0; i < nwalls; i++) {
    for (int t = 0; t < 24; t++) {
      float hl = rng_range(1.8f, 3.4f);
      float h = (rng_u32() & 1) ? rng_range(1.95f, 2.25f) : rng_range(0.95f, 1.10f);
      float cx = rng_range(-(H - RING_W - 1.5f), H - RING_W - 1.5f);
      float cz = rng_range(-(H - RING_W - 1.5f), H - RING_W - 1.5f);
      if (cx * cx + cz * cz < 64.0f) continue;  // leave mid to the landmark
      int ax = rng_u32() & 1;
      v3 mn = {{cx - (ax ? hl : 0.3f), 0, cz - (ax ? 0.3f : hl)}};
      v3 mx = {{cx + (ax ? hl : 0.3f), h, cz + (ax ? 0.3f : hl)}};
      if (mn.x < -(H - RING_W) || mn.z < -(H - RING_W) ||
          mx.x > H - RING_W || mx.z > H - RING_W) continue;
      if (!map_sym_free(mn, mx, 1.4f)) continue;
      map_add_wall_sym(mn, mx, col_tint(g_theme.cover, 0.94f, 1.06f), ax);
      break;
    }
  }

  // Crate clusters: anchor crate + optional jump-step + optional top box.
  int nclust = 4 + (int)(rng_u32() % 2);
  for (int i = 0; i < nclust && g_num_solids < MAX_SOLIDS - 8; i++) {
    for (int t = 0; t < 24; t++) {
      float hx = rng_range(0.7f, 1.15f), hz = rng_range(0.7f, 1.15f);
      float h = rng_range(1.0f, 1.3f);
      float cx = rng_range(-(H - RING_W - 1.2f), H - RING_W - 1.2f);
      float cz = rng_range(-(H - RING_W - 1.2f), H - RING_W - 1.2f);
      if (cx * cx + cz * cz < 42.0f) continue;
      v3 mn = {{cx - hx, 0, cz - hz}}, mx = {{cx + hx, h, cz + hz}};
      if (!map_sym_free(mn, mx, 2.2f)) continue;  // room incl. attachments
      map_add_sym(mn, mx, col_tint(g_theme.crate_a, 0.93f, 1.07f));
      uint32_t roll = rng_u32();
      if (roll & 1) {  // step crate: jump-height route onto the anchor
        float sh = rng_range(0.5f, 0.62f), sw = rng_range(0.45f, 0.6f);
        float sd = rng_range(0.4f, 0.55f);
        v3 smn, smx;  // overlaps the anchor 2 cm: no coplanar faces
        switch ((roll >> 1) & 3) {
        case 0:  smn = (v3){{mx.x - 0.02f, 0, cz - sw}};      smx = (v3){{mx.x + 2 * sd, sh, cz + sw}}; break;
        case 1:  smn = (v3){{mn.x - 2 * sd, 0, cz - sw}};     smx = (v3){{mn.x + 0.02f, sh, cz + sw}}; break;
        case 2:  smn = (v3){{cx - sw, 0, mx.z - 0.02f}};      smx = (v3){{cx + sw, sh, mx.z + 2 * sd}}; break;
        default: smn = (v3){{cx - sw, 0, mn.z - 2 * sd}};     smx = (v3){{cx + sw, sh, mn.z + 0.02f}}; break;
        }
        // The step extends up to 2*sd past the anchor with no bounds check of
        // its own, and the ANCHOR's placement bound only covers the anchor:
        // ~1 in 12 seeds put a step crate up to a metre inside the RING_W
        // corridor that is documented cover-free (rotation routes, the four
        // long sightlines, and every rig that stages there BECAUSE it is
        // cover-free). Same rejection the tactical walls use; the crate just
        // loses its optional jump route.
        if (smn.x >= -(H - RING_W) && smn.z >= -(H - RING_W) &&
            smx.x <= H - RING_W && smx.z <= H - RING_W)
          map_add_sym(smn, smx, col_tint(g_theme.crate_b, 0.93f, 1.07f));
      }
      if (((roll >> 3) & 63) < 28) {  // top box: silhouette variety
        float tx = hx * rng_range(0.5f, 0.7f), tz = hz * rng_range(0.5f, 0.7f);
        float th = rng_range(0.4f, 0.55f);
        // Inset by 3 cm, not flush: at the extremes of this range the top box's
        // side face lands on the anchor's, and two same-facing coplanar faces
        // z-fight at every distance. `mapcheck` counts exactly that.
        float ox = rng_range(-(hx - tx - 0.03f), hx - tx - 0.03f);
        float oz = rng_range(-(hz - tz - 0.03f), hz - tz - 0.03f);
        map_add_sym((v3){{cx + ox - tx, h - 0.02f, cz + oz - tz}},
                    (v3){{cx + ox + tx, h + th, cz + oz + tz}},
                    col_tint(g_theme.crate_b, 0.93f, 1.07f));
      }
      break;
    }
  }  // THE ARENA'S SHELL, dressed last: after every draw from the map stream, so a
  // recorded seed lays out exactly as it always did.
  small_map_style();
  k_shell_piers(g_theme.wall);
}

