// code/figure/draw.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// Draw one skeleton figure from its anim state. palette 0 = bot olive,
// 1 = the player's slate blue; wpn picks which rifle the hands are holding.
// flash01 washes the clothes toward flash_col.
// THE CARRIED WEAPONS. A soldier does not put his rifle in his pocket when he
// draws a sidearm: whatever long gun is not in his hands is SLUNG — on his back,
// diagonally, muzzle up behind one shoulder and the butt at the opposite hip,
// with the strap over that shoulder and across the chest. The same gun_build
// that draws the weapon in his hands draws it here, at the same 3P girth, so a
// slung rifle is the rifle and not a stand-in.
//
// Placed from the gun's OWN measured box (gun_box), never from constants: the
// carbine and the sniper differ by 30 cm in length and by where their grip
// origin sits along it, and the box keeps the whole gun outside the back — the
// pack's rear face when one is worn, the plate carrier's back plate or the
// shirt otherwise.
static void gun_box(int w, v3 *mn, v3 *mx) {
  static v3 bmn[WPN_COUNT], bmx[WPN_COUNT];
  static int have[WPN_COUNT];
  if (!have[w]) {
    // Built into the scene and immediately rewound: the only way to know what
    // gun_build emits is to let it emit. Axis-aligned, so a vertex's position
    // IS its model coordinate.
    int f0 = g_scene_floats, want0 = g_scene_want;
    float gg = g_gun_girth;
    g_gun_girth = GUN_GIRTH_3P;
    gun_build(w, (v3){{0, 0, 0}}, (v3){{1, 0, 0}}, (v3){{0, 1, 0}}, (v3){{0, 0, 1}},
              0.0f, 0.0f, 0.0f);
    g_gun_girth = gg;
    v3 lo = {{1e9f, 1e9f, 1e9f}}, hi = {{-1e9f, -1e9f, -1e9f}};
    for (int f = f0; f + VERT_FLOATS <= g_scene_floats; f += VERT_FLOATS)
      for (int k = 0; k < 3; k++) {
        float v = g_scene[f + k];
        if (v < lo.e[k]) lo.e[k] = v;
        if (v > hi.e[k]) hi.e[k] = v;
      }
    g_scene_floats = f0;
    g_scene_want = want0;
    if (hi.x < lo.x) { *mn = lo; *mx = hi; return; }   // scene full: next frame
    bmn[w] = lo; bmx[w] = hi; have[w] = 1;
  }
  *mn = bmn[w]; *mx = bmx[w];
}
// A weapon frame: origin and the three axes gun_build takes (rt = fw x up is
// the pose frame's own convention, see pose_gun_frame).
typedef struct { v3 o, rt, up, fw; } gframe_t;
// Place weapon `w` so its measured BOX CENTRE sits at `c` with axes fw/up.
static gframe_t gframe_at(int w, v3 c, v3 fw, v3 up) {
  v3 mn, mx;
  gun_box(w, &mn, &mx);
  gframe_t f;
  f.fw = v3_norm(fw);
  f.up = v3_norm(v3_sub(up, v3_scale(f.fw, v3_dot(up, f.fw))));
  f.rt = v3_norm(v3_cross(f.fw, f.up));
  v3 bc = {{0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y), 0.5f * (mn.z + mx.z)}};
  if (mx.x < mn.x) bc = (v3){{0, 0, 0}};
  f.o = v3_sub(c, v3_add(v3_scale(f.rt, bc.x),
                 v3_add(v3_scale(f.up, bc.y), v3_scale(f.fw, bc.z))));
  return f;
}
static v3 gframe_centre(int w, const gframe_t *f) {
  v3 mn, mx;
  gun_box(w, &mn, &mx);
  if (mx.x < mn.x) return f->o;
  return v3_add(f->o, v3_add(v3_scale(f->rt, 0.5f * (mn.x + mx.x)),
                v3_add(v3_scale(f->up, 0.5f * (mn.y + mx.y)),
                       v3_scale(f->fw, 0.5f * (mn.z + mx.z)))));
}
// The pistol in the drop-leg holster on the strong-side thigh: muzzle down the
// leg, slide forward, its thin axis pointing OUT of the leg, and the SLIDE
// seated in the holster body (the rig the leg already carries — same `out`
// cant, same stand-off from the thigh's across radius) so only the grip shows
// above and behind it. Recomputed from the pose alone, so the swap animation
// can fly the pistol to exactly where the leg builder draws it.
static gframe_t holster_frame(const pose_t *P, v3 spine) {
  v3 prt = v3_norm(v3_sub(P->hip[1], P->hip[0]));
  v3 td = v3_sub(P->knee[1], P->hip[1]);
  td = v3_dot(td, td) > 1e-6f ? v3_norm(td) : v3_scale(spine, -1.0f);
  v3 pfw = v3_norm(v3_cross(spine, prt));
  v3 out = v3_add(v3_scale(prt, 0.97f), v3_scale(pfw, 0.24f));
  out = v3_sub(out, v3_scale(td, v3_dot(out, td)));
  out = v3_dot(out, out) > 1e-6f ? v3_norm(out) : prt;
  float r = 0.5f * (fig_rmax(8, 0.0900f, 0.0900f) + fig_rmax(8, 0.0855f, 0.0855f));
  v3 fwd = v3_cross(out, td);                 // along the leg's front, either way
  if (v3_dot(fwd, pfw) < 0.0f) fwd = v3_scale(fwd, -1.0f);
  v3 mn, mx;
  gun_box(WPN_PT, &mn, &mx);
  float cu = mx.x > mn.x ? 0.5f * (mn.y + mx.y) - (mx.y - 0.022f) : -0.04f;
  v3 c = v3_add(P->hip[1], v3_add(v3_scale(out, r + 0.004f),
                v3_add(v3_scale(td, 0.112f), v3_scale(fwd, cu))));
  return gframe_at(WPN_PT, c, td, fwd);
}
// How far behind the chest node the back's outer surface is, for what he wears.
static float fig_back_depth(void) {
  return FIG_PACK == PACK_ASSAULT ? 0.236f : FIG_VEST == VEST_CARRIER ? 0.172f : 0.150f;
}
// WHERE EACH WEAPON LIVES WHEN IT IS NOT IN THE HANDS — the carry the
// reference loadout shows: the carbine down the middle of the back, MUZZLE
// DOWN with the stock standing up behind the neck; the sniper down the left
// side of the pack, muzzle down, scope facing back; the pistol in the drop-leg
// holster (built with the leg, see the drop-leg rig). Returns 0 for a weapon
// with no carry slot (knife, grenade).
static int stow_frame(const pose_t *P, v3 Rc, v3 spine, int w, gframe_t *f) {
  v3 cfw = v3_norm(v3_cross(spine, Rc));
  v3 mn, mx;
  gun_box(w, &mn, &mx);
  float half_rt = mx.x > mn.x ? 0.5f * (mx.x - mn.x) : 0.03f;
  if (w == WPN_AR) {
    float back = fig_back_depth();
    v3 fw = v3_norm(v3_add(v3_scale(spine, -1.0f), v3_scale(Rc, 0.07f)));
    v3 c = v3_add(P->chest, v3_add(v3_scale(spine, -0.120f),
                  v3_scale(cfw, -(back + half_rt + 0.004f))));
    // The BORE down the spine, not the box centre: the magazine hangs off
    // one side of the bore, so a centred box sits the rifle 45 mm off-line.
    if (mx.x > mn.x) c = v3_add(c, v3_scale(Rc, 0.5f * (mn.y + mx.y)));
    // ...then off the spine to his RIGHT, over the right shoulder blade: dead
    // centre the stock stood straight up behind the head and hid it.
    c = v3_add(c, v3_scale(Rc, 0.085f));
    *f = gframe_at(w, c, fw, Rc);          // sights to his right, magazine left
    return 1;
  }
  if (w == WPN_SR) {
    // Down the LEFT flank, behind the arm's swing and outboard of the pack's
    // side pouches, splayed a few degrees so the muzzle clears the thigh.
    // Tucked against the pack's left rear corner (or the back's), not hung
    // out past the arm: 0.19 m out with a 10% splay left a hand's width of
    // air between the rifle and the body from behind.
    v3 fw = v3_norm(v3_add(v3_scale(spine, -1.0f), v3_scale(Rc, -0.03f)));
    float side = FIG_PACK == PACK_ASSAULT ? 0.128f : 0.118f;
    float dep = FIG_PACK == PACK_ASSAULT ? 0.165f : 0.125f;
    v3 c = v3_add(P->chest, v3_add(v3_scale(spine, -0.210f),
                  v3_add(v3_scale(Rc, -(side + half_rt)), v3_scale(cfw, -dep))));
    *f = gframe_at(w, c, fw, v3_scale(cfw, -1.0f));   // scope faces back
    return 1;
  }
  return 0;
}
// The two long guns not in the hands, in their slots, and the carbine's strap:
// from the stock swivel behind the neck over the left shoulder, across the
// chest, and round the right hip to the lower swivel.
static void fig_stowed(const pose_t *P, const kit_t *kt, v3 Rc, v3 spine, int hold) {
  int ent = g_fig_ent;
  v3 cfw = v3_norm(v3_cross(spine, Rc));
  for (int w = WPN_AR; w <= WPN_SR; w++) {
    if (w == hold) continue;
    if (ent >= 0 && !mode_allowed(ent, w)) continue;
    gframe_t f;
    if (!stow_frame(P, Rc, spine, w, &f)) continue;
    probe("stow_gun");
    float gg = g_gun_girth;
    g_gun_girth = GUN_GIRTH_3P;
    gun_build(w, f.o, f.rt, f.up, f.fw, 0.0f, 0.0f, 0.0f);
    g_gun_girth = gg;
    v3 mn, mx;
    gun_box(w, &mn, &mx);
    if (mx.x < mn.x) continue;
    float half_rt = 0.5f * (mx.x - mn.x);
    v3 cen = gframe_centre(w, &f);
    float len = mx.z - mn.z;
    mat_set(MAT_GEAR_R, 0.0f);
    v3 sc = v3_scale(kt->web, 1.05f);
    #define SP(X, Y, Z) v3_add(P->chest, v3_add(v3_scale(Rc, (X)), \
                        v3_add(v3_scale(spine, (Y)), v3_scale(cfw, (Z)))))
    float front = FIG_VEST == VEST_CARRIER ? 0.205f : FIG_VEST == VEST_RIG ? 0.180f : 0.160f;
    float back = fig_back_depth();
    if (w == WPN_AR) {
      // fw points DOWN here, so the stock end is -fw.
      v3 top = v3_add(cen, v3_add(v3_scale(f.fw, -0.36f * len), v3_scale(cfw, half_rt)));
      v3 bot = v3_add(cen, v3_add(v3_scale(f.fw, 0.20f * len), v3_scale(cfw, half_rt)));
      float ptop = FIG_PACK == PACK_ASSAULT ? 0.100f : 0.080f;
      fnode_t st[8] = {
        {top, sc, sc, 0.0045f, 0.017f, 0, 0.0f},
        {SP(-0.090f, ptop + 0.012f, -(back - 0.012f)), sc, sc, 0.0045f, 0.018f, 0, 0.0f},
        {SP(-0.112f, 0.122f, -0.090f), sc, sc, 0.0045f, 0.018f, 0, 0.0f},
        {SP(-0.112f, 0.140f,  0.020f), sc, sc, 0.0045f, 0.018f, 0, 0.0f},
        {SP(-0.098f, 0.070f, front),   sc, sc, 0.0045f, 0.018f, 0, 0.0f},
        {SP(0.0f, -0.120f, front + 0.004f), sc, sc, 0.0045f, 0.018f, 0, 0.0f},
        {SP(0.150f, -0.330f, front - 0.030f), sc, sc, 0.0045f, 0.018f, 0, 0.0f},
        {bot, sc, sc, 0.0045f, 0.017f, 0, 0.0f}};
      probe("stow_strap");
      fig_chain(st, 8, 8, 1, 1, cfw);
    } else {
      // The sniper rides on a short loop off the pack's left shoulder strap.
      v3 top = v3_add(cen, v3_scale(f.fw, -0.30f * len));
      fnode_t st[3] = {
        {top, sc, sc, 0.0045f, 0.016f, 0, 0.0f},
        {SP(-0.150f, 0.050f, -0.110f), sc, sc, 0.0045f, 0.017f, 0, 0.0f},
        {SP(-0.118f, 0.110f, -0.050f), sc, sc, 0.0045f, 0.017f, 0, 0.0f}};
      probe("stow_strap");
      fig_chain(st, 3, 8, 1, 1, Rc);
    }
    #undef SP
  }
}

