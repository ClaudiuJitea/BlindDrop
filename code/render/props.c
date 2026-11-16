// code/render/props.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ===========================================================================
// PROPS — the detailed vehicles and lanterns, built in the footprint of the
// collision boxes their builders hid (see prop_add). Built through the scene
// primitives like the decor and copied into the CAST range of the world mesh:
// a truck has a shadow. Every dimension below is inside the original hull
// except mirror arms and an exhaust stack, which are thinner than any cover.
// ===========================================================================
typedef struct { v3 o, F, L; } pfr_t;   // prop frame: origin, forward, lateral
static v3 PF(const pfr_t *f, float a, float b, float y) {
  return v3_add(f->o, v3_add(v3_scale(f->F, a), v3_add(v3_scale(f->L, b), (v3){{0, y, 0}})));
}
// A box from its centre and three half-extent vectors, made right-handed.
static void pbox_v(v3 c, v3 ex, v3 ey, v3 ez, v3 col) {
  if (v3_dot(v3_cross(ex, ey), ez) < 0.0f) ez = v3_scale(ez, -1.0f);
  scene_box(c, ex, ey, ez, col);
}
static void pbox(const pfr_t *f, float a0, float a1, float b0, float b1, float y0, float y1, v3 col) {
  v3 c = PF(f, (a0 + a1) * 0.5f, (b0 + b1) * 0.5f, (y0 + y1) * 0.5f);
  pbox_v(c, v3_scale(f->F, fabsf(a1 - a0) * 0.5f), (v3){{0, fabsf(y1 - y0) * 0.5f, 0}},
         v3_scale(f->L, fabsf(b1 - b0) * 0.5f), col);
}
// A double-sided quad (glass, slanted panels): the culled side never shows,
// and neither winding can be wrong.
static void dquad(v3 a, v3 b, v3 c, v3 d, v3 col) {
  v3 n = v3_norm(v3_cross(v3_sub(b, a), v3_sub(d, a)));
  scene_quad(a, b, c, d, n, col);
  scene_quad(a, d, c, b, v3_scale(n, -1.0f), col);
}
// A capped cylinder about `ax` (unit), radius r, half-length hl.
static void pcyl(v3 c, v3 ax, float r, float hl, int segs, v3 col) {
  v3 t1, t2;
  axis_frame(ax, &t1, &t2);
  v3 e0 = v3_scale(ax, -hl), e1 = v3_scale(ax, hl);
  for (int i = 0; i < segs; i++) {
    float a0 = (float)i * F_TAU / (float)segs, a1 = (float)(i + 1) * F_TAU / (float)segs;
    v3 r0 = v3_add(v3_scale(t1, cosf(a0) * r), v3_scale(t2, sinf(a0) * r));
    v3 r1 = v3_add(v3_scale(t1, cosf(a1) * r), v3_scale(t2, sinf(a1) * r));
    v3 p0 = v3_add(c, v3_add(r0, e0)), p1 = v3_add(c, v3_add(r1, e0));
    v3 p2 = v3_add(c, v3_add(r1, e1)), p3 = v3_add(c, v3_add(r0, e1));
    v3 nm = v3_norm(v3_add(r0, r1));
    // Outward winding by test rather than by argument.
    if (v3_dot(v3_cross(v3_sub(p1, p0), v3_sub(p3, p0)), nm) > 0.0f) scene_quad(p0, p1, p2, p3, nm, col);
    else scene_quad(p0, p3, p2, p1, nm, col);
    for (int e = 0; e < 2; e++) {      // caps
      v3 cc = v3_add(c, e ? e1 : e0), nn = e ? ax : v3_scale(ax, -1.0f);
      v3 q0 = v3_add(cc, r0), q1 = v3_add(cc, r1);
      if (v3_dot(v3_cross(v3_sub(q0, cc), v3_sub(q1, cc)), nn) > 0.0f) {
        scene_push(cc, nn, col); scene_push(q0, nn, col); scene_push(q1, nn, col);
      } else {
        scene_push(cc, nn, col); scene_push(q1, nn, col); scene_push(q0, nn, col);
      }
    }
  }
}
static void loft_tri(v3 a, v3 b, v3 c, v3 na, v3 nb, v3 nc, v3 col) {
  v3 fn = v3_cross(v3_sub(b, a), v3_sub(c, a));
  if (v3_dot(fn, v3_add(na, v3_add(nb, nc))) >= 0.0f) {
    scene_push(a, na, col); scene_push(b, nb, col); scene_push(c, nc, col);
  } else {
    scene_push(a, na, col); scene_push(c, nc, col); scene_push(b, nb, col);
  }
}
// A cone frustum a -> b (radii r0, r1), open ends unless `caps`.
static void ptaper(v3 a, v3 b, float r0, float r1, int segs, v3 col, int caps) {
  v3 ax = v3_sub(b, a);
  float l = v3_len(ax);
  if (l < 1e-5f) return;
  ax = v3_scale(ax, 1.0f / l);
  v3 t1, t2;
  axis_frame(ax, &t1, &t2);
  float slope = (r0 - r1) / l;
  for (int i = 0; i < segs; i++) {
    float a0 = (float)i * F_TAU / (float)segs, a1 = (float)(i + 1) * F_TAU / (float)segs;
    v3 d0 = v3_add(v3_scale(t1, cosf(a0)), v3_scale(t2, sinf(a0)));
    v3 d1 = v3_add(v3_scale(t1, cosf(a1)), v3_scale(t2, sinf(a1)));
    v3 n0 = v3_norm(v3_add(d0, v3_scale(ax, slope))), n1 = v3_norm(v3_add(d1, v3_scale(ax, slope)));
    v3 p0 = v3_add(a, v3_scale(d0, r0)), p1 = v3_add(a, v3_scale(d1, r0));
    v3 p2 = v3_add(b, v3_scale(d1, r1)), p3 = v3_add(b, v3_scale(d0, r1));
    loft_tri(p0, p1, p2, n0, n1, n1, col);
    loft_tri(p0, p2, p3, n0, n1, n0, col);
    if (caps) {
      v3 mx = v3_scale(ax, -1.0f);
      loft_tri(a, p0, p1, mx, mx, mx, col);
      loft_tri(b, p3, p2, ax, ax, ax, col);
    }
  }
}
// A small UV sphere.
static void pball(v3 c, float r, v3 col) {
  enum { LA = 4, LO = 7 };
  for (int i = 0; i < LA; i++)
    for (int j = 0; j < LO; j++) {
      float t0 = F_PI * (float)i / LA - F_PI_2, t1 = F_PI * (float)(i + 1) / LA - F_PI_2;
      float p0 = F_TAU * (float)j / LO, p1 = F_TAU * (float)(j + 1) / LO;
      v3 n00 = {{cosf(t0) * cosf(p0), sinf(t0), cosf(t0) * sinf(p0)}};
      v3 n01 = {{cosf(t0) * cosf(p1), sinf(t0), cosf(t0) * sinf(p1)}};
      v3 n10 = {{cosf(t1) * cosf(p0), sinf(t1), cosf(t1) * sinf(p0)}};
      v3 n11 = {{cosf(t1) * cosf(p1), sinf(t1), cosf(t1) * sinf(p1)}};
      v3 a = v3_add(c, v3_scale(n00, r)), b = v3_add(c, v3_scale(n01, r));
      v3 d = v3_add(c, v3_scale(n10, r)), e = v3_add(c, v3_scale(n11, r));
      loft_tri(a, d, e, n00, n10, n11, col);
      loft_tri(a, e, b, n00, n11, n01, col);
    }
}
static void prop_truck(const prop_t *pr) {
  // The truck is the Blender model (tools/props_models.py truck_cab) in the
  // cab's paint; the container on its bed is world geometry. Only the amber
  // roof markers are drawn here, lit.
  pfr_t f = {{{pr->x, 0.0f, pr->z}}, pr->ax ? (v3){{0, 0, 1}} : (v3){{1, 0, 0}},
             pr->ax ? (v3){{1, 0, 0}} : (v3){{0, 0, 1}}};
  pmesh_draw(PM_TRUCK, f.o, f.F, f.L, pr->col, rgb(0.07f, 0.07f, 0.075f));
  g_scene_smat = SMAT_LED;
  mat_set(0.3f, 0.0f);
  for (int k = -2; k <= 2; k++)
    pbox(&f, 4.965f, 4.985f, (float)k * 0.22f - 0.035f, (float)k * 0.22f + 0.035f, 3.10f, 3.14f,
         rgb(0.95f, 0.55f, 0.10f));
  g_scene_smat = 0;
}

