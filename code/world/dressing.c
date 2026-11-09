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
  k_pipes(1, 24.0f, -1.0f, 0.06f, -13.8f, 13.8f, 1.75f, 3, rust);
  // the sheds' contents: crates, a pallet stack, drums
  k_crate(17.7f, -7.3f, 0.70f, 0.70f, 1.10f, t->crate_a);
  k_crate(18.7f, -7.3f, 0.30f, 0.40f, 0.55f, t->crate_b);
  k_crate(20.1f, 2.6f, 0.60f, 0.60f, 1.00f, t->crate_a);
  KMAT(SMAT_WOOD, KB(19.8f, 0.98f, 2.3f, 20.4f, 1.45f, 2.9f, kt_(t->crate_b, 20.1f, 2.6f, 4, 0.9f, 1.0f)));
  k_pallets(21.6f, -11.5f, 1, 5);
  k_jersey(0, 10.25f, 16.0f, 21.0f);
  barrel(g_ks * 16.2f, g_ks * -16.0f, rgb(0.62f, 0.18f, 0.12f));
  barrel(g_ks * 16.9f, g_ks * -16.3f, rgb(0.20f, 0.30f, 0.50f));
  barrel(g_ks * 16.2f, g_ks * -17.0f, rgb(0.74f, 0.56f, 0.16f));
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
  {
    v3 fy = rgb(0.86f, 0.64f, 0.12f);
    KMAT(SMAT_HAZARD, KB(-9.1f, 0.30f, 19.4f, -7.9f, 1.15f, 21.2f, fy));
    KMAT(SMAT_STEEL, {
      KB(-9.1f, 0.30f, 21.2f, -7.9f, 1.25f, 21.55f, dark);           // counterweight
      KB(-9.05f, 1.15f, 19.45f, -8.98f, 2.15f, 19.52f, dark);        // cage posts
      KB(-8.02f, 1.15f, 19.45f, -7.95f, 2.15f, 19.52f, dark);
      KB(-9.05f, 1.15f, 20.9f, -8.98f, 2.15f, 20.97f, dark);
      KB(-8.02f, 1.15f, 20.9f, -7.95f, 2.15f, 20.97f, dark);
      KB(-9.08f, 2.15f, 19.42f, -7.92f, 2.21f, 21.0f, dark);         // roof
      KB(-9.0f, 0.05f, 19.10f, -8.88f, 2.40f, 19.25f, steel);        // mast
      KB(-8.12f, 0.05f, 19.10f, -8.0f, 2.40f, 19.25f, steel);
      KB(-8.9f, 0.10f, 18.0f, -8.78f, 0.15f, 19.10f, steel);         // forks
      KB(-8.22f, 0.10f, 18.0f, -8.10f, 0.15f, 19.10f, steel);
    });
    KB(-9.18f, 0.0f, 19.6f, -9.1f, 0.55f, 20.2f, dark);              // wheels
    KB(-7.9f, 0.0f, 19.6f, -7.82f, 0.55f, 20.2f, dark);
    KB(-9.18f, 0.0f, 20.8f, -9.1f, 0.50f, 21.3f, dark);
    KB(-7.9f, 0.0f, 20.8f, -7.82f, 0.50f, 21.3f, dark);
  }
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
  // the roof stair, in stone
  KMAT(SMAT_STONE, {
    KB(3.4f, 0.0f, 9.0f, 4.2f, 1.05f, 10.4f, stone);
    KB(4.2f, 0.0f, 9.0f, 5.0f, 2.10f, 10.4f, kt_(stone, 4, 9, 5, 0.92f, 0.96f));
  });
  // doors with lintels, windows with sills and shutters (flush colour below
  // head height, the way the town facades are), an AC box and a hung rug
  KMAT(SMAT_WOOD, {
    KB(4.97f, 0.0f, 6.0f, 5.0f, 2.2f, 7.1f, door);
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
    KB(4.88f, 2.2f, 5.92f, 5.0f, 2.34f, 7.18f, stone);
    KB(-5.0f, 2.2f, 6.52f, -4.88f, 2.34f, 7.78f, stone);
    KB(7.3f, 1.20f, 4.92f, 8.5f, 1.30f, 5.0f, stone);
    KB(-8.7f, 1.20f, 4.92f, -7.5f, 1.30f, 5.0f, stone);
  });
  KMAT(SMAT_STEEL, KB(4.55f, 2.40f, 8.0f, 5.0f, 2.85f, 8.8f, rgb(0.74f, 0.74f, 0.72f)));
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
  k_crate(20.6f, -6.4f, 0.6f, 0.6f, 1.1f, t->crate_a);
  k_crate(21.3f, -4.9f, 0.4f, 0.4f, 0.6f, t->crate_b);
  lamp(g_ks * 4.4f, g_ks * -13.0f, 1, g_ks > 0.0f ? 1 : -1);
  lamp(g_ks * 13.0f, g_ks * 4.4f, 0, g_ks > 0.0f ? -1 : 1);
}
static void map_bazaar(void) {
  small_map_style();
  const theme_t *t = &g_theme;
  k_shell_piers(t->wall);
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
#define LADDER_OFF   0.16f   // rails' depth off the face
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
  for (int r = -1; r <= 1; r += 2) {   // rails
    float ax = x + tx * LADDER_HALF * (float)r, az = z + tz * LADDER_HALF * (float)r;
    bx(ax - 0.03f * fabsf(tx), 0.0f, az - 0.03f * fabsf(tz),
       ax + 0.03f * fabsf(tx) + nx * LADDER_OFF, top, az + 0.03f * fabsf(tz) + nz * LADDER_OFF, m);
  }
  for (float y = 0.30f; y < top - 0.1f; y += 0.30f)   // rungs
    bx(x - tx * (LADDER_HALF - 0.03f) + nx * 0.06f, y, z - tz * (LADDER_HALF - 0.03f) + nz * 0.06f,
       x + tx * (LADDER_HALF - 0.03f) + nx * 0.12f, y + 0.04f, z + tz * (LADDER_HALF - 0.03f) + nz * 0.12f, m);
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
  static const float R[9] = {8.0f, 7.3f, 6.7f, 6.25f, 6.0f, 5.9f, 6.0f, 6.25f, 6.6f};
  v3 c = rgb(0.74f, 0.72f, 0.68f);
  for (int i = 0; i < 9; i++)
    octa(cx, cz, R[i], (float)i * 2.0f, (float)(i + 1) * 2.0f, tint(c, 0.96f, 1.02f));
  octa(cx, cz, 6.8f, 18.0f, 18.6f, rgb(0.36f, 0.36f, 0.36f));
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

// A BLOCK OF TOWN: one merged '#' (tall) or '%' (low) rect, cut along its long
// axis into houses 6-11 m wide, each with its own height, plaster, windows on
// the sides that face a street and, often, a door on one of them.
static void town_block(const char *const rows[MAPGRID_N], float x0, float z0, float x1,
                       float z1, int tall) {
  int along_x = (x1 - x0) >= (z1 - z0);
  float a = along_x ? x0 : z0, end = along_x ? x1 : z1;
  while (end - a > 0.1f) {
    float w = rng_range(6.0f, 11.0f);
    if (end - (a + w) < 5.0f) w = end - a;          // no sliver at the end
    float hx0 = along_x ? a : x0, hx1 = along_x ? a + w : x1;
    float hz0 = along_x ? z0 : a, hz1 = along_x ? z1 : a + w;
    int win = 0;
    for (int sd = 1; sd <= 8; sd <<= 1)
      if (mg_side_open(rows, hx0, hz0, hx1, hz1, sd)) win |= sd;
    int floors = tall ? 2 + (int)(rng_u32() % 2) : 1 + (int)(rng_u32() % 2);
    if (hx1 - hx0 < 4.0f || hz1 - hz0 < 4.0f) floors = floors > 2 ? 2 : floors;
    int doors = 0;
    if (win && (rng_u32() % 10) < 7) {
      for (int sd = 1; sd <= 8; sd <<= 1) if (win & sd) { doors = sd; break; }
    }
    house(hx0, hz0, hx1, hz1, floors, win, doors, 0, 0.0f, 0.0f, 1);
    a += w;
  }
}

// A CLIMBABLE HOUSE ('H' tall, 'h' low): one house, its stair on the longest
// side facing a street, as many floors as that side has room to climb.
static void town_climb(const char *const rows[MAPGRID_N], float x0, float z0, float x1,
                       float z1, int tall) {
  int win = 0;
  for (int sd = 1; sd <= 8; sd <<= 1)
    if (mg_side_open(rows, x0, z0, x1, z1, sd)) win |= sd;
  // The stair stands in the street, so it needs a stretch of clear street
  // along the wall, not merely a side that touches one somewhere.
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
      int open = axz ? mg_point_open(m, pl + out * 1.0f) : mg_point_open(pl + out * 1.0f, m);
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
  while (floors > 1 && (float)floors * PF_FLOOR / PF_RISE * PF_RUN + 0.8f > best_len) floors--;
  if (!best || (float)floors * PF_FLOOR / PF_RISE * PF_RUN + 0.8f > best_len) {
    house(x0, z0, x1, z1, floors, win, 0, 0, 0.0f, 0.0f, 1);
    return;
  }
  int other = win & ~best;
  int doors = 0;
  for (int sd = 1; sd <= 8; sd <<= 1) if (other & sd) { doors = sd; break; }
  house_climb(x0, z0, x1, z1, floors, win, doors, best, +1, best_a + 0.4f);
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
    }
}
// THE LANDMARKS, by hand, on the cells the grids reserve ('X') or leave open.
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
  palm(7.6f, 4.6f, 5.2f);
  // Arched gates at mid doors and long A doors.
  arch_gate(0, -23.0f, -2.0f, 2.0f, -6.0f, 6.0f, 6.0f, s->plaster[0]);
  arch_gate(0, 7.0f, -42.0f, -38.0f, -46.0f, -36.0f, 6.0f, s->plaster[2]);
  // Palms, market stalls, carts, lamps, barrels.
  palm(-22.0f, -16.0f, 5.6f); palm(-12.0f, 44.0f, 5.0f); palm(12.0f, 44.0f, 5.4f);
  palm(-4.0f, -38.0f, 5.0f);
  stall1(4.6f, 12.0f, 1, s->awning[0]); stall1(4.6f, 22.0f, 1, s->awning[1]);
  car(-30.0f, -44.5f, 0); car(34.0f, -44.0f, 0); car(-30.0f, 32.0f, 0);
  for (float z = -10.0f; z <= 26.0f; z += 12.0f) lamp(5.6f, z + 1.0f, 0, -1);
  barrel(-38.0f, -22.0f, s->door); barrel(-37.2f, -22.4f, s->door);
  barrel(20.0f, -34.0f, s->door);
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
  // Trucks in the east lane and at T, lamps along the dock.
  truck(41.0f, 20.0f, 1, rgb(0.16f, 0.34f, 0.56f));
  truck(-26.0f, 46.0f, 0, rgb(0.66f, 0.20f, 0.14f));
  for (float z = -36.0f; z <= 40.0f; z += 12.0f) lamp(-43.4f, z, 0, -1);
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
  // A pipe rack down mid, and railings on the catwalk across it.
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
  bx(-8.0f, 2.8f, -4.0f, 8.0f, 3.8f, -3.92f, s->trim);
  bx(-8.0f, 2.8f, -0.08f, 8.0f, 3.8f, 0.0f, s->trim);
  for (float z = -30.0f; z <= 30.0f; z += 15.0f) lamp(-6.6f, z + 3.0f, 0, +1);
  barrel(-38.0f, -10.0f, rgb(0.20f, 0.30f, 0.50f)); barrel(-37.2f, -10.4f, rgb(0.20f, 0.30f, 0.50f));
  barrel(30.0f, -24.0f, rgb(0.72f, 0.16f, 0.12f)); barrel(30.8f, -24.4f, rgb(0.72f, 0.16f, 0.12f));
  car(20.0f, -46.0f, 0); car(-14.0f, 47.0f, 0);
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
  prop_add(PROP_HELI, hx, hz, 0, 0, od, dk);
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
  bx(-15.6f, 0.0f, -31.4f, -13.4f, 1.4f, -29.6f, rgb(0.38f, 0.42f, 0.30f));   // generator
  palm(-46.0f, -20.0f, 5.0f); palm(46.0f, 28.0f, 5.4f); palm(-44.0f, 16.0f, 5.6f);
  palm(30.0f, 49.0f, 5.0f); palm(-10.0f, 47.0f, 5.2f); palm(10.0f, 49.0f, 4.8f);
  palm(-48.0f, 30.0f, 5.0f); palm(47.0f, -30.0f, 5.2f);
}