// THE THIRD-PERSON WEAPON SWAP. The simulation swaps in 0.3 s and knows
// nothing of where a gun is kept; this is presentation only, per entity, on
// its own clock. Phase A puts the old weapon AWAY (hands carry it from the
// ready to its slot and let go), phase B DRAWS the new one (the hand reaches to
// the slot, closes, and brings it round over the shoulder to the ready). A
// weapon with no slot (knife, grenade) skips its phase. Any shot ends it.
#define SWAP_A 0.32f      // seconds: stow
#define SWAP_B 0.42f      // seconds: draw
static int   g_sw_seen[MAX_ENTS];   // last weapon seen + 1, 0 = never
static int   g_sw_from[MAX_ENTS];
static float g_sw_t0[MAX_ENTS];
static float ease3(float x) { x = f_clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
// A weapon frame between two others: the box centre on a quadratic arc that
// passes THROUGH `via` at the halfway point, axes blended and re-squared.
static gframe_t gframe_arc(int w, const gframe_t *a, const gframe_t *b, v3 via, float k) {
  v3 ca = gframe_centre(w, a), cb = gframe_centre(w, b);
  v3 ctl = v3_sub(v3_scale(via, 2.0f), v3_scale(v3_add(ca, cb), 0.5f));
  float u = 1.0f - k;
  v3 c = v3_add(v3_scale(ca, u * u), v3_add(v3_scale(ctl, 2.0f * u * k), v3_scale(cb, k * k)));
  v3 fw = v3_add(v3_scale(a->fw, u), v3_scale(b->fw, k));
  v3 up = v3_add(v3_scale(a->up, u), v3_scale(b->up, k));
  if (v3_dot(fw, fw) < 1e-4f) fw = k < 0.5f ? a->fw : b->fw;
  v3 upo = v3_sub(up, v3_scale(v3_norm(fw), v3_dot(up, v3_norm(fw))));
  if (v3_dot(upo, upo) < 1e-4f) up = v3_add(up, v3_scale(a->rt, 0.3f));
  return gframe_at(w, c, fw, up);
}
// The wrist that holds weapon `w` in frame `f` by its grip (hand 1) or its
// fore-end (hand 0) — the same construction the sidearm branch uses.
static v3 gframe_wrist(int w, const gframe_t *f, int hand) {
  v3 gc, ga; float gr;
  if (hand) {
    gun_hold(w, GH_GRIP, f->o, f->rt, f->up, f->fw, &gc, &ga, &gr);
    v3 br = v3_sub(v3_scale(f->fw, -1.0f), v3_scale(ga, -v3_dot(f->fw, ga)));
    br = v3_dot(br, br) > 1e-8f ? v3_norm(br) : v3_scale(f->up, -1.0f);
    return v3_add(gc, v3_add(v3_scale(ga, -0.046f * GUN_GIRTH_3P), v3_scale(br, gr + 0.066f)));
  }
  gun_hold(w, GH_FORE3, f->o, f->rt, f->up, f->fw, &gc, &ga, &gr);
  v3 br = v3_norm(v3_add(v3_scale(f->rt, -0.62f), v3_scale(f->up, -0.78f)));
  return v3_add(gc, v3_add(v3_scale(ga, -0.040f), v3_scale(br, gr + 0.066f)));
}

static void scene_draw_figure(const anim_t *A, v3 pos, float yaw,
                              const death_t *dth, float alpha,
                              float flash01, v3 flash_col, int palette, int wpn,
                              gunanim_t gs) {
  // The pouch trip rides the same travel scalar here (the third-person rifle
  // has one magazine axis): 5 x the 60 mm travel is the pouch's 0.30 m drop. A
  // magazine that is not in the hand at all is sent below the floor.
  float mag01 = gs.maghide ? 60.0f : gs.mag + gs.pouch3 * 5.0f, trig01 = gs.trig;
  // Both consumers of `pos` below are asking where this figure's GROUND is, and
  // an air crouch folds 0.60 m of leg tuck into the entity origin in one tick
  // (see anim_t.root_off). Left raw, the AO ramp — which is every band on the
  // body — stepped by that same 0.60 m in a single frame: a whole-figure flash,
  // in exactly the frame the body was already teleporting. 0 for every bot.
  pos.y -= A->root_off;
  {  // Profile resolution from distance: full detail inside 8 m, then two steps.
    float dx = pos.x - g_cam_eye.x, dy = pos.y + 1.0f - g_cam_eye.y,
          dz = pos.z - g_cam_eye.z;
    float d2 = dx * dx + dy * dy + dz * dz;
    g_fig_k = d2 < 36.0f ? 16 : d2 < 144.0f ? 12 : d2 < 484.0f ? 8 : 6;
    // A CORPSE IS SCENERY AND PAYS SCENERY'S PRICE. It is not something the
    // player aims at, it is about to come apart, and there can be several of
    // them on the floor at once on top of a full 20-bot brawl — measured, eight
    // at the distance tier they happened to be in took the scene to 453711 verts
    // of 393216 and DROPPED 154599, which is holes in whatever was drawn last.
    // Capped at the faceted tier the same eight cost a third of that.
    if (g_fig_kcap && g_fig_k > g_fig_kcap) g_fig_k = g_fig_kcap;
    // OFF SCREEN IS A TIER OF ITS OWN, and it is the coarsest one. A figure the
    // camera cannot see is still built and still drawn, because the shadow pass
    // reads this same buffer and a body behind the player casts into the frame
    // in front of him — but all that survives of it is a silhouette in a 4096
    // map over 62 m, i.e. 15 mm a texel. The 1.5% of radius that separates a
    // 16-gon from a 6-gon on a 160 mm torso is a tenth of one of those texels.
    // Off by default (g_cam_cull), so every proof that draws a figure outside
    // render_frame still gets the tier its own DIST argument asked for.
    if (g_cam_cull &&
        !frustum_sees(&g_cam_frustum, (v3){{pos.x, pos.y + 0.9f, pos.z}},
                      SCENE_CHUNK_R) &&
        g_fig_k > 6)
      g_fig_k = 6;   // a CEILING, like g_fig_kcap: it may never raise a tier
  }
  {
    v3 bc = {{pos.x, pos.y + 0.9f, pos.z}};
    // NOT BUILT AT ALL. Culling the DRAW still pays for the vertices: the
    // shadow pass reads the whole buffer, and scene_build is the one part of
    // the frame that is pure main-thread CPU. A body outside the frame whose
    // shadow segment is also outside it contributes nothing to any pass.
    if (g_cam_cull && !frustum_sees(&g_cam_frustum, bc, SCENE_CHUNK_R) &&
        !frustum_sees_cap(&g_cam_frustum, bc, shadow_foot(bc), SCENE_CHUNK_R))
      return;
    scene_chunk_begin(bc, SCENE_CHUNK_R);
  }
  // Interpolate the solved skeleton between the last two ticks.
  pose_t P;
  {
    const float *p0 = (const float *)&A->prev_pose;
    const float *p1 = (const float *)&A->pose;
    float *po = (float *)&P;
    for (unsigned k = 0; k < sizeof(pose_t) / sizeof(float); k++)
      po[k] = p0[k] + (p1[k] - p0[k]) * alpha;
  }
  // The corpse solve (gravity drape + tip + settle) rewrites the interpolated
  // pose in place; a living figure pays nothing for it, not even the ground ray.
  if (dth && dth->slack > 0.0001f) fig_death_pose(&P, dth, pos, yaw, wpn);
  // THE THROW IS THROWN WITH THE BODY (grenade, see NADE_HOLD). The trunk
  // turns about the vertical through the pelvis (tw: + takes the throwing
  // shoulder back) and pitches over the hips (bd: + brings the chest forward),
  // as one rigid move of everything above the waist — the same construction
  // the lean uses. The head only half-turns: the eyes stay on the target.
  // Applied to the interpolated pose BEFORE anything is built from it, so the
  // torso, the shoulders and the arms all come from the moved skeleton.
  if ((!dth || dth->slack <= 0.0001f) && wpn == WPN_GR &&
      (fabsf(gs.tw) > 1e-4f || fabsf(gs.bd) > 1e-4f)) {
    v3 F = {{P.head_f.x, 0, P.head_f.z}};
    F = v3_dot(F, F) > 1e-6f ? v3_norm(F) : (v3){{sinf(yaw), 0, -cosf(yaw)}};
    v3 U = {{0, 1, 0}};
    v3 Rt = v3_cross(F, U);
    v3 pv = P.pelvis;
    #define FIG_TURN(p_, k_) do {                                                   \
      float a_ = -gs.tw * (k_), c_ = cosf(a_), s_ = sinf(a_);                         \
      v3 d_ = v3_sub((p_), pv);                                                     \
      float r_ = v3_dot(d_, Rt), f_ = v3_dot(d_, F);                                 \
      d_ = v3_add(d_, v3_add(v3_scale(Rt, r_ * c_ - f_ * s_ - r_),                   \
                             v3_scale(F, f_ * c_ + r_ * s_ - f_)));                 \
      float b_ = gs.bd, cb_ = cosf(b_), sb_ = sinf(b_);                             \
      float u2_ = v3_dot(d_, U), f2_ = v3_dot(d_, F);                               \
      d_ = v3_add(d_, v3_add(v3_scale(U, u2_ * cb_ - f2_ * sb_ - u2_),              \
                             v3_scale(F, f2_ * cb_ + u2_ * sb_ - f2_)));            \
      (p_) = v3_add(pv, d_);                                                        \
    } while (0)
    for (int i = 0; i < 2; i++) {
      FIG_TURN(P.sho[i], 1.0f);
      FIG_TURN(P.elb[i], 1.0f);
      FIG_TURN(P.wri[i], 1.0f);
    }
    FIG_TURN(P.chest, 1.0f);
    FIG_TURN(P.neck, 1.0f);
    FIG_TURN(P.gun_o, 1.0f);
    FIG_TURN(P.head, 0.4f);
    #undef FIG_TURN
    {   // the right axis turns with the shoulders (no bend: it is horizontal);
      // the angle is negated as in FIG_TURN: + is the throwing shoulder BACK
      float c_ = cosf(gs.tw), s_ = sinf(-gs.tw);
      float r_ = v3_dot(P.right, Rt), f_ = v3_dot(P.right, F);
      P.right = v3_add(v3_scale(Rt, r_ * c_ - f_ * s_), v3_scale(F, f_ * c_ + r_ * s_));
    }
  }

  // No painted blob under the figure any more: the sun casts a real silhouette
  // now, which lands where the light says instead of always straight down, tracks
  // a jump on its own, and shows the pose. The disc that used to be here was a
  // hard-edged octagon that fought the real shadow it overlapped — and it was
  // also a CASTER, so it threw a second disc shadow of its own.
  g_fig_y = pos.y;
  kit_t kt = fig_kit(palette, flash01, flash_col);
  v3 Rc = v3_norm(P.right);
  v3 spine = v3_norm(v3_sub(P.chest, P.pelvis));
  v3 prt = v3_norm(v3_sub(P.hip[1], P.hip[0]));      // pelvis right
  // THE WEAPON SWAP (see SWAP_A): which weapon is in the hands this frame and
  // how far through putting one away / drawing one he is.
  int living = !dth || dth->slack <= 0.0001f;
  int hold = wpn, sw_phase = 0;
  float sw_k = 0.0f;
  // Carried kit is drawn for the local player always and for anyone else only
  // inside 12 m — it is a rifle and a half of vertices per figure.
  int carry = living && g_fig_ent >= 0 && g_fig_ent < MAX_ENTS &&
              (g_fig_ent == g_local_ent || g_fig_k >= 12);
  if (living && g_fig_ent >= 0 && g_fig_ent < MAX_ENTS) {
    int e = g_fig_ent;
    float now = ((float)g_tick + alpha) * TICK_DT;
    if (g_sw_seen[e] != wpn + 1) {
      if (g_sw_seen[e]) { g_sw_from[e] = g_sw_seen[e] - 1; g_sw_t0[e] = now; }
      else g_sw_t0[e] = -1e9f;
      g_sw_seen[e] = wpn + 1;
    }
    if (gs.trig > 0.05f) g_sw_t0[e] = -1e9f;   // a shot ends it at once
    int from = g_sw_from[e];
    float t = now - g_sw_t0[e];
    float da = (from == WPN_AR || from == WPN_SR || from == WPN_PT) ? SWAP_A : 0.0f;
    float db = (wpn == WPN_AR || wpn == WPN_SR || wpn == WPN_PT) ? SWAP_B : 0.0f;
    if (carry && from != wpn && t >= 0.0f && t < da + db) {
      if (t < da) { sw_phase = 1; sw_k = t / da; hold = from; }
      else        { sw_phase = 2; sw_k = (t - da) / db; }
    }
  }
  int holster_pt = carry && hold != WPN_PT && mode_allowed(g_fig_ent, WPN_PT);

  fig_torso(&P, &kt, Rc, spine, prt);
  // Neck + head + helmet, all in the look frame. The neck sinks into both the
  // chest and the skull, the helmet wraps the crown with its own brim ring —
  // no shared plane anywhere, so nothing can flicker between them.
  // Still inline rather than a fig_head builder — see the FIG_INLINE note above.
  {
    // The head's basis comes from `pose_head_frame` — ONE copy, shared with the
    // weld proof, because a proof that measures a different head from the one
    // drawn measures nothing. It carries the CANT: a shooter does not hold his
    // head plumb over a stock that is 30 degrees off his shoulder line, he lays
    // the cheek ONTO it and the crown goes with the cheek. Without it the weld
    // is a pure translation and the head arrives at the sight bolt upright,
    // which is the one thing nobody does behind a rifle.
    v3 hf, hr, hu;
    pose_head_frame(&P, anim_head_cant(A), NULL, &hr, &hu, &hf);
    // A point in the HEAD's own frame: P.head + hr*a (across) + hu*b (up) +
    // hf*c (forward). Same idiom as gun_build's GUNP and this function's own
    // BOOTN/WELTN — and the same reason: without it every node in here spends
    // two physical lines and two nested v3_adds saying three numbers, in two
    // different associations, and what the node actually IS disappears into the
    // boilerplate.
    #define HP(a, b, c) v3_add(P.head, v3_add(v3_scale(hr, (a)), \
                        v3_add(v3_scale(hu, (b)), v3_scale(hf, (c)))))
    mat_set(MAT_CLOTH_R, 0.0f);   // balaclava: the whole head is cloth now
    // The insurgent's head is the WRAP's cloth (a shemagh over face and neck)
    // with one dark band across the eyes; the operator's is the balaclava.
    int ins = FIG_INS;
    // The scarf across the face is the wrap's cloth a shade paler (a shemagh
    // is a light check), so wrap and face read as two windings of one cloth.
    v3 face = ins ? v3_lerp(kt.helm, (v3){{0.80f, 0.78f, 0.72f}}, 0.30f) : kt.bala;
    v3 slit = kt.bala;            // the eye line stays the dark cloth on both
    // Neck, MEASURED: ANSUR II (n=4082) gives 433 mm base circumference and 395
    // at the narrow point — 138 and 126 mm across. This was 162 -> 116, so the
    // base was 24 mm too thick while the top was 10 mm too thin, which put the
    // top neck ring WIDER than the jaw sitting on it. That inversion is the whole
    // bull-neck read; no amount of shading fixes a neck thicker than a head.
    // The lower bands are the uniform's COLLAR, not skin. Between the carrier
    // yoke and the helmet's rear edge sat ~118 mm of bare skin at twice the
    // uniform's albedo — measured as the brightest, highest-contrast surface on
    // the whole figure, and from behind the eye goes straight to it. A collar
    // leaves only the throat under the jaw showing, which is where skin belongs.
    fnode_t nk[5] = {
      {v3_add(P.chest, v3_scale(spine, 0.042f)), kt.vest,  kt.vest,  0.076f, 0.079f, 0, 0.70f},
      {v3_add(P.chest, v3_scale(spine, 0.092f)), kt.vest,  kt.vest,  0.070f, 0.073f, 0, 0.72f},
      {v3_add(P.chest, v3_scale(spine, 0.126f)), kt.olive, kt.olive, 0.066f, 0.068f, 0, 0.76f},
      // Held BELOW the mandible. At 0.152 the collar's top ring crossed the jaw
      // ring, and two intersecting curved surfaces in different colours show that
      // intersection as a sawtooth notch across the chin — invisible while the
      // neck was skin-on-skin there, obvious the moment the collar was olive.
      {v3_add(P.chest, v3_scale(spine, 0.133f)), kt.olive, kt.olive, 0.064f, 0.066f, 0, 0.80f},
      // The throat band is balaclava now — the whole head is covered. Slightly
      // darker than the face cloth: it lives in the helmet's shadow.
      {v3_add(P.head, v3_scale(hu, -0.056f)), v3_scale(face, 0.82f),
       v3_scale(face, 0.82f), 0.058f, 0.061f, 0, 0.62f}};
    probe("neck");
    fig_chain(nk, 5, 12, 1, 1, Rc);
    // Seven rings up the skull. A head is not an egg: the jaw is narrower than
    // the cheekbones, the cheekbones are the widest point, and the cranium is
    // wider than it is... no — it is DEEPER than it is wide (rz > rx), which is
    // what makes a profile read as a head rather than a ball. The chin ring also
    // sits FORWARD, so the jawline slopes instead of dropping vertically.
    // MEASURED against ANSUR II: head breadth 154 mm p50, head LENGTH (fore-aft)
    // 200. The skull measured 150 x 166 — the breadth was within 3%, the depth
    // was 17% short, and that one number is why the head read as a ball with a
    // face painted on it. It is also the pattern of the whole figure: every
    // width was authored against a front view and came out right, every DEPTH
    // was guessed and came out short.
    // The ring centres also shift BACK by ~10 mm, because the head joint here is
    // at the atlas (ear height): the face front sits ~88 mm ahead of it and the
    // occiput ~110 mm behind, not symmetrically. Growing rz alone would have
    // pushed the face forward through the goggles instead of filling the skull.
    fnode_t hd[7] = {  // chin -> jaw -> mouth -> cheek -> brow -> crown -> top
      // Cavity AO down the face: under the jaw and up under the helmet's brim
      // are both places light does not reach, and a head lit as if it were a
      // free-floating ball is most of why the face read as a flat patch. This
      // is the moving-geometry stand-in for the world's baked occlusion.
      // BALACLAVA head-to-toe: the bone-white skin wedge was the brightest
      // value on the whole model (a skull mask up close, a headlamp blob at
      // range) and the ramp contract says the face is never the peak. The
      // face is now goggles-over-dark-cloth; the goggle band carries the
      // facing cue with its one specular gleam.
      {HP(0, -0.0880f, 0.0210f), face, face, 0.0505f, 0.0570f, 0, 0.74f},
      {HP(0, -0.0640f, 0.0075f), face, face, 0.0680f, 0.0830f, 0, 0.80f},
      {HP(0, -0.0400f, -0.0035f), face, face, 0.0752f, 0.0930f, 0, 0.88f},
      {HP(0, -0.0056f, -0.0090f), slit, slit, 0.0790f, 0.0980f, 0, 0.94f},
      {HP(0, 0.0372f, -0.0110f), face, face, 0.0780f, 0.0965f, 0, 0.78f},
      {HP(0, 0.0818f, -0.0190f), face, face, 0.0692f, 0.0850f, 0, 0.0f},
      {HP(0, 0.1116f, -0.0250f), face, face, 0.0480f, 0.0605f, 0, 0.0f}};
    probe("skull");
    fig_chain(hd, 7, 16, 1, 1, hr);
    // Nose: the one feature that decides whether a head has a FRONT at 10 m. A
    // wedge, sunk into the face by more than its own depth so no cap shows.
    {
      // NO NOSE. A four-node chain modelling a nose under a balaclava, behind a
      // goggle band that now covers the whole eye line, contributed one soft
      // bump 19 mm across on a head that is 62 px tall at conversational range
      // — and it was the part that grazed the skull's own surface hardest,
      // because a small chain following a big ellipse is grazing it everywhere.
      // The face this figure needs is a dark cloth mass, one hard visor and two
      // ear covers; everything else is detail nobody resolves and geometry the
      // depth buffer has to arbitrate.
      // Ears, tucked under the helmet's ear covers. Two 12 mm chains, and the
      // head stops reading as a moulded blank the moment they are there.
      for (int e = 0; e < 2; e++) {
        float es = e ? 1.0f : -1.0f;
        fnode_t er[2] = {
          {HP(es * 0.068f, -0.030f, 0), face, face, 0.014f, 0.020f, 0, 0.0f},
          {HP(es * 0.082f, -0.026f, 0), face, face, 0.011f, 0.017f, 0, 0.0f}};
        probe("ear");
        fig_chain(er, 2, 8, 1, 1, hu);
      }
    }
    int head = FIG_HEAD;
    if (head == HEAD_WRAP) {
      // THE HEAD WRAP. Five rings over the crown, each 9-15 mm clear of the
      // skull station under it and on the skull's own fore/aft centre line, so
      // the two surfaces never run parallel; the bottom cap meets the skull at
      // a right angle and reads as the wrap's folded edge over the brow.
      // FULL value jitter: each ring its own shade, which is exactly what a
      // cloth wound round a head in turns looks like (a shell damps it).
      g_fig_jit = 1.0f;
      v3 wdk = v3_scale(kt.helm, 0.80f);
      fnode_t wr[5] = {
        // Wound cloth, not a shell: a fat fold over the brow, the bulk carried
        // HIGH and wide, and a broad flat crown — a helmet tapers to a dome,
        // a wrap does not.
        // Alternate turns a shade apart: the stripes are the winding.
        {HP(0, 0.010f, -0.010f), kt.helm, wdk, 0.0900f, 0.1080f, 0, 0.55f},
        {HP(0, 0.032f, -0.011f), wdk, kt.helm, 0.0970f, 0.1140f, 0, 0.0f},   // the fold
        {HP(0, 0.060f, -0.015f), kt.helm, wdk, 0.0920f, 0.1080f, 0, 0.0f},
        {HP(0, 0.095f, -0.022f), wdk, kt.helm, 0.0880f, 0.1020f, 0, 0.0f},
        {HP(0, 0.124f, -0.026f), kt.helm, kt.helm, 0.0620f, 0.0740f, 0, 0.0f}};
      probe("wrap");
      fig_chain(wr, 5, 8, 1, 1, hr);
      // ...and its TAIL, hanging down the back of the neck: rooted inside the
      // wrap, it leaves the cloth on a diagonal (never parallel to it) and
      // stands >= 4 mm off the back of the skull below.
      v3 tl = v3_scale(kt.helm, 0.88f);
      fnode_t tw3[4] = {
        {HP(0, 0.028f, -0.108f), tl, tl, 0.040f, 0.010f, 0, 0.0f},
        {HP(0, -0.010f, -0.134f), tl, tl, 0.046f, 0.009f, 0, 0.0f},
        {HP(0, -0.072f, -0.132f), tl, tl, 0.044f, 0.009f, 0, 0.0f},
        {HP(0, -0.118f, -0.128f), tl, tl, 0.034f, 0.008f, 0, 0.0f}};
      probe("wraptail");
      fig_chain(tw3, 4, 8, 1, 1, hr);
      g_fig_jit = 1.0f;
    } else {
      if (head == HEAD_HELMET) {
      mat_set(MAT_HELM_R, 0.0f);           // helmet: a hard shell, not cloth
      // THE BUCKET, diagnosed by reading the radii rather than the screenshot.
      // The old shell held rx between 0.088 and 0.093 from hu -0.024 all the way
      // to +0.104 — a CYLINDER 128 mm tall — and then closed it with a domed lid.
      // That is a coal scuttle, and it was the single largest silhouette element
      // on the figure. It was also 200 mm deep against 244 needed: a helmet as
      // deep as it is wide reads as a pot, one a quarter longer than wide reads as
      // a helmet. Nine rings now taper CONTINUOUSLY from the brim line up.
      // Sized from the head it has to fit: ANSUR head breadth 154 + 2*(15 mm pad +
      // 7 mm shell) = 198, head length 200 + 44 = 244. Every station is checked
      // clear of the skull below it by >= 12 mm.
      // The crown BALL is gone with the cylinder. A ball wide enough to close a
      // dome is necessarily wider than the shell near its own equator and narrower
      // further down, so the two surfaces CROSS — and along a crossing line depth
      // is decided by rounding, which is a hairline seam that wanders as the head
      // turns. figcheck cannot see it (two curved surfaces that intersect are
      // nowhere coplanar), and the old pairing was 0.1 mm from tangent at one
      // ring. Tapering to an 36 mm final ring costs one flat disc at the very top
      // of a helmet, which no camera in this game can get above.
      // FACETED shell, k=8: the redesign's helmet is a designed low-poly object
      // with visible plane breaks, not a smooth dome — the dome was the single
      // largest smooth surface on the figure and the loudest "vinyl toy" tell
      // against the arena's chamfered crates. Ring logic unchanged (nape flap,
      // buried front edges, the LIP as a fat node of the one shell — see the
      // history above fig_chain for why a nested brim cannot work). Grown ~4%
      // toward the x1.15 stylised helmet; every station still clears the skull
      // by >= 12 mm. Value jitter is DAMPED: a shell is one moulded object, and
      // full crate-jitter striped it horizontally.
      g_fig_jit = 0.35f;
      fnode_t hm[9] = {
        // Nape pulled 8 mm further down and back and 8 mm wider: at (0.054,
        // 0.064) against the base ring's (0.088, 0.095) 30 mm away, the shell
        // flared 49 degrees in one step and fig_chain's own bevel folded back
        // through the ring behind it — three helm_shell/helm_shell crossings, a
        // surface intersecting ITSELF. 36 degrees does not fold.
        {HP(0, -0.076f, -0.066f), kt.helm, kt.helm, 0.0620f, 0.0720f, 0, 0.42f},  // nape flap
        // Base dropped 8 mm and the LIP pulled in 5 mm, for the same reason the
        // nape moved: the brim flared 31 mm of radius in 25 mm of run — 51
        // degrees — and fig_chain's bevel folded the flare back through the ring
        // behind it, so the shell intersected ITSELF for a 25 mm seam right on
        // the brow line. 39 degrees does not fold, and the brim still reads as a
        // brim because it is still the widest ring on the helmet by 12 mm.
        {HP(0, -0.058f, -0.044f), kt.helm, kt.helm, 0.0880f, 0.0950f, 0, 0.55f},  // base
        {HP(0, -0.032f, -0.020f), kt.helm, kt.helm, 0.1075f, 0.1215f, 0, 0.0f},   // THE LIP
        {HP(0, -0.016f, -0.014f), kt.helm, kt.helm, 0.1040f, 0.1200f, 0, 0.0f},
        {HP(0, 0.024f, -0.012f), kt.helm, kt.helm, 0.1050f, 0.1205f, 0, 0.0f},
        {HP(0, 0.066f, -0.011f), kt.helm, kt.helm, 0.1000f, 0.1140f, 0, 0.0f},
        {HP(0, 0.104f, -0.012f), kt.helm, kt.helm, 0.0885f, 0.1000f, 0, 0.0f},
        {HP(0, 0.136f, -0.014f), kt.helm, kt.helm, 0.0680f, 0.0765f, 0, 0.0f},
        {HP(0, 0.156f, -0.016f), kt.helm, kt.helm, 0.0330f, 0.0375f, 0, 0.0f}};
      probe("helm_shell");
      fig_chain(hm, 9, 8, 1, 1, hr);
      g_fig_jit = 1.0f;
      // (The brim is a node of the shell above — see the LIP comment. It used to
      // be its own chain and could not be kept proud of a pushed-back base ring.)
      // NVG mount: the shrouded plate on the front of every modern helmet. Two
      // boxes, and it does more for "this is a soldier" than any amount of shell
      // resolution because it is the one asymmetric hard-edged thing up there.
      // NO NVG SHROUD. Two boxes stacked on the brow, and at the size a head
      // actually draws they read as a lump on the forehead rather than as a
      // mount — the same argument that took the nose chain and the goggle straps
      // off this head. They also crossed the shell they sat on. The helmet keeps
      // its asymmetric hard-edged detail in the side rails, which are on the axis
      // the eye reads a head by.
      mat_set(MAT_HELM_R, 0.0f);
      // Side rails / ear covers: two short pads over the temples. They also give
      // the helmet a left-right axis, which a dome does not have.
      for (int e = 0; e < 2; e++) {
        float es = e ? 1.0f : -1.0f;
        // Pushed out with the k=8 shell (rx ~0.107 at this height): the cover's
        // outer face rides 3 mm proud — buried is invisible, far proud is lumps.
        v3 ec = HP(es * 0.083f, -0.024f, 0);
        fnode_t er[2] = {
          {v3_add(ec, v3_scale(hf, 0.028f)), kt.helm, kt.helm, 0.024f, 0.020f, 0, 0.0f},
          {v3_add(ec, v3_scale(hf, -0.044f)), kt.helm, kt.helm, 0.027f, 0.024f, 0, 0.0f}};
        probe("helm_ear");
        fig_chain(er, 2, 8, 1, 1, hu);
      }
      }   // head == HEAD_HELMET
      if (head == HEAD_CAP) {
        // THE CAP: a soft crown fitted over the skull and a stiff bill. Each
        // crown ring stands 9-12 mm clear of the skull station under it, on the
        // skull's own fore/aft centre line (the helmet's rule: two surfaces that
        // never run parallel never seam). It sits HIGH, above the eye line, so
        // the dark slit of the balaclava under it carries the face; no goggles —
        // a band over the brow would cross the crown's lowest ring.
        mat_set(MAT_CLOTH_R, 0.0f);
        g_fig_jit = 0.45f;
        v3 cdk = v3_scale(kt.helm, 0.86f);
        fnode_t cp[6] = {
          {HP(0, 0.008f, -0.012f), cdk, kt.helm, 0.0780f, 0.0960f, 0, 0.55f},  // buried root
          {HP(0, 0.022f, -0.011f), kt.helm, kt.helm, 0.0900f, 0.1080f, 0, 0.0f},  // the band
          {HP(0, 0.050f, -0.013f), kt.helm, kt.helm, 0.0880f, 0.1050f, 0, 0.0f},
          {HP(0, 0.088f, -0.019f), kt.helm, kt.helm, 0.0790f, 0.0950f, 0, 0.0f},
          {HP(0, 0.118f, -0.025f), kt.helm, kt.helm, 0.0590f, 0.0720f, 0, 0.0f},
          {HP(0, 0.134f, -0.027f), kt.helm, kt.helm, 0.0280f, 0.0340f, 0, 0.0f}};
        probe("cap_crown");
        fig_chain(cp, 6, 8, 1, 1, hr);
        // The bill: a thin flat plate out of the band's front, rooted 20 mm
        // inside the crown and pitched a few degrees down, as a worn bill is.
        v3 bill = v3_scale(kt.helm, 0.78f);
        fnode_t bl[4] = {
          {HP(0, 0.026f, 0.070f), bill, bill, 0.060f, 0.0070f, 0, 0.0f},
          {HP(0, 0.024f, 0.104f), bill, bill, 0.068f, 0.0065f, 0, 0.0f},
          {HP(0, 0.018f, 0.142f), bill, bill, 0.064f, 0.0060f, 0, 0.0f},
          {HP(0, 0.012f, 0.166f), bill, bill, 0.046f, 0.0055f, 0, 0.0f}};
        probe("cap_bill");
        fig_chain(bl, 4, 8, 1, 1, hr);
        // ...and the button on top, so the crown has a centre.
        fnode_t bt[2] = {
          {HP(0, 0.132f, -0.027f), cdk, cdk, 0.012f, 0.012f, 0, 0.0f},
          {HP(0, 0.142f, -0.027f), cdk, cdk, 0.009f, 0.009f, 0, 0.0f}};
        probe("cap_button");
        fig_chain(bt, 2, 8, 1, 1, hr);
        g_fig_jit = 1.0f;
      }
      // Visor: ONE box spanning exactly the skull's flat front facet, which is
      // 2*0.42*rx wide for the chamfered profile. Two earlier attempts failed for
      // opposite reasons and both are worth remembering: a shallow bar sinks into
      // the nose bridge and reads as two disconnected patches, while a band CHAIN
      // that follows the face gets its corners bevel-cut (that is what fig_chain
      // does at every bend) and the chamfer edge then pokes through the cut as a
      // skin-coloured sliver. A single box inside the flat facet has no corner to
      // cut and nothing to sink into.
      // Goggles: a band that WRAPS the face, swept temple to temple along five
      // nodes that follow the skull's own curve. The box this replaces was sized to
      // the old chamfered head's flat front facet — once the skull became round
      // there was no flat facet left for it to sit in, and it stood off the face at
      // the cheeks like a pair of wings. A chain has no such assumption: it follows
      // whatever curve its nodes describe, and its ends bury themselves under the
      // helmet's ear covers.
      mat_set(MAT_GLASS_R, 0.0f);          // ballistic lens: the one wet gleam
      if (head != HEAD_CAP) {
        // Re-fitted to the deeper skull. The face front at eye height moved from
        // 0.076 to 0.087, which would have left the whole band 11 mm INSIDE the
        // face — a goggle you cannot see is worse than none, because the geometry
        // is still paid for. Each node now stands 9-11 mm proud of the skull
        // ellipse at its own lateral station, computed rather than nudged.
        // WIDER: the goggle band is the face now (the head is balaclava cloth
        // underneath), so it carries the whole facing cue and gets a third more
        // height — a wide dark visor with one wet gleam.
        // THICKER, AND FACETED. At 14 mm of depth the band stood 9-11 mm proud of
        // a skull it only bit 3-5 mm into, so over most of its length two curved
        // surfaces ran nearly PARALLEL a couple of millimetres apart — which is
        // the wandering depth seam this file calls a z-glitch, and which figcheck
        // cannot see because two curved surfaces that intersect are nowhere
        // coplanar. 24 mm buries it properly. The profile drops from 12-gon to
        // 8-gon at the same time: a visor is a moulded plate with plane breaks,
        // not a tube, and a flat band grazing a round skull grazes it in four
        // places instead of everywhere.
        const float gz_[5] = {-0.072f, -0.050f, 0.000f, 0.050f, 0.072f};  // across
        const float gd_[5] = {-0.026f, 0.056f, 0.080f, 0.056f, -0.026f};  // forward
        const float gu_[5] = {0.006f, 0.016f, 0.020f, 0.016f, 0.006f};    // up
        const float gr_[5] = {0.016f, 0.034f, 0.038f, 0.034f, 0.016f};    // height
        fnode_t gg[5];
        // The temple ends are a QUIET step, not a bright one. At 1.55 the rim
        // read as two pale wings either side of a dark visor — the loudest thing
        // on a head that is supposed to be one dark mass with one hard band
        // across it, and the first thing the eye found on the whole figure.
        v3 gfr = v3_scale(kt.bala, 1.12f);   // frame rim: a step over the cloth,
        for (int q = 0; q < 5; q++)          // still far under the helmet
          gg[q] = (fnode_t){.p = HP(gz_[q], gu_[q], gd_[q]),
                            .col = (q == 0 || q == 4) ? gfr : kt.gm2,
                            .bcol = (q == 0 || q == 4) ? gfr : kt.gm2,
                            .rx = gr_[q], .rz = 0.024f};
        probe("goggle");
        fig_chain(gg, 5, 8, 1, 1, hu);
        // NO STRAP. It existed because "goggles that stop at the temples look
        // like a sleep mask" — true of the narrow band it was written for, and no
        // longer true of this one: the band is 144 mm across and the helmet's ear
        // covers sit at 178, so its ends bury under them with nothing left
        // hanging in the air. What the strap actually contributed at the size a
        // head renders was two pale coyote strips either side of the visor,
        // poking out from under the helmet brim — the brightest thing on the
        // figure's face and the first thing the eye found on the whole model.
        // Two chains per head x 21 heads, removed.
      }
      // Chin strap: two thin runs from the ear covers down under the jaw. Nothing
      // says "helmet" like the thing that holds it on.
      mat_set(MAT_GEAR_R, 0.0f);
      if (head == HEAD_HELMET) {
      for (int e = 0; e < 2; e++) {
        float es = e ? 1.0f : -1.0f;
        fnode_t cs[3] = {
          // 0.078, not 0.089: the top node's outer edge stood at 99 mm where the
          // shell has already narrowed past it, so the strap emerged THROUGH the
          // helmet for five pairs. It hangs off the ear cover, so it belongs
          // inside its shadow.
          {HP(es * 0.084f, -0.028f, 0), kt.gear, kt.gear, 0.010f, 0.008f, 0, 0.0f},
          {HP(es * 0.066f, -0.088f, 0), kt.gear, kt.gear, 0.009f, 0.007f, 0, 0.0f},
          {HP(es * 0.024f, -0.098f, -0.014f), kt.gear, kt.gear, 0.009f, 0.007f, 0, 0.0f}};
        probe("chinstrap");
        fig_chain(cs, 3, 8, 1, 1, hf);
      }
      // Low-profile NVG brow shroud bolted flush to the helmet's front facet in
      // the head's own canted frame (hr, hu, hf), tagged cleanly as its own part.
      {
        v3 sh_col = v3_scale(kt.web, 0.90f);
        fnode_t nv[3] = {
          {HP(0, 0.054f, 0.098f), sh_col, sh_col, 0.024f, 0.016f, 0, 0.0f},
          {HP(0, 0.074f, 0.094f), sh_col, sh_col, 0.026f, 0.017f, 0, 0.0f},
          {HP(0, 0.092f, 0.086f), sh_col, sh_col, 0.020f, 0.014f, 0, 0.0f}};
        probe("nvg");
        g_fig_jit = 0.30f;
        fig_chain(nv, 3, 8, 1, 1, hr);
        g_fig_jit = 1.0f;
      }
      }   // head == HEAD_HELMET: chin strap and NVG shroud
    }
    #undef HP
  }
  // Legs: the chain starts INSIDE the pelvis, so the thigh's end cap can never
  // peek out of the hip, and the knee bevel lands as a faceted pad.
  // Still inline rather than a fig_legs builder — see the FIG_INLINE note above.
  mat_set(MAT_CLOTH_R, 0.0f);
  for (int i = 0; i < 2; i++) {
    v3 td = v3_sub(P.knee[i], P.hip[i]);
    td = v3_dot(td, td) > 1e-6f ? v3_norm(td) : v3_scale(spine, -1.0f);
    v3 sd = v3_sub(P.ankle[i], P.knee[i]);
    sd = v3_dot(sd, sd) > 1e-6f ? v3_norm(sd) : v3_scale(spine, -1.0f);
    // TWO chains and a ball, not one chain through the knee. Each is collinear
    // with its own bone, so neither contains a bend at all and fig_chain's bevel
    // clamp — which used to thin the cross-section by whatever the bones could not
    // pay for, and collapsed a deeply flexed knee into two cone points — never
    // fires on a leg again. The ball covers the junction at every joint angle and
    // is the knee pad.
    v3 thv = v3_sub(P.knee[i], P.hip[i]);
    v3 shv = v3_sub(P.ankle[i], P.knee[i]);
    fnode_t th[6];
    int nth = 0;
    // The thigh reaches BACKWARD ALONG ITS OWN AXIS into the pelvis: the end
    // cap ends up buried whatever the leg does, and because the stub is
    // collinear with the thigh the hip contributes no bend either.
    // k=8 faceted with real quad mass: the legs join the crate language.
    th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(td, -0.090f)),
                          kt.pants, kt.pants, 0.046f, 0.050f, 0, 0.0f};
    // Narrowed ACROSS more than in depth, which is both the anatomy and the
    // fix for the one thing the first attempt broke: hip_span is 190 mm, so at
    // rx 0.1023 each thigh reached 7 mm PAST the midline and the two legs
    // intersected each other. They always had — at the old 0.110 the overlap
    // was 30 mm deep and steep enough not to graze — but a shallower overlap
    // grazes, which is the wandering seam this file keeps deleting. 0.090
    // leaves a 5 mm gap at the midline and nothing to cross. The fore/aft
    // radius keeps most of its mass: a thigh IS deeper than it is wide.
    // -7% on every station above the knee. ANSUR II p50 thigh circumference is
    // ~600 mm, i.e. 191 mm across; with a trouser over it this chain carried
    // 220-233, which is 30-40 mm of nothing. It is the one dimension on the
    // figure that was measurably over its own reference, and it is also where
    // the "heavy" read comes from — a torso reads its weight against the legs
    // under it. The knee-end node is UNCHANGED at 0.066/0.072: the knee ball is
    // sized to swallow exactly that, and moving it would re-open a z-race that
    // cost a round to close.
    th[nth++] = (fnode_t){P.hip[i], kt.pants, kt.pants, 0.0900f, 0.1079f, 0, 0.0f};
    if (i == 1) {
      // The drop-leg rig's LEG STRAP, as two fat nodes OF THIS CHAIN rather than
      // a band wrapped round it. A nested strap has to cross the thigh's surface
      // to bury its own edges, and two surfaces that cross while nearly PARALLEL
      // are a depth seam decided by rounding — the defect the rig below was just
      // fixed for, and the same lesson the glove's cuff records: prefer a fat
      // node to a nested chain. A fat node is one surface and cannot cross
      // anything. 6 mm proud of the taper it interrupts, which is a webbing
      // strap; stations are FRACTIONS of the bone so they can never overtake the
      // belly node whatever the thigh's length becomes.
      th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(thv, 0.28f)),
                            kt.pants, kt.pants, 0.0896f, 0.1085f, 0, 0.0f};
      th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(thv, 0.36f)),
                            kt.belt, kt.belt, 0.0882f, 0.1070f, 0, 0.0f};
    }
    th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(thv, 0.42f)),
                          kt.pants, kt.pants, 0.0855f, 0.1017f, 0, 0.0f};
    th[nth++] = (fnode_t){P.knee[i], kt.pants, kt.pants, 0.066f, 0.072f, 0, 0.0f};
    probe("thigh");
    g_fig_jit = 0.40f;   // same cap as the torso: cloth, not crates
    fig_chain(th, nth, 8, 1, 1, prt);
    g_fig_jit = 1.0f;
    fnode_t sh[4] = {
      {P.knee[i], kt.pants, kt.pants, 0.066f, 0.072f, 0, 0.0f},
      // Calf belly UP (0.076 -> 0.081): a shank with no calf is a pipe.
      {v3_add(P.knee[i], v3_scale(shv, 0.30f)), kt.pants, kt.pants, 0.081f, 0.083f, 0, 0.0f},
      {P.ankle[i], kt.dust1, kt.dust1, 0.048f, 0.050f, 0, 0.0f},          // ankle
      // ...and the shin keeps going 3 cm PAST the ankle, into the boot: a cap
      // sitting exactly at the ankle pokes out of the collar the moment the
      // foot pitches, which reads as a boot that came off.
      {v3_add(P.ankle[i], v3_scale(sd, 0.032f)), kt.dust1, kt.dust1, 0.035f, 0.037f, 0, 0.0f}};
    probe("shin");
    g_fig_jit = 0.40f;
    fig_chain(sh, 4, 8, 1, 1, prt);
    g_fig_jit = 1.0f;
    // The knee itself: a ball 4 mm proud of both tubes, so both of their flat end
    // caps are strictly inside it whatever the flex. Swept about the knee's own
    // hinge axis, which keeps its pole seams where the leg does not bend.
    {
      v3 hinge = v3_cross(td, sd);
      if (v3_dot(hinge, hinge) < 1e-8f) hinge = prt;
      mat_set(MAT_RUBBER_R, 0.0f);
      // >= 6 mm bigger than the tube ends it swallows (0.066/0.072) in EVERY
      // direction: at 0.072 the ball's surface came out tangent to the tube's at
      // some flex angles, which is 0.025 mm of separation — a z-fight.
      // ...and the middle axis was 0.076, i.e. 4 mm — the one direction that
      // BROKE that rule, and the one the tubes swing through when the knee
      // folds. Standing it never showed; in a deep crouch the shin leaves the
      // ball at 90 degrees and its skin runs alongside the ball's for a whole
      // ring at ~4 mm, which the arris chamfer then eats into. 0.084 puts every
      // axis at 12-13 mm. (Nothing here is coplanar, so figcheck's zfight tier
      // is blind to it by construction: two curved surfaces a hair apart are a
      // valid mesh. The eye is the instrument, and it saw it.)
      probe("knee");
      fig_joint(P.knee[i], td, sd, 0.079f, 0.092f, 0.12f, 8, kt.pad);
    }
    if (i == 1) {  // Drop-leg rig on the strong-side thigh. A loaded soldier's
      // widest point below the chest is his belt kit, and with nothing on the
      // thigh the legs taper straight out of the hips like a diver's.
      //
      // OUTBOARD, canted slightly forward, where a holster platform rides. The
      // cant used to be built from lo = cross(td, prt) with its sign picked by
      // `dot(lo, prt) < 0` — but lo is perpendicular to prt BY CONSTRUCTION, so
      // that test compares a mathematically ZERO quantity and its sign is the
      // rounding error in it. Measured over a turn and over a slide, out.fwd
      // flipped between -0.25 and +0.29 on CONSECUTIVE ticks: the whole rig
      // snapped ~30 degrees around the leg every frame. A cant is relative to
      // the body's forward, so build it out of the body's forward.
      v3 pfw = v3_norm(v3_cross(spine, prt));                      // pelvis fwd
      v3 out = v3_add(v3_scale(prt, 0.97f), v3_scale(pfw, 0.24f));  // ~14 deg
      out = v3_sub(out, v3_scale(td, v3_dot(out, td)));
      out = v3_dot(out, out) > 1e-6f ? v3_norm(out) : prt;
      mat_set(MAT_GEAR_R, 0.0f);
      // SEATED OFF THE THIGH'S OWN CROSS-SECTION, never off a constant. This
      // hung 68 mm off the thigh axis while the leg's radius there is 101-111 mm
      // (measured, along `out`) — so 88% of the pouch was inside the trouser and
      // its outer wall stood 0 to 7 mm proud of it. That is not a pouch, it is
      // two nearly-parallel surfaces less than a millimetre apart over 36 cm2,
      // and which one is in front is then decided by rounding: the wandering
      // depth seam the "nested surfaces must not CROSS" rule is about. figcheck
      // cannot see it — two curved surfaces that intersect are nowhere coplanar
      // — so it is measured instead: the pouch's own surface against the thigh's
      // swept surface, and the near-and-PARALLEL area must be 0.
      // fig_rmax is the profile's CIRCUMRADIUS, i.e. the leg's radius in its
      // WORST direction, so a stand-off measured from it holds whichever way
      // `out` points and follows the thigh if those dimensions ever move again.
      // th[1] is the hip ring and th[2] the belly ring; the rig hangs between
      // them.
      // ...measured on the ACROSS radius, not the circumradius of the whole
      // ellipse. The thigh is no longer nearly circular (it is 90 mm across and
      // 108 deep, which is what a thigh is), and `out` points across — so a
      // stand-off taken from the fore/aft radius pushed the pouch 18 mm off the
      // leg it is strapped to and it read as a box floating beside a knee. rx
      // is the radius in the direction that matters here; the fore/aft
      // direction only ever buries the inner face deeper, which is free.
      float rhip = fig_rmax(8, th[1].rx, th[1].rx);
      float rbel = fig_rmax(8, th[nth - 2].rx, th[nth - 2].rx);
      // Half-depth 40 mm at the belly: the outer face then stands >= 40 mm proud
      // of the leg in every direction, and the inner face is >= 25 mm buried in
      // it — the circumradius is ~15 mm over the inradius on this profile and
      // that difference is the whole margin budget. rx is the OUTWARD half-depth
      // and rz the half-width around the leg, because the hint below is `out`
      // and fig_ring lays rx along the hint.
      const float dls[4] = {0.040f, 0.090f, 0.150f, 0.196f};  // below the hip
      const float dlx[4] = {0.030f, 0.040f, 0.038f, 0.028f};  // depth, along out
      const float dlz[4] = {0.038f, 0.048f, 0.046f, 0.034f};  // width, round leg
      fnode_t dl[4];
      for (int q = 0; q < 4; q++) {
        float u = (dls[q] - dls[0]) / (dls[3] - dls[0]);
        dl[q] = (fnode_t){v3_add(v3_add(P.hip[i], v3_scale(out, rhip + (rbel - rhip) * u)),
                                 v3_scale(td, dls[q])),
                          kt.belt, kt.belt, dlx[q], dlz[q], 0, q ? 0.0f : 0.62f};
      }
      probe("dropleg");
      // The hint is `out`, NOT `td`. fig_chain consults the hint for the FIRST
      // ring only and every chain has to pick one that cannot align with its own
      // first bone — and this chain's first bone IS td, so the hint projected to
      // zero and the fallback anchored the cross-section to cross(td, WORLD UP).
      // Measured, that rolled the pouch through 107 degrees around the leg during
      // a plain turn-in-place, which is most of why the seam wandered. `out` is
      // perpendicular to td by construction and can never degenerate.
      fig_chain(dl, 4, 8, 1, 1, out);
      // LEG STRAPS: two webbing bands round the thigh that the holster hangs
      // from — on the thigh's own axis and hint, so their profile is the
      // thigh's profile 4% out, and they can never cross it.
      {
        const float su[2] = {0.20f, 0.40f};
        const float srx[2] = {0.0898f, 0.0864f}, srz[2] = {0.1083f, 0.1035f};
        v3 stc = v3_scale(kt.web, 1.10f);
        // Not on the coarsest tier (past 22 m): there the thigh is a 6-gon
        // whose flats the strap's own 6-gon crosses at a shallow angle, and a
        // 28 mm band is under a pixel at that range anyway.
        for (int q = 0; q < (g_fig_k > 6 ? 2 : 0); q++) {
          v3 c0 = v3_add(P.hip[i], v3_scale(thv, su[q]));
          fnode_t sb[2] = {
            {v3_add(c0, v3_scale(td, -0.014f)), stc, stc, srx[q] * 1.045f, srz[q] * 1.045f, 0, 0.0f},
            {v3_add(c0, v3_scale(td, 0.014f)), stc, stc, srx[q] * 1.045f, srz[q] * 1.045f, 0, 0.0f}};
          probe("legstrap");
          fig_chain(sb, 2, 8, 1, 1, prt);
        }
      }
      // ...and the pistol in it, when it is not in his hand.
      if (holster_pt) {
        gframe_t hf = holster_frame(&P, spine);
        probe("holster_gun");
        float gg = g_gun_girth;
        g_gun_girth = GUN_GIRTH_3P;
        gun_build(WPN_PT, hf.o, hf.rt, hf.up, hf.fw, 0.0f, 0.0f, 0.0f);
        g_gun_girth = gg;
        mat_set(MAT_GEAR_R, 0.0f);
      }
    }
    mat_set(MAT_LEATHER_R, 0.0f);   // boot + cuff
    // Boot: swept along the actual foot axis (which carries the toe pitch), so
    // the sole stays flat on the ground and lifts with the step.
    v3 fF = v3_sub(P.toe[i], P.ankle[i]);
    if (v3_dot(fF, fF) > 1e-6f) {
      fF = v3_norm(fF);
      // Node heights are chosen so every sole vertex lands ~2 mm above the
      // planted ankle's floor (ANIM_ANKLE_Y): a boot built symmetrically around
      // the ankle rides toe-down and digs its tip into the ground.
      // The SHOE hangs straight DOWN from the ankle (world `spine`), never along
      // the shin: offsetting along `sd` lifts the whole sole with shin
      // inclination — measured with `kine`, a planted foot floated up to 2.4 cm
      // at run speed. The CUFF below is what follows the shin. Node heights are
      // then chosen so every sole vertex sits 0..3 mm above the planted ankle's
      // floor (ANIM_ANKLE_Y); built symmetrically around the ankle instead, the
      // boot rides toe-down and digs its tip through the ground.
      // Six rings and a welt instead of four rings. The old boot was monotonic —
      // one 96 mm wedge that read as a black doorstop with no heel, no instep and
      // no toe box.
      //
      // The sole height is a CONTRACT, and shaping a boot by changing rx alone
      // breaks it: rx IS the half-height here, so a shorter heel section lifts its
      // own sole with it. Every node's centre is therefore placed at
      // (rx - BOOT_H) along the spine, which puts centre - rx on ONE plane
      // whatever that node's height is. k stays 8, not 12: the chamfered profile
      // has a genuinely FLAT bottom facet between its two lowest vertices, and a
      // round profile would put a single line of contact on the floor and lift the
      // sole 3% of its height off it.
      // THE SOLE PLANE IS PERPENDICULAR TO `spine`, BUT THE RINGS MARCH ALONG fF,
      // and those two facts do not commute. A ring at station T sits at
      // av + fF*T, so a PITCHED foot puts it fF.y*T off the plane through av —
      // below it at whichever end the pitch drops. Measured with `kine` over 120
      // ticks per speed, a PLANTED boot was 11.1 mm into the floor at 3 m/s and
      // 5.6 at 12 (worst at a walk, where the ankle pitch is largest relative to
      // the stride), and never once above it. That is a sinking boot, not a
      // rolling one.
      //
      // Lift the whole boot by exactly that much. The lowest sole vertex then
      // lands ON the plane through av at every pitch, the boot still rolls about
      // whichever end is loaded — which is what it already looked like it was
      // doing — and the contract collapses to something much stronger than
      // before: the rendered sole is at ankle.y - ANIM_ANKLE_Y, full stop. `kine`
      // therefore stops measuring the boot's arithmetic (which is now an
      // invariant) and starts measuring the FOOT CONTROLLER, which is the thing
      // that can actually be wrong.
      float sole_dip = fminf(fminf(fF.y * -0.086f, fF.y * 0.194f), 0.0f);
      v3 av = v3_add(P.ankle[i], v3_scale(spine, -0.028f - sole_dip));
      // A BOOT IS THREE PIECES, and building it as one wedge with a strip under
      // it is why it read as a moccasin however its outline was shaped. What the
      // eye actually uses to recognise a combat boot is a stack of three
      // horizontal lines at ankle height — lugged rubber OUTSOLE, pale MIDSOLE
      // wedge, leather UPPER — plus a laced instep. The old build had the first
      // two collapsed into one near-black `rub` strip that also stopped 25 mm
      // short of the toe (measured with figm: welt 196.9 mm deep against the
      // boot's 308.4), so the front third of every boot had no sole at all.
      //
      // The SOLE PLANE IS THE CONTRACT and it does not move: every piece places
      // its ring centres at (rx - H) along `spine`, which puts centre - rx on ONE
      // plane whatever that ring's height is, and the lowest of those planes is
      // still av - 0.048 — the number `kine` mirrors. Shaping any of these by
      // radius alone would lift its own sole with it.
      // k stays 8: the chamfered profile has a genuinely FLAT bottom facet
      // between its two lowest vertices, where a round one would stand the boot
      // on a single line of contact.
      // A MINIMAL BOOT IS TWO PIECES: a smooth rubber SLAB and a leather
      // UPPER set into it. What was here was five — lugged outsole, proud pale
      // welt, upper, lace strip and four cross-bar boxes — and every one of the
      // three that just died was authored for a distance nobody plays at.
      // Measured against this renderer's own 960/d px per metre: the welt is
      // 10.5 mm, i.e. 1.0 px at 10 m and 0.5 at 20; a lug stands 3.4 mm PROUD IN
      // rz, which side-on (the only angle a boot is ever read from) moves no
      // silhouette at all; the lace strip is 3 mm proud, 0.48 px at its own 6 m
      // gate. None of them is geometry at play distance — they are a stripe, a
      // texture and a stripe, and what they cost is real: the welt alone was the
      // most expensive chain on the boot, and the lug rows made the node count
      // TIER-DEPENDENT, which is the exact shape of the FIG_NMAX truncation this
      // file has already been bitten by once.
      //
      // What replaces the welt is not a line of paint but the SHAPE the k=6
      // profile makes: its z = +-1 vertices sit at x = 0, i.e. exactly mid-slab,
      // so the sweep produces one continuous ridge running heel to toe at
      // mid-sole height. Under a 38-degree sun that splits the sole into an
      // up-facing chamfer and a down-facing one — a hard horizontal line at the
      // sole, in the SAME albedo, for zero extra nodes and zero contrast budget.
      // k=6 also has the widest flat bottom facet of the faceted family
      // (1.04*rz against k=8's 0.84), which is 54 mm of ground contact on a
      // 104 mm sole instead of 44: a square-edged slab, not a rocker.
      //
      // 0.047, not 0.048: ANIM_ANKLE_Y is 0.075 and av is 28 mm under the ankle,
      // so this is the number that puts the sole exactly on the floor rather
      // than 1 mm into it. UNCHANGED, deliberately — `kine`'s `sole` column is
      // then byte-identical and the rebuild is provably contact-neutral.
      const float SOLE_H = 0.047f;    // sole floor — the ground contact
      // 0.021, not 0.026: with the welt gone the leather sits directly into the
      // slab, buried 3-14 mm inside it at every station, which is what closes
      // the "10 mm of open air between the upper's floor and the sole's top"
      // the welt used to fill.
      // 0.031, not 0.021, and this is the single biggest z-race on the figure.
      // Both chains put every ring's BOTTOM on their own plane, so the upper's
      // floor is a flat cap at -UPPR_H and the sole's top is a flat cap at
      // -SOLE_H + 2*rx — and at 0.021 those two ran within 0.2-1.0 mm of each
      // other, PARALLEL, across the whole middle of the foot (measured at nine
      // stations: -21.0 against -21.2, -21.7, -20.2, -22.0). Two parallel
      // surfaces a fifth of a millimetre apart is a depth-order coin flip that
      // wanders as the foot pitches, and `figv` counted ten crossing pairs on
      // this one join alone — more than any other pair on the model. 0.031
      // buries the leather 8-11 mm inside the slab everywhere and still leaves
      // 16 mm of rubber under it.
      const float UPPR_H = 0.031f;    // leather floor, well inside the slab
      #define SOLEN(H, T, RX, RZ, C, BC) {                                  \
        v3_add(v3_add(av, v3_scale(fF, (T))), v3_scale(spine, (RX) - (H))), \
        C, BC, (RX), (RZ), 0, 0.0f}
      // ANSUR foot length is 271 mm p50 and a combat boot's exterior is ~290;
      // this runs -0.086 to +0.194, i.e. 280 mm heel to toe with the ankle at
      // 31% of it from the heel (a real foot is ~25%).
      //
      // THOSE TWO EXTREMES ARE A CONTRACT, not a taste: `sole_dip` above and
      // `skel_solve`'s ankle-pitch pivots both name -0.086 and 0.194 as the
      // sole's own ends, and a boot whose ends move without them is a boot that
      // pitches through the floor. They are unchanged here for exactly that
      // reason — the rebuild is a change of SHAPE, not of extent.
      //
      // All of the shape that costs anything is in `rx` (the half-HEIGHT, which
      // the sole-plane contract pins: each ring centre goes at rx - H, so
      // centre - rx lands on one plane at every station). `rz` is free — it
      // moves no ring centre — so the width profile is where the toe box and the
      // waist live.
      mat_set(MAT_RUBBER_R, 0.0f);
      // One value step UNDER the leather, not over it. `kt.rub` measured 1.38x
      // LIGHTER than the upper it carries, which is a trainer's midsole read:
      // a pale band under a dark shoe. A combat sole is the darkest thing on the
      // boot and separates from the leather by FINISH (rubber 0.90 against
      // leather 0.50), not by value.
      // 0.78, not 0.92. MEASURED off the render, the slab and the leather sat
      // 0.03 stops apart — whatever separation the eye thought it saw was pure
      // silhouette and one specular highlight, so the two-piece boot was still
      // reading as one dark clog. Rubber against leather is a real material
      // step and is allowed more than this file's 1.15x internal cap, but it
      // has to stay under the boot's own silhouette against the ground.
      v3 sole_c = v3_scale(kt.boot, 0.78f);
      fnode_t so[7] = {
        SOLEN(SOLE_H, -0.086f, 0.0168f, 0.0410f, sole_c, sole_c),  // heel, squared
        SOLEN(SOLE_H, -0.050f, 0.0175f, 0.0448f, sole_c, sole_c),  // heel block
        SOLEN(SOLE_H, -0.026f, 0.0120f, 0.0424f, sole_c, sole_c),  // heel breast
        SOLEN(SOLE_H,  0.030f, 0.0134f, 0.0448f, sole_c, sole_c),  // waist
        SOLEN(SOLE_H,  0.090f, 0.0145f, 0.0520f, sole_c, sole_c),  // ball, widest
        SOLEN(SOLE_H,  0.152f, 0.0132f, 0.0486f, sole_c, sole_c),  // toe box
        SOLEN(SOLE_H,  0.194f, 0.0116f, 0.0300f, sole_c, sole_c)}; // toe end
      probe("outsole");
      // Half jitter. The faceted family's per-band value variation is what stops
      // a slab reading as an extrusion, so it does not go to zero — but with the
      // welt's cue gone it would otherwise be most of the sole's internal
      // contrast, and its edge-darkening term puts 0.29 stops on the band next
      // to the heel cap, which is one of the few end caps on this figure the
      // player actually sees.
      g_fig_jit = 0.5f;
      fig_chain(so, 7, 6, 1, 1, spine);
      g_fig_jit = 1.0f;
      mat_set(MAT_LEATHER_R, 0.0f);
      // The leather upper, set INTO the slab: narrower than it at every station
      // (3.3-4.2 mm of clearance per side) and with its floor buried inside it,
      // so no edge of the leather is ever the silhouette's bottom and the two
      // surfaces meet in exactly one place — a buried floor plate. Strictly
      // inside or strictly buried is this file's nesting rule; a part narrower
      // in one axis and wider in the other is the wandering depth seam.
      // WIDE-narrow from the ground up, now that the welt has gone: the old
      // narrow-WIDE-narrow stack existed only to make a pale line catch light.
      // Toe cap, vamp, throat and heel counter are four distinct stations rather
      // than one monotone wedge — the blunt high toe box and the heel breast
      // step are what say MILITARY once the lugs are gone.
      // The instep band is the one value step on the boot: 1.63x over the sole,
      // 0.37 of the boot's own 3.79x silhouette against this ground. The welt
      // ran 2.18x internal, i.e. 0.58 of the silhouette, which is the trainer
      // stripe the rebuild is about. The ramp also runs UPWARD now (dark sole to
      // lighter ankle), which is the direction the light comes from.
      // The boot measured 3.35 stops under the ground — DARKER THAN A FULL CAST
      // SHADOW here, which is 2.94 — so it read as a hole in the floor rather
      // than as an object standing on it. The leather comes up; `kt.boot` stays
      // where the figure's value-ramp contract pins it (boots are still the
      // darkest band on the body, and 1.40x is still under the trouser).
      v3 lea_c = v3_scale(kt.boot, 1.40f);
      v3 up_c = v3_lerp(lea_c, g_theme.ground, 0.20f);
      fnode_t bt[7] = {
        SOLEN(UPPR_H, -0.074f, 0.0290f, 0.0352f, lea_c, lea_c),      // counter
        SOLEN(UPPR_H, -0.040f, 0.0375f, 0.0404f, lea_c, lea_c),      // counter top
        SOLEN(UPPR_H,  0.012f, 0.0400f, 0.0402f, up_c,   up_c),       // throat
        SOLEN(UPPR_H,  0.068f, 0.0340f, 0.0452f, up_c,   up_c),       // instep
        SOLEN(UPPR_H,  0.124f, 0.0268f, 0.0462f, up_c,   up_c),       // vamp
        SOLEN(UPPR_H,  0.172f, 0.0206f, 0.0370f, lea_c, lea_c),      // toe cap
        // Toe tip in to 0.0262: the sole was 1.0 mm wider here, which is
        // inside the same coin flip the floors were in.
        SOLEN(UPPR_H,  0.194f, 0.0132f, 0.0262f, lea_c, lea_c)};     // toe tip
      probe("boot");
      fig_chain(bt, 7, 8, 1, 1, spine);  // sole down: rx is the boot height
      #undef SOLEN
      // Cuff: the shaft up the shin, decisively WIDER than the trouser it
      // swallows. Without it the trouser (the same radius as the shoe) occludes
      // the whole boot upper and all that survives is a 4 cm black wedge lying
      // on the floor — feet read as doorstops with no ankle and no heel counter.
      // The top ring is the BLOUSE — trouser folded over the boot top, wider
      // than the shin it swallows, which is what chunks the ankle; the ring
      // under it is the padded COLLAR, which is the widest point of a real boot
      // shaft and the thing that separates blouse from leather.
      // THE BLOUSE HAS TO OVERHANG, and the leather has to reach 8 inches. The
      // first build tapered monotonically from trouser to boot, so there was no
      // hem — just a cylinder changing colour — and the kt.boot section topped
      // out 121 mm off the ground against the 203 an 8-inch combat boot runs.
      // At play distance the leg was an olive tube ending in a brown lump.
      //
      // The load-bearing property is the 10 mm gap between the last PANTS node
      // and the first BOOT node: the olive ends WIDE and the collar under it is
      // 14 mm narrower per side, so that undercut compresses into a few rows
      // with a shadow in it — a lip, rather than a value step painted on a
      // smooth cone. Convex, not a waist: this file's own rule is never to put
      // the value step on the shape step, and a pinch at a colour boundary is
      // exactly that.
      fnode_t cf[5] = {
        {v3_add(P.ankle[i], v3_scale(sd, -0.150f)), kt.pants, kt.pants, 0.068f, 0.071f, 0, 0.0f},
        {v3_add(P.ankle[i], v3_scale(sd, -0.108f)), kt.pants, kt.pants, 0.078f, 0.081f, 0, 0.0f},
        {v3_add(P.ankle[i], v3_scale(sd, -0.086f)), kt.pants, kt.pants, 0.076f, 0.079f, 0, 0.0f},
        {v3_add(P.ankle[i], v3_scale(sd, -0.070f)), up_c, up_c, 0.062f, 0.065f, 0, 0.0f},
        {v3_add(P.ankle[i], v3_scale(sd,  0.014f)), up_c, up_c, 0.050f, 0.053f, 0, 0.0f}};
      probe("bootcuff");
      // The shaft leather is the UPPER's band, not the toe-cap's. With the welt
      // gone the boot's value ramp runs dark sole -> leather -> lighter ankle,
      // in the direction the light comes from; leaving the shaft on `kt.boot`
      // put a dark band ABOVE the lighter instep and broke the ramp at the one
      // ring the eye uses to find the top of the boot.
      // CLOTH, not leather. mat_set(MAT_LEATHER_R) is set for the boot above and
      // was never restored, so the trouser hem rendered at roughness 0.50 — a
      // shiny olive cuff, on the one band of this chain that is fabric.
      mat_set(MAT_CLOTH_R, 0.0f);
      g_fig_jit = 0.40f;
      fig_chain(cf, 5, 8, 1, 1, prt);
      g_fig_jit = 1.0f;
      mat_set(MAT_LEATHER_R, 0.0f);
      // NO LACING. It was a proud 6-gon strip plus four scene_box cross-bars,
      // gated at 12 m and 6 m — and 3 mm of proudness is 0.48 px even at 6 m, so
      // both were functioning as painted stripes rather than as geometry, on the
      // one part of the figure a minimal brief is loudest about. What tells the
      // eye this is a combat boot at any distance a player is ever at is the
      // three-line stack of blouse, shaft and slab, and all three survive.
    }
    mat_set(MAT_CLOTH_R, 0.0f);   // next iteration of the loop starts on trouser
  }
  // The rifle: the SAME model the player's own viewmodel is built from, anchored
  // so the trigger hand lands on the pistol grip and the support hand on the
  // handguard. The pose already places both wrists relative to P.gun_o, so the
  // anchor is a fixed offset in the model's own frame rather than a second set
  // of hand positions to keep in sync.
  //
  // It is BUILT before the arms, not merely framed before them, and that is a
  // dependency rather than a preference: gun_build is what publishes the grip
  // cylinders' scale and the TRIGGER contact point, and the hands close round
  // both. Built after, the trigger finger would aim at whatever weapon was drawn
  // last — on the player's own body, the previous frame's viewmodel.
  v3 g_org, g_rt, g_up, g_fw;
  pose_gun_frame(&P, wpn, &g_org, &g_rt, &g_up, &g_fw);
  g_gun_girth = GUN_GIRTH_3P;
  // SIDEARM AND BLADE: the pose was solved for a two-handed long gun, so the arms
  // are re-aimed here, the one place that knows what is in the hands. Wrists are
  // derived from the weapon's own grip cylinders, then both elbows are re-solved.
  if (wpn == WPN_KN || wpn == WPN_PT || wpn == WPN_GR) {
    v3 aim = v3_norm(P.head_f);
    v3 wup = {{0, 1, 0}};
    v3 ar = v3_cross(aim, wup);
    ar = v3_dot(ar, ar) > 1e-6f ? v3_norm(ar) : Rc;
    v3 au = v3_cross(ar, aim);
    if (wpn != WPN_PT) {
      // The same timeline the viewmodel plays, measured from the same eye, opened
      // out a little because an arm is longer than a viewmodel's foreshortening.
      g_rt = ar; g_up = au; g_fw = aim;
      rot2(&g_fw, &g_up, gs.kn[3]);
      rot2(&g_rt, &g_fw, gs.kn[4]);
      rot2(&g_rt, &g_up, gs.kn[5]);
      g_org = v3_add(P.head, v3_add(v3_scale(ar, gs.kn[0] * 1.15f),
              v3_add(v3_scale(au, gs.kn[1] * 1.15f - 0.06f), v3_scale(aim, gs.kn[2] * 1.20f))));
      // The off hand rides at the sternum and comes up behind the blade in guard.
      v3 lo_ = v3_add(P.chest, v3_add(v3_scale(ar, -0.17f),
               v3_add(v3_scale(au, -0.06f), v3_scale(aim, 0.26f))));
      v3 hi_ = v3_add(P.chest, v3_add(v3_scale(ar, -0.06f),
               v3_add(v3_scale(au, 0.15f), v3_scale(aim, 0.33f))));
      P.wri[0] = v3_lerp(lo_, hi_, gs.guard);
      // The throw's off arm points out at the target through the draw, the
      // counterweight a real thrower uses, and tucks in as the throw comes over.
      // Not while it is taking the pin (guard): that hand is busy at the ring.
      float lr_ = gs.lr * (1.0f - gs.guard);
      if (wpn == WPN_GR && lr_ > 1e-4f) {
        v3 pt_ = v3_add(P.chest, v3_add(v3_scale(ar, -0.45f),
                 v3_add(v3_scale(au, 0.02f), v3_scale(aim, 0.55f))));
        P.wri[0] = v3_lerp(P.wri[0], pt_, lr_);
      }
    }
    v3 gc, ga; float gr;
    gun_hold(wpn, GH_GRIP, g_org, g_rt, g_up, g_fw, &gc, &ga, &gr);
    v3 br = v3_sub(v3_scale(g_fw, -1.0f), v3_scale(ga, -v3_dot(g_fw, ga)));
    br = v3_norm(br);
    P.wri[1] = v3_add(gc, v3_add(v3_scale(ga, -0.046f * GUN_GIRTH_3P), v3_scale(br, gr + 0.066f)));
    if (wpn == WPN_PT) {
      gun_hold(wpn, GH_FORE3, g_org, g_rt, g_up, g_fw, &gc, &ga, &gr);
      br = v3_norm(v3_add(v3_scale(g_rt, -0.62f), v3_scale(g_fw, -0.78f)));
      P.wri[0] = v3_add(gc, v3_add(v3_scale(ga, -0.040f), v3_scale(br, gr + 0.066f)));
    }
    v3 p0 = v3_add(v3_scale(Rc, -0.7f), v3_scale(spine, -0.8f));
    v3 p1 = v3_add(v3_scale(Rc, 0.7f), v3_scale(spine, -0.8f));
    if (wpn == WPN_GR && gs.eu > 1e-4f)   // the throwing elbow up and out, back
      p1 = v3_lerp(p1, v3_add(v3_scale(Rc, 0.9f),
                       v3_add(v3_scale(spine, 0.30f), v3_scale(aim, -0.35f))), gs.eu);
    P.elb[0] = ik_solve(P.sho[0], P.wri[0], ANIM_UARM, ANIM_FARM, p0);
    P.elb[1] = ik_solve(P.sho[1], P.wri[1], ANIM_UARM, ANIM_FARM, p1);
  }
  // THE BODY RELOADS TOO. Filmed from outside at five stations, an AR magazine
  // change was a man standing perfectly still holding a rifle for 1.27 seconds
  // — the viewmodel had a whole choreography and the third-person figure had
  // nothing, which is the first/third-person divergence in its purest form. It
  // is the SAME timeline (`gun_anim`), applied to the pose instead of to a
  // camera-space offset: the weapon comes off the shoulder and cants so the
  // magwell faces the shooter, the support hand leaves the fore-end for the
  // magazine, and the trigger hand works the bolt on the sniper.
  // Gate on EVERY field the block consumes, not just arc/bhand: arc is 0 past
  // prog 0.5587 while gs.hand holds to 0.72, so the old gate cut the support
  // hand off the magazine in ONE tick just before the seat (filmed: a 25k px
  // frame-diff spike at exactly the gate's closing tick) and the AR's
  // charging-handle cant (cyc, window 0.60+) was unreachable outright.
  if (gs.arc + gs.cyc + gs.hand + gs.bhand > 0.001f) {
    // Cant about the BORE, pivoting at the trigger grip so the hand that stays
    // on the weapon does not have to travel. Half the viewmodel's roll: a
    // third-person figure is seen from outside, where 26 degrees already reads
    // clearly and 49 looks like the rifle is falling out of his hands.
    float cant = gs.arc * -0.46f + gs.cyc * (wpn == WPN_SR ? 0.52f : 0.20f);
    float lower = gs.arc * 0.070f;      // off the shoulder, down and inboard
    v3 piv = P.wri[1];
    v3 nu = v3_add(v3_scale(g_up, cosf(cant)), v3_scale(g_rt, sinf(cant)));
    v3 nr = v3_add(v3_scale(g_rt, cosf(cant)), v3_scale(g_up, -sinf(cant)));
    v3 off = v3_add(v3_scale(g_up, -lower),
                    v3_scale(g_fw, -gs.arc * 0.050f - gs.cyc * 0.030f));
    v3 rel = v3_sub(g_org, piv);
    float ra_ = v3_dot(rel, g_rt), rb = v3_dot(rel, g_up), rc_ = v3_dot(rel, g_fw);
    g_org = v3_add(v3_add(piv, off),
                   v3_add(v3_scale(nr, ra_),
                          v3_add(v3_scale(nu, rb), v3_scale(g_fw, rc_))));
    g_rt = nr; g_up = nu;
    // Re-solve both elbows from wherever the hands end up: a wrist that moves
    // without its elbow following is an arm that stretches, which is the defect
    // fig_hand's own reach clamp exists to survive rather than to hide.
    v3 pole0 = v3_add(v3_scale(Rc, -0.6f), v3_scale(spine, -0.8f));
    v3 pole1 = v3_add(v3_scale(Rc, 0.6f), v3_scale(spine, -0.8f));
    if (gs.hand > 0.001f) {   // support hand -> the magazine
      v3 rc, ra; float rr;
      gun_hold(wpn, GH_RELD, g_org, g_rt, g_up, g_fw, &rc, &ra, &rr);
      rc = v3_add(rc, v3_add(v3_scale(g_up, -gs.mag * GUN_MAG_TRAVEL[wpn][0] * GUN_GIRTH_3P),
                             v3_scale(g_rt, -gs.mag * GUN_MAG_TRAVEL[wpn][1] * GUN_GIRTH_3P)));
      rc = v3_add(rc, v3_scale(gun_pouch_ofs(g_rt, g_up, g_fw, gs.pouch), GUN_GIRTH_3P));
      // The wrist stands off the magazine on the side the arm comes from, the
      // same construction the viewmodel uses — the hand is a fixed size and it
      // is the CYLINDER that has to be reachable, not the palm that stretches.
      v3 br = v3_sub(P.wri[0], rc);
      br = v3_sub(br, v3_scale(ra, v3_dot(br, ra)));
      br = v3_dot(br, br) > 1e-8f ? v3_norm(br) : v3_scale(g_rt, -1.0f);
      v3 want = v3_add(rc, v3_add(v3_scale(ra, -0.046f * GUN_GIRTH_3P),
                                  v3_scale(br, rr + 0.072f)));
      P.wri[0] = v3_lerp(P.wri[0], want, gs.hand);
    }
    if (gs.bhand > 0.001f && wpn == WPN_SR) {  // trigger hand -> the bolt knob
      v3 knob = gun_bolt_knob(gs.bolt, g_org, g_rt, g_up, g_fw, GUN_GIRTH_3P);
      v3 want = v3_add(knob,
                       v3_add(v3_scale(g_rt, 0.058f), v3_scale(g_up, 0.022f)));
      P.wri[1] = v3_lerp(P.wri[1], want, gs.bhand);
    }
    P.elb[0] = ik_solve(P.sho[0], P.wri[0], ANIM_UARM, ANIM_FARM, pole0);
    P.elb[1] = ik_solve(P.sho[1], P.wri[1], ANIM_UARM, ANIM_FARM, pole1);
  }
  if (sw_phase) {
    // Fly the weapon in the hands between the ready and its carry slot, and
    // the hands with it. gk is the GUN (0 = at the ready, 1 = seated); the
    // trigger hand holds the grip near the ready and the STOCK near the slot
    // (the grip of a rifle on a man's back is out of his reach, the butt
    // standing up behind his neck is not), the support hand only near the
    // ready. Off the gun, a hand closes on nothing (g_fig_free).
    int w = hold;
    gframe_t S, H;
    int ok = 1;
    if (w == WPN_PT) S = holster_frame(&P, spine);
    else ok = stow_frame(&P, Rc, spine, w, &S);
    pose_gun_frame(&P, w, &H.o, &H.rt, &H.up, &H.fw);
    if (ok) {
      v3 cfw = v3_norm(v3_cross(spine, Rc));
      v3 via = w == WPN_AR
             ? v3_add(P.sho[1], v3_add(v3_scale(spine, 0.17f), v3_scale(Rc, -0.05f)))
             : w == WPN_SR
             ? v3_add(P.chest, v3_add(v3_scale(spine, 0.06f),
                      v3_add(v3_scale(Rc, -0.12f), v3_scale(cfw, 0.24f))))
             : v3_add(P.hip[1], v3_add(v3_scale(spine, 0.20f),
                      v3_add(v3_scale(cfw, 0.16f), v3_scale(Rc, 0.08f))));
      float gk, att, sup;
      if (sw_phase == 1) {
        gk  = ease3(sw_k / 0.78f);
        att = 1.0f - ease3((sw_k - 0.80f) / 0.20f);
        sup = 1.0f - ease3(sw_k / 0.30f);
      } else {
        gk  = 1.0f - ease3((sw_k - 0.26f) / 0.74f);
        att = ease3(sw_k / 0.26f);
        sup = ease3((sw_k - 0.72f) / 0.28f);
      }
      gframe_t F = gframe_arc(w, &H, &S, via, gk);
      g_org = F.o; g_rt = F.rt; g_up = F.up; g_fw = F.fw;
      v3 fin0 = P.wri[0], fin1 = P.wri[1];
      v3 rest1 = v3_add(P.chest, v3_add(v3_scale(Rc, 0.17f),
                  v3_add(v3_scale(spine, -0.20f), v3_scale(cfw, 0.08f))));
      v3 rest0 = v3_add(P.chest, v3_add(v3_scale(Rc, -0.17f),
                  v3_add(v3_scale(spine, -0.20f), v3_scale(cfw, 0.08f))));
      v3 grip = gframe_wrist(w, &F, 1);
      if (w != WPN_PT) {
        v3 mn, mx;
        gun_box(w, &mn, &mx);
        v3 stock = v3_add(F.o, v3_add(v3_scale(F.fw, mn.z * 0.80f),
                          v3_scale(F.up, 0.5f * (mn.y + mx.y))));
        grip = v3_lerp(grip, v3_add(stock, v3_scale(spine, 0.03f)), ease3((gk - 0.25f) / 0.6f));
      }
      v3 w1 = v3_lerp(rest1, grip, att);
      v3 w0 = v3_lerp(rest0, gframe_wrist(w, &F, 0), sup);
      // The last stretch of the draw lands on the pose's OWN hold, so there is
      // no pop where the animation hands back to the ordinary carry.
      if (sw_phase == 2) {
        float e = ease3((sw_k - 0.82f) / 0.18f);
        w1 = v3_lerp(w1, fin1, e);
        w0 = v3_lerp(w0, fin0, e);
      }
      P.wri[1] = w1;
      P.wri[0] = w0;
      g_fig_free = (att < 0.5f || (w != WPN_PT && gk > 0.55f) ? 2 : 0) | (sup < 0.5f ? 1 : 0);
      v3 p0 = v3_add(v3_scale(Rc, -0.7f), v3_scale(spine, -0.8f));
      v3 p1 = v3_add(v3_scale(Rc, 0.7f), v3_scale(spine, -0.8f));
      P.elb[0] = ik_solve(P.sho[0], P.wri[0], ANIM_UARM, ANIM_FARM, p0);
      P.elb[1] = ik_solve(P.sho[1], P.wri[1], ANIM_UARM, ANIM_FARM, p1);
    } else {
      sw_phase = 0;
      hold = wpn;
    }
  }
  if (hold != wpn) { mag01 = 0.0f; trig01 = 0.0f; }
  {
    probe("gun");
    int gv0 = g_scene_floats;
    gun_build(hold, g_org, g_rt, g_up, g_fw, mag01, trig01, hold == wpn ? gs.bolt : 0.0f);
    // Publish the model's own below-origin depth, once per weapon per session.
    // Gated on the magazine being HOME (a reload has it 60 mm out, which would
    // record the animation instead of the model) and on a figure that is not
    // dissolving (fig_diss_cut drops whole parts, and a missing magazine
    // measures as a shallow rifle).
    if (!sw_phase && g_gun_low[wpn] == 0.0f && mag01 <= 0.001f && g_fig_diss == 0.0f &&
        gs.arc <= 0.001f) {
      // Relative to the POSE ANCHOR `P.gun_o`, not to `g_org`: pose_gun_frame
      // puts GUN_GRIP between the two, and the death pose can only place the
      // anchor. Measured against g_org instead, the placement came out 58 mm
      // short and the magazine stayed in the floor.
      float lo = 0.0f;
      for (int f = gv0; f + VERT_FLOATS <= g_scene_floats; f += VERT_FLOATS) {
        v3 p = {{g_scene[f], g_scene[f + 1], g_scene[f + 2]}};
        float d = v3_dot(v3_sub(p, P.gun_o), g_up);
        if (d < lo) lo = d;
      }
      // Back out of world units into model units. Only this call site fills it
      // and `g_gun_girth` is GUN_GIRTH_3P here, so the round trip against the
      // use site is currently an identity — it is written as a division rather
      // than dropped because the stored number is a MODEL dimension, and a
      // second caller at a different girth would otherwise store a world one.
      if (lo < 0.0f) g_gun_low[wpn] = -lo / g_gun_girth;
    }
  }
  fig_arms(&P, &kt, Rc, spine, g_org, g_rt, g_up, g_fw, hold,
           A->slide_s, A->slide_dir);
  g_fig_free = 0;
  // The long guns he is not holding, on his back. Living figures only (a
  // corpse's drape would have to carry them too), and for a bot only inside
  // 12 m: a slung rifle is ~one more rifle of vertices, and a full brawl's
  // budget is spent on the figures that matter, not on far backs.
  if (carry) fig_stowed(&P, &kt, Rc, spine, hold);
  g_gun_girth = 1.0f;
  probe(NULL);
  ao_off();
  scene_chunk_end();
}