// THE CAR: a saloon or a single-cab pickup, modelled in Blender
// (tools/props_models.py) in the footprint car() / pickup_truck() gave its
// collision — a hard-surface body cut from side, plan and front profiles, a
// separate glasshouse with framed panes, lined wheelhouses, recessed lamp
// bands, bumpers, shut lines, mirrors and alloy wheels — and placed whole.
static void prop_car(const prop_t *pr) {
  v3 F = pr->ax ? (v3){{0, 0, 1}} : (v3){{1, 0, 0}};
  v3 L = pr->ax ? (v3){{1, 0, 0}} : (v3){{0, 0, 1}};
  if (pr->dir & CAR_FLIP) { F = v3_scale(F, -1.0f); L = v3_scale(L, -1.0f); }
  pmesh_draw(pr->dir & CAR_PICKUP ? PM_PICKUP : PM_SEDAN, (v3){{pr->x, 0.0f, pr->z}}, F, L,
             pr->col, pr->col2);
}
// STREET FURNITURE (street_prop): a mesh turned by quarter turns.
static void prop_mesh(const prop_t *pr) {
  static const v3 FQ[4] = {{{1, 0, 0}}, {{0, 0, 1}}, {{-1, 0, 0}}, {{0, 0, -1}}};
  v3 F = FQ[pr->ax & 3], L = FQ[(pr->ax + 1) & 3];
  if (pr->dir >= 0 && pr->dir < PM_COUNT)
    pmesh_draw(pr->dir, (v3){{pr->x, pr->y, pr->z}}, F, L, pr->col, pr->col2);
}

