// code/render/scene_batch.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// --- scene batch: CPU-built oriented boxes, world-shader vertex format -----

// 21 skinned figures (20 bots + the player's own body) plus the 64-slot
// impact-mark ring have to fit. A figure at full profile resolution is ~6.8k
// tris = 20k verts (six joint balls at ~170 tris each included), so 21 of
// those would need 420k; what makes them fit is the distance-tiered profile
// resolution (g_fig_k: 16-gon inside 6 m down to 6-gon past 22 m), and 14
// figures inside 8 m of the camera at once is already an implausible pile-up.
// Overrunning the budget does not crash, it silently drops triangles — i.e.
// holes in whatever is drawn last — so scene_push counts drops and says so
// once, and the `budget` harness command prints the peak, because a dropped
// triangle looks exactly like geometry that was never authored.
//
// THE CEILING IS SIZED FOR THE WORST SUPPORTED MATCH, NOT FOR A TYPICAL ONE.
// It sat at 294912 against a measured 20-bot peak of 288611 — 97.9%, i.e. 2.1%
// of margin on a configuration the MATCH tab lets any player select, and a
// review pass did observe a real drops=5112 overflow. The old comment's
// reassurance that "14 figures inside 8 m at once is already an implausible
// pile-up" is simply not what 20 hard bots do: swept over 10 seeds x 3 match
// times x 4 frames the peak band is 268k-289k, so the ceiling was inside the
// NOISE of ordinary play. 393216 puts the same worst case at 73.4% and costs
// 4.7 MB of static array (12 floats/vert x 4 B: 14.2 -> 18.9 MB), which is
// nothing next to shipping holes in a figure. The GPU side is unaffected —
// the upload is g_scene_floats, not the array size.
// Re-measure with `budget` over SEVERAL seeds and frames after anything that
// adds figure geometry; one seed is not a measurement.
// RAISED AGAIN FOR THE CORPSE POOL. Bodies now outlive the entities that were
// them, so a 20-bot hard brawl can carry up to six of them on the floor ON TOP
// of its 21 live figures. Measured over two seeds with three frames each, that
// took the peak to 453711 of 393216 with 154599 verts DROPPED — i.e. holes in
// whatever was drawn last. Capping a corpse at the faceted profile tier
// (CORPSE_K) takes it to 426555; 524288 puts that at 81% and costs 6.3 MB of
// static array (12 floats/vert x 4 B: 18.9 -> 25.2 MB), which is nothing next to
// shipping holes in a figure. Re-measure with `budget` over SEVERAL seeds and
// several frames after anything that adds figure geometry; one seed is not a
// measurement, and the frame has to be RENDERED first or the peak reads 0.
#define SCENE_MAX_VERTS 524288
static float g_scene[SCENE_MAX_VERTS * VERT_FLOATS];
static long g_scene_drops;
static int   g_scene_floats;
static int   g_scene_want;   // floats ASKED for this frame, including any dropped

static int g_scene_peak;   // most verts any one frame has needed
static int g_scene_dirty;  // buffer changed since it was last handed to the GL

// Kept OUT of scene_push: this is the once-per-session diagnostic, and an
// fprintf plus a string literal in the body is what pushed the hot function
// over gcc's inline budget at its ~50 call sites. It then survived -O3 -flto as
// a real out-of-line symbol (`scene_push.lto_priv.0`) called once per emitted
// vertex — 264k times a frame at 20 bots, and 44% of all scene_build samples.
static void scene_drop_note(void) {
  if (!g_scene_drops++)
    fprintf(stderr, "scene: SCENE_MAX_VERTS exhausted — geometry dropped\n");
}