// The renderer's view of a ragdoll, interpolated. ONE of these, because the
// entity and the corpse pool are two views of the same body and five hand-
// written lerps repeated at two call sites is exactly how they drift apart.
static death_t rag_death(const rag_t *r, float prev_slack, float slack,
                         float alpha) {
  return (death_t){prev_slack + (slack - prev_slack) * alpha,
                   r->prev_tilt + (r->tilt - r->prev_tilt) * alpha,
                   r->prev_slam + (r->slam - r->prev_slam) * alpha,
                   r->axis,
                   r->prev_yaw + (r->yaw - r->prev_yaw) * alpha,
                   (r->prev_tilt - r->prev_lag) +
                       ((r->tilt - r->lag) - (r->prev_tilt - r->prev_lag)) * alpha};
}

static void scene_draw_bot(const bot_t *b, float alpha) {
  v3 pos = v3_lerp(b->prev_pos, b->pos, alpha);
  float yaw = b->prev_yaw + (b->yaw - b->prev_yaw) * alpha;
  death_t dth = rag_death(&b->rag, b->prev_topple, b->topple, alpha);
  float flash01 = ent_flash01(b->flash, b->flash_head);
  v3 fc = b->flash_head ? (v3){{1.0f, 0.22f, 0.14f}} : (v3){{1, 1, 1}};
  // b->wp.cur, not a hardwired WPN_AR (A.1): today masked by bot_spawn always
  // issuing the AR — but any figure drawn through this path holding the SR
  // would render an AR, and the far SR report bake becomes reachable with it.
  g_fig_var = (int)(b - g_bots) & 3;              // entity id - 1
  g_fig_team = fig_team_of((int)(b - g_bots) + 1);
  g_fig_ins = fig_ins_of((int)(b - g_bots) + 1);
  g_fig_ent = (int)(b - g_bots) + 1;
  scene_draw_figure(&b->anim, pos, yaw, &dth, alpha, flash01, fc, 0, b->wp.cur,
                    gun_anim(&b->wp, alpha));
  g_fig_ent = -1;
  g_fig_var = 0;
  g_fig_team = -1;
  g_fig_ins = 0;
}

