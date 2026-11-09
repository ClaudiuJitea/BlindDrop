// code/render/town.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ===========================================================================
// TOWN DRESSING — the architectural detail a block-out never has, generated
// from the solids that are already there. Same rules as the rest of the
// decor: render-only, never a solid, and never anything that could hide a
// player a round would pass through. Everything below either sits on a face
// that is already solid (a frame in a window recess, a flange on a pipe, a
// moulding on a facade above head height) or is too thin to hide anything
// (a cable 4 m up).
// ===========================================================================
static int dec_inside(v3 p) {
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *s = &g_solids[i];
    if (p.x > s->min.x && p.x < s->max.x && p.y > s->min.y && p.y < s->max.y &&
        p.z > s->min.z && p.z < s->max.z) return 1;
  }
  return 0;
}
static int dec_same(v3 a, v3 b) {
  return fabsf(a.x - b.x) + fabsf(a.y - b.y) + fabsf(a.z - b.z) < 1e-4f;
}
// An axis-aligned box from two corners, through the scene primitive.
static void dec_box(v3 a, v3 b, v3 col) {
  v3 c = v3_scale(v3_add(a, b), 0.5f);
  v3 h = v3_scale(v3_sub(b, a), 0.5f);
  scene_box(c, (v3){{fabsf(h.x), 0, 0}}, (v3){{0, fabsf(h.y), 0}}, (v3){{0, 0, fabsf(h.z)}}, col);
}
// A box on a face: `ax` is the axis the face's normal lies on (0 = x, 2 = z),
// `sg` the normal's sign, `pl` the face plane; u is along the face, y up, and
// t0..t1 how far OUT of the face it stands.
static void dec_fbox(int ax, float sg, float pl, float u0, float u1, float y0, float y1,
                     float t0, float t1, v3 col) {
  float d0 = pl + sg * t0, d1 = pl + sg * t1;
  if (ax == 0) dec_box((v3){{fminf(d0, d1), y0, u0}}, (v3){{fmaxf(d0, d1), y1, u1}}, col);
  else         dec_box((v3){{u0, y0, fminf(d0, d1)}}, (v3){{u1, y1, fmaxf(d0, d1)}}, col);
}
// A thin bar between two points (a cable segment, a handwheel spoke).
static void dec_bar(v3 a, v3 b, float r, v3 col) {
  v3 d = v3_sub(b, a);
  float l = v3_len(d);
  if (l < 1e-4f) return;
  v3 z = v3_scale(d, 1.0f / l);
  v3 up = fabsf(z.y) > 0.9f ? (v3){{1, 0, 0}} : (v3){{0, 1, 0}};
  v3 x = v3_norm(v3_cross(up, z)), y = v3_cross(z, x);
  scene_box(v3_scale(v3_add(a, b), 0.5f), v3_scale(x, r), v3_scale(y, r), v3_scale(z, l * 0.5f), col);
}
// Which side of a thin slab is the street: a probe a little past each big
// face, below the slab's own top (where a house's interior is its solid core).
static int dec_out_side(const solid_t *s, int ax, float probe_y) {
  float c = ax == 0 ? (s->min.z + s->max.z) * 0.5f : (s->min.x + s->max.x) * 0.5f;
  v3 pp = ax == 0 ? (v3){{s->max.x + 0.45f, probe_y, c}} : (v3){{c, probe_y, s->max.z + 0.45f}};
  v3 pn = ax == 0 ? (v3){{s->min.x - 0.45f, probe_y, c}} : (v3){{c, probe_y, s->min.z - 0.45f}};
  int ip = dec_inside(pp), in = dec_inside(pn);
  if (ip == in) return 0;
  return ip ? -1 : 1;
}

