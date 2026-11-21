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
static gframe_t holster_frame_leg(const pose_t *P, v3 spine, int leg) {
  v3 prt = v3_norm(v3_sub(P->hip[1], P->hip[0]));
  v3 td = v3_sub(P->knee[leg], P->hip[leg]);
  td = v3_dot(td, td) > 1e-6f ? v3_norm(td) : v3_scale(spine, -1.0f);
  v3 pfw = v3_norm(v3_cross(spine, prt));
  v3 out = v3_add(v3_scale(prt, leg ? 0.97f : -0.97f), v3_scale(pfw, 0.24f));
  out = v3_sub(out, v3_scale(td, v3_dot(out, td)));
  out = v3_dot(out, out) > 1e-6f ? v3_norm(out) : prt;
  float r = 0.5f * (fig_rmax(8, 0.0900f, 0.0900f) + fig_rmax(8, 0.0855f, 0.0855f));
  v3 fwd = v3_cross(out, td);                 // along the leg's front, either way
  if (v3_dot(fwd, pfw) < 0.0f) fwd = v3_scale(fwd, -1.0f);
  v3 mn, mx;
  gun_box(WPN_PT, &mn, &mx);
  // HOW A DROP-LEG HOLSTER CARRIES A PISTOL: muzzle down, the slide toward
  // the BACK of the leg and the trigger guard toward the front, so the grip
  // rises out of the mouth pointing FORWARD and up, under the hand that hangs
  // beside it — with a forward cant of ~12 degrees (the muzzle swung back), so
  // the grip leans toward the hand. It used to hang the other way round: slide
  // forward, so the grip stood straight out behind the thigh like the handle
  // of a hammer in a tool belt.
  float cant = 0.21f;
  v3 mz = v3_norm(v3_sub(v3_scale(td, cosf(cant)), v3_scale(fwd, sinf(cant))));
  float half_h = mx.x > mn.x ? 0.5f * (mx.y - mn.y) : 0.074f;
  // the slide's top 45 mm behind the leg's mid-line, the frame forward of it
  float cu = half_h - 0.045f;
  // r + 0.030: the pistol rides OUTSIDE the leg, on the platform the rig
  // straps to the thigh — closer and half its frame was inside the trouser.
  v3 c = v3_add(P->hip[leg], v3_add(v3_scale(out, r + 0.030f),
                v3_add(v3_scale(td, 0.100f), v3_scale(fwd, cu))));
  return gframe_at(WPN_PT, c, mz, v3_scale(fwd, -1.0f));
}
// The pistol a holster has to close over, measured in its own frame: the
// third-person Blender model when the figure carries a variant that has one
// (that is what is drawn), else the built pistol. Same trick as gun_box: emit
// into the scene and rewind.
#define HOL_NS 5
static void pt_holster_box(int ent, v3 *mn, v3 *mx, float *yfront, float ylo[HOL_NS], float zst[HOL_NS]) {
  // ...and *yfront: the lowest point of the pistol AHEAD of the grip (under
  // the barrel: a light, a foregrip, a long magazine well), which is how deep
  // the holster has to be all the way to the muzzle.
  const mdl_ref_t *tm = gun_tp_model(ent, WPN_PT);
  int f0 = g_scene_floats, want0 = g_scene_want;
  float gg = g_gun_girth;
  g_gun_girth = GUN_GIRTH_3P;
  if (tm) gun_tp_draw(tm, WPN_PT, (v3){{0, 0, 0}}, (v3){{1, 0, 0}}, (v3){{0, 1, 0}}, (v3){{0, 0, 1}}, 0.0f);
  else gun_build(WPN_PT, (v3){{0, 0, 0}}, (v3){{1, 0, 0}}, (v3){{0, 1, 0}}, (v3){{0, 0, 1}}, 0.0f, 0.0f, 0.0f);
  g_gun_girth = gg;
  v3 lo = {{1e9f, 1e9f, 1e9f}}, hi = {{-1e9f, -1e9f, -1e9f}};
  for (int f = f0; f + VERT_FLOATS <= g_scene_floats; f += VERT_FLOATS)
    for (int k = 0; k < 3; k++) {
      float v = g_scene[f + k];
      if (v < lo.e[k]) lo.e[k] = v;
      if (v > hi.e[k]) hi.e[k] = v;
    }
  float yf = 1e9f;
  for (int f = f0; f + VERT_FLOATS <= g_scene_floats; f += VERT_FLOATS)
    if (g_scene[f + 2] > lo.z + 0.075f && g_scene[f + 1] < yf) yf = g_scene[f + 1];
  // THE DEPTH PROFILE: how far below the slide the pistol reaches at HOL_NS
  // stations from just ahead of the grip to the muzzle — deep over the
  // trigger guard, shallow over the slide and dust cover — so the shell can
  // follow it and step in, as a molded holster does, instead of being a box.
  float z0 = lo.z + 0.080f, z1 = hi.z;   // clear of the grip's front strap
  for (int k = 0; k < HOL_NS; k++) {
    zst[k] = z0 + (z1 - z0) * (float)k / (float)(HOL_NS - 1);
    ylo[k] = 1e9f;
  }
  for (int f = f0; f + VERT_FLOATS <= g_scene_floats; f += VERT_FLOATS) {
    float z = g_scene[f + 2], y = g_scene[f + 1];
    if (z < z0 - 0.012f) continue;                 // the grip: it rides out of the mouth
    for (int k = 0; k < HOL_NS; k++)
      if (fabsf(z - zst[k]) < 0.5f * (z1 - z0) / (float)(HOL_NS - 1) + 0.010f && y < ylo[k]) ylo[k] = y;
  }
  g_scene_floats = f0;
  g_scene_want = want0;
  if (hi.x < lo.x) {   // scene full: the standard pistol's numbers
    lo = (v3){{-0.020f, -0.112f, -0.054f}}; hi = (v3){{0.020f, 0.036f, 0.153f}}; yf = -0.053f;
  }
  for (int k = 0; k < HOL_NS; k++) {
    if (zst[k] != zst[k] || ylo[k] > 1e8f) ylo[k] = hi.y - 0.050f;
    if (hi.x < lo.x) zst[k] = lo.z + 0.080f + (hi.z - lo.z - 0.080f) * (float)k / (float)(HOL_NS - 1);
  }
  *mn = lo; *mx = hi; *yfront = yf < 1e8f ? yf : hi.y - 0.090f;
}
// The strong-side holster (the draw and holster swap arcs use this one).
static gframe_t holster_frame(const pose_t *P, v3 spine) { return holster_frame_leg(P, spine, 1); }
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
    g_gun_ak = kt->ins;
    {
      int m0 = g_scene_floats;
      gun_build(w, f.o, f.rt, f.up, f.fw, 0.0f, 0.0f, 0.0f);
      const mdl_ref_t *tm = gun_tp_model(ent, w);
      if (tm) { g_scene_floats = m0; gun_tp_draw(tm, w, f.o, f.rt, f.up, f.fw, 0.0f); }
    }
    g_gun_ak = 0;
    g_gun_smoke = 0;
    g_gun_girth = gg;
    v3 mn, mx;
    gun_box(w, &mn, &mx);
    if (mx.x < mn.x) continue;
    float half_rt = 0.5f * (mx.x - mn.x);
    v3 cen = gframe_centre(w, &f);
    float len = mx.z - mn.z;
    mat_set(MAT_GEAR_R, 0.0f);
    // v5: a MUTED two-point sling — a value step under the webbing, 26 mm
    // wide (was 36), so it is a strap lying on the kit, not a light stripe.
    v3 sc = v3_scale(kt->web, 0.82f);
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
        {top, sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        {SP(-0.090f, ptop + 0.012f, -(back - 0.012f)), sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        {SP(-0.112f, 0.122f, -0.090f), sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        {SP(-0.112f, 0.140f,  0.020f), sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        // v5: the slack across the chest and round the hip lags with the same
        // follower as the antenna, a little at the shoulder, most at the hip.
        {v3_add(SP(-0.098f, 0.070f, front), v3_scale(P->ant_sway, 0.04f)),   sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        {v3_add(SP(0.0f, -0.120f, front + 0.004f), v3_scale(P->ant_sway, 0.10f)), sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        {v3_add(SP(0.150f, -0.330f, front - 0.030f), v3_scale(P->ant_sway, 0.14f)), sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        {bot, sc, sc, 0.0040f, 0.013f, 0, 0.0f}};
      probe("stow_strap");
      fig_chain(st, 8, 8, 1, 1, cfw);
    } else {
      // The sniper rides on a short loop off the pack's left shoulder strap.
      v3 top = v3_add(cen, v3_scale(f.fw, -0.30f * len));
      fnode_t st[3] = {
        {top, sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        {SP(-0.150f, 0.050f, -0.110f), sc, sc, 0.0040f, 0.013f, 0, 0.0f},
        {SP(-0.118f, 0.110f, -0.050f), sc, sc, 0.0040f, 0.013f, 0, 0.0f}};
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
// the slot, closes, and brings it out to the ready). A weapon with no slot
// (knife, grenade) skips its phase. A shot does not cut it (that was a pop):
// it runs the rest SWAP_RUSH times faster, so the gun is up in a few frames.
//
// v2 — what made the old one look wrong, and what replaced it:
//  * the gun's axes were blended LINEARLY between the slot and the ready and
//    re-squared, and its centre was forced THROUGH a point 17 cm above the
//    shoulder: a rifle that hangs muzzle-down on the back went up over the
//    head, stood on its stock and cartwheeled down into the hands — the
//    helicopter blade. Now the orientation is a true shortest-arc rotation
//    (axes_slerp), through an explicit middle key where the two ends are half
//    a turn apart (pistol, sniper) so it can never pick the long way round or
//    flip between frames, and the centre runs a spline through keys that
//    keep it at chest height beside the body: pulled UP out of the slot first
//    while still muzzle-down, then swung forward round the strong side as the
//    muzzle comes up.
//  * the rotation LAGS the lift, so a gun clears its slot before it turns.
//  * the hand no longer parks in front of the belly between the two phases:
//    after letting go of the old gun it travels straight on toward the new
//    one's slot, on a slight outward arc, and the elbow lifts when the hand
//    goes up behind the shoulder instead of folding down through the chest.
//  * 0.32 + 0.42 s was a long gun stowed and drawn in about the time of a
//    blink; the long guns now take 0.50 + 0.66 s, the pistol 0.40 + 0.46 s.
// v3 — natural, unhurried weapon grabbing & switching:
//  * Deliberate pacing (0.62/0.78 s long guns, 0.48/0.56 s pistol) with quintic
//    smootherstep (`ease5`) so every motion starts and settles at zero jerk, plus
//    a tactile grasp/seat beat at the slot before pulling out or letting go.
//  * Continuous C1 upper-body, shoulder and head accompaniment (`swap_body_pose`)
//    so the torso turns and the active shoulder eases into the reach instead of
//    reaching off a frozen statue.
//  * Anatomical elbow poles (`swap_pole`) that NEVER shoot above the helmet on a
//    back reach or wing out horizontally into a T-pose; reach clamped to <= 90%
//    of arm length mid-swap so neither elbow ever locks straight.
//  * Wrist-aligned stock/grip handles (`swap_handle`) with consistent thumb axis
//    (zero sign-flip mid-swing) and continuous hand-cylinder blending from the
//    relaxed forearm line onto the weapon (zero 0.5 binary hand-pose pop).
#define SWAP_A_LONG 0.62f   // seconds: stow a long gun
#define SWAP_B_LONG 0.78f   // seconds: draw a long gun
#define SWAP_A_PT   0.48f   // seconds: holster the pistol
#define SWAP_B_PT   0.56f   // seconds: draw the pistol
#define SWAP_RUSH   5.5f    // a shot mid-swap runs the rest this much faster
static int   g_sw_seen[MAX_ENTS];   // last weapon seen + 1, 0 = never
static int   g_sw_from[MAX_ENTS];
static float g_sw_t0[MAX_ENTS];
static float g_sw_rush[MAX_ENTS];   // when a shot rushed this swap (< t0: not)
static float ease3(float x) { x = f_clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
static float ease5(float x) {
  x = f_clamp(x, 0.0f, 1.0f);
  return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}
static float swap_bell(float t, float t0, float pk, float t1) {
  if (t <= t0 || t >= t1) return 0.0f;
  return t < pk ? ease5((t - t0) / fmaxf(pk - t0, 1e-4f))
                : 1.0f - ease5((t - pk) / fmaxf(t1 - pk, 1e-4f));
}
static void swap_slot_body(int w, float *tw, float *sh0_y, float *sh0_f,
                           float *sh1_y, float *sh1_f, float *hpitch) {
  *tw = *sh0_y = *sh0_f = *sh1_y = *sh1_f = *hpitch = 0.0f;
  if (w == WPN_AR) {
    // Over-the-right-shoulder rifle reach: turn torso cleanly right and
    // elevate/protract the right shoulder into the overhead arch.
    *tw = 0.26f; *sh1_y = 0.036f; *sh1_f = 0.016f; *hpitch = -0.04f;
  } else if (w == WPN_SR) {
    // Mirror over the left shoulder for the sniper/DMR.
    *tw = -0.26f; *sh0_y = 0.036f; *sh0_f = 0.016f; *hpitch = -0.04f;
  } else if (w == WPN_PT) {
    // Reaching down for the drop-leg holster: ease the right shoulder down and
    // glance slightly toward the hip.
    *tw = 0.08f; *sh1_y = -0.022f; *sh1_f = 0.012f; *sh0_y = 0.006f; *hpitch = -0.07f;
  }
}
static float swap_stow_s(int w) {
  return w == WPN_PT ? SWAP_A_PT : (w == WPN_AR || w == WPN_SR) ? SWAP_A_LONG : 0.0f;
}
static float swap_draw_s(int w) {
  return w == WPN_PT ? SWAP_B_PT : (w == WPN_AR || w == WPN_SR) ? SWAP_B_LONG : 0.0f;
}
// Turn the axes (art, aup, afw) toward (brt, bup, bfw) by the fraction k of
// the single shortest rotation between them — what a wrist does. Blending the
// axes linearly instead swings through whatever the average happens to be,
// and near half a turn that average is noise.
static void axes_slerp(v3 art, v3 aup, v3 afw, v3 brt, v3 bup, v3 bfw, float k,
                       v3 *rt, v3 *up, v3 *fw) {
  if (k <= 0.0f) { *rt = art; *up = aup; *fw = afw; return; }
  if (k >= 1.0f) { *rt = brt; *up = bup; *fw = bfw; return; }
  float m[3][3];
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++)
      m[r][c] = brt.e[r] * art.e[c] + bup.e[r] * aup.e[c] + bfw.e[r] * afw.e[c];
  float ang = acosf(f_clamp(0.5f * (m[0][0] + m[1][1] + m[2][2] - 1.0f), -1.0f, 1.0f));
  if (ang < 1e-4f) { *rt = brt; *up = bup; *fw = bfw; return; }
  v3 ax = {{m[2][1] - m[1][2], m[0][2] - m[2][0], m[1][0] - m[0][1]}};
  if (v3_dot(ax, ax) < 1e-8f) {   // a half turn: the axis is the symmetric part
    int j = 0;
    for (int q = 1; q < 3; q++) if (m[q][q] > m[j][j]) j = q;
    float aj = sqrtf(fmaxf(0.5f * (m[j][j] + 1.0f), 1e-8f));
    for (int q = 0; q < 3; q++) ax.e[q] = q == j ? aj : 0.25f * (m[q][j] + m[j][q]) / aj;
  }
  ax = v3_norm(ax);
  v3 f = v3_rot_axis(afw, ax, ang * k), u = v3_rot_axis(aup, ax, ang * k);
  *fw = v3_norm(f);
  *up = v3_norm(v3_sub(u, v3_scale(*fw, v3_dot(u, *fw))));
  *rt = v3_norm(v3_cross(*fw, *up));
}
// Catmull-Rom through four points at u = 0, 1/3, 2/3, 1 (ends clamped).
static v3 spline4(const v3 c[4], float u) {
  float x = f_clamp(u, 0.0f, 1.0f) * 3.0f;
  int i = x >= 3.0f ? 2 : (int)x;
  float t = x - (float)i;
  v3 p0 = c[i > 0 ? i - 1 : 0], p1 = c[i], p2 = c[i + 1], p3 = c[i + 2 < 4 ? i + 2 : 3];
  float t2 = t * t, t3 = t2 * t;
  v3 r = v3_scale(p1, 2.0f);
  r = v3_add(r, v3_scale(v3_sub(p2, p0), t));
  r = v3_add(r, v3_scale(v3_add(v3_sub(v3_scale(p0, 2.0f), v3_scale(p1, 5.0f)),
                                v3_sub(v3_scale(p2, 4.0f), p3)), t2));
  r = v3_add(r, v3_scale(v3_add(v3_sub(v3_scale(p1, 3.0f), p0),
                                v3_sub(p3, v3_scale(p2, 3.0f))), t3));
  return v3_scale(r, 0.5f);
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

// Where weapon `w` lives off the hands, for the swap: its back/flank slot or
// the strong-side holster. 0 for a weapon with no slot.
static int swap_slot(const pose_t *P, v3 Rc, v3 spine, int w, gframe_t *S) {
  if (w == WPN_PT) { *S = holster_frame(P, spine); return 1; }
  return stow_frame(P, Rc, spine, w, S);
}
// The weapon `w` on its way between its slot S (p = 0) and the ready H
// (p = 1). The centre follows a spline through two body-relative keys: LIFT
// (pulled straight up out of the slot, still in its slot attitude) and SWING
// (beside the strong side at chest height for the carbine, in front of the
// chest for the sniper coming off the left flank, in front of the hip for the
// pistol). The attitude turns on the shortest arc, starting only once the gun
// is clear, through a MID attitude — muzzle down and forward, rolled on its
// side — wherever the slot and the ready are close to half a turn apart.
static gframe_t swap_frame(const pose_t *P, v3 Rc, v3 spine, int w,
                           const gframe_t *S, const gframe_t *H, float p) {
  v3 cfw = v3_norm(v3_cross(spine, Rc));
  v3 c[4];
  c[0] = gframe_centre(w, S);
  c[3] = gframe_centre(w, H);
  float lag;
  if (w == WPN_AR) {
    // Lift the carbine cleanly UP and OVER the right shoulder (diagonal
    // over-the-shoulder draw arc), sweeping forward clear of the deltoid into
    // the waiting support hand in front of the chest.
    c[1] = v3_add(P->chest, v3_add(v3_scale(Rc, 0.22f),
                  v3_add(v3_scale(spine, 0.16f), v3_scale(cfw, 0.08f))));
    c[2] = v3_add(P->chest, v3_add(v3_scale(Rc, 0.22f),
                  v3_add(v3_scale(spine, -0.02f), v3_scale(cfw, 0.44f))));
    lag = 0.10f;
  } else if (w == WPN_SR) {
    // Mirror over the left shoulder: lift up and over the left shoulder,
    // sweeping cleanly forward into ready.
    c[1] = v3_add(P->chest, v3_add(v3_scale(Rc, -0.22f),
                  v3_add(v3_scale(spine, 0.12f), v3_scale(cfw, 0.12f))));
    c[2] = v3_add(P->chest, v3_add(v3_scale(Rc, -0.18f),
                  v3_add(v3_scale(spine, -0.04f), v3_scale(cfw, 0.44f))));
    lag = 0.10f;
  } else {
    // Clean vertical clear out of the drop-leg holster, then punch smoothly up
    // and forward in front of the sternum into two-handed presentation.
    c[1] = v3_add(c[0], v3_add(v3_scale(spine, 0.12f),
                  v3_add(v3_scale(Rc, 0.02f), v3_scale(cfw, 0.05f))));
    c[2] = v3_add(P->chest, v3_add(v3_scale(Rc, 0.13f),
                  v3_add(v3_scale(spine, -0.22f), v3_scale(cfw, 0.26f))));
    lag = 0.20f;
  }
  float ro = ease5((p - lag) / (0.88f - lag));
  v3 rt, up, fw;
  if (w == WPN_PT) {
    // The drop-leg holster carries the slide facing back and muzzle down, so
    // holster -> ready is a clean ~90-degree pitch up: shortest-arc slerp keeps
    // the slide upright with zero sideways roll or wrist twist.
    axes_slerp(S->rt, S->up, S->fw, H->rt, H->up, H->fw, ro, &rt, &up, &fw);
  } else {
    // Over-the-shoulder diagonal middle attitude: barrel swings diagonally
    // across/forward over the shoulder while sights roll smoothly upright.
    float side = w == WPN_SR ? -1.0f : 1.0f;
    v3 mfw = v3_norm(v3_add(v3_scale(Rc, -side * 0.24f),
                     v3_add(v3_scale(spine, -0.30f), v3_scale(cfw, 0.92f))));
    v3 mup = v3_norm(v3_add(v3_scale(spine, 0.88f),
                     v3_add(v3_scale(cfw, 0.35f), v3_scale(Rc, side * 0.32f))));
    mup = v3_norm(v3_sub(mup, v3_scale(mfw, v3_dot(mup, mfw))));
    v3 mrt = v3_norm(v3_cross(mfw, mup));
    if (ro < 0.5f)
      axes_slerp(S->rt, S->up, S->fw, mrt, mup, mfw, ro * 2.0f, &rt, &up, &fw);
    else
      axes_slerp(mrt, mup, mfw, H->rt, H->up, H->fw, ro * 2.0f - 1.0f, &rt, &up, &fw);
  }
  if (p <= 0.0f) return *S;
  if (p >= 1.0f) return *H;
  return gframe_at(w, spline4(c, p), fw, up);
}
// Anatomical elbow pole for weapon swaps:
//  * Low / chest / holster: elbow stays compact and tucked down-out-back.
//  * Over-the-shoulder reach (wrist rising above shoulder / behind head):
//    elbow arcs cleanly OUTBOARD-AND-FORWARD at ear/crown height so the upper
//    arm opens ~50 deg up-out and the forearm runs nearly horizontal across to
//    the weapon behind the shoulder.
static v3 swap_pole(v3 sho, v3 wr, v3 Rc, v3 spine, float side) {
  v3 cfw = v3_norm(v3_cross(spine, Rc));
  v3 d = v3_sub(wr, sho);
  float dy = v3_dot(d, spine);
  float df = v3_dot(d, cfw);
  float rear = ease3((-df + 0.14f) / 0.30f);
  float hi   = ease3((dy + 0.04f + 0.10f * rear) / 0.24f);
  v3 base = v3_add(v3_scale(Rc, side * 0.40f),
            v3_add(v3_scale(spine, -0.82f), v3_scale(cfw, -0.40f)));
  // While the hand is lifting up in front of the shoulder, keep the elbow
  // tucked down-and-forward (bicep-curl lift — never flaring sideways into a
  // horizontal T-pose); open into the wide outboard-forward arch as the hand
  // reaches back over the shoulder.
  v3 mid_p = v3_add(v3_scale(Rc, side * 0.46f),
             v3_add(v3_scale(spine, -0.72f), v3_scale(cfw, 0.32f)));
  v3 ov_p = v3_add(v3_scale(Rc, side * 0.78f),
            v3_add(v3_scale(spine, -0.06f), v3_scale(cfw, 0.62f)));
  v3 p = v3_lerp(base, mid_p, hi);
  p = v3_lerp(p, ov_p, hi * rear);
  return v3_norm(p);
}
// Compute a forearm-aligned wrist on grip cylinder (hc, ha, hr) using a damped
// 4-pass fixed-point IK solve so the wrist and palm ALWAYS run cleanly out of
// the forearm (zero kink, zero axial twist, zero reach-clamp stretch) across
// the entire draw/stow arc.
static v3 swap_align_wrist(v3 sho, v3 hc, v3 ha, float hr, float side,
                           v3 Rc, v3 spine) {
  v3 out_h = v3_scale(Rc, side);
  v3 init = v3_add(v3_sub(sho, hc), v3_scale(out_h, 0.25f));
  v3 br = v3_sub(init, v3_scale(ha, v3_dot(init, ha)));
  br = v3_dot(br, br) > 1e-8f ? v3_norm(br) : out_h;
  v3 wri = v3_add(hc, v3_add(v3_scale(ha, -0.020f), v3_scale(br, hr + 0.064f)));
  float amax = (ANIM_UARM + ANIM_FARM) * 0.93f;
  for (int it = 0; it < 4; it++) {
    v3 pole = swap_pole(sho, wri, Rc, spine, side);
    v3 elb = ik_solve(sho, wri, ANIM_UARM, ANIM_FARM, pole);
    v3 to_e = v3_sub(elb, hc);
    float tl = v3_len(to_e);
    v3 ten = tl > 1e-6f ? v3_scale(to_e, 1.0f / tl) : out_h;
    br = v3_sub(ten, v3_scale(ha, v3_dot(ten, ha)));
    br = v3_dot(br, br) > 1e-8f ? v3_norm(br) : out_h;
    v3 e2 = v3_cross(ha, br);
    br = v3_norm(v3_add(br, v3_scale(e2, side * 0.32f)));
    float aw = f_clamp(v3_dot(ten, ha) * 0.072f, -0.062f, 0.062f);
    v3 wn = v3_add(hc, v3_add(v3_scale(ha, aw), v3_scale(br, hr + 0.064f)));
    wri = v3_lerp(wri, wn, 0.65f);
    v3 awv = v3_sub(wri, sho);
    float awl = v3_len(awv);
    if (awl > amax) wri = v3_add(sho, v3_scale(awv, amax / awl));
  }
  return wri;
}
// Where a hand holds weapon `w` in frame F during the swap, p as in swap_frame.
// Long guns are grasped OVER THE SHOULDER by the upper buttstock with the back
// of the hand facing rear/up and fingers curling over the top/side of the stock
// (thumb down along the gun toward the barrel — zero wrist kink or twist), then
// slide smoothly onto the grip/fore-end as the weapon swings over the shoulder.
static v3 swap_handle(int w, const gframe_t *F, float p, int hand, v3 sho,
                      v3 Rc, v3 spine, v3 *hc, v3 *ha, float *hr) {
  float side = hand ? 1.0f : -1.0f;
  v3 gc, ga; float gr;
  gun_hold(w, hand ? GH_GRIP : GH_FORE3, F->o, F->rt, F->up, F->fw, &gc, &ga, &gr);
  v3 gw = gframe_wrist(w, F, hand);
  if (w == WPN_PT) {
    *hc = gc; *ha = ga; *hr = gr;
    v3 aw = swap_align_wrist(sho, gc, ga, gr, side, Rc, spine);
    return v3_lerp(aw, gw, ease5((p - 0.55f) / 0.40f));
  }
  v3 mn, mx;
  gun_box(w, &mn, &mx);
  // Upper buttstock standing behind the shoulder/ear (right at head height for
  // the clean horizontal-forearm over-the-shoulder grasp).
  float zf = w == WPN_AR ? 0.92f : 0.94f;
  v3 sc = v3_add(F->o, v3_add(v3_scale(F->fw, mn.z * zf),
                 v3_scale(F->up, 0.012f)));
  // sa = +F->fw (thumb down toward barrel): puts the back of the hand facing
  // cleanly back/up toward the camera with fingers wrapping over the weapon,
  // and stays sign-consistent with both AR GH_GRIP and SR GH_FORE3.
  v3 sa = F->fw;
  const float sr = 0.018f;
  float s = ease5((p - 0.24f) / 0.48f);
  *hc = v3_lerp(sc, gc, s);
  v3 ha_mix = v3_lerp(sa, ga, s);
  *ha = v3_dot(ha_mix, ha_mix) > 1e-8f ? v3_norm(ha_mix) : ga;
  *hr = sr + (gr - sr) * s;
  v3 aw = swap_align_wrist(sho, *hc, *ha, *hr, side, Rc, spine);
  return v3_lerp(aw, gw, ease5((p - 0.62f) / 0.34f));
}

// A point on the procedural skull's surface (the seven rings scene_draw_figure
// builds the head from), `x` across and `y` up the head frame, standing `lift`
// out along the surface normal. Facial hair is laid with it, so a beard follows
// the jaw instead of floating as a ring in front of it.
static v3 head_surf(v3 hp, v3 hr, v3 hu, v3 hf, float x, float y, float lift) {
  static const float ry_[7] = {-0.0880f, -0.0640f, -0.0400f, -0.0056f, 0.0372f, 0.0818f, 0.1116f};
  static const float rc_[7] = {0.0210f, 0.0075f, -0.0035f, -0.0090f, -0.0110f, -0.0190f, -0.0250f};
  static const float rx_[7] = {0.0505f, 0.0680f, 0.0752f, 0.0790f, 0.0780f, 0.0692f, 0.0480f};
  static const float rz_[7] = {0.0570f, 0.0830f, 0.0930f, 0.0980f, 0.0965f, 0.0850f, 0.0605f};
  int k = 0;
  while (k < 5 && y > ry_[k + 1]) k++;
  float u = f_clamp((y - ry_[k]) / (ry_[k + 1] - ry_[k]), 0.0f, 1.0f);
  float c = rc_[k] + (rc_[k + 1] - rc_[k]) * u;
  float rx = rx_[k] + (rx_[k + 1] - rx_[k]) * u, rz = rz_[k] + (rz_[k + 1] - rz_[k]) * u;
  float xr = f_clamp(x / rx, -0.97f, 0.97f);
  float zf = c + rz * sqrtf(1.0f - xr * xr);
  float xs = xr * rx;
  v3 n = v3_norm(v3_add(v3_scale(hr, xs / (rx * rx)), v3_scale(hf, (zf - c) / (rz * rz))));
  v3 p = v3_add(hp, v3_add(v3_scale(hr, xs), v3_add(v3_scale(hu, y), v3_scale(hf, zf))));
  return v3_add(p, v3_scale(n, lift));
}
// A run of facial hair over the face: each point (x, y, r) a round node of
// radius r sunk 55% into the skin, so the strand reads as hair lying ON it.
// Full value jitter: every band a shade of its own, the way hair catches light.
static void face_hair(v3 hp, v3 hr, v3 hu, v3 hf, const float (*pt)[3], int n, v3 col) {
  fnode_t nd[FIG_NMAX];
  if (n > FIG_NMAX) n = FIG_NMAX;
  for (int q = 0; q < n; q++) {
    v3 c = q & 1 ? v3_scale(col, 0.90f) : col;
    nd[q] = (fnode_t){head_surf(hp, hr, hu, hf, pt[q][0], pt[q][1], pt[q][2] * 0.45f),
                      c, c, pt[q][2], pt[q][2], 0, 0.0f};
  }
  g_fig_jit = 1.0f;
  probe("beard");
  fig_chain(nd, n, 16, 1, 1, hf);
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
    // VERTEX PRESSURE IS A TIER TOO. The round body costs ~40k verts at the
    // close tier (12952 tris, measured with `figcheck 1 0`) where the faceted
    // one cost 25k, so twenty figures packed inside 10 m of the camera ask for
    // more than SCENE_MAX_VERTS — and a dropped triangle is a hole in whatever
    // was drawn last. Measured with `lineup` (20 bots, 5-10 m): 561k asked of
    // 524k. Each figure therefore steps down a tier until its measured cost
    // fits in what is left, keeping a reserve for the corpses and stowed guns
    // that share the pass, AND a coarsest-tier share for every figure still to
    // come (g_fig_todo) — greedy alone spends the buffer on the first dozen
    // bodies and leaves the rest to drop. Normal play never gets near it (20
    // bots hard peak 173k), and it can only LOWER a tier, like the ceilings
    // above.
    {
      // v5: MEASURED on the heaviest kit a bot can wear (gas mask + NVG up,
      // `figgear 4 3; figcheck 1 DIST`, tris x 3): 4988 / 7180 / 25032 / 29560
      // tris. The old {15k, 20k, 28k, 40k} was stale before v5 (the shipped
      // close tier measured 62.6k verts), and with the hero masks it let a
      // packed squad overcommit the buffer and drop 30k verts (art_review/r10).
      static const int cost[4] = {15500, 21500, 76000, 90000};   // k 6, 8, 12, 16
      int after = g_fig_todo > 1 ? g_fig_todo - 1 : 0;
      if (g_fig_todo > 0) g_fig_todo--;
      int left = SCENE_MAX_VERTS - g_scene_floats / VERT_FLOATS - 16384 - after * cost[0];
      while (g_fig_k > 6) {
        int t = g_fig_k >= 16 ? 3 : g_fig_k >= 12 ? 2 : 1;
        if (cost[t] <= left) break;
        g_fig_k = g_fig_k >= 16 ? 12 : g_fig_k >= 12 ? 8 : 6;
      }
    }
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
  int living_pose = !dth || dth->slack <= 0.0001f;
  // THE THROW IS THROWN WITH THE BODY (grenade, see NADE_HOLD). The trunk
  // turns about the vertical through the pelvis (tw: + takes the throwing
  // shoulder back) and pitches over the hips (bd: + brings the chest forward),
  // as one rigid move of everything above the waist — the same construction
  // the lean uses. The head only half-turns: the eyes stay on the target.
  // Applied to the interpolated pose BEFORE anything is built from it, so the
  // torso, the shoulders and the arms all come from the moved skeleton.
  // v5: the KNIFE drives the same two channels (KNIFE_BODY), plus a lunge below.
  if ((!dth || dth->slack <= 0.0001f) && (wpn == WPN_GR || wpn == WPN_KN) &&
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

  // THE KNIFE'S LUNGE (v5, gs.ln / gs.lf): the hips and everything above them
  // travel `ln` forward along the facing and a quarter of it down, the lead
  // (left) foot steps out 1.4x as far and lifts `lf` on the way, the trail foot
  // stays planted, and both knees are re-solved on their own bend. Applied to
  // the interpolated pose like the throw: presentation only — the colliders and
  // the sim never see it. Negative `ln` is the weight going back on the cock.
  if (living_pose && wpn == WPN_KN && (fabsf(gs.ln) > 1e-4f || gs.lf > 1e-4f)) {
    v3 F = {{P.head_f.x, 0, P.head_f.z}};
    F = v3_dot(F, F) > 1e-6f ? v3_norm(F) : (v3){{sinf(yaw), 0, -cosf(yaw)}};
    v3 d = v3_add(v3_scale(F, gs.ln), (v3){{0, -0.25f * fabsf(gs.ln), 0}});
    v3 *up_[] = {&P.pelvis, &P.chest, &P.neck, &P.head, &P.gun_o, &P.hip[0], &P.hip[1],
                 &P.sho[0], &P.sho[1], &P.elb[0], &P.elb[1], &P.wri[0], &P.wri[1]};
    for (unsigned k = 0; k < sizeof up_ / sizeof *up_; k++) *up_[k] = v3_add(*up_[k], d);
    v3 step = v3_add(v3_scale(F, 1.4f * fmaxf(gs.ln, 0.0f)), (v3){{0, gs.lf, 0}});
    P.ankle[0] = v3_add(P.ankle[0], step);
    P.toe[0] = v3_add(P.toe[0], step);
    for (int i = 0; i < 2; i++) {
      v3 pole = v3_sub(P.knee[i], v3_lerp(P.hip[i], P.ankle[i], 0.5f));
      if (v3_dot(pole, pole) < 1e-8f) pole = F;
      P.knee[i] = ik_solve(P.hip[i], P.ankle[i], ANIM_THIGH, ANIM_SHIN, v3_norm(pole));
    }
  }
  // THE RELOAD MOVES THE HEAD AND SHOULDERS (v5). The weapon already comes off
  // the shoulder and cants (the reload block below), but the head stayed on the
  // aim and the shoulders square — hands working a magazine under a statue's
  // face. The head drops and comes forward to LOOK at the magwell (and pitches
  // down 0.30 rad), the support shoulder rolls forward and down to send the
  // hand to the pouch. Same timeline (gs.arc, gs.hand), applied here because the
  // head is built before the weapon. Living figures, long guns and pistol.
  if (living_pose && (gs.arc > 1e-4f || gs.hand > 1e-4f) && wpn != WPN_KN && wpn != WPN_GR) {
    v3 F = {{P.head_f.x, 0, P.head_f.z}};
    F = v3_dot(F, F) > 1e-6f ? v3_norm(F) : (v3){{sinf(yaw), 0, -cosf(yaw)}};
    float k = f_clamp(gs.arc * 1.6f, 0.0f, 1.0f);
    P.head = v3_add(P.head, v3_add(v3_scale(F, 0.026f * k), (v3){{0, -0.030f * k, 0}}));
    P.neck = v3_add(P.neck, v3_add(v3_scale(F, 0.012f * k), (v3){{0, -0.012f * k, 0}}));
    v3 hf_ = v3_norm(P.head_f), hr_ = v3_cross(hf_, (v3){{0, 1, 0}});
    if (v3_dot(hr_, hr_) > 1e-6f) P.head_f = v3_rot_axis(hf_, v3_norm(hr_), -0.30f * k);
    float hk = f_clamp(gs.hand, 0.0f, 1.0f);
    P.sho[0] = v3_add(P.sho[0], v3_add(v3_scale(F, 0.024f * hk), (v3){{0, -0.016f * hk, 0}}));
  }
  // THE OPERATOR STANDS ATHLETIC (v5). Standing, his chest comes 3 degrees
  // over the toes about the pelvis — compact, weight forward, on soft knees and
  // the staggered feet the solver gives him — where the insurgent below hunches
  // 5 degrees and stands wide. Presentation only and operator only (the solver
  // is shared, and putting it there gave the insurgent 8 degrees). Fades out
  // with any movement, a crouch, a slide or a jump.
  if (living_pose && !FIG_INS) {
    float st_ = (1.0f - f_clamp(A->spd / 0.6f, 0.0f, 1.0f)) * (1.0f - A->crouch_s) *
                (1.0f - A->slide_s) * (1.0f - A->air_s);
    if (st_ > 1e-3f) {
      v3 F = {{P.head_f.x, 0, P.head_f.z}};
      F = v3_dot(F, F) > 1e-6f ? v3_norm(F) : (v3){{sinf(yaw), 0, -cosf(yaw)}};
      v3 Rt = v3_cross(F, (v3){{0, 1, 0}});
      v3 pv = P.pelvis;
      float ang = -0.052f * st_;   // about Rt, negative takes the chest FORWARD
      #define ATHL(p_) ((p_) = v3_add(pv, v3_rot_axis(v3_sub((p_), pv), Rt, ang)))
      for (int i = 0; i < 2; i++) { ATHL(P.sho[i]); ATHL(P.elb[i]); ATHL(P.wri[i]); }
      ATHL(P.chest); ATHL(P.neck); ATHL(P.head); ATHL(P.gun_o);
      #undef ATHL
      // ...on SOFT KNEES: everything from the hips up 16 mm lower, the feet
      // where they are, both knees re-solved on their own bend (`gait` idle:
      // the knee stood at full height, a man at attention).
      v3 dn = {{0, -0.016f * st_, 0}};
      v3 *up_[] = {&P.pelvis, &P.chest, &P.neck, &P.head, &P.gun_o, &P.hip[0], &P.hip[1],
                   &P.sho[0], &P.sho[1], &P.elb[0], &P.elb[1], &P.wri[0], &P.wri[1]};
      for (unsigned k = 0; k < sizeof up_ / sizeof *up_; k++) *up_[k] = v3_add(*up_[k], dn);
      for (int i = 0; i < 2; i++) {
        v3 pole = v3_sub(P.knee[i], v3_lerp(P.hip[i], P.ankle[i], 0.5f));
        if (v3_dot(pole, pole) < 1e-8f) pole = F;
        P.knee[i] = ik_solve(P.hip[i], P.ankle[i], ANIM_THIGH, ANIM_SHIN, v3_norm(pole));
      }
    }
  }
  // THE INSURGENT STANDS LIKE ONE. An operator's carry is upright and
  // compact; an irregular fighter is a little hunched over his rifle and
  // stands wider. Applied to the INTERPOLATED pose, like the throw above, so it
  // is presentation only: the sim skeleton, the colliders and every proof that
  // measures the operator are untouched. The upper body pitches 5 degrees over
  // the hips about the pelvis (positions only: the rifle keeps pointing where
  // the pose aimed it, the head keeps looking where it looked) and each foot
  // goes 30 mm further out, with the knee re-solved on its own bend.
  if (living_pose && FIG_INS && !g_fig_hos && A->slide_s < 0.01f) {
    v3 F = {{P.head_f.x, 0, P.head_f.z}};
    F = v3_dot(F, F) > 1e-6f ? v3_norm(F) : (v3){{sinf(yaw), 0, -cosf(yaw)}};
    v3 Rt = v3_cross(F, (v3){{0, 1, 0}});
    v3 pv = P.pelvis;
    const float hunch = -0.088f;   // negative: about Rt this takes the chest FORWARD
    #define HUNCH(p_) ((p_) = v3_add(pv, v3_rot_axis(v3_sub((p_), pv), Rt, hunch)))
    for (int i = 0; i < 2; i++) { HUNCH(P.sho[i]); HUNCH(P.elb[i]); HUNCH(P.wri[i]); }
    HUNCH(P.chest); HUNCH(P.neck); HUNCH(P.head); HUNCH(P.gun_o);
    #undef HUNCH
    v3 lat = v3_norm(P.right);
    for (int i = 0; i < 2; i++) {
      v3 d = v3_scale(lat, i ? 0.030f : -0.030f);
      v3 pole = v3_sub(P.knee[i], v3_lerp(P.hip[i], P.ankle[i], 0.5f));
      if (v3_dot(pole, pole) < 1e-8f) pole = F;
      P.ankle[i] = v3_add(P.ankle[i], d);
      P.toe[i] = v3_add(P.toe[i], d);
      P.knee[i] = ik_solve(P.hip[i], P.ankle[i], ANIM_THIGH, ANIM_SHIN, v3_norm(pole));
    }
  }
  // No painted blob under the figure any more: the sun casts a real silhouette
  // now, which lands where the light says instead of always straight down, tracks
  // a jump on its own, and shows the pose. The disc that used to be here was a
  // hard-edged octagon that fought the real shadow it overlapped — and it was
  // also a CASTER, so it threw a second disc shadow of its own.
  g_fig_y = pos.y;
  kit_t kt = fig_kit(palette, flash01, flash_col);
  // THE WEAPON SWAP (see SWAP_A_LONG): resolved BEFORE fig_torso so the upper
  // body, shoulders, and head turn smoothly into the reach instead of reaching
  // off a frozen statue. C1-continuous across the stow->draw seam.
  int living = !dth || dth->slack <= 0.0001f;
  int hold = wpn, sw_phase = 0, sw_from = wpn;
  float sw_k = 0.0f;
  int carry = living && g_fig_ent >= 0 && g_fig_ent < MAX_ENTS &&
              (g_fig_ent == g_local_ent || g_fig_k >= 12);
  if (living && g_fig_ent >= 0 && g_fig_ent < MAX_ENTS) {
    int e = g_fig_ent;
    float now = ((float)g_tick + alpha) * TICK_DT;
    if (g_sw_seen[e] != wpn + 1) {
      if (g_sw_seen[e]) { g_sw_from[e] = g_sw_seen[e] - 1; g_sw_t0[e] = now; }
      else g_sw_t0[e] = -1e9f;
      g_sw_rush[e] = -2e9f;
      g_sw_seen[e] = wpn + 1;
    }
    int from = g_sw_from[e];
    sw_from = from;
    float da = swap_stow_s(from), db = swap_draw_s(wpn);
    float t = now - g_sw_t0[e];
    if (gs.trig > 0.05f && g_sw_rush[e] < g_sw_t0[e] && t >= 0.0f && t < da + db) g_sw_rush[e] = now;
    if (g_sw_rush[e] >= g_sw_t0[e]) t = (g_sw_rush[e] - g_sw_t0[e]) + (now - g_sw_rush[e]) * SWAP_RUSH;
    if (carry && from != wpn && t >= 0.0f && t < da + db) {
      if (t < da) { sw_phase = 1; sw_k = t / da; hold = from; }
      else        { sw_phase = 2; sw_k = (t - da) / db; }
      // Natural torso counter-twist, shoulder drop/ease, and subtle head follow
      // driven by smooth bell curves over the full [0, da + db] timeline.
      float twA, s0yA, s0fA, s1yA, s1fA, hpA;
      float twB, s0yB, s0fB, s1yB, s1fB, hpB;
      swap_slot_body(from, &twA, &s0yA, &s0fA, &s1yA, &s1fA, &hpA);
      swap_slot_body(wpn,  &twB, &s0yB, &s0fB, &s1yB, &s1fB, &hpB);
      float wA = da > 0.001f ? swap_bell(t, 0.0f, 0.72f * da, da + 0.35f * db) : 0.0f;
      float wB = db > 0.001f ? swap_bell(t, 0.65f * da, da + 0.28f * db, da + db) : 0.0f;
      float stw = twA * wA + twB * wB;
      float s0y = s0yA * wA + s0yB * wB, s0f = s0fA * wA + s0fB * wB;
      float s1y = s1yA * wA + s1yB * wB, s1f = s1fA * wA + s1fB * wB;
      float shp = hpA * wA + hpB * wB;
      if (fabsf(stw) > 1e-4f || fabsf(s0y) + fabsf(s1y) + fabsf(shp) > 1e-4f) {
        v3 F = {{P.head_f.x, 0, P.head_f.z}};
        F = v3_dot(F, F) > 1e-6f ? v3_norm(F) : (v3){{sinf(yaw), 0, -cosf(yaw)}};
        v3 U = {{0, 1, 0}};
        v3 Rt = v3_cross(F, U);
        v3 pv = P.pelvis;
        #define SW_TURN(p_, k_) do {                                             \
          float a_ = -stw * (k_), c_ = cosf(a_), s_ = sinf(a_);                  \
          v3 d_ = v3_sub((p_), pv);                                              \
          float r_ = v3_dot(d_, Rt), f_ = v3_dot(d_, F);                         \
          d_ = v3_add(d_, v3_add(v3_scale(Rt, r_ * c_ - f_ * s_ - r_),           \
                                 v3_scale(F,  f_ * c_ + r_ * s_ - f_)));         \
          (p_) = v3_add(pv, d_);                                                 \
        } while (0)
        for (int i = 0; i < 2; i++) {
          SW_TURN(P.sho[i], 1.0f);
          SW_TURN(P.elb[i], 1.0f);
          SW_TURN(P.wri[i], 1.0f);
        }
        SW_TURN(P.chest, 1.0f);
        SW_TURN(P.neck, 1.0f);
        SW_TURN(P.gun_o, 1.0f);
        SW_TURN(P.head, 0.35f);
        #undef SW_TURN
        {
          float c_ = cosf(stw), s_ = sinf(-stw);
          float r_ = v3_dot(P.right, Rt), f_ = v3_dot(P.right, F);
          P.right = v3_add(v3_scale(Rt, r_ * c_ - f_ * s_), v3_scale(F, f_ * c_ + r_ * s_));
        }
        P.sho[0] = v3_add(P.sho[0], v3_add(v3_scale(F, s0f), (v3){{0, s0y, 0}}));
        P.sho[1] = v3_add(P.sho[1], v3_add(v3_scale(F, s1f), (v3){{0, s1y, 0}}));
        if (fabsf(shp) > 1e-4f) {
          v3 hf_ = v3_norm(P.head_f), hr_ = v3_cross(hf_, U);
          if (v3_dot(hr_, hr_) > 1e-6f) P.head_f = v3_rot_axis(hf_, v3_norm(hr_), shp);
        }
      }
    }
  }
  v3 Rc = v3_norm(P.right);
  v3 spine = v3_norm(v3_sub(P.chest, P.pelvis));
  v3 prt = v3_norm(v3_sub(P.hip[1], P.hip[0]));      // pelvis right
  int holster_pt = carry && hold != WPN_PT && mode_allowed(g_fig_ent, WPN_PT);
  // THE DUAL BERETTAS: a drop-leg holster on EACH thigh, a pistol in each when
  // they are not in his hands, and in his hands the pair (g_fig_dual).
  int fig_dual = g_fig_ent >= 0 && mode_allowed(g_fig_ent, WPN_PT) &&
                 WPN_VAR[1][ent_wvar(g_fig_ent, WPN_PT)].dual;
  g_fig_dual = fig_dual && hold == WPN_PT;

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
    // ...in the wrap's own two colours: the scarf is the check, the turban the
    // red, so the head reads as two windings and not as one red ball.
    v3 face = g_fig_hos ? kt.glove : ins ? v3_lerp(kt.wst, kt.helm, 0.25f) : kt.bala;
    v3 slit = g_fig_hos ? kt.glove : kt.bala;   // the eye line stays the dark cloth on both
    // (a hostage's face is bare skin: his eyes and brows are drawn on below)
    // v5: WHAT THE HEAD WEARS, decided once. `fg` is the face (the insurgent's
    // is always his wrap), `head` the headgear with the two NVG options folded
    // into the helmet they ride on. MASK x HEADGEAR, where a pair cannot work
    // the conflicting part is HIDDEN, never left to intersect:
    //   cap or wrap + goggles          -> the goggles (the balaclava stays)
    //   bare + goggles                 -> the goggles (no helmet to strap to)
    //   cap or wrap + any other mask   -> the cap / wrap (a mask's brow, lenses
    //                                     or glasses sit where the bill and the
    //                                     wrap's lowest turn are)
    // The helmet (and both NVG positions) takes every mask: its front cut sits
    // 12 mm above the highest mask edge (the gas masks' brow at y 0.045).
    int fg = ins ? FACE_PLAIN : FIG_FACE;
    int head = FIG_HEAD;
    int nvg = head == HEAD_NVG_UP ? 1 : head == HEAD_NVG_DOWN ? 2 : 0;
    if (nvg) head = HEAD_HELMET;
    if (fg == FACE_GOGGLES && head != HEAD_HELMET) fg = FACE_PLAIN;
    int ownlens = fg == FACE_GAS_TWIN || fg == FACE_GAS_PANO || fg == FACE_PLATE ||
                  fg == FACE_RESP || fg == FACE_GOGGLES || fg == FACE_SHEMAGH;
    if (ownlens && fg != FACE_GOGGLES && (head == HEAD_CAP || head == HEAD_WRAP)) head = HEAD_BARE;
    int skullface = fg == FACE_SKULL || fg == FACE_SKULL_HALF;
    // Past 12 m the head is procedural, and a mask has to survive the swap: the
    // skull's own rings take the mask's colours (rubber, composite or cloth over
    // the face; the lens or slit on the eye ring), and the parts that make its
    // OUTLINE (filters, canister, NVG) are drawn as chains below.
    int hero_ = !ins && !g_fig_sil && g_fig_k >= 12;
    v3 rc[7] = {face, face, face, slit, face, face, face};
    v3 rubber = v3_scale(kt.web, 0.62f);
    if (!hero_ && !ins) {
      v3 lo = face, eye = slit;
      int top = -1;
      if (fg == FACE_GAS_TWIN || fg == FACE_GAS_PANO) { lo = rubber; top = 4; eye = fg == FACE_GAS_PANO ? kt.gm2 : rubber; }
      else if (fg == FACE_PLATE) { lo = v3_scale(kt.helm, 0.62f); top = 4; eye = (v3){{0.035f, 0.035f, 0.040f}}; }
      else if (fg == FACE_RESP) { lo = rubber; top = 2; }
      else if (fg == FACE_SHEMAGH) { lo = v3_scale(kt.helm, 0.80f); top = 2; eye = kt.gm2; }
      for (int q = 0; q <= top; q++) rc[q] = q == 3 ? eye : lo;
      if (top >= 0 && top < 3) rc[3] = eye;
    }
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
    fig_chain(nk, 5, 16, 1, 1, Rc);
    if (g_fig_hos) {
      // THE SHIRT COLLAR: a band round the back of the neck standing 12 mm
      // off it, turning down at the front into two points either side of the
      // throat — the one line that says "shirt" rather than "tunic".
      v3 cfw_ = v3_norm(v3_cross(spine, Rc)), col = v3_scale(kt.olive, 0.97f);
      for (int e = 0; e < 2; e++) {
        float es = e ? 1.0f : -1.0f;
        #define CP(r_, u_, f_) v3_add(P.chest, v3_add(v3_scale(Rc, es * (r_)), \
                               v3_add(v3_scale(spine, (u_)), v3_scale(cfw_, (f_)))))
        fnode_t cl[4] = {
          {CP(0.000f, 0.112f, -0.086f), col, col, 0.016f, 0.005f, 0, 0.0f},
          {CP(0.062f, 0.114f, -0.058f), col, col, 0.017f, 0.005f, 0, 0.0f},
          {CP(0.086f, 0.108f, 0.020f), col, col, 0.017f, 0.005f, 0, 0.0f},
          {CP(0.040f, 0.082f, 0.092f), col, col, 0.012f, 0.004f, 0, 0.0f}};
        #undef CP
        probe("collar");
        fig_chain(cl, 4, 8, 1, 1, spine);
      }
    }
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
      {HP(0, -0.0880f, 0.0210f), rc[0], rc[0], 0.0505f, 0.0570f, 0, 0.74f},
      {HP(0, -0.0640f, 0.0075f), rc[1], rc[1], 0.0680f, 0.0830f, 0, 0.80f},
      {HP(0, -0.0400f, -0.0035f), rc[2], rc[2], 0.0752f, 0.0930f, 0, 0.88f},
      {HP(0, -0.0056f, -0.0090f), rc[3], rc[3], 0.0790f, 0.0980f, 0, 0.94f},
      {HP(0, 0.0372f, -0.0110f), rc[4], rc[4], 0.0780f, 0.0965f, 0, 0.78f},
      {HP(0, 0.0818f, -0.0190f), rc[5], rc[5], 0.0692f, 0.0850f, 0, 0.0f},
      {HP(0, 0.1116f, -0.0250f), rc[6], rc[6], 0.0480f, 0.0605f, 0, 0.0f}};
    probe("skull");
    // The head is what a shooter looks at, so it gets one profile tier MORE
    // than the body at every distance (20 inside 6 m): a 16-gon skull read as a
    // faceted can at conversational range, the one place it is ever examined.
    int kbody = g_fig_k;
    g_fig_k = kbody >= 16 ? 20 : kbody >= 12 ? 16 : kbody >= 8 ? 12 : kbody;
    fig_chain(hd, 7, 20, 1, 1, hr);
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
        fig_chain(er, 2, 12, 1, 1, hu);
      }
    }
    // THE SKULL PRINT on the balaclava (FIG_FACE): bone-coloured shapes printed on
    // the cloth — two rows of teeth with the dark gaps between them, the maxilla
    // either side of the black nose cavity, the mandible down both sides to the
    // chin, the cheekbones under the goggle line; the full mask adds the brow
    // ridge over the goggles and the temples beside them. Each shape is a 2.7 mm
    // disc laid ON the face's own surface: its point and normal come from the
    // skull rings above (hd[]), interpolated at its height, so the print follows
    // the curve of the face and never floats off it or sinks into it.
    // THE HERO HEAD (tools/hero_models.py): an operator drawn at the close tier
    // wears the sculpted helmet, visor and skull mask in place of the procedural
    // shell, goggle band and face print. Bots at range keep the cheap head.
    int hero = !ins && g_fig_k >= 16 && !g_fig_sil;
    if (!ins && !hero && skullface && g_fig_k >= 8) {
      int full = fg == FACE_SKULL;
      // v5: matte bone-grey (was 0.72 bone, the lightest value on the head)
      v3 bone = {{0.460f, 0.450f, 0.420f}};
      const float ry_[5] = {-0.0880f, -0.0640f, -0.0400f, -0.0056f, 0.0372f};
      const float rc_[5] = {0.0210f, 0.0075f, -0.0035f, -0.0090f, -0.0110f};
      const float rx_[5] = {0.0505f, 0.0680f, 0.0752f, 0.0790f, 0.0780f};
      const float rz_[5] = {0.0570f, 0.0830f, 0.0930f, 0.0980f, 0.0965f};
      // x across, y up (head frame), half-width, half-height, layer: 0 bone,
      // 1 the dark lines PRINTED OVER the bone (tooth gaps, the mouth line, the
      // nose cavity) — a skull reads by its dark gaps cut into a bone mass, not
      // by white dots on black.
      static const float SK[][5] = {
        // bone: the teeth band, the maxilla, cheekbones, the mandible to the chin
        { 0.0000f, -0.0450f, 0.0345f, 0.0130f, 0}, { 0.0000f, -0.0240f, 0.0280f, 0.0080f, 0},
        {-0.0500f, -0.0185f, 0.0160f, 0.0070f, 0}, { 0.0500f, -0.0185f, 0.0160f, 0.0070f, 0},
        { 0.0000f, -0.0700f, 0.0240f, 0.0080f, 0},
        {-0.0400f, -0.0620f, 0.0120f, 0.0085f, 0}, { 0.0400f, -0.0620f, 0.0120f, 0.0085f, 0},
        {-0.0540f, -0.0440f, 0.0070f, 0.0120f, 0}, { 0.0540f, -0.0440f, 0.0070f, 0.0120f, 0},
        // ink over the bone: the mouth line, the tooth gaps, the nose cavity
        { 0.0000f, -0.0450f, 0.0320f, 0.0017f, 1}, { 0.0000f, -0.0225f, 0.0078f, 0.0090f, 1},
        { 0.0000f, -0.0450f, 0.0016f, 0.0112f, 1},
        {-0.0110f, -0.0450f, 0.0016f, 0.0110f, 1}, { 0.0110f, -0.0450f, 0.0016f, 0.0110f, 1},
        {-0.0215f, -0.0450f, 0.0016f, 0.0100f, 1}, { 0.0215f, -0.0450f, 0.0016f, 0.0100f, 1},
        {-0.0300f, -0.0450f, 0.0016f, 0.0075f, 1}, { 0.0300f, -0.0450f, 0.0016f, 0.0075f, 1},
        // FULL ONLY: the brow ridge over the goggles, the temples
        {-0.0300f, 0.0200f, 0.0220f, 0.0070f, 0}, { 0.0300f, 0.0200f, 0.0220f, 0.0070f, 0},
        {-0.0680f, -0.0040f, 0.0060f, 0.0120f, 0}, { 0.0680f, -0.0040f, 0.0060f, 0.0120f, 0}};
      v3 ink = {{0.045f, 0.045f, 0.050f}};   // printed black, not the cloth's own colour
      int nsk = full ? (int)(sizeof SK / sizeof SK[0]) : 18;
      for (int q = 0; q < nsk; q++) {
        float x = SK[q][0], y = SK[q][1];
        int k = 0;
        while (k < 3 && y > ry_[k + 1]) k++;
        float u = f_clamp((y - ry_[k]) / (ry_[k + 1] - ry_[k]), 0.0f, 1.0f);
        float c = rc_[k] + (rc_[k + 1] - rc_[k]) * u;
        float rx = rx_[k] + (rx_[k + 1] - rx_[k]) * u, rz = rz_[k] + (rz_[k + 1] - rz_[k]) * u;
        float xr = f_clamp(x / rx, -0.97f, 0.97f);
        float zf = c + rz * sqrtf(1.0f - xr * xr);
        v3 n = v3_norm(v3_add(v3_scale(hr, x / (rx * rx)), v3_scale(hf, (zf - c) / (rz * rz))));
        v3 sp = HP(x, y, zf);
        v3 side = v3_norm(v3_cross(hu, n));   // across the face, in its tangent plane
        int dk = SK[q][4] > 0.5f;
        v3 pc = dk ? ink : bone;
        // The ink lies over the bone, and the MOUTH LINE (q 9) over the tooth gaps
        // it crosses: every overlapping pair has its caps at different depths.
        float lift = dk ? (q == 9 ? 0.0026f : 0.0020f) : 0.0012f;
        float sink = dk ? (q == 9 ? 0.0006f : 0.0010f) : 0.0015f;
        fnode_t pt[2] = {
          {v3_sub(sp, v3_scale(n, sink)), pc, pc, SK[q][2], SK[q][3], 0, 0.0f},
          {v3_add(sp, v3_scale(n, lift)), pc, pc, SK[q][2], SK[q][3], 0, 0.0f}};
        probe("skullprint");
        fig_chain(pt, 2, 8, 1, 1, side);
      }
    }
    if (g_fig_hos && g_fig_hos_hood) {
      // THE SACK. Loose hessian over the whole head, gathered by a drawstring
      // under the jaw: every ring 10-20 mm clear of the skull ring under it
      // (looser than the wrap: it is a bag, not a winding), the bulk carried
      // round the crown, the hem bunched below the chin over the collar.
      g_fig_jit = 1.0f;
      v3 sk = kt.helm, sw = kt.wst;
      fnode_t sc[8] = {
        {HP(0, -0.118f, 0.002f), sw, sw, 0.074f, 0.078f, 0, 0.60f},    // the bunched hem
        {HP(0, -0.104f, 0.008f), sw, sw, 0.064f, 0.070f, 0, 0.0f},     // the drawstring
        {HP(0, -0.082f, 0.012f), sk, sk, 0.072f, 0.084f, 0, 0.0f},
        {HP(0, -0.050f, 0.004f), sk, sw, 0.090f, 0.106f, 0, 0.0f},
        {HP(0, -0.008f, -0.008f), sw, sk, 0.096f, 0.114f, 0, 0.0f},
        {HP(0, 0.040f, -0.012f), sk, sk, 0.096f, 0.113f, 0, 0.0f},
        {HP(0, 0.090f, -0.020f), sk, sw, 0.082f, 0.098f, 0, 0.0f},
        {HP(0, 0.132f, -0.026f), sw, sw, 0.046f, 0.056f, 0, 0.0f}};
      probe("hood");
      fig_chain(sc, 8, 20, 1, 1, hr);
      // the drawstring's knot and its two tails under the chin
      v3 cord = v3_scale(sw, 0.80f);
      fnode_t kn[3] = {
        {HP(0, -0.104f, 0.060f), cord, cord, 0.010f, 0.010f, 0, 0.0f},
        {HP(0.008f, -0.140f, 0.066f), cord, cord, 0.005f, 0.005f, 0, 0.0f},
        {HP(0.014f, -0.178f, 0.060f), cord, cord, 0.004f, 0.004f, 0, 0.0f}};
      probe("hood");
      fig_chain(kn, 3, 6, 1, 1, hr);
      // ...the two sewn corners of the bag standing off the crown, which is
      // what makes it a SACK pulled over a head and not a knitted cap,
      fnode_t ea[2][3];
      for (int e = 0; e < 2; e++) {
        float es = e ? 1.0f : -1.0f;
        ea[e][0] = (fnode_t){HP(es * 0.040f, 0.114f, -0.026f), sk, sk, 0.030f, 0.028f, 0, 0.0f};
        ea[e][1] = (fnode_t){HP(es * 0.072f, 0.132f, -0.032f), sk, sw, 0.016f, 0.012f, 0, 0.0f};
        ea[e][2] = (fnode_t){HP(es * 0.086f, 0.122f, -0.040f), sw, sw, 0.008f, 0.006f, 0, 0.0f};   // flops over
        probe("hood");
        fig_chain(ea[e], 3, 8, 1, 1, hf);
      }
      // ...and the rope wound twice round the neck over the hem.
      v3 rope = {{0.300f, 0.250f, 0.170f}};
      fnode_t rp[2] = {
        {HP(0, -0.112f, 0.004f), rope, rope, 0.079f, 0.083f, 0, 0.0f},
        {HP(0, -0.098f, 0.006f), rope, rope, 0.073f, 0.079f, 0, 0.0f}};
      fig_chain(rp, 2, 16, 1, 1, hr);
    } else if (g_fig_hos) {
      // THE FREED HOSTAGE'S FACE: bare skin, so it gets what the balaclava never
      // needed — hair, brows, eyes, a nose and a mouth. The hair rings sit
      // further BACK than the skull's (their centre behind it), so they clear
      // the back and sides by 7-18 mm and fall inside the forehead: a hairline.
      // grey (him: receding, a beard, glasses), black (short, a moustache),
      // auburn (her: full, a ponytail)
      static const float HAIR[3][3] = {{0.560f, 0.550f, 0.530f}, {0.045f, 0.040f, 0.038f},
                                       {0.520f, 0.340f, 0.170f}};
      int hv = (g_fig_hos - 1) % 3;
      v3 hc = {{HAIR[hv][0], HAIR[hv][1], HAIR[hv][2]}};
      g_fig_jit = 0.50f;
      if (hv == 0) {   // a fringe round the back and sides; the crown is bare
        fnode_t hh[3] = {
          {HP(0, -0.032f, -0.058f), hc, hc, 0.074f, 0.066f, 0, 0.45f},
          {HP(0, 0.004f, -0.062f), hc, hc, 0.086f, 0.068f, 0, 0.0f},
          {HP(0, 0.042f, -0.066f), hc, hc, 0.082f, 0.062f, 0, 0.0f}};   // all behind the temples
        probe("hair");
        fig_chain(hh, 3, 20, 1, 1, hr);
        // the beard: rings centred FORWARD of the jaw's, so they stand off the
        // chin and cheeks and sink into the neck behind
        // THE BEARD, laid on the face (head_surf): a full band along the jaw
        // from sideburn to sideburn, thickest and lowest at the chin; the
        // cheeks filled above it up to the moustache, which joins them across
        // the upper lip; the mouth left open between.
        v3 bd = v3_scale(hc, 0.90f);
        static const float JAW[9][3] = {
          {-0.077f, 0.004f, 0.010f}, {-0.075f, -0.034f, 0.015f}, {-0.064f, -0.068f, 0.020f},
          {-0.040f, -0.092f, 0.024f}, {0.000f, -0.104f, 0.028f}, {0.040f, -0.092f, 0.024f},
          {0.064f, -0.068f, 0.020f}, {0.075f, -0.034f, 0.015f}, {0.077f, 0.004f, 0.010f}};
        face_hair(P.head, hr, hu, hf, JAW, 9, bd);
        for (int e = 0; e < 2; e++) {   // cheek fill: sideburn -> mouth corner
          float es = e ? 1.0f : -1.0f;
          const float CH[4][3] = {{es * 0.066f, -0.020f, 0.012f}, {es * 0.058f, -0.046f, 0.016f},
                                  {es * 0.042f, -0.064f, 0.016f}, {es * 0.028f, -0.070f, 0.012f}};
          face_hair(P.head, hr, hu, hf, CH, 4, bd);
        }
        static const float MO[5][3] = {   // the moustache, drooping at the corners
          {-0.030f, -0.058f, 0.008f}, {-0.020f, -0.045f, 0.010f}, {0.000f, -0.041f, 0.010f},
          {0.020f, -0.045f, 0.010f}, {0.030f, -0.058f, 0.008f}};
        face_hair(P.head, hr, hu, hf, MO, 5, v3_scale(bd, 1.06f));
        static const float CN[3][3] = {   // under the lower lip, down into the chin
          {0.000f, -0.070f, 0.012f}, {0.000f, -0.084f, 0.020f}, {0.000f, -0.100f, 0.024f}};
        face_hair(P.head, hr, hu, hf, CN, 3, bd);
      } else {
        float big = hv == 2 ? 1.06f : 1.0f;   // hers is fuller
        fnode_t hh[5] = {
          {HP(0, hv == 2 ? -0.070f : -0.030f, -0.052f), hc, hc, 0.074f * big, 0.074f * big, 0, 0.45f},
          {HP(0, 0.004f, -0.044f), hc, hc, 0.087f * big, 0.084f * big, 0, 0.0f},
          {HP(0, 0.050f, -0.030f), hc, hc, 0.087f * big, 0.096f * big, 0, 0.0f},
          {HP(0, 0.088f, -0.024f), hc, hc, 0.077f * big, 0.094f * big, 0, 0.0f},
          {HP(0, 0.120f, -0.027f), hc, hc, 0.050f * big, 0.064f * big, 0, 0.0f}};
        probe("hair");
        fig_chain(hh, 5, 20, 1, 1, hr);
        if (hv == 2) {   // shoulder-length at the back, hanging behind the neck
          fnode_t lg[3] = {
            {HP(0, -0.040f, -0.058f), hc, hc, 0.084f, 0.074f, 0, 0.0f},
            {HP(0, -0.110f, -0.062f), hc, hc, 0.088f, 0.066f, 0, 0.0f},
            {HP(0, -0.160f, -0.066f), v3_scale(hc, 0.85f), hc, 0.080f, 0.054f, 0, 0.0f}};
          probe("hair");
          fig_chain(lg, 3, 16, 1, 1, hr);
        }
        if (hv == 2) {   // the ponytail, tied at the back of the head
          fnode_t pt[5] = {
            {HP(0, 0.040f, -0.100f), hc, hc, 0.026f, 0.026f, 0, 0.0f},
            {HP(0, 0.030f, -0.128f), v3_scale(hc, 0.6f), hc, 0.020f, 0.020f, 0, 0.0f},   // the band
            {HP(0, -0.010f, -0.142f), hc, hc, 0.026f, 0.024f, 0, 0.0f},
            {HP(0, -0.080f, -0.146f), hc, hc, 0.022f, 0.020f, 0, 0.0f},
            {HP(0, -0.140f, -0.136f), hc, hc, 0.010f, 0.010f, 0, 0.0f}};
          probe("ponytail");
          fig_chain(pt, 5, 12, 1, 1, hr);
        }
        if (hv == 1) {   // a trimmed moustache and a goatee, laid on the face
          static const float MO[5][3] = {
            {-0.028f, -0.050f, 0.006f}, {-0.018f, -0.044f, 0.008f}, {0.000f, -0.041f, 0.008f},
            {0.018f, -0.044f, 0.008f}, {0.028f, -0.050f, 0.006f}};
          face_hair(P.head, hr, hu, hf, MO, 5, hc);
          for (int e = 0; e < 2; e++) {   // the corners run down to the chin
            float es = e ? 1.0f : -1.0f;
            const float SD[3][3] = {{es * 0.028f, -0.050f, 0.006f}, {es * 0.026f, -0.070f, 0.007f},
                                    {es * 0.016f, -0.090f, 0.010f}};
            face_hair(P.head, hr, hu, hf, SD, 3, hc);
          }
          static const float GT[3][3] = {
            {-0.016f, -0.090f, 0.010f}, {0.000f, -0.098f, 0.014f}, {0.016f, -0.090f, 0.010f}};
          face_hair(P.head, hr, hu, hf, GT, 3, hc);
          static const float SP[2][3] = {{0.000f, -0.068f, 0.006f}, {0.000f, -0.080f, 0.008f}};
          face_hair(P.head, hr, hu, hf, SP, 2, hc);   // the soul patch
        }
      }
      g_fig_jit = 1.0f;
      if (g_fig_k >= 8) {
        // Features ON the face: (x, y) in the head frame, laid on the skull
        // ellipse there the way the skull print is, as 2-node discs.
        const float ry_[5] = {-0.0880f, -0.0640f, -0.0400f, -0.0056f, 0.0372f};
        const float rc_[5] = {0.0210f, 0.0075f, -0.0035f, -0.0090f, -0.0110f};
        const float rx_[5] = {0.0505f, 0.0680f, 0.0752f, 0.0790f, 0.0780f};
        const float rz_[5] = {0.0570f, 0.0830f, 0.0930f, 0.0980f, 0.0965f};
        v3 lip = hv == 2 ? (v3){{0.520f, 0.180f, 0.170f}} : v3_scale(face, 0.72f);
        // x, y, half-width, half-height, colour 0 dark (eyes) 1 hair (brows) 2 lip
        static const float FT[][5] = {
          {-0.030f, 0.010f, 0.0095f, 0.0042f, 0}, {0.030f, 0.010f, 0.0095f, 0.0042f, 0},
          {-0.031f, 0.026f, 0.0140f, 0.0030f, 1}, {0.031f, 0.026f, 0.0140f, 0.0030f, 1},
          {0.000f, -0.054f, 0.0185f, 0.0032f, 2}};
        for (int q = 0; q < 5; q++) {
          float x = FT[q][0], y = FT[q][1];
          int k = 0;
          while (k < 3 && y > ry_[k + 1]) k++;
          float u = f_clamp((y - ry_[k]) / (ry_[k + 1] - ry_[k]), 0.0f, 1.0f);
          float c = rc_[k] + (rc_[k + 1] - rc_[k]) * u;
          float rx = rx_[k] + (rx_[k + 1] - rx_[k]) * u, rz = rz_[k] + (rz_[k + 1] - rz_[k]) * u;
          float xr = f_clamp(x / rx, -0.97f, 0.97f);
          float zf = c + rz * sqrtf(1.0f - xr * xr);
          v3 n = v3_norm(v3_add(v3_scale(hr, x / (rx * rx)), v3_scale(hf, (zf - c) / (rz * rz))));
          v3 sp = HP(x, y, zf);
          v3 side = v3_norm(v3_cross(hu, n));
          v3 pc = FT[q][4] < 0.5f ? kt.bala : FT[q][4] < 1.5f ? hc : lip;
          fnode_t pt[2] = {
            {v3_sub(sp, v3_scale(n, 0.0015f)), pc, pc, FT[q][2], FT[q][3], 0, 0.0f},
            {v3_add(sp, v3_scale(n, 0.0022f)), pc, pc, FT[q][2], FT[q][3], 0, 0.0f}};
          probe("face");
          fig_chain(pt, 2, 8, 1, 1, side);
        }
        // the nose: a wedge out of the face between the eyes and the mouth
        fnode_t ns[3] = {
          {HP(0, 0.008f, 0.078f), face, face, 0.008f, 0.007f, 0, 0.0f},
          {HP(0, -0.020f, 0.098f), face, face, 0.011f, 0.010f, 0, 0.0f},
          {HP(0, -0.033f, 0.093f), face, face, 0.012f, 0.009f, 0, 0.0f}};
        probe("nose");
        fig_chain(ns, 3, 8, 1, 1, hr);
        if (hv == 0) {   // his glasses: two tinted lenses on a dark frame, a bridge, the arms
          v3 fr = {{0.060f, 0.050f, 0.045f}}, gl = {{0.150f, 0.165f, 0.180f}};
          mat_set(MAT_GLASS_R, 0.0f);
          for (int e = 0; e < 2; e++) {
            float es = e ? 1.0f : -1.0f;
            fnode_t ln[2] = {{HP(es * 0.030f, 0.011f, 0.083f), fr, fr, 0.0125f, 0.0095f, 0, 0.0f},
                             {HP(es * 0.030f, 0.011f, 0.086f), gl, fr, 0.0115f, 0.0085f, 0, 0.0f}};
            probe("glasses");
            fig_chain(ln, 2, 12, 1, 1, hr);
            fnode_t am[2] = {{HP(es * 0.042f, 0.013f, 0.078f), fr, fr, 0.0025f, 0.0025f, 0, 0.0f},
                             {HP(es * 0.084f, 0.012f, -0.010f), fr, fr, 0.003f, 0.003f, 0, 0.0f}};
            fig_chain(am, 2, 6, 1, 1, hu);
          }
          fnode_t bg[2] = {{HP(-0.018f, 0.014f, 0.088f), fr, fr, 0.0025f, 0.0025f, 0, 0.0f},
                           {HP(0.018f, 0.014f, 0.088f), fr, fr, 0.0025f, 0.0025f, 0, 0.0f}};
          fig_chain(bg, 2, 6, 1, 1, hu);
          mat_set(MAT_CLOTH_R, 0.0f);
        }
      }
    } else if (head == HEAD_WRAP) {
      // THE HEAD WRAP. Five rings over the crown, each 9-15 mm clear of the
      // skull station under it and on the skull's own fore/aft centre line, so
      // the two surfaces never run parallel; the bottom cap meets the skull at
      // a right angle and reads as the wrap's folded edge over the brow.
      // FULL value jitter: each ring its own shade, which is exactly what a
      // cloth wound round a head in turns looks like (a shell damps it).
      g_fig_jit = 1.0f;
      v3 wdk = kt.wst;   // the check: off-white or black woven into the red
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
      fig_chain(wr, 5, 20, 1, 1, hr);   // cloth: round, smooth
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
      fig_chain(tw3, 4, 16, 1, 1, hr);
      g_fig_jit = 1.0f;
      // AN OPERATOR IN A WRAP STILL WEARS HIS HEADSET. The Armory offers the
      // wrap to the player, and without this a player in a wrap and a chest
      // rig was an insurgent outline in friendly colours. Two hard ear cups
      // over the cloth (centred 23 mm outside the wrap's widest ring, so they
      // cross it decisively) and a band over the crown keep the read: cups on
      // a head mean comms, and comms mean the squad.
      if (!ins) {
        mat_set(MAT_HELM_R, 0.0f);
        v3 cup = v3_scale(kt.web, 1.10f);
        for (int e = 0; e < 2; e++) {
          float es = e ? 1.0f : -1.0f;
          fnode_t ec[2] = {
            {HP(es * 0.088f, -0.024f, -0.004f), cup, cup, 0.030f, 0.036f, 0, 0.0f},
            {HP(es * 0.112f, -0.024f, -0.004f), cup, cup, 0.026f, 0.031f, 0, 0.0f}};
          probe("headset");
          fig_chain(ec, 2, 8, 1, 1, hu);
        }
        fnode_t hb[3] = {
          {HP(-0.100f, 0.010f, -0.004f), cup, cup, 0.008f, 0.012f, 0, 0.0f},
          {HP(0, 0.142f, -0.026f), cup, cup, 0.008f, 0.014f, 0, 0.0f},
          {HP(0.100f, 0.010f, -0.004f), cup, cup, 0.008f, 0.012f, 0, 0.0f}};
        probe("headset");
        fig_chain(hb, 3, 8, 1, 1, hf);
        mat_set(MAT_CLOTH_R, 0.0f);
      }
    } else {
      if (head == HEAD_HELMET && !hero) {
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
      fnode_t hm[10] = {
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
        {HP(0, 0.156f, -0.016f), kt.helm, kt.helm, 0.0330f, 0.0375f, 0, 0.0f},
        // THE CROWN CLOSES. The shell used to stop at the 36 mm ring above —
        // a flat lid, which is what made the helmet read as a pot with a top
        // on it from any camera above head height (a ledge, or a man on a
        // stair). 7.5 mm further up on the same centre line it shrinks to a
        // 10 mm button, so the last band is the top of a dome.
        {HP(0, 0.1635f, -0.017f), kt.helm, kt.helm, 0.0110f, 0.0125f, 0, 0.0f}};
      probe("helm_shell");
      // ROUND and smooth-shaded at the head's tier (20 up close). It was
      // faceted k=8 on purpose (a "designed low-poly object"), and that read
      // as a flat-topped bucket of planks; a hard shell is moulded, and the
      // stations above still taper it as a helmet, not as a ball.
      fig_chain(hm, 10, 20, 1, 1, hr);
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
      // THE HELMET'S HARDWARE: the side rails (ARC rails) running fore-aft on
      // the shell above the ear covers, following its curve 4 mm proud, and the
      // COUNTERWEIGHT POUCH on the back that balances the NVG mount, with a
      // velcro patch on it. Operators only — an insurgent's helmet is bare.
      if (!ins && g_fig_k > 8) {
        v3 rl = v3_scale(kt.web, 0.80f);
        for (int e = 0; e < 2; e++) {
          float es = e ? 1.0f : -1.0f;
          fnode_t sr[4];
          const float zs[4] = {-0.064f, -0.026f, 0.014f, 0.048f};
          for (int q = 0; q < 4; q++) {
            float zz = (zs[q] + 0.0115f) / 0.117f;
            // ROOTED, not laid on: centred ON the shell with 6 mm half-thickness,
            // so its inner face is 6 mm inside the shell and its outer 6 mm proud —
            // two surfaces that cross, never two running parallel a hair apart.
            float xx = 0.1025f * sqrtf(fmaxf(0.0f, 1.0f - zz * zz));
            sr[q] = (fnode_t){HP(es * xx, 0.044f, zs[q]), rl, rl, 0.0060f, 0.0060f, 0, 0.0f};
          }
          probe("helm_rail");
          fig_chain(sr, 4, 8, 1, 1, hu);
        }
        v3 cw = v3_scale(kt.web, 0.92f);
        fnode_t cwp[3] = {
          {HP(0, 0.034f, -0.118f), cw, cw, 0.036f, 0.022f, 0, 0.55f},
          {HP(0, 0.034f, -0.134f), cw, cw, 0.040f, 0.025f, 0, 0.0f},
          {HP(0, 0.034f, -0.146f), cw, cw, 0.034f, 0.020f, 0, 0.0f}};
        probe("counterweight");
        g_fig_soft = 1;
        fig_chain(cwp, 3, 12, 1, 1, hr);
        g_fig_soft = 0;
        v3 patch = v3_scale(kt.helm, 0.70f);
        fnode_t ptc[2] = {
          {HP(0, 0.034f, -0.1450f), patch, patch, 0.018f, 0.011f, 0, 0.0f},
          {HP(0, 0.034f, -0.1495f), patch, patch, 0.018f, 0.011f, 0, 0.0f}};
        fig_chain(ptc, 2, 8, 1, 1, hr);
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
        fig_chain(cp, 6, 20, 1, 1, hr);
        // The bill: a thin flat plate out of the band's front, rooted 20 mm
        // inside the crown and pitched a few degrees down, as a worn bill is.
        v3 bill = v3_scale(kt.helm, 0.78f);
        // v5: 6 mm higher, so glasses under it (respirator, shemagh) clear it.
        fnode_t bl[4] = {
          {HP(0, 0.032f, 0.070f), bill, bill, 0.060f, 0.0070f, 0, 0.0f},
          {HP(0, 0.030f, 0.104f), bill, bill, 0.068f, 0.0065f, 0, 0.0f},
          {HP(0, 0.024f, 0.142f), bill, bill, 0.064f, 0.0060f, 0, 0.0f},
          {HP(0, 0.018f, 0.166f), bill, bill, 0.046f, 0.0055f, 0, 0.0f}};
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
      if (head != HEAD_CAP && !hero && (!ownlens || fg == FACE_GOGGLES)) {
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
      if (head == HEAD_HELMET && !hero) {
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
      // IR STROBE: a small cyan beacon on the back of the shell — the friendly
      // marker real squads wear, in the HUD's SIGNAL colour, lit (an albedo over
      // 1 is the goggles' own emissive trick). On the BACK, so it is what a
      // squadmate following him sees, and a hostile facing him does not.
      // Rooted 22 mm inside the shell at its rear ring, standing 16 mm out.
      if (!ins) {
        // v5: an UNLIT strobe — a matte housing with a dark lens. It was a
        // cyan lamp (albedo 1.5-1.8) on the back of every operator's head.
        v3 ir = {{0.130f, 0.140f, 0.150f}};
        v3 irb = v3_scale(kt.web, 0.85f);
        fnode_t sb[3] = {
          {HP(0, 0.098f, -0.088f), irb, irb, 0.016f, 0.012f, 0, 0.0f},
          {HP(0, 0.104f, -0.120f), irb, ir, 0.016f, 0.012f, 0, 0.0f},
          {HP(0, 0.106f, -0.130f), ir, ir, 0.012f, 0.009f, 0, 0.0f}};
        probe("irstrobe");
        fig_chain(sb, 3, 8, 1, 1, hr);
      }
      }   // head == HEAD_HELMET: chin strap and NVG shroud
    }
    // THE LENS GLINT (v5): the smoked lens has no light of its own, so it gets
    // one pinpoint — a 9 x 4 mm highlight high on the left lens,
    // lifted from the lens's own hue toward cool grey (saturation <= 0.2). A
    // lens that only gleams when the sun is behind the camera reads as a hole.
    if (!ins && head != HEAD_CAP && !ownlens && g_fig_k >= 12 && !g_fig_sil) {
      float gmx = fmaxf(kt.gm2.x, fmaxf(kt.gm2.y, fmaxf(kt.gm2.z, 1e-3f)));
      v3 gl = v3_lerp((v3){{0.60f, 0.63f, 0.68f}}, v3_scale(kt.gm2, 0.66f / gmx), 0.25f);
      // the outer surface at x = -0.030: hero lens (skull + 13.4 mm) or the
      // procedural goggle's face (0.104 forward at the centre, 24 mm deep)
      float gx = -0.030f, gy = hero ? 0.026f : 0.030f, gz;
      if (hero) { float rx = 0.0790f + 0.0134f, rz = 0.0975f + 0.0134f;
                  gz = -0.010f + rz * sqrtf(1.0f - (gx / rx) * (gx / rx)); }
      else gz = 0.080f + 0.024f - 0.010f;
      mat_set(MAT_GLASS_R, 0.0f);
      fnode_t gn[2] = {{HP(gx, gy, gz - 0.0012f), gl, gl, 0.0045f, 0.0020f, 0, 0.0f},
                       {HP(gx, gy, gz + 0.0010f), gl, gl, 0.0045f, 0.0020f, 0, 0.0f}};
      probe("lensglint");
      fig_chain(gn, 2, 8, 1, 1, hr);
    }
    if (hero) {
      // slot 11 is the face cloth: a CLEAR lens is the balaclava behind it
      v3 slots[4] = {kt.helm, kt.web, kt.gm2, kt.bala};
      g_md_slots = slots;
      #define HERO(NAME, TAG) do { probe(NAME); \
        model_draw(TAG##_V, TAG##_P, TAG##_NP, 0, P.head, hr, hu, hf); } while (0)
      int mouth = fg == FACE_GAS_TWIN || fg == FACE_GAS_PANO || fg == FACE_PLATE || fg == FACE_RESP;
      if (head == HEAD_HELMET) {
        HERO("hero_helmet", HERO_HELMET);
        // v5: the headset's boom mic and the chin strap, each its own sculpt:
        // a mask over the mouth drops both (its chin cup is where they were).
        if (!mouth) { HERO("hero_chin", HERO_CHIN); HERO("hero_boom", HERO_BOOM); }
        if (nvg == 1) HERO("hero_nvg", HERO_NVGU);
        if (nvg == 2) HERO("hero_nvg", HERO_NVGD);
        if (fg == FACE_GOGGLES) HERO("hero_gstrap", HERO_GSTRAP);
      }
      if (head != HEAD_CAP && !ownlens) HERO("hero_visor", HERO_VISOR);
      if (skullface) HERO("hero_skull", HERO_SKULL);
      if (fg == FACE_GAS_TWIN) HERO("hero_mask", HERO_GAS2);
      if (fg == FACE_GAS_PANO) HERO("hero_mask", HERO_GAS1);
      if (fg == FACE_RESP)     HERO("hero_mask", HERO_RESP);
      if (fg == FACE_GOGGLES)  HERO("hero_mask", HERO_GOG);
      if (fg == FACE_PLATE)    HERO("hero_mask", HERO_PLATE);
      if (fg == FACE_SHEMAGH)  HERO("hero_mask", HERO_SHEM);
      #undef HERO
      g_md_slots = NULL;
    } else if (!ins && g_fig_k >= 8) {
      // THE FAR MASK (v5): what makes each mask's OUTLINE, as chains, for the
      // procedural head past 12 m (its colours are on the skull rings above).
      #define FAR_CAN(C, DIR, R, HL, COL) do { fnode_t cn_[2] = {           \
        {v3_sub((C), v3_scale((DIR), (HL))), COL, COL, (R), (R), 0, 0.0f},     \
        {v3_add((C), v3_scale((DIR), (HL))), COL, COL, (R), (R), 0, 0.0f}};    \
        fig_chain(cn_, 2, 12, 1, 1, hu); } while (0)
      v3 fc = v3_scale(kt.helm, 0.90f);
      mat_set(MAT_GEAR_R, 0.0f);
      probe("mask");
      if (fg == FACE_GAS_TWIN)
        for (int e = 0; e < 2; e++) {
          float es = e ? 1.0f : -1.0f;
          v3 dir = v3_norm(v3_add(v3_scale(hr, es * 0.867f), v3_scale(hf, 0.497f)));
          FAR_CAN(HP(es * 0.066f, -0.050f, 0.063f), dir, 0.030f, 0.014f, fc);
        }
      if (fg == FACE_GAS_PANO) {
        v3 dir = v3_norm(v3_add(v3_scale(hu, -0.523f), v3_scale(hf, 0.852f)));
        FAR_CAN(HP(0, -0.068f, 0.120f), dir, 0.033f, 0.018f, fc);
      }
      if (fg == FACE_RESP)
        for (int e = 0; e < 2; e++) {
          float es = e ? 1.0f : -1.0f;
          v3 dir = v3_norm(v3_add(v3_scale(hr, es * 0.717f), v3_scale(hf, 0.697f)));
          FAR_CAN(HP(es * 0.050f, -0.054f, 0.084f), dir, 0.023f, 0.005f, fc);
        }
      if (head == HEAD_HELMET && nvg) {
        v3 nc = v3_scale(kt.web, 0.95f);
        fnode_t nb[2] = {
          {nvg == 2 ? HP(0, 0.014f, 0.125f) : HP(0, 0.098f, 0.135f), nc, nc, 0.050f, 0.014f, 0, 0.0f},
          {nvg == 2 ? HP(0, 0.014f, 0.200f) : HP(0, 0.157f, 0.124f), nc, nc, 0.046f, 0.012f, 0, 0.0f}};
        probe("nvg");
        fig_chain(nb, 2, 8, 1, 1, hr);
      }
      #undef FAR_CAN
    }
    g_fig_k = kbody;   // the head's extra tier ends with the head
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
    fnode_t th[10];
    int nth = 0;
    // The thigh reaches BACKWARD ALONG ITS OWN AXIS into the pelvis: the end
    // cap ends up buried whatever the leg does, and because the stub is
    // collinear with the thigh the hip contributes no bend either.
    // k=8 faceted with real quad mass: the legs join the crate language.
    // THE SEAT OVER THE HIP JOINT. The thigh is a straight tube whose top end
    // is buried in the pelvis — but swing the leg back in a stride and that
    // top edge rose out through the back of the seat ring: from behind, the
    // legs read as sticks punched through the buttocks. A buttock-shaped
    // ellipsoid behind each hip joint (the knee's trick: a ball that swallows
    // the tube end at every angle) turns half-way with the thigh, so the
    // junction is always skin, never a tube end.
    // (close tiers only: past ~12 m the junction is a few pixels, and a
    // 6-8-gon ellipsoid against an 8-gon tube is all shallow facet crossings.)
    // THE SEAT PANEL. Trousers are not a tube with two tubes stuck under it:
    // each leg is cut as one panel that runs from the waistband over the seat
    // and the hip down into the thigh, and the two panels meet in a seam down
    // the middle. So each leg now carries that panel — a short chain that
    // starts buried in the torso under the belt, swells over the seat and the
    // hip joint (wider than the thigh's own top ring, so the thigh's tube end
    // can never show at the hip, standing or in a stride), and dives into the
    // thigh a sixth of the way down. The two panels overlap past the midline,
    // and where they cross is the seat seam. It replaces the two buttock balls
    // that used to stand out of the trouser under the belt.
    {
      v3 pfw_ = v3_norm(v3_cross(spine, prt));
      float s_ = i ? 1.0f : -1.0f;
      v3 pv = P.pelvis;
      // (builder.c's FIG_SEAT_ST is this list at rest: keep the two in step)
      #define SEAT_P(T, L, F) v3_add(pv, v3_add(v3_scale(spine, (T)), \
                              v3_add(v3_scale(prt, s_ * (L)), v3_scale(pfw_, (F)))))
      v3 hipn = v3_add(P.hip[i], v3_scale(pfw_, -0.012f));
      // the thigh's own axis decides where the panel ends, so a stride carries
      // the panel with the leg and the seat stretches over the hip
      v3 low = v3_add(v3_add(P.hip[i], v3_scale(thv, 0.20f)), v3_scale(pfw_, -0.006f));   // above the leg strap (0.28)
      // As the thigh comes FORWARD (a crouch, a kneel, the front leg of a
      // stride) its buried top end swings out BEHIND the hip, and the seat
      // must follow it there: the seat node slides back over the hip joint,
      // which is also what a seat does when a man squats.
      // (from 0.62, not 0: a running stride brings the front thigh up to ~0.6
      // forward, and a seat that half-grew there lay almost ON the panel and
      // the two tore through each other in patches)
      // Gated by the CROUCH (or the slide), not by the thigh angle alone: a
      // running stride drives the front thigh just as far forward (0.94 here
      // against 0.90-0.98 crouched), and a seat that grew under a runner lay
      // on the panel and tore through it in patches.
      float squat = fmaxf(A->crouch_s, A->slide_s);
      float fl = smooth01(f_clamp((v3_dot(td, pfw_) - 0.62f) / 0.28f, 0.0f, 1.0f)) *
                 smooth01(f_clamp((squat - 0.25f) / 0.55f, 0.0f, 1.0f));
      v3 seat_up = SEAT_P(-0.020f, 0.072f, -0.016f);
      v3 seat_bk = v3_add(v3_add(P.hip[i], v3_scale(td, -0.060f)), v3_scale(spine, 0.020f));
      v3 seat_mid = v3_lerp(seat_up, seat_bk, fl);
      // A node a tenth of the way down the thigh shares the turn from the
      // seat into the leg between two joints. With the leg swung behind, that
      // turn is ~40 degrees, and taken at one node fig_chain's bevel clamp
      // thinned the ring there by half: the panel sank, and its neighbour and
      // the thigh tore through it in patches across the seat.
      v3 thmid = v3_add(v3_add(P.hip[i], v3_scale(thv, 0.10f)), v3_scale(pfw_, -0.010f));
      // EACH NODE SITS BEHIND ITS JOINT. The seat is all BEHIND the hip: the
      // back of these rings is where the seat is, and the front must stay inside
      // the thigh and the belly. Centred on the joints they stood as far out in
      // front as behind, and the two legs' fronts met between the thighs in a
      // rounded mound — the egg under the belt. Shifted back by what they lost
      // in depth, the seat is exactly where it was and the front is 2 cm inside
      // (any shallower and the two seats' backs meet at a graze under the belt).
      fnode_t sp[5] = {
        {SEAT_P(0.040f, 0.048f, -0.013f), kt.pants, kt.pants, 0.088f, 0.087f, 0, 0.0f},   // well inside the belt
        {v3_add(seat_mid, v3_scale(pfw_, -0.004f)), kt.pants, kt.pants, 0.112f, 0.100f, 0, 0.0f},  // over the seat
        {v3_add(hipn, v3_scale(pfw_, -0.004f)), kt.pants, kt.pants, 0.101f, 0.102f, 0, 0.0f},      // behind the hip joint
        {thmid, kt.pants, kt.pants, 0.097f, 0.116f, 0, 0.0f},                             // a tenth down the leg
        {low, kt.pants, kt.pants, 0.064f, 0.074f, 0, 0.0f}};                              // into the thigh
      #undef SEAT_P
      probe("seat");
      g_fig_jit = 0.0f;   // one cloth with the thigh: no band jitter, no darkened ends
      if (!kt.ins) fig_chain(sp, 5, 16, 1, 1, prt);   // the insurgent's tunic is his seat
      // THE SEAT HAS NO CLEFT (0.8.7). The two panels are ellipses 72 mm off
      // the midline, so where they meet their backs had fallen 27 mm short of
      // their crowns: from behind, two round cheeks with a groove down the
      // middle — in light trousers (SAHARA, ARCTIC) with the sun behind the
      // camera, a cartoon backside. One broad mass across the seat, its back
      // flush with the panels' (140 mm behind the pelvis), fills the groove, so
      // the trouser seat reads as one surface. Short along the spine (its
      // bottom 75 mm under the pelvis) so a stride never shows it between the
      // legs, and drawn once, with the second leg.
      if (!kt.ins && i == 1) {
        // In a squat the seat rolls back over the hips: the mass follows it
        // (fl, this leg's squat flexion), so it never shows as a flap between
        // the two rounded seat masses the crouch grows below.
        v3 sc_ = v3_add(pv, v3_add(v3_scale(spine, -0.030f - 0.020f * fl), v3_scale(pfw_, -0.036f - 0.040f * fl)));
        probe("seat");
        fig_ball(sc_, 0.150f, 0.070f + 0.020f * fl, 0.086f + 0.020f * fl, spine, prt, 16, kt.pants);
      }
      // ...and the ROUND OF THE SEAT in a squat. Thighs forward, the leg tubes
      // are seen end-on from behind — two flat discs where a seat should be —
      // and no panel swept down from the belt can close a disc that faces
      // backwards. A seat in a crouch IS round, so with the flexion a rounded
      // mass grows over the back of each hip joint, wider and deeper than the
      // thigh so it swallows the tube end; standing (fl = 0) it is gone.
      if (fl > 0.02f) {
        // centred 2 cm in from the hip joint and wider, so the two overlap well
        // past the midline: one seat with a soft seam, not two balls
        // (0.8.7: pulled in to 5 cm off the midline and a little shallower —
        // 2.2 cm in left two 12 cm balls side by side, a cartoon backside in
        // light trousers; now the two masses merge into one seat)
        v3 bc = v3_add(v3_add(P.hip[i], v3_scale(td, -0.040f * fl)),
                       v3_add(v3_scale(spine, 0.006f), v3_scale(prt, -s_ * 0.050f * fl)));
        probe("seat");
        fig_ball(bc, 0.076f + 0.040f * fl, 0.050f + 0.055f * fl, 0.080f + 0.030f * fl, td, prt, 16, kt.pants);
      }
      g_fig_jit = 1.0f;
    }
    // The buried top end points UP INTO THE PELVIS, not back along the thigh:
    // with the thigh driven forward (a running stride, a crouch) "back along
    // the thigh" is straight out behind the hip, and its end showed as a white
    // disc under the seat. Standing it is the same point it always was.
    {
      v3 sd_ = v3_add(v3_scale(td, -1.0f), v3_scale(spine, 1.2f));
      sd_ = v3_dot(sd_, sd_) > 1e-6f ? v3_norm(sd_) : spine;
      th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(sd_, 0.090f)),
                            kt.pants, kt.pants, 0.046f, 0.050f, 0, 0.0f};
    }
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
    if (i == 1 && !g_fig_hos) {   // (no drop-leg rig on a hostage)
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
    int th_bel = nth;   // the belly ring: the drop-leg rig is seated off it
    th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(thv, 0.42f)),
                          kt.pants, kt.pants, 0.0855f, 0.1017f, 0, 0.0f};
    // v5: the taper to the knee is a CURVE — the vastus holds its width to two
    // thirds down and then the leg narrows into the joint. Straight from 0.42
    // to the knee it was a cone. Then the knee pad's upper STRAP, two fat nodes
    // 4 mm proud (one surface: it cannot cross the trouser it is part of),
    // operators only.
    th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(thv, 0.68f)),
                          kt.pants, kt.pants, 0.0780f, 0.0900f, 0, 0.0f};
    if (!kt.ins && g_fig_k > 8) {   // under a pixel past 12 m
      v3 kst = v3_scale(kt.web, 1.05f);
      th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(thv, 0.80f)),
                            kt.pants, kt.pants, 0.0742f, 0.0840f, 0, 0.0f};
      th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(thv, 0.84f)),
                            kst, kst, 0.0760f, 0.0850f, 0, 0.0f};
      th[nth++] = (fnode_t){v3_add(P.hip[i], v3_scale(thv, 0.88f)),
                            kt.pants, kt.pants, 0.0712f, 0.0790f, 0, 0.0f};
    }
    th[nth++] = (fnode_t){P.knee[i], kt.pants, kt.pants, 0.066f, 0.072f, 0, 0.0f};
    probe("thigh");
    g_fig_jit = 0.40f;   // same cap as the torso: cloth, not crates
    fig_chain(th, nth, 16, 1, 1, prt);   // round, smooth: hip -> knee taper
    g_fig_jit = 1.0f;
    // v5: the pad's lower strap under the knee (operators), a calf that is
    // DEEPER than it is wide and holds to mid-shin before it falls away, and an
    // ankle 4 mm slimmer — the old shank tapered in one line from the calf.
    v3 kst2 = v3_scale(kt.web, 1.05f);
    int op_ = !kt.ins && g_fig_k > 8;   // the strap: under a pixel past 12 m
    fnode_t sh[7] = {
      {P.knee[i], kt.pants, kt.pants, 0.066f, 0.072f, 0, 0.0f},
      {v3_add(P.knee[i], v3_scale(shv, 0.10f)), kt.pants, op_ ? kst2 : kt.pants,
       0.0720f, 0.0780f, 0, 0.0f},
      {v3_add(P.knee[i], v3_scale(shv, 0.15f)), op_ ? kst2 : kt.pants, kt.pants,
       op_ ? 0.0770f : 0.0740f, op_ ? 0.0815f : 0.0790f, 0, 0.0f},
      // Calf belly UP (0.076 -> 0.081): a shank with no calf is a pipe.
      // ...and the insurgent's trouser is LOOSE: a fuller calf that gathers at
      // the ankle (shalwar cut), where the operator's is a fitted combat pant.
      {v3_add(P.knee[i], v3_scale(shv, kt.ins ? 0.40f : 0.32f)), kt.pants, kt.pants,
       kt.ins ? 0.092f : 0.079f, kt.ins ? 0.096f : 0.088f, 0, 0.0f},
      {v3_add(P.knee[i], v3_scale(shv, 0.58f)), kt.pants, kt.pants,
       kt.ins ? 0.080f : 0.064f, kt.ins ? 0.084f : 0.069f, 0, 0.0f},
      {P.ankle[i], kt.dust1, kt.dust1, 0.044f, 0.047f, 0, 0.0f},          // ankle
      // ...and the shin keeps going 3 cm PAST the ankle, into the boot: a cap
      // sitting exactly at the ankle pokes out of the collar the moment the
      // foot pitches, which reads as a boot that came off.
      {v3_add(P.ankle[i], v3_scale(sd, 0.032f)), kt.dust1, kt.dust1, 0.035f, 0.037f, 0, 0.0f}};
    probe("shin");
    g_fig_jit = 0.40f;
    fig_chain(sh, 7, 16, 1, 1, prt);   // calf belly -> ankle, round
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
      // v5: the joint is the TROUSER's knee (a shade down: the crease), and the
      // PAD is its own moulded shell over the front — a dark rubber sphere was
      // the visible ball the review called out.
      fig_joint(P.knee[i], td, sd, 0.079f, 0.092f, 0.12f, 16,
                kt.ins ? kt.pad : v3_scale(kt.pants, 0.90f));
      if (!kt.ins && g_fig_k > 8) {   // the pad: close tiers (inside 12 m)
        // Outside of the bend; a straight leg points the knee down the
        // pelvis's forward. Rooted 8 mm into the ball (its forward radius is
        // 79 mm straight and swells with flex), 28 mm proud of it.
        v3 bis = v3_add(td, sd);
        bis = v3_dot(bis, bis) > 1e-6f ? v3_norm(bis) : td;
        v3 out = v3_sub(td, sd);
        if (v3_dot(out, out) < 4e-3f) out = v3_norm(v3_cross(spine, prt));
        out = v3_sub(out, v3_scale(bis, v3_dot(out, bis)));
        out = v3_dot(out, out) > 1e-8f ? v3_norm(out) : v3_norm(v3_cross(spine, prt));
        v3 lat = v3_norm(v3_cross(bis, out));
        float bend = f_clamp(1.0f - v3_dot(td, sd), 0.0f, 1.0f);
        v3 kb = v3_add(P.knee[i], v3_scale(out, 0.079f * (1.0f + 0.12f * bend) + 0.22f * 0.079f * bend + 0.006f));
        v3 kp = v3_scale(kt.pad, 1.08f), kpe = v3_scale(kt.pad, 0.86f);
        fnode_t kn[4] = {
          {v3_add(kb, v3_scale(bis, -0.050f)), kpe, kpe, 0.030f, 0.010f, 0, 0.0f},
          {v3_add(kb, v3_scale(bis, -0.026f)), kp, kp, 0.040f, 0.014f, 0, 0.0f},
          {v3_add(kb, v3_scale(bis, 0.030f)), kp, kp, 0.040f, 0.014f, 0, 0.0f},
          {v3_add(kb, v3_scale(bis, 0.054f)), kpe, kpe, 0.030f, 0.010f, 0, 0.0f}};
        probe("kneepad");
        g_fig_soft = 1;
        fig_chain(kn, 4, 16, 1, 1, lat);
        g_fig_soft = 0;
      }
    }
    if ((i == 1 || fig_dual) && !g_fig_hos) {  // Drop-leg rig on the strong-side thigh (both, for the Berettas). A loaded soldier's
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
      v3 out = v3_add(v3_scale(prt, i ? 0.97f : -0.97f), v3_scale(pfw, 0.24f));  // ~14 deg
      out = v3_sub(out, v3_scale(td, v3_dot(out, td)));
      out = v3_dot(out, out) > 1e-6f ? v3_norm(out) : v3_scale(prt, i ? 1.0f : -1.0f);
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
      float rbel = fig_rmax(8, th[th_bel].rx, th[th_bel].rx);
      // Half-depth 40 mm at the belly: the outer face then stands >= 40 mm proud
      // of the leg in every direction, and the inner face is >= 25 mm buried in
      // it — the circumradius is ~15 mm over the inradius on this profile and
      // that difference is the whole margin budget. rx is the OUTWARD half-depth
      // and rz the half-width around the leg, because the hint below is `out`
      // and fig_ring lays rx along the hint.
      (void)rhip; (void)rbel;
      // THE HOLSTER is a Blender mesh (tools/props_models.py `holster`,
      // FIG_HOLSTER): a molded duty shell deep over the trigger guard that
      // steps to a slim sheath over the slide, a raised mouth lip, a rotating
      // hood on pivot hubs over the back of the slide, a thigh platform with
      // strap slots, screwed to the shell. Authored round the standard
      // pistol's box in the pistol's own frame and STRETCHED per axis to the
      // box of the pistol actually carried, so a long or deep variant (a
      // Deagle, a light under the barrel) still closes inside it. The mouth
      // stops ahead of the grip; only the grip shows, leaning forward under
      // the hand. Hand-placed rings could not make the crisp step, lip and
      // hood a molded holster is read by — they made a pouch, then a box.
      // Drawn whether or not the pistol is in it.
      {
        gframe_t hf = holster_frame_leg(&P, spine, i);
        v3 bmn, bmx;
        float yfr, ylo[HOL_NS], zst[HOL_NS];
        pt_holster_box(g_fig_ent, &bmn, &bmx, &yfr, ylo, zst);
        (void)yfr; (void)zst;
        float in_ = v3_dot(hf.rt, out) > 0.0f ? -1.0f : 1.0f;   // rt sign toward the leg
        // the standard pistol the mesh was cut round
        const float B_HX = 0.020f, B_YT = 0.036f, B_YG = -0.055f, B_ZR = -0.054f, B_ZM = 0.153f;
        float ytg = fminf(ylo[0], ylo[1]);                       // the trigger guard's depth
        float sx = fmaxf(fmaxf(bmx.x, -bmn.x) / B_HX, 1.0f);
        float sy = f_clamp((bmx.y - ytg) / (B_YT - B_YG), 0.85f, 1.6f);
        float sz = f_clamp((bmx.z - bmn.z) / (B_ZM - B_ZR), 0.85f, 1.6f);
        float xc = 0.5f * (bmn.x + bmx.x);
        v3 ax = v3_scale(hf.rt, sx * in_), ay = v3_scale(hf.up, sy), az = v3_scale(hf.fw, sz);
        v3 org = v3_add(hf.o, v3_add(v3_scale(hf.rt, xc),
                 v3_add(v3_scale(hf.up, bmx.y - B_YT * sy), v3_scale(hf.fw, bmn.z - B_ZR * sz))));
        v3 slots[4] = {kt.web, kt.belt, kt.belt, kt.belt};
        const v3 *s0 = g_md_slots;
        g_md_slots = slots;
        probe("dropleg");
        model_draw(FIG_HOLSTER_V, FIG_HOLSTER_P, FIG_HOLSTER_NP, 0, org, ax, ay, az);
        g_md_slots = s0;
        // the HANGER: a strap from the top of the platform up to the belt
        #define HM_(X, Y, Z) v3_add(org, v3_add(v3_scale(ax, (X)), v3_add(v3_scale(ay, (Y)), v3_scale(az, (Z)))))
        v3 htop = HM_(0.036f, 0.010f, -0.088f);
        #undef HM_
        v3 bt = v3_add(P.pelvis, v3_add(v3_scale(spine, 0.040f), v3_scale(out, 0.150f)));
        fnode_t hg[3] = {
          {v3_add(htop, v3_scale(td, 0.014f)), kt.web, kt.web, 0.006f, 0.015f, 0, 0.0f},
          {v3_lerp(htop, bt, 0.55f), kt.web, kt.web, 0.006f, 0.015f, 0, 0.0f},
          {bt, kt.web, kt.web, 0.006f, 0.015f, 0, 0.0f}};
        probe("legstrap");
        fig_chain(hg, 3, 6, 1, 1, out);
      }
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
          fig_chain(sb, 2, 16, 1, 1, prt);   // the thigh's own profile, 4% out
        }
      }
      // ...and the pistol in it, when it is not in his hand.
      if (holster_pt) {
        gframe_t hf = holster_frame_leg(&P, spine, i);
        probe("holster_gun");
        float gg = g_gun_girth;
        g_gun_girth = GUN_GIRTH_3P;
        {
          int m0 = g_scene_floats;
          gun_build(WPN_PT, hf.o, hf.rt, hf.up, hf.fw, 0.0f, 0.0f, 0.0f);
          const mdl_ref_t *tm = gun_tp_model(g_fig_ent, WPN_PT);
          if (tm) { g_scene_floats = m0; gun_tp_draw(tm, WPN_PT, hf.o, hf.rt, hf.up, hf.fw, 0.0f); }
        }
        g_gun_girth = gg;
        mat_set(MAT_GEAR_R, 0.0f);
      }
    }
    mat_set(MAT_LEATHER_R, 0.0f);   // boot + cuff
    // Boot: swept along the actual foot axis (which carries the toe pitch), so
    // the sole stays flat on the ground and lifts with the step.
    v3 fF = v3_sub(P.toe[i], P.ankle[i]);
    // (a hostage kneeling has his feet instep-down behind him: g_fig_bup)
    v3 bsp = g_fig_hos && v3_dot(g_fig_bup[i], g_fig_bup[i]) > 0.5f ? g_fig_bup[i] : spine;
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
      v3 av = v3_add(P.ankle[i], v3_scale(bsp, -0.028f - sole_dip));
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
        v3_add(v3_add(av, v3_scale(fF, (T))), v3_scale(bsp, (RX) - (H))), \
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
      // v5: 0.70 — the slab is the darkest band on the figure.
      v3 sole_c = v3_scale(kt.boot, 0.70f);
      fnode_t so[7] = {
        SOLEN(SOLE_H, -0.086f, 0.0168f, 0.0443f, sole_c, sole_c),  // heel, squared
        SOLEN(SOLE_H, -0.050f, 0.0175f, 0.0484f, sole_c, sole_c),  // heel block
        SOLEN(SOLE_H, -0.026f, 0.0120f, 0.0458f, sole_c, sole_c),  // heel breast
        SOLEN(SOLE_H, 0.030f, 0.0134f, 0.0484f, sole_c, sole_c),  // waist
        SOLEN(SOLE_H, 0.090f, 0.0145f, 0.0562f, sole_c, sole_c),  // ball, widest
        SOLEN(SOLE_H, 0.152f, 0.0132f, 0.0525f, sole_c, sole_c),  // toe box
        SOLEN(SOLE_H, 0.194f, 0.0116f, 0.0324f, sole_c, sole_c)}; // toe end
      probe("outsole");
        // Half jitter. The faceted family's per-band value variation is what stops
      // a slab reading as an extrusion, so it does not go to zero — but with the
      // welt's cue gone it would otherwise be most of the sole's internal
      // contrast, and its edge-darkening term puts 0.29 stops on the band next
      // to the heel cap, which is one of the few end caps on this figure the
      // player actually sees.
      g_fig_jit = 0.5f;
      fig_chain(so, 7, 6, 1, 1, bsp);
      g_fig_jit = 1.0f;
      // v5: ROUGH-OUT suede, the modern combat boot, at the cloth roughness.
      // As 0.50 leather its up-facing vamp caught the armory's fill and the sky
      // reflection and read as a pale grey block on a dark boot.
      mat_set(MAT_CLOTH_R, 0.0f);
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
      // v5: 1.16 and a tenth of the ground, not 1.40 and a fifth. Lifted
      // that far, the leather on a light floor came out PALE — the "small pale
      // blocks" of the review — and the boot stopped being the ramp's floor.
      // The toe cap and heel counter go a step DARKER than the vamp (scuffed,
      // polished leather), so the boot has a front and a back.
      v3 lea_c = v3_scale(kt.boot, 1.16f);
      v3 up_c = v3_lerp(lea_c, g_theme.ground, 0.10f);
      lea_c = v3_scale(lea_c, 0.86f);
      fnode_t bt[7] = {
        SOLEN(UPPR_H, -0.074f, 0.0290f, 0.0380f, lea_c, lea_c),      // counter
        SOLEN(UPPR_H, -0.040f, 0.0375f, 0.0436f, lea_c, lea_c),      // counter top
        SOLEN(UPPR_H, 0.012f, 0.0400f, 0.0434f, up_c,   up_c),       // throat
        SOLEN(UPPR_H, 0.068f, 0.0340f, 0.0488f, up_c,   up_c),       // instep
        SOLEN(UPPR_H, 0.124f, 0.0268f, 0.0499f, up_c,   up_c),       // vamp
        SOLEN(UPPR_H, 0.172f, 0.0206f, 0.0400f, lea_c, lea_c),      // toe cap
        // Toe tip in to 0.0262: the sole was 1.0 mm wider here, which is
        // inside the same coin flip the floors were in.
        SOLEN(UPPR_H, 0.194f, 0.0132f, 0.0283f, lea_c, lea_c)};     // toe tip
      probe("boot");
      // Soft box: the leather keeps its flat floor (buried in the outsole) and
      // flat sides, with rolled edges where the octagon had arrises.
      g_fig_soft = 1;
      fig_chain(bt, 7, 16, 1, 1, bsp);  // sole down: rx is the boot height
      g_fig_soft = 0;
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
      fig_chain(cf, 5, 16, 1, 1, prt);
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
      // v5: the knife's swing brings the off hand up into the guard too (gs.kg,
      // third person only), so a cut is never thrown with the other arm dead.
      P.wri[0] = v3_lerp(lo_, hi_, wpn == WPN_KN ? fmaxf(gs.guard, gs.kg) : gs.guard);
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
    if (wpn == WPN_PT && g_fig_dual) {
      // The left hand on the second Beretta's grip, wrist placed exactly as the
      // right hand's is on the first.
      gun_hold(wpn, GH_GRIP, v3_sub(g_org, v3_scale(g_rt, DUAL3P * GUN_GIRTH_3P)),
               g_rt, g_up, g_fw, &gc, &ga, &gr);
      br = v3_norm(v3_sub(v3_scale(g_fw, -1.0f), v3_scale(ga, -v3_dot(g_fw, ga))));
      P.wri[0] = v3_add(gc, v3_add(v3_scale(ga, -0.046f * GUN_GIRTH_3P), v3_scale(br, gr + 0.066f)));
    } else if (wpn == WPN_PT) {
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
  // THE TRIGGER WRIST GOES BEHIND THE GRIP, NOT UNDER IT. The pose solves
  // the rifle hold with the wrist 75 mm under the bore and 100 mm back, which
  // is the BOTTOM of the pistol grip: the forearm then ran straight up the
  // grip's own axis (measured: forearm . grip axis = 0.93) and fig_hand,
  // wrapping the fingers round from the wrist, folded the hand 82-92 degrees
  // forward over the receiver — a hand bent down at the wrist and sunk into
  // the stock. A real hand sits BEHIND the grip with the forearm running back
  // along the stock, so the wrist is placed exactly as the pistol's is: off the
  // grip cylinder, against the bore, and the elbow re-solved on the side the
  // pose already put it.
  if (living_pose && !g_fig_hos && (wpn == WPN_AR || wpn == WPN_SR)) {
    v3 gc1, ga1; float gr1;
    gun_hold(wpn, GH_GRIP, g_org, g_rt, g_up, g_fw, &gc1, &ga1, &gr1);
    v3 br1 = v3_sub(v3_scale(g_fw, -1.0f), v3_scale(ga1, -v3_dot(g_fw, ga1)));
    br1 = v3_norm(br1);
    P.wri[1] = v3_add(gc1, v3_add(v3_scale(ga1, -0.046f * GUN_GIRTH_3P), v3_scale(br1, gr1 + 0.066f)));
    v3 aw = v3_sub(P.wri[1], P.sho[1]);
    float awl = v3_len(aw), amax = (ANIM_UARM + ANIM_FARM) * 0.96f;
    if (awl > amax) P.wri[1] = v3_add(P.sho[1], v3_scale(aw, amax / awl));
    v3 pl = v3_sub(P.elb[1], v3_lerp(P.sho[1], P.wri[1], 0.5f));
    if (v3_dot(pl, pl) < 1e-8f) pl = v3_add(v3_scale(Rc, 0.7f), v3_scale(spine, -0.8f));
    P.elb[1] = ik_solve(P.sho[1], P.wri[1], ANIM_UARM, ANIM_FARM, v3_norm(pl));
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
    // Fly the weapon cleanly between its carry slot and ready hold with smooth
    // quintic easing, tactile grasp/seat beats, natural bent elbows (<= 90% arm
    // reach so neither arm ever locks straight), and continuous hand-to-grip
    // orientation blending (no 0.5 binary wrist pop).
    int w = hold;
    int other = sw_phase == 1 ? wpn : sw_from;
    gframe_t S, H, So;
    int ok = swap_slot(&P, Rc, spine, w, &S);
    int ok_o = other != w && swap_slot(&P, Rc, spine, other, &So);
    pose_gun_frame(&P, w, &H.o, &H.rt, &H.up, &H.fw);
    if (ok) {
      v3 cfw = v3_norm(v3_cross(spine, Rc));
      float p, att, sup, land = 0.0f;
      if (sw_phase == 1) {
        // Stow: support hand relaxes off -> gun glides smoothly to slot -> brief
        // tactile seat dwell (0.76..0.80) -> lead hand releases cleanly.
        sup = 1.0f - ease5(sw_k / 0.30f);
        p   = 1.0f - ease5((sw_k - 0.04f) / 0.72f);
        att = 1.0f - ease5((sw_k - 0.78f) / 0.22f);
      } else {
        // Draw: lead hand reaches & closes on stock/grip (0..0.30) -> brief grasp
        // beat -> weapon draws smoothly around flank (0.26..0.88) -> support hand
        // rises to meet it (0.60..0.94) -> seamless settle onto ready pose.
        att  = ease5(sw_k / 0.30f);
        p    = ease5((sw_k - 0.26f) / 0.62f);
        sup  = ease5((sw_k - 0.60f) / 0.34f);
        land = ease5((sw_k - 0.84f) / 0.16f);
      }
      gframe_t F = swap_frame(&P, Rc, spine, w, &S, &H, p);
      g_org = F.o; g_rt = F.rt; g_up = F.up; g_fw = F.fw;
      v3 fin0 = P.wri[0], fin1 = P.wri[1];
      // Ready chest-guard off-hand: tucked cleanly in front of lower chest/ribs
      // (bent ~95 deg, ready to immediately catch the weapon as it swings over
      // the shoulder — matching athletic tactical draw posture).
      v3 rest0 = v3_add(P.chest, v3_add(v3_scale(Rc, -0.14f),
                  v3_add(v3_scale(spine, -0.18f), v3_scale(cfw, 0.24f))));
      v3 rest1 = v3_add(P.chest, v3_add(v3_scale(Rc,  0.14f),
                  v3_add(v3_scale(spine, -0.18f), v3_scale(cfw, 0.24f))));
      int lead_l = w == WPN_SR, olead_l = ok_o && other == WPN_SR;
      // Front-outboard transition waypoint when the strong hand moves between
      // the over-shoulder rifle stock and the drop-leg holster, so the arm
      // sweeps cleanly up/down in front of the right shoulder (never behind back).
      v3 free1 = fin1;
      v3 c_[2], a_[2]; float r_[2];
      if (ok_o && !lead_l && !olead_l)
        free1 = v3_add(P.chest, v3_add(v3_scale(Rc, 0.22f),
                       v3_add(v3_scale(spine, -0.08f), v3_scale(cfw, 0.20f))));
      else if (ok_o)
        free1 = rest1;
      float k_ = 1.0f - att;
      float bow = sinf(F_PI * f_clamp(k_, 0.0f, 1.0f)) * (ok_o ? 0.75f : 0.50f);
      v3 w1, w0;
      float on1, on0;
      if (!lead_l) {
        v3 h1 = swap_handle(w, &F, p, 1, P.sho[1], Rc, spine, &c_[1], &a_[1], &r_[1]);
        w1 = v3_lerp(h1, free1, ease5(k_));
        // Arc cleanly UP IN FRONT OF THE SHOULDER (modest outboard, strong forward
        // and slight lift on over-shoulder reach) — never sticking out sideways.
        w1 = v3_add(w1, v3_add(v3_scale(Rc, 0.025f * bow),
                        v3_add(v3_scale(cfw, (w == WPN_AR ? 0.095f : 0.035f) * bow),
                               v3_scale(spine, (w == WPN_AR ? 0.055f : 0.0f) * bow))));
        gun_hold(w, GH_FORE3, F.o, F.rt, F.up, F.fw, &c_[0], &a_[0], &r_[0]);
        w0 = v3_lerp(rest0, gframe_wrist(w, &F, 0), sup);
        on1 = att; on0 = sup;
      } else {
        v3 h0 = swap_handle(w, &F, p, 0, P.sho[0], Rc, spine, &c_[0], &a_[0], &r_[0]);
        w0 = v3_lerp(h0, rest0, ease5(k_));
        w0 = v3_add(w0, v3_add(v3_scale(Rc, -0.025f * bow),
                        v3_add(v3_scale(cfw, 0.095f * bow), v3_scale(spine, 0.055f * bow))));
        float jr = ease5((p - 0.36f) / 0.38f);
        gun_hold(w, GH_GRIP, F.o, F.rt, F.up, F.fw, &c_[1], &a_[1], &r_[1]);
        w1 = v3_lerp(free1, gframe_wrist(w, &F, 1), jr);
        on0 = att; on1 = jr;
      }
      if (land > 0.0f) {
        w1 = v3_lerp(w1, fin1, land);
        w0 = v3_lerp(w0, fin0, land);
      }
      // Keep wrists comfortably inside arm reach (<= 93% mid-swap, 95% ready)
      // so neither elbow locks dead-straight while preserving exact grip contact.
      float amax = (ANIM_UARM + ANIM_FARM) * (0.93f + 0.02f * land);
      v3 ws_[2] = {w0, w1};
      for (int i = 0; i < 2; i++) {
        v3 aw = v3_sub(ws_[i], P.sho[i]);
        float awl = v3_len(aw);
        if (awl > amax) ws_[i] = v3_add(P.sho[i], v3_scale(aw, amax / awl));
        P.wri[i] = ws_[i];
      }
      float on_[2] = {on0, on1};
      v3 rest_p0 = v3_norm(v3_add(v3_scale(cfw, -0.45f), v3_add(v3_scale(Rc, -0.45f), v3_scale(spine, -0.78f))));
      v3 rest_p1 = v3_norm(v3_add(v3_scale(cfw, -0.45f), v3_add(v3_scale(Rc,  0.45f), v3_scale(spine, -0.78f))));
      // Use swap_pole throughout the lead hand's reach so the elbow smoothly
      // tracks the moving wrist with zero pole snap.
      v3 pole0 = v3_lerp(rest_p0, swap_pole(P.sho[0], P.wri[0], Rc, spine, -1.0f),
                         lead_l ? 1.0f : fmaxf(on0, land));
      v3 pole1 = v3_lerp(rest_p1, swap_pole(P.sho[1], P.wri[1], Rc, spine,  1.0f),
                         !lead_l ? 1.0f : fmaxf(on1, land));
      P.elb[0] = ik_solve(P.sho[0], P.wri[0], ANIM_UARM, ANIM_FARM, pole0);
      P.elb[1] = ik_solve(P.sho[1], P.wri[1], ANIM_UARM, ANIM_FARM, pole1);
      // Bulletproof straight-wrist, anatomically-rotated hand placement: while a
      // hand is traveling off the gun, orient its thumb along the elbow's natural
      // bicep-fold neutral axis (plus 22 deg relaxed pronation — never flipping
      // 180 deg) and keep the palm 72 mm straight ahead of the forearm. Only
      // engage the weapon cylinder once the wrist is actually AT the gun.
      g_fig_free = 0;
      g_fig_grab = 0;
      for (int i = 0; i < 2; i++) {
        if (land >= 0.92f) continue;
        v3 fd = v3_sub(P.wri[i], P.elb[i]);
        fd = v3_dot(fd, fd) > 1e-8f ? v3_norm(fd) : cfw;
        v3 ua = v3_sub(P.elb[i], P.sho[i]);
        ua = v3_dot(ua, ua) > 1e-8f ? v3_norm(ua) : v3_scale(spine, -1.0f);
        v3 bi = v3_sub(v3_scale(ua, -1.0f), v3_scale(fd, -v3_dot(ua, fd)));
        bi = v3_dot(bi, bi) > 1e-4f ? v3_norm(bi) : spine;
        v3 inw = v3_scale(v3_cross(bi, fd), i ? 1.0f : -1.0f);
        v3 fa = v3_norm(v3_add(v3_scale(bi, 0.92f), v3_scale(inw, 0.38f)));
        float dg = v3_len(v3_sub(c_[i], P.wri[i]));
        float prox = 1.0f - ease3((dg - 0.095f) / 0.055f);
        float hk = ease5((on_[i] - 0.45f) / 0.40f) * prox;
        g_fig_grab |= (1 << i);
        if (hk >= 0.99f) {
          g_fig_grab_c[i] = c_[i];
          g_fig_grab_a[i] = a_[i];
          g_fig_grab_r[i] = r_[i];
        } else {
          v3 td = dg > 1e-6f ? v3_scale(v3_sub(c_[i], P.wri[i]), 1.0f / dg) : fd;
          v3 md = v3_lerp(fd, td, hk);
          md = v3_dot(md, md) > 1e-8f ? v3_norm(md) : fd;
          float rr = 0.072f + (f_clamp(dg, 0.055f, 0.095f) - 0.072f) * hk;
          g_fig_grab_c[i] = v3_add(P.wri[i], v3_scale(md, rr));
          v3 am = v3_lerp(fa, a_[i], hk);
          if (v3_dot(am, am) < 1e-4f) am = inw;
          g_fig_grab_a[i] = v3_norm(am);
          g_fig_grab_r[i] = 0.013f + (r_[i] - 0.013f) * hk;
        }
      }
    } else {
      sw_phase = 0;
      hold = wpn;
    }
  }
  if (hold != wpn) { mag01 = 0.0f; trig01 = 0.0f; }
  if (g_fig_hos) g_fig_free = 3;   // a hostage holds nothing: both hands close empty
  else {
    probe("gun");
    int gv0 = g_scene_floats;
    g_gun_ak = kt.ins;   // the insurgent's rifle wears the AK's dress (looks only)
    g_gun_smoke = g_fig_nade_kind;   // the grenade in his hand: frag or canister
    gun_build(hold, g_org, g_rt, g_up, g_fw, mag01, trig01, hold == wpn ? gs.bolt : 0.0f);
    {
      const mdl_ref_t *tm = gun_tp_model(g_fig_ent, hold);
      if (tm) { g_scene_floats = gv0; gun_tp_draw(tm, hold, g_org, g_rt, g_up, g_fw, mag01); }
      if (tm && g_fig_dual)   // ...and the second Beretta, in the left hand
        gun_tp_draw(tm, hold, v3_sub(g_org, v3_scale(g_rt, DUAL3P * GUN_GIRTH_3P)),
                    g_rt, g_up, g_fw, mag01);
    }
    g_gun_ak = 0;
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
  // Only form the knuckle-drag brace fist when the off-hand is actually down
  // skimming the ground (not when raised onto the fore-end to shoot mid-slide).
  float brace_k = A->slide_s * (1.0f - f_clamp(fmaxf(A->ads_s, A->ready_s * 0.85f), 0.0f, 1.0f));
  fig_arms(&P, &kt, Rc, spine, g_org, g_rt, g_up, g_fw, hold,
           brace_k, A->slide_dir);
  g_fig_free = 0;
  g_fig_grab = 0;
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
  g_fig_var = ent_kind((int)(b - g_bots) + 1);   // the archetype (ent_kind)
  g_fig_team = fig_team_of((int)(b - g_bots) + 1);
  g_fig_ins = fig_ins_of((int)(b - g_bots) + 1) && g_fig_op_ent != (int)(b - g_bots) + 1 &&
              g_fig_op_ent != 999;   // harness `figop 999`: every bot an operator (a squad)
  g_fig_ent = (int)(b - g_bots) + 1;
  g_fig_nade_kind = b->wp.gr_kind;
  scene_draw_figure(&b->anim, pos, yaw, &dth, alpha, flash01, fc, 0, b->wp.cur,
                    gun_anim(&b->wp, alpha));
  g_fig_ent = -1;
  g_fig_var = 0;
  g_fig_team = -1;
  g_fig_ins = 0;
  g_fig_nade_kind = 0;
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
static int g_fx_flash_ak;   // fx_flash: the insurgent AK's warmer star (fx_build)
static void fx_flash(v3 pos, v3 fw, v3 t1, v3 t2, v3 eye, float s, float f,
                     uint32_t seed, int wpn) {
  uint32_t h = fx_hash(seed);
  float a0 = (float)(h & 255) * (F_TAU / 256.0f);
  #define FH01(k) ((float)((fx_hash(seed * 5u + (uint32_t)(k)) >> 4) & 255) / 255.0f)
  v3 hot = v3_scale(g_fx_flash_ak ? (v3){{2.15f, 1.30f, 0.50f}} : (v3){{2.10f, 1.55f, 0.85f}}, f);
  v3 mid = v3_scale(g_fx_flash_ak ? (v3){{1.20f, 0.40f, 0.08f}} : (v3){{1.10f, 0.46f, 0.12f}}, f);
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