static void prop_heli(const prop_t *pr) {
  // The airframe is the Blender model (tools/props_models.py helicopter) in
  // the instance's paint; the rotor blades turn, so rescue_scene draws them,
  // and the lamps below are drawn lit.
  pfr_t f = {{{pr->x, 0.0f, pr->z}}, pr->ax ? (v3){{0, 0, 1}} : (v3){{1, 0, 0}},
             pr->ax ? (v3){{1, 0, 0}} : (v3){{0, 0, 1}}};
  pmesh_draw(PM_HELI, f.o, f.F, f.L, pr->col, pr->col2);
  // Lights: red to port, green to starboard, anti-collision beacons, a
  // landing light under the chin. Always lit (SMAT_LED).
  g_scene_smat = SMAT_LED;
  mat_set(0.3f, 0.0f);
  pbox(&f, 0.80f, 0.95f, -1.10f, -1.07f, 2.42f, 2.48f, rgb(0.95f, 0.10f, 0.06f));   // on the housings
  pbox(&f, 0.80f, 0.95f, 1.07f, 1.10f, 2.42f, 2.48f, rgb(0.10f, 0.95f, 0.25f));
  pbox(&f, -1.10f, -0.90f, -0.08f, 0.08f, 3.13f, 3.20f, rgb(0.95f, 0.12f, 0.06f));
  pbox(&f, -11.10f, -10.98f, -0.04f, 0.04f, 4.42f, 4.49f, rgb(0.95f, 0.12f, 0.06f));   // fin tip
  pbox(&f, -10.82f, -10.77f, -0.04f, 0.04f, 2.24f, 2.31f, rgb(0.92f, 0.92f, 0.88f));   // boom end
  g_scene_smat = SMAT_VGLASS;
  pbox(&f, 3.20f, 3.45f, -0.14f, 0.14f, 0.66f, 0.70f, rgb(0.90f, 0.90f, 0.85f));
  g_scene_smat = 0;
}