// The static-world material a SCENE vertex carries (0 = none): only set
// while props and decor are built into the world mesh, where uDetail decodes
// it from the AO channel exactly as mesh_push's g_mesh_mat.
static int g_scene_smat;
static inline void scene_push(v3 p, v3 n, v3 c) {
  // The peak tracks DEMAND, not what fitted. Peaking on g_scene_floats made the
  // one case the number exists for — an actual overflow — the one case it could
  // not describe: it saturated at exactly SCENE_MAX_VERTS however far over the
  // frame really went, so `budget` could say THAT geometry was dropped but never
  // HOW MUCH ceiling to add. Counting the ask instead makes the overflow
  // self-diagnosing (peak > max, by the amount missing).
  g_scene_want += VERT_FLOATS;
  if (g_scene_want > g_scene_peak) g_scene_peak = g_scene_want;
  if (g_scene_floats + VERT_FLOATS > SCENE_MAX_VERTS * VERT_FLOATS) {
    scene_drop_note();
    return;
  }
  float *v = g_scene + g_scene_floats;
  v[0] = p.x; v[1] = p.y; v[2] = p.z;
  v[3] = n.x; v[4] = n.y; v[5] = n.z;
  v[6] = c.x; v[7] = c.y; v[8] = c.z;
  v[9] = g_mat_rough; v[10] = g_mat_metal; v[11] = ao_at(p.y) * g_ao_mul + 4.0f * (float)g_scene_smat;
  g_scene_floats += VERT_FLOATS;
}

// Every refill goes through here so the upload flag cannot be forgotten by a
// new builder: scene_draw re-sends the buffer only when this has been called
// since the last hand-over.
static inline void scene_reset(void);

// --- scene chunks: what a pass is allowed to SKIP ---------------------------
// One buffer feeds four passes with three different frusta — the shadow map
// (ortho, the whole arena), the main view, and the sniper's scope at a NINE
// degree lens. Drawn as one glDrawArrays they all pay for all twenty figures,
// and the scope pass pays 322k vertices to photograph the one or two bodies
// that fit in a 9-degree cone. A chunk is one figure's vertex range plus the
// sphere it lives in, so a pass can cull what its own frustum cannot see and
// merge what survives back into runs — the geometry stays in ONE buffer and
// ONE upload; only the DRAW is split.
//
// The sphere is analytic, not measured: tracking a bounding box inside
// scene_push would put six compares on the hot path that scene_drop_note was
// deliberately taken off (see there). A standing figure is 1.85 m tall and a
// low-ready rifle reaches ~0.9 m out from the spine, a sprawled corpse ~1.2 m,
// so a 2.0 m radius about the hip clears every pose the solver can reach with
// room to spare. Conservative is the only safe direction: a sphere that is too
// small pops a body out of the frame it was standing in.
#define SCENE_MAX_CHUNKS (MAX_ENTS + MAX_CORPSES + 4)
#define SCENE_CHUNK_R    2.0f
typedef struct { int f0, f1; v3 c; float r; } schunk_t;
static schunk_t g_schunk[SCENE_MAX_CHUNKS];
static int g_nschunk, g_schunk_open = -1;

static void scene_chunk_begin(v3 c, float r) {
  if (g_nschunk >= SCENE_MAX_CHUNKS) return;   // tail geometry: never culled
  g_schunk_open = g_nschunk++;
  g_schunk[g_schunk_open] = (schunk_t){g_scene_floats, g_scene_floats, c, r};
}
static void scene_chunk_end(void) {
  if (g_schunk_open >= 0) g_schunk[g_schunk_open].f1 = g_scene_floats;
  g_schunk_open = -1;
}

// Six clip planes out of a view-projection (Gribb/Hartmann), in the column-major
// layout m4_mul builds: row r is m[0*4+r], m[1*4+r], m[2*4+r], m[3*4+r].
typedef struct { float p[6][4]; } frustum_t;
static frustum_t frustum_of(const mat4 *m) {
  frustum_t f;
  for (int i = 0; i < 6; i++) {
    int a = i >> 1, sgn = (i & 1) ? -1 : 1;
    for (int c = 0; c < 4; c++)
      f.p[i][c] = m->m[c * 4 + 3] + (float)sgn * m->m[c * 4 + a];
    float l = sqrtf(f.p[i][0] * f.p[i][0] + f.p[i][1] * f.p[i][1] +
                    f.p[i][2] * f.p[i][2]);
    if (l > 1e-9f) for (int c = 0; c < 4; c++) f.p[i][c] /= l;
  }
  return f;
}
static int frustum_sees(const frustum_t *f, v3 c, float r) {
  for (int i = 0; i < 6; i++)
    if (f->p[i][0] * c.x + f->p[i][1] * c.y + f->p[i][2] * c.z + f->p[i][3] < -r)
      return 0;
  return 1;
}
// The same test for a swept sphere. Conservative in the ONE direction that is
// safe: a capsule is rejected only when BOTH ends are fully behind the same
// plane, so a capsule that straddles two planes' outsides is kept even though
// it is invisible. Over-drawing costs a frame; under-drawing loses a shadow.
static int frustum_sees_cap(const frustum_t *f, v3 a, v3 b, float r) {
  for (int i = 0; i < 6; i++) {
    float da = f->p[i][0] * a.x + f->p[i][1] * a.y + f->p[i][2] * a.z + f->p[i][3];
    float db = f->p[i][0] * b.x + f->p[i][1] * b.y + f->p[i][2] * b.z + f->p[i][3];
    if (da < -r && db < -r) return 0;
  }
  return 1;
}

