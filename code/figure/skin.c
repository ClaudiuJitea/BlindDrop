// code/figure/skin.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// --- figure skinning: swept prisms wrapped around the solved pose ----------
// The pose is a stick figure; the body is a handful of "chains" — polylines of
// nodes, each carrying a cross-section, a roll reference and a color — swept
// into closed low-poly prisms.
//
// Why chains and not a stack of boxes: consecutive rings SHARE their vertex
// positions bit for bit and the ends are closed with fans, so every chain is a
// watertight manifold. There is no seam that can open into a hole when a joint
// bends, and no pair of same-facing coplanar faces anywhere — the two things
// that make stacked boxes flicker and leak.
//
// Bending: where a polyline turns by theta, TWO rings straddle the corner at
// distance r*tan(theta/2) along their own bones. That is exactly the bevel
// distance at which the inside faces meet in a point (r*sin/(1+cos)), so the
// inside can never fold through itself, while the outside opens into one flat
// facet — the knee / elbow / deltoid plate that gives the low-poly read. A
// single mitre ring on the angle bisector would balloon by 1/cos(theta/2) and
// self-intersect past ~90 degrees of bend; a bevel cannot, at any angle.

#define FIG_KMAX 20   // profile vertices
#define FIG_NMAX 10   // nodes per chain

typedef struct {
  v3    p;          // joint position
  v3    col;        // color of the band ARRIVING here (node 0: its end cap)
  v3    bcol;       // color of the bevel facet AT this node (knee/elbow pad)
  float rx, rz;     // cross-section half extents (rx along the chain's roll ref)
  float roll;       // extra roll of the cross-section here (radians) — the
                    // torso uses it to twist the mesh with the spine
  float ao;         // 0 = just the figure's height ramp; >0 scales it for the
                    // band arriving here — armpits, under the helmet brim and
                    // under the carrier are cavities, and no height ramp knows
                    // that
} fnode_t;

typedef struct { v3 c, ny, v[FIG_KMAX]; int k; } fring_t;

// Unit cross-sections, traversed counter-clockwise around the sweep axis (the
// band/cap winding below depends on that orientation).
//
// Two families, and the family decides how the band SHADES:
//   4 / 6 / 8  authored facets — a chamfered box. Flat-shaded, because the
//              facet IS the look: crates, receivers, butt pads.
//   12/16/20   the unit circle. Smooth-shaded (see fig_band), which is what
//              turns a 16-gon into a barrel instead of a visible 16-gon. The
//              circle is generated rather than typed out, so adding a
//              resolution is one entry in FIG_KS.
static const float FIG_FACET[3][8][2] = {
  {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}},
  {{1, 0.52f}, {0, 1}, {-1, 0.52f}, {-1, -0.52f}, {0, -1}, {1, -0.52f}},
  {{1, 0.42f}, {0.42f, 1}, {-0.42f, 1}, {-1, 0.42f},
   {-1, -0.42f}, {-0.42f, -1}, {0.42f, -1}, {1, -0.42f}},
};
static const int FIG_KS[6] = {4, 6, 8, 12, 16, 20};
static float g_prof[6][FIG_KMAX][2];
static int   g_prof_ready;

// Half-step offset (i+0.5): it puts EDGE MIDPOINTS on the cross-section axes
// instead of vertices, so a 12-gon barrel presents a face to the light where a
// vertex would otherwise present a hard edge.
static void fig_prof_init(void) {
  for (int s = 0; s < 3; s++)
    for (int i = 0; i < FIG_KS[s]; i++) {
      g_prof[s][i][0] = FIG_FACET[s][i][0];
      g_prof[s][i][1] = FIG_FACET[s][i][1];
    }
  for (int s = 3; s < 6; s++) {
    int k = FIG_KS[s];
    for (int i = 0; i < k; i++) {
      float a = ((float)i + 0.5f) * F_TAU / (float)k;
      g_prof[s][i][0] = cosf(a);
      g_prof[s][i][1] = sinf(a);
    }
  }
  g_prof_ready = 1;
}

// How many profile vertices the figure being built right now may use. A 16-gon
// and a 12-gon differ by 1.5% of the radius at the flats: on a 0.16 m torso at
// 18 m that is a seventh of a pixel, so this is not an LOD tier that pops — it is
// declining to pay for detail that cannot land on a pixel. Everything within
// conversational distance still gets the full profile, and `figcheck`/`vmcheck`
// run at the default, so the invariants are always proved on the detailed mesh.
static int g_fig_k = FIG_KMAX;
// A CEILING on the tier the next figure may use, in the same batch-state idiom
// as mat_set/ao_set/g_fig_diss. 0 = whatever the distance says. Only the corpse
// pool sets it.
static int g_fig_kcap;

static int fig_pi(int k) {
  return k <= 4 ? 0 : k <= 6 ? 1 : k <= 8 ? 2 : k <= 12 ? 3 : k <= 16 ? 4 : 5;
}
// Round profiles shade smooth; authored facets shade flat. One rule, so a part
// changes its whole look by changing one number.
static int fig_round(int k) { return k >= 12; }

// One ring in an explicit frame: `ny` is the sweep direction, `nx` the profile's
// first axis (already perpendicular to ny).
static fring_t fig_ring(v3 c, v3 ny, v3 nx, float rx, float rz, int k) {
  if (!g_prof_ready) fig_prof_init();
  if (k > g_fig_k) k = g_fig_k < 4 ? 4 : g_fig_k;
  fring_t r;
  r.k = k;
  r.c = c;
  r.ny = ny;
  v3 nz = v3_cross(nx, ny);
  const float(*pr)[2] = g_prof[fig_pi(k)];
  for (int i = 0; i < k; i++)
    r.v[i] = v3_add(c, v3_add(v3_scale(nx, pr[i][0] * rx),
                              v3_scale(nz, pr[i][1] * rz)));
  return r;
}

// Circumradius of a profile: the bevel has to be sized off the FURTHEST
// profile vertex, not off rx/rz, or the corners of the cross-section fold
// through each other on the inside of a sharp bend.
static float fig_rmax(int k, float rx, float rz) {
  if (!g_prof_ready) fig_prof_init();
  const float(*pr)[2] = g_prof[fig_pi(k)];
  float m = 0;
  for (int i = 0; i < k; i++) {
    float x = pr[i][0] * rx, z = pr[i][1] * rz;
    m = fmaxf(m, x * x + z * z);   // sqrt commutes with max: one root suffices
  }
  return sqrtf(m);
}

static float g_fig_y;  // ground height of the figure currently being built
static v3    g_cam_eye;      // where the frame is being viewed from (declared early)

// Quad band a -> b (b is the later ring along the sweep). Normals come from
// the real quad diagonals, so a tapered or sheared band still shades flat and
// correct — no per-vertex normal to average away the facets. A collapsed quad
// (the inside of a bevel that had to be clamped) must still be EMITTED, or the
// surface loses four edges and springs a hole; it covers no pixels anyway.
// Per-vertex normals of ONE ring as seen from ONE of its bands: the cross
// product of the two surface tangents that actually meet at that vertex —
// around the ring, and along the sweep. Exact for a cone, exact for an ellipse,
// and it needs no data the sweep does not already have. (Averaging the two
// adjacent FACE normals is the same thing to first order but wrong on a taper,
// where the two faces are not symmetric about the vertex.)
static void fig_ring_nrm(const fring_t *r, v3 sweep_from, v3 sweep_to, v3 *out) {
  int k = r->k;
  v3 ts = v3_sub(sweep_to, sweep_from);
  for (int i = 0; i < k; i++) {
    v3 x = v3_cross(ts, v3_sub(r->v[(i + 1) % k], r->v[(i + k - 1) % k]));
    out[i] = v3_dot(x, x) > 1e-16f ? v3_norm(x) : r->ny;
  }
}

// A FIGURE THAT IS DISSOLVING LOSES ITS SURFACE IN CHUNKS. This pass has no
// textures and no alpha, so a fade is not available; what IS available is
// dropping geometry, and dropping it on a SPATIAL hash rather than per triangle
// is what makes the body break into pixel-sized pieces instead of shimmering.
// The hash is quantised to 25 mm cells in world space, so a chunk is a chunk at
// every profile tier and the pattern is stable while the corpse lies still —
// and it is deterministic, so two runs of one script dissolve identically.
// 0 = solid, 1 = gone. Only the corpse pool ever sets it.
static float g_fig_diss;
// ...quantised RELATIVE to the body's own origin and hashed with its own seed:
// a world-locked grid makes two overlapping corpses erode in exactly the same
// holes, which reads as one shared hole rather than two bodies.
static v3       g_fig_diss_o;
static uint32_t g_fig_diss_seed;
static int fig_diss_cut(v3 p) {
  if (g_fig_diss <= 0.0f) return 0;
  p = v3_sub(p, g_fig_diss_o);
  // 12.5 cells/m = 80 mm. At 25 mm a cell subtended 2.9 px at 10 m through this
  // FOV, so the body read as THINNING rather than breaking up; 80 mm is 9.4 px
  // at 10 m and still 4.7 at 20, i.e. a chunk stays a chunk to the far end of a
  // 48 m arena.
  uint32_t h = fx_hash(g_fig_diss_seed ^
                       (uint32_t)(int)(p.x * 12.5f) * 73856093u ^
                       (uint32_t)(int)(p.y * 12.5f) * 19349663u ^
                       (uint32_t)(int)(p.z * 12.5f) * 83492791u);
  // ONE THRESHOLD, AND IT MOVES FAST. A travelling front was tried and cut:
  // an edge the eye can follow only earns its keep if it is given time to
  // travel, and time is exactly what a body vanishing should NOT take — it
  // turns a punctuation mark into a scene. Uniform + brief reads as the body
  // being ANNIHILATED; uniform + slow was the fade this started as. The 80 mm
  // hash cell is what survives from both, because that is the pixel.
  return (float)(h >> 8) * (1.0f / 16777216.0f) < g_fig_diss;
}

// Quad band a -> b with EXPLICIT per-vertex normals (round profiles).
static void fig_band_smooth(const fring_t *a, const fring_t *b,
                            const v3 *na, const v3 *nb, v3 col) {
  for (int i = 0; i < a->k; i++) {
    int j = (i + 1) % a->k;
    if (fig_diss_cut(a->v[i])) continue;
    scene_push(a->v[i], na[i], col);
    scene_push(b->v[i], nb[i], col);
    scene_push(b->v[j], nb[j], col);
    scene_push(a->v[i], na[i], col);
    scene_push(b->v[j], nb[j], col);
    scene_push(a->v[j], na[j], col);
  }
}

// Flat band: one normal per quad, from the real quad diagonals, so a tapered or
// sheared band still shades correctly. A collapsed quad (the inside of a bevel
// that had to be clamped) must still be EMITTED, or the surface loses four edges
// and springs a hole; it covers no pixels anyway.
static void fig_band(const fring_t *a, const fring_t *b, v3 col) {
  for (int i = 0; i < a->k; i++) {
    int j = (i + 1) % a->k;
    if (fig_diss_cut(a->v[i])) continue;
    v3 n = v3_cross(v3_sub(b->v[j], a->v[i]), v3_sub(a->v[j], b->v[i]));
    if (v3_dot(n, n) < 1e-12f) n = v3_cross(a->ny, v3_sub(a->v[j], a->v[i]));
    if (v3_dot(n, n) < 1e-12f) n = a->ny;
    scene_quad(a->v[i], b->v[i], b->v[j], a->v[j], v3_norm(n), col);
  }
}

