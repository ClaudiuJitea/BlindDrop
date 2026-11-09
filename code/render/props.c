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
// A road wheel: tyre with a rounded shoulder, steel rim, hub and lug nuts.
// `out` is the lateral direction the wheel's face looks toward.
static void pwheel(v3 c, v3 out, float r, float w, int duals) {
  v3 rub = rgb(0.045f, 0.045f, 0.05f), rim = rgb(0.52f, 0.53f, 0.55f), hub = rgb(0.30f, 0.30f, 0.32f);
  mat_set(0.92f, 0.0f);
  pcyl(c, out, r, w * 0.5f, 20, rub);
  pcyl(v3_add(c, v3_scale(out, w * 0.5f)), out, r * 0.86f, 0.006f, 20, rgb(0.06f, 0.06f, 0.065f)); // sidewall
  mat_set(0.30f, 0.85f);
  pcyl(v3_add(c, v3_scale(out, w * 0.5f + 0.004f)), out, r * 0.58f, 0.010f, 18, rim);
  mat_set(0.40f, 0.75f);
  pcyl(v3_add(c, v3_scale(out, w * 0.5f + 0.016f)), out, r * 0.22f, 0.012f, 12, hub);
  v3 t1, t2;
  axis_frame(out, &t1, &t2);
  int nl = duals ? 8 : 5;
  for (int k = 0; k < nl; k++) {
    float a = (float)k * F_TAU / (float)nl;
    v3 lp = v3_add(c, v3_add(v3_scale(out, w * 0.5f + 0.022f),
                   v3_add(v3_scale(t1, cosf(a) * r * 0.33f), v3_scale(t2, sinf(a) * r * 0.33f))));
    pcyl(lp, out, 0.014f, 0.010f, 6, hub);
  }
}