// Perpendicular frame for marks/tracers whose only anchor is one direction.
static void axis_frame(v3 n, v3 *t1, v3 *t2) {
  v3 up = n.y > 0.9f || n.y < -0.9f ? (v3){{1, 0, 0}} : (v3){{0, 1, 0}};
  *t1 = v3_norm(v3_cross(n, up));
  *t2 = v3_cross(n, *t1);
}

// One impact crater: a MARK_K-segment fan of four concentric rings, every vertex
// RADIALLY CLAMPED into the face rectangle the mark was stamped on. Outward:
//   0 .. MARK_R1   flat dark core         — the hole
//   MARK_R2        pale spall / dust ring — fresh fractured material
//   1.0            the host surface       — no silhouette step, no sticker edge
//
// The clip is the whole trick and it is a construction guarantee rather than a
// tuning: every vertex is centre + dir*r with r <= the ray's exit distance from
// the window, the window is convex and CONTAINS the centre, so every point on
// the ray at r <= lim is inside it and a triangle with three vertices inside a
// convex set is inside it. The mark cannot leave the face, at any radius, any
// wobble, any K. It costs nothing: the straight edges come out exact (vertices
// clamped by the same half-plane share a u or a v, so the polyline through them
// IS the edge) and only a window CORNER is chamfered, by 1-cos(pi/K) = 7.6% of
// the local radius, which is inside the rim's own +/-14% wobble.
static void scene_mark(const mark_t *m) {
  v3 t1, t2;
  axis_frame(m->nrm, &t1, &t2);
  const float rf[4] = {0.0f, MARK_R1, MARK_R2, 1.0f};
  // The birth bloom. `bl` is 1 at the stamp tick and 0 once settled; it is a
  // squared ease so most of the collapse happens in the first few frames, which
  // is what a spall cloud does. Everything downstream — the window clamp, the
  // jitter, the winding — is unchanged and still operates on whatever radius
  // comes out of here.
  float age = (float)(g_tick - m->t0) / (float)MARK_BLOOM_T;
  float bl = age >= 1.0f ? 0.0f : (1.0f - age) * (1.0f - age);
  float grow = 1.0f + (MARK_BLOOM_K - 1.0f) * bl;
  float hot  = 1.0f + (MARK_BLOOM_C - 1.0f) * bl;
  v3 col[4];
  col[0] = col[1] = v3_scale(m->surf, MARK_CORE_K);
  col[2] = v3_scale(m->surf, MARK_HALO_K * hot);
  col[3] = m->surf;                       // ring 3 IS the host: no edge at all
  float a0 = (float)(m->seed & 1023u) * (F_TAU / 1024.0f);
  v3 prev[4] = {{{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}}};
  // PARALLAX RIM SHADING. A hole is a crater, not a sticker: its inner wall
  // faces back toward the axis and its spall lip leans outward, so the sun
  // lights one side of the bore and leaves the other in shadow — the cue that
  // reads as DEPTH on a decal that is in fact a few hundred microns thick.
  // The band normals carry that tilt; the centre and the outermost ring stay
  // on the host's normal, so the decal still has no edge.
  v3 pn[4], cn[4];
  for (int j = 0; j < 4; j++) pn[j] = cn[j] = m->nrm;
  int prev_live = 0;
  for (int i = 0; i <= MARK_K; i++) {
    float a = a0 + (float)i * (F_TAU / MARK_K);
    float cu = cosf(a), cv = sinf(a);
    // Distance from the centre to the window along this ray: a min over the two
    // half-planes the ray actually runs toward. The window contains the origin,
    // so this can never come out negative.
    float lim = 1e9f, l;
    if (cu > 1e-6f) lim = m->u1 / cu; else if (cu < -1e-6f) lim = m->u0 / cu;
    if (cv > 1e-6f) { l = m->v1 / cv; if (l < lim) lim = l; }
    else if (cv < -1e-6f) { l = m->v0 / cv; if (l < lim) lim = l; }
    // Wobble on the spall and the rim only — a bullet hole is round and its
    // spall is not. Indexed i % MARK_K so the last segment closes on the first,
    // and hashed rather than drawn from g_rng_vfx so a spray chart reproduces.
    float jt = MARK_JIT_LO + MARK_JIT_SPAN *
               (float)((fx_hash(m->seed ^ (uint32_t)(i % MARK_K) * 0x9E3779B9u)
                        >> 4) & 255) / 255.0f;
    int live = lim > 1e-4f;         // pinched onto the centre: no area
    v3 cur[4];
    for (int j = 0; j < 4; j++) {
      float r = m->rad * grow * rf[j] * (j >= 2 ? jt : 1.0f);
      if (r > lim) r = lim;
      cur[j] = v3_add(m->pos, v3_add(v3_scale(t1, cu * r), v3_scale(t2, cv * r)));
    }
    {
      v3 rd = v3_add(v3_scale(t1, cu), v3_scale(t2, cv));
      cn[0] = m->nrm;
      cn[1] = v3_norm(v3_sub(v3_scale(m->nrm, 0.70f), v3_scale(rd, 0.72f)));   // the bore's wall
      cn[2] = v3_norm(v3_add(m->nrm, v3_scale(rd, 0.30f)));                    // the spall lip
      cn[3] = m->nrm;
    }
    if (i && (live || prev_live)) {
      // Ring 0 has radius 0, so the innermost band is a FAN, not a degenerate
      // strip. Winding: (t1, t2, nrm) is right-handed and the angle runs
      // t1 -> t2, so radial-then-tangential is CCW seen from +nrm — front
      // facing, the same orientation the disc this replaced had.
      // ROUGHNESS RUNS OUT TO THE HOST'S, band by band. It is per-vertex batch
      // state, so this is free, and it is not cosmetic: both roughness-sensitive
      // terms in the world shader are grazing-only (sheen ~ (1-NoV)^4, the env
      // Fresnel ~ (1-NoV)^5), so a decal 0.23 rougher than its wall is invisible
      // head-on and a bright outline at exactly the angle a wall is seen from
      // while running past it — which would put a silhouette back on the one
      // ring whose whole job is not to have one.
      mat_set(MARK_ROUGH, 0.0f);
      scene_push(prev[0], pn[0], col[0]);
      scene_push(prev[1], pn[1], col[1]);
      scene_push(cur[1],  cn[1], col[1]);
      for (int j = 1; j < 3; j++) {
        mat_set(MARK_ROUGH + (m->rough - MARK_ROUGH) * (float)j * 0.5f, 0.0f);
        scene_push(prev[j],     pn[j],     col[j]);
        scene_push(prev[j + 1], pn[j + 1], col[j + 1]);
        scene_push(cur[j + 1],  cn[j + 1], col[j + 1]);
        scene_push(prev[j],     pn[j],     col[j]);
        scene_push(cur[j + 1],  cn[j + 1], col[j + 1]);
        scene_push(cur[j],      cn[j],     col[j]);
      }
    }
    for (int j = 0; j < 4; j++) { prev[j] = cur[j]; pn[j] = cn[j]; }
    prev_live = live;
  }
}