// Flat end cap; sign +1 closes the ring facing along its own sweep direction.
// ...and the SAME broken edge on a round part. A swept prism ends in a flat fan,
// so every barrel, every scope bell, every turret and every grip butt on the
// weapon terminated in a razor rim at exactly 90 degrees — the one edge on a
// real gun that is always crowned, chamfered or radiused, because an unbroken
// rim on a muzzle is a burr that damages a bullet's base. It reads the same way
// a sharp box arris does: with no surface between the side and the face, the end
// of a tube is a hard silhouette with nothing to catch light, which is why the
// scope's two bells rendered as flat discs pasted on a cylinder.
// Driven by the SAME g_cham the box uses, so "the first-person weapon has broken
// edges" is one decision in one place rather than two that can drift apart.
// The chamfer takes material off BOTH ways — the face shrinks by t and the side
// shortens by t, which is what a 45-degree tool actually removes — so a
// chamfered part is never larger than the part it replaces and no clearance in
// gun_build can get tighter because of it.
static void fig_cap(const fring_t *r, float sign, v3 col_in) {
  v3 n = v3_scale(r->ny, sign);
  v3 col = col_in;
  fring_t in;
  const fring_t *f = r;
  if (g_cham > 0.0f) {
    // Mean radius from the profile itself rather than from rx/rz, which this
    // function does not have: the inset is a scale about the ring's centre, so
    // it is uniform on a circle and proportional on an ellipse, and a ring
    // smaller than twice the chamfer keeps its flat cap instead of collapsing
    // into a cone point (2 degenerate triangles per vertex — figcheck's degen
    // tier, and the reason the ratio is checked rather than assumed).
    float rm = 0.0f;
    for (int i = 0; i < r->k; i++) rm += v3_len(v3_sub(r->v[i], r->c));
    rm /= (float)r->k;
    if (rm > g_cham * 2.2f) {
      float sc = 1.0f - g_cham / rm;
      in = *r;
      in.c = v3_add(r->c, v3_scale(n, g_cham));
      for (int i = 0; i < r->k; i++)
        in.v[i] = v3_add(v3_add(r->c, v3_scale(v3_sub(r->v[i], r->c), sc)),
                         v3_scale(n, g_cham));
      // The bevel band, and the same wear argument the box's edges carry: an
      // arris is where a finish is thinnest, so it is a shade brighter than the
      // two surfaces it joins.
      v3 ecol = v3_scale(col_in, 1.16f);
      for (int i = 0; i < r->k; i++) {
        int j = (i + 1) % r->k;
        if (fig_diss_cut(r->v[i])) continue;
        // Outward radial at the band's mid-edge, averaged with the axial
        // normal: a 45-degree bevel points exactly between the side it came
        // from and the face it leads to. The winding is REPAIRED against that
        // normal rather than hand-derived — the ring's own winding depends on
        // the sweep direction AND on `sign`, and getting it wrong emits 1016
        // inside-out triangles, which backface culling renders as holes in
        // every barrel and bell at once (measured, first attempt).
        v3 rad = v3_sub(v3_scale(v3_add(r->v[i], r->v[j]), 0.5f), r->c);
        v3 bn = v3_norm(v3_add(v3_norm(rad), n));
        scene_poly4(r->v[i], r->v[j], in.v[j], in.v[i], bn, ecol);
      }
      f = &in;
    }
  }
  for (int i = 0; i < f->k; i++) {
    int j = (i + 1) % f->k;
    if (fig_diss_cut(f->v[i])) continue;
    if (sign > 0) { scene_push(f->c, n, col); scene_push(f->v[j], n, col); scene_push(f->v[i], n, col); }
    else          { scene_push(f->c, n, col); scene_push(f->v[i], n, col); scene_push(f->v[j], n, col); }
  }
}

// Rotate v by the minimal rotation that maps unit a onto unit b. Antiparallel
// has no minimal rotation; keeping v is still a valid frame vector there.
static v3 v3_rot_align(v3 v, v3 a, v3 b) {
  v3 ax = v3_cross(a, b);
  float s = v3_len(ax);
  if (s < 1e-5f) return v;
  return v3_rot_axis(v, v3_scale(ax, 1.0f / s), atan2f(s, v3_dot(a, b)));
}

// Per-band value jitter SCALE, batch state in the same sense as mat_set/ao_set:
// it belongs to a PART, not to a vertex or a call argument. 1 = the authored
// amount, 0 = off. A DIGIT is not a crate: the faceted amplitude that separates
// a receiver from its barrel puts three visibly different values on a 19 mm
// finger, so the row of them reads as a stack of washers threaded on the
// handguard, and the first/last-band darkening lands a dark ring on the knuckle
// and a pale one on the fingertip. Every other faceted chain in the file still
// wants the full amount, which is why this is a scale and not a deletion.
static float g_fig_jit = 1.0f;

// Sweep a whole chain. Interior nodes get the bevel pair described above,
// except where the turn is negligible — there one shared ring on the bisector
// keeps the band count (and the vertex budget) down.
//
// The cross-section roll is PARALLEL TRANSPORTED from ring to ring (rotation-
// minimizing frame) instead of being re-derived from a reference vector at each
// ring: a reference only has to graze the bone direction once — a leg thrown
// sideways in a slide grazes the body's right axis exactly — and the ring it
// produces there spins, twisting one band against the next until adjacent faces
// fold flat onto each other and flicker. Transport has no such singularity;
// `hint` is consulted for the FIRST ring only, and every chain picks one that
// can never align with its own first bone.
// A unit vector perpendicular to `ax`, seeded from `hint`. The two fallbacks
// are what stop a hint that lies ALONG the axis from producing a zero basis —
// the same four lines were written out at every ring builder.
static v3 fig_perp(v3 ax, v3 hint) {
  v3 nx = v3_sub(hint, v3_scale(ax, v3_dot(hint, ax)));
  if (v3_dot(nx, nx) < 1e-8f) nx = v3_cross(ax, (v3){{0, 1, 0}});
  if (v3_dot(nx, nx) < 1e-8f) nx = v3_cross(ax, (v3){{1, 0, 0}});
  return v3_norm(nx);
}

static void fig_chain(const fnode_t *src, int nsrc, int k, int cap0, int cap1, v3 hint) {
  fnode_t nd[FIG_NMAX];
  int n = 0;
  for (int i = 0; i < nsrc && n < FIG_NMAX; i++)  // drop coincident nodes:
    if (!n || v3_len(v3_sub(src[i].p, nd[n - 1].p)) > 1e-4f) nd[n++] = src[i];
  if (n < 2) return;
  // THE LOD CLAMP BELONGS HERE, NOT NEXT TO THE RING LOOP: the bevel below is
  // sized off fig_rmax(k, ...), i.e. off the circumradius of the profile the
  // rings will ACTUALLY be built with. It used to be applied after this loop,
  // so a chain authored at k=12 and drawn at g_fig_k=6 sized its bevel from the
  // 12-gon while emitting a hexagon — and the hexagon's corner sits 9.1% further
  // out than the 12-gon's, which is exactly the 1.09 margin the bevel carries.
  // Margin cancelled, the two rings of a CLIPPED bevel met at one vertex: the
  // forearm's wrist corner pinched shut on the far tier, 2 degenerate triangles
  // and 4 open edges at k=6 only, in whatever pose the arm happened to be in
  // (figcheck 600 30, seed 1337, t=415 here; the x86_64 build reached the same
  // pose inside the 60-tick CI window and turned the release gate red).
  if (k > g_fig_k) k = g_fig_k < 4 ? 4 : g_fig_k;
  struct { v3 c, ny, col; float rx, rz, roll, ao; int hard; } rr[FIG_NMAX * 2];
  int nr = 0;
  for (int i = 0; i < n; i++) {
    v3 dp = i > 0 ? v3_sub(nd[i].p, nd[i - 1].p) : (v3){{0, 0, 0}};
    v3 dn = i + 1 < n ? v3_sub(nd[i + 1].p, nd[i].p) : (v3){{0, 0, 0}};
    float lp = v3_len(dp), ln = v3_len(dn);
    if (lp > 1e-5f) dp = v3_scale(dp, 1.0f / lp);
    if (ln > 1e-5f) dn = v3_scale(dn, 1.0f / ln);
#define FIG_PUSH(CEN, AXIS, COL) \
  rr[nr].c = (CEN); rr[nr].ny = (AXIS); rr[nr].col = (COL); rr[nr].hard = 1; \
  rr[nr].rx = nd[i].rx; rr[nr].rz = nd[i].rz; rr[nr].roll = nd[i].roll; \
  rr[nr].ao = nd[i].ao > 0.0f ? nd[i].ao : 1.0f; nr++
    if (i == 0) {
      FIG_PUSH(nd[i].p, dn, nd[i].col);
    } else if (i == n - 1) {
      FIG_PUSH(nd[i].p, dp, nd[i].col);
    } else {
      float th = acosf(f_clamp(v3_dot(dp, dn), -1.0f, 1.0f));
      float rm = fig_rmax(k, nd[i].rx, nd[i].rz);
      float e = rm * tanf(fminf(th, 2.55f) * 0.5f) * 1.09f;  // margin over exact
      float lim = 0.46f * fminf(lp, ln);  // a bevel may never eat a whole bone
      float sh = 1.0f;
      if (e > lim) { sh = lim / e; e = lim; }
      // A joint can only be as fat as its bones are long: when the bevel had to
      // be clipped, THIN the cross-section by the same factor instead of letting
      // the inner faces cross. Crossed faces are a fold — two same-facing
      // coplanar triangles fighting for the same pixels.
      if (e < 0.004f) {  // straight enough: one ring, no facet, shade across it
        FIG_PUSH(nd[i].p, v3_norm(v3_add(dp, dn)), nd[i].col);
        rr[nr - 1].hard = 0;
      } else {
        // The bevel pair below is GEOMETRY — it has to exist or the inner faces
        // fold through each other. Whether it also reads as a hard EDGE is a
        // separate question, and it was being answered by the bevel distance:
        // anything past 4 degrees of turn got a crease. On a dome that is every
        // node, because a dome is nothing but small turns — which is why the
        // helmet, the skull and the torso all came out as stacks of visibly
        // separate bands. A crease belongs at a real corner, so it is now decided
        // by the ANGLE: under ~34 degrees the two rings still straddle the corner
        // but their normals are averaged and the surface reads as one curve.
        // (Knees, elbows and shoulders are balls now, so almost nothing left in
        // the figure turns hard enough to want the crease.)
        int soft = th < 0.60f;
        FIG_PUSH(v3_add(nd[i].p, v3_scale(dp, -e)), dp, nd[i].col);
        rr[nr - 1].rx *= sh; rr[nr - 1].rz *= sh;
        rr[nr - 1].hard = !soft;
        FIG_PUSH(v3_add(nd[i].p, v3_scale(dn, e)), dn, nd[i].bcol);  // the facet
        rr[nr - 1].rx *= sh; rr[nr - 1].rz *= sh;
        rr[nr - 1].hard = !soft;
      }
    }
#undef FIG_PUSH
  }
  if (nr < 2) return;
  fring_t rings[FIG_NMAX * 2];
  v3 nx = fig_perp(rr[0].ny, hint);
  for (int i = 0; i < nr; i++) {
    if (i) {
      nx = v3_rot_align(nx, rr[i - 1].ny, rr[i].ny);
      nx = v3_sub(nx, v3_scale(rr[i].ny, v3_dot(nx, rr[i].ny)));  // re-orthogonalize
      nx = v3_dot(nx, nx) > 1e-8f ? v3_norm(nx) : v3_norm(v3_cross(rr[i].ny, nx));
    }
    v3 rx = rr[i].roll != 0.0f ? v3_rot_axis(nx, rr[i].ny, rr[i].roll) : nx;
    rings[i] = fig_ring(rr[i].c, rr[i].ny, rx, rr[i].rx, rr[i].rz, k);
  }
  // Per-band variation. Every band carried ONE value, which is most of why a
  // barrel and a handguard read as the same extrusion: real hardware has parts
  // that were finished separately and joints that sit in their own shadow. Each
  // band's value is jittered a few percent off a hash of its index, and the first
  // and last — the ones that bury into whatever the chain grows out of — are
  // darkened. Colour only, no vertex moves, so it cannot regress a manifold.
  for (int i = 0; i < nr; i++) {
    unsigned hh = ((unsigned)i * 2654435761u) ^ 0x9e3779b9u;
    hh ^= hh >> 13;
    // Amplitude by PROFILE FAMILY. 4/6/8 are authored facets where the band IS
    // the look and a few percent of value variation separates a crate from an
    // extrusion. 12/16/20 are meant to read as one continuous curved surface,
    // and the same +/-3.8% there defeats the whole point of smooth-shading them:
    // a helmet, a skull and a limb each came out as a stack of visibly separate
    // rings, which is the single most "generated" thing left in the shading.
    float amp = (fig_round(k) ? 0.020f : 0.076f) * g_fig_jit;
    float jit = (1.0f - amp * 0.5f) + amp * (float)(hh & 255u) * (1.0f / 255.0f);
    float ed = i == 1 ? (fig_round(k) ? 0.94f : 0.82f)
             : (i == nr - 1 && nr > 2) ? (fig_round(k) ? 0.96f : 0.90f) : 1.0f;
    jit *= 1.0f - (1.0f - ed) * g_fig_jit;
    rr[i].col = v3_scale(rr[i].col, jit);
  }
  g_ao_mul = rr[0].ao;
  if (cap0) fig_cap(&rings[0], -1.0f, rr[0].col);
  if (!fig_round(k)) {
    for (int i = 1; i < nr; i++) {
      g_ao_mul = rr[i].ao;
      fig_band(&rings[i - 1], &rings[i], rr[i].col);
    }
  } else {
    // Round profiles shade as the surface of revolution they approximate, and
    // that has to include the sweep DIRECTION, not just the circumference: a
    // head or a tapering limb changes radius from ring to ring, so two bands
    // meeting at a shared ring compute different normals there and leave a
    // visible horizontal seam at every node. Averaging across a ring that is
    // NOT a crease removes them. Bevel-pair rings (rr[].hard, set where the
    // polyline genuinely turns) keep their own side's normal, so a knee, an
    // elbow and the shoulder of a barrel step still read as hard edges.
    v3 NA[FIG_NMAX * 2][FIG_KMAX], NB[FIG_NMAX * 2][FIG_KMAX];
    for (int i = 1; i < nr; i++) {
      fig_ring_nrm(&rings[i - 1], rings[i - 1].c, rings[i].c, NA[i]);
      fig_ring_nrm(&rings[i], rings[i - 1].c, rings[i].c, NB[i]);
    }
    for (int i = 1; i < nr - 1; i++)
      if (!rr[i].hard || !rr[i + 1].hard)
        for (int j = 0; j < k; j++) {
          v3 av = v3_add(NB[i][j], NA[i + 1][j]);
          if (v3_dot(av, av) > 1e-12f) {
            av = v3_norm(av);
            NB[i][j] = av;
            NA[i + 1][j] = av;
          }
        }
    for (int i = 1; i < nr; i++) {
      g_ao_mul = rr[i].ao;
      fig_band_smooth(&rings[i - 1], &rings[i], NA[i], NB[i], rr[i].col);
    }
  }
  g_ao_mul = rr[nr - 1].ao;
  if (cap1) fig_cap(&rings[nr - 1], 1.0f, rr[nr - 1].col);
  g_ao_mul = 1.0f;
}