// ---------------------------------------------------------------------------
// THE LOFT: a smooth body swept through a row of rounded cross-sections.
// Every section is a superellipse (exponent p: 2 an ellipse, 8 a box with
// soft corners) of half-sizes hu x hv, centred (cu, cv) off the loft axis at
// distance t along it; `tw` narrows the upper half (a car's tumblehome). The
// ends can be ROUNDED (rnd): the first and last sections are replaced by a
// quarter-circle run of shrinking copies, so a nose or a roof edge turns over
// instead of ending on a flat cap. Normals are taken from the surface itself
// (central differences), so the body shades as one smooth skin, and every
// quad asks a colour function what it is — paint, glass, a lamp lens, a grille
// — from where it sits on the body and which way it faces. That is how the
// windows, lights and seams are part of the shape rather than boxes on it.
// ---------------------------------------------------------------------------
typedef struct { float t, cu, cv, hu, hv, p, tw; } lsec_t;
typedef struct { v3 A, U, V; } lfr_t;     // loft axis and the section's two axes
typedef struct { v3 col; float rough, metal; int smat; } lmat_t;
// q and n arrive in the PROP frame: x = along F, y = up, z = along L.
typedef lmat_t (*lcol_fn)(const void *ctx, v3 q, v3 n);
#define LOFT_MAXS 64
#define LOFT_MAXR 48
#define LOFT_RND 5
static float sgnpow(float x, float e) { return copysignf(powf(fabsf(x), e), x); }
static v3 loft_local(const pfr_t *f, v3 p) {
  v3 d = v3_sub(p, f->o);
  return (v3){{v3_dot(d, f->F), d.y, v3_dot(d, f->L)}};
}
static v3 loft_dir(const pfr_t *f, v3 n) {
  return (v3){{v3_dot(n, f->F), n.y, v3_dot(n, f->L)}};
}
static void loft_tri(v3 a, v3 b, v3 c, v3 na, v3 nb, v3 nc, v3 col) {
  v3 fn = v3_cross(v3_sub(b, a), v3_sub(c, a));
  if (v3_dot(fn, v3_add(na, v3_add(nb, nc))) >= 0.0f) {
    scene_push(a, na, col); scene_push(b, nb, col); scene_push(c, nc, col);
  } else {
    scene_push(a, na, col); scene_push(c, nc, col); scene_push(b, nb, col);
  }
}
static void loft(const pfr_t *f, const lfr_t *lf, const lsec_t *in, int ns, int ring,
                 float rnd, lcol_fn fn, const void *ctx) {
  static lsec_t S[LOFT_MAXS];
  static v3 P[LOFT_MAXS][LOFT_MAXR], N[LOFT_MAXS][LOFT_MAXR], C[LOFT_MAXS];
  if (ring > LOFT_MAXR) ring = LOFT_MAXR;
  int n = 0;
  for (int i = 0; i < ns && n < LOFT_MAXS - 2 * LOFT_RND; i++) {
    int end = i == 0 ? -1 : i == ns - 1 ? 1 : 0;
    if (rnd > 0.0f && end) {
      for (int k = 0; k < LOFT_RND; k++) {
        // phi 0 = the tip, pi/2 = full size; the run sits INSIDE the length
        int kk = end < 0 ? k : LOFT_RND - 1 - k;
        float phi = (float)kk / (float)(LOFT_RND - 1) * F_PI * 0.5f;
        // A BLUNT end that still closes: the section scales by sin(phi)^0.4,
        // so it turns over over `rnd` of length and ends on a point — no flat
        // cap, whose fan of triangles could only ever take one colour badly.
        lsec_t s = in[i];
        float sc = powf(fmaxf(sinf(phi), 0.0f), 0.4f);
        if (end < 0) s.t = in[i].t + rnd * (1.0f - cosf(phi));
        else         s.t = in[i].t - rnd * (1.0f - cosf(phi));
        s.hu = fmaxf(s.hu * sc, 0.002f);
        s.hv = fmaxf(s.hv * sc, 0.002f);
        S[n++] = s;
      }
    } else {
      S[n++] = in[i];
    }
  }
  for (int i = 0; i < n; i++) {
    const lsec_t *s = &S[i];
    float e = 2.0f / s->p;
    C[i] = v3_add(f->o, v3_add(v3_scale(lf->A, s->t), v3_add(v3_scale(lf->U, s->cu), v3_scale(lf->V, s->cv))));
    for (int j = 0; j < ring; j++) {
      float th = (float)j * F_TAU / (float)ring;
      float x = sgnpow(cosf(th), e) * s->hu, y = sgnpow(sinf(th), e) * s->hv;
      if (y > 0.0f) x *= 1.0f + (s->tw - 1.0f) * (y / s->hv);
      P[i][j] = v3_add(C[i], v3_add(v3_scale(lf->U, x), v3_scale(lf->V, y)));
    }
  }
  for (int i = 0; i < n; i++)
    for (int j = 0; j < ring; j++) {
      v3 dt = v3_sub(P[i < n - 1 ? i + 1 : i][j], P[i > 0 ? i - 1 : i][j]);
      v3 dr = v3_sub(P[i][(j + 1) % ring], P[i][(j + ring - 1) % ring]);
      v3 nn = v3_cross(dr, dt);
      v3 rad = v3_sub(P[i][j], C[i]);
      if (v3_dot(nn, nn) < 1e-12f) nn = v3_dot(rad, rad) > 1e-10f ? rad : lf->A;
      nn = v3_norm(nn);
      if (v3_dot(nn, rad) < 0.0f) nn = v3_scale(nn, -1.0f);
      // A tip ring is a point: its normal is the axis, outward.
      if (S[i].hu < 0.01f && S[i].hv < 0.01f) nn = i == 0 ? v3_scale(lf->A, -1.0f) : lf->A;
      N[i][j] = nn;
    }
  for (int i = 0; i + 1 < n; i++)
    for (int j = 0; j < ring; j++) {
      int j1 = (j + 1) % ring;
      v3 a = P[i][j], b = P[i + 1][j], c = P[i + 1][j1], d = P[i][j1];
      v3 q = v3_scale(v3_add(v3_add(a, b), v3_add(c, d)), 0.25f);
      v3 nq = v3_norm(v3_add(v3_add(N[i][j], N[i + 1][j]), v3_add(N[i + 1][j1], N[i][j1])));
      lmat_t m = fn(ctx, loft_local(f, q), loft_dir(f, nq));
      mat_set(m.rough, m.metal);
      g_scene_smat = m.smat;
      loft_tri(a, b, c, N[i][j], N[i + 1][j], N[i + 1][j1], m.col);
      loft_tri(a, c, d, N[i][j], N[i + 1][j1], N[i][j1], m.col);
    }
  for (int e = 0; e < 2; e++) {           // caps, where an end is not a point
    int i = e ? n - 1 : 0;
    if (S[i].hu < 0.012f && S[i].hv < 0.012f) continue;
    v3 an = e ? lf->A : v3_scale(lf->A, -1.0f);
    // Each fan triangle asks for itself: a cap can span a grille AND paint.
    for (int j = 0; j < ring; j++) {
      v3 p1 = P[i][j], p2 = P[i][(j + 1) % ring];
      v3 qc = v3_add(v3_scale(C[i], 0.2f), v3_scale(v3_add(p1, p2), 0.4f));
      lmat_t m = fn(ctx, loft_local(f, qc), loft_dir(f, an));
      mat_set(m.rough, m.metal);
      g_scene_smat = m.smat;
      loft_tri(C[i], p1, p2, an, an, an, m.col);
    }
  }
  g_scene_smat = 0;
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
static lmat_t LM(v3 col, float rough, float metal, int smat) {
  return (lmat_t){col, rough, metal, smat};
}
#define LGLASS(c) LM(c, 0.04f, 0.0f, SMAT_VGLASS)
// A section for a loft along F: from floor-height y0 to y1, half-width hw.
#define BSEC(a, y0, y1, hw, p, tw) ((lsec_t){(a), 0.0f, ((y0) + (y1)) * 0.5f, (hw), ((y1) - (y0)) * 0.5f, (p), (tw)})

// Alloy spokes over a pwheel: five twin spokes from the hub to the rim.
static void pspokes(v3 c, v3 out, float r, float w) {
  v3 t1, t2;
  axis_frame(out, &t1, &t2);
  mat_set(0.25f, 0.85f);
  v3 al = rgb(0.62f, 0.63f, 0.65f);
  for (int k = 0; k < 5; k++)
    for (int s = -1; s <= 1; s += 2) {
      float a = (float)k * F_TAU / 5.0f + (float)s * 0.09f;
      v3 d = v3_add(v3_scale(t1, cosf(a)), v3_scale(t2, sinf(a)));
      v3 q = v3_cross(out, d);
      pbox_v(v3_add(c, v3_add(v3_scale(out, w * 0.5f + 0.012f), v3_scale(d, r * 0.38f))),
             v3_scale(d, r * 0.17f), v3_scale(q, 0.018f), v3_scale(out, 0.008f), al);
    }
}

// THE TRUCK: a cab-over carrying a container. The cab is lofted UP (sections
// are horizontal slices), so its front, sides and the raked windscreen are all
// one rounded pressing; the glass and grille are coloured onto it. Fenders
// wrap the wheels, the chassis shows behind the cab.
typedef struct { v3 paint; } truck_ctx_t;
static lmat_t truck_cab_col(const void *ctx, v3 q, v3 n) {
  const truck_ctx_t *c = ctx;
  float a = q.x, y = q.y, b = fabsf(q.z);
  v3 gl = rgb(0.06f, 0.08f, 0.09f), rub = rgb(0.03f, 0.03f, 0.035f);
  if (n.y > 0.75f) return LM(c->paint, 0.25f, 0.0f, 0);
  if (n.x > 0.45f) {                                       // the face
    if (y > 1.93f && y < 2.73f && b < 0.99f) return LGLASS(gl);
    if (y > 1.88f && y < 2.78f && b < 1.04f) return LM(rub, 0.6f, 0.0f, 0);   // its rubber
    if (y > 1.18f && y < 1.76f && b < 0.76f) return LM(rgb(0.04f, 0.04f, 0.045f), 0.5f, 0.3f, 0);
  }
  if (b > 1.0f && fabsf(n.z) > 0.55f) {                    // the doors
    if (a > 4.40f && a < 5.10f && y > 1.98f && y < 2.68f) return LGLASS(gl);
    if (a > 4.36f && a < 5.14f && y > 1.94f && y < 2.72f) return LM(rub, 0.6f, 0.0f, 0);
  }
  if (y < 1.10f) return LM(v3_scale(c->paint, 0.5f), 0.4f, 0.0f, 0);
  return LM(c->paint, 0.25f, 0.0f, 0);
}
static void fender(const pfr_t *f, float a, float side, float r, float w, v3 col) {
  // An arc of short plates over a wheel, from the front round to the back.
  for (int k = 0; k < 9; k++) {
    float t0 = F_PI * (0.06f + 0.88f * (float)k / 9.0f), t1 = F_PI * (0.06f + 0.88f * (float)(k + 1) / 9.0f);
    float tm = (t0 + t1) * 0.5f;
    v3 c = PF(f, a + cosf(tm) * r, side * (1.03f), 0.48f + sinf(tm) * r);
    v3 tg = v3_add(v3_scale(f->F, -sinf(tm)), (v3){{0, cosf(tm), 0}});
    v3 rd = v3_add(v3_scale(f->F, cosf(tm)), (v3){{0, sinf(tm), 0}});
    pbox_v(c, v3_scale(tg, r * (t1 - t0) * 0.55f), v3_scale(rd, 0.02f), v3_scale(f->L, w * 0.5f), col);
  }
}
static void prop_truck(const prop_t *pr) {
  pfr_t f = {{{pr->x, 0.0f, pr->z}}, pr->ax ? (v3){{0, 0, 1}} : (v3){{1, 0, 0}},
             pr->ax ? (v3){{1, 0, 0}} : (v3){{0, 0, 1}}};
  v3 cab = pr->col, dk = rgb(0.07f, 0.07f, 0.075f), steel = rgb(0.24f, 0.25f, 0.26f);
  v3 chrome = rgb(0.70f, 0.71f, 0.72f);
  // CHASSIS: two ladder rails and cross members, under where the box sat.
  mat_set(0.55f, 0.6f);
  for (int sd = -1; sd <= 1; sd += 2)
    pbox(&f, -3.6f, 3.6f, (float)sd * 0.52f - 0.06f, (float)sd * 0.52f + 0.06f, 0.70f, 1.00f, steel);
  for (float a = -3.2f; a < 3.4f; a += 1.1f) pbox(&f, a, a + 0.10f, -0.52f, 0.52f, 0.78f, 0.94f, steel);
  pbox(&f, -3.6f, 3.38f, -1.10f, 1.10f, 1.04f, 1.20f, rgb(0.18f, 0.19f, 0.20f));   // the bed
  mat_set(0.35f, 0.0f);
  pbox(&f, -3.60f, 3.38f, -1.105f, -1.095f, 1.06f, 1.10f, rgb(0.85f, 0.70f, 0.15f));
  pbox(&f, -3.60f, 3.38f, 1.095f, 1.105f, 1.06f, 1.10f, rgb(0.85f, 0.70f, 0.15f));
  // Fuel tank, air tanks, battery box, side guards.
  mat_set(0.18f, 0.95f);
  pcyl(PF(&f, 2.15f, -0.86f, 0.80f), f.F, 0.24f, 0.55f, 18, chrome);
  for (int k = 0; k < 2; k++) pcyl(PF(&f, 2.15f + (float)k * 0.42f - 0.42f, -0.86f, 0.80f), f.F, 0.245f, 0.015f, 18, steel);
  mat_set(0.5f, 0.4f);
  pcyl(PF(&f, 0.6f, 0.80f, 0.78f), f.F, 0.13f, 0.40f, 12, rgb(0.30f, 0.31f, 0.33f));
  pcyl(PF(&f, -0.4f, 0.80f, 0.78f), f.F, 0.13f, 0.40f, 12, rgb(0.30f, 0.31f, 0.33f));
  mat_set(0.6f, 0.0f);
  pbox(&f, 1.0f, 1.5f, 0.70f, 1.05f, 0.62f, 0.98f, dk);
  for (int sd = -1; sd <= 1; sd += 2) {
    mat_set(0.5f, 0.5f);
    pbox(&f, -1.40f, 1.60f, (float)sd * 1.02f, (float)sd * 1.06f, 0.48f, 0.52f, steel);
    pbox(&f, -1.40f, 1.60f, (float)sd * 1.02f, (float)sd * 1.06f, 0.74f, 0.78f, steel);
  }
  // THE CAB.
  {
    lfr_t lf = {{{0, 1, 0}}, f.L, f.F};
    truck_ctx_t tc = {cab};
    static const lsec_t CAB[] = {
      {1.00f, 0.0f, 4.36f, 1.12f, 0.94f, 9.0f, 1.0f},
      {1.85f, 0.0f, 4.38f, 1.14f, 0.97f, 9.0f, 1.0f},
      {2.00f, 0.0f, 4.37f, 1.14f, 0.95f, 9.0f, 1.0f},
      {2.45f, 0.0f, 4.35f, 1.13f, 0.91f, 9.0f, 1.0f},
      {2.80f, 0.0f, 4.32f, 1.11f, 0.87f, 8.0f, 1.0f},
      {3.00f, 0.0f, 4.30f, 1.08f, 0.84f, 7.0f, 1.0f},
    };
    loft(&f, &lf, CAB, (int)(sizeof CAB / sizeof CAB[0]), 44, 0.14f, truck_cab_col, &tc);
  }
  // A roof visor with marker lamps, a sun visor over the glass, wipers.
  mat_set(0.3f, 0.0f);
  pbox(&f, 3.70f, 4.90f, -0.95f, 0.95f, 2.96f, 3.26f, v3_scale(cab, 0.85f));
  g_scene_smat = SMAT_LED;
  for (int k = -2; k <= 2; k++)
    pbox(&f, 5.13f, 5.17f, (float)k * 0.22f - 0.04f, (float)k * 0.22f + 0.04f, 2.82f, 2.86f, rgb(0.95f, 0.55f, 0.10f));
  g_scene_smat = 0;
  mat_set(0.5f, 0.2f);
  pbox(&f, 5.05f, 5.30f, -1.02f, 1.02f, 2.76f, 2.80f, dk);
  for (int sd = -1; sd <= 1; sd += 2)
    pbox(&f, 5.30f, 5.33f, (float)sd * 0.45f - 0.40f, (float)sd * 0.45f + 0.10f, 1.96f, 1.98f, dk);
  // Door handles, steps, mirrors.
  for (int sd = -1; sd <= 1; sd += 2) {
    float bs = (float)sd * 1.14f;
    mat_set(0.25f, 0.9f);
    pbox(&f, 4.42f, 4.62f, bs - 0.02f, bs + 0.02f, 1.78f, 1.82f, chrome);
    mat_set(0.6f, 0.5f);
    pbox(&f, 4.30f, 5.05f, (float)sd * 0.92f, (float)sd * 1.10f, 0.72f, 0.76f, steel);
    pbox(&f, 4.30f, 5.05f, (float)sd * 0.92f, (float)sd * 1.10f, 0.98f, 1.02f, steel);
    mat_set(0.4f, 0.6f);
    pbox(&f, 5.20f, 5.24f, (float)sd * 1.12f, (float)sd * 1.34f, 2.42f, 2.45f, dk);
    pbox(&f, 5.20f, 5.24f, (float)sd * 1.12f, (float)sd * 1.34f, 1.98f, 2.01f, dk);
    pbox(&f, 5.12f, 5.22f, (float)sd * 1.30f, (float)sd * 1.38f, 1.95f, 2.55f, dk);
    g_scene_smat = SMAT_VGLASS;
    mat_set(0.03f, 0.0f);
    pbox(&f, 5.11f, 5.115f, (float)sd * 1.305f, (float)sd * 1.375f, 1.98f, 2.52f, rgb(0.5f, 0.55f, 0.6f));
    g_scene_smat = 0;
  }
  // FRONT: grille slats, badge, headlamps, bumper, fog lamps, plate.
  mat_set(0.55f, 0.3f);
  for (float y = 1.24f; y < 1.72f; y += 0.08f) pbox(&f, 5.30f, 5.35f, -0.72f, 0.72f, y, y + 0.03f, steel);
  mat_set(0.2f, 0.9f);
  pbox(&f, 5.31f, 5.36f, -0.18f, 0.18f, 1.78f, 1.83f, chrome);
  for (int sd = -1; sd <= 1; sd += 2) {
    g_scene_smat = 0;
    mat_set(0.10f, 0.7f);
    pbox(&f, 5.28f, 5.33f, (float)sd * 0.78f, (float)sd * 1.04f, 1.22f, 1.42f, rgb(0.80f, 0.80f, 0.76f));
    pbox(&f, 5.28f, 5.33f, (float)sd * 0.78f, (float)sd * 1.04f, 1.44f, 1.50f, rgb(0.95f, 0.55f, 0.10f));
    pbox(&f, 5.43f, 5.46f, (float)sd * 0.70f, (float)sd * 0.90f, 0.74f, 0.84f, rgb(0.92f, 0.90f, 0.80f));
    g_scene_smat = 0;
  }
  mat_set(0.45f, 0.4f);
  pbox(&f, 5.26f, 5.44f, -1.14f, 1.14f, 0.62f, 0.96f, rgb(0.16f, 0.17f, 0.18f));
  mat_set(0.4f, 0.0f);
  pbox(&f, 5.44f, 5.445f, -0.26f, 0.26f, 0.66f, 0.80f, rgb(0.92f, 0.82f, 0.20f));
  // WHEELS under their fenders.
  for (int sd = -1; sd <= 1; sd += 2) {
    v3 out = v3_scale(f.L, (float)sd);
    pwheel(PF(&f, 4.40f, (float)sd * 1.03f, 0.48f), out, 0.48f, 0.30f, 0);
    pwheel(PF(&f, -2.00f, (float)sd * 1.03f, 0.48f), out, 0.48f, 0.30f, 1);
    pwheel(PF(&f, -3.05f, (float)sd * 1.03f, 0.48f), out, 0.48f, 0.30f, 1);
    mat_set(0.6f, 0.0f);
    fender(&f, 4.40f, (float)sd, 0.58f, 0.34f, v3_scale(cab, 0.7f));
    fender(&f, -2.00f, (float)sd, 0.58f, 0.34f, dk);
    fender(&f, -3.05f, (float)sd, 0.58f, 0.34f, dk);
    pbox(&f, -3.62f, -3.58f, (float)sd * 0.80f, (float)sd * 1.16f, 0.18f, 0.82f, dk);     // mud flap
    g_scene_smat = SMAT_VGLASS;
    mat_set(0.04f, 0.0f);
    pbox(&f, -3.62f, -3.60f, (float)sd * 0.70f, (float)sd * 1.00f, 0.86f, 0.98f, rgb(0.85f, 0.08f, 0.05f));
    pbox(&f, -3.62f, -3.60f, (float)sd * 0.55f, (float)sd * 0.68f, 0.86f, 0.98f, rgb(0.95f, 0.55f, 0.10f));
    g_scene_smat = 0;
  }
  // Exhaust stack behind the cab, its heat shield and rain cap; air horns.
  mat_set(0.2f, 0.95f);
  pcyl(PF(&f, 3.30f, 0.92f, 2.20f), (v3){{0, 1, 0}}, 0.065f, 1.10f, 12, chrome);
  mat_set(0.5f, 0.6f);
  pcyl(PF(&f, 3.30f, 0.92f, 2.00f), (v3){{0, 1, 0}}, 0.085f, 0.45f, 12, steel);
  mat_set(0.2f, 0.95f);
  for (int k = 0; k < 2; k++)
    ptaper(PF(&f, 4.2f + (float)k * 0.3f, 0.35f, 3.30f), PF(&f, 4.6f + (float)k * 0.3f, 0.35f, 3.30f), 0.025f, 0.05f, 8, chrome, 1);
}

// ---------------------------------------------------------------------------
// THE PROFILE EXTRUSION: a side silhouette (a, y) pushed across the width with
// ROUNDED shoulders. Every profile vertex grows a strip that runs from the
// left side face, over a quarter-round of radius r, flat across the top of
// that edge, and down the other quarter-round to the right face; neighbouring
// strips make the rim (roof, bonnet, nose, sills, and — the point of it — the
// liners of wheel arches cut INTO the silhouette). The two side faces are the
// silhouette itself, ear-clipped, so a concave outline (arches) is a real cut.
// The rim asks its colour function per quad with the profile SEGMENT and
// whether the quad is on a shoulder (arc) or the flat between them.
// ---------------------------------------------------------------------------
typedef lmat_t (*pext_fn)(const void *ctx, int seg, int arc, v3 q, v3 n);
#define PEXT_MAX 96
#define PEXT_K 4
static int poly_ccw(const float (*p)[2], int n) {
  float a = 0.0f;
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    a += p[i][0] * p[j][1] - p[j][0] * p[i][1];
  }
  return a > 0.0f;
}
// Ear clipping of a simple CCW polygon; emits triangles through `emit`.
static void poly_fill(const pfr_t *f, const float (*p)[2], int n, float b, v3 nrm, v3 col) {
  int idx[PEXT_MAX], m = n;
  for (int i = 0; i < n; i++) idx[i] = i;
  int guard = 0;
  while (m > 2 && guard++ < 4 * PEXT_MAX * PEXT_MAX) {
    int found = 0;
    for (int i = 0; i < m; i++) {
      int i0 = idx[(i + m - 1) % m], i1 = idx[i], i2 = idx[(i + 1) % m];
      float ax = p[i0][0], ay = p[i0][1], bx_ = p[i1][0], by = p[i1][1], cx = p[i2][0], cy = p[i2][1];
      float cr = (bx_ - ax) * (cy - ay) - (by - ay) * (cx - ax);
      if (cr <= 1e-7f) continue;                       // reflex: not an ear
      int inside = 0;
      for (int k = 0; k < m && !inside; k++) {
        int q = idx[k];
        if (q == i0 || q == i1 || q == i2) continue;
        float px = p[q][0], py = p[q][1];
        float d1 = (bx_ - ax) * (py - ay) - (by - ay) * (px - ax);
        float d2 = (cx - bx_) * (py - by) - (cy - by) * (px - bx_);
        float d3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
        inside = d1 > 0.0f && d2 > 0.0f && d3 > 0.0f;
      }
      if (inside) continue;
      loft_tri(PF(f, ax, b, ay), PF(f, bx_, b, by), PF(f, cx, b, cy), nrm, nrm, nrm, col);
      for (int k = i; k < m - 1; k++) idx[k] = idx[k + 1];
      m--;
      found = 1;
      break;
    }
    if (!found) break;
  }
}
static void pext(const pfr_t *f, const float (*in)[2], int n, float hw, float r, pext_fn fn,
                 const void *ctx, v3 side_col, float side_rough) {
  float p[PEXT_MAX][2];
  int ccw = poly_ccw(in, n);
  for (int i = 0; i < n; i++) {                        // CCW in (a, y)
    int s = ccw ? i : n - 1 - i;
    p[i][0] = in[s][0]; p[i][1] = in[s][1];
  }
  // Vertex normals (outward) and the inset the shoulders roll from.
  float nv[PEXT_MAX][2], inner[PEXT_MAX][2];
  for (int i = 0; i < n; i++) {
    int a = (i + n - 1) % n, c = (i + 1) % n;
    float e0x = p[i][0] - p[a][0], e0y = p[i][1] - p[a][1];
    float e1x = p[c][0] - p[i][0], e1y = p[c][1] - p[i][1];
    float l0 = sqrtf(e0x * e0x + e0y * e0y) + 1e-9f, l1 = sqrtf(e1x * e1x + e1y * e1y) + 1e-9f;
    float n0x = e0y / l0, n0y = -e0x / l0, n1x = e1y / l1, n1y = -e1x / l1;
    float mx = n0x + n1x, my = n0y + n1y, ml = sqrtf(mx * mx + my * my) + 1e-9f;
    mx /= ml; my /= ml;
    float cs = fmaxf(mx * n1x + my * n1y, 0.55f);
    nv[i][0] = mx; nv[i][1] = my;
    inner[i][0] = p[i][0] - mx * r / cs;
    inner[i][1] = p[i][1] - my * r / cs;
  }
  enum { COLS = 2 * (PEXT_K + 1) };
  static v3 P[PEXT_MAX][COLS], N[PEXT_MAX][COLS];
  for (int i = 0; i < n; i++)
    for (int j = 0; j < COLS; j++) {
      int right = j > PEXT_K;
      int k = right ? j - (PEXT_K + 1) : j;
      float s = right ? F_PI_2 * (1.0f - (float)k / PEXT_K) : F_PI_2 * (float)k / PEXT_K;
      float sg = right ? 1.0f : -1.0f;
      float a2 = inner[i][0] + nv[i][0] * r * sinf(s), y2 = inner[i][1] + nv[i][1] * r * sinf(s);
      float b = sg * ((hw - r) + r * cosf(s));
      P[i][j] = PF(f, a2, b, y2);
      v3 n2 = v3_add(v3_add(v3_scale(f->F, nv[i][0] * sinf(s)), (v3){{0, nv[i][1] * sinf(s), 0}}),
                     v3_scale(f->L, sg * cosf(s)));
      N[i][j] = v3_norm(n2);
    }
  for (int i = 0; i < n; i++) {
    int i1 = (i + 1) % n;
    int seg = ccw ? i : (n - 2 - i + n) % n;           // the caller's segment index
    for (int j = 0; j + 1 < COLS; j++) {
      v3 a = P[i][j], b = P[i1][j], c = P[i1][j + 1], d = P[i][j + 1];
      v3 q = v3_scale(v3_add(v3_add(a, b), v3_add(c, d)), 0.25f);
      v3 nq = v3_norm(v3_add(v3_add(N[i][j], N[i1][j]), v3_add(N[i1][j + 1], N[i][j + 1])));
      int arc = j != PEXT_K;                           // PEXT_K..PEXT_K+1 is the flat
      lmat_t m = fn(ctx, seg, arc, loft_local(f, q), loft_dir(f, nq));
      mat_set(m.rough, m.metal);
      g_scene_smat = m.smat;
      loft_tri(a, b, c, N[i][j], N[i1][j], N[i1][j + 1], m.col);
      loft_tri(a, c, d, N[i][j], N[i1][j + 1], N[i][j + 1], m.col);
    }
  }
  g_scene_smat = 0;
  mat_set(side_rough, 0.0f);
  poly_fill(f, (const float (*)[2])inner, n, hw, f->L, side_col);
  poly_fill(f, (const float (*)[2])inner, n, -hw, v3_scale(f->L, -1.0f), side_col);
}
// A flat convex polygon on a side face (a window, a seal), both sides of the car.
static void pside(const pfr_t *f, const float (*p)[2], int n, float b, v3 col) {
  for (int sd = -1; sd <= 1; sd += 2) {
    v3 nn = v3_scale(f->L, (float)sd);
    for (int i = 1; i + 1 < n; i++)
      loft_tri(PF(f, p[0][0], (float)sd * b, p[0][1]), PF(f, p[i][0], (float)sd * b, p[i][1]),
               PF(f, p[i + 1][0], (float)sd * b, p[i + 1][1]), nn, nn, nn, col);
  }
}

