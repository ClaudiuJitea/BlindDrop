// code/render/viewmodel.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// --- viewmodel: the AR, built in gun-local space (x right, y up, z forward,
// origin at the receiver rear) and placed by the pose + spring offsets. -----

// Rotate a onto b's plane by ang (small-rotation compose for the gun basis).
static void rot2(v3 *a, v3 *b, float ang) {
  float c = cosf(ang), s = sinf(ang);
  v3 na = v3_add(v3_scale(*a, c), v3_scale(*b, s));
  *b = v3_add(v3_scale(*b, c), v3_scale(*a, -s));
  *a = na;
}

// The viewmodel pass has its own projection, deliberately wider than the world
// FOV: at 50 degrees a weapon close enough to read is mostly outside the frame.
// The scope disc derives its radius from this same number, so the two can never
// drift — that was two hardcoded halves of one constant.
// A PLAYER SETTING NOW (ARMORY > VIEW > WEAPON FOV), default 58 = the
// historical constant, so an untouched config renders exactly as before. A
// wider lens draws the same weapon smaller: at 58 the rifle covers about a
// fifth of the frame, centre included, and a player who wants the target
// rather than the receiver had no way to ask for it. Every reader below goes
// through this one function, the scope disc and holo window included, so
// they still derive from the lens actually used.
static float vm_fov_deg(void) { return f_clamp(g_cfg.vm_fov, 50.0f, 74.0f); }
#define VM_POS_X 0.016f      // m out, per WEAPON POSITION step
#define VM_POS_Y 0.020f      // m down, per step
#define VM_FOV_DEG vm_fov_deg()
static v3 g_vm_muzzle;  // world-space barrel tip, set by vm_build every frame
// The weapon's own basis in world space, published by the same vm_build call
// that builds the mesh. The shader's micro-structure is a function of position
// in THIS frame rather than in the world's, which is the whole difference
// between a finish that rides with the rifle and scratches that crawl over it
// as the player walks. Published unconditionally, like the muzzle the same call
// returns: the one consumer reads it immediately after the call it came from,
// and a harness probe that builds a pose without drawing it consumes nothing.
static v3 g_vm_mdl_o, g_vm_mdl_x, g_vm_mdl_y, g_vm_mdl_z;
static v3 g_vm_muz_cam; // the same tip in CAMERA space (right/up/forward) —
                        // what the tracer re-anchor FOV-remaps, since the
                        // viewmodel renders at VM_FOV_DEG, not the world FOV
static long g_vm_muz_tick = -1000; // publish tick: a windowed session renders
                        // every frame so the tip is at most a frame stale, but
                        // the HARNESS renders only on `shot` — a 100-tick-old
                        // hip-pose tip re-anchored under ADS zoom put the
                        // streak's start in the bottom corner of the frame

// TURNTABLE for the viewmodel (harness only). The viewmodel is the one part of
// the game that cannot be inspected with `cam`: it is drawn in its own pass with
// a projection built from the PLAYER's eye, so the only view of it that exists is
// the one the player has. That is exactly the wrong constraint when the thing
// being judged is whether a hand WRAPS the object it holds — from behind the
// gun, a hand that grips and a hand that merely floats beside the handguard
// render as the same brown mass, and every hand pass before this one was tuned
// on that single view. `vmorbit` builds the real pose (same springs, same ADS
// blend, same weapon) and then draws it from a camera orbiting the hands.
// `fov` and the bore pivot exist for the BOOT SCREEN, which drives this same
// rig: a 30-degree lens on a 20 cm subject is a wide-angle product shot, and
// the one move that shows a rifle IS a rifle is a long lens travelling along
// its own axis. `bore` is a station in the weapon's own frame, resolved
// against g_vm_mdl_* AFTER vm_build has published them.
static struct { int on, which, piv_on;
                float yaw, pit, dist, fov, bore, rise, comp; } g_vmorb;
// Hand/wrist diagnostics, published by vm_build for the `vmhand` command. The
// user complaint that started three rounds of this work was "the wrist looks
// badly kinked", and it kept surviving fixes because it was being judged from
// renders. These are the four numbers that decide it: how far the wrist bends,
// how the forearm meets the weapon, how long the palm is, and how far the wrist
// stands off the grip surface.
static struct { float bend_deg, bore_deg, palm_mm, off_mm; } g_vm_hdbg[2];
static v3 g_vm_hand_p[2];   // trigger wrist, support wrist — published by vm_build

// cubic-bezier(x1, y1, x2, y2) easing with (0,0) and (1,1) fixed: solve x(t)
// for t by Newton, return y(t).
static float ads_bezier(float x) {
  x = f_clamp(x, 0.0f, 1.0f);
  const float x1 = 0.22f, y1 = 0.61f, x2 = 0.36f, y2 = 1.0f;
  float t = x;
  for (int i = 0; i < 6; i++) {
    float it = 1.0f - t;
    float xt = 3.0f * it * it * t * x1 + 3.0f * it * t * t * x2 + t * t * t;
    float dx = 3.0f * it * it * x1 + 6.0f * it * t * (x2 - x1) + 3.0f * t * t * (1.0f - x2);
    if (fabsf(dx) < 1e-5f) break;
    t = f_clamp(t - (xt - x) / dx, 0.0f, 1.0f);
  }
  float it = 1.0f - t;
  return 3.0f * it * it * t * y1 + 3.0f * it * t * t * y2 + t * t * t;
}

// THE PIN CLOCK: ticks since the press, continuous through the release. A tap
// lets go at NADE_PIN_T, gr_cook resets and gr_t starts, so the off hand's
// pull-and-carry would freeze mid-air on any clock that only read gr_cook.
// gr_fuse is NADE_FUSE minus the cook at release, which is what stitches the two
// halves together. -1 = no pull in progress (idle, or the next grenade is up).
#define NPIN_REACH   20.0f   // off hand arrives at the ring
#define NPIN_PULL0   27.0f   // ...the finger has it, and draws
#define NPIN_PULL1   44.0f   // pin clear
#define NPIN_SPOON0  ((float)NADE_PIN_T)   // the sim lets the spoon go
#define NPIN_SPOON1  (NPIN_SPOON0 + 7.0f)  // ...and the viewmodel's has swung away
#define NPIN_GONE    88.0f   // off hand and pin are out of frame
#define VM_CONVERGE  3.0f    // m: where the hip-held bore crosses the crosshair's line
#define GP_SKY       0.5f    // pin hand roll: palm-down weight...
#define GP_THB       0.8f    // ...and thumb side toward the eye, so the hooked index shows
static float nade_clock(const weapon_t *w, float alpha) {
  if (w->cur != WPN_GR) return -1.0f;
  if (w->gr_cook > 0) return (float)w->gr_cook - 1.0f + alpha;
  if (w->gr_t > 0) {
    float c = (float)(NADE_FUSE - w->gr_fuse) - 1.0f + (float)w->gr_t + alpha;
    return c < NPIN_GONE ? c : -1.0f;
  }
  return -1.0f;
}