// The additive FX pass runs the world shader with uEmis set, which puts the
// vertex color on screen untouched — so the normal these carry is never read.
static const v3 FX_N = {{0.0f, 1.0f, 0.0f}};

// Camera-facing tapered segment: width and colour interpolate from a to b —
// the building block for streaks and flash petals. fx_ribbon is the constant
// special case, kept for the callers that want a plain bar.
static void fx_seg(v3 a, v3 b, v3 eye, float wa, float wb, v3 ca, v3 cb) {
  v3 seg = v3_add(b, v3_scale(a, -1.0f));
  v3 mid = v3_add(a, v3_scale(seg, 0.5f));
  v3 side = v3_cross(seg, v3_add(eye, v3_scale(mid, -1.0f)));
  float l = sqrtf(v3_dot(side, side));
  if (l < 1e-6f) return;
  side = v3_scale(side, 1.0f / l);
  v3 sa = v3_scale(side, wa), sb = v3_scale(side, wb);
  scene_push(v3_add(a, sa), FX_N, ca);
  scene_push(v3_add(b, sb), FX_N, cb);
  scene_push(v3_sub(b, sb), FX_N, cb);
  scene_push(v3_add(a, sa), FX_N, ca);
  scene_push(v3_sub(b, sb), FX_N, cb);
  scene_push(v3_sub(a, sa), FX_N, ca);
}