// THE CAR: a four-door saloon, built the way a car model is: its side
// silhouette, wheel arches cut into it, extruded across the width with rounded
// shoulders; the glasshouse the same way above it, the side glass flush panels
// in their seals, real A- and C-pillars where the glasshouse turns.
typedef struct { v3 paint; int arch0, arch1, arch2, arch3; } car_ctx_t;
#define CAR_ARCH_N 9
static lmat_t car_rim_col(const void *ctx, int seg, int arc, v3 q, v3 n) {
  const car_ctx_t *c = ctx;
  int in_arch = (seg >= c->arch0 && seg < c->arch1) || (seg >= c->arch2 && seg < c->arch3);
  if (in_arch) return arc ? LM(c->paint, 0.22f, 0.0f, 0) : LM(rgb(0.03f, 0.03f, 0.03f), 0.95f, 0.0f, 0);
  if (n.y < -0.6f) return LM(rgb(0.05f, 0.05f, 0.05f), 0.9f, 0.0f, 0);           // the floor pan
  if (q.y < 0.40f && fabsf(q.x) > 1.80f) return LM(rgb(0.07f, 0.07f, 0.075f), 0.7f, 0.0f, 0);   // bumpers
  if (q.y < 0.27f) return LM(rgb(0.07f, 0.07f, 0.075f), 0.7f, 0.0f, 0);         // sills
  return LM(c->paint, 0.22f, 0.0f, 0);
}
static lmat_t car_cabin_col(const void *ctx, int seg, int arc, v3 q, v3 n) {
  const car_ctx_t *c = ctx;
  (void)q; (void)n;
  // seg 1 = windscreen, seg 5 = rear screen (see CABIN below); their
  // shoulders are the pillars.
  if ((seg == 1 || seg == 5) && !arc) return LGLASS(rgb(0.07f, 0.09f, 0.10f));
  return LM(c->paint, 0.22f, 0.0f, 0);
}
static void prop_car(const prop_t *pr) {
  pfr_t f = {{{pr->x, 0.0f, pr->z}}, pr->ax ? (v3){{0, 0, 1}} : (v3){{1, 0, 0}},
             pr->ax ? (v3){{1, 0, 0}} : (v3){{0, 0, 1}}};
  v3 paint = pr->col, dk = rgb(0.05f, 0.05f, 0.055f), chrome = rgb(0.70f, 0.71f, 0.72f);
  // The silhouette, from the rear bumper's foot forward along the sills (the
  // two arches cut up into it), round the nose, back along the bonnet, the
  // belt and the boot.
  float B[PEXT_MAX][2];
  int nb = 0;
  car_ctx_t cc = {paint, 0, 0, 0, 0};
#define BP(x, y) (B[nb][0] = (x), B[nb][1] = (y), nb++)
  BP(-2.10f, 0.42f); BP(-2.05f, 0.30f); BP(-1.85f, 0.23f);
  for (int w = 0; w < 2; w++) {
    float wa = w ? 1.27f : -1.25f, R0 = 0.385f, cy = 0.33f;
    float t0 = F_PI + asinf((cy - 0.23f) / R0), t1 = -asinf((cy - 0.23f) / R0);
    if (w) cc.arch2 = nb; else cc.arch0 = nb;
    for (int k = 0; k < CAR_ARCH_N; k++) {
      float t = t0 + (t1 - t0) * (float)k / (CAR_ARCH_N - 1);
      BP(wa + R0 * cosf(t), cy + R0 * sinf(t));
    }
    if (w) cc.arch3 = nb - 1; else cc.arch1 = nb - 1;
    if (!w) { BP(-0.40f, 0.23f); BP(0.50f, 0.23f); }
  }
  BP(1.85f, 0.23f); BP(2.04f, 0.30f); BP(2.11f, 0.42f); BP(2.13f, 0.56f); BP(2.10f, 0.67f);
  BP(1.90f, 0.75f); BP(1.40f, 0.83f); BP(0.95f, 0.88f); BP(-0.20f, 0.91f); BP(-1.25f, 0.92f);
  BP(-1.80f, 0.93f); BP(-2.02f, 0.90f); BP(-2.11f, 0.80f); BP(-2.13f, 0.62f);
#undef BP
  pext(&f, (const float (*)[2])B, nb, 0.93f, 0.075f, car_rim_col, &cc, paint, 0.22f);
  // The glasshouse: seg 0 the base, 1 windscreen, 2-4 roof, 5 rear screen.
  static const float CABIN[][2] = {
    {1.00f, 0.86f}, {0.93f, 0.90f}, {0.38f, 1.38f}, {0.10f, 1.45f}, {-0.62f, 1.46f},
    {-0.88f, 1.40f}, {-1.32f, 0.93f}, {-1.36f, 0.86f},
  };
  pext(&f, CABIN, (int)(sizeof CABIN / sizeof CABIN[0]), 0.85f, 0.13f, car_cabin_col, &cc, paint, 0.22f);
  // Side glass in its seals; the black B-pillar between the doors.
  static const float SEAL_F[][2] = {{0.88f, 0.92f}, {0.38f, 1.335f}, {0.14f, 1.385f}, {-0.10f, 1.385f}, {-0.10f, 0.92f}};
  static const float GLASS_F[][2] = {{0.83f, 0.95f}, {0.37f, 1.31f}, {0.14f, 1.355f}, {-0.07f, 1.355f}, {-0.07f, 0.95f}};
  static const float SEAL_R[][2] = {{-0.22f, 0.92f}, {-0.22f, 1.385f}, {-0.60f, 1.39f}, {-0.84f, 1.33f}, {-1.16f, 0.98f}, {-1.20f, 0.92f}};
  static const float GLASS_R[][2] = {{-0.25f, 0.95f}, {-0.25f, 1.355f}, {-0.60f, 1.36f}, {-0.82f, 1.31f}, {-1.12f, 0.98f}, {-1.14f, 0.95f}};
  static const float BPIL[][2] = {{-0.10f, 0.92f}, {-0.10f, 1.40f}, {-0.22f, 1.40f}, {-0.22f, 0.92f}};
  mat_set(0.7f, 0.0f);
  pside(&f, SEAL_F, 5, 0.853f, rgb(0.02f, 0.02f, 0.025f));
  pside(&f, SEAL_R, 6, 0.853f, rgb(0.02f, 0.02f, 0.025f));
  pside(&f, BPIL, 4, 0.854f, rgb(0.02f, 0.02f, 0.025f));
  g_scene_smat = SMAT_VGLASS;
  mat_set(0.04f, 0.0f);
  pside(&f, GLASS_F, 5, 0.856f, rgb(0.07f, 0.09f, 0.10f));
  pside(&f, GLASS_R, 6, 0.856f, rgb(0.07f, 0.09f, 0.10f));
  g_scene_smat = 0;
  // Wheels in the arches.
  for (int sd = -1; sd <= 1; sd += 2)
    for (int w = 0; w < 2; w++) {
      v3 out = v3_scale(f.L, (float)sd), wc = PF(&f, w ? 1.27f : -1.25f, (float)sd * 0.80f, 0.32f);
      pwheel(wc, out, 0.32f, 0.22f, 0);
      pspokes(wc, out, 0.32f, 0.22f);
    }
  for (int sd = -1; sd <= 1; sd += 2) {
    float bs = (float)sd;
    // Shut lines (fine), handles, mirrors, a side repeater.
    mat_set(0.6f, 0.0f);
    v3 shut = v3_scale(paint, 0.30f);
    pbox(&f, 0.865f, 0.872f, bs * 0.930f, bs * 0.933f, 0.30f, 0.86f, shut);
    pbox(&f, -0.165f, -0.158f, bs * 0.930f, bs * 0.933f, 0.27f, 0.90f, shut);
    pbox(&f, -1.10f, -1.093f, bs * 0.930f, bs * 0.933f, 0.40f, 0.90f, shut);
    mat_set(0.25f, 0.9f);
    pbox(&f, 0.36f, 0.50f, bs * 0.930f, bs * 0.945f, 0.79f, 0.81f, chrome);
    pbox(&f, -0.66f, -0.52f, bs * 0.930f, bs * 0.945f, 0.79f, 0.81f, chrome);
    mat_set(0.22f, 0.0f);
    pbox(&f, 0.72f, 0.80f, bs * 0.85f, bs * 0.92f, 0.95f, 0.99f, dk);
    pbox(&f, 0.66f, 0.80f, bs * 0.91f, bs * 1.04f, 0.94f, 1.08f, paint);
    g_scene_smat = SMAT_VGLASS;
    mat_set(0.04f, 0.0f);
    pbox(&f, 0.655f, 0.66f, bs * 0.92f, bs * 1.03f, 0.95f, 1.07f, rgb(0.5f, 0.55f, 0.6f));
    g_scene_smat = 0;
    mat_set(0.2f, 0.0f);
    pbox(&f, 1.55f, 1.62f, bs * 0.930f, bs * 0.936f, 0.72f, 0.75f, rgb(0.95f, 0.55f, 0.10f));
    // Lamps: headlamp clusters in the nose, tail lamps in the tail.
    mat_set(0.6f, 0.0f);
    pbox(&f, 2.03f, 2.125f, bs * 0.42f, bs * 0.80f, 0.575f, 0.700f, rgb(0.03f, 0.03f, 0.035f));
    mat_set(0.10f, 0.75f);
    pbox(&f, 2.04f, 2.13f, bs * 0.45f, bs * 0.62f, 0.59f, 0.685f, rgb(0.82f, 0.82f, 0.78f));
    pbox(&f, 2.03f, 2.122f, bs * 0.64f, bs * 0.76f, 0.60f, 0.675f, rgb(0.82f, 0.82f, 0.78f));
    mat_set(0.2f, 0.0f);
    pbox(&f, 2.02f, 2.11f, bs * 0.76f, bs * 0.80f, 0.58f, 0.63f, rgb(0.95f, 0.55f, 0.10f));
    mat_set(0.6f, 0.0f);
    pbox(&f, -2.150f, -2.05f, bs * 0.44f, bs * 0.86f, 0.645f, 0.815f, rgb(0.02f, 0.02f, 0.02f));
    mat_set(0.08f, 0.0f);
    pbox(&f, -2.158f, -2.05f, bs * 0.47f, bs * 0.83f, 0.665f, 0.795f, rgb(0.42f, 0.015f, 0.01f));
    pbox(&f, -2.162f, -2.05f, bs * 0.58f, bs * 0.68f, 0.69f, 0.73f, rgb(0.90f, 0.88f, 0.84f));
    pbox(&f, -2.162f, -2.05f, bs * 0.74f, bs * 0.82f, 0.69f, 0.73f, rgb(0.95f, 0.50f, 0.08f));
  }
  // Grille and badge, plates, wipers, an exhaust tip, an aerial.
  mat_set(0.6f, 0.3f);
  pbox(&f, 2.07f, 2.135f, -0.38f, 0.38f, 0.42f, 0.555f, rgb(0.03f, 0.03f, 0.035f));
  for (float yy = 0.445f; yy < 0.55f; yy += 0.035f)
    pbox(&f, 2.135f, 2.142f, -0.36f, 0.36f, yy, yy + 0.012f, rgb(0.35f, 0.36f, 0.38f));
  mat_set(0.2f, 0.9f);
  pbox(&f, 2.12f, 2.145f, -0.07f, 0.07f, 0.57f, 0.61f, chrome);
  mat_set(0.5f, 0.0f);
  pbox(&f, 2.13f, 2.145f, -0.24f, 0.24f, 0.30f, 0.41f, rgb(0.90f, 0.90f, 0.86f));
  pbox(&f, -2.15f, -2.13f, -0.24f, 0.24f, 0.46f, 0.57f, rgb(0.90f, 0.90f, 0.86f));
  mat_set(0.6f, 0.0f);
  pbox(&f, 0.90f, 0.94f, -0.55f, -0.05f, 0.885f, 0.90f, dk);
  pbox(&f, 0.90f, 0.94f, 0.10f, 0.60f, 0.885f, 0.90f, dk);
  mat_set(0.3f, 0.9f);
  pcyl(PF(&f, -2.04f, 0.55f, 0.29f), f.F, 0.032f, 0.06f, 10, chrome);
  mat_set(0.5f, 0.3f);
  ptaper(PF(&f, -0.95f, 0.0f, 1.43f), PF(&f, -1.15f, 0.0f, 1.62f), 0.008f, 0.004f, 5, dk, 0);
}