// A DATE PALM: a trunk that leans and curves, ringed with old leaf bases and
// flaring at the foot; a crown of arching fronds, each a midrib with leaflets
// along both sides, the oldest hanging brown under the green; a fruit cluster.
// col2 carries the palm's height, lean and a seed (it has no other numbers).
static void prop_palm(const prop_t *pr) {
  float h = pr->col2.x, lean = pr->col2.y;
  uint32_t sd = (uint32_t)(int32_t)(pr->x * 131.0f + pr->z * 977.0f) * 2654435761u;
  v3 base = {{pr->x, 0.0f, pr->z}};
  v3 bark0 = rgb(0.36f, 0.27f, 0.19f), bark1 = rgb(0.27f, 0.20f, 0.14f);
  // The trunk centreline: the lean grows toward the top (x, as the boxes had it).
  enum { TS = 16 };
  v3 tp[TS + 1];
  for (int i = 0; i <= TS; i++) {
    float t = (float)i / TS;
    tp[i] = (v3){{base.x + lean * 2.0f * powf(t, 1.6f), h * t, base.z + 0.06f * sinf(t * 3.0f)}};
  }
  mat_set(0.95f, 0.0f);
  for (int i = 0; i < TS; i++) {
    float t0 = (float)i / TS, t1 = (float)(i + 1) / TS;
    float r0 = 0.15f + 0.05f * (1.0f - t0) + (i == 0 ? 0.08f : 0.0f), r1 = 0.15f + 0.05f * (1.0f - t1);
    ptaper(tp[i], tp[i + 1], r0, r1, 8, (i & 1) ? bark1 : bark0, 0);
    // the knob of an old leaf base at every other ring
    if (i & 1) ptaper(tp[i + 1], v3_lerp(tp[i + 1], tp[i], 0.35f), r1 + 0.014f, r1, 8, bark1, 0);
  }
  v3 top = tp[TS];
  ptaper(v3_sub(top, (v3){{0, 0.25f, 0}}), v3_add(top, (v3){{0, 0.25f, 0}}), 0.21f, 0.12f, 10, rgb(0.30f, 0.26f, 0.16f), 1);
  // Dates: a few hanging bunches under the crown.
  mat_set(0.6f, 0.0f);
  for (int k = 0; k < 3; k++) {
    float a = (float)k * 2.1f + (float)(sd & 7);
    for (int d = 0; d < 3; d++)
      pball((v3){{top.x + cosf(a) * (0.24f + 0.05f * (float)d), top.y - 0.30f - 0.09f * (float)d,
                  top.z + sinf(a) * (0.24f + 0.05f * (float)d)}}, 0.06f, rgb(0.55f, 0.30f, 0.08f));
  }
  // The fronds.
  int nf = 13;
  for (int k = 0; k < nf; k++) {
    sd = sd * 1664525u + 1013904223u;
    float jit = (float)(sd >> 8) / 16777216.0f;
    int dead = k >= nf - 3;                      // the lowest hang brown and dry
    float az = (float)k * 2.39996f + jit * 0.4f;
    float elev = dead ? -0.9f : 0.65f - 0.55f * (float)(k % 5) / 4.0f;
    float L = (dead ? 2.0f : 2.6f + 0.6f * jit);
    v3 dir = {{cosf(az) * cosf(elev), sinf(elev), sinf(az) * cosf(elev)}};
    v3 side = v3_norm((v3){{-sinf(az), 0.0f, cosf(az)}});
    v3 green = dead ? rgb(0.42f, 0.32f, 0.18f) : v3_lerp(rgb(0.16f, 0.30f, 0.11f), rgb(0.24f, 0.38f, 0.14f), jit);
    enum { FS = 10 };
    v3 p = v3_add(top, (v3){{0, 0.15f, 0}}), d = dir;
    float seg = L / FS;
    for (int i = 0; i < FS; i++) {
      float u = (float)i / FS, u1 = (float)(i + 1) / FS;
      // gravity bends the midrib down along its length
      d = v3_norm(v3_add(d, (v3){{0, dead ? -0.10f : -0.16f, 0}}));
      v3 q = v3_add(p, v3_scale(d, seg));
      mat_set(0.8f, 0.0f);
      ptaper(p, q, 0.028f * (1.0f - u) + 0.008f, 0.028f * (1.0f - u1) + 0.008f, 4, v3_scale(green, 0.8f), 0);
      if (i > 0) {
        // LEAFLETS: two pairs per segment, narrow pointed blades angled
        // forward along the midrib and folded up into a V, longest mid-frond.
        float len = (dead ? 0.40f : 0.70f) * sinf(F_PI * (0.12f + 0.88f * u)) + 0.06f;
        for (int h2 = 0; h2 < 2; h2++) {
          v3 root = v3_lerp(p, q, 0.25f + 0.5f * (float)h2);
          for (int s2 = -1; s2 <= 1; s2 += 2) {
            v3 out = v3_norm(v3_add(v3_add(v3_scale(side, (float)s2), v3_scale(d, 0.75f)),
                                    (v3){{0, dead ? -0.9f : 0.22f - 0.55f * u, 0}}));
            v3 tip = v3_add(root, v3_scale(out, len));
            v3 wid = v3_scale(d, 0.045f);
            v3 mid = v3_add(v3_lerp(root, tip, 0.45f), (v3){{0, -0.05f * len, 0}});
            mat_set(0.65f, 0.0f);
            v3 lc = s2 > 0 ? green : v3_scale(green, 0.86f);
            dquad(v3_sub(root, wid), v3_add(root, wid), v3_add(tip, v3_scale(wid, 0.15f)),
                  v3_sub(tip, v3_scale(wid, 0.15f)), lc);
            (void)mid;
          }
        }
      }
      p = q;
    }
  }
}