// The cable sag: a real catenary y = a(cosh(x/a) - 1), with `a` solved by
// bisection for the requested sag, so a long span droops more than a parabola
// would in the middle and less near the anchors.
static float catenary_a(float span, float sag) {
  float lo = 0.05f, hi = 1e4f;
  for (int i = 0; i < 48; i++) {
    float a = 0.5f * (lo + hi);
    float s_ = a * (coshf(span * 0.5f / a) - 1.0f);
    if (s_ > sag) lo = a; else hi = a;
  }
  return 0.5f * (lo + hi);
}
static void dec_cable(v3 p0, v3 p1, v3 col) {
  v3 d = v3_sub(p1, p0);
  float span = sqrtf(d.x * d.x + d.z * d.z);
  float sag = 0.03f * span + 0.12f;
  float a = catenary_a(span, sag);
  const int N = 14;
  v3 prev = p0;
  for (int i = 1; i <= N; i++) {
    float t = (float)i / (float)N;
    float x = (t - 0.5f) * span;
    float y = a * (coshf(x / a) - coshf(span * 0.5f / a));   // 0 at both ends
    v3 q = v3_add(v3_lerp(p0, p1, t), (v3){{0, y, 0}});
    dec_bar(prev, q, 0.011f, col);
    prev = q;
  }
}