static void vm_build(const player_t *p, float alpha, v3 eye, v3 rgt, v3 upv, v3 fwd) {
  const vm_t *v = &p->vm;
  const weapon_t *w = &p->wp;
  scene_reset();
  #define VLERP(f) (v->prev_##f + (v->f - v->prev_##f) * alpha)
  // The raise follows a CUBIC BEZIER, not the bare ramp: quick off the hip,
  // and a long soft arrival into the eye (CSS's ease-out, 0.22/0.61/0.36/1),
  // so the last millimetres of alignment decelerate instead of stopping
  // dead. Both ends are fixed (0 and 1), so the hip pose and the aligned
  // sight picture are exactly where they were.
  float ads = ads_bezier(w->prev_ads_s + (w->ads_s - w->prev_ads_s) * alpha);
  float kick = VLERP(kick);
  float dip = p->prev_land_dip + (p->land_dip - p->prev_land_dip) * alpha;

  int cur = w->cur;
  int vvar = wvar(w, cur);   // the loadout variant in the hands (WPN_VAR)
  // The holographic sight sits higher than the iron sight the shared builder knows.
  float sight_y = cur == WPN_SR ? VM_SIGHT_SR : cur == WPN_PT ? VM_SIGHT_PT
                : vvar == RV_FAMAS ? VM_FAMAS_IRON_Y : VM_HOLO_Y;

  // Reload, trigger and bolt all come from ONE solver shared with the
  // third-person figure (`gun_anim`) — see the note there. `rl` is the body
  // arc, `rk` the two beat impulses, `rch` the charging-handle / bolt lobe,
  // `mdrop` the magazine's own travel and `mgv` the support hand's.
  gunanim_t ga_ = gun_anim(w, alpha);
  float rl = ga_.arc, rk = ga_.punch, mdrop = ga_.mag, rch = ga_.cyc;
  float trig = ga_.trig, mgv = ga_.hand;
  // Swap: the incoming weapon rises from below, muzzle tilted down.
  float sw = smooth01(w->prev_switch +
                      ((float)w->switch_t / GUN_SWITCH_TICKS - w->prev_switch) * alpha);

  // Pose: hip vs sight-aligned, then every spring offset on top.
  // FRAMING. The hip pose used to sit 155 mm below the eye and 300 mm forward
  // with the gun's own axis parallel to the view, so the camera looked DOWN onto
  // the weapon: the top rail and the barrel filled the corner and the receiver,
  // the magwell, the magazine and the stock were all outside the frustum. What
  // the player stared at all match was a black ladder. A first-person weapon is
  // a composition, not a placement — it is raised until its SIDE faces the
  // camera, canted so the right of the receiver turns into view, and pulled back
  // until the magazine is in frame. Every extra term is scaled by `hip` and is
  // therefore exactly zero in ADS, where the sight has to stay on the axis.
  float hip = 1.0f - ads;
  // Lean: how far the gun is behind the body's lean right now (0 once settled),
  // and the settled lean itself. Both act on the HIP pose only — in ADS the
  // sight has to stay exactly on the view axis, and the camera roll has already
  // canted the whole weapon with the shooter.
  float lean = p->prev_lean + (p->lean - p->prev_lean) * alpha;
  float lean_d = (v->prev_lean_lag + (v->lean_lag - v->prev_lean_lag) * alpha) - lean;
  // THE RELOAD DIP USED TO PUT THE WHOLE WEAPON BELOW THE FRAME. At 0.30 m in
  // front of the eye a 62-degree pass has 0.18 m of half-height, and the hip pose
  // already sits 0.120 m down; another 0.100 m plus 0.25 rad of muzzle-down pitch
  // took everything but the top rail off screen for the entire 1.8 s. Filmed at
  // six stations through an AR reload, four of the six frames contained no
  // magazine, no magwell and no hands at all — so the one moment of the viewmodel
  // that is all HAND was also the one moment nothing was visible. Scaled back to
  // where the magwell stays in frame; the dip still reads as a dip.
  // THE RELOAD HAS TO BRING THE MAGWELL INTO FRAME, and 30 mm of left drift did
  // not: filmed at 13 stations, the magazine and the hand on it sat in the
  // bottom 60 px of every one of them, on a weapon parked 158 mm right of the
  // eye. The subject of the animation was in the corner. 72 mm brings the
  // receiver's centreline to 86 mm right of the axis, which puts the magwell in
  // the middle third of the screen where the player is already looking.
  // THE CYCLE BEAT IS FRAMED PER WEAPON. The AR's charging handle is on the
  // receiver's top LEFT — the side the camera is already on — so its beat only
  // has to punch the pose. The sniper's bolt is on the RIGHT, so its beat has to
  // bring the action across the frame and toward the eye as well as roll it into
  // view; filmed at 78 and 88 percent with the AR's numbers, the whole receiver
  // sat in the right-hand edge of the frame and the handle was 20 px of grey.
  int srcyc = cur == WPN_SR;
  int ptl = cur == WPN_PT;    // a pistol sits lower and further out than a rifle
  // WEAPON POSITION (ARMORY > VIEW), HIP ONLY: the player's own offset
  // down and out, scaled by `hip` so ADS — where the sight IS the aim — is
  // untouched. Measured: the weapon FOV alone shrinks the rifle but pulls the
  // stock and arm in from off-frame, so the centre stays covered; moving the
  // hold is what clears it. 0 = the authored pose, exactly.
  // ...and NOT DURING A RELOAD OR A BOLT CYCLE: those poses are framed so the
  // magwell and the bolt are mid-frame, and the LOWER offset put the magazine
  // change half under the bottom edge. The offset blends out with them.
  float vmo = (float)(g_cfg.vm_pos < 0 ? 0 : g_cfg.vm_pos > 2 ? 2 : g_cfg.vm_pos)
            * (1.0f - f_clamp(rl + rch, 0.0f, 1.0f));
  float px = (ptl ? 0.112f : 0.148f) * hip + VLERP(bob_x) + VLERP(lag_x) + rl * 0.030f
           + VM_POS_X * vmo * hip
           + rk * 0.011f + rch * (srcyc ? -0.062f : 0.026f)
           + (lean_d * 0.16f - lean * 0.028f) * hip;
  // DOWN a little and NOT toward the eye. The comment used to call this a "dip
  // down-right" while the numbers raised the weapon 36 mm and pulled it 55 mm
  // CLOSER, which at a 62-degree pass magnifies it and pushes the magwell — the
  // one part of the gun a magazine change is about — off the bottom of the
  // frame. 28 mm down is a fifth of the 100 mm that once put the whole weapon
  // below frame, so the old failure is not being re-created.
  float py = (ptl ? -0.105f : -0.063f) * hip - sight_y * ads + VLERP(bob_y) + VLERP(lag_y)
           - VM_POS_Y * vmo * hip
           // sw * 0.125, not 0.22: THE SWAP PUT THE WHOLE WEAPON BELOW THE FRAME,
           // which is the identical failure the reload dip above was already fixed
           // for. At sw = 1 the origin sat 0.340 m under an axis whose half-height
           // at 0.248 m is 0.149 m, and the muzzle another 0.32 m under that.
           // Measured over the 36-tick raise: ticks 1-10 were byte-identical to an
           // empty frame — 83 ms, 28% of every weapon switch, with nothing on
           // screen — and it took 22 ticks to reach half the settled area. After:
           // 11034 px of weapon already visible on tick 1 and monotone from there.
           - dip * 0.5f - rl * 0.070f + rch * (srcyc ? 0.044f : 0.020f)
           - rk * 0.020f - sw * 0.125f;
  // The sniper pulls closer to the eye in ADS so the scope window opens up.
  // TAKE-UP: the weapon comes back into the grip as the finger closes on it.
  // This is the term that makes the press READ, and it is not a stylistic
  // choice — it is the only candidate with enough screen area. Measured, trig 0
  // against trig 1: the whole finger animation moves 227 px of 921600 (0 of them
  // by more than 10% of range) because the only visible part of the digit is its
  // distal band at 196 px and the blade itself renders 0 px from every camera
  // the game draws; 1.5 mm on this axis moves 34212. Along the VIEW AXIS rather
  // than as a pitch or a drop, because physically the finger pulls the rifle
  // rearward into the palm and a rotation has no ADS-safe form at all.
  // PER WEAPON IN ADS, not simply hip-scaled. The hazard the hip scale was
  // written for is specific to the SNIPER: its scope disc derives its radius
  // from the ocular's own distance, so any move along the view axis breathes the
  // picture (measured, 2% of the radius on every squeeze — a scope that pulses
  // when you press reads as a bug). The AR has no such coupling: its dot is a
  // HUD element on the screen's own centre once the sight is up, so a move ALONG
  // the axis cannot shift the point of aim by construction. Scaling the AR to
  // zero in ADS therefore bought nothing and cost everything — ADS is the state
  // a player is in when they actually shoot, and the press rendered
  // byte-identical frames there. 1.5 mm also stays well under the fire kick's
  // own 9.1 mm peak on the same axis, so recoil still dominates either way.
  // WALL PULL-BACK: drawn in toward the chest, down, and (below) muzzle up.
  float wallk = VLERP(wall);
  wallk = wallk * wallk * (3.0f - 2.0f * wallk);
  px -= 0.035f * wallk;
  py -= 0.045f * wallk;
  float tkick = cur == WPN_SR ? hip : 0.45f + 0.55f * hip;
  float pz = (ptl ? 0.37f : 0.30f) - 0.0015f * trig * tkick
           - 0.008f * hip - (cur == WPN_SR ? 0.152f : 0.06f) * ads - kick
           + (cur == WPN_AR && vvar == RV_FAMAS ? VM_FAMAS_ADS_PUSH : 0.0f) * ads
           + VLERP(lag_z) - sw * 0.03f + rl * 0.050f
           - rch * (srcyc ? 0.062f : 0.030f) + rk * 0.016f
           - 0.17f * wallk;
  float twist = VLERP(twist);

  // THE BARREL POINTS AT THE CROSSHAIR. The hip hold is angled in (yaw) and up
  // (pitch) so the receiver's side reads; those two were fixed numbers, 6.4 and
  // 1.3 deg, unrelated to where the gun sits, so the bore crossed the view axis
  // in neither plane at once: on screen the barrel ran off left of the T while
  // the round flew from its tip to the T — a shot that visibly bent out of the
  // muzzle. Both angles are now SOLVED to aim the bore at the crosshair's own
  // line VM_CONVERGE metres out, so the barrel's screen line passes through the
  // T and the tracer leaves along it. Same inward look, about half the yaw.
  // Positions are the hold's static terms plus whatever bob/lag is in flight —
  // the solve follows the gun, it does not fight the sway or the kick, which
  // are added on top below. Knife and grenade keep their authored hold.
  float aim_y = 0.112f, aim_p = 0.022f;
  if (cur != WPN_KN && cur != WPN_GR) {
    float dz = fmaxf(VM_CONVERGE - (pz + GUN_MUZ[cur][2]), 0.5f);
    aim_y = atanf(px / dz);
    aim_p = atanf(-(py + GUN_MUZ[cur][1]) / dz);
  }
  v3 gx = rgt, gy = upv, gz = fwd;
  // sw * 0.42, not 0.7: 0.7 rad of muzzle-down put the barrel tip a further
  // 0.32 m below an already-off-frame origin — see the note on py above.
  // MUZZLE FLIP, and it already existed — a review claimed "the gun never
  // ROTATES on a shot, the entire viewmodel recoil is a 9.5 mm slide along the
  // bore", and that is not what the code does: `kick * 6.0` is a PITCH, and at
  // the shipped ar_punch it is 0.055 rad = 3.1 deg, which moves 88 of the
  // weapon's ~234 kilopixels with the camera frozen. What it lacked is a reason
  // to be bigger at the HIP than in ADS. The extra term is hip-scaled to zero,
  // so ADS — the state a player actually shoots from, where the AR's dot is a
  // HUD element and the SR's disc is derived from the ocular — is left exactly
  // as it was measured, and the hip gets 4.8 deg instead of 3.1.
  rot2(&gz, &gy, -(VLERP(sway_p) + kick * (cur == WPN_SR ? 6.0f + 3.2f * hip : 2.6f + 1.2f * hip)
                 + rl * 0.055f - rch * 0.045f
                 - rk * 0.075f + sw * 0.42f) + aim_p * hip
                 + 0.62f * wallk);                                         // pitch
  rot2(&gx, &gz, VLERP(sway_y) + rl * 0.10f
                 + (aim_y + lean_d * 0.22f) * hip + twist * 0.25f
                 - 0.20f * wallk);                                         // yaw
  // THE CANT HAS A SIGN, and it decides whether the reload is about the magazine
  // or about the top rail. A positive roll swings the magwell AWAY from the eye
  // (the weapon sits right of centre at the hip, so the camera is on its left);
  // negative turns the magwell's own face toward the camera. At +0.85 the player
  // watched the receiver's right side for 1.3 s; at -0.55 the magazine is the
  // largest moving thing in the frame, which is what a magazine change is about.
  // The second beat's cant is POSITIVE — the opposite sign from the magazine
  // change above — precisely so it does not read as a second, smaller mag swap:
  // the magwell rolls away and the receiver's top and left, where the charging
  // handle is, turn toward the eye.
  // A BOLT HANDLE IS ON THE WEAPON'S RIGHT AND THE FIRST-PERSON CAMERA IS ON ITS
  // LEFT, so a bolt cycle is invisible unless the weapon is rolled to present
  // that side. This is what a shooter actually does — the rifle comes off the
  // shoulder and rotates so the action faces up and inboard — and it is the
  // opposite roll from the magazine change, which is also what stops the two
  // halves of the reload reading as one long wobble. 49 degrees, against the
  // AR's 19: the AR's charging handle is already on the camera's side.
  float cyroll = cur == WPN_SR ? 0.86f : 0.34f;
  rot2(&gx, &gy, VLERP(roll) + rl * -0.46f + rch * cyroll - rk * 0.16f + twist
                 + 0.30f * wallk +
                 (-0.086f - lean * 0.10f + lean_d * 0.30f) * hip);         // roll
  int fistw = cur == WPN_KN || cur == WPN_GR;
  float gr_arm = 0.0f;   // grenade: how much the forearm runs straight out of the wrist
  if (cur == WPN_KN) {
    // The knife IS its timeline: every other spring only rides on top of it.
    // The knife rides a little further out than its timeline says: at the
    // timeline's own distance the fist was nearly a third of the frame
    // in the guard and dwarfed the blade it holds. Scaling the offset from the
    // eye keeps every keyframe's screen position and only shrinks the
    // assembly. Viewmodel only: the figure keeps the timeline as authored.
    gx = rgt; gy = upv; gz = fwd;
    rot2(&gz, &gy, ga_.kn[3] - VLERP(sway_p) - kick * 2.0f + sw * 0.60f);
    rot2(&gx, &gz, ga_.kn[4] + VLERP(sway_y));
    rot2(&gx, &gy, ga_.kn[5] + VLERP(roll));
    px = ga_.kn[0] + VLERP(bob_x) + VLERP(lag_x);
    py = ga_.kn[1] + VLERP(bob_y) + VLERP(lag_y) - dip * 0.5f - sw * 0.125f;
    pz = ga_.kn[2] + VLERP(lag_z);
    px *= KNIFE_VM_PUSH; py *= KNIFE_VM_PUSH; pz *= KNIFE_VM_PUSH;
  } else if (cur == WPN_GR) {
    // Dedicated first-person grenade timeline: HAND_G cradles the shell from
    // behind-right-below on a true anatomical ulnar wrist, so the fuze stands
    // upright at rest, turns cleanly inward to offer the pull ring to HAND_GP,
    // and raises into a poised upright cook hold (no palm-over-top wrist flip).
    // Viewmodel only: the third-person figure keeps NADE_HOLD..NADE_DOWN.
    static const float G_IDLE[6] = {0.130f, -0.036f, 0.300f,  0.30f, 1.20f, -0.07f};
    static const float G_PULL[6] = {0.096f, -0.040f, 0.372f,  0.32f, 0.54f, -0.06f};
    static const float G_COOK[6] = {0.150f, -0.004f, 0.300f,  0.34f, 1.30f, -0.10f};
    // THE THROW, overhand. Drawn back beside the head (the shoulder cocks, the
    // grenade rises toward the top right and turns back on the wrist), whipped
    // over and forward on an accelerating drive, let go at full extension a
    // little left of centre, then the arm follows through down and across the
    // body and out of the frame, as a real throw's does. The next grenade comes
    // up from below once the hand is out of sight.
    static const float G_BACK[6] = {0.215f,  0.090f, 0.215f,  0.95f, 1.45f, -0.38f};
    static const float G_OUT[6]  = {0.060f,  0.030f, 0.520f, -0.30f, 1.05f, -0.04f};
    static const float G_FOLL[6] = {0.010f, -0.200f, 0.460f, -0.70f, 0.95f,  0.10f};
    // ...and underhand (AIM held): the hand swings back low by the hip, comes
    // through under the line of sight and lets go rising, the arm carrying on up.
    static const float G_LOWB[6] = {0.200f, -0.135f, 0.255f,  0.85f, 1.30f, -0.12f};
    static const float G_LOWO[6] = {0.090f, -0.055f, 0.500f,  0.15f, 1.05f, -0.06f};
    static const float G_LOWF[6] = {0.070f,  0.010f, 0.510f, -0.25f, 1.00f, -0.02f};
    // Out of frame: low and left after the follow-through, then low on the
    // right, straight under the hold, for the rise.
    static const float G_DOWN[6] = {0.050f, -0.430f, 0.330f, -0.90f, 1.00f,  0.10f};
    static const float G_UNDR[6] = {0.170f, -0.330f, 0.250f,  0.70f, 1.20f, -0.07f};
    #define G_COOK_AT(dst, c_) do {                                                \
      float u_in_ = smooth01(f_clamp((c_) / NPIN_REACH, 0.0f, 1.0f));              \
      float u_ck_ = smooth01(f_clamp(((c_) - 42.0f) / 26.0f, 0.0f, 1.0f));         \
      for (int i_ = 0; i_ < 6; i_++) {                                             \
        float p_ = G_IDLE[i_] + (G_PULL[i_] - G_IDLE[i_]) * u_in_;                 \
        (dst)[i_] = p_ + (G_COOK[i_] - p_) * u_ck_;                                \
      }                                                                            \
    } while (0)
    float kk[6];
    for (int i = 0; i < 6; i++) kk[i] = G_IDLE[i];
    if (w->gr_t > 0) {
      float t = (float)w->gr_t - 1.0f + alpha;
      float c_rel = (float)(NADE_FUSE - w->gr_fuse);
      float st[6];
      G_COOK_AT(st, c_rel);
      const float WIND = (float)NADE_WIND_T, REL = (float)NADE_RELEASE_T;
      const float FOLL = REL + 12.0f, DOWN = (float)NADE_RISE_T - 10.0f;
      const float RISE = (float)NADE_RISE_T, END = (float)NADE_THROW_T;
      const float *back = w->gr_lob ? G_LOWB : G_BACK;
      const float *out  = w->gr_lob ? G_LOWO : G_OUT;
      const float *foll = w->gr_lob ? G_LOWF : G_FOLL;
      const float *pa, *pb;
      float u;
      if (t < WIND) {                         // the draw: eases into the cock
        pa = st; pb = back; u = smooth01(t / WIND);
      } else if (t < REL) {                   // the drive: fastest at the release
        pa = back; pb = out; u = (t - WIND) / (REL - WIND);
        u = u * u * (1.7f - 0.7f * u);
      } else if (t < FOLL) {                  // the arm keeps its speed, then bleeds it
        pa = out; pb = foll; u = (t - REL) / (FOLL - REL);
        u = 1.0f - (1.0f - u) * (1.0f - u);
      } else if (t < DOWN) {                  // ...and drops out of the frame
        pa = foll; pb = G_DOWN; u = smooth01((t - FOLL) / (DOWN - FOLL));
      } else if (t < RISE) {                  // unseen: round under the hold
        pa = G_DOWN; pb = G_UNDR; u = smooth01((t - DOWN) / (RISE - DOWN));
      } else {                                // the next one comes up, settling in
        pa = G_UNDR; pb = G_IDLE; u = f_clamp((t - RISE) / (END - RISE), 0.0f, 1.0f);
        u = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
      }
      for (int i = 0; i < 6; i++) kk[i] = pa[i] + (pb[i] - pa[i]) * u;
    } else if (w->gr_cook > 0) {
      G_COOK_AT(kk, (float)w->gr_cook - 1.0f + alpha);
      if (w->gr_cook > NADE_PIN_T) {
        float k = (float)w->gr_cook / (float)NADE_FUSE, c = (float)w->gr_cook + alpha;
        k = k * k * k * 0.0065f;
        kk[0] += sinf(c * 1.13f) * k;
        kk[1] += sinf(c * 1.61f + 1.0f) * k;
        kk[5] += sinf(c * 0.91f + 2.0f) * k * 3.0f;
      }
    }
    #undef G_COOK_AT
    gx = rgt; gy = upv; gz = fwd;
    rot2(&gz, &gy, kk[3] - VLERP(sway_p) - kick * 2.0f + sw * 0.60f);
    rot2(&gx, &gz, kk[4] + VLERP(sway_y));
    rot2(&gx, &gy, kk[5] + VLERP(roll));
    gr_arm = 1.0f;
    px = kk[0] + VLERP(bob_x) + VLERP(lag_x);
    py = kk[1] + VLERP(bob_y) + VLERP(lag_y) - dip * 0.5f - sw * 0.125f;
    pz = kk[2] + VLERP(lag_z);
  }
  v3 org = v3_add(eye, v3_add(v3_scale(rgt, px),
           v3_add(v3_scale(upv, py), v3_scale(fwd, pz))));
  // ---- THE PIN PULL (grenade, viewmodel only) --------------------------------
  // The off hand used to be the knife fist bobbing up beside the grenade while
  // the pin and the lever simply stopped being drawn. Now the ring is TAKEN: the
  // left fist's index finger goes through the ring's far side, the ring swivels
  // on the pin to meet it (a real ring does), the pin draws straight out along
  // its own axis riding the finger, and the hand carries it down out of frame.
  // The fist's orientation is SOLVED from the ring rather than keyframed, so the
  // finger stays on the wire however the grenade hand sways.
  float pin_c = cur == WPN_GR ? nade_clock(w, alpha) : -1.0f;
  int pin_on = pin_c >= 0.0f;          // the pull owns groups 2 and 3 and the off hand
  v3 pin_o = org, pin_x = gx, pin_y = gy, pin_z = gz;   // group 3's frame
  v3 sp_o = org, sp_x = gx, sp_y = gy, sp_z = gz;       // group 2's frame (the spoon)
  v3 ph_o = org, ph_x = gx, ph_y = gy, ph_z = gz;       // the left fist
  v3 rg_o = org, rg_x = gx, rg_y = gy, rg_z = gz;       // group 4's frame (the ring)
  int sp_draw = 1;
  if (pin_on) {
    #define MP(a, b, c) v3_add(org, v3_add(v3_scale(gx, a), v3_add(v3_scale(gy, b), v3_scale(gz, c))))
    v3 Am = v3_add(v3_scale(gy, 0.921f), v3_scale(gz, 0.386f));    // NADE_A in the world
    v3 Em = v3_add(v3_scale(gy, -0.386f), v3_scale(gz, 0.921f));   // NADE_E
    v3 d = v3_scale(gx, -1.0f);                   // out along the pin, ring side
    float reach = ui_easeout(f_clamp(pin_c / NPIN_REACH, 0.0f, 1.0f));
    float pull = smooth01(f_clamp((pin_c - NPIN_PULL0) / (NPIN_PULL1 - NPIN_PULL0), 0.0f, 1.0f));
    float away = f_clamp((pin_c - NPIN_PULL1) / (NPIN_GONE - NPIN_PULL1), 0.0f, 1.0f);
    away = away * away * (3.0f - 2.0f * away);
    // The fist's own grip cylinder (the knife handle's, HAND_KL is the knife fist
    // mirrored): axis a, the palm on side s. The index finger's inner face sits
    // NPIN_FR off the axis on the finger side, a little toward the guard end.
    // THE PIN HAND (HAND_GP) IS PLACED BY ITS CROOK. The model exports the point
    // inside the hooked index (HOOK), the line the wire takes through it (WIRE)
    // and the hook's opening (OPEN, toward what it holds). The frame maps
    // WIRE -> the ring's wire tangent, -OPEN -> the pull, so the wire sits in the
    // crook in every pose and the hand pulls knuckle-first, as a hooked finger
    // does. (Model frames are mapped into the LEFT-handed camera convention,
    // so the third axis is d x A, not A x d.)
    const v3 Lh = HAND_GP_HOOK, Lw0 = HAND_GP_WIRE, Lo0 = HAND_GP_OPEN;
    v3 LW = v3_norm(Lw0);
    v3 LP = v3_norm(v3_sub(Lo0, v3_scale(LW, v3_dot(Lo0, LW))));
    v3 LN = v3_cross(LW, LP);
    // THE ARM DECIDES, THE RING FOLLOWS. A hooked finger pulls in line with its
    // forearm — the wrist straight, the pull coming from the elbow — and a pull
    // ring swings freely on the pin's eye. So the ring's line (eye -> far wire) is
    // aimed back at the left elbow, the hand lies along that same line, and the
    // forearm (below) continues it: no wrist bend can appear. The ring may only
    // swing so far off the pin's axis; past that the arm takes the difference.
    // The elbow sits low and forward of the left hip of the frame, so the forearm
    // rises steeply from the bottom edge instead of bridging across the screen.
    v3 elb = v3_add(eye, v3_add(v3_scale(rgt, -0.22f),
                    v3_add(v3_scale(upv, -0.60f), v3_scale(fwd, 0.16f))));
    v3 eye0 = MP(-0.0190f, -0.02446f, 0.04622f);  // the pin's eye: the ring hangs here
    v3 u = v3_norm(v3_sub(elb, eye0));
    {
      const float cm = 0.42f;                      // ~65 deg off the pin axis at most
      float c = v3_dot(u, d);
      if (c < cm) {
        v3 pp = v3_norm(v3_sub(u, v3_scale(d, c)));
        u = v3_add(v3_scale(d, cm), v3_scale(pp, sqrtf(1.0f - cm * cm)));
      }
    }
    // Roll about that line: the back of the glove to the eye and a little
    // skyward — a pronated pull, knuckles leading the hand home.
    v3 e1 = v3_norm(v3_sub(Am, v3_scale(u, v3_dot(Am, u))));
    v3 e2 = v3_cross(u, e1);
    v3 hook0 = v3_add(eye0, v3_scale(u, 0.0250f));
    v3 look = v3_norm(v3_sub(hook0, eye));
    #define GP_FRAME(Ah_, X, Y, Z) do {                                             \
      v3 Nw = v3_cross(u, (Ah_));                                                 \
      X = v3_add(v3_scale((Ah_), LW.x), v3_add(v3_scale(u, LP.x), v3_scale(Nw, LN.x))); \
      Y = v3_add(v3_scale((Ah_), LW.y), v3_add(v3_scale(u, LP.y), v3_scale(Nw, LN.y))); \
      Z = v3_add(v3_scale((Ah_), LW.z), v3_add(v3_scale(u, LP.z), v3_scale(Nw, LN.z))); \
    } while (0)
    float best = -9.0f, th = 0.0f;
    for (int i = 0; i < 72; i++) {
      float t = -F_PI + (float)i * (2.0f * F_PI / 72.0f);
      v3 Ah = v3_add(v3_scale(e1, cosf(t)), v3_scale(e2, sinf(t)));
      v3 kx, ky, kz;
      GP_FRAME(Ah, kx, ky, kz);
      (void)ky;
      float sc = v3_dot(kz, look) + GP_SKY * v3_dot(kz, v3_scale(upv, -1.0f))
               - GP_THB * v3_dot(kx, look);
      if (sc > best) { best = sc; th = t; }
    }
    v3 Ah = v3_add(v3_scale(e1, cosf(th)), v3_scale(e2, sinf(th)));
    GP_FRAME(Ah, ph_x, ph_y, ph_z);
    #undef GP_FRAME
    // The pin draws straight out along its own axis; the hand comes up its own
    // forearm's line to the ring and, with the pin clear, goes back down it.
    v3 pl = v3_scale(d, 0.046f * pull);
    v3 back = v3_sub(elb, hook0);
    v3 mv = v3_add(v3_scale(back, 0.70f * away), v3_scale(upv, -0.05f * away));
    v3 Lw = v3_add(v3_scale(ph_x, Lh.x), v3_add(v3_scale(ph_y, Lh.y), v3_scale(ph_z, Lh.z)));
    ph_o = v3_add(v3_sub(v3_add(hook0, pl), Lw),
                  v3_add(mv, v3_scale(back, 0.60f * (1.0f - reach))));
    // The pin keeps its own frame; the ring swings on its eye from where it lay
    // (out along d, in the A plane) to the hand's line, as the finger takes it.
    float hk = smooth01(f_clamp((pin_c - (NPIN_REACH - 6.0f)) / 6.0f, 0.0f, 1.0f));
    v3 Ar = v3_dot(Ah, Am) >= 0.0f ? Ah : v3_scale(Ah, -1.0f);   // the ring is symmetric
    v3 ub = v3_norm(v3_add(v3_scale(d, 1.0f - hk), v3_scale(u, hk)));
    v3 ab = v3_add(v3_scale(Am, 1.0f - hk), v3_scale(Ar, hk));
    ab = v3_norm(v3_sub(ab, v3_scale(ub, v3_dot(ab, ub))));
    float hs = v3_dot(Em, v3_cross(d, Am)) >= 0.0f ? 1.0f : -1.0f;
    v3 eb = v3_scale(v3_cross(ub, ab), hs);
    #define RV(v) v3_add(v3_scale(ub, v3_dot(v, d)), \
                  v3_add(v3_scale(ab, v3_dot(v, Am)), v3_scale(eb, v3_dot(v, Em))))
    rg_x = RV(gx); rg_y = RV(gy); rg_z = RV(gz);
    v3 oe = v3_sub(org, eye0);
    rg_o = v3_add(v3_add(eye0, RV(oe)), pl);
    #undef RV
    pin_o = v3_add(org, pl);
    if (pin_c >= NPIN_PULL1) { pin_o = v3_add(pin_o, mv); rg_o = v3_add(rg_o, mv); }
    // The spoon: the moment the pin clears it is no longer held, and it swings
    // open on the hinge at the fuze cap — outward, away from the body — before
    // the world's own spoon takes over.
    float sw2 = f_clamp((pin_c - NPIN_SPOON0) / (NPIN_SPOON1 - NPIN_SPOON0), 0.0f, 1.0f);
    sp_draw = pin_c < NPIN_SPOON1;
    if (sw2 > 0.0f) {
      v3 out = v3_norm(v3_add(v3_scale(Em, -0.883f), v3_scale(gx, 0.469f)));   // th -28, mirrored x
      v3 hax = v3_norm(v3_cross(Am, out));
      v3 hinge = MP(0.0f, -0.0116f, 0.0484f);
      float ang = -1.9f * sw2 * sw2;
      // Rodrigues on the three basis vectors; the origin turns about the hinge.
      #define RODR(v) v3_add(v3_add(v3_scale(v, cosf(ang)), v3_scale(v3_cross(hax, v), sinf(ang))), \
                             v3_scale(hax, v3_dot(hax, v) * (1.0f - cosf(ang))))
      sp_x = RODR(gx); sp_y = RODR(gy); sp_z = RODR(gz);
      v3 r0 = v3_sub(org, hinge);
      sp_o = v3_add(hinge, RODR(r0));
      sp_o = v3_add(sp_o, v3_scale(out, 0.012f * sw2));
      #undef RODR
    }
    #undef MP
  }

  ao_off();                               // a gun at arm's length sees open sky
  g_fig_k = FIG_KMAX;                     // ...and always at full resolution
  probe("vm_gun");
  g_vm_mdl_o = org; g_vm_mdl_x = gx; g_vm_mdl_y = gy; g_vm_mdl_z = gz;
  // BROKEN EDGES, FIRST PERSON ONLY, and that is an LOD decision of exactly the
  // same kind as g_fig_k above it rather than a second weapon model. A 0.7 mm
  // chamfer is a third of a pixel on a bot's rifle at 20 m and 3.7x the vertices
  // everywhere, and the scene budget is sized for twenty of those rifles at
  // once; at arm's length it is the difference between a machined receiver and a
  // slab. The one gun_build stays the one gun_build — this changes how its boxes
  // are TESSELLATED, not what it builds.
  g_cham = 0.0013f;
  int gun_mark = g_scene_floats;
  g_vm_muzzle = gun_build(cur, org, gx, gy, gz, mdrop, trig, ga_.bolt);
  g_cham = 0.0f;
  {
    // gun_build has published the muzzle, sight and trigger; the sculpted mesh
    // replaces only what it drew. The stock sits under the cheek when aiming and
    // swings past the lens during a reload, so it is left out for both.
    g_scene_floats = gun_mark;
    int ar = cur == WPN_AR;
    const mdl_ref_t *kn = armory_knife();
    const mdl_ref_t *gv = gun_var_model(cur, vvar);
    const int16_t (*mv)[7] = gv ? gv->v : cur == WPN_SR ? SNIPER_V
                           : cur == WPN_KN ? kn->v : GRENADE_V;
    const model_part_t *mp = gv ? gv->p : cur == WPN_SR ? SNIPER_P
                           : cur == WPN_KN ? kn->p : GRENADE_P;
    int mn = gv ? gv->np : cur == WPN_SR ? SNIPER_NP
           : cur == WPN_KN ? kn->np : GRENADE_NP;
    // The weapon theme covers the weapon and its magazines; the grenade is
    // ordnance, not furniture, and keeps its own olive.
    g_md_skin = cur != WPN_GR && vm_theme(cur, vvar)->pat > 0.0f;
    int nade_gone = cur == WPN_GR && ga_.bolt > 0.5f;    // released
    if (!nade_gone) model_draw(mv, mp, mn, 0, org, gx, gy, gz);
    if (pin_on) { if (sp_draw) model_draw(mv, mp, mn, 2, sp_o, sp_x, sp_y, sp_z); }
    else if (ads < 0.5f && rl < 0.2f && !nade_gone && ga_.spoon < 0.5f) model_draw(mv, mp, mn, 2, org, gx, gy, gz);
    float md = mdrop * GUN_MAG_TRAVEL[cur][0], mx = mdrop * -GUN_MAG_TRAVEL[cur][1];
    {
      // Old and fresh magazines (see gun_anim_t.pair): each sits at the well's
      // travel, rides as much of the hand's pouch trip as it is in the hand
      // for, and the pair is offset sideways by the shift across.
      v3 well = v3_add(org, v3_add(v3_scale(gy, -md), v3_scale(ar ? gz : gx, mx)));
      v3 L = vm_mag_centre_v(cur, vvar);
      v3 wc = v3_add(well, v3_add(v3_scale(gy, L.y), v3_scale(gz, L.z)));
      v3 po = (ar || cur == WPN_SR) ? vm_pouch_disp(wc, eye, rgt, upv, fwd, ga_.pouch)
                                    : gun_pouch_ofs(gx, gy, gz, ga_.pouch);
      v3 so = ga_.retain ? gun_pair_ofs(gx, gz) : (v3){{0, 0, 0}};
      if (ga_.old_vis)
        model_draw(mv, mp, mn, 1, v3_add(v3_add(well, v3_scale(po, ga_.old_pouch)),
                                         v3_scale(so, -ga_.pair)), gx, gy, gz);
      if (ga_.fresh_vis) {
        // INDEXED: tilted nose-forward about its own centre on the way up, so
        // the front of the magazine meets the well first and the hand rocks it
        // straight as it goes in.
        float ta = 0.55f * ga_.index;
        float ca = cosf(ta), sa = sinf(ta);
        v3 ty = v3_add(v3_scale(gy, ca), v3_scale(gz, sa));
        v3 tz = v3_sub(v3_scale(gz, ca), v3_scale(gy, sa));
        v3 c = v3_add(v3_add(wc, v3_scale(po, ga_.fresh_pouch)), v3_scale(so, 1.0f - ga_.pair));
        model_draw(mv, mp, mn, 1, v3_sub(c, v3_add(v3_scale(ty, L.y), v3_scale(tz, L.z))),
                   gx, ty, tz);
      }
    }
    // The spent magazine, falling free of the well (empty reloads only).
    if (ga_.drop_t >= 0.0f && ga_.drop_t < 0.6f && (ar || cur == WPN_SR)) {
      v3 dy = gy, dz = gz;
      v3 dorg = gun_drop_pose(cur, vvar, org, gx, &dy, &dz, ga_.drop_t);
      model_draw(mv, mp, mn, 1, dorg, gx, dy, dz);
    }
    if (cur == WPN_PT) {
      model_draw(mv, mp, mn, 3, v3_add(org, v3_scale(gz, -ga_.slide * 0.034f)), gx, gy, gz);
      g_vm_muzzle = v3_add(org, v3_add(v3_scale(gy, GUN_MUZ[WPN_PT][1]),
                                       v3_scale(gz, vvar == PV_DEAGLE ? VM_DEAGLE_MUZ_Z
                                                                      : GUN_MUZ[WPN_PT][2])));
      g_gun_sight_p = v3_add(org, v3_add(v3_scale(gy, sight_y), v3_scale(gz, gun_sight_z(cur))));
      g_gun_sight_r = 0.0026f;
    } else if (cur == WPN_GR) {
      if (pin_on) {
        model_draw(mv, mp, mn, 3, pin_o, pin_x, pin_y, pin_z);
        model_draw(mv, mp, mn, 4, rg_o, rg_x, rg_y, rg_z);
      } else if (!nade_gone && ga_.slide < 0.5f) {
        model_draw(mv, mp, mn, 3, org, gx, gy, gz);
        model_draw(mv, mp, mn, 4, org, gx, gy, gz);
      }
      g_vm_muzzle = org;
    } else if (cur == WPN_KN) {
      g_vm_muzzle = v3_add(org, v3_scale(gy, GUN_MUZ[WPN_KN][1]));
    } else if (ar) {
      g_vm_muzzle = v3_add(org, v3_add(v3_scale(gy, 0.022f),
                                       v3_scale(gz, vvar == RV_FAMAS ? VM_FAMAS_MUZ_Z : 0.492f)));
      int irons = vvar == RV_FAMAS;   // the FAMAS has no optic: its iron sights
      g_gun_sight_p = v3_add(org, v3_add(v3_scale(gy, sight_y),
                                         v3_scale(gz, irons ? VM_FAMAS_IRON_Z : VM_HOLO_Z)));
      g_gun_sight_r = irons ? 0.0026f : VM_HOLO_R;   // irons: the aperture's radius
      g_holo_kind = vvar != 0;   // the AK carries the compact red dot
      // NO MODELLED DOT. A holographic reticle is collimated: it is an image at
      // infinity, so it sits on the point of aim wherever the eye is behind the
      // window and slides across the glass as the rifle sways — a dot BUILT
      // into the hood does the opposite and rides with the frame. The reticle
      // is drawn in the HUD on the aim point and clipped by this window, which
      // is projected here exactly as the scope's ocular is.
      {
        v3 rel = v3_sub(g_gun_sight_p, eye);
        float oz = v3_dot(rel, fwd);
        g_holo_ex[0] = g_holo_ex[1] = g_holo_ey[0] = g_holo_ey[1] = 0.0f;
        if (oz > 0.02f && !irons) {   // no window, no glass, no projected dot
          float sc = ui_scale();
          float vh = (float)R.height / sc, vw = (float)R.width / sc;
          float f = (vh * 0.5f) / tanf(VM_FOV_DEG * 0.5f * DEG2RAD);
          g_holo_cx = 0.5f * vw + v3_dot(rel, rgt) / oz * f;
          g_holo_cy = 0.5f * vh - v3_dot(rel, upv) / oz * f;
          float hr = VM_HOLO_R * 0.92f / oz * f;   // inside the hood's own lip
          g_holo_ex[0] = v3_dot(gx, rgt) * hr * 1.18f; g_holo_ex[1] = -v3_dot(gx, upv) * hr * 1.18f;
          g_holo_ey[0] = v3_dot(gy, rgt) * hr;          g_holo_ey[1] = -v3_dot(gy, upv) * hr;
        }
      }
    } else {
      float blift, bdraw;
      gun_bolt_phase(ga_.bolt, &blift, &bdraw);
      v3 bx = gx, by = gy;
      rot2(&bx, &by, blift * 1.05f);   // 60-deg precision 3-lug bolt throw around y=0.034
      v3 bo = v3_add(org, v3_add(v3_scale(v3_sub(gy, by), 0.0340f),
                                 v3_scale(gz, -bdraw * 0.038f)));
      model_draw(mv, mp, mn, 3, bo, bx, by, gz);
      g_vm_muzzle = v3_add(org, v3_add(v3_scale(gy, 0.016f), v3_scale(gz, 0.506f)));
    }
  }
  // The tracer re-anchor publish does NOT happen here: vm_build is also the
  // engine of the harness probes (vmcheck/vmsight/vmhand), which build forced
  // poses on a synthetic camera basis — a probe must stay a side-effect-free
  // observer, and publishing from inside it re-anchored the next live tracer
  // onto a pose the render never draws. render_frame's real first-person
  // pass, the one caller whose basis IS the camera, owns the publish.

  // Hands and forearms. Without them the first-person view is a rifle floating
  // in space — the one thing the player looks at for a whole match, held by
  // nobody. Both are placed in the GUN's own frame (trigger hand on the pistol
  // grip, support hand under the handguard), so they inherit every spring, the
  // reload dip and the swap raise for free, and they are the same fig_hand the
  // third-person figure uses rather than a second pair.
  {
    // FROM THE FIGURE'S OWN KIT, not restated. These were two hand-tuned
    // constants that had to be kept in step with fig_kit by hand, and they had
    // already drifted apart: the third-person player's GLOVE was slate blue
    // while the first-person one was brown leather, so the same man had
    // different hands depending on which camera was looking at him — and the
    // sleeve was 5% off as well.
    //
    // The one thing that may legitimately differ is a LIGHTING compensation.
    // The viewmodel has its own hero rig and its own depth-cleared pass; the
    // world sun and the shadow map do not reach it, and at equal albedo it
    // measures a few percent darker than the same cloth on the body (measured:
    // rendered mean luminance of the bluish pixels, 95 on the body against 75
    // on the viewmodel before this was compensated at all). That factor lives
    // here, once, instead of in a second set of colours that can drift.
    g_fig_team = g_local_ent >= 0 ? fig_team_of(g_local_ent) : -1;
    kit_t vkt = fig_kit(1, 0.0f, (v3){{1, 1, 1}});
    g_fig_team = -1;
    v3 glove  = v3_scale(vkt.glove, 1.055f);    // ...and his own gloves
    // The cuff belongs to the GLOVE, not to the sleeve. In the sleeve's own
    // slate it read as a stripe painted on the arm; in the glove's leather two
    // value steps down it is a third object between two others, which is what
    // turns hand/arm into hand/cuff/arm and gives the silhouette its middle term.
    // BOTH hands are placed on the weapon's OWN published grip cylinders
    // (`gun_hold`) rather than on hand-tuned offsets. The hand builder wraps a
    // cylinder, so the only thing left to author per hand is where the arm comes
    // from — a bearing about that cylinder and how far the wrist stands off it —
    // and both of those are things you can reason about: the support arm comes up
    // from the lower left, the trigger arm from behind and below.
    probe("vm_hands");
    struct { v3 wrist, gc, ga; float gr, side; } hd[2];
    // THE RELOAD IS THE ONE MOMENT OF A VIEWMODEL THAT IS ALL HAND, and it was
    // the one moment nothing happened: the gun dipped and rolled while both hands
    // stayed welded where they were, so a magazine change read as "the rifle
    // tilts a bit". The support hand now leaves the fore-end for the weapon's own
    // published reload hold and comes back — and because a hand is built as a
    // wrap round whatever cylinder it is given, gripping a magazine instead of a
    // handguard costs one table row and no new geometry.
    // Envelope: leave at 10% of the reload, arrive by 22% — before the magazine
    // starts moving — hold, and be back on the fore-end before the bolt, so the
    // gun is never "live" with the support hand still off it. `mgv` comes from
    // gun_anim, which the third-person figure reads too.
    for (int h = 0; h < 2; h++) {
      v3 gc, ga; float gr;
      gun_hold(cur, h == 0 ? GH_GRIP : GH_FORE, org, gx, gy, gz, &gc, &ga, &gr);
      if (h == 1 && mgv > 0.0f) {
        v3 rc, ra; float rr;
        gun_hold(cur, GH_RELD, org, gx, gy, gz, &rc, &ra, &rr);
        if (cur == WPN_AR && vvar == RV_FAMAS) {
          // High on the FAMAS's magazine, right under its well behind the grip;
          // the box is nearly upright, so the hand's axis is too.
          rc = v3_add(org, v3_add(v3_scale(gy, -0.0520f), v3_scale(gz, -0.1420f)));
          ra = v3_norm(v3_add(v3_scale(gy, 0.990f), v3_scale(gz, 0.140f)));
        }
        // The AR's magazine drops away on the same curve the model uses, so the
        // hand has to travel with it or it grips where the mag no longer is.
        // Exactly the magazine's own travel, not the pose arc's. They were
        // decoupled: the hand followed `rl * 0.052` while the magazine dropped
        // `md01 * 0.150`, so mid-reload the hand gripped 10 cm of empty air
        // above a magazine that had already left.
        // BOTH components of the magazine's travel, not just the drop — the mag
        // now swings toward the camera as well as down (see `mdx` in gun_build),
        // and a hand that only follows the vertical half grips 34 mm to the side
        // of the thing it is holding. Exactly the decoupling this line was written
        // to fix, in the other axis.
        rc = v3_add(rc, v3_add(v3_scale(gy, -mdrop * GUN_MAG_TRAVEL[cur][0]),
                               v3_scale(gx, -mdrop * GUN_MAG_TRAVEL[cur][1])));
        if (cur == WPN_AR || cur == WPN_SR) {
          float md2 = mdrop * GUN_MAG_TRAVEL[cur][0], mx2 = mdrop * -GUN_MAG_TRAVEL[cur][1];
          v3 L = vm_mag_centre_v(cur, vvar);
          v3 wc = v3_add(org, v3_add(v3_scale(gy, -md2 + L.y),
                         v3_add(v3_scale(gz, L.z), v3_scale(cur == WPN_AR ? gz : gx, mx2))));
          rc = v3_add(rc, vm_pouch_disp(wc, eye, rgt, upv, fwd, ga_.pouch));
          // The hand turns with the magazine it is indexing.
          float ta = 0.55f * ga_.index;
          ra = v3_norm(v3_add(v3_scale(ra, cosf(ta)), v3_scale(v3_cross(gx, ra), sinf(ta))));
        } else {
          rc = v3_add(rc, gun_pouch_ofs(gx, gy, gz, ga_.pouch));
        }
        if (ga_.retain) {
          // Two in the hand: grip between them. One (the old, after the seat):
          // grip it.
          float both = (float)(ga_.old_vis && ga_.fresh_vis && ga_.fresh_pouch > 0.5f);
          float k = both ? 0.5f - ga_.pair : (ga_.fresh_pouch > 0.5f ? 1.0f - ga_.pair : -ga_.pair);
          rc = v3_add(rc, v3_scale(gun_pair_ofs(gx, gz), k));
        }
        gc = v3_lerp(gc, rc, mgv);
        ga = v3_norm(v3_lerp(ga, ra, mgv));
        gr = gr + (rr - gr) * mgv;
      }
      // THE RIGHT HAND WORKS THE BOLT. This is the other half of a bolt gun's
      // reload and the half a player recognises instantly — a magazine change
      // alone is an AR reload with a different magazine in it. The hand leaves
      // the pistol grip for the knob, rides it up-back-forward-down (the knob's
      // own position comes from gun_build, so the hand cannot grip where the
      // bolt is not), and is back on the grip before the weapon goes live.
      if (h == 0 && ga_.bhand > 0.0f && cur == WPN_SR) {
        // A bolt knob is a small ball gripped across, not a cylinder wrapped
        // along: the axis is the weapon's own right, and the radius is the knob's.
        v3 kc = gun_bolt_knob(ga_.bolt, org, gx, gy, gz, 1.0f), ka = gx;
        float kr = 0.0082f;
        gc = v3_lerp(gc, kc, ga_.bhand);
        ga = v3_norm(v3_lerp(ga, ka, ga_.bhand));
        gr = gr + (kr - gr) * ga_.bhand;
      }
      // Wrist bearing, authored as a raw direction and then projected onto the
      // plane perpendicular to the grip axis, so it stays a legal bearing however
      // the axis is raked. Trigger hand: back, down and a little right. Support
      // hand: down and left, which is the side the arm actually comes from and
      // therefore the side the palm's heel lands on.
      // Support: nearly straight DOWN — the heel grip carries the weight
      // under the handguard, not on its left flank.
      v3 br = h == 0
        ? v3_add(v3_scale(gx, 0.34f), v3_add(v3_scale(gy, 0.16f), v3_scale(gz, -0.92f)))
        : v3_add(v3_scale(gx, -0.55f), v3_scale(gy, -0.84f));
      // On the magazine the arm comes from the left and FORWARD instead of from
      // below, which is what turns the travel into a reach rather than a slide.
      if (h == 1 && mgv > 0.0f)
        br = v3_lerp(br, v3_add(v3_scale(gx, -0.82f), v3_scale(gz, 0.57f)), mgv);
      br = v3_sub(br, v3_scale(ga, v3_dot(br, ga)));
      br = v3_norm(br);
      // How far the wrist stands off the surface, and how far back along the
      // knuckle row it sits. A wrist is on the LITTLE-finger side of the palm's
      // breadth, never centred on it — centring it is what makes a hand look
      // like a bat welded to a stick.
      // How far the wrist stands off the surface. This IS the palm's length:
      // the chord from the wrist to the knuckle row is 52 mm at lift 42 and
      // 76 mm at lift 72, against ANSUR's ~107 mm. The hand was carrying full
      // anatomical BREADTH (88 mm) and full anatomical wrist DEPTH (41 mm) on
      // half its anatomical length — 88 x 50 x 41 mm is a bar of soap, and it
      // rendered as one from every angle. Exactly the failure `figm` was written
      // to catch on the body ("every WIDTH within a few percent of ANSUR and
      // every LENGTH and DEPTH short"), reproduced inside the hand. The reach
      // clamp above has to move with it or it silently eats the change.
      // The trigger hand's stand-off is smaller than the support hand's for a
      // reason that is measured rather than anatomical: in the ADS frame the
      // trigger arm rendered 7184 px of bare glove and ZERO sleeve px, because
      // its cuff sat below the bottom edge. One arm blue and one arm flesh is
      // not a pair however good each is on its own, and the fix has to shorten
      // the visible GLOVE rather than the forearm — the forearm is already
      // mostly off screen.
      // SOLVED AGAINST `vmhand`, NOT AGAINST RENDERS. Four numbers decide
      // whether a hand looks KINKED at the wrist, and none can be read off a
      // screenshot because foreshortening hides all four: the WRIST BEND (palm
      // axis to forearm axis — a firing grip runs 15-25 degrees, 0 is a splint,
      // 45 is the original kink), the FOREARM-TO-BORE angle (a real support arm
      // sits 40-55 degrees off the weapon's axis; 90 is the perpendicular clamp
      // that reads as "awkward"), the PALM CHORD (ANSUR p50 is 116 mm) and the
      // wrist's STAND-OFF from the grip SURFACE (55-70 mm; more and the hand
      // sits on a stalk).
      //
      // Before this sweep the support hand measured bend 35.5 / bore 54.5 /
      // palm 94.6 / off 92.0 — the bore angle had been bought by bending the
      // wrist and standing the hand off the weapon, which is why it kept
      // reading as kinked after two rounds of fixing it. The lever that costs
      // nothing is `axo`: sliding the wrist BACK along the grip axis tilts the
      // palm's own axis rearward, so the forearm leaves at the right angle
      // without the wrist doing any of the work. Now 18.5 / 48.8 / 97.8 / 72.
      float lift = h == 0 ? 0.0700f : 0.0660f;
      // How far DOWN the grip axis the wrist sits. On a pistol grip this is
      // what decides whether the arm comes from below or from behind: the
      // knuckle row is at axial 0 by construction, so a wrist 20 mm down the
      // grip gives a palm that runs almost horizontally and a forearm that then
      // has to leave sideways across the frame. At 48 mm the palm climbs the
      // back of the grip the way a real one does and the arm falls away
      // down-and-back on its own, which is also what lets WRIST_EXT come down.
      float axo  = h == 0 ? -0.048f : -0.046f;
      if (h == 1) { lift += 0.004f * mgv; axo += 0.010f * mgv; }
      hd[h].gc = gc; hd[h].ga = ga; hd[h].gr = gr;
      hd[h].side = h == 0 ? 1.0f : -1.0f;       // right hand on the grip
      hd[h].wrist = v3_add(gc, v3_add(v3_scale(ga, axo),
                    v3_scale(br, gr + lift)));
    }
    g_vm_hand_p[0] = hd[0].wrist;
    g_vm_hand_p[1] = hd[1].wrist;
    for (int h = 0; h < 2; h++) {
      v3 wv = v3_sub(hd[h].wrist, hd[h].gc);
      v3 r = v3_sub(wv, v3_scale(hd[h].ga, v3_dot(wv, hd[h].ga)));
      g_vm_hdbg[h].off_mm = (v3_len(r) - hd[h].gr) * 1000.0f;
    }
    for (int h = 0; h < 2; h++) {
      mat_set(MAT_GLOVE_R, -2.0f);   // padded glove, see the world shader
      v3 pd;
      // A hand that has left the grip for the bolt is not on the trigger, and
      // routing its index at a blade 60 mm away would stretch the digit across
      // the receiver.
      // Trigger hand fires (1) unless it left for the bolt knob (0 = wrap);
      // support hand holds the forward-thumb HEEL grip (2) on the fore-end
      // and closes the full C (0) once it travels to the magazine.
      int style = h == 0 ? (ga_.bhand < 0.35f ? 1 : 0) : (mgv > 0.5f ? 0 : 2);
      int hand_mark = g_scene_floats;
      fig_hand(hd[h].wrist, hd[h].gc, hd[h].ga, hd[h].gr, hd[h].side, glove,
               style, style == 1 ? &g_gun_trig_p : NULL, &pd);
      g_scene_floats = hand_mark;   // the sculpted glove replaces the geometry
      g_vm_hdbg[h].palm_mm = g_fig_palm_mm;
      mat_set(MAT_CLOTH_R, 0.0f);
      // THE FOREARM'S DIRECTION IS DERIVED FROM THE PALM'S, and that is the
      // whole wrist. It used to be authored independently — an elbow offset in
      // gun-local space, chosen so the arm left the frame nicely — while the
      // palm ran wherever the grip put it, and the two disagreed by 39 degrees
      // (support) and 53 (trigger). fig_chain then straddled that corner with a
      // bevel pair and the result was the hard, angular kink the whole arm read
      // as: a hand stuck on the end of a pipe at right angles.
      //
      // THE WRIST BEND IS AN ANGLE, NOT A LERP, and that distinction is the
      // whole defect. Easing `-pd` toward a straight-down vector buys however
      // many degrees happen to separate those two directions at that hold —
      // measured, the span is 13.85 degrees on the support hand and 57.5 on the
      // trigger hand, so the same constant 0.24 meant 3.4 degrees on one wrist
      // and 0.30 meant 16.4 on the other. The knob was SATURATED on the hand the
      // player actually looks at: the support wrist bent 3.4 degrees, which is a
      // splint, and the forearm therefore inherited the palm's own 79-degree
      // sweep across the fore-end and came at the rifle from directly below at
      // 77 degrees off the bore. A real support arm sits 40-55 degrees off it.
      // That perpendicular approach IS the "awkward".
      //
      // Two axes, because a wrist has two. EXTENSION (dorsiflexion) about the
      // palm's own breadth axis — the knuckle-row direction — and DEVIATION
      // about the palm normal: radial on the support hand, ulnar on the trigger
      // hand, which is exactly what the two grips ask of a real wrist. Expressed
      // this way the two hands' numbers are comparable quantities instead of
      // accidents of how far `down` happened to be from `pd` at each hold.
      v3 bx = v3_sub(hd[h].ga, v3_scale(pd, v3_dot(hd[h].ga, pd)));
      bx = v3_dot(bx, bx) > 1e-8f ? v3_norm(bx) : v3_norm(v3_cross(pd, gx));
      v3 bn = v3_norm(v3_cross(pd, bx));            // out of the palm
      float ext = (h == 0 ? 18.0f : 10.0f) * DEG2RAD;
      float dev = (h == 0 ? 15.0f : -6.0f) * DEG2RAD;
      v3 back = v3_rot_axis(v3_scale(pd, -1.0f), bx, hd[h].side * ext);
      back = v3_norm(v3_rot_axis(back, bn, hd[h].side * dev));
      // AND THEN LEAN IT BACK. Getting the wrist angle right left the support
      // forearm hanging plumb under the rifle: measured on the hip frame its
      // screen axis went 19.4 degrees off vertical to 7.4 — twelve degrees the
      // WRONG way, straight back toward the perpendicular approach the wrist
      // work was supposed to cure. A shoulder is behind the weapon, not under
      // it, so the arm has to carry a rearward component that no rotation of the
      // palm's own axis can produce.
      back = v3_norm(v3_add(back, v3_scale(gz, h == 0 ? -0.16f : -0.30f)));
      g_vm_hdbg[h].bend_deg = acosf(f_clamp(v3_dot(v3_scale(pd, -1.0f), back),
                                            -1.0f, 1.0f)) / DEG2RAD;
      g_vm_hdbg[h].bore_deg = acosf(f_clamp(fabsf(v3_dot(back, gz)),
                                            0.0f, 1.0f)) / DEG2RAD;
      // A WRIST, and this is the change that made the hands exist at all. The
      // old forearm was 54 x 58 mm at the wrist against a palm 78 x 31 — the arm
      // was nearly twice as THICK as the hand it ended in, so from every angle
      // the hand was a nub on a cone. ANSUR II: wrist breadth 58 mm, wrist
      // thickness 42, forearm max 80 x 62. A hand reads by the NOTCH between it
      // and the arm; without that notch nothing else in here matters.
      //
      // Node 0 sits 26 mm INSIDE the palm, along the PALM's own axis rather than
      // the arm's, so the sleeve's end cap is buried in a cross-section it
      // actually fits inside; node 1 is the wrist itself and is now an almost
      // straight node rather than the chain's one hard corner.
      // Nodes 1-2 are the CUFF: a sleeve running straight into a glove at full
      // value has a hard hue step across the wrist with no garment edge at it,
      // and that edge is 40 cm from the camera for the whole match.
      // The wrist ring has to fit inside the PALM at the station its bevel pair
      // lands on, not merely at the node — see the blue chevron that survived
      // two attempts to fix it at the node.
      // A MONOTONE ramp, and only three steps in it. Measured down the sleeve
      // there were three hard albedo bands at luminance 53 / 74 / 41 running
      // hand -> elbow: a +31% step over 12 px followed by a -48% step over 6 px,
      // i.e. a rugby sock whose brightest band was the middle of the forearm.
      // Worse, the arm's strongest INTERNAL value step measured 1.98x while its
      // silhouette contrast against the background was 1.19x — the banding was
      // four times stronger in stops than the shape's own edge, which is exactly
      // why the arm read as stripes rather than as an arm. The dark first bands
      // were the old painted "cuff"; that job now belongs to the gauntlet below,
      // so the sleeve starts at full value and only falls.
      //
      // The far end also has to taper FASTER than perspective widens it. On
      // screen the arm ran 56 px at the cuff to 89 px at the bottom edge with a
      // second derivative of zero — a linear cone. Two nodes past the belly, each
      // stepping down harder than the last, bend that profile back.
      // THE WRIST IS A LEDGE, and that ledge was most of "the transition is
      // very angular". At the wrist station the palm ring was 33.2 x 20.5 and
      // the arm ring 28.6 x 16.0 — the arm entered the hand 4.6 mm narrower
      // and 4.5 mm thinner all the way round, at the same point in space, so the palm's
      // proximal ring stood proud as a collar and the arm read as a pipe pushed
      // into a cup. Real anatomy runs the other way: at the wrist crease the hand
      // is ~62 x 30 mm and the styloid region 20 mm proximal is 58 x 42, i.e. the
      // THICKNESS increases monotonically from hand into forearm and there is no
      // notch in that axis at all. The notch is entirely a BREADTH phenomenon —
      // 88 mm across the knuckles falling to 58 at the styloids.
      // The two rings are also no longer at the same station: the arm's first
      // visible ring sits 8 mm proximal of the palm's, so they stop competing.
      //
      // The profile past it is one smooth swell rather than three corners. The
      // old chain reached 78% of its total flare within 90 mm of the wrist
      // against a real forearm's 47%, in band half-angles of 7.1 / 5.3 / 1.1 /
      // -4.7 degrees — a funnel with three visible silhouette corners in 250 mm
      // of arm. ANSUR: wrist 58 x 42, forearm max 80 x 62.
      // THE CUFF IS A FAT NODE OF THIS CHAIN, not a nested one. As its own
      // chain it had to cross the sleeve's surface to bury its far cap, and two
      // 12-gons whose rings are parallel-transported from DIFFERENT first bones
      // are angularly out of phase — so the intersection came out as a zig-zag
      // torn edge round the whole wrist. This file already learned that on the
      // helmet brim: prefer a fat node to a nested chain whenever the host's
      // centre is not constant. One surface, one silhouette, no crossing.
      //
      // THE WRIST IS ALSO NO LONGER A LEDGE. At the wrist station the palm ring
      // was 33.2 x 20.5 and the arm ring 28.6 x 16.0 — the arm entered the hand
      // 4.6 mm narrower and 4.5 mm thinner all the way round, so the palm's
      // proximal ring stood proud as a collar and the arm read as a pipe pushed
      // into a cup. Real anatomy: at the wrist crease the hand is ~62 x 30 mm
      // and the styloid region 20 mm proximal is 58 x 42, i.e. the THICKNESS
      // increases monotonically from hand into forearm and the notch is entirely
      // a BREADTH phenomenon.
      //
      // Past the cuff it is one smooth swell. The old chain reached 78% of its
      // total flare within 90 mm of the wrist against a real forearm's 47%, in
      // band half-angles of 7.1 / 5.3 / 1.1 / -4.7 degrees — a funnel with three
      // visible silhouette corners in 250 mm of arm. ANSUR: wrist 58 x 42,
      // forearm max 80 x 62.
      // ONE FOREARM, and the third-person body builds the same one — see
      // fig_forearm. `back` above is this view's contribution: the wrist's own
      // two rotations off the palm axis. NULL elbow, because a viewmodel arm
      // simply leaves the frame at its authored length.
    }
    g_md_skin = 0;
    // Hands and arms are sculpted around the rifle, in its frame. Each follows the
    // grip point the hold logic worked out, so reloads, the bolt and the sniper's
    // different holds carry them along as one rigid piece.
    g_md_gear = armory_gear();
    {
      #define VP(a, b, c) v3_add(org, v3_add(v3_scale(gx, a), \
                          v3_add(v3_scale(gy, b), v3_scale(gz, c))))
      #define VD(a, b, c) v3_add(v3_scale(gx, a), v3_add(v3_scale(gy, b), v3_scale(gz, c)))
      v3 rx = gx, ry = gy, rz = gz;
      v3 ro = v3_sub(hd[0].gc, VP(0.0f, -0.0500f, -0.0078f));
      // The grenade hand is sculpted round the grenade in this same frame, so it
      // takes no grip offset: applying one drew it 37 mm off the thing it holds.
      if (cur == WPN_GR) ro = (v3){{0, 0, 0}};
      if (cur == WPN_SR && ga_.bhand > 0.0f) {
        float blift, bdraw;
        gun_bolt_phase(ga_.bolt, &blift, &bdraw);
        float ba = blift * 1.05f, cb = cosf(ba), sb = sinf(ba);
        float kx = 0.0328f * cb + 0.0118f * sb;
        float ky = 0.0340f + 0.0328f * sb - 0.0118f * cb;
        float kz = -0.0370f - bdraw * 0.038f;
        rot2(&rx, &ry, -0.32f * ga_.bhand);
        rot2(&rx, &rz,  0.22f * ga_.bhand);
        rot2(&rz, &ry, -0.16f * ga_.bhand);
        v3 k_target = VP(kx + 0.014f, ky - 0.012f, kz + 0.006f);
        v3 g_local = v3_add(v3_scale(ry, -0.0520f), v3_scale(rz, 0.0080f));
        v3 o_bolt = v3_sub(k_target, g_local);
        v3 ro_rest = VD(0.0f, -0.0010f, -0.0366f);
        ro = v3_lerp(ro_rest, v3_sub(o_bolt, org), ga_.bhand);
      }
      // The pistol's support hand was sculpted in the pistol's own frame, so its
      // rest offset is the station itself; the rifles' hand is shared and keeps
      // the handguard station it was modelled on.
      const float *hrest = GUN_HOLD[cur][GH_FORE];
      v3 lo = v3_sub(hd[1].gc, ptl ? VP(hrest[0], hrest[1], hrest[2])
                                   : VP(0.0f,  0.0190f,  0.2500f));
      v3 or_p = v3_add(org, ro);
      int knf = fistw;
      // THE KNIFE FIST IS SMALLER THAN THE RIFLE HAND IT WAS CUT FROM. Sculpted
      // round a rifle grip in a padded glove, it read twice the girth of the
      // 30 mm handle it holds. It shrinks about the handle's own axis, so it
      // still closes on the handle; the fingers' inner face sinks into the
      // grip by under 2 mm, out of sight inside the fist.
      float kfs = cur == WPN_KN ? KNIFE_FIST_S : 1.0f;
      v3 or_h = or_p;
      if (cur == WPN_KN)
        or_h = v3_add(or_p, v3_scale(v3_add(v3_scale(ry, -0.0371f), v3_scale(rz, -0.0024f)),
                                     1.0f - kfs));
      if (cur == WPN_GR) model_draw(HAND_G_V, HAND_G_P, HAND_G_NP, 0, or_p, rx, ry, rz);
      else if (knf) {
        g_md_scale = kfs;
        model_draw(HAND_K_V, HAND_K_P, HAND_K_NP, 0, or_h, rx, ry, rz);
        g_md_scale = 1.0f;
      }
      else     model_draw(HAND_R_V, HAND_R_P, HAND_R_NP, 0, or_p, rx, ry, rz);
      // The knife's off hand is only in frame in the guard: it rises to brace the
      // flat of the blade, palm in, and drops away below the screen otherwise.
      float gk = smooth01(ga_.guard);
      v3 klx = rgt, kly = upv, klz = fwd, klo = eye;
      // THE KNIFE'S GUARD IS AN OPEN HAND. It used to be the knife fist mirrored
      // — a closed fist gripping nothing, 30 cm from the eye and low enough that
      // its gauntlet's amber window was the biggest thing in the frame. A
      // fighter's lead hand is open: fingers up and together, palm turned toward
      // the threat, so the camera sees the back of the glove. It rises from
      // below the frame on the guard ramp and stands well out to the left, where
      // it frames the blade instead of covering the target.
      v3 kshoulder = v3_add(eye, v3_add(v3_scale(rgt, -0.22f),
                     v3_add(v3_scale(upv, -0.46f), v3_scale(fwd, 0.02f))));
      if (cur == WPN_KN && gk > 0.01f) {
        float dn = 1.0f - gk;
        rot2(&kly, &klz, 0.48f - 0.55f * dn);   // fingers lean toward the threat
        rot2(&klz, &klx, 0.72f);                // palm turned in, toward the centre
        rot2(&klx, &kly, -0.16f);               // fingertips cant toward the blade
        klo = v3_add(eye, v3_add(v3_scale(rgt, -0.165f + 0.03f * dn),
              v3_add(v3_scale(upv, -0.200f - 0.22f * dn), v3_scale(fwd, 0.40f - 0.06f * dn))));
        model_draw(HAND_KO_V, HAND_KO_P, HAND_KO_NP, 0, klo, klx, kly, klz);
      } else if (cur == WPN_GR) {
        // The pin hand: its frame was solved from the ring above (pin_o / ph_*).
        if (pin_on) {
          klx = ph_x; kly = ph_y; klz = ph_z; klo = ph_o;
          model_draw(HAND_GP_V, HAND_GP_P, HAND_GP_NP, 0, klo, klx, kly, klz);
        }
      } else if (knf && gk > 0.01f) {
        rot2(&klz, &kly, -0.60f + 0.30f * (1.0f - gk));
        rot2(&klx, &klz, 0.20f);
        rot2(&klx, &kly, -0.45f);
        klo = v3_add(eye, v3_add(v3_scale(rgt, -0.080f),
              v3_add(v3_scale(upv, -0.080f - 0.230f * (1.0f - gk)), v3_scale(fwd, 0.300f))));
        model_draw(HAND_KL_V, HAND_KL_P, HAND_KL_NP, 0, klo, klx, kly, klz);
      } else if (cur == WPN_PT) {
        model_draw(HAND_P_V, HAND_P_P, HAND_P_NP, 0, v3_add(org, lo), gx, gy, gz);
      } else if (!knf) {
        model_draw(HAND_L_V, HAND_L_P, HAND_L_NP, 0, v3_add(org, lo), gx, gy, gz);
      }
      for (int h = 0; h < 2; h++) {
        if (h == 1 && knf && (cur == WPN_GR ? !pin_on : gk <= 0.01f)) continue;
        const v3 gr_wr = HAND_G_WRIST, gr_ax = HAND_G_AX;
        v3 wr = h == 0
          ? cur == WPN_GR
            ? v3_add(or_p, v3_add(v3_scale(rx, gr_wr.x),      // the grenade hand's own wrist
                           v3_add(v3_scale(ry, gr_wr.y), v3_scale(rz, gr_wr.z))))
            : cur == WPN_KN
            ? v3_add(or_h, v3_scale(v3_add(v3_scale(rx, 0.018f),
                           v3_add(v3_scale(ry, -0.080f), v3_scale(rz, -0.066f))), kfs))
            : v3_add(or_p, v3_add(v3_scale(rx, 0.018f),
                           v3_add(v3_scale(ry, -0.080f), v3_scale(rz, -0.066f))))
          : (cur == WPN_KN || cur == WPN_GR) ? v3_add(klo, v3_add(v3_scale(kly, -0.024f), v3_scale(klz, -0.003f)))
          : knf ? v3_add(klo, v3_add(v3_scale(klx, -0.018f),
                         v3_add(v3_scale(kly, -0.080f), v3_scale(klz, -0.066f))))
          : ptl ? v3_add(VP(-0.038f, -0.102f, -0.018f), lo)
                : v3_add(VP(-0.031f, -0.035f, 0.198f), lo);
        // A rifle pins the arm's angle; a swinging knife does not, and a forearm
        // rigid to the hand sweeps straight through the camera on every cut. The
        // knife arm aims at the shoulder instead, so it always leaves the frame.
        // THE GRENADE WRIST IS STRAIGHT: the forearm runs out along the hand's
        // own anatomical forearm line (HAND_G_AX in the sculpt), so neither the
        // hold nor the throw kinks the wrist or sweeps the vambrace across the lens.
        v3 shd = v3_norm(v3_sub(v3_add(eye, v3_add(v3_scale(rgt, 0.30f),
                         v3_add(v3_scale(upv, -0.42f), v3_scale(fwd, -0.10f)))), wr));
        if (h == 0 && cur == WPN_GR && gr_arm > 0.0f) {
          v3 hax = v3_norm(v3_add(v3_scale(rx, gr_ax.x), v3_add(v3_scale(ry, gr_ax.y), v3_scale(rz, gr_ax.z))));
          shd = v3_norm(v3_add(v3_scale(shd, 1.0f - gr_arm), v3_scale(hax, gr_arm)));
        }
        v3 ax = v3_norm(h == 0
          ? (knf ? shd
                 : v3_add(v3_scale(rx, 0.32f), v3_add(v3_scale(ry, -0.66f), v3_scale(rz, -0.68f))))
          : cur == WPN_KN ? v3_sub(kshoulder, wr)
          : cur == WPN_GR ? v3_scale(kly, -1.0f)      // the pin hand's wrist is straight
          : knf ? v3_add(v3_scale(klx, -0.32f), v3_add(v3_scale(kly, -0.66f), v3_scale(klz, -0.68f)))
          : ptl ? VD(-0.50f, -0.52f, -0.69f) : VD(-0.41f, -0.82f, -0.39f));
        v3 te = v3_sub(eye, wr);                 // the armour faces the eye
        v3 on = v3_norm(v3_sub(te, v3_scale(ax, v3_dot(te, ax))));
        if (h == 0) g_md_scale = kfs;            // the knife arm shrinks with its fist
        model_draw(BRACER_V, BRACER_P, BRACER_NP, 0, wr, ax, on, v3_cross(ax, on));
        g_md_scale = 1.0f;
        if (h == 1 && !knf && cur != WPN_GR) {
          // The wrist display, laid over the bracer's amber window (that part
          // spans x 41..61 mm, z +-10.8 mm, its top at 40.8 mm in model units).
          v3 az = v3_cross(ax, on);
          v3 c0 = v3_add(wr, v3_add(v3_scale(ax, 0.0510f), v3_scale(on, 0.0413f)));
          v3 eu = v3_scale(ax, 0.0094f), ev = v3_scale(az, 0.0100f);
          mat_set(0.3f, -8.0f);
          v3 P4[4] = {v3_sub(v3_sub(c0, eu), ev), v3_sub(v3_add(c0, eu), ev),
                      v3_add(v3_add(c0, eu), ev), v3_add(v3_sub(c0, eu), ev)};
          // UV: u along the forearm reads left-to-right from the eye, v up.
          v3 U4[4] = {{{1, 1, 0}}, {{1, 0, 0}}, {{0, 0, 0}}, {{0, 1, 0}}};
          int ord[2][6] = {{0, 1, 2, 0, 2, 3}, {0, 2, 1, 0, 3, 2}};
          for (int w2 = 0; w2 < 2; w2++)
            for (int k = 0; k < 6; k++) scene_push(P4[ord[w2][k]], on, U4[ord[w2][k]]);
          mat_set(MAT_PARK_R, MAT_PARK_M);
        }
      }
      #undef VP
      #undef VD
    }
    g_md_gear = NULL;
    mat_set(MAT_PARK_R, MAT_PARK_M);
    probe(NULL);
  }
  if (cur == WPN_SR) {
    // The scope disc is DERIVED from the ocular, not tuned: project the ocular's
    // glass radius at its own distance through the viewmodel's own projection, so
    // the picture fills the lens exactly in every pose and travels with the scope
    // as it comes up instead of fading in at a fixed screen spot.
    v3 ocu = v3_add(org, v3_add(v3_scale(gy, sight_y),
                                v3_scale(gz, SCOPE_OCU_Z)));
    g_gun_sight_p = ocu;
    g_gun_sight_r = SCOPE_OCU_R;
    v3 rel = v3_sub(ocu, eye);
    float oz = v3_dot(rel, fwd);
    if (oz > 0.02f) {
      // Work in the HUD's own virtual units (ui_scale, the same integer scale
      // ui_begin lays the HUD out with), derived from the real viewport rather
      // than assumed to be 360 tall: a disc sized for 360 lands above the lens
      // and half its size at any other height.
      float sc = ui_scale();
      float vh = (float)R.height / sc, vw = (float)R.width / sc;
      float f = (vh * 0.5f) / tanf(VM_FOV_DEG * 0.5f * DEG2RAD);
      g_scope_r = (SCOPE_OCU_R - 0.0016f) / oz * f;
      g_scope_cx = 0.5f * vw + v3_dot(rel, rgt) / oz * f;
      g_scope_cy = 0.5f * vh - v3_dot(rel, upv) / oz * f;
    } else {
      g_scope_r = 0.0f;
    }
  }
  #undef VLERP
}