// A 200-LITRE DRUM: ribbed steel, two rolling hoops, rolled chimes at the top
// and bottom, a lid with its two bungs; a little rust at the foot.
static void prop_barrel(const prop_t *pr) {
  v3 c = pr->col, base = {{pr->x, 0.0f, pr->z}}, up = {{0, 1, 0}};
  v3 dark = v3_scale(c, 0.62f), rust = rgb(0.30f, 0.17f, 0.09f);
  mat_set(0.45f, 0.35f);
  ptaper(v3_add(base, (v3){{0, 0.03f, 0}}), v3_add(base, (v3){{0, 0.10f, 0}}), 0.282f, 0.285f, 22, v3_lerp(c, rust, 0.6f), 0);
  ptaper(v3_add(base, (v3){{0, 0.10f, 0}}), v3_add(base, (v3){{0, 0.86f, 0}}), 0.285f, 0.285f, 22, c, 0);
  for (int k = 0; k < 2; k++)
    pcyl(v3_add(base, (v3){{0, k ? 0.60f : 0.30f, 0}}), up, 0.298f, 0.018f, 22, dark);
  pcyl(v3_add(base, (v3){{0, 0.885f, 0}}), up, 0.292f, 0.022f, 22, dark);          // top chime
  pcyl(v3_add(base, (v3){{0, 0.02f, 0}}), up, 0.29f, 0.02f, 22, v3_lerp(dark, rust, 0.5f));
  pcyl(v3_add(base, (v3){{0, 0.87f, 0}}), up, 0.278f, 0.008f, 22, v3_scale(c, 0.85f));   // the lid
  mat_set(0.35f, 0.7f);
  pcyl(v3_add(base, (v3){{0.15f, 0.89f, 0.04f}}), up, 0.035f, 0.012f, 10, rgb(0.55f, 0.55f, 0.56f));
  pcyl(v3_add(base, (v3){{-0.17f, 0.885f, -0.02f}}), up, 0.022f, 0.01f, 10, rgb(0.55f, 0.55f, 0.56f));
}

// A LANTERN: a glass box in a cast frame with a cap, hung from the arm. The
// glass is SMAT_LAMP and lights up at dusk (see the shader's uLampOn).
static void prop_lamp(const prop_t *pr) {
  v3 c = {{pr->x, 3.85f, pr->z}}, m = pr->col2;
  mat_set(0.5f, 0.7f);
  dec_box((v3){{c.x - 0.20f, 3.96f, c.z - 0.20f}}, (v3){{c.x + 0.20f, 4.02f, c.z + 0.20f}}, m);   // cap
  dec_box((v3){{c.x - 0.12f, 4.02f, c.z - 0.12f}}, (v3){{c.x + 0.12f, 4.08f, c.z + 0.12f}}, m);
  dec_box((v3){{c.x - 0.16f, 3.70f, c.z - 0.16f}}, (v3){{c.x + 0.16f, 3.74f, c.z + 0.16f}}, m);   // base ring
  for (int k = 0; k < 4; k++) {                                                                    // corner posts
    float sx = k & 1 ? 0.15f : -0.15f, sz = k & 2 ? 0.15f : -0.15f;
    dec_box((v3){{c.x + sx - 0.012f, 3.74f, c.z + sz - 0.012f}}, (v3){{c.x + sx + 0.012f, 3.96f, c.z + sz + 0.012f}}, m);
  }
  g_scene_smat = SMAT_LAMP;
  mat_set(0.05f, 0.0f);
  dec_box((v3){{c.x - 0.14f, 3.74f, c.z - 0.14f}}, (v3){{c.x + 0.14f, 3.96f, c.z + 0.14f}}, pr->col);
  g_scene_smat = 0;
}