// A disc facing the eye: `mid` at the centre, `rim` at the edge. The soft blob both
// blast passes are built from.
static void fx_disc(v3 at, v3 eye, float r, v3 mid, v3 rim, int n) {
  v3 f = v3_norm(v3_sub(eye, at)), up = {{0, 1, 0}};
  v3 x = v3_cross(up, f);
  x = v3_dot(x, x) > 1e-6f ? v3_norm(x) : (v3){{1, 0, 0}};
  v3 y = v3_cross(f, x);
  for (int i = 0; i < n; i++) {
    float a0 = (float)i / (float)n * F_TAU, a1 = (float)(i + 1) / (float)n * F_TAU;
    v3 p0 = v3_add(at, v3_add(v3_scale(x, cosf(a0) * r), v3_scale(y, sinf(a0) * r)));
    v3 p1 = v3_add(at, v3_add(v3_scale(x, cosf(a1) * r), v3_scale(y, sinf(a1) * r)));
    scene_push(at, FX_N, mid);
    scene_push(p0, FX_N, rim);
    scene_push(p1, FX_N, rim);
  }
}

// A soft additive ring: nothing at both edges, `col` on the radius, in the plane u,v.
static void fx_annulus(v3 c, v3 u, v3 v, float r, float w, v3 col, int n) {
  static const v3 K = {{0, 0, 0}};
  for (int i = 0; i < n; i++) {
    float a0 = (float)i / (float)n * F_TAU, a1 = (float)(i + 1) / (float)n * F_TAU;
    v3 d0 = v3_add(v3_scale(u, cosf(a0)), v3_scale(v, sinf(a0)));
    v3 d1 = v3_add(v3_scale(u, cosf(a1)), v3_scale(v, sinf(a1)));
    float rs[3] = {fmaxf(r - w, 0.0f), r, r + w * 0.35f};
    for (int k = 0; k < 2; k++) {
      v3 ci = k == 0 ? K : col, co = k == 0 ? col : K;
      v3 p00 = v3_add(c, v3_scale(d0, rs[k])), p10 = v3_add(c, v3_scale(d1, rs[k]));
      v3 p01 = v3_add(c, v3_scale(d0, rs[k + 1])), p11 = v3_add(c, v3_scale(d1, rs[k + 1]));
      scene_push(p00, FX_N, ci); scene_push(p10, FX_N, ci); scene_push(p11, FX_N, co);
      scene_push(p00, FX_N, ci); scene_push(p11, FX_N, co); scene_push(p01, FX_N, co);
    }
  }
}