// A closed ellipsoid, for JOINTS. This exists because a swept tube cannot bend
// far without either folding or pinching, and that is not a tuning problem, it is
// geometry: a tube of radius r turning by theta needs r*tan(theta/2) of bone on
// each side to fillet the corner without its inner surface crossing itself. A
// 0.072 m knee at 146 degrees of flex needs 0.235 m — the shin only has 0.14 m to
// give. fig_chain's answer was to THIN the cross-section by whatever factor was
// missing, which is why a deeply bent knee collapsed to a pair of cone points
// with the pad floating off beside it.
//
// The robust construction, and what capsule-skinned characters have always done:
// end both bones AT the joint and put a ball over the junction. Each limb segment
// is then a straight tube with no corner to fillet, the two flat end caps are
// buried inside the ball, and the union is closed at ANY joint angle — 0 degrees
// or 150, standing or in a slide. The ball also happens to be the right shape for
// the thing that lives on a knee and an elbow, so it IS the pad and the separate
// pad chains (which straddled two rotating directions and detached from the limb
// the moment it bent) are gone with it.
//
// Normals are the exact ellipsoid normal (x/rx^2, y/ry^2, z/rz^2), so the joint
// shades as a smooth sphere rather than as a stack of rings.
#define FIG_BALL_LAT 6   // enough that a knee's outline reads round
static void fig_ball(v3 c, float rx, float ry, float rz, v3 ax, v3 hint,
                     int k, v3 col) {
  // The six joint balls are the most visible residue a dissolve can leave: they
  // are the only near-spherical masses on the figure, so three dark spheres
  // hanging in mid-air is what a chain-only cull looks like.
  if (fig_diss_cut(c)) return;
  if (!g_prof_ready) fig_prof_init();
  if (k > g_fig_k) k = g_fig_k < 4 ? 4 : g_fig_k;
  ax = v3_norm(ax);
  v3 nx = fig_perp(ax, hint);
  v3 nz = v3_cross(nx, ax);
  const float(*pr)[2] = g_prof[fig_pi(k)];
  v3 v[FIG_BALL_LAT][FIG_KMAX], n[FIG_BALL_LAT][FIG_KMAX];
  for (int i = 0; i < FIG_BALL_LAT; i++) {
    float lat = -F_PI_2 + F_PI * (float)(i + 1) / (FIG_BALL_LAT + 1);
    float cl = cosf(lat), sl = sinf(lat);
    v3 cen = v3_add(c, v3_scale(ax, ry * sl));
    for (int j = 0; j < k; j++) {
      float ox = pr[j][0] * rx * cl, oz = pr[j][1] * rz * cl;
      v[i][j] = v3_add(cen, v3_add(v3_scale(nx, ox), v3_scale(nz, oz)));
      v3 g = v3_add(v3_scale(nx, ox / (rx * rx)),
             v3_add(v3_scale(nz, oz / (rz * rz)), v3_scale(ax, ry * sl / (ry * ry))));
      n[i][j] = v3_norm(g);
    }
  }
  v3 sp = v3_sub(c, v3_scale(ax, ry)), np = v3_add(c, v3_scale(ax, ry));
  v3 sn = v3_scale(ax, -1.0f);
  for (int j = 0; j < k; j++) {
    int j2 = (j + 1) % k;
    scene_push(sp, sn, col); scene_push(v[0][j], n[0][j], col);
    scene_push(v[0][j2], n[0][j2], col);
    for (int i = 0; i + 1 < FIG_BALL_LAT; i++) {
      scene_push(v[i][j], n[i][j], col);
      scene_push(v[i + 1][j], n[i + 1][j], col);
      scene_push(v[i + 1][j2], n[i + 1][j2], col);
      scene_push(v[i][j], n[i][j], col);
      scene_push(v[i + 1][j2], n[i + 1][j2], col);
      scene_push(v[i][j2], n[i][j2], col);
    }
    int L = FIG_BALL_LAT - 1;
    scene_push(np, ax, col); scene_push(v[L][j2], n[L][j2], col);
    scene_push(v[L][j], n[L][j], col);
  }
}

// A JOINT IS NOT A BEARING. Every flexing joint on this figure was a near-round
// ball — the shoulder 132 x 124 x 136 mm at k=16, i.e. a smooth sphere on an
// otherwise 8-faceted body, which is exactly why it read as an action-figure
// ball joint. The file had tried to fix that twice with COLOUR (recolour the
// ball to the sleeve, then take 12% of value off it); colour cannot fix a
// shape, and the shape was the problem.
//
// What a joint actually is: a SWELLING OF THE LIMB that changes shape as it
// bends. Three properties, and all three come out of the two bone directions
// rather than out of a table:
//   * ORIENTED ALONG THE LIMB. The long axis is the bisector of the two bones,
//     so a straight joint is an ellipsoid buried inside its own limb and shows
//     nothing at all. A sphere has no long axis, which is why it always shows.
//   * SHORTER WHEN BENT. Soft tissue over a folding joint bunches: the mass
//     stops running along the limb and starts standing out from it. 30% off the
//     long axis at full flex.
//   * SWOLLEN ON THE OUTSIDE. The elbow point, the kneecap, the deltoid — all
//     of them are the OUTSIDE of a bend. The in-plane radius grows with the
//     flex and the centre slides the same way, so the outside gains mass and
//     the inside does not (an ellipsoid is symmetric; the offset is what makes
//     it one-sided).
// This is the "elastic surface at every joint" in the only form this renderer
// can express cheaply — fig_chain's own bevel cannot do it, because a joint
// past ~90 degrees has to pay for a fillet out of bone length it does not have
// and fig_chain pays by thinning the cross-section to nothing.
//
// `da` points INTO the joint along the parent bone, `db` OUT of it along the
// child. `rs` is the half-width across the flex axis (this one has to keep
// swallowing the tube ends it covers, so it is the size floor), `rl` the
// half-length along the limb, `swell` how much the outside gains at full flex.
static void fig_joint(v3 c, v3 da, v3 db, float rs, float rl, float swell,
                      int k, v3 col) {
  da = v3_norm(da);
  db = v3_norm(db);
  v3 bis = v3_add(da, db);
  if (v3_dot(bis, bis) < 1e-6f) bis = da;      // folded flat: keep the parent
  bis = v3_norm(bis);
  v3 flex = v3_cross(da, db);                  // the hinge axis, zero when straight
  if (v3_dot(flex, flex) < 1e-8f) {
    // Straight: any axis perpendicular to the limb will do, and it has to be
    // chosen deterministically or the ellipsoid's facets spin as the pose
    // passes through zero flex.
    flex = v3_cross(bis, (v3){{0, 1, 0}});
    if (v3_dot(flex, flex) < 1e-8f) flex = v3_cross(bis, (v3){{1, 0, 0}});
  }
  flex = v3_norm(flex);
  // 0 straight, 1 folded to a right angle and beyond. dot(da, db) is +1 straight
  // and 0 at 90 degrees, so this is the same number the bevel above measures.
  float bend = f_clamp(1.0f - v3_dot(da, db), 0.0f, 1.0f);
  v3 out = v3_norm(v3_sub(da, db));            // away from the fold
  fig_ball(v3_add(c, v3_scale(out, rs * 0.22f * bend)),
           rs, rl * (1.0f - 0.30f * bend), rs * (1.0f + swell * bend),
           bis, flex, k, col);
}

// One hand, CLOSED AROUND A CYLINDER. The figure and the viewmodel both call
// this, because a hand built twice is a hand that ends up different in the two
// places the player sees it — and the viewmodel's is the closest geometry to the
// camera in the whole game.
//
// Every version of this before it was authored in the HAND's own frame — a palm
// slab, a knuckle ball and finger stubs at hand-tuned millimetre offsets — and
// then parked next to the thing it was supposed to be holding. That can only
// read as a grip from the one angle it was tuned on, and the turntable
// (`vmorbit`) says so in one frame: from in front of the muzzle the support
// "hand" was three brown wedges lying beside the handguard with a blue cone
// behind them, and from below there was no hand at all.
//
// A grip is not a pose, it is a CONSTRAINT: the digits lie ON the surface of the
// thing held. So the hand is built in the GRIPPED OBJECT's frame — a cylinder
// (a point `gc` on its axis, the axis `ga`, radius `gr`) — and every digit is an
// arc about that axis at the radius the surface actually has. Move the hand,
// change the weapon, swap the handguard for a pistol grip: the fingers follow the
// object instead of being aimed at it, and the same function wraps both.
//
// `wrist` stays the CALLER's authority — the viewmodel derives it from the gun,
// the third-person figure gets it from two-bone IK — and its bearing about the
// axis is phi = 0, so every angle below is relative to where the arm comes from
// and nothing needs re-tuning when the hand is moved along the weapon.
//
// `ga` points from the LITTLE finger toward the INDEX (and the thumb), i.e.
// toward the muzzle on a handguard and toward the trigger on a pistol grip.
// `side` is +1 for the right hand, -1 for the left.
//
// HANDEDNESS IS DERIVED, NOT GUESSED, and it is the one sign in here that cannot
// be eyeballed. With (e1, e2, ga) right-handed, a hand curls its fingers in the
// +phi sense if it is a RIGHT hand and -phi if it is a LEFT one; that falls out
// of L x N = +B for a right hand, where L = wrist -> knuckles, N = out of the
// palm and B = little -> index. Get it backwards and the support hand wraps over
// the TOP of the handguard and into the sight line instead of under it and up
// the far side — which is the same class of bug as the red dot that once
// rendered on the shooter's own knuckles, and `vmsight` is what catches it.
// Note that this file's gun and camera bases are (right, up, FORWARD), which is
// LEFT-handed — gx x gy = -gz — so e2 is built with a cross product rather than
// written down.

// A point on the grip cylinder's frame: `ax` along the axis from `gc`, `ph`
// around it measured from e1, at radius `rad`.
// THE TRIGGER, published the same way and for the same class of reason. A
// trigger finger is the one digit that has to reach INTO the weapon, and the
// two things that decide whether it reads are both invisible to figcheck and to
// vmcheck: whether the pad actually lands on the blade, and whether any part of
// the finger is inside the guard's tube or the receiver. Both are perfectly
// valid meshes and both are what "the fingers reach through the weapon model"
// means. `vmtrig` measures them against these, so the check and the geometry
// can never describe two different rifles.
static v3    g_gun_trig_p;    // world contact point on the blade's rear face
static v3    g_gun_trig_n;    // unit REARWARD normal there (the pull direction)
static float g_gun_trig_s;    // ...and the 0..1 press that put them there. The
                              // hand needs the PRESS and not just the position:
                              // the digit curls as it closes, and a fig_hand
                              // argument would be a second copy of a number the
                              // weapon already knows, at two call sites.