// Where this body's shadow can possibly land. The sun is a fixed world
// direction, so a caster's shadow lives on the segment between the body and the
// point that ray reaches the floor — and nowhere else. A body BEHIND the player
// still has to be built and drawn if that segment crosses the frame; one whose
// segment does not is invisible in every pass this frame and is the geometry
// worth not having. Swept to y = -0.5 (under the floor, so a shadow on the
// floor itself is inside the segment rather than on its endpoint) and capped,
// because a figure launched off a wall must not generate a kilometre of capsule.
#define SHADOW_CAST_MAX 24.0f
static v3 shadow_foot(v3 c) {
  v3 sn = atm_sun();
  const float ly = sn.y;
  float t = (c.y + 0.5f) / ly;
  if (t < 0.0f) t = 0.0f;
  if (t > SHADOW_CAST_MAX) t = SHADOW_CAST_MAX;
  v3 d = v3_scale(sn, -1.0f);
  return v3_add(c, v3_scale(d, t));
}

// The camera frustum of the frame being built, published for scene_build. Off
// for every proof that draws a figure outside render_frame (figcheck, vmcheck,
// figbury and the ragdoll rigs all call scene_draw_figure directly), so the
// invariants keep running on the tier the command asked for.
static frustum_t g_cam_frustum;
static int       g_cam_cull;

static inline void scene_reset(void) {
  g_scene_floats = 0; g_scene_want = 0; g_scene_dirty = 1;
  g_nschunk = 0; g_schunk_open = -1;
}

static void scene_quad(v3 a, v3 b, v3 c, v3 d, v3 n, v3 col) {
  scene_push(a, n, col); scene_push(b, n, col); scene_push(c, n, col);
  scene_push(a, n, col); scene_push(c, n, col); scene_push(d, n, col);
}

// --- figure measurement probe -----------------------------------------------
// The figure's proportions are the thing that decides whether it reads as a man
// or as a stack of tubes, and they were authored by eye against a FRONT view —
// which is why every width was roughly right and every length and depth was not.
// A screenshot cannot answer "how long is the boot"; this can. `probe(name)`
// marks the vertex range a part occupies, and `figm` prints each part's extent
// in the figure's OWN frame (x right, y up from the feet, z forward) in mm, so
// every edit is a checked delta against ANSUR II rather than a guess.
// Off by default: one predictable branch per part, nothing per vertex.
#define PROBE_MAX 96
typedef struct { const char *name; int v0, v1; } probe_t;
static probe_t g_probe[PROBE_MAX];
static int g_nprobe, g_probe_on;

static void probe(const char *name) {
  if (!g_probe_on) return;
  if (g_nprobe) g_probe[g_nprobe - 1].v1 = g_scene_floats;
  if (!name || g_nprobe >= PROBE_MAX) return;
  g_probe[g_nprobe++] = (probe_t){name, g_scene_floats, g_scene_floats};
}

// Oriented box: center + three half-extent vectors (need not be unit; their
// directions give the face normals). One call = 12 triangles.
// The face winding below is only outward-facing for a RIGHT-handed frame; a
// left-handed (ex,ey,ez) would emit the whole box inside-out, and backface
// culling then makes it see-through (that is what a transparent head looks
// like). A box is symmetric in ez, so flipping it repairs the handedness
// without changing the shape.
static int fig_diss_cut(v3 p);   // defined with the figure; see the dissolve