static void map_compound(void) {
  g_town_kind = 3; town_style(0); map_grid(MAP_COMPOUND); compound_extras(); g_mg_rows = NULL;
}

static void map_citadel(void) {
  g_town_kind = 0; town_style(0); map_grid(MAP_CITADEL); citadel_extras(); g_mg_rows = NULL;
}
static void map_harbor(void) {
  g_town_kind = 1; town_style(1); map_grid(MAP_HARBOR); harbor_extras(); g_mg_rows = NULL;
}
static void map_refinery(void) {
  g_town_kind = 2; town_style(2); map_grid(MAP_REFINERY); refinery_extras(); g_mg_rows = NULL;
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
  {"FOUNDRY",  "foundry",   1,  4, 24.0f, 0.0f,  0, map_foundry},   // SLATE, STORMFRONT
  {"BAZAAR",   "bazaar",    2,  1, 24.0f, 0.0f,  0, map_bazaar},    // SAND, GOLDEN HOUR
  {"OUTPOST",  "outpost",   0,  0, 24.0f, 0.0f,  0, map_outpost},   // QUARTZ, HIGH NOON
  {"CITADEL",  "citadel",   2,  2, 52.0f, 0.45f, 1, map_citadel},   // SAND, EMBER DUSK
  {"HARBOR",   "harbor",    1,  3, 52.0f, 0.45f, 1, map_harbor},    // SLATE, COLD DAWN
  {"REFINERY", "refinery",  0,  1, 52.0f, 0.45f, 1, map_refinery},  // QUARTZ, GOLDEN HOUR
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
  if (md->build) { md->build(); return; }

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