static v3    g_gun_guard[4];  // the guard bow's polyline, world space
static float g_gun_guard_r;   // ...and the tube radius round it
// The cross-section scale the rifle is being drawn at — 1.0 in first person,
// GUN_GIRTH_3P on a figure. Declared here because fig_guard_clear scales its
// margin by it; defined with the gun tables below.
static float g_gun_girth;
static v3    g_gun_blade[3];  // the trigger blade's polyline, world space
static float g_gun_blade_r;   // ...and its tube radius. Published for the same
                              // reason the guard is: it is a THIN part in open
                              // air, and the proof has to be able to ask whether
                              // a fat finger has swallowed it. As a single POINT
                              // (g_gun_trig_p) it could not be asked at all, and
                              // the blade was measured 11-13 mm inside the
                              // fingertip on both weapons.
static v3    g_gun_body_c, g_gun_body_e[3];   // receiver box: centre + 3 half-axes

static float g_fig_palm_mm;   // last hand's wrist-to-knuckle chord, for `vmhand`
static float g_fig_wrist_mm;  // ...its distance from the grip axis, and whether
static int   g_fig_clamped;   // the reach clamp had to move the weapon: `vmtrig`
                              // prints both, because "the third-person hand is
                              // not on the grip" was an invisible fact for as
                              // long as nothing printed it.
static v3 hand_pt(v3 gc, v3 ga, v3 e1, v3 e2, float ax, float ph, float rad) {
  return v3_add(gc, v3_add(v3_scale(ga, ax),
         v3_add(v3_scale(e1, cosf(ph) * rad), v3_scale(e2, sinf(ph) * rad))));
}

// Digit profile: the SMOOTH family in first person, the faceted one everywhere
// else. The palm and the forearm are 12-gons and the fingers were 8s on the same
// hand — at 0.35 m a finger is ~50 px wide, so its facets are 13 px and every
// digit read as an octagonal post next to two smooth-shaded chains. Paying for
// that on the figure as well is not free: 12-gon digits on 21 figures took the
// measured 20-bot peak to 289919 of 294912 verts (98.3%), and a dropped triangle
// looks exactly like geometry that was never authored. `vm_build` forces
// FIG_KMAX, so the test is exact rather than a guess about distance.
#define FIG_DIGIT_K (g_fig_k >= FIG_KMAX ? 12 : 8)
// THE TRIGGER FINGER IS AIMED, NOT AUTHORED. It used to be three numbers — a
// third of the other fingers' wrap plus a radial reach — chosen so it looked
// right from behind the gun, on a weapon whose guard had no finger-sized hole in
// it. That cannot be right from two angles at once and it was not: the digit
// went straight through the guard and the receiver.
//
// `g_gun_trig_p` is where the blade is; this routes the index along a quadratic
// Bezier from the metacarpal head to that point, with the control point pushed
// out along the grip's own surface so the finger BOWS the way a curled one does
// instead of running there in a straight line. The tip then lands on the trigger
// whatever the hold, the press, the weapon or the third-person girth scale —
// and `vmtrig` measures that it did.
static v3 fig_trig_pt(v3 a, v3 c, v3 b, float t) {
  float u = 1.0f - t;
  return v3_add(v3_scale(a, u * u), v3_add(v3_scale(c, 2.0f * u * t),
                                           v3_scale(b, t * t)));
}

// The aimed tip's own cross-section, NAMED because the pad's stand-off from the
// blade is derived from it. Written as two independent numbers, the tip was
// placed 0.22 r = 2.57 mm off the blade's face while its own half-thickness was
// 9.13 mm — so 6.5 mm of steel sat BEHIND the flesh and the blade rendered
// impaled through the fingertip from every angle vmorbit can reach. Measured
// blade-vs-hand: -10.96 mm (AR) and -12.69 mm (SR). That is the single most
// visible instance of "the trigger finger reaches through the material", and vmtrig
// could not see it because `reach` was the tip's own cap-centre vertex, i.e.
// this very constant read back to itself.
#define FTIP_RX 0.82f
#define FTIP_RZ 0.78f
// A pad PRESSES on a trigger, it does not pass through it: the surface stops on
// the steel and deforms about a millimetre and a half. Not zero — a tangent
// contact is the hairline seam this file bans everywhere else.
#define TRIG_PAD_SQUASH 0.0015f
// How far the press slides the index's interior nodes along its own Bezier.
// 0.35 moves the middle node from 0.500 to 0.588 of the curve — a visible
// closing of the digit that costs no change in the curve's shape and therefore
// none in fig_chain's corner budget.
#define TRIG_CURL 0.35f
// A TRIGGER PULL IS A HAND CLOSING, NOT ONE FINGER SLIDING. Three more things
// move with the press, and each is a joint a real index finger uses:
//  * the MCP — the knuckle the digit leaves the palm at — flexes, which swings
//    the WHOLE finger and is the only part of it with real screen area;
//  * the pad ROLLS on the shoe as the shoe rotates under it;
//  * and the other three digits tighten on the grip, because a shooter squeezes
//    the whole hand and not just the one finger. That last one is what turns a
//    press from a finger animation into a hand animation.
#define TRIG_MCP  0.150f   // rad of extra knuckle flexion at full press
#define TRIG_ROLL 0.220f   // rad the pad rolls on the shoe
#define TRIG_GRIP 0.055f   // rad the other three digits tighten by

// How much air a digit keeps between itself and the guard's wire, before the
// girth scale. 6.0 mm and not 1.5: the push moves NODES, and the swept surface
// between two cleared nodes is a chord where the arc is not, so a margin sized
// only for the nodes leaves the BAND between them inside the tube. Solved
// against vmtrig's worst row across both weapons and both views, and re-solved
// when the grip radius was corrected and the whole hand moved 2 mm outward:
// 1.5 mm left the AR at -0.6, 3.0 left the third-person SR at -3.9, 6.0 cleared
// the old hand and 9.0 clears the correctly-sized one — bounded on the other
// side by `pad`, which stays at -2.9 mm, i.e. the fingertip is still pressed
// into the blade rather than standing off it.
#define HAND_GUARD_CLR 0.0090f

// Push a digit node out of the trigger guard's tube. PUSH, never snap-to-plane:
// a node moved to the nearest legal point travels a few millimetres and the arc
// keeps its shape, while snapping would put a corner in the chain — and
// fig_chain answers a corner it cannot fillet by collapsing the cross-section
// and emitting degenerate triangles (see the Bezier tangent note above). A no-op
// beyond r + guard_r + 1.5 mm, so the support hand 200 mm forward, a corpse's
// hand and the slide brace are untouched.
// Push a digit node out of the RECEIVER, the other piece of weapon a hand can
// cross visibly. The box is the receiver FORWARD of the grip, published by
// gun_build for exactly this reason; the palm's top corner is legitimately
// buried in it (a hand's index MCP sits right under the receiver, and burial is
// how every joint in this model is closed), but a DIGIT crossing the metal is
// what the player sees. Measured with vmtrig's `inwpn` — the deepest hand vertex
// inside the weapon's own closed mesh — the trigger hand's THUMB, which sweeps
// up the camera side of the grip and passes the receiver's lower corner, was
// 9.7 mm in. Out along the SHALLOWEST axis, so the node travels the least
// distance that clears it and the chain keeps its shape.
static v3 fig_body_clear(v3 p, float r) {
  v3 rel = v3_sub(p, g_gun_body_c);
  float best = 1e9f, sgn = 1.0f;
  int axis = -1;
  for (int k = 0; k < 3; k++) {
    float h = v3_len(g_gun_body_e[k]);
    if (h < 1e-6f) return p;
    float d = v3_dot(rel, v3_scale(g_gun_body_e[k], 1.0f / h));
    float out = h + r - fabsf(d);
    if (out <= 0.0f) return p;              // already clear on this axis
    if (out < best) { best = out; axis = k; sgn = d < 0.0f ? -1.0f : 1.0f; }
  }
  float h = v3_len(g_gun_body_e[axis]);
  return v3_add(p, v3_scale(g_gun_body_e[axis], sgn * best / h));
}