// --- broken edges ----------------------------------------------------------
// Every machined or moulded part that exists has its arrises broken: a cast one
// because the mould needs draft, a machined one because a sharp arris is a burr
// and a cut finger. A renderer that emits perfect 90-degree corners is therefore
// modelling the one thing that is never there — and it costs far more than
// pedantry, because a chamfer is the ONLY surface on a box whose normal is not
// one of the three axis normals. It is the only place a highlight can sit that
// the two faces beside it cannot carry. On a black rifle that thin bright line
// IS the edge; without it the silhouette is drawn entirely by whatever happens
// to be behind the weapon, which is why the receiver read as a cut-out.
//
// STATE, not a parameter, for exactly the reason the material is state:
// threading a chamfer through fifty GBOX call sites to say "still 0.7 mm" is
// fifty chances to say something else. Zero by default, so every existing
// caller — the whole figure, the corpse pool, the world mesh — emits the mesh
// it emitted before, byte for byte.
static float g_cham;

// One quad and one triangle with the winding REPAIRED against a stated outward
// normal. A chamfered box has twenty-six polygons in eight orientations and
// hand-winding them is how a box ends up with three faces inside out — which
// backface culling then renders as a see-through hole, the same failure
// scene_box's own handedness repair below exists to prevent.
static void scene_poly4(v3 a, v3 b, v3 c, v3 d, v3 n, v3 col) {
  if (v3_dot(v3_cross(v3_sub(b, a), v3_sub(c, a)), n) < 0.0f)
    scene_quad(d, c, b, a, n, col);
  else
    scene_quad(a, b, c, d, n, col);
}
static void scene_poly3(v3 a, v3 b, v3 c, v3 n, v3 col) {
  if (v3_dot(v3_cross(v3_sub(b, a), v3_sub(c, a)), n) < 0.0f) {
    scene_push(c, n, col); scene_push(b, n, col); scene_push(a, n, col);
  } else {
    scene_push(a, n, col); scene_push(b, n, col); scene_push(c, n, col);
  }
}

// The chamfered box: six inset axis faces, twelve edge bevels, eight corner
// triangles. Watertight by construction rather than by tolerance — every one of
// the twenty-four vertices is computed once from the same three expressions, so
// each edge of the result is shared by exactly two polygons and figcheck's
// open/dup/degen tiers stay at zero.
// 132 verts against the plain box's 36, which is why this is not on by default:
// the same gun_build runs for twenty bots in the world pass, and 3.7x on every
// box there would spend the scene budget on a bevel nobody can resolve at 20 m.
static void scene_box_cham(v3 c, v3 nx, v3 ny, v3 nz,
                           float ax, float ay, float az, float t, v3 col) {
  // A chamfer may never eat more than a third of the smallest half-extent: the
  // weapon's smallest controls are 2 mm boxes, and a 0.7 mm bevel on a 2 mm
  // half-extent is not a broken edge, it is a different part.
  float mn = ax < ay ? (ax < az ? ax : az) : (ay < az ? ay : az);
  if (t > mn * 0.34f) t = mn * 0.34f;
  if (t <= 1e-6f) return;
  v3 px[8], py[8], pz[8];
  for (int i = 0; i < 8; i++) {
    float sx = i & 1 ? 1.0f : -1.0f, sy = i & 2 ? 1.0f : -1.0f,
          sz = i & 4 ? 1.0f : -1.0f;
    v3 ux = v3_scale(nx, sx), uy = v3_scale(ny, sy), uz = v3_scale(nz, sz);
    px[i] = v3_add(c, v3_add(v3_scale(ux, ax),
            v3_add(v3_scale(uy, ay - t), v3_scale(uz, az - t))));
    py[i] = v3_add(c, v3_add(v3_scale(ux, ax - t),
            v3_add(v3_scale(uy, ay), v3_scale(uz, az - t))));
    pz[i] = v3_add(c, v3_add(v3_scale(ux, ax - t),
            v3_add(v3_scale(uy, ay - t), v3_scale(uz, az))));
  }
  #define CIDX(I, J, K) (((I) > 0) | (((J) > 0) << 1) | (((K) > 0) << 2))
  // The bevels and the corners are the surfaces that WEAR. A coating is thinnest
  // on an arris and the substrate shows through there first, so the broken edge
  // is a shade brighter than the faces it joins — which also means the edge
  // reads when the sun is nowhere near it, instead of only when it is.
  v3 ecol = v3_scale(col, 1.16f);
  for (int s = -1; s <= 1; s += 2) {
    float f = (float)s;
    scene_poly4(px[CIDX(s, -1, -1)], px[CIDX(s, 1, -1)],
                px[CIDX(s, 1, 1)],   px[CIDX(s, -1, 1)], v3_scale(nx, f), col);
    scene_poly4(py[CIDX(-1, s, -1)], py[CIDX(1, s, -1)],
                py[CIDX(1, s, 1)],   py[CIDX(-1, s, 1)], v3_scale(ny, f), col);
    scene_poly4(pz[CIDX(-1, -1, s)], pz[CIDX(1, -1, s)],
                pz[CIDX(1, 1, s)],   pz[CIDX(-1, 1, s)], v3_scale(nz, f), col);
  }
  for (int a = -1; a <= 1; a += 2)
    for (int b = -1; b <= 1; b += 2) {
      float fa = (float)a, fb = (float)b;
      v3 nab = v3_norm(v3_add(v3_scale(nx, fa), v3_scale(ny, fb)));
      scene_poly4(px[CIDX(a, b, -1)], px[CIDX(a, b, 1)],
                  py[CIDX(a, b, 1)],  py[CIDX(a, b, -1)], nab, ecol);
      v3 nac = v3_norm(v3_add(v3_scale(nx, fa), v3_scale(nz, fb)));
      scene_poly4(px[CIDX(a, -1, b)], px[CIDX(a, 1, b)],
                  pz[CIDX(a, 1, b)],  pz[CIDX(a, -1, b)], nac, ecol);
      v3 nbc = v3_norm(v3_add(v3_scale(ny, fa), v3_scale(nz, fb)));
      scene_poly4(py[CIDX(-1, a, b)], py[CIDX(1, a, b)],
                  pz[CIDX(1, a, b)],  pz[CIDX(-1, a, b)], nbc, ecol);
    }
  for (int i = 0; i < 8; i++) {
    float sx = i & 1 ? 1.0f : -1.0f, sy = i & 2 ? 1.0f : -1.0f,
          sz = i & 4 ? 1.0f : -1.0f;
    v3 n = v3_norm(v3_add(v3_scale(nx, sx),
           v3_add(v3_scale(ny, sy), v3_scale(nz, sz))));
    scene_poly3(px[i], py[i], pz[i], n, ecol);
  }
  #undef CIDX
}