// A SHED'S ROOF STRUCTURE (Foundry). The roof was one flat slab, and from
// inside the shed a third of the frame was a single unbroken grey with nothing
// to say how big the space is or what holds it up (art_review/r2 09_int_foundry).
// Steel I-beam rafters every 2.2 m wall to shell, a tie along the middle, and a
// pendant lamp hung off every second rafter. All of it is above 3.3 m: nobody
// stands, aims or hides there, so render-only cannot lie about cover.
static void prop_roof(const prop_t *pr) {
  float s = pr->dir < 0 ? -1.0f : 1.0f;
  float x0 = s * 13.0f, x1 = s * 24.0f;
  float xa = fminf(x0, x1), xb = fmaxf(x0, x1);
  v3 st = pr->col, dk = pr->col2;
  mat_set(0.58f, 0.35f);
  for (int k = 0; k <= 12; k++) {
    float z = s * (-13.2f + 2.2f * (float)k);
    dec_box((v3){{xa, 3.575f, z - 0.08f}}, (v3){{xb, 3.598f, z + 0.08f}}, st);   // top flange
    dec_box((v3){{xa, 3.380f, z - 0.018f}}, (v3){{xb, 3.575f, z + 0.018f}}, st); // web
    dec_box((v3){{xa, 3.355f, z - 0.08f}}, (v3){{xb, 3.380f, z + 0.08f}}, st);   // bottom flange
  }
  // the tie and a purlin each side of it, under the rafters' bottom flanges
  for (int j = 0; j < 3; j++) {
    float x = s * (15.6f + 2.7f * (float)j);
    dec_box((v3){{x - 0.05f, 3.30f, -14.2f}}, (v3){{x + 0.05f, 3.355f, 14.2f}}, dk);
  }
  // pendant lamps: rod, an enamelled shade, the lamp glass (SMAT_LAMP: lit at dusk)
  for (int k = 1; k <= 11; k += 2) {
    float z = s * (-13.2f + 2.2f * (float)k), x = s * 18.3f;
    dec_box((v3){{x - 0.012f, 3.02f, z - 0.012f}}, (v3){{x + 0.012f, 3.30f, z + 0.012f}}, dk);
    mat_set(0.35f, 0.0f);
    ptaper((v3){{x, 3.04f, z}}, (v3){{x, 2.90f, z}}, 0.07f, 0.24f, 14, rgb(0.20f, 0.30f, 0.24f), 1);
    g_scene_smat = SMAT_LAMP;
    mat_set(0.05f, 0.0f);
    dec_box((v3){{x - 0.07f, 2.86f, z - 0.07f}}, (v3){{x + 0.07f, 2.90f, z + 0.07f}},
            rgb(0.95f, 0.86f, 0.62f));
    g_scene_smat = 0;
    mat_set(0.58f, 0.35f);
  }
}

static void mesh_props(void) {
  int save_floats = g_scene_floats, save_want = g_scene_want;
  int save_peak = g_scene_peak;
  long save_drops = g_scene_drops;
  g_scene_floats = 0;
  g_scene_drops = 0;
  float save_cham = g_cham;
  g_cham = 0.0f;
  g_ao_mul = 1.0f;
  ao_set(0.0f, 1.0f / 0.30f, 0.70f);   // the underside of a chassis is dark
  for (int i = 0; i < g_nprops; i++) {
    const prop_t *pr = &g_props[i];
    if (pr->kind == PROP_TRUCK) prop_truck(pr);
    else if (pr->kind == PROP_CAR) prop_car(pr);
    else if (pr->kind == PROP_LAMP) prop_lamp(pr);
    else if (pr->kind == PROP_HELI) prop_heli(pr);
    else if (pr->kind == PROP_PALM) prop_palm(pr);
    else if (pr->kind == PROP_BARREL) prop_barrel(pr);
    else if (pr->kind == PROP_MESH) prop_mesh(pr);
    else if (pr->kind == PROP_ROOF) prop_roof(pr);
  }
  g_scene_smat = 0;
  g_cham = save_cham;
  ao_off();
  int n = g_scene_floats;
  int cap = (int)(sizeof g_mesh / sizeof g_mesh[0]);
  if (n > PROPS_MAX_VERTS * VERT_FLOATS) n = PROPS_MAX_VERTS * VERT_FLOATS;
  if (g_mesh_floats + n > cap) n = cap - g_mesh_floats;
  if ((n < g_scene_floats || g_scene_drops) && !g_mesh_drops++)
    fprintf(stderr, "mesh: props budget exhausted — geometry dropped\n");
  memcpy(g_mesh + g_mesh_floats, g_scene, (size_t)n * sizeof(float));
  g_mesh_floats += n;
  g_props_verts = n / VERT_FLOATS;
  g_scene_floats = save_floats; g_scene_want = save_want;
  g_scene_peak = save_peak; g_scene_drops = save_drops;
  g_scene_dirty = 1;
}