static v3 fig_guard_clear(v3 p, float r) {
  // The margin scales with the weapon's own girth for the same reason the
  // radii do: the third-person rifle draws at GUN_GIRTH_3P and its digits are
  // fatter in proportion, so a fixed millimetre margin covers less of the
  // chord sag between two cleared nodes there than it does in first person.
  float need = r + g_gun_guard_r + HAND_GUARD_CLR * g_gun_girth;
  for (int e = 0; e + 1 < 4; e++) {
    v3 a = g_gun_guard[e], ab = v3_sub(g_gun_guard[e + 1], a);
    float l2 = v3_dot(ab, ab);
    float u = l2 > 1e-9f ? f_clamp(v3_dot(v3_sub(p, a), ab) / l2, 0.0f, 1.0f) : 0.0f;
    v3 c = v3_add(a, v3_scale(ab, u)), d = v3_sub(p, c);
    float len = v3_len(d);
    if (len < 1e-5f || len >= need) continue;
    p = v3_add(c, v3_scale(d, need / len));
  }
  return p;
}
// `style`: 1 = trigger hand (aimed index, TWRAP thumb cut), 0 = support hand
// closing a full C-wrap (magazine, bolt knob — anything gripped ACROSS), 2 =
// the support HEEL grip: palm heel under the handguard, fingers wrapping the
// far flank, THUMB POINTING FORWARD along the near-upper flank — the modern
// C-clamp-family hold both weapons now share on the fore-end.
static void fig_hand(v3 wrist, v3 gc, v3 ga, float gr, float side, v3 col,
                     int style, const v3 *trig_pt, v3 *out_palm_dir) {
  ga = v3_norm(ga);
  v3 w = v3_sub(wrist, gc);
  float aw = v3_dot(w, ga);                     // where along the tube the hand is
  v3 e1 = v3_sub(w, v3_scale(ga, aw));
  float rw = v3_len(e1);
  if (rw < 1e-4f) {                             // wrist on the axis: any bearing
    e1 = v3_cross(ga, (v3){{0, 1, 0}});
    if (v3_dot(e1, e1) < 1e-8f) e1 = v3_cross(ga, (v3){{1, 0, 0}});
    rw = gr + 0.050f;
  }
  e1 = v3_norm(e1);
  v3 e2 = v3_cross(ga, e1);        // (e1, e2, ga) right-handed BY CONSTRUCTION
  // A HAND IS A FIXED SIZE. The palm runs from the wrist, which is the caller's,
  // to the knuckle row, which is the weapon's — and if those two ever get far
  // apart the palm simply stretches to span the gap. The viewmodel can never do
  // that (its wrist IS derived from the grip), but the third-person figure can
  // and does: `anim` lerps a wrist onto the ground for a slide brace, reaches one
  // down on a hard landing, and on death sprawls both to the chest while the
  // rifle leaves with the trigger hand. Measured on a filmed slide, the wrist
  // went 0.7-1.0 m off the fore-end and the palm rendered as a metre-long
  // glove-brown blade from the rifle to the knee, with the fingers still wrapped
  // on the handguard — 4-11% of bot-ticks by tacstat, plus every corpse.
  //
  // The fix keeps the HAND rigid and moves the CYLINDER: when the wrist is out of
  // range, slide the grip frame along the wrist's own bearing until it is back in
  // range. The hand then closes round empty air exactly where the wrist is, which
  // is what a hand that has let go of a rifle looks like, and the palm keeps its
  // authored proportions in every pose. The same clamp at the near end stops an
  // arm sinking into the tube.
  const float HAND_REACH_LO = gr + 0.026f, HAND_REACH_HI = gr + 0.088f;
  // A hand is a fixed size in EVERY direction, and the clamp below used to fix
  // only one of them. `rw` is the distance from the tube's AXIS; `aw` is how far
  // ALONG it the wrist sits, and it was passed through untouched — so a wrist a
  // metre up the barrel still produced a metre-long palm. Measured on a settled
  // corpse, where fig_death_pose lays the rifle on the ground while the wrists
  // drape to the sides: aw = 416.7 mm and a 425.9 mm palm against an authored
  // 104.8, i.e. the same glove-brown blade the radial clamp was written to kill,
  // rotated 90 degrees. 95 mm clears every LIVE pose measured (idle 76.0,
  // running 70.5-84.5, low-ready/slide 55.4, viewmodel 48.0) and still catches
  // the corpse and the hard-landing hand reach (141.3).
  const float HAND_AX_MAX = 0.095f;
  float rc = f_clamp(rw, HAND_REACH_LO, HAND_REACH_HI);
  float ac = f_clamp(aw, -HAND_AX_MAX, HAND_AX_MAX);
  // Whether the clamp had to MOVE the weapon out from under the wrist. It is a
  // fact about the pose, and one consumer needs it: a hand that is not on the
  // grip is not on the trigger either, so the index must not route to a blade
  // the hand has let go of. Third person hits this every frame today (the wrist
  // lands 26.2 mm from the grip axis against a 39.2 mm floor), and the index was
  // still reaching for the unmoved blade across the receiver.
  int slid = (rc != rw || ac != aw);
  g_fig_wrist_mm = (rw - gr) * 1000.0f;
  g_fig_clamped = slid;
  if (slid) {
    gc = v3_sub(wrist, v3_add(v3_scale(ga, ac), v3_scale(e1, rc)));
    rw = rc; aw = ac;
  }

  // The knuckle row: 62 degrees round the tube from the wrist's bearing, riding
  // 10.5 mm off the surface so the palm's inner face bites 3 mm INTO it. Contact
  // has to be interpenetration and not tangency, or the light gets through the
  // seam and the hand reads as hovering.
  const float KN_PH = side * 1.52f;
  // The bite is EXPLICIT, so the palm's inner face stays 3 mm inside the tube
  // whatever its thickness is set to. Writing the knuckle radius as a number
  // instead is how thinning the palm silently turns contact into a gap.
  const float HAND_BITE = 0.0030f;

  // --- palm: wrist -> knuckle row -------------------------------------------
  // THE BACK OF A HAND IS FLAT. Only the fingers curl, and that is not an
  // abstraction, it is the anatomy — so the palm is a straight tapered slab from
  // the wrist to the knuckle row with a shallow cup in it, and the whole wrap
  // lives in the digits.
  //
  // It also HAS to be flat, and that cost a round to find. fig_chain pays for a
  // turning node with fig_rmax*tan(theta/2) of bone on each side and THINS the
  // cross-section by whatever is missing. fig_rmax for a palm is its BREADTH
  // (44.6 mm half), so sweeping the palm round the tube on the same arc as the
  // fingers asked for 19.8 mm of bevel against 12.8 mm of bone at every node and
  // pinched the hand's own waist: the mitten with a crease in it. At this width
  // the primitive simply cannot bend, and the fix is that it does not need to.
  //
  // Widest and thinnest at the knuckles: ANSUR II hand breadth is 88 mm p50 and
  // ~92 gloved, metacarpal-III thickness ~28 mm and ~33 gloved, wrist breadth
  // 58 mm. A palm whose cross-section runs anywhere near round has no back and
  // no edge and reads as a sausage whatever else is right.
  // Thickest at the WRIST, thinnest at the knuckles, which is the way round a
  // real hand goes: ANSUR wrist thickness ~42 mm against ~33 mm across the
  // metacarpals. It is also what buries the sleeve's end cap — see the wrist
  // node in the viewmodel's forearm, which has to fit inside this.
  // The section change belongs at the KNUCKLE row, not at mid-palm. A breadth
  // jump of 30.0 -> 40.4 mm at node 2 while the thickness FELL put a hard bevel
  // edge on the roundest surface of the hand, oriented parallel to the forearm —
  // which is the visual signature of "no wrist" and was the only landmark on the
  // back of the trigger fist. Spread over two nodes, the widening becomes form.
  // x1.13 stylisation over the ANSUR-derived originals: hands are an attention
  // point and the genre oversizes them 1.2-1.4x; girths only, so every arc,
  // bite and burial derived from the grip radius is untouched.
  static const float PRX[5] = {0.0267f, 0.0336f, 0.0399f, 0.0484f, 0.0452f};
  static const float PRZ[5] = {0.0168f, 0.0208f, 0.0206f, 0.0164f, 0.0113f};
  float rk = gr + PRZ[4] - HAND_BITE;
  v3 pk = hand_pt(gc, ga, e1, e2, 0.0f, KN_PH, rk);
  // The cup: 3 mm of sag toward the tube across the middle of the slab, which is
  // enough to stop the back of the hand reading as a plank and far too little to
  // trigger the bevel clamp above.
  v3 mid = v3_lerp(wrist, pk, 0.5f);
  v3 md = v3_sub(mid, gc);
  md = v3_sub(md, v3_scale(ga, v3_dot(md, ga)));
  v3 inw = v3_dot(md, md) > 1e-9f ? v3_scale(v3_norm(md), -0.0038f)
                                  : (v3){{0, 0, 0}};
  // A glove is not one piece of leather: it has a padded knuckle panel and a
  // cuff, and those are the only two landmarks the BACK of a hand has — which is
  // the side the player looks at all match. Colour is per band and therefore
  // free, so this costs no geometry. Kept inside 12% of the base value, per the
  // match-value-within-one-object rule: a hard step here would read as two
  // objects rather than as one glove with a panel on it.
  v3 knuck = v3_scale(col, 0.865f);
  v3 heel  = v3_scale(col, 0.925f);
  fnode_t pm[5];
  const float PT[5] = {0.0f, 0.0f, 0.42f, 0.78f, 1.0f};
  for (int i = 1; i < 5; i++) {
    pm[i].p = v3_lerp(wrist, pk, PT[i]);
    // The cup grows with distance from the wrist. Applied flat it displaced the
    // palm's centre-line 3 mm at the very station the sleeve's buried entry ring
    // sits at, and that ring then surfaced through the back of the hand as a
    // blue chevron — a hole in the glove, in every view, from a 3 mm number that
    // had nothing to do with the arm.
    pm[i].p = v3_add(pm[i].p, v3_scale(inw, PT[i] * PT[i]));
    // The panel covers the last TWO nodes, not just the knuckle row: coloured
    // only at the distal node it lands where the fingers already are and the
    // back of the hand stays one featureless dome, which at 60 x 40 px is the
    // whole hand. A band across the metacarpals is the only landmark that side
    // has, and at 13.5% down it is a panel on one glove rather than two objects.
    pm[i].col = pm[i].bcol = i >= 3 ? knuck : col;
    pm[i].rx = PRX[i]; pm[i].rz = PRZ[i]; pm[i].roll = 0; pm[i].ao = 0.0f;
  }
  // Node 0 reaches BACK past the wrist along the palm's own direction, so
  // whatever the arm is — a sleeve chain on the figure, a forearm chain on the
  // viewmodel — swallows this end cap. A palm that starts AT the wrist leaves a
  // ring of bare geometry across it, and in first person that is a hole between
  // the hand and the arm with the world visible through it.
  // The heel: SHORT and tapered, not a 26 mm stub. It reaches back past the
  // wrist so the arm's leading ring is swallowed, but it reaches back along the
  // PALM's axis while the arm leaves along its own — so a long one is not buried
  // by anything, it just hangs off the back of the wrist as a flat-capped beak.
  // 12 mm is inside the sleeve from every direction and still covers the seam.
  v3 bk = v3_norm(v3_sub(pm[1].p, pm[2].p));
  pm[0].p = v3_add(pm[1].p, v3_scale(bk, 0.012f));
  pm[0].col = pm[0].bcol = heel;
  pm[0].rx = PRX[0]; pm[0].rz = PRZ[0]; pm[0].roll = 0; pm[0].ao = 0.0f;
  // The palm's own axis, handed back so the arm can enter ALONG it. An arm that
  // arrives on its own axis presents its end cap to the palm's flank at 50-odd
  // degrees, and a 54 x 31 mm disc does not fit inside a 60 x 39 mm ellipse at
  // that angle: it emerges as a pale flat wedge hanging off the wrist, which is
  // exactly what the turntable showed the moment the palm stopped being an arc.
  if (out_palm_dir) *out_palm_dir = v3_scale(bk, -1.0f);
  g_fig_palm_mm = v3_len(v3_sub(pk, wrist)) * 1000.0f;
  fig_chain(pm, 5, 12, 1, 1, ga);   // hint = ga, so rx is the BREADTH

  // --- THE PALM'S OWN FRAME, AND WHY THE DIGITS HAVE TO BE ROOTED IN IT -----
  // Every digit below used to be placed purely in the GRIP CYLINDER's polar
  // frame — hand_pt(FAX[f], phi, rad) — while the palm is a chord from the
  // CALLER's wrist to pk. The two frames agree only when the wrist has no
  // component along ga, and the disagreement IS that component:
  //
  //     ga . pu = -aw / palm_chord     (pk sits at ax = 0, so ga.(pk-wrist) = -aw)
  //
  // so a digit asked for FAX mm ACROSS the palm lands FAX*sqrt(1-k^2) across it
  // and FAX*k ALONG it. k is 0.47 on the viewmodel (aw 45.9 over a 98.0 mm
  // palm) and 0.68 at the reach clamp a corpse or a slide brace hits — the row
  // is skewed up to 20 mm down the palm's length and the little finger's root
  // ring ends up off its proximal-outer corner in open air. Measured by
  // replaying this function and counting root-cap vertices outside the palm's
  // own mesh: little finger 14 of 36 out at +6.19 mm on the fore-end, 28 of 36
  // at +8.79 on the pistol grip, 36 of 36 at +16.43 at the clamp, and the
  // far-tier merged mass 6 of 24 at +17.88 (+28.65 at the clamp). The THUMB —
  // the one digit already rooted with a lerp ON the palm — measures -5.4 mm,
  // i.e. correctly buried. The FRAME is the whole difference, not the tuning,
  // and that is why the defect is state-dependent: k is not a constant, it is
  // the pose. This is what "the fingers float in front of the hand" is.
  //
  // pu/pbx are the frame fig_chain itself builds for the distal ring (ny = the
  // last bone, nx = the hint projected perpendicular to it), derived here
  // rather than restated.
  v3 pu  = v3_norm(v3_sub(pm[4].p, pm[3].p));
  v3 pbx = v3_sub(ga, v3_scale(pu, v3_dot(ga, pu)));
  pbx = v3_dot(pbx, pbx) > 1e-9f ? v3_norm(pbx) : e1;
  // Where a digit may leave the hand: on the palm's CENTRE LINE, so the palm's
  // whole local thickness is available to swallow the root ring, ROOT_IN of the
  // last palm bone back from the knuckle row — where the section is 14.1 x 46.9
  // mm against 11.3 x 45.2 at the row itself.
  const float ROOT_IN = 0.55f;
  v3    root_c = v3_lerp(pm[4].p, pm[3].p, ROOT_IN);
  float root_x = PRX[4] + (PRX[3] - PRX[4]) * ROOT_IN;

  // SEPARATE FINGERS ARE THE MOST EXPENSIVE THING ON THE FIGURE, so the gate is
  // the tightest tier: 16 = inside 6 m. It used to be 12 (inside 12 m), which was
  // right for the four 2-node stubs this replaced and is not right for four
  // 5-node wraps — measured, a 20-bot brawl on seed 99 hit SCENE_MAX_VERTS and
  // DROPPED 2832 verts, and a dropped triangle looks exactly like geometry that
  // was never authored. At 12 m a finger is a third of a pixel wide; at 6 m it is
  // still under two. The viewmodel is unaffected either way — vm_build forces
  // FIG_KMAX, so first person always gets the full hand.
  if (g_fig_k >= 16) {
    // --- four fingers, close range only ------------------------------------
    // Each is an ARC about the grip axis: root buried in the palm's knuckle end,
    // then two thirds of a turn round the tube. That single fact is what makes a
    // hand read as gripping from every angle — a row of parallel rolls crossing
    // the object is the cue, and no amount of finger detail substitutes for it.
    // Gated on the profile tier: 4 fingers x 2 hands x 21 figures is thousands of
    // verts against a measured peak already at 92% of SCENE_MAX_VERTS, and at
    // 12 m a finger is a third of a pixel wide.
    //
    // Every finger gets its own girth, its own wrap and its own radius. Four
    // identical arcs offset along ga would put their side facets on shared
    // planes, which is the textbook z-fight and is exactly what figcheck caught
    // on the previous fingers (18 pairs). Here the fix is also the anatomy:
    // index through little taper in girth, the middle wraps furthest, and the
    // knuckle line arcs across the hand instead of running straight.
    // Pitch 20 mm against girths of 17-21 mm, so adjacent fingers very nearly
    // graze: fingers that TOUCH read as one hand, fingers with daylight between
    // them read as rings threaded on a barrel. They are never coplanar even so,
    // because no two share a girth, a wrap, a radial lift or a knuckle height —
    // four identical parallel arcs would put their side facets on shared planes,
    // which is the textbook z-fight figcheck caught on the previous fingers.
    static const float FAX[4] = {-0.0300f, -0.0100f, 0.0100f, 0.0295f}; // little->index
    // THE MIDDLE FINGER IS THE THICKEST, not the index. The index was authored
    // as the fattest of the four, and because it is also the one digit that
    // leaves the grip and reaches into the guard — where nothing hides it — it
    // rendered as a mass three or four times the visual weight of the others,
    // painted per digit to check. ANSUR has the middle broadest, then index,
    // then ring, then little.
    static const float FGR[4] = {0.0098f, 0.0110f, 0.0118f, 0.0111f};  // girth, x1.13
    // A little past the top of the tube. MEASURED on the player's own frame, not
    // judged: with the tips stopping at 12 o'clock the entire finger row sat on
    // the FAR side of the fore-end and the only digit the player ever saw was the
    // thumb — four wrapped fingers' worth of geometry, paid for every frame, on
    // the one surface the camera cannot see. Another 13 degrees brings the index
    // and middle tips over the crest and into the near silhouette, which is also
    // where a closed grip actually puts them.
    // 6-8 degrees past the crest, not 13. The extra wrap that brought the index
    // and middle tips into the near silhouette went too far and laid all four
    // fingertips FLAT across the top of the fore-end pointing sideways, so the
    // read went from "wrapped around it" to "laid over it". The distal node also
    // pitches further in, because the last phalanx of a curled finger points at
    // the palm rather than along the tangent.
    // Index-most-closed, not middle-most-closed. On a fore-end the index is the
    // highest and the most wrapped finger; the old order made the middle the
    // most closed at 167 degrees and the index only 154, which is the wrong
    // hand. FKN also gave index and ring the same knuckle height, so the arc
    // across the knuckles was flat exactly where it should peak.
    // WRAP, rad — x1.12 over the authored values. A closed hand on a 26 mm
    // pistol grip carries its fingertips ~200 degrees round, past the far side
    // and back toward the thumb; at 139-170 the tips stopped on the near face
    // and the four digits read as lumps stacked against the grip rather than as
    // a fist closed round it. Bounded by vmtrig: past x1.24 the index's own
    // sweep starts crowding the guard's front strap.
    static const float FSW[4] = {2.710f, 3.069f, 3.203f, 3.315f};      // wrap, rad
    static const float FLF[4] = {0.0015f, 0.0006f, 0.0000f, 0.0009f};  // radial lift
    static const float FKN[4] = {-0.22f, -0.08f, 0.01f, 0.06f};        // knuckle arc
    g_fig_jit = 0.0f;    // a digit is not a crate — see the note on g_fig_jit
    for (int f = 0; f < 4; f++) {
      float r = FGR[f], rr = gr + r - 0.0034f + FLF[f];
      float ph0 = KN_PH + side * FKN[f];
      // THE TRIGGER FINGER. Four identical curled digits clamped on a pistol
      // grip is a clamp, not a firing grip: what says "about to shoot" is one
      // finger doing something different from the other three. The index breaks
      // out of the set while middle, ring and little stay fully closed — and
      // where it breaks out TO is the weapon's own published trigger contact
      // point, not a tuned sweep. See fig_trig_pt.
      float sweep = FSW[f], reach = 0.0f;
      // HEEL grip: the fingers hold the FAR FLANK, they do not crest the top.
      // Reference (CoD4/BO2 first-person frames): from the eye you see the
      // back of the hand and at most the first knuckles — full 155-190 deg
      // wraps put fingertips over the rail into the sight line's neighborhood
      // and read as claws. 0.70 ends the tips just past the far mid-line.
      if (style == 2) sweep *= 0.66f;
      int aimed = style == 1 && f == 3 && trig_pt != NULL && !slid;
      // The whole hand squeezes, not just the index. Tiny — 3 degrees — but it
      // is three digits' worth of screen area moving together, where the index
      // alone measured 4-9 pixels of travel.
      if (style == 1 && !aimed) sweep += TRIG_GRIP * g_gun_trig_s;
      if (style == 1 && f == 3 && !aimed) { sweep = 1.42f; reach = 0.0125f; }
      fnode_t fg[6];   // 5 authored + one DOME ring, below
      // Node 0 sits BEHIND the knuckle row and inside the palm, so the root cap
      // is buried whatever the wrap angle; node 1 is the METACARPAL HEAD and is
      // deliberately fat enough to stand ~3 mm proud of the palm's distal end,
      // which is the only landmark the back of a hand has and the thing whose
      // absence made this read as a spade from the one angle the player looks at
      // it from. Nodes 2-4 ride the surface, and the tip pulls in because the
      // last phalanx of a curled finger points at the palm, not along the
      // tangent; node 4 domes it so no flat octagon ever faces the camera.
      // FT[0] used to be -0.30, i.e. 40-48 degrees BEHIND the knuckle row. That
      // is correct in phi and wrong in radius: the palm at that angle is at its
      // LARGEST distance from the tube, not its smallest, so the four root caps
      // sat in open air with a void under the palm — measured, the little
      // finger's whole root ring stood 15.7 mm clear of any palm surface. Kept
      // close to the row, where the palm's distal end actually is.
      const float FT[5]  = {-0.16f, 0.08f, 0.46f, 0.76f, 1.00f};
      // 0.86 / 0.72 on the ROOT, not 1.16 / 0.90. Node 0 used to be the widest
      // ring of the whole digit, which is backwards twice over: anatomically
      // the metacarpal head (node 1, 1.24) is the fat one, and geometrically a
      // fat ring is exactly what cannot be buried in the thin distal end of the
      // palm. Node 0 is inside the palm, so nothing visible shrinks.
      // 1.16 on the metacarpal head, not 1.24. The knuckle is deliberately fat
      // enough to stand proud of the palm, but on the AIMED index that ring is
      // out in the open at the top of the grip rather than half buried between
      // its neighbours, so the same factor reads as a swelling there.
      const float FRX[5] = { 0.86f, 1.16f, 1.04f, 0.94f, 0.76f};
      const float FRZ[5] = { 0.72f, 1.11f, 0.94f, 0.84f, 0.64f};
      // The aimed index runs a Bezier instead of an arc: A = the metacarpal
      // head, where the digit leaves the palm; B = the blade, pulled back along
      // the guard's own axis by the finger's own radius so the PAD touches it
      // rather than the tip's centre landing inside the steel; C = out in front
      // of the knuckle, which is what bows the proximal phalanx away from the
      // grip and takes the middle of the finger clear of the guard's rear
      // upright. The two proximal nodes stay on the grip arc so the root cap is
      // buried exactly as the other three fingers' are.
      v3 tA = {{0, 0, 0}}, tB = tA, tC = tA;
      if (aimed) {
        // The METACARPAL joint flexes with the press, which swings the whole
        // digit rather than only its last two nodes. It moves the Bezier's start
        // AND its tangent, so the curve stays a curve and fig_chain's corner
        // budget is untouched — the same reason the press re-parameterises
        // instead of bowing.
        float aph = ph0 + side * (0.30f + TRIG_MCP * g_gun_trig_s);
        float arad = rr + 0.0018f;
        tA = hand_pt(gc, ga, e1, e2, FAX[f] - 0.0050f, aph, arad);
        // The tip's own half-thickness minus the squash: the pad's SURFACE
        // lands on the steel and deforms into it by TRIG_PAD_SQUASH, which is
        // what vmtrig's `pad` reads back (-0.8 mm at rest, -1.9 pressed).
        tB = v3_add(*trig_pt,
                    v3_scale(g_gun_trig_n, r * FTIP_RZ - TRIG_PAD_SQUASH));
        // THE CONTROL POINT IS THE ARC'S OWN TANGENT, and that is a manifold
        // requirement rather than a nicety. A quadratic Bezier leaves A toward
        // C, so an arbitrary bow direction puts a hard corner at the node where
        // the wrap hands over to the curve — and fig_chain answers a corner it
        // cannot fillet by collapsing the cross-section, which emits degenerate
        // triangles. figcheck saw it immediately: open=4 degen=2, all of them in
        // `trigfinger`, i.e. a hole in the one digit this work is about.
        // Continuing along the wrap is also the anatomy: the proximal phalanx
        // keeps curling off the grip before the finger straightens to the blade.
        v3 tg = v3_sub(hand_pt(gc, ga, e1, e2, FAX[f] - 0.0050f,
                               aph + side * 0.08f, arad), tA);
        tg = v3_dot(tg, tg) > 1e-12f ? v3_norm(tg) : v3_norm(v3_sub(tB, tA));
                // 0.35, not 0.62, and the departure 0.30 rad rather than 0.60: with the
        // Bezier leaving a third of a radian further round the grip and bowing
        // out by two thirds of its own chord, the index left the knuckle, ran
        // AROUND the grip and then turned back to the trigger — a hard V that
        // read as a folded sausage from every angle behind the weapon. Leaving
        // near the knuckle with a shallow bow is one smooth curve.
        tC = v3_add(tA, v3_scale(tg, 0.35f * v3_len(v3_sub(tB, tA))));
      }
      for (int i = 0; i < 5; i++) {
        float t = FT[i];
        // -0.0074, not -0.0054, on the distal pull-in. Every digit ended in a
        // flat-cut cylinder whose end cap faced the camera: a bright disc on the
        // tip of each finger, and four of them in a row read as cut-off tubes
        // rather than as a closed hand. Pulling the tip 4.6 mm further toward
        // the tube swings that cap behind the digit's own silhouette from the
        // whole front hemisphere, and it is also the anatomy — the last phalanx
        // of a curled finger points at the palm, not along the tangent. Bounded
        // by figcheck rather than by taste: at -0.0086 and beyond the tips start
        // grazing the fore-end they are wrapped round (2 -> 4 zfight pairs in
        // gun/hand), because pulling the tip in is pulling it INTO the tube.
        float rad = i == 0 ? rk - 0.0022f
                  : i == 1 ? rr + 0.0016f : rr - 0.0074f * t * t;
        rad += reach * t * t;              // straightens off the grip's front
        fg[i].p = hand_pt(gc, ga, e1, e2, FAX[f], ph0 + side * sweep * t, rad);
        if (aimed && i >= 2) {
          // THE PRESS SLIDES THE NODES ALONG THE CURVE, IT DOES NOT BOW IT.
          // The tip is pinned to the blade and the two proximal nodes sit on the
          // grip arc, so the only thing a press can shape is what happens
          // between them — and shaping it with the CONTROL POINT was wrong:
          // measured, pushing tC out by 0.30 took node 3's turn from 71.5 to
          // 92.8 degrees on the SR, and fig_chain answers a corner it cannot
          // fillet by collapsing the cross-section, so the fingertip pinched and
          // then flared into a trumpet exactly when the trigger was pulled.
          // Re-parameterising leaves the CURVE alone — same tA, tC, tB, same
          // tangents, same manifold — and only moves where along it the interior
          // nodes sample, which is what a finger closing actually does.
          float u = (t - FT[2]) / (1.0f - FT[2]);
          u += TRIG_CURL * g_gun_trig_s * u * (1.0f - u);
          fg[i].p = fig_trig_pt(tA, tC, tB, u);
        }
        fg[i].col = fg[i].bcol = col;
        fg[i].rx = r * (aimed && i == 4 ? FTIP_RX : reach > 0.0f && i == 4 ? 0.80f : FRX[i]);
        fg[i].rz = r * (aimed && i == 4 ? FTIP_RZ : reach > 0.0f && i == 4 ? 0.74f : FRZ[i]);
        // THE GUARD IS THE ONE PART OF THE WEAPON A HAND MAY NOT SHARE SPACE
        // WITH. Everywhere else in this model neighbours interpenetrate on
        // purpose and the burial is hidden inside a solid; nothing hides a
        // 6.8 mm wire crossing a 24 mm finger. The digits are constant-radius
        // arcs about the GRIP axis and the guard's rear strap sits INSIDE the
        // circle they sweep (its surface is 12.4-21.0 mm from that axis against
        // a swept 29.7), so no tuning of FSW/FLF can fix it — the arc has to be
        // told the guard exists. Measured before this: the strap was 3.5 mm
        // (AR) and 8.4 mm (SR) inside the digits in first person and 13.4 /
        // 21.0 mm inside the whole hand in third.
        fg[i].p = fig_guard_clear(fg[i].p, fmaxf(fg[i].rx, fg[i].rz));
        // The root node lives inside the palm and the palm's own corner is
        // legitimately under the receiver, so only the nodes that are OUT on the
        // weapon are cleared of it. 0.72 of the girth rather than all of it: a
        // digit resting against the receiver's flank should touch it, which is
        // this file's contact rule, not stand off it.
        if (i) fg[i].p = fig_body_clear(fg[i].p, fmaxf(fg[i].rx, fg[i].rz) * 0.72f);
        // The pad ROLLS on the shoe as the shoe turns under it. Only the tip:
        // rolling the whole digit would twist the wrap off the grip.
        fg[i].roll = aimed && i == 4 ? side * TRIG_ROLL * g_gun_trig_s : 0.0f;
        // AO 0.88..0.96, not 0.58..0.66. The cavity override exists for armpits
        // and the underside of a plate carrier; the OUTER surface of a finger on
        // the outside of a grip is not in a cavity, and blanketing the chain with
        // it rendered the fingers at 73% of the palm's luminance — nearly half a
        // stop inside one object, which is what makes them read as dark parts
        // glued to a pale one instead of as the same hand.
        fg[i].ao = 0.88f + 0.020f * (float)i;
      }
      // A KNUCKLE IS A POINT ON THE PALM, NOT A BEARING ON THE TUBE. Node 0 is
      // the only node of a digit that is never seen and its only job is
      // ATTACHMENT, so it belongs in the palm's frame while nodes 1-4 stay on
      // the wrap. Three things have to hold at once and each was measured:
      //  * BURIED. On the palm's centre line at the digit's own breadth
      //    station. ROOT_SPREAD tucks the row 8% inboard because the station has
      //    to fit inside an ELLIPSE — at |u| = 0.64 the palm has 76% of its
      //    thickness left — and 8% is what takes the worst cap vertex from
      //    +0.96 mm (outside) to -2.5 mm (buried).
      //  * TANGENT. The first bone leaves along the palm's OWN axis, so the
      //    digit continues the hand instead of starting beside it: the angle
      //    between the palm's distal bone and the digit's first bone goes from
      //    28.1-47.4 degrees to 3.5-10.5. That is the "elastic" half of the
      //    complaint — a digit that leaves at 40 degrees reads as glued on
      //    however well it is buried.
      //  * LONG ENOUGH. A joint can only be as fat as its bones are long, which
      //    is fig_chain's own rule and was never applied to this chain: node 0
      //    and node 1 sat 0.24 of a sweep apart on ONE arc, so the metacarpal
      //    head — the fattest ring in the digit and the only landmark that says
      //    a finger is JOINED to a hand — was thinned to 0.51-0.65 of its
      //    authored girth on every finger. ROOT_BONE restores it to 0.82-1.00.
      const float ROOT_SPREAD = 0.92f, ROOT_BONE = 0.026f;
      {
        v3 a = v3_add(root_c,
                      v3_scale(pbx, FAX[f] * (root_x / PRX[4]) * ROOT_SPREAD));
        float bl = v3_len(v3_sub(fg[1].p, a));
        // Sliding further back is always safe: pu runs down the palm's centre
        // line into thicker section, so extra bone buys extra burial.
        if (bl < ROOT_BONE) a = v3_sub(a, v3_scale(pu, ROOT_BONE - bl));
        fg[0].p = a;
      }
      // The aimed index gets its own probe row so `vmtrig` can ask about the
      // ONE digit that reaches into the weapon. The palm's top corner is
      // legitimately buried inside the receiver — that is this file's own
      // interpenetration rule, and testing the whole hand against the receiver
      // reported 313 "defects" that are all invisible by construction. A thin
      // digit half in and half out of the metal is the real thing, and it needs
      // to be asked about separately.
      // A ROUND FINGERTIP, and it takes a ring rather than a radius. Pulling
      // the tip node toward the palm (above) swings its cap out of the front
      // hemisphere, which is most of the fix and was as far as the last pass
      // got — but the cap is still a FLAT disc, 0.76 x 0.64 of the digit's
      // radius, and at the size a first-person hand renders that is a 12 x 10 mm
      // hard-edged plate on the end of every finger. One more ring at a third of
      // the width, set half a radius further along the digit's own last bone,
      // turns the last band into the dome and leaves a cap a ninth of the area
      // pointing away from the camera. It goes through the same guard and body
      // clearances every other node does, so a tip that would land in the
      // trigger guard is pushed out of it exactly as node 4 is.
      {
        v3 d = v3_sub(fg[4].p, fg[3].p);
        float dl = v3_len(d);
        v3 dir = dl > 1e-6f ? v3_scale(d, 1.0f / dl) : ga;
        fg[5] = fg[4];
        fg[5].rx = fg[4].rx * 0.34f;
        fg[5].rz = fg[4].rz * 0.34f;
        fg[5].p = v3_add(fg[4].p, v3_scale(dir, fmaxf(fg[4].rx, fg[4].rz) * 0.58f));
        fg[5].p = fig_guard_clear(fg[5].p, fmaxf(fg[5].rx, fg[5].rz));
        fg[5].p = fig_body_clear(fg[5].p, fmaxf(fg[5].rx, fg[5].rz) * 0.72f);
      }
      const char *pv = g_probe_on && g_nprobe ? g_probe[g_nprobe - 1].name : NULL;
      if (aimed) probe("trigfinger");
      fig_chain(fg, 6, FIG_DIGIT_K, 1, 1, ga);
      if (aimed) probe(pv);
    }
    g_fig_jit = 1.0f;
  } else {
    // Far tier: ONE mass, and it is STRAIGHT — two nodes, no interior node, and
    // therefore no bevel and nothing that can be thinned. That is not a
    // simplification for looks, it is the only shape this primitive can hold at
    // this width: fig_chain sizes a bevel off fig_rmax, the profile's
    // CIRCUMRADIUS, so a 40 mm-wide slab turning 67 degrees over a 12 mm bone
    // asked for 26.9 mm of fillet, got 7.3, and was thinned to 27% — an 84 x 21
    // mm mass pinching to a 20 x 5 mm sliver and flaring back out, on the tier
    // that covers most of a 48 m arena. The chord still passes 5 mm outside the
    // tube's axis-line and its inner face bites 5 mm into the surface, so the
    // mass reads as closed round the fore-end from any distance where it is
    // drawn at all.
    // The root is the PALM's, exactly as the four fingers' roots now are. A
    // 2-node chain has ONE ring plane and it is perpendicular to its own bone,
    // so a full-breadth slab rooted on the tube ARC drives its corners straight
    // out through the back of the hand: measured 17.9 mm at k=12 and k=8, and
    // 28.7 mm at the reach clamp — 3 px at this tier's 6 m near edge, wandering
    // with the pose, which is what reads as a blob stuck on beside the hand.
    // Rooted on the palm's centre line the same vertices measure -0.55/-0.29 mm
    // live and +1.45 at the clamp (0.24 px at 6 m). It has to stay TWO nodes, so
    // the residual is sub-pixel rather than zero. The narrower root is also the
    // anatomy: four merged fingers are ~69 mm across, not the whole hand's 90.
    const float FAR_IN = 0.80f;
    fnode_t fg[2];
    fg[0].p = v3_lerp(pm[4].p, pm[3].p, FAR_IN);
    fg[1].p = hand_pt(gc, ga, e1, e2, -0.0015f, KN_PH + side * 1.34f, gr + 0.0098f);
    fg[0].col = fg[0].bcol = col; fg[1].col = fg[1].bcol = col;
    fg[0].rx = (PRX[4] + (PRX[3] - PRX[4]) * FAR_IN) * 0.72f;
    fg[0].rz = (PRZ[4] + (PRZ[3] - PRZ[4]) * FAR_IN) * 0.78f;
    fg[1].rx = 0.0340f; fg[1].rz = 0.0096f;
    fg[1].p = fig_guard_clear(fg[1].p, fg[1].rx);
    fg[0].roll = fg[1].roll = 0; fg[0].ao = 0.90f; fg[1].ao = 0.96f;
    fig_chain(fg, 2, 8, 1, 1, ga);
  }

  // --- thumb: the OPPOSITION, and the strongest grip cue there is -----------
  // A hand whose digits all wrap the same way is a clamp. What says "closed
  // round something" is the C: a thumb coming up the NEAR side while the fingers
  // come up the far one, with a readable gap between the two tips. Before this
  // the thumb was rooted PAST the knuckle row and swept back across the palm —
  // a 40 mm hexagonal dowel lying parallel to the fingers with a flat end cap
  // pointed at the camera, which is why it read as a bolt head rather than a
  // digit, and why nothing in the hand was opposing anything.
  //
  // It is built in TWO spaces, because a thumb lives in both: the metacarpal is
  // part of the palm (rooted in the thenar, on the palm's radial edge, buried so
  // no cap shows) and the phalanges are part of the wrap (arcs about the tube,
  // going -side, i.e. the other way round from the fingers).
  {
    // The root has to be BURIED, and it was not: 25.0 mm along ga plus its own
    // 12.8 mm of half-breadth put its outer extent 3.8 mm outside a palm that is
    // 34.0 mm there, so the thumb's proximal end sat ON the palm surface with a
    // dark seam under it and read as a rod glued to the hand.
    // Deeper and thinner at the root. A thumb branching off a palm must cross
    // its surface somewhere, and a GRAZING crossing is a 1-2 px black slit
    // running 200 px diagonally across the back of the fist — two surfaces at
    // equal depth along the crossing curve, with the winner decided by rounding.
    // Decisive burial moves that curve inside the palm where nothing renders it.
    v3 tb0 = v3_add(v3_lerp(wrist, pk, 0.42f), v3_scale(ga, 0.0150f));
    v3 tb1 = v3_add(v3_lerp(wrist, pk, 0.62f), v3_scale(ga, 0.0382f));
    // Tip at 100 degrees round the near side: the index finger's tip comes over
    // the top at about 74 degrees the other way, so the two end ~26 mm apart on a
    // 28 mm tube. That gap IS the grip.
    // Each node rides so its INNER face is 3 mm inside the surface — the same
    // HAND_BITE the palm and the fingers obey. Measured before this, the thumb's
    // inner face stood gr + 7.3 / 4.1 / 5.5 mm off the fore-end: the one digit
    // the player actually sees, and the only one that never touched the weapon,
    // with ~23 px of daylight under it at 0.35 m through a 62-degree pass. A grip
    // whose most visible finger floats is not a grip.
    // A THUMB ON A PISTOL GRIP DOES NOT WRAP IT — it lies ALONG the frame, and
    // that is not a stylistic call: above a pistol grip there is a RECEIVER, and
    // a thumb that keeps climbing runs into it. Measured by painting the digit
    // and looking at the player's own frame, the tip came out the FAR side of
    // the AR's receiver as a severed green blob with the shaft on the near side
    // — the single most visible instance of "the finger reaches through the
    // weapon material" left in the model, and invisible to every distance-based
    // proof because the thumb is not inside anything, it is THROUGH it.
    // The support hand keeps the full wrap: a handguard has nothing above it,
    // and there the C the thumb makes with the fingers is the whole grip cue.
    // ...and it TIGHTENS with the press. Measured by recolour, the thumb is
    // 3627 px of the AR hip frame against the whole index finger's 12 — it is
    // three hundred times more of the trigger hand than the digit the press was
    // being animated on, so a squeeze that does not move it is a squeeze the
    // player cannot see.
    const float TWRAP = (style == 1 ? 0.68f : 1.0f) *
                        (1.0f + (style == 1 ? 0.06f * g_gun_trig_s : 0.0f));
    v3 tb2, tb3, tb4;
    if (style == 2) {
      // FORWARD THUMB. The heel grip trades the C's opposition cue for the
      // drive-forward cue: phalanges at INCREASING axial stations along ga
      // (little-finger -> index -> muzzle) at a nearly constant bearing on
      // the near-upper flank, radius easing down so the pad RIDES the
      // handguard surface (same HAND_BITE philosophy as the wrap). The
      // bearing stays off the fingers' own arc so the two chains never run
      // parallel on a shared flank (the z-fight class).
      // Stations end WITH the handguard (the grip point moved forward, so a
      // 99 mm thumb ran off the fore-end onto the bare barrel), and the whole
      // digit stays LOW on the near flank — in the reference frames no digit
      // ever breaks the handguard's top silhouette.
      tb2 = hand_pt(gc, ga, e1, e2, 0.0420f, -side * 0.84f, gr + 0.0070f);
      tb3 = hand_pt(gc, ga, e1, e2, 0.0560f, -side * 0.76f, gr + 0.0052f);
      tb4 = hand_pt(gc, ga, e1, e2, 0.0660f, -side * 0.70f, gr + 0.0033f);
    } else {
      tb2 = hand_pt(gc, ga, e1, e2, 0.0455f, -side * 0.62f * TWRAP, gr + 0.0092f);
      tb3 = hand_pt(gc, ga, e1, e2, 0.0430f, -side * 1.28f * TWRAP, gr + 0.0079f);
      tb4 = hand_pt(gc, ga, e1, e2, 0.0395f, -side * 1.76f * TWRAP, gr + 0.0040f);
    }
    fnode_t th[5] = {
      {tb0, col, col, 0.0119f, 0.0132f, 0, 0.86f},
      {tb1, col, col, 0.0134f, 0.0150f, 0, 0.92f},
      {tb2, col, col, 0.0123f, 0.0137f, 0, 0.96f},
      {tb3, col, col, 0.0110f, 0.0122f, 0, 0.96f},
      // The last node domes the tip instead of leaving a flat octagon facing
      // whatever the camera happens to be. Same trick on the fingers.
      {tb4, col, col, 0.0071f, 0.0078f, 0, 0.96f}};
    // The thumb sweeps 1.9 rad over four bones, so every interior node earns a
    // bevel PAIR — five nodes is eight rings, which is more geometry than the
    // whole far-tier hand it sits on. Beyond 6 m it drops to the three nodes that
    // carry the silhouette: base, knuckle, tip.
    // The thumb comes up the NEAR side of the grip, which is the side the
    // guard's rear strap is on. Same no-go rule as the fingers.
    for (int i = 0; i < 5; i++) {
      th[i].p = fig_guard_clear(th[i].p, fmaxf(th[i].rx, th[i].rz));
      th[i].p = fig_body_clear(th[i].p, fmaxf(th[i].rx, th[i].rz) * 0.72f);
    }
    g_fig_jit = 0.0f;
    if (g_fig_k >= 16) {
      fig_chain(th, 5, FIG_DIGIT_K, 1, 1, ga);
    } else {
      fnode_t t3[3] = {th[0], th[2], th[4]};
      t3[2].rx = 0.0082f; t3[2].rz = 0.0092f;
      fig_chain(t3, 3, 8, 1, 1, ga);
    }
    g_fig_jit = 1.0f;
  }
}