// The multiply pass: dust, smoke, clods and the scorch on the ground.
static void puffs_build(v3 eye) {
  scene_reset();
  for (int i = 0; i < MAX_SCORCH; i++)
    if (g_scorch[i].life > 0) {
      const scorch_t *q = &g_scorch[i];
      float k = 0.68f * f_clamp((float)q->life / 900.0f, 0.0f, 1.0f);
      float d = 1.0f - k;
      v3 c = {{q->pos.x, 0.012f, q->pos.z}};
      for (int a = 0; a < 18; a++) {
        // Ragged, so it reads as burnt ground and not as a painted circle.
        float a0 = (float)a / 18.0f * F_TAU, a1 = (float)(a + 1) / 18.0f * F_TAU;
        float r0 = q->r * (0.82f + 0.18f * sinf(a0 * 3.0f + q->r * 9.0f) * cosf(a0 * 7.0f));
        float r1 = q->r * (0.82f + 0.18f * sinf(a1 * 3.0f + q->r * 9.0f) * cosf(a1 * 7.0f));
        scene_push(c, FX_N, (v3){{d, d, d}});
        scene_push(v3_add(c, (v3){{cosf(a1) * r1, 0, sinf(a1) * r1}}), FX_N, (v3){{1, 1, 1}});
        scene_push(v3_add(c, (v3){{cosf(a0) * r0, 0, sinf(a0) * r0}}), FX_N, (v3){{1, 1, 1}});
      }
    }
  for (int i = 0; i < MAX_PUFFS; i++)
    if (g_puffs[i].life > 0) {
      const puff_t *q = &g_puffs[i];
      float f = (float)q->life / (float)q->life0, a1 = 1.0f - f;
      float r = q->r0 + (q->r1 - q->r0) * sqrtf(a1);
      // Thick while young, and it thins as it spreads and ages.
      // Inside the cloud every puff stacks on the lens at once and the frame went
      // near black, so smoke thins out over its last few metres to the eye: you
      // can still see to fight in it.
      float ed = v3_len(v3_sub(q->pos, eye));
      float d = 1.0f - q->dens * f_clamp(f * 2.2f, 0.0f, 1.0f) *
                       (q->r1 < 0.2f ? 1.0f : 0.15f + 0.85f * smooth01(f_clamp((ed - 1.0f) / 5.0f, 0.0f, 1.0f)));
      // Dust is warm and soot is a cold grey; the one mixes into the other.
      v3 mid = {{d, d * (0.97f + 0.03f * q->soot), d * (0.92f + 0.08f * q->soot)}};
      fx_disc(q->pos, eye, r, mid, (v3){{1, 1, 1}}, q->r1 < 0.2f ? 6 : 12);
    }
}