static void mesh_decor(void) {
  // Its OWN stream, seeded off the map seed. g_rng belongs to the sim's map
  // generation and a single draw from it here would re-roll the layout of every
  // recorded seed for a handful of pebbles.
  g_rng_dec = (uint64_t)(g_map_seed ^ 0xD3C0u) * 2654435761u + 1;
  int save_floats = g_scene_floats, save_want = g_scene_want;
  int save_peak = g_scene_peak;
  long save_drops = g_scene_drops;
  g_scene_floats = 0;
  g_scene_drops = 0;   // a STAGING overflow is a decor drop too (see below)

  const theme_t *th = &g_theme;
  int grass = th->surf != SURF_WET;   // nothing dry grows in standing water
  int drifts = th->surf == SURF_SAND;
  v3 wind = wind_dir();
  float H = ARENA_HALF;
  mat_set(WORLD_GROUND_R, 0.0f);
  // scene_push multiplies its AO by g_ao_mul, which is FIGURE state: a global
  // the corpse bands write per band, and world_upload runs at the top of a
  // frame with whatever the PREVIOUS frame's last band left in it. Borrowing
  // the scene primitives means inheriting their batch state, so it gets stated
  // rather than assumed. (scene_box's other hidden input, g_fig_diss, is
  // declared below this point and is cleared by every one of its own callers.)
  g_ao_mul = 1.0f;
  // A six-centimetre stone does not self-occlude the way a wall does. At 0.62
  // every pebble in the arena came out a near-black chip on bright sand, which
  // is the highest local contrast anything ankle-high should ever have.
  ao_set(0.0f, 1.0f / 0.16f, 0.78f);
  // THE HALF-AND-ROTATE ONLY HOLDS ON A SYMMETRIC MAP. The generated arenas are
  // point-symmetric, so one half is dressed and turned 180 degrees onto the
  // other. The grid towns are not: there the turned copy of a wall's drift
  // lay in the open street with no wall behind it. They are dressed whole.
  int asym = (g_map_id >= 4 && g_map_id <= 6) || g_map_id == MAP_COMPOUND_ID;

  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *s = &g_solids[i];
    if (s->max.y < 0.42f) continue;                 // rubble dresses nothing
    if (s->max.y - s->min.y < 0.42f) continue;
    // A PROP'S HULL IS NOT A WALL. A vehicle's or a cafe table's collision boxes
    // are hidden under the model, and dressing their feet put a pebble fan round
    // a chair leg and a sand drift against a wheel.
    if (g_solid_hide[i]) continue;
    // AND IT HAS TO STAND ON THE GROUND. A stacked top box or a step crate
    // perched on an anchor is a solid like any other, and dressing its foot put
    // a clump of stones at y = 0 — under the box, floating in the open air
    // beneath a crate that starts a metre up.
    if (s->min.y > 0.05f) continue;
    for (int f = 0; f < 4; f++) {
      v3 o = {{f == 0 ? 1.0f : f == 1 ? -1.0f : 0.0f, 0.0f,
               f == 2 ? 1.0f : f == 3 ? -1.0f : 0.0f}};
      float base = f == 0 ? s->max.x : f == 1 ? s->min.x
                 : f == 2 ? s->max.z : s->min.z;
      float lo = (f < 2) ? s->min.z : s->min.x;
      float hi = (f < 2) ? s->max.z : s->max.x;
      if (hi - lo < 0.35f) continue;
      // The lee face gets the drift. One per face at most: a wall banked on all
      // four sides has no wind in it.
      float lee = -(o.x * wind.x + o.z * wind.z);
      if (drifts && lee > 0.35f && hi - lo > 0.9f) {
        float dl = dr(0.30f, 0.62f), dh = dr(0.055f, 0.135f) * lee;
        float c0 = lo + dr(0.05f, 0.35f) * (hi - lo);
        float c1 = hi - dr(0.05f, 0.35f) * (hi - lo);
        if (c1 - c0 > 0.5f) {
          v3 a0 = (f < 2) ? (v3){{base, 0.0f, c0}} : (v3){{c0, 0.0f, base}};
          v3 a1 = (f < 2) ? (v3){{base, 0.0f, c1}} : (v3){{c1, 0.0f, base}};
          if (asym || a0.z + a1.z > 0.0f)
            for (int sg = 0; sg < (asym ? 1 : 2); sg++)
              dec_drift(a0, a1, o, dl, dh,
                        v3_scale(th->ground, 1.06f), sg ? -1.0f : 1.0f);
        }
      }
      // Clump sites along the face. The MASK is what keeps two thirds of the
      // arena bare: a face that scores under the threshold gets nothing at all,
      // and empty stretches are as much of the read as dressed ones.
      for (float t = lo + 0.25f; t < hi - 0.15f;
           t += asym ? dr(1.30f, 2.80f) : dr(0.95f, 2.30f)) {
        v3 p = (f < 2) ? (v3){{base, 0.0f, t}} : (v3){{t, 0.0f, base}};
        if (!asym && p.z <= 0.0f) continue;          // one half, then rotate
        if (fabsf(p.x) > H - 0.4f || fabsf(p.z) > H - 0.4f) continue;
        float m = fnoise2(p.x * 0.21f, p.z * 0.21f) * 0.68f +
                  fnoise2(p.x * 0.62f, p.z * 0.62f) * 0.32f;
        // Corners collect from two directions and get roughly twice the density,
        // which is also where the wall/floor line is hardest to hide.
        float corner = fminf(t - lo, hi - t) < 0.55f ? 0.14f : 0.0f;
        if (m + corner < 0.645f) continue;
        for (int sg = 0; sg < (asym ? 1 : 2); sg++)
          dec_clump(p, o, wind, th, s->color, grass, sg ? -1.0f : 1.0f);
      }
    }
  }
  {
    float save_cham = g_cham;
    g_cham = 0.0f;
    ao_set(0.0f, 1.0f, 1.0f);
    for (int i = 0; i < g_num_solids; i++)
      if (g_solid_mat[i] == SMAT_CONTAINER && !g_solid_hide[i]) {
        const solid_t *cs = &g_solids[i];
        float lx = cs->max.x - cs->min.x, lz = cs->max.z - cs->min.z, ly = cs->max.y - cs->min.y;
        if (ly > 2.3f && ly < 2.9f && fmaxf(lx, lz) > 5.5f && fminf(lx, lz) > 2.0f)
          dec_container(cs, i);
      }
    g_cham = save_cham;
  }
  if (g_map_id >= 4 && g_map_id <= 6) dec_town(g_map_id - 4);
  else if (g_map_id == MAP_COMPOUND_ID) dec_town(0);
  ao_off();

  int n = g_scene_floats;
  int cap = (int)(sizeof g_mesh / sizeof g_mesh[0]);
  if (n > DECOR_MAX_VERTS * VERT_FLOATS) n = DECOR_MAX_VERTS * VERT_FLOATS;
  if (g_mesh_floats + n > cap) n = cap - g_mesh_floats;
  if ((n < g_scene_floats || g_scene_drops) && !g_mesh_drops++)
    fprintf(stderr, "mesh: decor budget exhausted — geometry dropped\n");
  memcpy(g_mesh + g_mesh_floats, g_scene, (size_t)n * sizeof(float));
  g_mesh_floats += n;

  g_scene_floats = save_floats; g_scene_want = save_want;
  g_scene_peak = save_peak; g_scene_drops = save_drops;
  g_scene_dirty = 1;
}