// ONE FOREARM, FOR BOTH VIEWS. The arm the player sees in first person and the
// arm on his own body in third person were two different objects: nine nodes
// with a gauntlet, a pronation roll and a five-step value ramp at profile 12,
// against three nodes of plain tapered sleeve at profile 8. That is the
// divergence the user actually noticed — "the arms are round in first person
// and low-poly in third" — and it is the same class as the two rifles
// `gun_build` was written to collapse into one: an arm authored twice ends up
// different in the two places the player sees it.
//
// `wrist` and `back` say where it starts and which way it runs — the viewmodel
// derives `back` from the palm's own axis through two real wrist rotations, the
// figure gets it from the IK elbow. `elbow` is NULL for the viewmodel, whose arm
// simply leaves the frame at its authored length; the figure passes its solved
// elbow and the stations scale to land on it. Everything else — the notch at the
// wrist, the cuff, the flexor swell, the pronation, the value ramp — is the same
// geometry in both, which is the whole point.
//
// ANSUR II: wrist 58 x 42 mm, forearm max 80 x 62, p50 flexed circumference 310.
// This sits at the 5th percentile (265 mm) rather than the median, because the
// same measurement that condemned the old broomstick also showed the sleeve
// eating 31% of the frame's height.
static void fig_forearm(v3 wrist, v3 back, v3 pd, v3 lat, float side,
                        v3 sleeve, v3 gnt, const v3 *elbow, int k) {
  // A MONOTONE ramp, and only three steps in it. Measured down the old sleeve
  // there were three hard albedo bands at luminance 53 / 74 / 41 running
  // hand -> elbow: a +31% step over 12 px followed by a -48% step over 6 px,
  // i.e. a rugby sock whose brightest band was the middle of the forearm, with
  // the strongest INTERNAL step at 1.98x against a silhouette contrast of 1.19x
  // — the banding four times stronger in stops than the shape's own edge. The
  // band next to the hand keeps the measured albedo; only the ones below it
  // move, and the last step is the biggest on purpose because it sits where the
  // arm is already leaving the frame.
  v3 slv1 = v3_scale(sleeve, 0.94f), slv2 = v3_scale(sleeve, 0.86f);
  v3 slv3 = v3_scale(sleeve, 0.76f), slv4 = v3_scale(sleeve, 0.58f);
  float sc = 1.0f;
  if (elbow) {
    float l = v3_len(v3_sub(*elbow, wrist));
    if (l > 0.06f) { sc = l / 0.268f; back = v3_scale(v3_sub(*elbow, wrist), 1.0f / l); }
  }
  // The ellipse ROLLS down the arm, which is pronation and is why a real forearm
  // looks different from every angle while a swept tube looks the same from all
  // of them. `fnode_t.roll` costs nothing — it turns the ring in the transported
  // frame — and 0 at the wrist to ~17 degrees at the elbow is the twist a hand
  // on a fore-end actually puts in. The small lateral drift is the ulnar bow.
  #define FAN(T, RX, RZ, C, AO)                                            \
    {v3_add(v3_add(wrist, v3_scale(back, (T) * sc)),                       \
            v3_scale(lat, side * (T) * sc * 0.075f)), C, C, RX, RZ,        \
     side * (T) * 1.10f, AO}
  fnode_t fa[9] = {
    // Node 0 sits 26 mm INSIDE the palm, along the PALM's own axis rather than
    // the arm's, so the sleeve's end cap is buried in a cross-section it
    // actually fits inside. THE WRIST IS NOT A LEDGE: the palm ring is
    // 33.2 x 20.5 mm and this one 24.0 x 12.2, and real anatomy runs that way —
    // at the wrist crease the hand is ~62 x 30 mm and the styloid region 20 mm
    // proximal is 58 x 42, i.e. the THICKNESS increases monotonically from hand
    // into forearm and the notch is entirely a BREADTH phenomenon.
    {v3_add(wrist, v3_scale(pd, 0.026f)), sleeve, sleeve, 0.0240f, 0.0122f, 0, 0.0f},
    // THE CUFF IS A FAT NODE OF THIS CHAIN, not a nested one: as its own chain
    // it had to cross the sleeve's surface to bury its far cap, and two 12-gons
    // transported from DIFFERENT first bones are angularly out of phase, so the
    // intersection came out as a zig-zag torn edge round the whole wrist. And
    // NEVER PUT THE VALUE STEP ON THE SHAPE STEP — the cuff crowns at node 2
    // while the leather -> slate boundary lands at node 4, on geometry that is
    // doing nothing. Stacked on one ring they multiply instead of masking each
    // other, which relocated the original "angular" complaint rather than
    // removing it.
    FAN(-0.010f, 0.0316f, 0.0234f, gnt,    0.99f),   // cuff lip, proud in BOTH axes
    FAN( 0.008f, 0.0342f, 0.0260f, gnt,    0.96f),   // cuff crown, tight to the wrist
    FAN( 0.030f, 0.0338f, 0.0254f, gnt,    0.93f),
    FAN( 0.050f, 0.0368f, 0.0278f, sleeve, 0.90f),   // leather -> slate
    FAN( 0.088f, 0.0392f, 0.0302f, slv1,   0.85f),
    FAN( 0.140f, 0.0434f, 0.0338f, slv2,   0.77f),
    FAN( 0.200f, 0.0470f, 0.0370f, slv3,   0.62f),   // the flexor belly
    // -2.9 degrees on the last band, not -4.7: a band that pinches out hard
    // reads as an amputated stub in any debug view, where a gentle one reads as
    // an arm continuing off frame.
    FAN( 0.268f, 0.0426f, 0.0336f, slv4,   0.46f)};
  #undef FAN
  // Past 12 m the cuff's three rings are under two pixels apart and the whole
  // gauntlet is one band, so the far tier keeps only the stations that carry the
  // silhouette: the buried root, the cuff crown, the leather/slate boundary, the
  // flexor belly and the elbow. Nine rings on 21 figures took the measured
  // 20-bot peak from 292k to 305k of 393216 (78%), and a dropped triangle looks
  // exactly like geometry that was never authored.
  if (g_fig_k >= 12) {
    fig_chain(fa, 9, k, 1, 1, lat);
  } else {
    fnode_t f5[5] = {fa[0], fa[2], fa[4], fa[7], fa[8]};
    fig_chain(f5, 5, k, 1, 1, lat);
  }
}