// THE HELICOPTER: a twin-engine utility type in olive drab. The fuselage is one
// loft from the rounded nose through the cabin into the tapering tail boom;
// the cockpit glazing (windscreen, chin bubbles, door windows), the open
// cabin door and the anti-glare panel are coloured onto it. The engine
// cowling, exhausts, fin, stabiliser, skids and lights are their own parts.
// The rotors turn, so they are drawn live (rescue_scene), not here.
typedef struct { v3 paint; } heli_ctx_t;
static lmat_t heli_body_col(const void *ctx, v3 q, v3 n) {
  const heli_ctx_t *c = ctx;
  float a = q.x, y = q.y, b = fabsf(q.z);
  v3 gl = rgb(0.15f, 0.19f, 0.21f), fr = rgb(0.09f, 0.10f, 0.08f), in = rgb(0.035f, 0.037f, 0.035f);
  v3 pt = c->paint;
  if (n.y < -0.55f) pt = v3_scale(pt, 0.78f);                   // the belly
  if (a > 2.80f && n.x > 0.12f) {                                // the cockpit glazing
    if (y > 1.52f && y < 2.46f && n.y < 0.90f) {
      if (b < 0.045f) return LM(fr, 0.6f, 0.0f, 0);             // centre post
      if (fabsf(b - 0.66f) < 0.035f) return LM(fr, 0.6f, 0.0f, 0);   // corner posts
      if (fabsf(y - 2.05f) < 0.02f && b > 0.66f) return LM(fr, 0.6f, 0.0f, 0);
      return LGLASS(gl);
    }
    if (a > 3.70f && y > 0.86f && y < 1.40f && b > 0.10f && b < 0.72f) return LGLASS(gl);   // chin
    if (y >= 1.40f && y <= 1.52f) return LM(rgb(0.05f, 0.05f, 0.05f), 0.8f, 0.0f, 0);       // anti-glare lip
  }
  if (fabsf(n.z) > 0.55f && b > 0.6f) {
    if (a > 2.05f && a < 2.90f && y > 1.58f && y < 2.38f && fabsf(a - 2.48f) > 0.02f) return LGLASS(gl);
    if (a > 1.98f && a < 2.97f && y > 0.80f && y < 2.45f && (y < 1.0f || fabsf(a - 2.0f) < 0.03f))
      return LM(fr, 0.6f, 0.0f, 0);                                            // door seams
    if (a > -1.50f && a < 0.95f && y > 0.80f && y < 2.36f) {                 // the open cabin door
      if (a > -1.45f && a < 0.90f && y > 0.85f && y < 2.31f) return LM(in, 0.95f, 0.0f, 0);
      return LM(fr, 0.6f, 0.0f, 0);
    }
    if (a > -2.35f && a < -1.70f && y > 1.66f && y < 2.24f) return LGLASS(gl);  // aft window
  }
  if (a < -9.7f && a > -10.0f) return LM(rgb(0.80f, 0.74f, 0.20f), 0.5f, 0.0f, 0);  // boom band
  return LM(pt, 0.62f, 0.0f, 0);
}
static lmat_t heli_plain_col(const void *ctx, v3 q, v3 n) {
  const heli_ctx_t *c = ctx;
  (void)q;
  return LM(n.y < -0.55f ? v3_scale(c->paint, 0.8f) : c->paint, 0.62f, 0.0f, 0);
}
static lmat_t heli_cowl_col(const void *ctx, v3 q, v3 n) {
  const heli_ctx_t *c = ctx;
  if (fabsf(n.z) > 0.55f && q.x > 0.45f && q.x < 1.10f && q.y > 2.70f)
    return LM(rgb(0.03f, 0.03f, 0.03f), 0.9f, 0.2f, 0);     // intake screens
  if (fabsf(n.z) > 0.55f && q.x > -2.1f && q.x < -0.3f && q.y > 2.68f && q.y < 2.98f &&
      fmodf(q.x + 3.0f, 0.36f) < 0.05f)
    return LM(rgb(0.08f, 0.09f, 0.07f), 0.8f, 0.0f, 0);     // access panel seams
  return LM(v3_scale(c->paint, 0.94f), 0.6f, 0.0f, 0);
}
static void prop_heli(const prop_t *pr) {
  pfr_t f = {{{pr->x, 0.0f, pr->z}}, pr->ax ? (v3){{0, 0, 1}} : (v3){{1, 0, 0}},
             pr->ax ? (v3){{1, 0, 0}} : (v3){{0, 0, 1}}};
  lfr_t lf = {f.F, f.L, {{0, 1, 0}}};
  heli_ctx_t hc = {pr->col};
  v3 stl = rgb(0.20f, 0.21f, 0.22f), dk = rgb(0.06f, 0.065f, 0.06f), soot = rgb(0.07f, 0.06f, 0.05f);
  static const lsec_t BODY[] = {
    BSEC(4.58f, 1.02f, 1.50f, 0.28f, 2.6f, 0.90f),  BSEC(4.32f, 0.84f, 1.78f, 0.68f, 2.8f, 0.85f),
    BSEC(3.92f, 0.72f, 2.06f, 0.94f, 3.0f, 0.82f),  BSEC(3.40f, 0.66f, 2.36f, 1.10f, 3.4f, 0.84f),
    BSEC(2.85f, 0.63f, 2.58f, 1.18f, 4.0f, 0.88f),
    BSEC(2.20f, 0.63f, 2.64f, 1.20f, 4.2f, 0.90f),  BSEC(0.00f, 0.62f, 2.66f, 1.22f, 4.6f, 0.90f),
    BSEC(-1.80f, 0.64f, 2.66f, 1.20f, 4.6f, 0.90f), BSEC(-2.60f, 0.88f, 2.60f, 1.04f, 4.0f, 0.90f),
    BSEC(-3.20f, 1.32f, 2.52f, 0.70f, 3.4f, 0.92f), BSEC(-3.75f, 1.74f, 2.44f, 0.40f, 3.0f, 0.95f),
    BSEC(-4.40f, 1.90f, 2.40f, 0.29f, 2.6f, 1.00f), BSEC(-7.00f, 2.00f, 2.36f, 0.20f, 2.4f, 1.00f),
    BSEC(-10.20f, 2.08f, 2.33f, 0.13f, 2.2f, 1.00f), BSEC(-10.60f, 2.10f, 2.32f, 0.11f, 2.2f, 1.00f),
  };
  loft(&f, &lf, BODY, (int)(sizeof BODY / sizeof BODY[0]), 36, 0.22f, heli_body_col, &hc);
  // The engine cowling and transmission fairing on the cabin roof.
  static const lsec_t COWL[] = {
    BSEC(1.50f, 2.50f, 2.86f, 0.40f, 3.0f, 0.80f), BSEC(1.00f, 2.50f, 3.12f, 0.60f, 4.0f, 0.80f),
    BSEC(-0.60f, 2.50f, 3.18f, 0.64f, 4.5f, 0.82f), BSEC(-2.10f, 2.50f, 3.08f, 0.56f, 4.0f, 0.82f),
    BSEC(-3.00f, 2.45f, 2.80f, 0.34f, 3.0f, 0.85f),
  };
  loft(&f, &lf, COWL, (int)(sizeof COWL / sizeof COWL[0]), 28, 0.18f, heli_cowl_col, &hc);
  // Exhausts, angled up and out of the cowling's tail.
  mat_set(0.85f, 0.3f);
  for (int sd = -1; sd <= 1; sd += 2) {
    v3 a = PF(&f, -2.30f, (float)sd * 0.30f, 2.92f), b = PF(&f, -2.95f, (float)sd * 0.46f, 3.12f);
    ptaper(a, b, 0.12f, 0.15f, 12, soot, 0);
    pcyl(b, v3_norm(v3_sub(b, a)), 0.105f, 0.01f, 12, rgb(0.02f, 0.02f, 0.02f));
  }
  // The mast, the swashplate, the hub and the tail gearbox (the blades turn).
  mat_set(0.35f, 0.8f);
  pcyl(PF(&f, 0.0f, 0.0f, 3.32f), (v3){{0, 1, 0}}, 0.12f, 0.16f, 14, stl);
  pcyl(PF(&f, 0.0f, 0.0f, 3.24f), (v3){{0, 1, 0}}, 0.30f, 0.035f, 18, stl);
  pcyl(PF(&f, 0.0f, 0.0f, 3.52f), (v3){{0, 1, 0}}, 0.20f, 0.06f, 14, dk);
  pball(PF(&f, -10.72f, 0.0f, 3.42f), 0.16f, pr->col);
  // The FIN, swept and tapering, lofted upward; the stabiliser across the boom.
  {
    lfr_t vf = {{{0, 1, 0}}, f.L, f.F};
    static const lsec_t FIN[] = {
      {2.22f, 0.0f, -10.15f, 0.07f, 0.62f, 2.4f, 1.0f}, {3.10f, 0.0f, -10.55f, 0.06f, 0.46f, 2.4f, 1.0f},
      {4.25f, 0.0f, -11.00f, 0.045f, 0.30f, 2.4f, 1.0f},
    };
    loft(&f, &vf, FIN, 3, 16, 0.05f, heli_plain_col, &hc);
    static const lsec_t STAB[] = {
      {-1.45f, 0.0f, 2.20f, 0.30f, 0.035f, 2.4f, 1.0f}, {1.45f, 0.0f, 2.20f, 0.30f, 0.035f, 2.4f, 1.0f},
    };
    lfr_t sf = {f.L, f.F, {{0, 1, 0}}};
    pfr_t sfr = f;
    sfr.o = PF(&f, -7.70f, 0.0f, 0.0f);
    loft(&sfr, &sf, STAB, 2, 14, 0.06f, heli_plain_col, &hc);
  }
  // SKIDS: two tubes with upturned toes, on two bowed cross tubes.
  mat_set(0.4f, 0.7f);
  for (int sd = -1; sd <= 1; sd += 2) {
    float bs = (float)sd * 1.34f;
    ptaper(PF(&f, -2.80f, bs, 0.07f), PF(&f, 2.55f, bs, 0.07f), 0.05f, 0.05f, 10, stl, 1);
    ptaper(PF(&f, 2.55f, bs, 0.07f), PF(&f, 3.00f, bs, 0.30f), 0.05f, 0.045f, 10, stl, 1);
    for (int k = 0; k < 2; k++) {
      float a = k ? 1.55f : -1.45f;
      ptaper(PF(&f, a, bs, 0.07f), PF(&f, a, (float)sd * 1.22f, 0.50f), 0.05f, 0.05f, 10, stl, 1);
      ptaper(PF(&f, a, (float)sd * 1.22f, 0.50f), PF(&f, a, (float)sd * 0.70f, 0.64f), 0.05f, 0.05f, 10, stl, 1);
    }
    pbox(&f, -0.20f, 0.25f, (float)sd * 1.30f, (float)sd * 1.42f, 0.30f, 0.33f, stl);   // boarding step
    // The cabin door rail and handle, the cockpit door handle.
    mat_set(0.5f, 0.6f);
    pbox(&f, -1.60f, 1.30f, (float)sd * 1.185f, (float)sd * 1.215f, 2.40f, 2.44f, dk);
    pbox(&f, 2.30f, 2.42f, (float)sd * 1.18f, (float)sd * 1.215f, 1.48f, 1.52f, dk);
    mat_set(0.4f, 0.7f);
  }
  // Lights: red to port, green to starboard, anti-collision beacons, a
  // landing light under the chin. Always lit (SMAT_LED).
  g_scene_smat = SMAT_LED;
  mat_set(0.3f, 0.0f);
  pbox(&f, 0.80f, 0.95f, -1.19f, -1.15f, 2.56f, 2.62f, rgb(0.95f, 0.10f, 0.06f));
  pbox(&f, 0.80f, 0.95f, 1.15f, 1.19f, 2.56f, 2.62f, rgb(0.10f, 0.95f, 0.25f));
  pbox(&f, -1.10f, -0.90f, -0.08f, 0.08f, 3.13f, 3.20f, rgb(0.95f, 0.12f, 0.06f));
  pbox(&f, -11.08f, -10.95f, -0.05f, 0.05f, 4.25f, 4.32f, rgb(0.95f, 0.12f, 0.06f));
  pbox(&f, -10.62f, -10.58f, -0.05f, 0.05f, 2.18f, 2.26f, rgb(0.92f, 0.92f, 0.88f));
  g_scene_smat = SMAT_VGLASS;
  pbox(&f, 3.20f, 3.45f, -0.14f, 0.14f, 0.66f, 0.70f, rgb(0.90f, 0.90f, 0.85f));
  g_scene_smat = 0;
  // Aerials and the pitot probe.
  mat_set(0.5f, 0.4f);
  pbox(&f, -1.70f, -1.20f, -0.02f, 0.02f, 2.66f, 2.92f, dk);
  pbox(&f, -0.60f, -0.40f, -0.02f, 0.02f, 0.42f, 0.63f, dk);
  ptaper(PF(&f, 4.40f, 0.30f, 1.40f), PF(&f, 4.95f, 0.30f, 1.40f), 0.018f, 0.010f, 6, stl, 1);
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

  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *s = &g_solids[i];
    if (s->max.y < 0.42f) continue;                 // rubble dresses nothing
    if (s->max.y - s->min.y < 0.42f) continue;
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
          if (a0.z + a1.z > 0.0f)
            for (int sg = 0; sg < 2; sg++)
              dec_drift(a0, a1, o, dl, dh,
                        v3_scale(th->ground, 1.06f), sg ? -1.0f : 1.0f);
        }
      }
      // Clump sites along the face. The MASK is what keeps two thirds of the
      // arena bare: a face that scores under the threshold gets nothing at all,
      // and empty stretches are as much of the read as dressed ones.
      for (float t = lo + 0.25f; t < hi - 0.15f; t += dr(0.95f, 2.30f)) {
        v3 p = (f < 2) ? (v3){{base, 0.0f, t}} : (v3){{t, 0.0f, base}};
        if (p.z <= 0.0f) continue;                   // one half, then rotate
        if (fabsf(p.x) > H - 0.4f || fabsf(p.z) > H - 0.4f) continue;
        float m = fnoise2(p.x * 0.21f, p.z * 0.21f) * 0.68f +
                  fnoise2(p.x * 0.62f, p.z * 0.62f) * 0.32f;
        // Corners collect from two directions and get roughly twice the density,
        // which is also where the wall/floor line is hardest to hide.
        float corner = fminf(t - lo, hi - t) < 0.55f ? 0.14f : 0.0f;
        if (m + corner < 0.645f) continue;
        for (int sg = 0; sg < 2; sg++)
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