static void scene_box(v3 c, v3 ex, v3 ey, v3 ez, v3 col) {
  // A dissolving figure loses its BOXES too. Without this the NVG shroud, the
  // pouches and every box part of the rifle survive the whole dissolve and then
  // all disappear in one frame — filmed at 4.5 m, a scatter of boxes and three
  // dark joint balls left floating in mid-air. Whole box on its centre, not per
  // face: a pouch is smaller than one dissolve cell, so a per-face cut would
  // shred it instead of removing it.
  if (fig_diss_cut(c)) return;
  if (v3_dot(v3_cross(ex, ey), ez) < 0.0f) ez = v3_scale(ez, -1.0f);
  v3 nx = v3_norm(ex), ny = v3_norm(ey), nz = v3_norm(ez);
  if (g_cham > 0.0f) {
    scene_box_cham(c, nx, ny, nz, v3_len(ex), v3_len(ey), v3_len(ez),
                   g_cham, col);
    return;
  }
  v3 p[8];
  for (int i = 0; i < 8; i++) {
    v3 q = c;
    q = v3_add(q, v3_scale(ex, i & 1 ? 1.0f : -1.0f));
    q = v3_add(q, v3_scale(ey, i & 2 ? 1.0f : -1.0f));
    q = v3_add(q, v3_scale(ez, i & 4 ? 1.0f : -1.0f));
    p[i] = q;
  }
  scene_quad(p[1], p[3], p[7], p[5], nx, col);                       // +x
  scene_quad(p[4], p[6], p[2], p[0], v3_scale(nx, -1.0f), col);      // -x
  scene_quad(p[2], p[6], p[7], p[3], ny, col);                       // +y
  scene_quad(p[4], p[0], p[1], p[5], v3_scale(ny, -1.0f), col);      // -y
  scene_quad(p[4], p[5], p[7], p[6], nz, col);                       // +z
  scene_quad(p[0], p[2], p[3], p[1], v3_scale(nz, -1.0f), col);      // -z
}