static void pbox_v(v3 c, v3 ex, v3 ey, v3 ez, v3 col);
static void pcyl(v3 c, v3 ax, float r, float hl, int segs, v3 col);
// A 5x7 dot font for the signs and stencils: each glyph is seven rows of five
// bits, MSB on the left. Only the characters the towns actually spell.
static const struct { char c; uint8_t r[7]; } DOT_FONT[] = {
  {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
  {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
  {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
  {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
  {'I', {14, 4, 4, 4, 4, 4, 14}},      {'K', {17, 18, 20, 24, 20, 18, 17}},
  {'L', {16, 16, 16, 16, 16, 16, 31}}, {'M', {17, 27, 21, 21, 17, 17, 17}},
  {'N', {17, 25, 21, 19, 17, 17, 17}}, {'O', {14, 17, 17, 17, 17, 17, 14}},
  {'P', {30, 17, 17, 30, 16, 16, 16}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
  {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
  {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
  {'W', {17, 17, 17, 21, 21, 27, 17}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
  {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
  {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
  {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
  {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 30, 1, 1, 17, 14}},
  {'6', {6, 8, 16, 30, 17, 17, 14}},   {'7', {31, 1, 2, 4, 8, 8, 8}},
  {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 2, 12}},
  {'-', {0, 0, 0, 31, 0, 0, 0}},       {'.', {0, 0, 0, 0, 0, 12, 12}},
};
static const uint8_t *dot_glyph(char c) {
  for (size_t i = 0; i < sizeof DOT_FONT / sizeof DOT_FONT[0]; i++)
    if (DOT_FONT[i].c == c) return DOT_FONT[i].r;
  return NULL;
}
// Lay text on a face: `o` the text's top-left on the face, `u` one dot pitch
// along the line, `v` one dot pitch DOWN, `nrm` the face normal. Each lit dot
// is a quad `dot` pitches across; `lift` stands it off the face.
static void dot_text(const char *t, v3 o, v3 u, v3 v, v3 nrm, float dot, float lift, v3 col) {
  v3 base = v3_add(o, v3_scale(nrm, lift));
  for (int ci = 0; t[ci]; ci++) {
    const uint8_t *g = dot_glyph(t[ci]);
    if (g)
      for (int r = 0; r < 7; r++)
        for (int k = 0; k < 5; k++) {
          if (!(g[r] & (16 >> k))) continue;
          v3 p = v3_add(base, v3_add(v3_scale(u, (float)(ci * 6 + k)), v3_scale(v, (float)r)));
          v3 du = v3_scale(u, dot), dv = v3_scale(v, dot);
          v3 a = p, b = v3_add(p, du), c = v3_add(p, v3_add(du, dv)), d = v3_add(p, dv);
          if (v3_dot(v3_cross(v3_sub(b, a), v3_sub(d, a)), nrm) > 0.0f) scene_quad(a, b, c, d, nrm, col);
          else scene_quad(a, d, c, b, nrm, col);
        }
  }
}
static int dot_len(const char *t) { return (int)strlen(t) * 6 - 1; }

// A SHIPPING CONTAINER, dressed: the eight ISO corner castings, the door end's
// four locking bars with their handles, and the owner code stencilled on the
// side. The box is a solid; all of this sits within 3 cm of its faces.
static void dec_container(const solid_t *s, int idx) {
  v3 mn = s->min, mx = s->max;
  int alx = (mx.x - mn.x) > (mx.z - mn.z);           // long axis along x?
  v3 cast = v3_scale(s->color, 0.55f);
  mat_set(0.6f, 0.3f);
  for (int k = 0; k < 8; k++) {
    float x = k & 1 ? mx.x : mn.x, y = k & 2 ? mx.y : mn.y, z = k & 4 ? mx.z : mn.z;
    float sx = k & 1 ? -1.0f : 1.0f, sy = k & 2 ? -1.0f : 1.0f, sz = k & 4 ? -1.0f : 1.0f;
    dec_box((v3){{fminf(x, x + sx * 0.17f) - 0.006f, fminf(y, y + sy * 0.12f) - 0.006f, fminf(z, z + sz * 0.17f) - 0.006f}},
            (v3){{fmaxf(x, x + sx * 0.17f) + 0.006f, fmaxf(y, y + sy * 0.12f) + 0.006f, fmaxf(z, z + sz * 0.17f) + 0.006f}}, cast);
  }
  // Door end: the +long-axis face.
  float H0 = mn.y + 0.12f, H1 = mx.y - 0.12f;
  mat_set(0.45f, 0.7f);
  v3 rod = rgb(0.22f, 0.23f, 0.24f);
  float wid = alx ? (mx.z - mn.z) : (mx.x - mn.x);
  for (int r = 0; r < 4; r++) {
    float u = (alx ? mn.z : mn.x) + wid * (0.16f + 0.225f * (float)r);
    v3 c0 = alx ? (v3){{mx.x + 0.035f, (H0 + H1) * 0.5f, u}} : (v3){{u, (H0 + H1) * 0.5f, mx.z + 0.035f}};
    pcyl(c0, (v3){{0, 1, 0}}, 0.017f, (H1 - H0) * 0.5f, 6, rod);
    // Handle, a third of the way up, and the cam keepers top and bottom.
    v3 hd = alx ? (v3){{0, 0, 1}} : (v3){{1, 0, 0}};
    v3 hc = v3_add(c0, v3_add((v3){{0, -(H1 - H0) * 0.18f, 0}}, v3_scale(hd, 0.09f)));
    dec_bar(v3_add(c0, (v3){{0, -(H1 - H0) * 0.18f, 0}}), v3_add(hc, v3_scale(hd, 0.09f)), 0.012f, rod);
  }
  // The door seam down the middle of the end.
  float um = (alx ? mn.z + mx.z : mn.x + mx.x) * 0.5f;
  if (alx) dec_box((v3){{mx.x, mn.y + 0.1f, um - 0.008f}}, (v3){{mx.x + 0.012f, mx.y - 0.1f, um + 0.008f}}, v3_scale(s->color, 0.4f));
  else     dec_box((v3){{um - 0.008f, mn.y + 0.1f, mx.z}}, (v3){{um + 0.008f, mx.y - 0.1f, mx.z + 0.012f}}, v3_scale(s->color, 0.4f));
  // THE OWNER CODE: four letters, U, a six-digit serial — stencilled in white
  // near the top of one long side, as every real box carries it.
  static const char *PFX[] = {"BDXU", "MSKU", "TGHU", "CAIU", "HLXU", "OOLU"};
  char code[16];
  uint32_t hsh = (uint32_t)idx * 2654435761u ^ 0x5EEDu;
  snprintf(code, sizeof code, "%s %06u", PFX[hsh % 6u], (unsigned)((hsh >> 4) % 1000000u));
  float pitch = 0.024f;
  float len = (float)dot_len(code) * pitch;
  mat_set(0.7f, 0.0f);
  v3 wht = rgb(0.86f, 0.86f, 0.84f);
  if (alx) {
    float x0 = mx.x - 0.5f - len;
    dot_text(code, (v3){{x0, mx.y - 0.32f, mx.z}}, (v3){{pitch, 0, 0}}, (v3){{0, -pitch, 0}},
             (v3){{0, 0, 1}}, 0.8f, 0.004f, wht);
  } else {
    float z0 = mx.z - 0.5f - len;
    dot_text(code, (v3){{mn.x, mx.y - 0.32f, z0}}, (v3){{0, 0, pitch}}, (v3){{0, -pitch, 0}},
             (v3){{-1, 0, 0}}, 0.8f, 0.004f, wht);
  }
}

// AN LED SIGN on a facade: a dark backing board, a thin frame, and the text
// as lit dots (SMAT_LED — always on, and it blooms). `o` is the sign's
// centre on the face, `u` along the face, `nrm` out of it.
static void dec_sign(const char *text, v3 o, v3 u, v3 nrm, v3 led) {
  float pitch = 0.050f;
  float len = (float)dot_len(text) * pitch, hgt = 7.0f * pitch;
  float bw = len * 0.5f + 0.14f, bh = hgt * 0.5f + 0.10f;
  mat_set(0.5f, 0.2f);
  pbox_v(v3_add(o, v3_scale(nrm, 0.04f)), v3_scale(u, bw), (v3){{0, bh, 0}}, v3_scale(nrm, 0.04f),
         rgb(0.05f, 0.05f, 0.055f));
  mat_set(0.3f, 0.8f);
  pbox_v(v3_add(o, v3_add(v3_scale(nrm, 0.045f), (v3){{0, bh, 0}})), v3_scale(u, bw + 0.02f),
         (v3){{0, 0.02f, 0}}, v3_scale(nrm, 0.045f), rgb(0.35f, 0.36f, 0.37f));
  pbox_v(v3_add(o, v3_add(v3_scale(nrm, 0.045f), (v3){{0, -bh, 0}})), v3_scale(u, bw + 0.02f),
         (v3){{0, 0.02f, 0}}, v3_scale(nrm, 0.045f), rgb(0.35f, 0.36f, 0.37f));
  g_scene_smat = SMAT_LED;
  mat_set(0.3f, 0.0f);
  v3 tl = v3_add(o, v3_add(v3_scale(u, -len * 0.5f), (v3){{0, hgt * 0.5f, 0}}));
  dot_text(text, tl, v3_scale(u, pitch), (v3){{0, -pitch, 0}}, nrm, 0.62f, 0.082f, led);
  g_scene_smat = 0;
}

static void dec_town(int town) {
  const town_style_t *ts = &g_town;
  uint64_t save = g_rng_dec;
  g_rng_dec = (uint64_t)(g_map_seed ^ 0x70A7u) * 2654435761u + 7;
  float save_cham = g_cham;
  g_cham = 0.0f;
  ao_set(0.0f, 1.0f, 1.0f);
  v3 iron = rgb(0.10f, 0.10f, 0.11f), timber = v3_scale(ts->door, 0.85f);
  v3 trim_l = v3_scale(ts->trim, 1.06f);
  enum { MAX_ANCH = 512 };
  static v3 anch[MAX_ANCH];
  static int anch_s[MAX_ANCH];
  int na = 0;
  int n0 = g_num_solids;
  int n_signs = 0;
  for (int i = 0; i < n0; i++) {
    const solid_t *s = &g_solids[i];
    v3 mn = s->min, mx = s->max;
    float sx = mx.x - mn.x, sy = mx.y - mn.y, sz = mx.z - mn.z;
    // --- PARAPETS: a crown moulding under the coping, outside only -----------
    if (dec_same(s->color, ts->trim) && fabsf(sy - 0.70f) < 0.02f && mn.y > 2.9f &&
        (fabsf(sx - 0.25f) < 0.02f || fabsf(sz - 0.25f) < 0.02f)) {
      int ax = fabsf(sx - 0.25f) < 0.02f ? 0 : 2;
      int o = dec_out_side(s, ax, mn.y - 0.6f);
      if (o) {
        float pl = ax == 0 ? (o > 0 ? mx.x : mn.x) : (o > 0 ? mx.z : mn.z);
        float u0 = ax == 0 ? mn.z : mn.x, u1 = ax == 0 ? mx.z : mx.x;
        float Hb = mn.y, Ht = mx.y;
        // Citadel: a three-step lime cornice. The ports: a single steel coping.
        if (town == 0) {
          dec_fbox(ax, (float)o, pl, u0, u1, Hb - 0.093f, Hb + 0.013f, 0.0f, 0.105f, trim_l);
          dec_fbox(ax, (float)o, pl, u0, u1, Hb - 0.177f, Hb - 0.093f, 0.0f, 0.067f, ts->trim);
          dec_fbox(ax, (float)o, pl, u0, u1, Hb - 0.243f, Hb - 0.177f, 0.0f, 0.033f, trim_l);
        } else {
          dec_fbox(ax, (float)o, pl, u0, u1, Hb - 0.063f, Hb - 0.011f, 0.0f, 0.045f,
                   rgb(0.30f, 0.31f, 0.33f));
        }
        dec_fbox(ax, (float)o, pl, u0 - 0.017f, u1 + 0.017f, Ht - 0.047f, Ht - 0.003f, 0.0f, 0.037f,
                 town == 0 ? trim_l : rgb(0.34f, 0.35f, 0.37f));       // drip edge
        // Cable anchors at both ends of the run, just under the coping.
        for (int e = 0; e < 2 && na < MAX_ANCH; e++) {
          float u = e ? u1 - 0.2f : u0 + 0.2f;
          float d = pl + (float)o * 0.06f;
          anch[na] = ax == 0 ? (v3){{d, Hb - 0.35f, u}} : (v3){{u, Hb - 0.35f, d}};
          anch_s[na++] = i;
        }
      }
      continue;
    }
    // --- WINDOWS: a frame in the reveal, and bars or louvers in front --------
    if (dec_same(s->color, ts->glass) && sy > 1.0f && sy < 1.5f &&
        (sx < 0.32f || sz < 0.32f)) {
      int ax = sx < sz ? 0 : 2;
      int o = dec_out_side(s, ax, (mn.y + mx.y) * 0.5f);
      if (!o) continue;
      float pl = ax == 0 ? (o > 0 ? mx.x : mn.x) : (o > 0 ? mx.z : mn.z);
      float u0 = ax == 0 ? mn.z : mn.x, u1 = ax == 0 ? mx.z : mx.x;
      float y0 = mn.y, y1 = mx.y, fw = 0.055f;
      int ground = y0 < 1.5f;
      float deep = ground ? 0.0f : 0.11f;      // how much recess there is to use
      v3 fc = v3_scale(ts->trim, 0.92f);
      dec_fbox(ax, (float)o, pl, u0, u1, y1 - fw, y1, 0.0f, 0.03f, fc);         // head
      dec_fbox(ax, (float)o, pl, u0, u1, y0, y0 + fw, 0.0f, 0.03f, fc);         // sill
      dec_fbox(ax, (float)o, pl, u0, u0 + fw, y0 + fw, y1 - fw, 0.0f, 0.03f, fc);
      dec_fbox(ax, (float)o, pl, u1 - fw, u1, y0 + fw, y1 - fw, 0.0f, 0.03f, fc);
      if ((u1 - u0) > 1.4f)                                                        // mullion
        dec_fbox(ax, (float)o, pl, (u0 + u1) * 0.5f - 0.025f, (u0 + u1) * 0.5f + 0.025f,
                 y0 + fw, y1 - fw, 0.0f, 0.022f, fc);
      uint32_t r = (uint32_t)(dr(0.0f, 1.0f) * 1000.0f);
      if (town == 0 && ground && r % 2 == 0) {
        // SECURITY BARS on a street-level window: square iron at 12 cm, two
        // flat rails, set 5 cm proud of the glass.
        for (float u = u0 + 0.11f; u < u1 - 0.07f; u += 0.15f)
          dec_fbox(ax, (float)o, pl, u - 0.008f, u + 0.008f, y0 + fw, y1 - fw, 0.035f, 0.051f, iron);
        dec_fbox(ax, (float)o, pl, u0 + fw, u1 - fw, y0 + 0.28f, y0 + 0.31f, 0.031f, 0.055f, iron);
        dec_fbox(ax, (float)o, pl, u0 + fw, u1 - fw, y1 - 0.31f, y1 - 0.28f, 0.031f, 0.055f, iron);
      } else if (town == 0 && deep > 0.0f && r % 4 == 1) {
        // WOODEN LOUVERS, inside the 12 cm reveal: tilted slats, closed half
        // way, so the window reads as shuttered against the sun.
        v3 tl = tint(timber, 0.85f, 1.1f);
        for (float y = y0 + fw + 0.05f; y < y1 - fw - 0.03f; y += 0.09f) {
          float c = 0.045f;
          v3 a, b;
          if (ax == 0) { a = (v3){{pl + (float)o * 0.012f, y - 0.03f, u0 + fw}}; b = (v3){{pl + (float)o * 0.012f + (float)o * c, y + 0.012f, u1 - fw}}; }
          else         { a = (v3){{u0 + fw, y - 0.03f, pl + (float)o * 0.012f}}; b = (v3){{u1 - fw, y + 0.012f, pl + (float)o * 0.012f + (float)o * c}}; }
          // A slat is a tilted board: a box sheared by its basis.
          v3 cen = v3_scale(v3_add(a, b), 0.5f);
          v3 along = ax == 0 ? (v3){{0, 0, (u1 - u0 - 2 * fw) * 0.5f}} : (v3){{(u1 - u0 - 2 * fw) * 0.5f, 0, 0}};
          v3 nrm = ax == 0 ? (v3){{(float)o, 0, 0}} : (v3){{0, 0, (float)o}};
          v3 wv = v3_add(v3_scale(nrm, 0.024f * 0.8f), (v3){{0, -0.024f * 0.6f, 0}});
          v3 tv = v3_add(v3_scale(nrm, 0.006f * 0.6f), (v3){{0, 0.006f * 0.8f, 0}});
          v3 X = along, Y = tv, Z = wv;
          if (v3_dot(v3_cross(X, Y), Z) < 0.0f) Z = v3_scale(Z, -1.0f);
          scene_box(cen, X, Y, Z, tl);
        }
      }
      continue;
    }
    // --- PIPES: flanges every 3 m and the odd handwheel valve ----------------
    if (town == 2 && mn.y > 4.0f && sz > 20.0f && sx < 0.7f && sy < 0.7f) {
      for (float z = mn.z + 1.5f; z < mx.z - 1.0f; z += 3.0f) {
        dec_box((v3){{mn.x - 0.033f, mn.y - 0.033f, z - 0.035f}},
                (v3){{mx.x + 0.033f, mx.y + 0.033f, z + 0.035f}}, v3_scale(s->color, 0.8f));
        // Bolt heads on the flange faces.
        for (int b = 0; b < 4; b++) {
          float bx_ = b & 1 ? mx.x + 0.02f : mn.x - 0.02f, by_ = b & 2 ? mx.y : mn.y;
          dec_box((v3){{bx_ - 0.014f, by_ - 0.014f, z - 0.048f}}, (v3){{bx_ + 0.014f, by_ + 0.014f, z + 0.048f}},
                  rgb(0.20f, 0.20f, 0.21f));
        }
      }
      for (float z = mn.z + 7.5f; z < mx.z - 3.0f; z += 15.0f) {
        float cx = (mn.x + mx.x) * 0.5f, top = mx.y;
        dec_box((v3){{cx - 0.06f, top, z - 0.06f}}, (v3){{cx + 0.06f, top + 0.10f, z + 0.06f}},
                rgb(0.24f, 0.25f, 0.26f));                              // bonnet
        dec_bar((v3){{cx, top + 0.10f, z}}, (v3){{cx, top + 0.26f, z}}, 0.012f, rgb(0.24f, 0.25f, 0.26f));
        v3 wc = rgb(0.62f, 0.12f, 0.08f);                                 // red handwheel
        for (int k = 0; k < 10; k++) {
          float a0 = (float)k * F_TAU / 10.0f, a1 = (float)(k + 1) * F_TAU / 10.0f;
          dec_bar((v3){{cx + cosf(a0) * 0.14f, top + 0.26f, z + sinf(a0) * 0.14f}},
                  (v3){{cx + cosf(a1) * 0.14f, top + 0.26f, z + sinf(a1) * 0.14f}}, 0.011f, wc);
        }
        for (int k = 0; k < 3; k++) {
          float a0 = (float)k * F_TAU / 3.0f;
          dec_bar((v3){{cx, top + 0.26f, z}}, (v3){{cx + cosf(a0) * 0.14f, top + 0.26f, z + sinf(a0) * 0.14f}},
                  0.008f, wc);
        }
      }
      continue;
    }
    // --- VENT GRATES high on the walls of the ports -------------------------
    if (town >= 1 && sy > 3.4f && mn.y < 0.05f && (fabsf(sx - PF_SKIN) < 0.01f || fabsf(sz - PF_SKIN) < 0.01f) &&
        fmaxf(sx, sz) > 1.6f && dr(0.0f, 1.0f) < 0.16f) {
      int ax = fabsf(sx - PF_SKIN) < 0.01f ? 0 : 2;
      int o = dec_out_side(s, ax, 1.5f);
      if (!o) continue;
      float pl = ax == 0 ? (o > 0 ? mx.x : mn.x) : (o > 0 ? mx.z : mn.z);
      float uc = ax == 0 ? (mn.z + mx.z) * 0.5f : (mn.x + mx.x) * 0.5f;
      float w = 0.34f, y0 = 2.55f, y1 = 2.95f;
      v3 gc = rgb(0.30f, 0.31f, 0.32f);
      dec_fbox(ax, (float)o, pl, uc - w, uc + w, y0, y0 + 0.04f, 0.0f, 0.035f, gc);
      dec_fbox(ax, (float)o, pl, uc - w, uc + w, y1 - 0.04f, y1, 0.0f, 0.035f, gc);
      dec_fbox(ax, (float)o, pl, uc - w, uc - w + 0.04f, y0, y1, 0.0f, 0.035f, gc);
      dec_fbox(ax, (float)o, pl, uc + w - 0.04f, uc + w, y0, y1, 0.0f, 0.035f, gc);
      dec_fbox(ax, (float)o, pl, uc - w + 0.04f, uc + w - 0.04f, y0 + 0.04f, y1 - 0.04f, 0.0f, 0.004f,
               rgb(0.04f, 0.04f, 0.045f));                                // the dark duct behind
      for (float y = y0 + 0.07f; y < y1 - 0.05f; y += 0.045f)
        dec_fbox(ax, (float)o, pl, uc - w + 0.04f, uc + w - 0.04f, y, y + 0.012f, 0.004f, 0.028f, gc);
      continue;
    }
    // --- LED SIGNS over the street: shop names in the citadel, berths and
    // units in the ports. A handful per town, above head height on a
    // street-facing wall.
    if (n_signs < 14 && mn.y < 0.05f && sy > 3.6f &&
        (fabsf(sx - PF_SKIN) < 0.01f || fabsf(sz - PF_SKIN) < 0.01f) &&
        fmaxf(sx, sz) > 2.6f && dr(0.0f, 1.0f) < 0.07f) {
      int ax = fabsf(sx - PF_SKIN) < 0.01f ? 0 : 2;
      int o = dec_out_side(s, ax, 1.5f);
      if (o) {
        static const char *TXT[3][7] = {
          {"CAFE", "SOUK", "HOTEL", "BAZAR", "TEA", "PHARMA", "BAKERY"},
          {"BERTH 7", "CUSTOMS", "PORT 12", "DOCK B", "CARGO", "BAR", "MARINE"},
          {"UNIT 4", "GATE B", "DANGER", "PUMP 3", "NO FIRE", "LAB 2", "EXIT"}};
        static const v3 LEDC[3] = {{{1.0f, 0.38f, 0.10f}}, {{0.20f, 0.75f, 1.0f}}, {{1.0f, 0.15f, 0.08f}}};
        const char *t = TXT[town][n_signs % 7];
        float pl = ax == 0 ? (o > 0 ? mx.x : mn.x) : (o > 0 ? mx.z : mn.z);
        float uc = ax == 0 ? (mn.z + mx.z) * 0.5f : (mn.x + mx.x) * 0.5f;
        float half = ((float)dot_len(t) * 0.05f) * 0.5f + 0.16f;
        if (half * 2.0f < fmaxf(sx, sz) - 0.2f) {
          v3 nrm = ax == 0 ? (v3){{(float)o, 0, 0}} : (v3){{0, 0, (float)o}};
          v3 u = ax == 0 ? (v3){{0, 0, (float)-o}} : (v3){{(float)o, 0, 0}};
          v3 c = ax == 0 ? (v3){{pl, 3.05f, uc}} : (v3){{uc, 3.05f, pl}};
          dec_sign(t, c, u, nrm, v3_scale(LEDC[town], 0.92f));
          n_signs++;
          continue;
        }
      }
    }
    // --- RUBBLE at the foot of the citadel's render walls --------------------
    if (town == 0 && mn.y < 0.05f && sy > 2.5f && (fabsf(sx - PF_SKIN) < 0.01f || fabsf(sz - PF_SKIN) < 0.01f) &&
        fmaxf(sx, sz) > 1.0f && dr(0.0f, 1.0f) < 0.10f) {
      int ax = fabsf(sx - PF_SKIN) < 0.01f ? 0 : 2;
      int o = dec_out_side(s, ax, 1.5f);
      if (!o) continue;
      float pl = ax == 0 ? (o > 0 ? mx.x : mn.x) : (o > 0 ? mx.z : mn.z);
      float uc = ax == 0 ? dr(mn.z + 0.3f, mx.z - 0.3f) : dr(mn.x + 0.3f, mx.x - 0.3f);
      // Fallen render and the mud brick under it: the wall's own colour,
      // in a fan against its foot.
      int nst = 5 + (int)dr(0.0f, 6.0f);
      for (int k = 0; k < nst; k++) {
        float uo = dr(-0.45f, 0.45f), to = dr(0.04f, 0.38f) * (1.0f - fabsf(uo));
        float d = pl + (float)o * to;
        v3 at = ax == 0 ? (v3){{d, 0.0f, uc + uo}} : (v3){{uc + uo, 0.0f, d}};
        float rr = dr(0.025f, 0.075f);
        at.y = rr * 0.6f;
        v3 col = k & 1 ? tint(s->color, 0.85f, 1.05f) : tint(rgb(0.46f, 0.32f, 0.22f), 0.8f, 1.1f);
        dec_stone(at, rr * dr(0.9f, 1.6f), rr * 0.6f, rr, dr(0.0f, F_TAU), dr(-0.4f, 0.4f), col, 1.0f);
      }
      continue;
    }
  }
  // --- CABLES between the rooftops ----------------------------------------
  // Each anchor takes at most one cable, and a span has to cross open air
  // (sampled along its sag) between two DIFFERENT buildings.
  static unsigned char used[MAX_ANCH];
  memset(used, 0, sizeof used);
  int cables = 0;
  v3 cc = rgb(0.06f, 0.06f, 0.065f);
  for (int i = 0; i < na && cables < 48; i++) {
    if (used[i] || dr(0.0f, 1.0f) > 0.55f) continue;
    int best = -1; float bd = 1e9f;
    for (int j = 0; j < na; j++) {
      if (j == i || used[j] || anch_s[j] == anch_s[i]) continue;
      v3 d = v3_sub(anch[j], anch[i]);
      float h = sqrtf(d.x * d.x + d.z * d.z);
      if (h < 6.0f || h > 19.0f || fabsf(d.y) > 3.5f) continue;
      float score = fabsf(h - 11.0f) + dr(0.0f, 4.0f);
      if (score >= bd) continue;
      int clear = 1;
      float span = h, sag = 0.03f * span + 0.12f;
      for (int k = 1; k < 10 && clear; k++) {
        float t = (float)k / 10.0f;
        v3 q = v3_lerp(anch[i], anch[j], t);
        q.y -= sag * 4.0f * t * (1.0f - t);
        if (dec_inside(q) || q.y < 3.0f) clear = 0;
      }
      if (clear) { bd = score; best = j; }
    }
    if (best < 0) continue;
    used[i] = used[best] = 1;
    dec_cable(anch[i], anch[best], cc);
    cables++;
  }
  g_cham = save_cham;
  g_rng_dec = save;
}