// A HOLLOW round tube: two concentric walls closed by an annulus at each end.
// fig_chain closes a chain with a fan to its own axis, which is right for a solid
// body and exactly wrong for anything the player has to see THROUGH — the window
// of a red dot, the bore of a muzzle brake. A sight you cannot see through is
// worse than no sight, and that is what a capped chain made.
// Still one closed manifold: outer wall, inner wall wound the other way, and two
// annular rims, so every edge belongs to exactly two triangles.
// The cross-section takes separate x/z half-extents, so the same primitive makes
// a round bore and a RECTANGULAR sight window; and the wall shades per FACE when
// the profile is a faceted one, because radial per-vertex normals would round the
// flat sides of a square housing off into a pillow.
static void fig_tube(v3 a, v3 b, float rox, float roz, float rix, float riz,
                     int k, v3 col, v3 hint) {
  if (fig_diss_cut(v3_lerp(a, b, 0.5f))) return;
  if (!g_prof_ready) fig_prof_init();
  v3 ax = v3_sub(b, a);
  float l = v3_len(ax);
  if (l < 1e-5f || rix >= rox || riz >= roz || rix <= 0.0f) return;
  ax = v3_scale(ax, 1.0f / l);
  if (k > g_fig_k) k = g_fig_k < 4 ? 4 : g_fig_k;
  v3 nx = fig_perp(ax, hint);
  fring_t ao = fig_ring(a, ax, nx, rox, roz, k), bo = fig_ring(b, ax, nx, rox, roz, k);
  fring_t ai = fig_ring(a, ax, nx, rix, riz, k), bi = fig_ring(b, ax, nx, rix, riz, k);
  int round = fig_round(k);
  v3 rn[FIG_KMAX], fn[FIG_KMAX];
  for (int i = 0; i < k; i++) {
    rn[i] = v3_norm(v3_sub(ao.v[i], a));                       // per vertex
    v3 e = v3_sub(ao.v[(i + 1) % k], ao.v[i]);
    v3 f = v3_cross(e, ax);                                    // per face
    fn[i] = v3_dot(f, f) > 1e-14f ? v3_norm(f) : rn[i];
    if (v3_dot(fn[i], rn[i]) < 0.0f) fn[i] = v3_scale(fn[i], -1.0f);
  }
  for (int i = 0; i < k; i++) {
    int j = (i + 1) % k;
    v3 n0 = round ? rn[i] : fn[i], n1 = round ? rn[j] : fn[i];
    scene_push(ao.v[i], n0, col);  scene_push(bo.v[i], n0, col);
    scene_push(bo.v[j], n1, col);  scene_push(ao.v[i], n0, col);
    scene_push(bo.v[j], n1, col);  scene_push(ao.v[j], n1, col);
    v3 m0 = v3_scale(n0, -1.0f), m1 = v3_scale(n1, -1.0f);
    scene_push(ai.v[j], m1, col);  scene_push(bi.v[j], m1, col);
    scene_push(bi.v[i], m0, col);  scene_push(ai.v[j], m1, col);
    scene_push(bi.v[i], m0, col);  scene_push(ai.v[i], m0, col);
    scene_quad(ai.v[i], ao.v[i], ao.v[j], ai.v[j], v3_scale(ax, -1.0f), col);
    scene_quad(bi.v[j], bo.v[j], bo.v[i], bi.v[i], ax, col);
  }
}