static void fx_ribbon(v3 a, v3 b, v3 eye, float w, v3 col) {
  fx_seg(a, b, eye, w, w, col, col);
}

// The near-camera ease every burst shares (the HITSPARK_NEAR/FADE law):
// returns 0 inside the cull radius — the caller skips the particle — else
// writes the eased 0..1 factor. This lives ONCE for a reason: the one kind
// that shipped without its copy was a measured 94.1%-of-the-crosshair blowout
// (see the contact flare's note); a helper is how the next kind cannot
// forget it.
static int fx_near_ease(v3 pos, v3 eye, float *nf) {
  v3 dq = v3_sub(pos, eye);
  float dq2 = v3_dot(dq, dq);
  if (dq2 < HITSPARK_NEAR * HITSPARK_NEAR) return 0;
  *nf = f_clamp((sqrtf(dq2) - HITSPARK_NEAR) / (HITSPARK_FADE - HITSPARK_NEAR),
                0.0f, 1.0f);
  return 1;
}

// Warm-biased halo round a hot core. The coefficients are arithmetic, not
// taste: uEmis short-circuits the tonemap, so the additive pass blends
// ONE/ONE onto an already display-encoded frame — a halo whose green lands
// above the background's headroom clips the sum to white and takes the hue
// with it. Kinds 0, 4 and 5 share this exact cut (the skid spark runs its own
// hotter 0.34/0.075/0.030 on purpose — an ember at arm's length).
static v3 fx_warm_halo(v3 col) {
  return (v3){{col.x * 0.30f, col.y * 0.060f, col.z * 0.025f}};
}

// TEARDROP, not a bar. fx_seg interpolates colour and width ALONG the segment
// but the cross-section is flat, so one quad from head to tail has a BLUNT
// full-width leading edge — at 3x zoom that edge made every spark read as a
// little orange arrowhead and every halo as a solid pale wedge. Two segments
// meeting a quarter of the way back give a sharp tip, the widest point just
// behind it and a taper to nothing: the silhouette of a spark, and the only
// softening available to a pass with no texture and no bloom. Core (wc/col)
// over halo (wh/halo); was open-coded byte-identically in kinds 0, 2, 4, 5.
static void fx_teardrop(v3 pos, v3 tail, v3 eye, float wc, float wh,
                        v3 col, v3 halo) {
  v3 dv = v3_sub(tail, pos);
  v3 mid = v3_add(pos, v3_scale(dv, 0.25f));
  fx_seg(pos, mid, eye, wh * 0.14f, wh, v3_scale(halo, 0.55f), halo);
  fx_seg(mid, tail, eye, wh, wh * 0.02f, halo, (v3){{0.0f, 0.0f, 0.0f}});
  fx_seg(pos, mid, eye, wc * 0.30f, wc, col, col);
  fx_seg(mid, tail, eye, wc, wc * 0.30f, col, v3_scale(col, 0.20f));
}

// Deterministic per-shot variety with no RNG stream: flash petals rotate and
// jitter off a hash of the shot index, so a burst flickers frame to frame and
// two runs of one script stay byte-identical.
static uint32_t fx_hash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du;
  x ^= x >> 15; x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

// One muzzle flash, designed per MUZZLE DEVICE, because that is what shapes
// the gas leaving a barrel and the three guns carry three different ones:
//   AR — a birdcage flash hider. It exists to kill the side flash, so what is
//        left is a narrow forward cone of three tongues out of the slots and
//        a small, tight star round the crown.
//   SR — a ported brake. It throws the blast SIDEWAYS through its ports to
//        tame the recoil, so the magnum's signature is two big lateral jets
//        (and a pair of smaller second-port jets behind them) with only a
//        modest forward plume. It is the biggest flash in the game.
//   PT — a bare muzzle. A short barrel burns powder outside it: a round,
//        warm fireball with irregular petals and a short tongue.
// t1/t2 span the plane across the bore (the star's petals), s scales it in
// metres and f is the 1->0 life fade. The SIDE axis for the brake is world
// right across the bore, not t1: the ports are on the flanks of a level gun.
// Shared by the world pass (every visible shooter) and the viewmodel pass
// (the player's own barrel), so a gun flashes the same in both.
static void fx_flash(v3 pos, v3 fw, v3 t1, v3 t2, v3 eye, float s, float f,
                     uint32_t seed, int wpn) {
  uint32_t h = fx_hash(seed);
  float a0 = (float)(h & 255) * (F_TAU / 256.0f);
  #define FH01(k) ((float)((fx_hash(seed * 5u + (uint32_t)(k)) >> 4) & 255) / 255.0f)
  v3 hot = v3_scale((v3){{2.10f, 1.55f, 0.85f}}, f);
  v3 mid = v3_scale((v3){{1.10f, 0.46f, 0.12f}}, f);
  v3 tip = v3_scale((v3){{0.45f, 0.13f, 0.03f}}, f);
  v3 K = {{0, 0, 0}};
  if (wpn == WPN_SR) {
    s *= 1.45f;
    v3 side = v3_cross(fw, (v3){{0, 1, 0}});
    float sl = v3_len(side);
    side = sl > 1e-3f ? v3_scale(side, 1.0f / sl) : t1;
    v3 upx = v3_cross(side, fw);
    for (int sd = -1; sd <= 1; sd += 2)
      for (int port = 0; port < 2; port++) {
        // Port 0 is the big front one, angled a little forward; port 1 sits
        // 4 cm back on the brake and throws a smaller jet straight out.
        float ln = s * (port ? 0.42f : 0.78f) * (0.80f + 0.35f * FH01(sd + 3 * port + 4));
        v3 o = v3_add(pos, v3_scale(fw, port ? -0.045f * s / 0.30f : 0.0f));
        v3 jd = v3_norm(v3_add(v3_scale(side, (float)sd),
                        v3_add(v3_scale(fw, port ? 0.10f : 0.32f),
                               v3_scale(upx, 0.10f * (FH01(sd + 9) - 0.5f)))));
        fx_seg(o, v3_add(o, v3_scale(jd, ln)), eye, s * (port ? 0.09f : 0.15f),
               s * 0.05f, hot, tip);
        // The jet's cooler body, wider and longer than its core.
        fx_seg(o, v3_add(o, v3_scale(jd, ln * 1.25f)), eye, s * 0.10f,
               s * (port ? 0.12f : 0.22f), v3_scale(mid, 0.55f), K);
      }
    fx_seg(pos, v3_add(pos, v3_scale(fw, s * 0.85f)), eye, s * 0.16f, s * 0.03f, hot, mid);
    fx_seg(v3_add(pos, v3_scale(t1, -0.05f * s)), v3_add(pos, v3_scale(t1, 0.05f * s)),
           eye, s * 0.11f, s * 0.11f, v3_scale(hot, 1.10f), v3_scale(hot, 1.10f));
    // The glow the blast throws into the air round it.
    fx_disc(pos, eye, s * 1.05f, v3_scale(mid, 0.42f), K, 14);
  } else if (wpn == WPN_PT) {
    v3 warm = v3_scale((v3){{2.05f, 1.20f, 0.48f}}, f);
    for (int k = 0; k < 8; k++) {
      float ang = a0 + (float)k * (F_TAU / 8.0f) + 0.3f * (FH01(k + 20) - 0.5f);
      float lk = s * (0.22f + 0.30f * FH01(k));
      v3 rd = v3_add(v3_scale(t1, cosf(ang)), v3_scale(t2, sinf(ang)));
      rd = v3_norm(v3_add(rd, v3_scale(fw, 0.35f)));
      fx_seg(v3_add(pos, v3_scale(rd, 0.03f * s)), v3_add(pos, v3_scale(rd, lk)),
             eye, s * 0.16f, s * 0.03f, warm, tip);
    }
    fx_seg(pos, v3_add(pos, v3_scale(fw, s * 0.70f)), eye, s * 0.22f, s * 0.05f, warm, mid);
    fx_seg(v3_add(pos, v3_scale(t1, -0.10f * s)), v3_add(pos, v3_scale(t1, 0.10f * s)),
           eye, s * 0.22f, s * 0.22f, v3_scale(warm, 1.30f), v3_scale(warm, 1.30f));
    fx_disc(v3_add(pos, v3_scale(fw, s * 0.12f)), eye, s * 0.75f, v3_scale(mid, 0.40f), K, 12);
  } else {
    // Birdcage: three tongues out of the slots, fanned a few degrees off the
    // bore, rotating shot to shot with the petal clock.
    for (int k = 0; k < 3; k++) {
      float ang = a0 + (float)k * (F_TAU / 3.0f);
      v3 rd = v3_add(v3_scale(t1, cosf(ang)), v3_scale(t2, sinf(ang)));
      v3 td = v3_norm(v3_add(fw, v3_scale(rd, 0.22f + 0.10f * FH01(k + 30))));
      float ln = s * (0.95f + 0.55f * FH01(k + 40));
      fx_seg(pos, v3_add(pos, v3_scale(td, ln)), eye, s * 0.10f, s * 0.012f, hot, tip);
    }
    for (int k = 0; k < 4; k++) {   // what escapes the cage: a short, tight star
      float ang = a0 + 0.785f + (float)k * (F_TAU / 4.0f);
      float lk = s * (0.30f + 0.32f * FH01(k));
      v3 rd = v3_add(v3_scale(t1, cosf(ang)), v3_scale(t2, sinf(ang)));
      fx_seg(v3_add(pos, v3_scale(rd, 0.03f * s)), v3_add(pos, v3_scale(rd, lk)),
             eye, s * 0.09f, s * 0.010f, hot, tip);
    }
    fx_seg(pos, v3_add(pos, v3_scale(fw, s * 1.30f)), eye, s * 0.14f, s * 0.02f, hot, mid);
    fx_seg(v3_add(pos, v3_scale(t1, -0.07f * s)), v3_add(pos, v3_scale(t1, 0.07f * s)),
           eye, s * 0.14f, s * 0.14f, v3_scale(hot, 1.35f), v3_scale(hot, 1.35f));
    fx_disc(v3_add(pos, v3_scale(fw, s * 0.25f)), eye, s * 0.70f, v3_scale(mid, 0.30f), K, 12);
  }
  // THE VOLUME: a stack of soft glow slices down the bore, each smaller and
  // cooler than the last. One disc is a sprite; five overlapping ones are a
  // body of hot gas with depth, and from the side they read as a cone.
  for (int k = 0; k < 5; k++) {
    float t = (float)k / 4.0f;
    fx_disc(v3_add(pos, v3_scale(fw, s * (0.10f + 0.55f * t))), eye,
            s * (0.42f - 0.24f * t) * (0.85f + 0.3f * FH01(k + 60)),
            v3_scale(v3_add(v3_scale(hot, 1.0f - t), v3_scale(mid, t)), 0.10f), K, 10);
  }
  #undef FH01
}


