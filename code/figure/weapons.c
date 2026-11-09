// code/figure/weapons.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// --- the rifle: ONE model, built in gun-local space (x right, y up, z forward,
// origin at the receiver rear) and placed by whoever is holding it. -----------
//
// The viewmodel and every third-person figure call THIS. The bot's rifle used to
// be seven boxes with their own dimensions — a second model of the same weapon,
// free to drift from the one the player stares at all day, and it did: the thing
// in a bot's hands was a different length, a different profile and had no optic
// worth the name. Same reasoning as one locomotion path and one hitscan.
//
// Round parts are CHAINS — the same watertight swept prism the body is built
// from (closed, correctly bevelled corners, no coplanar pairs by construction,
// smooth-shaded because the profile is round). Barrels, tubes, grips, magazines
// and the scope are round objects and boxes were always the wrong primitive for
// them; only genuinely flat-sided parts (receivers, rails, pads) stay boxes.
//
// Placement discipline: neighbouring parts INTERPENETRATE by >=1 mm with
// different cross-sections (hidden faces, no coplanar pairs) — nothing floats,
// nothing shares a plane. Returns the world-space muzzle tip.

// Sight line height above the gun origin — the ADS pose puts exactly this on
// the camera axis, so the HUD dot/reticle lands in the aperture's center.
#define VM_SIGHT_AR 0.062f
#define VM_SIGHT_SR 0.085f

// Where the trigger hand grips, in gun-local space: what a third-person figure
// anchors the model to, so the hands land on the gun instead of near it.
static const float GUN_GRIP[WPN_COUNT][3] = {{0.0f, -0.034f, 0.006f},    // AR
                                             {0.0f, -0.038f, -0.036f},   // SR
                                             {0.0f, -0.034f, 0.006f},    // pistol
                                             {0.0f, -0.034f, 0.006f},    // knife
                                             {0.0f, -0.034f, 0.006f}};   // grenade

// Barrel tip in gun-local space, one row per weapon. gun_build places its
// muzzle geometry at this point and the tracer/flash spawn reads the same row
// through pose_muzzle, so a streak's origin cannot drift from the barrel the
// renderer actually draws — the drift it replaces was 1.6 m of "in front of
// the camera", which read as shooting from the eye.
// Third-person rifle stylisation scale — see the note at g_gun_girth below.
#define GUN_GIRTH_3P 1.05f
static const float GUN_MUZ[WPN_COUNT][3] = {{0.0f, 0.0225f, 0.494f},     // AR
                                            {0.0f, 0.016f,  0.498f},     // SR
                                            {0.0f, 0.0060f, 0.150f},     // pistol
                                            {0.0f, 0.1931f, 0.0941f},    // knife tip
                                            {0.0f, 0.0f, 0.0f}};         // grenade

// HOW FAR THE MODEL HANGS BELOW ITS OWN ORIGIN, in model units, per weapon.
// The origin sits at the receiver; the magazine and the floor plate reach a
// quarter of a metre under it. A corpse LAYS ITS RIFLE DOWN, and placing that
// origin at "the ground plus 85 mm" therefore buried the magazine 235 mm in
// the floor on every corpse in the game, on every surface, for the corpse's
// whole life (ragx: clr = -0.235 <gun>).
// MEASURED off the emitted geometry the first time each weapon is built with
// its magazine home, not written down: a hand-typed constant here is a second
// copy of the magazine's length, and this file's own history says which of the
// two copies drifts. Filled by scene_draw_figure / the viewmodel long before
// any corpse exists, so it costs one predictable branch per figure and one
// scan per weapon per session.
static float g_gun_low[WPN_COUNT];

// The rifle model's frame from a solved pose: exactly the anchor
// scene_draw_figure uses to place gun_build so the trigger hand lands on the
// pistol grip. Factored out because the VFX spawn needs the same muzzle the
// renderer draws — a second copy of these offsets is how a tracer ends up
// leaving a point beside the weapon.
static float gun_sight_y(int cur);   // defined with the weapon builder below
static void pose_gun_frame(const pose_t *P, int wpn,
                           v3 *org, v3 *rt, v3 *up, v3 *fw) {
  *fw = v3_norm(P->gun_f);
  *up = v3_norm(P->gun_u);
  *rt = v3_norm(v3_cross(*fw, *up));
  const float *gp = GUN_GRIP[wpn];
  // WHERE THE RIFLE HANGS OFF THE POSE'S GUN ANCHOR, and it is not a taste
  // number: it decides whether the third-person hand is ON the grip at all.
  // At (-0.070, -0.045) the trigger wrist landed 26.2 mm from the grip AXIS
  // against fig_hand's own 39.2 mm floor, so the reach clamp fired on EVERY
  // third-person frame and slid the whole grip cylinder 13 mm out from under
  // the rifle — the palm and all five digits were then built round a cylinder
  // the weapon does not draw, and the trigger guard measured 18.5 mm inside the
  // fist. The lever is the direction PERPENDICULAR to the grip's own axis
  // (0, 0.921, 0.386): moving along it is what changes the wrist's distance
  // from the axis, where moving along `up` alone mostly slides the anchor down
  // the axis and changes nothing. Solved against `vmtrig`'s wrist/clamp columns
  // — 54.2 mm and clamp=0 in bot 0's spawn pose, 73-80 mm running, crouching and
  // backpedalling, against the 55-70 mm band `vmhand` holds the viewmodel to and
  // a hard floor of 39.2 where the clamp fires — and filmed at four stations to
  // confirm the carriage still reads as a high ready rather than a rifle slung
  // low.
  v3 gw = v3_add(P->gun_o, v3_add(v3_scale(*fw, 0.005f),
                                  v3_scale(*up, -0.098f)));
  // TWO WEAPON-AWARE SEATS, and this is the only place that can apply them:
  // `skel_solve` does not know which rifle it is holding, and the two rifles
  // differ in exactly the two ways a cheek weld cares about.
  //
  // ALONG THE BORE — where the butt pad ends up. Body-space, the pad sits at
  // gw + fw*(butt_z*girth - grip_z), and the two weapons put that 22.7 mm
  // apart: measured with `figbury-butt`, the AR's pad is at model -0.2220 and
  // the sniper's at -0.2480, and their grip stations differ by another 50 mm
  // the other way. `figbury` saw the result — the AR biting 10 mm into the
  // deltoid and the sniper's pad 6.8 mm CLEAR of the body, i.e. one stock in
  // the pocket and one floating in front of it. A stock goes in the pocket
  // whichever rifle it belongs to.
  //
  // ALONG UP — where the SIGHT LINE ends up. The sniper's glass sits 23 mm
  // higher over the bore than the carbine's aperture, so with both weapons
  // held at the same height the sniper's sight line ran 24 mm above the eye:
  // the `weld` rows measured eye_up +21 mm on the AR and -7 on the sniper
  // standing, -8 and -36 crouched, from the same head. Mounting an optic
  // makes a rifle ride LOWER in the hands, and that is what this is.
  static const float GUN_BUTT_Z[WPN_COUNT] = {-0.2220f, -0.2480f, 0, 0, 0};  // figbury-butt
  #define GUN_SEAT_(w, gz) (GUN_BUTT_Z[w] * GUN_GIRTH_3P - (gz))
  float seat = GUN_SEAT_(WPN_AR, GUN_GRIP[WPN_AR][2]) - GUN_SEAT_(wpn, gp[2]);
  #undef GUN_SEAT_
  float rise = -(gun_sight_y(wpn) - VM_SIGHT_AR) * GUN_GIRTH_3P;
  gw = v3_add(gw, v3_add(v3_scale(*fw, seat), v3_scale(*up, rise)));
  // A SIDEARM HAS NO STOCK, so the shoulder-pocket arithmetic above means nothing
  // to it. It rides out on both arms, centred under the eye; tracers and flashes
  // read this same frame through pose_muzzle.
  if (wpn == WPN_PT)
    gw = v3_add(P->gun_o, v3_add(v3_scale(*fw, 0.135f),
                 v3_add(v3_scale(*up, -0.045f), v3_scale(*rt, -0.080f))));
  *org = v3_sub(gw, v3_add(v3_scale(*rt, gp[0]),
               v3_add(v3_scale(*up, gp[1]), v3_scale(*fw, gp[2]))));
}

// HOW FAR THE HEAD IS CANTED, in radians, positive = crown toward the strong
// side. It lives on the ANIM rather than in `pose_t` because pose_t is asserted
// to be a flat array of v3 (the death topple rotates the whole struct as one),
// and a scalar cannot ride in there — and it is a function of the blend states
// anyway, so the draw and the proof can each ask for it rather than store it.
//
// A shooter lays his CHEEK on the stock; he does not hold his head plumb over a
// weapon that is 30 degrees off his shoulder line. Half the blade angle is the
// honest number: the shoulder line turns 30 degrees under `blade` and the head
// follows about half of that, which is what puts the eye over the sight instead
// of beside it. Scaled by ads_s so a walking figure keeps his head level.
static float anim_head_cant(const anim_t *A) {
  return 0.26f * A->ads_s;
}

// THE HEAD'S OWN FRAME, and the EYE in it — one copy, because the renderer
// builds this basis to draw the skull and the weld proof has to measure the
// same one or it is measuring a different head. `anim_head_cant` supplies the
// cant; everything else here is the draw's own construction.
// The eye sits 18 mm above and 62 mm forward of the head node: the visor's
// centre node is at (0, 0.020, 0.080) in this frame, and an eye is behind the
// lens, not on it.
static void pose_head_frame(const pose_t *P, float cant,
                            v3 *eye, v3 *hr, v3 *hu, v3 *hf) {
  v3 sp = v3_sub(P->chest, P->pelvis);
  sp = v3_dot(sp, sp) > 1e-8f ? v3_norm(sp) : (v3){{0, 1, 0}};
  v3 f = v3_norm(P->head_f);
  v3 r = v3_cross(f, sp);
  r = v3_dot(r, r) > 1e-6f ? v3_norm(r) : v3_norm(P->right);
  if (fabsf(cant) > 1e-4f) r = v3_norm(v3_rot_axis(r, f, cant));
  v3 u = v3_cross(r, f);
  if (hr) *hr = r;
  if (hu) *hu = u;
  if (hf) *hf = f;
  if (eye) *eye = v3_add(P->head, v3_add(v3_scale(u, 0.018f), v3_scale(f, 0.062f)));
}

static v3 pose_muzzle(const pose_t *P, int wpn) {
  v3 org, rt, up, fw;
  pose_gun_frame(P, wpn, &org, &rt, &up, &fw);
  const float *m = GUN_MUZ[wpn];
  // The third-person rifle draws at GUN_GIRTH_3P, so the flash/tracer anchor
  // has to sit on the SCALED barrel tip or bursts detach from the muzzle.
  return v3_add(org, v3_add(v3_scale(rt, m[0] * GUN_GIRTH_3P),
               v3_add(v3_scale(up, m[1] * GUN_GIRTH_3P),
                      v3_scale(fw, m[2] * GUN_GIRTH_3P))));
}

// The two CYLINDERS a pair of hands closes around, in gun-local space:
// {cx, cy, cz, radius, ax, ay, az}. `fig_hand` builds a hand as arcs about a
// cylinder, so the weapon has to say where its grip surfaces are — and it has to
// say it ONCE, or the viewmodel and the third-person figure grip two different
// rifles, which is the same defect the single `gun_build` was written to kill.
// The axis points from the LITTLE finger toward the INDEX: forward along a
// handguard, up toward the trigger on a pistol grip. Radii track the chains in
// gun_build at those stations (AR handguard 0.0149 -> 0.0138 over z 0.185-0.300;
// AR grip 0.0121 -> 0.0112 over its first two nodes; likewise the SR) — a hand
// sized to a surface that is not there is a hand floating a millimetre off it.
#define GH_GRIP 0
#define GH_FORE 1
#define GH_RELD 2   // where the support hand goes mid-reload
// ...and the THIRD-PERSON support station, which is a different number for a
// reason that is not stylistic: the figure has ARMS and the viewmodel does not.
// The AR's fore-end station sits 0.503 model units ahead of the butt pad, drawn
// at GUN_GIRTH_3P that is 614 mm, and the figure's arm is 580 (ANIM_UARM +
// ANIM_FARM, from ANSUR II). With the buttstock where a buttstock belongs — in
// the shoulder pocket, in FRONT of the chest — there is no pose in which that
// hand arrives: the IK over-extends, points a dead-straight arm at a handguard
// it cannot touch, and fig_hand stretches the palm across the gap. Real
// shooters solve the same arithmetic by gripping nearer the magwell; so does
// this one. The viewmodel keeps GH_FORE, where the hand is placed directly and
// nothing has to reach.
#define GH_FORE3 3
// The radius is the surface a finger can actually touch, which on a 12-gon is
// the FLATS — cos(pi/12) = 0.966 of the authored half-extent — not the
// circumradius, and it is the smaller of the two half-extents at that station.
// Measured on the AR fore-end: 14.11 x 13.45 mm authored, 12.99 mm at the flats,
// so a hand sized to 14.10 mm floats 1.1 mm off the wood before any per-finger
// lift is added. At 0.4 m through a 62-degree viewport that is 3.5 px of daylight
// under the little finger, which is exactly the "resting near it" read a grip
// must not have.
static const float GUN_HOLD[WPN_COUNT][4][7] = {
  // THE PUBLISHED RADIUS IS THE ONE THE HAND CLOSES ON, so it has to be the
  // radius the grip ACTUALLY HAS along the row the fingers occupy — not an
  // independent number. Computed from the grip chain's own nodes, the surface
  // sits 12.8-13.9 mm from this axis over the finger row's span (s = -0.030 to
  // +0.0295); the authored 10.8 put the wrap 2 mm inside the polymer, so each
  // digit was buried to a third of its girth and the four of them read as small
  // blobs stuck to the side of the grip instead of as a hand closed round it.
  { { 0.0f, -0.0500f,  -0.0078f, 0.0130f,  0.0f, 0.921f,  0.386f },   // AR grip
    { 0.0f,  0.0190f,   0.2640f, 0.0123f,  0.0f, 0.0f,    1.0f   },   // AR fore-end
    // The magazine. A hand wraps its NARROW axis, so the radius is the small
    // half-extent of the mag chain at that station, not its 19 mm depth.
    // High on the magazine, right under the magwell, not at its middle. The
    // magazine hangs 196 mm below the bore and the viewmodel's frustum has
    // 180 mm of half-height at the weapon's own distance, so a hand at the mag's
    // centre works entirely below the bottom edge of the screen — an animation
    // nobody can see is an animation that does not exist.
    // A HAND CLOSES ACROSS A MAGAZINE'S NARROW DIMENSION. The radius here is
    // the mag's WIDTH half-extent (12.0 mm) plus a bite, not something between
    // its width and its 19.6 mm depth: `fig_hand` wraps a circle, so a radius
    // sized to the average left the fingers 11 mm clear of the flat sides and
    // the hand rendered BESIDE the magazine rather than round it. Closing on the
    // narrow axis buries the fingers in the deep one, which is what
    // interpenetration is for.
    { 0.0f, -0.0640f,   0.1010f, 0.0124f,  0.0f, 0.959f,  0.283f },   // AR mag
    // Third person: the REAR of the same handguard, just ahead of the delta
    // ring (the guard body spans z 0.196-0.330), which is 100 mm closer to the
    // butt than GH_FORE and exactly what buys the elbow its bend.
    { 0.0f,  0.0190f,   0.1780f, 0.0135f,  0.0f, 0.0f,    1.0f   } }, // AR fore, 3p
  // 10.4-14.1 mm measured the same way; 12.6 is the mean over the finger row.
  { { 0.0f, -0.0510f,  -0.0444f, 0.0126f,  0.0f, 0.900f,  0.436f },   // SR grip
    { 0.0f,  0.0130f,   0.2080f, 0.0121f,  0.0f, 0.0f,    1.0f   },   // SR fore-end
    // The sniper changes a magazine too now. Its box is straighter and wider
    // than the AR's (a .308 case is nearly parallel where a 5.56 curve is not),
    // so the hold is higher up the body, right under the magwell, and the axis
    // is almost vertical. Same rule as the AR's: the radius is the mag's NARROW
    // half-extent plus a bite, because a hand closes across the narrow axis and
    // lets the deep one bury the fingers.
    // 0.0640 and not 0.0518: at 52 mm the support palm's rear corner reached
    // back over the trigger guard's bottom bow and crossed it by 3.8 mm through
    // the middle of the reload — the SR's magazine sits almost directly under
    // its trigger group, where the AR's is 101 mm forward and clear by
    // construction. Measured with `vmtrig`'s rl55 station, which is why that
    // station exists.
    { 0.0f, -0.0480f,   0.0640f, 0.0130f,  0.0f, 0.9997f, -0.0256f },   // SR mag
    // The sniper's fore-end is already 100 mm nearer its butt than the AR's, so
    // the third-person station moves less; it still moves, because the sniper's
    // butt is further back again.
    { 0.0f,  0.0130f,   0.1480f, 0.0128f,  0.0f, 0.0f,    1.0f   } }, // SR fore, 3p
  // The pistol shares the AR's grip stations, which is why the sculpted firing
  // hand fits it unchanged. The support hand wraps that same grip from the front.
  { { 0.0f, -0.0500f,  -0.0078f, 0.0130f,  0.0f, 0.921f,  0.386f },   // PT grip
    { 0.0f, -0.0760f,   0.0020f, 0.0170f,  0.0f, 0.921f,  0.386f },   // PT cup
    { 0.0f, -0.1180f,  -0.0170f, 0.0124f,  0.0f, 0.921f,  0.386f },   // PT mag
    { 0.0f, -0.0760f,   0.0020f, 0.0170f,  0.0f, 0.921f,  0.386f } }, // PT cup, 3p
  // The knife's handle lies on the same cylinder. Its off-hand rows are unused
  // (that hand holds nothing); they stay valid because proofs walk every row.
  { { 0.0f, -0.0500f,  -0.0078f, 0.0130f,  0.0f, 0.921f,  0.386f },   // KN handle
    { 0.0f, -0.0500f,  -0.0078f, 0.0130f,  0.0f, 0.921f,  0.386f },
    { 0.0f, -0.0500f,  -0.0078f, 0.0130f,  0.0f, 0.921f,  0.386f },
    { 0.0f, -0.0500f,  -0.0078f, 0.0130f,  0.0f, 0.921f,  0.386f } },
  // The hand closes round the grenade's own body, seated forward of the handle
  // station by that body's radius.
  { { 0.0f, -0.0643f,   0.0263f, 0.0300f,  0.0f, 0.921f,  0.386f },   // GR body
    { 0.0f, -0.0643f,   0.0263f, 0.0300f,  0.0f, 0.921f,  0.386f },
    { 0.0f, -0.0643f,   0.0263f, 0.0300f,  0.0f, 0.921f,  0.386f },
    { 0.0f, -0.0643f,   0.0263f, 0.0300f,  0.0f, 0.921f,  0.386f } },
};
// The magazine's reload travel (down, camera-side) per weapon, published ONCE
// beside the grip surfaces for the same reason they are: the model (gun_build),
// the viewmodel's hand-follow (vm_build) and the third-person hand-follow all
// move a hand with this magazine, and the third copy had already drifted to a
// weapon-independent 0.058/0.033 — the exact hand/magazine decoupling the
// hand-follow was written to fix. Sites apply their own signs.
static const float GUN_MAG_TRAVEL[WPN_COUNT][2] = {
  [WPN_AR] = {0.060f, 0.034f},
  [WPN_SR] = {0.056f, 0.032f},
  [WPN_PT] = {0.082f, 0.0f},
};
// Where the support hand's magazine goes on its way to the belt pouch and back,
// in the gun's own frame per unit of `pouch`: well below the weapon, back
// toward the body, and a little toward the camera side.
static v3 gun_pouch_ofs(v3 gx, v3 gy, v3 gz, float pouch) {
  return v3_add(v3_scale(gy, -0.30f * pouch),
         v3_add(v3_scale(gz, -0.12f * pouch), v3_scale(gx, -0.05f * pouch)));
}
// THE POUCH IS ON THE BODY, NOT ON THE GUN. Its position is fixed in the
// shooter's own (camera) frame — the chest rig, low and a little left of the
// centre line, below the frame — and a hand-held magazine travels to it from
// wherever the well is, along an ARC that swings out and back from the
// weapon rather than a straight line through it. `wc` is the magazine's
// centre at the well; the return is the displacement to add.
static v3 vm_pouch_disp(v3 wc, v3 eye, v3 rgt, v3 upv, v3 fwd, float t) {
  v3 P = v3_add(eye, v3_add(v3_scale(rgt, -0.12f),
                     v3_add(v3_scale(upv, -0.50f), v3_scale(fwd, 0.20f))));
  v3 arc = v3_scale(v3_add(v3_scale(rgt, -0.09f), v3_scale(fwd, -0.06f)), sinf(F_PI * t));
  return v3_add(v3_scale(v3_sub(P, wc), t), arc);
}
// The magazine's own centre in the baked model (its bounds' middle).
// THE FAMAS IS A BULLPUP: its magazine sits BEHIND the grip, so the viewmodel's
// rig carries a per-variant override for the three things that hang off the
// magazine (its centre, the reload hold, the free fall) and for its sight: no
// optic, the iron sights inside the carry handle's window (post, aperture). Everything else — grip,
// trigger, fore-end — is on the AR's stations by construction (build_models.py).
#define RV_FAMAS 2
#define PV_DEAGLE 2
#define VM_FAMAS_IRON_Y  0.0600f  // the post tip and the aperture's centre, INSIDE
#define VM_FAMAS_IRON_Z -0.1400f  // the handle's window; the aperture at its back
#define VM_FAMAS_MUZ_Z   0.500f
// A bullpup's cheek weld is on the BUTT, behind the handle: aimed, the FAMAS sits
// this much further from the eye than the AR, so the handle's rear wall and its
// sight tunnel frame the view instead of filling it.
#define VM_FAMAS_ADS_PUSH 0.070f
#define VM_DEAGLE_MUZ_Z  0.196f
static v3 vm_mag_centre_v(int cur, int var) {
  if (cur == WPN_AR && var == RV_FAMAS) return (v3){{0.0f, -0.076f, -0.146f}};
  return cur == WPN_SR ? (v3){{0.0f, -0.066f, 0.053f}} : (v3){{0.0f, -0.097f, 0.127f}};
}
// The second magazine held in the same hand during a tactical reload: beside
// the first, forward along the bore and a little toward the camera.
static v3 gun_pair_ofs(v3 gx, v3 gz) {
  return v3_add(v3_scale(gz, 0.046f), v3_scale(gx, -0.016f));
}
// The spent magazine's free fall, for `t` seconds after release: gravity, a
// small push out of the well, and a forward tumble about its own centre.
// Returns the model origin to draw the magazine group at, and its basis.
static v3 gun_drop_pose(int cur, int var, v3 org, v3 gx, v3 *gy, v3 *gz, float t) {
  v3 L = vm_mag_centre_v(cur, var);
  v3 c = v3_add(org, v3_add(v3_scale(*gy, L.y), v3_scale(*gz, L.z)));
  v3 fall = {{0.0f, -(0.5f * 9.81f * t * t + 0.35f * t), 0.0f}};
  fall = v3_add(fall, v3_scale(gx, -0.18f * t));
  float a = 2.2f * t;
  float ca = cosf(a), sa = sinf(a);
  v3 ny = v3_add(v3_scale(*gy, ca), v3_scale(*gz, sa));
  v3 nz = v3_sub(v3_scale(*gz, ca), v3_scale(*gy, sa));
  *gy = ny; *gz = nz;
  return v3_sub(v3_add(c, fall), v3_add(v3_scale(ny, L.y), v3_scale(nz, L.z)));
}
// Third-person rifle stylisation: at 20 m the true-scale barrel is 1-2 px
// and the class-defining prop vanishes; the genre oversizes weapons x1.2-1.5.
// The FIGURE draws the same model UNIFORMLY scaled about the gun origin —
// positions and extents together, so every designed clearance scales with its
// parts and no coplanar pair can appear (radii-only scaling measurably did).
// The viewmodel keeps 1.0: every first-person contract is untouched.
static float g_gun_girth = 1.0f;   // declared above fig_hand, which reads it

static void gun_hold(int cur, int which, v3 org, v3 gx, v3 gy, v3 gz,
                     v3 *c, v3 *a, float *r) {
  const float *h = GUN_HOLD[cur][which];
  float gg = g_gun_girth;
  *c = v3_add(org, v3_add(v3_scale(gx, h[0] * gg),
       v3_add(v3_scale(gy, h[1] * gg), v3_scale(gz, h[2] * gg))));
  *a = v3_norm(v3_add(v3_scale(gx, h[4]),
       v3_add(v3_scale(gy, h[5]), v3_scale(gz, h[6]))));
  *r = h[3] * gg;
}

// The first-person holo sight, as scaled by SIGHT_K in tools/build_models.py. The
// third-person rifle keeps its own compact sight at VM_SIGHT_AR.
#define VM_HOLO_Y 0.0741f     // the window's centre: the line ADS puts on the eye
#define VM_HOLO_Z 0.1680f     // ...its front glass
#define VM_HOLO_R 0.0133f     // ...and its half-height
#define VM_HOLO_DOT_Z 0.1600f // the dot, inside the hood
#define VM_SIGHT_PT 0.0345f   // top of the pistol's sights: the line the eye sits on
static float gun_sight_y(int cur) {
  return cur == WPN_SR ? VM_SIGHT_SR : cur == WPN_PT ? VM_SIGHT_PT : VM_SIGHT_AR;
}
// ...and WHERE along the bore the shooter's eye is supposed to sit behind. The
// height alone defines a LINE, which is enough to ask "is the eye on the axis"
// and not enough to ask "is the eye behind the glass or in front of it" — and
// in a crouch it was in front of it. One copy: gun_build draws the AR's
// aperture at this station and the sniper's ocular at SCOPE_OCU_Z.
#define AR_APERTURE_Z 0.1835f
static float gun_sight_z(int cur) {
  return cur == WPN_SR ? SCOPE_OCU_Z : cur == WPN_PT ? -0.0405f : AR_APERTURE_Z;
}

// The bolt cycle, as two scalars: how far the handle is LIFTED and how far it is
// DRAWN. Three beats inside one 0..1 — lift, draw and return, drop — with sines
// at every seam so nothing has a velocity step through it. Shared, because the
// geometry in gun_build and the hand that grips the knob in two different
// renderers all have to agree about where the handle is; a second copy of these
// numbers is how a fist ends up closing on air 30 mm behind a bolt.
static void gun_bolt_phase(float bolt01, float *lift, float *draw) {
  float b = f_clamp(bolt01, 0.0f, 1.0f);
  *lift = b < 0.25f ? smooth01(b / 0.25f)
        : b < 0.80f ? 1.0f : 1.0f - smooth01((b - 0.80f) / 0.20f);
  *draw = b < 0.25f ? 0.0f
        : b < 0.80f ? sinf((b - 0.25f) / 0.55f * F_PI) : 0.0f;
}
// ...and the knob's world position for that phase. The rotation is about the
// weapon's own right axis through the bolt's centreline at y = 0.014.
// The bolt handle's placement numbers, named ONCE: gun_build sweeps the drawn
// handle chains with them and gun_bolt_knob hands the same point to the hand
// that grips it — a second copy of these is how a fist ends up closing on air
// 30 mm behind a bolt.
#define BOLT_LIFT_A   1.12f   // rad of bolt lift at full phase (~64 degrees UP, +gy)
#define BOLT_PIVOT_Y  0.014f  // the bolt axis the handle rotates about
#define BOLT_KNOB_U   0.030f  // knob station along the receiver's right
#define BOLT_KNOB_Z  -0.037f  // ...and along the bore
#define BOLT_DRAW_M   0.034f  // rearward throw of a full draw
static v3 gun_bolt_knob(float bolt01, v3 org, v3 gx, v3 gy, v3 gz, float gg) {
  float lift, draw;
  gun_bolt_phase(bolt01, &lift, &draw);
  float ba = lift * BOLT_LIFT_A, u = BOLT_KNOB_U,
        vy = -0.004f - BOLT_PIVOT_Y,   // -0.004 = the knob chain's VY midpoint
        wz = BOLT_KNOB_Z - draw * BOLT_DRAW_M;
  return v3_add(org, v3_add(v3_scale(gx, (u * cosf(ba) - vy * sinf(ba)) * gg),
                v3_add(v3_scale(gy, (u * sinf(ba) + vy * cosf(ba) + BOLT_PIVOT_Y) * gg),
                       v3_scale(gz, wz * gg))));
}

// Where the player's eye looks THROUGH the weapon, published by gun_build so
// the clearance proof below reads the same numbers the geometry does. Occlusion
// of this aperture by the shooter's own hands is invisible to figcheck and to
// vmcheck — both test the mesh against itself, and a hand in front of a sight
// is a perfectly valid mesh. It is also the single worst thing a viewmodel can
// do: at one point the AR's red dot rendered ON the support hand's knuckles.
static v3    g_gun_sight_p;   // world-space centre of the aperture
static float g_gun_sight_r;   // its smaller half-extent (the tighter constraint)

// `md01` is the MAGAZINE's own 0..1 travel, separate from `rl`. Driving the
// magazine off the pose arc meant it was 61 mm out of the well at the exact tick
// the seat sound played and the seat beat punched, and did not reach home until
// 300 ms after the game said the mag was in — a hard A/V desync on the
// mechanic's defining moment. 75 mm of travel on a 196 mm magazine that starts
// 30 mm below the bore never separated either: there was no empty-magwell frame
// in the whole animation.
// WHERE EACH TRIGGER GROUP SITS ALONG THE BORE. Both were solved against the
// rewritten `vmtrig`, which samples the guard's and the blade's own centrelines
// against the hand's closed mesh, and both are a compromise between two
// neighbours: too far back and the guard's rear upright runs through the palm
// wrapped round the pistol grip (the SR measured -7.5 mm inside the hand at its
// authored place, with its bottom-rear corner directly UNDER the grip's top
// node); too far forward and the same upright pushes the fingertip into the
// blade behind it. The AR's own bow is unchanged and only its blade moved.
#define SRTRIG_Z 0.0180f
#define ARTRIG_Z 0.0400f
// Sculpted first-person meshes, generated by tools/build_models.py and compiled
// in, so the game still ships as one file. Each is placed by a rigid frame; the
// procedural builders stay the authority for where hands and sights go.
#include "../models.h"

// --- THE ARMORY'S TABLES (names and config keys: see KNIFE_MODEL_NAMES) ----
// Every knife is authored on the combat knife's own grip cylinder, so the one
// closed fist (HAND_K) holds all of them and the swing timeline is shared.
typedef struct { const int16_t (*v)[7]; const model_part_t *p; int np; } mdl_ref_t;
static const mdl_ref_t KNIFE_MODELS[KNIFE_MODEL_COUNT] = {
  {KNIFE_V, KNIFE_P, KNIFE_NP},             {KNIFE_TANTO_V, KNIFE_TANTO_P, KNIFE_TANTO_NP},
  {KNIFE_DAGGER_V, KNIFE_DAGGER_P, KNIFE_DAGGER_NP},
  {KNIFE_KUKRI_V, KNIFE_KUKRI_P, KNIFE_KUKRI_NP},
  {KNIFE_TALON_V, KNIFE_TALON_P, KNIFE_TALON_NP},
};
// The loadout's first-person meshes, [slot][variant]: row 0 the rifles, row 1
// the pistols. NULL for a slot without variants.
static const mdl_ref_t GUN_VAR_MODELS[2][WPN_VARS] = {
  {{RIFLE_V, RIFLE_P, RIFLE_NP}, {RIFLE_AK_V, RIFLE_AK_P, RIFLE_AK_NP},
   {RIFLE_FAMAS_V, RIFLE_FAMAS_P, RIFLE_FAMAS_NP}},
  {{PISTOL_V, PISTOL_P, PISTOL_NP}, {PISTOL_GLOCK_V, PISTOL_GLOCK_P, PISTOL_GLOCK_NP},
   {PISTOL_DEAGLE_V, PISTOL_DEAGLE_P, PISTOL_DEAGLE_NP}},
};
static const mdl_ref_t *gun_var_model(int cur, int var) {
  if (cur != WPN_AR && cur != WPN_PT) return NULL;
  return &GUN_VAR_MODELS[cur == WPN_PT][armory_idx(var, WPN_VARS)];
}
// A weapon theme is PAINT, applied in the shader's viewmodel finish block
// (uSkin): `pat` 0 = factory (the block is skipped), 1 solid, 2 blotch camo,
// 3 digital, 4 tiger, 5 fade. a..c are the pattern's display colours on the
// light primary family, d the dark furniture's tint (GRAPH = unchanged).
typedef struct { float pat, scale, metal; float a[3], b[3], c[3], d[3]; } wpn_theme_t;
static const wpn_theme_t WPN_THEMES[WPN_THEME_COUNT] = {
  {0, 0, 0, {0}, {0}, {0}, {0}},
  {2, 15.0f, 0, {0.74f, 0.62f, 0.44f}, {0.55f, 0.42f, 0.28f}, {0.85f, 0.77f, 0.61f}, {0.36f, 0.30f, 0.23f}},
  {2, 15.0f, 0, {0.36f, 0.39f, 0.24f}, {0.21f, 0.18f, 0.12f}, {0.50f, 0.45f, 0.30f}, {0.21f, 0.23f, 0.17f}},
  {3, 9.0f, 0, {0.88f, 0.90f, 0.92f}, {0.62f, 0.66f, 0.71f}, {0.40f, 0.43f, 0.48f}, {0.50f, 0.52f, 0.55f}},
  {3, 9.0f, 0, {0.50f, 0.51f, 0.54f}, {0.22f, 0.23f, 0.25f}, {0.74f, 0.75f, 0.77f}, {0.185f, 0.188f, 0.197f}},
  {4, 5.0f, 0, {0.96f, 0.50f, 0.12f}, {0.10f, 0.09f, 0.08f}, {0.99f, 0.82f, 0.48f}, {0.14f, 0.12f, 0.11f}},
  {1, 0, 0,    {0.17f, 0.17f, 0.18f}, {0}, {0}, {0.12f, 0.12f, 0.13f}},
  {1, 0, 0,    {0.66f, 0.12f, 0.10f}, {0}, {0}, {0.17f, 0.13f, 0.13f}},
  {1, 0, 0,    {0.20f, 0.37f, 0.72f}, {0}, {0}, {0.15f, 0.16f, 0.21f}},
  {5, 4.0f, 0, {0.99f, 0.82f, 0.22f}, {0.92f, 0.30f, 0.56f}, {0.30f, 0.42f, 0.96f}, {0.16f, 0.15f, 0.18f}},
  {1, 0, 0.95f, {1.00f, 0.80f, 0.40f}, {0}, {0}, {0.10f, 0.10f, 0.11f}},
};
// A gear colourway re-dyes the gloves and the gauntlet ON THE CPU, per part, by
// the part's finish flag: each family keeps its own internal steps (a seam is
// still darker than the plate it frames) because the new colour is scaled by
// the part's value RELATIVE to its family's authored reference.
typedef struct { float fab[3], pad[3], hard[3], acc[3], lea[3], led[3]; } gear_theme_t;
static const gear_theme_t GEAR_THEMES[GEAR_THEME_COUNT] = {
  {{0}, {0}, {0}, {0}, {0}, {1.00f, 0.62f, 0.10f}},           // SLATE: identity
  {{0.50f, 0.42f, 0.30f}, {0.42f, 0.37f, 0.29f}, {0.44f, 0.37f, 0.27f},
   {0.25f, 0.23f, 0.20f}, {0.24f, 0.20f, 0.15f}, {1.00f, 0.62f, 0.10f}},
  {{0.29f, 0.32f, 0.21f}, {0.33f, 0.34f, 0.27f}, {0.27f, 0.30f, 0.20f},
   {0.64f, 0.54f, 0.40f}, {0.16f, 0.16f, 0.13f}, {0.45f, 1.00f, 0.30f}},
  {{0.13f, 0.13f, 0.14f}, {0.22f, 0.22f, 0.23f}, {0.12f, 0.12f, 0.13f},
   {0.62f, 0.12f, 0.09f}, {0.09f, 0.09f, 0.10f}, {1.00f, 0.16f, 0.08f}},
  {{0.70f, 0.72f, 0.75f}, {0.50f, 0.52f, 0.56f}, {0.80f, 0.82f, 0.85f},
   {0.28f, 0.31f, 0.36f}, {0.30f, 0.31f, 0.34f}, {0.25f, 0.80f, 1.00f}},
};
// ...and the same colourway on the WHOLE third-person kit, not just the hands:
// fatigues, trousers, plate carrier, helmet, webbing, boots, pads, balaclava
// and the goggle glow. Each row keeps fig_kit's value contract — boots the
// floor, carrier a clear step off the uniform, helmet a peak or an accent — so
// a re-dyed operator still reads as one at 25 m. Row 0 is the authored player
// kit exactly (fig_kit's palette 1), so SLATE changes nothing.
typedef struct {
  float uni[3], pants[3], carrier[3], helm[3], web[3], boot[3], rub[3], bala[3], gog[3];
} body_kit_t;
static const body_kit_t BODY_KITS[GEAR_THEME_COUNT] = {
  {{0.335f, 0.375f, 0.440f}, {0.290f, 0.320f, 0.380f}, {0.700f, 0.610f, 0.470f},   // SLATE
   {0.900f, 0.480f, 0.120f}, {0.150f, 0.160f, 0.190f}, {0.130f, 0.130f, 0.150f},
   {0.160f, 0.168f, 0.190f}, {0.260f, 0.275f, 0.305f}, {0.360f, 1.700f, 2.000f}},
  {{0.500f, 0.430f, 0.320f}, {0.430f, 0.370f, 0.280f}, {0.400f, 0.390f, 0.290f},   // COYOTE
   {0.620f, 0.530f, 0.380f}, {0.240f, 0.200f, 0.150f}, {0.200f, 0.160f, 0.120f},
   {0.300f, 0.260f, 0.200f}, {0.360f, 0.310f, 0.240f}, {2.000f, 1.200f, 0.250f}},
  {{0.300f, 0.335f, 0.235f}, {0.260f, 0.285f, 0.210f}, {0.640f, 0.540f, 0.400f},   // RANGER
   {0.420f, 0.450f, 0.310f}, {0.160f, 0.160f, 0.130f}, {0.150f, 0.140f, 0.120f},
   {0.200f, 0.210f, 0.170f}, {0.240f, 0.250f, 0.200f}, {0.800f, 2.000f, 0.600f}},
  {{0.140f, 0.140f, 0.150f}, {0.120f, 0.120f, 0.130f}, {0.230f, 0.230f, 0.240f},   // SHADOW
   {0.620f, 0.120f, 0.090f}, {0.090f, 0.090f, 0.100f}, {0.080f, 0.080f, 0.090f},
   {0.180f, 0.180f, 0.190f}, {0.120f, 0.120f, 0.130f}, {2.000f, 0.300f, 0.150f}},
  {{0.700f, 0.720f, 0.750f}, {0.600f, 0.620f, 0.660f}, {0.300f, 0.330f, 0.380f},   // ARCTIC
   {0.820f, 0.840f, 0.870f}, {0.280f, 0.300f, 0.340f}, {0.300f, 0.310f, 0.340f},
   {0.480f, 0.500f, 0.540f}, {0.620f, 0.640f, 0.680f}, {0.500f, 1.600f, 2.000f}},
};
static const mdl_ref_t *armory_knife(void) {
  return &KNIFE_MODELS[armory_idx(g_cfg.knife_model, KNIFE_MODEL_COUNT)];
}
static const wpn_theme_t *armory_theme(void) {
  return &WPN_THEMES[armory_idx(g_cfg.wpn_theme, WPN_THEME_COUNT)];
}
// THE LOADOUT GUNS' OWN FACTORY FINISH. The AK, the FAMAS, the Glock and the
// Desert Eagle ship in desert digital: their light family (receiver, covers,
// furniture, handle, slides) is the
// pixelated camo, the dark family a warm gunmetal. It is only the FACTORY
// answer — any theme picked in the Armory still repaints them like every gun.
static const wpn_theme_t VAR_DIGITAL_DESERT = {
  3, 17.0f, 0, {0.70f, 0.60f, 0.45f}, {0.53f, 0.43f, 0.31f}, {0.36f, 0.29f, 0.21f},
  {0.21f, 0.195f, 0.18f}};
// The same paint at the pistols' own scale: a pixel sized for a rifle's receiver
// is a third of a slide, and the pattern stops reading as digital.
static const wpn_theme_t VAR_DIGITAL_DESERT_PT = {
  3, 30.0f, 0, {0.70f, 0.60f, 0.45f}, {0.53f, 0.43f, 0.31f}, {0.36f, 0.29f, 0.21f},
  {0.21f, 0.195f, 0.18f}};
static const wpn_theme_t *vm_theme(int cur, int var) {
  if (!armory_idx(g_cfg.wpn_theme, WPN_THEME_COUNT) && (cur == WPN_AR || cur == WPN_PT) && var > 0)
    return cur == WPN_PT ? &VAR_DIGITAL_DESERT_PT : &VAR_DIGITAL_DESERT;
  return armory_theme();
}
static const gear_theme_t *armory_gear(void) {
  int g = armory_idx(g_cfg.gear_theme, GEAR_THEME_COUNT);
  return g ? &GEAR_THEMES[g] : NULL;
}
// model_draw's two armory hooks, both scoped by vm_build around the draws they
// apply to and NULL/0 everywhere else.
static int g_md_skin;                    // 1: weapon parts carry the theme flag
static float g_md_scale = 1.0f;          // uniform mesh scale about the origin
#define KNIFE_FIST_S  0.86f              // the knife fist against the rifle hand
#define KNIFE_VM_PUSH 1.10f              // the knife assembly's distance factor
static const gear_theme_t *g_md_gear;    // non-NULL: re-dye glove/gauntlet parts

static v3 gear_dye(int flag, v3 c) {
  const gear_theme_t *g = g_md_gear;
  const float *t, *ref;
  static const float SLATE_R[3] = {0.204f, 0.224f, 0.268f}, SLEEVE_R[3] = {0.132f, 0.138f, 0.152f},
                     PAD_R[3] = {0.320f, 0.322f, 0.330f}, HARD_R[3] = {0.166f, 0.186f, 0.230f},
                     TAN_R[3] = {0.64f, 0.54f, 0.40f}, LEA_R[3] = {0.134f, 0.140f, 0.152f},
                     AMB_R[3] = {1.0f, 0.62f, 0.10f};
  switch (flag) {
    case 1: t = g->fab;  ref = SLEEVE_R; break;
    case 2: t = g->fab;  ref = SLATE_R;  break;
    case 3: t = g->led;  ref = AMB_R;    break;
    case 4: t = g->pad;  ref = PAD_R;    break;
    case 5: t = g->hard; ref = HARD_R;   break;
    case 6: t = g->acc;  ref = TAN_R;    break;
    case 7: t = g->lea;  ref = LEA_R;    break;
    default: return c;
  }
  float k = (c.x + c.y + c.z) / (ref[0] + ref[1] + ref[2]);
  if (flag == 1) k *= (SLEEVE_R[0] + SLEEVE_R[1] + SLEEVE_R[2]) / (SLATE_R[0] + SLATE_R[1] + SLATE_R[2]);
  return (v3){{f_clamp(t[0] * k, 0, 1), f_clamp(t[1] * k, 0, 1), f_clamp(t[2] * k, 0, 1)}};
}

static void model_draw(const int16_t (*V)[7], const model_part_t *P, int np, int group,
                       v3 o, v3 ax, v3 ay, v3 az) {
  // The weapon frame (x right, y up, z forward) is left-handed where the world is
  // right-handed, so a mesh placed by it arrives with its winding reversed. Without
  // this the back faces render and every part shows its inside.
  int flip = v3_dot(v3_cross(ax, ay), az) < 0.0f;
  for (int i = 0; i < np; i++) {
    if (P[i].group != group) continue;
    // Only bare steel is metal. Painted parts stay dielectric, or the shader's
    // sky reflection turns the whole weapon to chrome.
    float mtl = P[i].flag ? -(float)P[i].flag : P[i].metal >= 0.8f ? P[i].metal : 0.0f;
    v3 c = {{P[i].r, P[i].g, P[i].b}};
    // The theme's two families, as a fraction on the finish flag the shader
    // already branches on (-4.3 is still polymer, -6.2 still painted alloy):
    // .3 the light primary paint (the tan family), .2 the dark furniture.
    if (g_md_skin && P[i].flag >= 4 && P[i].flag <= 6)
      mtl -= P[i].r > P[i].b * 1.3f ? 0.3f : 0.2f;
    if (g_md_gear) c = gear_dye(P[i].flag, c);
    mat_set(P[i].rough, mtl);
    for (int k = P[i].start; k < P[i].start + P[i].count; k++) {
      int t = (k - P[i].start) % 3;
      const int16_t *q = V[flip && t ? k + (t == 1 ? 1 : -1) : k];
      v3 lq = v3_add(v3_scale(ax, q[0] * MODEL_SCALE),
              v3_add(v3_scale(ay, q[1] * MODEL_SCALE), v3_scale(az, q[2] * MODEL_SCALE)));
      if (g_md_scale != 1.0f) lq = v3_scale(lq, g_md_scale);
      v3 p = v3_add(o, lq);
      v3 n = v3_add(v3_scale(ax, q[3] * MODEL_NSCALE),
             v3_add(v3_scale(ay, q[4] * MODEL_NSCALE), v3_scale(az, q[5] * MODEL_NSCALE)));
      float ao = (float)q[6] / 255.0f;
      g_ao_rate = 0.0f; g_ao_min = ao;
      scene_push(p, n, v3_scale(c, 0.78f + 0.22f * ao));   // baked occlusion
    }
  }
  ao_off();
}

static v3 gun_build(int cur, v3 org, v3 gx, v3 gy, v3 gz, float md01,
                    float trig01, float bolt01) {
  float sight_y = gun_sight_y(cur);
  v3 muzzle = org;
  // Shared palette: slim two-tone polymer/graphite with cool steel accents.
  // These are ALBEDOS now — a dark metal is dark in diffuse and gets all its
  // brightness from the sky it reflects, which is why steel can sit at 0.19 and
  // still read as bright metal.
  // Three families that actually differ, and none of them tinted blue. Every tone
  // here sat at b/r ~= 1.10, so the sky reflection made the whole weapon one navy
  // moulding: steel, polymer and rubber mirrored the same colour and separated
  // only by six hundredths of roughness.
  // These are DISPLAY albedos and the shader squares them, so 0.068 display is
  // 0.0046 linear — four times darker than real black paint, which bottoms out
  // near 0.04. Every weapon rendered as an unlit cut-out with three discrete
  // values across the whole model. Lifted to a real floor; still unmistakably
  // black, but now with something for the sun and the sky reflection to work on.
  // ...AND THEY SPLIT BY HUE, NOT ONLY BY VALUE. Measured, all four albedos sat
  // at b/r 1.063-1.078, i.e. four VALUES of one neutral warm grey: the rendered
  // weapon mask came out at mean saturation 0.097 against an arena at 0.207,
  // 2.08 stops darker than it, with only 1.04 stops of INTERQUARTILE range out
  // of a palette spanning 1.29. Half of what the player looks at is therefore
  // one flat tone. The polymer goes COOL (b/r 1.12 the other way) so the
  // furniture separates from the parkerized upper by hue instead of by three
  // hundredths of value — and deliberately cool rather than the warm FDE that
  // measured best in isolation, because this arena's ground IS warm sand and a
  // tan weapon loses the silhouette the whole palette was lifted to protect.
  // `steel` goes slightly warm and slightly brighter so the metal is the accent,
  // and `black` drops so the rubber/pad family is a real bottom end. The
  // extremes are pushed apart rather than everything raised: a lighter gun
  // against a light arena loses its silhouette, which is the failure the
  // paragraph above already paid for once.
  v3 acc   = {{1.000f, 0.480f, 0.070f}};  // blaze accent: the brand on the weapon
  v3 body  = {{0.690f, 0.575f, 0.415f}};  // dark polymer (dielectric), COOL
  // ...AND FOR A METAL THE ALBEDO IS ITS SPECULAR REFLECTANCE, F0, not a paint
  // colour — which is the term these two were being authored as. At 0.288
  // display the parkerized upper was F0 = 0.083 LINEAR: less reflective than
  // bare plastic (0.04 is the dielectric floor and this sat at twice it), on a
  // material whose diffuse is switched off. So every metal part on the weapon
  // reflected almost nothing, had almost no diffuse to fall back on, and came
  // out as one charcoal moulding whatever its roughness claimed. Real numbers:
  // a phosphate conversion coating is a dark, rough crystal layer over steel and
  // lands near 0.13 linear; bare/blued steel is 0.45-0.56 and reads that way in
  // any light. Both are pushed toward those, stopping short of bare steel's true
  // figure because this arena's ground IS warm sand and a rifle that mirrors it
  // at 0.5 loses exactly the silhouette the palette paragraph above was written
  // to protect. 0.365 display = 0.133 linear, 0.520 = 0.270.
  v3 upper = {{0.300f, 0.310f, 0.335f}};  // parkerized alloy: dark, but metal
  v3 steel = {{0.520f, 0.502f, 0.462f}};  // bare steel: reflectance, not paint
  v3 black = {{0.128f, 0.126f, 0.124f}};  // rubber / dead-black — nothing real
                                          // sits at 0.005 linear
  // THE MAGAZINE IS POLYMER AND IT WAS BORROWING THE RECEIVER'S ALBEDO. That
  // worked only while `upper` was being authored as a paint colour; the moment
  // it became a metal's F0 (see the note above) the magazine — a DIELECTRIC,
  // metalness 0 — inherited a number chosen for a reflectance it does not have
  // and rendered as the brightest object on the weapon. It gets its own tone,
  // keeping the thing the borrowed one was for: ONE VALUE STEP lighter than the
  // magwell it hangs out of, so a reload has a silhouette, in the furniture's
  // own cool family rather than the receiver's warm one.
  v3 mag   = {{0.130f, 0.135f, 0.145f}};  // magazine body: polymer, one step up
  v3 chas  = {{0.655f, 0.545f, 0.395f}};  // SR chassis polymer: the AWM's
                                          // olive-drab — the one color that
                                          // says "precision rifle" on sight
  v3 glass = {{0.045f, 0.055f, 0.070f}};  // coated lens: dark pupil, bright rim
  v3 lens  = {{0.086f, 0.112f, 0.118f}};  // multi-coated objective: blue-green
  float gg = g_gun_girth;                 // cross-section chunk (1.0 in first person)
  g_gun_trig_s = trig01;                  // published once, at the top, so a
                                          // third weapon cannot forget it
  ao_off();                               // a gun at arm's length sees open sky
  mat_set(MAT_PARK_R, MAT_PARK_M);
  #define GUNP(u, vy, wz) v3_add(org, v3_add(v3_scale(gx, (u) * gg), \
                          v3_add(v3_scale(gy, (vy) * gg), v3_scale(gz, (wz) * gg))))
  #define GBOX(u, vy, wz, hx, hy, hz, col) \
    scene_box(GUNP(u, vy, wz), v3_scale(gx, (hx) * gg), v3_scale(gy, (hy) * gg), \
              v3_scale(gz, (hz) * gg), col)
  // Placement discipline: neighboring parts INTERPENETRATE by >=1 mm with
  // different cross-sections (hidden faces, no coplanar pairs) — nothing
  // floats, nothing shares a plane.

  // Round parts are CHAINS — the same watertight swept prism the body is built
  // from (closed, correctly bevelled corners, no coplanar pairs by
  // construction). Barrels, tubes, grips, magazines and the scope are round
  // objects and boxes were always the wrong primitive for them; only genuinely
  // flat-sided parts (receivers, rails, pads) stay boxes.
  #define GCH(k, c0, c1, hint, ...) do {                                      \
    fnode_t nd_[] = __VA_ARGS__;                                              \
    int n_ = (int)(sizeof nd_ / sizeof *nd_);                                  \
    for (int i_ = 0; i_ < n_; i_++)                                            \
      nd_[i_].p = GUNP(nd_[i_].p.x, nd_[i_].p.y, nd_[i_].p.z);                 \
    fig_chain(nd_, n_, k, c0, c1, hint);                                       \
  } while (0)
  #define GN(u, vy, wz, r, rr, c) \
    (fnode_t){.p = {{u, vy, wz}}, .col = c, .bcol = c, \
              .rx = (r) * gg, .rz = (rr) * gg}

  if (cur == WPN_AR) {
    // ---- receiver group (flat-sided: boxes) ----
    mat_set(MAT_PARK_R, MAT_PARK_M);
    GBOX(0,  0.021f,  0.085f, 0.0155f, 0.0145f, 0.105f, upper);  // upper receiver
    // ONE CONTINUOUS TOP RAIL, receiver rear to handguard end — the
    // monolithic rail line that makes an HK416 read as one machine (ref:
    // commons HK416N). Rear face stays 6 mm INSIDE the receiver's (two
    // same-facing coplanar rears were a measured z-fight), front reaches
    // over the shortened quad-rail handguard.
    GBOX(0,  0.0375f, 0.155f, 0.0082f, 0.0030f, 0.169f, black);  // top rail
    GBOX(0, -0.005f,  0.058f, 0.0145f, 0.0135f, 0.072f, body);   // lower receiver
    // Every small control is CENTRED on the face it sits on and at least 2.6 mm
    // thick, so both of its own side faces clear that face by more than the depth
    // buffer can lose at across-the-arena range. Straddling a face by 0.3-1.0 mm
    // (which is what these did) is the classic way to build flicker into a part
    // that is otherwise correct: the surfaces never intersect, they just sit
    // inside each other's depth resolution.
    GBOX(0.0155f, 0.020f, 0.055f, 0.0028f, 0.0075f, 0.026f, black); // ejection port
    // ...AND THE SAME GROUP ON THE SIDE THE PLAYER CAN ACTUALLY SEE. Measured
    // with per-part recolour masks on the hip frame, the AR's ejection port, its
    // bolt catch and its four +x M-LOK boxes render EXACTLY 0 px, while their -x
    // twins render 164-384 px: the first-person camera sits on the weapon's LEFT
    // (the file states this itself where the charging handle is placed), so the
    // single most gun-like feature on a rifle is invisible for the whole match.
    // The real port stays where it is for third person; this is its mirror,
    // built as a RECESS — an inset panel with a proud dust-cover lip over it and
    // a brass deflector wedge behind — because a hole reads as machined where a
    // bump reads as a lump. The same measurement says what it is worth: a 36-vert
    // GBOX on this surface bought 164-384 px, i.e. 4.6-10.7 px per vertex,
    // against the 0.008 px/vert of the bore tube nobody can resolve.
    //
    // ONE BOX, NOT THREE, AND THE REASON IS THE GATE. The first version added a
    // proud dust-cover lip and a deflector wedge with it; measured, the lip took
    // `vmcheck`'s near from 20 to 25 at one spacing and produced 3 real ZFIGHT
    // pairs at another, because a thin proud strip on a flat receiver has six
    // faces that all want to graze something — the exact failure the "every
    // small control is CENTRED on the face it sits on and at least 2.6 mm thick"
    // paragraph above was written about. The RECESS is the part that carries the
    // read anyway: a hole in a flat slab is machined, a bump on it is a lump.
    // Alone it measures near=20 and zfight=0, i.e. byte-free against the
    // baseline IN VMCHECK, and it lands on the largest unbroken surface in the
    // first-person frame. It is a PROUD dark panel and not a cut recess — this
    // renderer has no CSG — and it costs figcheck two `near` pairs at every
    // profile tier (10->12 / 22->24 / 17->19), a 0.123 mm graze of exactly the
    // kind the +x twin already had, with zfight still 0.
    GBOX(-0.0150f, 0.0195f, 0.0550f, 0.0034f, 0.0068f, 0.0240f, black);  // port well
    // Picatinny ribs. A rail is a row of slots, and without them the top of the
    // receiver is one 27 cm bar that catches a single unbroken highlight — the
    // thing that made the whole gun read as a painted slab. Each rib stands 1 mm
    // proud and 0.4 mm wider, so nothing is coplanar with the rail under it.
    for (int r = 0; r < 6; r++)
      // 2.6 mm proud of the rail's top, not 0.9: at 0.9 the tooth's top face and
      // the rail's are inside the depth buffer's resolution from 28 m out, and
      // this is a 27 cm bar across the top of the gun — the most visible flicker
      // on the whole model. These 48 pairs were more than half of everything
      // figcheck's `near` tier was reporting.
      // Real Picatinny is a 10 mm slot pitch. Six teeth at 36 mm with 15 mm
      // treads standing 4.8 mm proud is a staircase, and it was the loudest shape
      // on the weapon in every first-person frame. Halved in every dimension: the
      // rail now reads as a rail instead of as the gun's defining feature, and it
      // still clears the depth buffer at arena range (2.4 mm proud, see below).
      // Rear group only: teeth forward of the sight put machining clutter
      // exactly where the eye rests over the support hand — the fore rail
      // stays a clean unbroken line.
      GBOX(0, 0.0415f, -0.005f + 0.021f * (float)r, 0.0104f, 0.0020f, 0.0042f, upper);
    GBOX(0.0145f, -0.004f, 0.040f, 0.0030f, 0.0044f, 0.0050f, acc);  // bolt catch
    GBOX(0.0168f, 0.0210f, 0.0275f, 0.0028f, 0.0042f, 0.0062f, upper);  // brass deflector
    GBOX(-0.0145f, -0.006f, 0.078f, 0.0030f, 0.0038f, 0.0042f, acc); // mag release
    GBOX(-0.0155f, 0.028f, 0.056f, 0.0030f, 0.0040f, 0.0060f, upper);  // forward assist
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    // THE CHARGING HANDLE MOVES. The AR's reload went visually dead from the
    // magazine seating to the end — measured, 45 changed pixels of 921600
    // between prog 0.921 and the standing idle frame — and the audio layer had
    // already patched the same hole from its own side with a sixth event, "hand
    // to the handle", whose comment says "in exactly the window where the
    // ANIMATION also goes static". This is that event's picture. The handle is
    // on the receiver's top LEFT, which is the side the first-person camera is
    // on, so unlike the sniper's bolt it needs no roll to be seen.
    {
      float ch = f_clamp(bolt01, 0.0f, 1.0f);
      float dr = ch < 0.18f ? smooth01(ch / 0.18f)
               : ch < 0.62f ? 1.0f - 0.05f * (ch - 0.18f) / 0.44f
               : 1.0f - 0.95f * smooth01((ch - 0.62f) / 0.38f);
      GBOX(-0.0155f, 0.026f, 0.020f - dr * 0.030f, 0.0040f, 0.0035f, 0.010f, steel);
      // The latch paddle, which is the part a hand actually hooks: it rides
      // with the handle and is what makes the pull read at all from the eye.
      GBOX(-0.0205f, 0.026f, 0.026f - dr * 0.030f, 0.0026f, 0.0032f, 0.0052f, black);
    }
    mat_set(MAT_PARK_R, MAT_PARK_M);
    GBOX(0, -0.020f, 0.108f, 0.0146f, 0.011f, 0.0292f, body);    // magwell
    // Flared feed collar, proud of the well on every side — the aggressive
    // competition-magwell silhouette, and a real thickness step (>2.6 mm
    // through the well's top face) so nothing grazes.
    GBOX(0, -0.0122f, 0.1085f, 0.0158f, 0.0036f, 0.0310f, acc); // magwell flare
    // ---- stock group ----
    // THE BUFFER TUBE DID NOT COME OUT OF THE MIDDLE OF THE RECEIVER, and that
    // is a y offset the whole stock group carries. The tube's axis sat at 0.014
    // against the upper receiver's 0.021 and the bore's 0.0225: 7 mm under the
    // box it exits and 8.5 mm under the bore, on a rifle whose entire design
    // premise is that the receiver extension IS the bore line. What that looks
    // like from the first-person camera — above the weapon and on its left — is
    // 8.9 mm of bare receiver face left standing above the tube while the tube
    // hangs 5.1 mm out under the box and slides away toward the lower corner:
    // the stock reads as bolted on off-centre, and from a raking angle it reads
    // as bolted on to one SIDE, which is the shape of the complaint. Measured on
    // the reported frame, the tube's apex is 5 px off the rear face's centre in
    // a 215 px face — the horizontal was never the defect, the vertical was, and
    // perspective turned one into the other.
    // The whole group moves as ONE rigid +7 mm so nothing inside it is
    // re-authored: every burial, every proud edge and every interpenetration
    // between tube, body, nose wedge, cheek line, toe spur and pad is a
    // difference of two stations here, and a uniform translation preserves all
    // of them. POSE_BUTT_UP is the one number outside this block that measures
    // it, and it follows.
    GCH(16, 1, 1, gz, {  // buffer tube, both ends capped and buried
      // -0.178, not -0.155: THE BUTT PAD WAS NOT ATTACHED TO THE RIFLE. The
      // tube's rear cap sat at z = -0.155 and the pad spans [-0.194, -0.172],
      // so 17 mm of open air separated them and the pad rendered as a black
      // slab floating behind the stock — from either side under `vmorbit`, and
      // in the plain hip frame, where the tube's dome ends in mid-air and the
      // ground shows through the gap 20 px before the pad starts. `vmcheck`
      // cannot see this: it tests the mesh against ITSELF for manifoldness and
      // coplanarity, and two parts that never touch are a perfectly valid mesh.
      // The house rule is the one the rest of this function obeys — neighbours
      // INTERPENETRATE, nothing floats — so the tube now ends 6 mm inside the
      // pad's front face with its cap buried there.
      GN(0, 0.021f, -0.198f, 0.0114f, 0.0119f, upper),
      // ...and the front cap goes INSIDE the receiver rather than ON its rear
      // plane. At 0.014 the tube hung out under the box, so its end disc was
      // never wholly inside that face; concentric it would be, and a cap
      // coplanar with a face it sits entirely within is the z-fight class this
      // function names twice. 4.5 mm forward buries it, and still 1.5 mm behind
      // the LOWER receiver's rear plane, so the two never share a volume.
      GN(0, 0.021f, -0.0155f, 0.0126f, 0.0121f, upper),
    });
    mat_set(MAT_POLY_R * 1.7f, 0.0f);   // rubber recoil pad: dead matte
    // 14.2 mm of half-width, not 12.8: the buffer tube now ends INSIDE this pad
    // (see the node above) and a 11.9 mm ring inside a 12.8 mm box leaves 0.9 mm
    // of clearance, which is inside figcheck's 1 mm `near` tier along the whole
    // buried length. A butt is wider than the tube it caps anyway.
    // 20 mm LONGER. A buffer tube and stock 212 mm behind the grip is a
    // collapsed carbine; a shouldered one is ~250, and the extra length is what
    // puts the pad ON the shoulder instead of reaching for it. Every station
    // behind the receiver moved with it, so the stock is longer rather than
    // detached.
    GBOX(0,  0.017f, -0.203f, 0.0142f, 0.0265f, 0.011f, black);  // butt pad
    GBOX(0,  0.017f, -0.2175f, 0.0148f, 0.0242f, 0.0045f, acc); // pad rib plate
    mat_set(MAT_POLY_R, 0.0f);
    // HK-SLIDER STOCK (ref: commons HK416N): ~5 cm of bare buffer tube shows
    // behind the receiver, then the sliding stock BODY takes over — nose
    // wedge climbing onto the tube, straight cheek line, body flaring to the
    // pad. Every piece buried in a neighbour; the pad plate ribs the butt.
    // (body top 0.0345, NOT the pad's own 0.0365 top plane — the shared-top
    // z-fight lesson from the slab it replaces.)
    // (half-width 0.0136: the buffer tube's 45-degree VERTEX sits at x
    // 0.0126, and a vertex ON the wall plane was a measured z-fight)
    GBOX(0,  0.0165f, -0.156f, 0.0136f, 0.0208f, 0.0410f, body); // stock body
    GBOX(0,  0.0355f, -0.1285f, 0.0100f, 0.0088f, 0.0125f, body); // nose wedge
    GBOX(0,  0.0408f, -0.160f, 0.0088f, 0.0048f, 0.0345f, body); // cheek line
    // (front face 3 mm BEHIND the body's — z -0.115 on both was the same-
    // facing coplanar z-fight the stock has already paid for twice)
    GBOX(0, -0.0085f, -0.1515f, 0.0098f, 0.0072f, 0.0335f, body); // toe spur
    // ---- grip, trigger, magazine ----
    mat_set(MAT_POLY_R, 0.0f);
    // A PISTOL GRIP HAS TO BE AS LONG AS THE HAND ON IT. This was 62 mm from
    // the receiver to the butt against a hand breadth of 90 mm (ANSUR 88, x1.13
    // here), so the palm overhung both ends: its index-side corner ran up inside
    // the receiver and its little-finger corner hung 7 mm off the bottom in open
    // air. A real AR grip is ~95 mm of gripping surface for exactly this reason.
    GCH(12, 1, 1, gz, {  // pistol grip, raked back and tapering into a swell
      GN(0, -0.014f, 0.008f, 0.0121f, 0.0116f, body),
      GN(0, -0.052f, -0.0100f, 0.0114f, 0.0126f, body),
      GN(0, -0.090f, -0.0239f, 0.0095f, 0.0105f, black),
    });
    // THE TRIGGER GROUP. A FINGER HAS TO FIT THROUGH THE GUARD, and this one's
    // opening was 25 x 10 mm — a curled index finger's distal phalanx is 17-19
    // mm thick, so there was no pose in which the digit could be inside the bow
    // rather than through it. That is the whole "the fingers reach through the
    // weapon model": not a placement error, a hole too small to place anything
    // in. A real AR guard is ~30 x 28 mm inside, which is what this is, and the
    // bow is a rounded RECTANGLE rather than a shallow arc so the length is
    // usable rather than eaten by the uprights.
    mat_set(MAT_POLY_R, 0.0f);
    GCH(6, 1, 1, gy, {
      GN(0, -0.0170f, 0.0140f, 0.0034f, 0.0030f, body),   // buried in the receiver
      GN(0, -0.0500f, 0.0186f, 0.0032f, 0.0028f, body),
      GN(0, -0.0510f, 0.0596f, 0.0032f, 0.0028f, body),
      GN(0, -0.0180f, 0.0660f, 0.0034f, 0.0030f, body),   // buried
    });
    // The blade: a real curved trigger on a pivot inside the receiver, not a
    // 5 mm box. `trig01` rotates it about that pivot — 7.2 degrees, which is
    // 4.4 mm of travel at the toe, the distance a finger actually moves.
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    {
      // 0.0375 and not 0.0330: the blade used to sit hard against the REAR of
      // its own bow with 2.67 mm of clearance and 30 mm of empty bow in FRONT of
      // it, so once the toe travels the right way it ends 2.06 mm INSIDE the
      // guard tube — which figcheck cannot see, because two closed meshes that
      // intersect are nowhere coplanar. Bounded on the other side too: at 0.0385
      // the finger's own Bezier starts clipping the bow.
      v3 tpv = GUNP(0, -0.0168f, ARTRIG_Z);       // pivot, inside the receiver
      // POSITIVE. This basis is (right, up, FORWARD), i.e. LEFT-handed, so the
      // textbook rotation runs backwards here — at -0.210 the toe swung 4.86 mm
      // toward the MUZZLE and the camera-space depth of the published contact
      // point went 0.3111 -> 0.3145 m, i.e. the trigger moved AWAY from the
      // finger pulling it. Now 4.86 mm rearward and 1.9 mm up, which is the arc
      // of a real shoe on a pin. Same trap the handedness note at fig_hand
      // records; it cost a round there too.
      float ta = trig01 * 0.210f;                 // rad, toward the shooter
      // Rotating about gx (the weapon's right) takes the toe REARWARD, which is
      // -gz, and slightly up. Built as a rotation rather than a translation
      // because a trigger is a lever and the toe travels further than the face.
      #define TRIGP(VY, WZ) v3_add(tpv, v3_add(                                \
          v3_scale(gy, ((VY) * cosf(ta) - (WZ) * sinf(ta)) * gg),               \
          v3_scale(gz, ((VY) * sinf(ta) + (WZ) * cosf(ta)) * gg)))
      // Radii carry the girth scale exactly as the positions do (every other
      // chain routes through GN, which applies it): raw, the third-person
      // blade drew 18% thinner than designed and the published radius below
      // no longer matched "the chain's own widest rz".
      fnode_t tb[3] = {
        {TRIGP(0.0016f, -0.0016f), steel, steel, 0.0026f * gg, 0.0042f * gg, 0, 0.0f},
        {TRIGP(-0.0122f, -0.0034f), steel, steel, 0.0026f * gg, 0.0040f * gg, 0, 0.0f},
        {TRIGP(-0.0226f, -0.0068f), steel, steel, 0.0024f * gg, 0.0034f * gg, 0, 0.0f}};
      fig_chain(tb, 3, 8, 1, 1, gz);
      for (int q = 0; q < 3; q++) g_gun_blade[q] = tb[q].p;
      g_gun_blade_r = 0.0042f * gg;   // the chain's own widest rz
      // THE PAD IS BEHIND THE BLADE, NOT IN FRONT OF IT. This used to publish
      // the blade's MUZZLE-side face with a forward normal, on the reasoning
      // that "a trigger finger hooks PAST the blade and pulls it back toward the
      // palm" — which is not how a rifle trigger is pulled. The pad rests on the
      // face that looks at the SHOOTER and pushes it rearward, and the wrong
      // side is not a cosmetic error: it put the digit's whole mid-section
      // across the blade's plane, so the steel ran through the flesh. Measured
      // with the rewritten vmtrig, which samples the blade's own centreline
      // against the hand's closed mesh: the blade was 9.9-11.1 mm inside the
      // finger on both weapons, and moving the TIP could not fix it (a 1.18 ->
      // 2.40 sweep of the stand-off moved it 9.9 -> 8.2 and then back to 11.3)
      // because the tip was never the part doing the swallowing.
      // -0.0073 is the rear face: the blade's centreline is at WZ -0.0048 at
      // this height and its half-thickness is 0.0025 (rx, because the chain's
      // hint is gz). `g_gun_trig_n` is that face's outward normal, i.e. the
      // direction the finger sits from the blade, and both come out of the same
      // rotation the geometry uses.
      g_gun_trig_p = TRIGP(-0.0165f, -0.0073f);
      g_gun_trig_n = v3_norm(v3_sub(TRIGP(-0.0165f, -0.0110f),
                                    TRIGP(-0.0165f, -0.0073f)));
      #undef TRIGP
      g_gun_guard[0] = GUNP(0, -0.0170f, 0.0140f);
      g_gun_guard[1] = GUNP(0, -0.0500f, 0.0186f);
      g_gun_guard[2] = GUNP(0, -0.0510f, 0.0596f);
      g_gun_guard[3] = GUNP(0, -0.0180f, 0.0660f);
      g_gun_guard_r  = 0.0034f * gg;
      // The no-go box for the trigger finger: the receiver FORWARD of the
      // grip. It deliberately stops short of the grip's own station, because
      // the index finger's root is buried in a palm whose top corner is itself
      // legitimately inside the receiver — a hand's index MCP sits right under
      // it, and burial is how every other joint in this model is closed. What
      // must never happen is a digit crossing the metal between the grip and
      // the muzzle, which is what the player sees as a finger inside the gun.
      g_gun_body_c   = GUNP(0, 0.0085f, 0.0700f);
      g_gun_body_e[0] = v3_scale(gx, 0.0155f * gg);
      g_gun_body_e[1] = v3_scale(gy, 0.0270f * gg);
      g_gun_body_e[2] = v3_scale(gz, 0.0600f * gg);
    }
    mat_set(MAT_POLY_R, 0.0f);
    {  // The magazine: a BOX, not a tube. It was a 12-gon — the smooth family,
      // the one the barrels and the scope are built from — so the single most
      // recognisable silhouette on the weapon rendered as a rounded extrusion.
      // A STANAG magazine is a flat-sided polymer box with broken edges, which
      // is exactly what the k=8 chamfered profile is for.
      //
      // Its cross-section was also TRANSPOSED, the same class of bug this file
      // records on the palm: `fig_ring` lays `rx` along the hint, the hint here
      // is gz, so rx was the FRONT-TO-BACK dimension at 24.4 mm and rz the WIDTH
      // at 38 — a magazine 38 mm wide and 24 deep, inside a magwell that is
      // 29 wide and 58 deep. A real 5.56 mag is ~24 wide and ~39 front to back.
      // 60 mm, not 150. The magazine only has to CLEAR the well to read as
      // out; at 150 it ended 34 cm below the bore, and the viewmodel's frustum
      // has 18 cm of half-height at the weapon's own distance, so the one object
      // a magazine change is about spent the whole animation off screen. At 60
      // the floor plate is still in frame as it goes.
      // AND SIDEWAYS, NOT ONLY DOWN. Measured through a magenta recolour mask on
      // the shipped reload: the magazine covers 5025 px while it is IN the gun and
      // only 4144 px at prog 0.32 and 3209 px at prog 0.40 with the well empty —
      // it gets LESS visible at exactly the moment it is the subject, because
      // 60 mm of straight-down travel is 60 mm straight at the bottom edge of the
      // frame (bbox y goes [590..719] -> [623..719]; a 190 mm magazine is ~380 px
      // at this distance, so three quarters of it is off screen at every station).
      // The camera sits on the weapon's LEFT, so a component along -gx swings the
      // magazine out of the well INTO frame — which is also what stripping one
      // actually looks like from the shooter's own eye. Measured after: 6926 px
      // at prog 0.32 and 5583 at 0.40, i.e. the magazine is finally more visible
      // out of the rifle than in it.
      float md = md01 * GUN_MAG_TRAVEL[WPN_AR][0],
            mdx = md01 * -GUN_MAG_TRAVEL[WPN_AR][1];
      mat_set(MAT_POLY_R, 0.0f);
      // ONE VALUE STEP LIGHTER THAN THE RECEIVER. At `body` the magazine was
      // the same albedo as the lower receiver and the magwell it hangs out of,
      // so the one object a reload is about had no silhouette against the part
      // it separates from — invisible for a different reason than being off
      // screen, and just as complete.
      // SHORT AND PLAIN. The 160 mm box was a full 30-round STANAG and it read
      // as the loudest thing on the weapon from every angle — it hung past the
      // firing elbow in the carry and past the bottom of the frame in half the
      // macro shots. 110 mm is a short magazine, which is what a compact carbine
      // carries, and three nodes instead of five is the same curve with nothing
      // in it that a silhouette can see. Its two recessed ribs went with it: the
      // minimal read is the point.
      GCH(8, 1, 1, gz, {
        GN(mdx, -0.030f - md, 0.107f, 0.0196f, 0.0120f, mag),
        GN(mdx, -0.090f - md, 0.089f, 0.0190f, 0.0117f, mag),
        GN(mdx, -0.140f - md, 0.074f, 0.0184f, 0.0114f, mag),
      });
      // Floor plate: proud of the body on every side, which is what gives a
      // magazine its one hard horizontal line and the thing a hand actually
      // grips when it pulls one out. Boxes here rather than a chain — it is a
      // flat-sided stamping and the chain would only round its corners again.
      mat_set(MAT_POLY_R * 1.4f, 0.0f);
      // ON THE MAGAZINE'S OWN AXIS, NOT ON THE GUN'S. The magazine is raked 20.6
      // degrees forward (z 0.107 -> 0.060 over y -0.030 -> -0.190), so the
      // chain's end cap is TILTED, while these two boxes were axis-aligned in
      // GUN space. Measured: the cap spans y -0.1964 at z 0.077 down to -0.1836
      // at z 0.043 against a flat plate top face of -0.1937, so the cap's REAR
      // edge stood 10.1 mm above the plate in open air and the plate rendered as
      // a separate black slab lying UNDER the magazine — a 7 px band of arena
      // visible straight through the joint under `vmorbit 270 -15 0.42 1`, and
      // on every bot's rifle in third person. Same class as the butt pad and the
      // cheek riser, and equally invisible to vmcheck, which only ever tests one
      // mesh against itself: two parts that never touch are a valid mesh. `mup`
      // is the magazine's own up, so the plate's top face is PARALLEL to the cap
      // it buries and the geometry cannot come apart at any magazine travel.
      // (0.9362, 0.3511) is the last bone (0, -0.032, -0.012) normalised; the
      // clearances are unchanged — 13.4 mm of half-width against the chain's
      // 11.4 and 20.6 of half-depth against its 18.2, i.e. still proud all round.
      // The magazine's OWN axis, re-derived with the shortened chain: the last
      // bone is (0, -0.050, -0.015), normalised (0.958, 0.287). Written out
      // rather than left at the old (0.9362, 0.3511) because a floor plate
      // square to the wrong axis is the 10 mm gap this comment block already
      // records once.
      v3 mup = v3_norm(v3_add(v3_scale(gy, 0.9580f), v3_scale(gz, 0.2870f)));
      v3 mfw = v3_norm(v3_add(v3_scale(gy, -0.2870f), v3_scale(gz, 0.9580f)));
      v3 mbt = GUNP(mdx, -0.140f - md, 0.074f);   // the chain's last node
      scene_box(v3_add(mbt, v3_scale(mup, -0.0028f * gg)),
                v3_scale(gx, 0.0134f * gg), v3_scale(mup, 0.0058f * gg),
                v3_scale(mfw, 0.0206f * gg), black);
      scene_box(v3_add(mbt, v3_scale(mup, -0.0108f * gg)),
                v3_scale(gx, 0.0122f * gg), v3_scale(mup, 0.0034f * gg),
                v3_scale(mfw, 0.0192f * gg), black);
      mat_set(MAT_POLY_R, 0.0f);
    }
    // ---- handguard, barrel, muzzle ----
    mat_set(MAT_POLY_R, 0.0f);
    // QUAD-RAIL HANDGUARD: a chunky faceted block (k=8 — the facet IS the
    // look), SHORTER than the old tube so real barrel shows ahead of it
    // (ref: the HK416's exposed muzzle end), entered through a delta-ring
    // taper at the receiver.
    // Vertical extents: the delta ring BURIES its top vertex in the rail
    // (1.7 mm past its underside), the guard body then drops 1.5 mm CLEAR of
    // it — a top vertex 0.1 mm off the rail's bottom plane was a measured
    // z-fight on the adjacent diagonal facets.
    GCH(8, 1, 1, gy, {
      GN(0, 0.019f, 0.178f, 0.0170f, 0.0154f, body),   // delta ring
      GN(0, 0.019f, 0.196f, 0.0138f, 0.0148f, body),
      GN(0, 0.019f, 0.300f, 0.0136f, 0.0144f, body),
      GN(0, 0.019f, 0.330f, 0.0132f, 0.0140f, body),
    });
    // Side + bottom rail strips on the guard (the quad in quad-rail).
    // Strip faces buried a full millimetre past the octagon's FLATS and kept
    // inside the flats' spans — a corner touching the 45-degree facet plane
    // was a measured z-fight.
    GBOX( 0.0150f, 0.019f, 0.262f, 0.0028f, 0.0032f, 0.0560f, black);
    GBOX(-0.0150f, 0.019f, 0.262f, 0.0028f, 0.0032f, 0.0560f, black);
    GBOX(0, 0.0027f, 0.262f, 0.0044f, 0.0038f, 0.0560f, black);
    // Gas tube running from the low block back under the rail into the guard.
    GCH(8, 1, 1, gy, {
      GN(0, 0.0306f, 0.186f, 0.0019f, 0.0019f, steel),
      GN(0, 0.0306f, 0.368f, 0.0019f, 0.0019f, steel),
    });
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    GCH(16, 1, 1, gy, {  // barrel: real exposure from the guard mouth out,
      GN(0, 0.0225f, 0.334f, 0.0086f, 0.0084f, steel),  // heavy at the chamber
      GN(0, 0.0225f, 0.368f, 0.0077f, 0.0075f, steel),
      GN(0, 0.0225f, 0.404f, 0.0069f, 0.0067f, steel),
      GN(0, 0.0225f, 0.432f, 0.0062f, 0.0061f, steel),
      GN(0, 0.0225f, 0.450f, 0.0058f, 0.0057f, steel),
      GN(0, 0.0225f, 0.462f, 0.0055f, 0.0054f, steel),
    });
    // THE GAS BLOCK HAS TO CLAMP THE BARREL, and it was hovering over it. At
    // y 0.036 with 6.2 mm of half-height its underside sat at 0.0298 while the
    // barrel's top at that station is 0.0302 — 0.4 mm of overlap, which the
    // 0.7 mm arris chamfer then ate: from the side there is a sliver of SKY
    // between a gas block and the barrel it is supposed to be pinned to. The
    // house rule for this whole function is that neighbours interpenetrate by
    // at least a millimetre; this one was written before the chamfer existed
    // and never re-checked against it. Now 5.7 mm of overlap, and it grew a
    // little because a block that straddles a barrel is wider than one perched
    // on it.
    GBOX(0, 0.0325f, 0.368f, 0.0074f, 0.0080f, 0.011f, black);   // gas block
    // Sling loop at the guard's mouth — the quad-rail block carries the rail
    // furniture now, so the loop is the guard's one remaining jewel.
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    fig_tube(GUNP(-0.0130f, 0.0090f, 0.196f), GUNP(-0.0182f, 0.0090f, 0.196f),
             0.0068f * gg, 0.0068f * gg, 0.0040f * gg, 0.0040f * gg, 12, steel, gy);   // front sling loop
    mat_set(MAT_PARK_R, MAT_PARK_M);
    GCH(16, 1, 1, gy, {  // muzzle: a drawn-out, purposeful head — mass
      GN(0, 0.0225f, 0.455f, 0.0090f, 0.0087f, black),  // without bluntness
      GN(0, 0.0225f, 0.474f, 0.0100f, 0.0097f, black),
      GN(0, 0.0225f, 0.486f, 0.0100f, 0.0097f, black),
      GN(0, 0.0225f, 0.489f, 0.0082f, 0.0080f, upper),
    });
    // Side ports, proud of the head — the detail that says COMPENSATOR.
    GBOX( 0.0092f, 0.0225f, 0.478f, 0.0026f, 0.0022f, 0.0070f, black);
    GBOX(-0.0092f, 0.0225f, 0.478f, 0.0026f, 0.0022f, 0.0070f, black);
    // ...and a real bore down the crown: a solid muzzle face is the one detail
    // the player looks straight at every time the gun comes up.
    fig_tube(GUNP(0, 0.0225f, 0.4855f), GUNP(0, 0.0225f, 0.4925f),
             0.0069f * gg, 0.0069f * gg, 0.0038f * gg, 0.0038f * gg, 16, black, gy);
    // ---- red dot: a thin SQUARE pane, forward over the barrel ----------------
    // A narrow plate, not a housing with a barrel's worth of depth. Two things
    // this has to keep from the versions before it: the window is a real HOLE the
    // player looks through (a modelled pane just becomes an opaque plate over the
    // target), and the fine outer edge is a real lip that shades itself rather
    // than a thinner box. So: one shallow rectangular ring for the frame and a
    // second, shorter one wrapped around its outside, both hollow, both shaded
    // per FACE because the profile is faceted — radial vertex normals would round
    // a square frame off into a pillow.
    // rx is HEIGHT here and rz is WIDTH: the ring's first axis comes from the `gy`
    // hint, its second from gy x gz = gx.
    // Mounted at the front of the rail rather than over the receiver, so the eye
    // sees it against the target instead of halfway down the gun; the SIGHT LINE
    // is a height (VM_SIGHT_AR), not a station along the barrel, so moving it
    // forward costs the ADS alignment nothing.
    mat_set(MAT_PARK_R, MAT_PARK_M * 0.75f);
    // Every dimension of this three-piece mount is chosen so that NO face of it
    // lands within a millimetre of a face of its neighbours: widths 0.0136 and
    // 0.0060 against the rail's 0.0092 and its teeth's 0.0106, and the neck's top
    // deliberately ends 1.5 mm ABOVE the window's lower edge — in open air inside
    // the frame — rather than buried 0.8 mm inside a 1.6 mm wall where its top
    // face and the wall's inner face are closer together than the depth buffer can
    // resolve at 25 m. It costs a 1.5 mm tab at the bottom of the sight picture,
    // which is where a real emitter housing sits anyway.
    // TWO SLIM POSTS, NOT ONE CENTRAL NECK. The neck was 12 mm wide and 26 mm
    // tall directly under the aperture, so the bottom of the sight picture — the
    // part of a target below its chest, at exactly the height a torso occupies
    // when the dot is on a head — was a black slab. It obeyed every geometry
    // rule and it was still the wrong shape: a mount's job is to hold the window
    // up, and the cheapest way to do that without standing in front of anything
    // is two legs at the window's own corners. The centre of the sight picture
    // below the dot is now open sky.
    // The clearances are the same ones the single neck was chosen for: the posts
    // sit at x +/-0.0148 with 2.8 mm of half-width, so their outer faces are
    // 1.07 mm inside the frame's inner wall (clear of figcheck's 1 mm `near`
    // tier), their inner faces are inside the rail clamp's 13.6 mm half-width,
    // and their tops still end 1.5 mm ABOVE the window's lower edge — in open
    // air inside the frame — rather than buried in a 1.6 mm wall where two faces
    // would be closer together than the depth buffer can resolve at 25 m.
    // LOWERED 13 mm (VM_SIGHT_AR 0.075 -> 0.062): a dot belongs just above
    // the bore, not on stilts. The posts shrink with it — 13.3 mm tall now —
    // and every stated clearance of the three-piece mount is re-solved for
    // the new height: bottoms buried 1 mm into the rail (whose teeth no
    // longer reach this station), tops still 1.5 mm ABOVE the window's inner
    // lower edge in open air, outer faces 1.07 mm inside the frame's wall.
    GBOX(0, 0.0424f, 0.1834f, 0.0136f, 0.0060f, 0.0100f, black);  // rail clamp
    GBOX( 0.0148f, 0.04635f, 0.1834f, 0.0028f, 0.00665f, 0.0050f, black);  // R post
    GBOX(-0.0148f, 0.04635f, 0.1834f, 0.0028f, 0.00665f, 0.0050f, black);  // L post
    // The window is 16:9 — 21.0 x 37.3 mm — and the frame around it is ONE ring:
    // 1.6 mm of wall, 3.4 mm deep, in bright steel. That is the whole sight.
    //
    // ONE ring is also why there is no frame macro here any more. The four-box
    // version butted its posts and bars corner to corner, which left each post's
    // outer face coplanar with each bar's end face and made the four corner edges
    // non-manifold — six such edges were the `dup` vmcheck found on the shipped
    // sights. A frame built from boxes has to INTERPENETRATE with differing
    // cross-sections to bury those faces; a single ring cannot have the defect.
    //
    // There was a second, nested ring here for the "fine outer edge" and it had to
    // go, not for tidiness: its inner surface sat 0.8 mm inside the frame's wall,
    // which is the same 1.6 mm band the mount arm has to end in, and my two
    // independent "0.8 mm clearance" choices put the arm's top face and the lip's
    // inner face on exactly the same plane — four coplanar same-facing triangles,
    // which figcheck reported as z-fighting. A minimal frame does not need the lip
    // anyway: at 1.6 mm of polished steel the ring IS the fine bright edge, and
    // there is now only one surface for the mount to bury itself in.
    //
    // A 1.6 mm wall has one consequence worth stating: the arm can bury itself
    // only 0.8 mm into it, so it lands in figcheck's `near` tier (within 1 mm)
    // rather than clear of it. That is the advisory tier, not the z-fight tier
    // (0.05 mm) — a sight frame this thin belongs there by construction.
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    fig_tube(GUNP(0, sight_y, AR_APERTURE_Z - 0.0017f),
             GUNP(0, sight_y, AR_APERTURE_Z + 0.0017f),
             0.0121f * gg, 0.02027f * gg, 0.0105f * gg, 0.01867f * gg, 4, steel, gy);
    g_gun_sight_p = GUNP(0, sight_y, gun_sight_z(cur));   // one copy; see figm weld
    g_gun_sight_r = 0.0105f;      // the window's inner HALF-HEIGHT, the tighter one
    mat_set(MAT_POLY_R, 0.0f);
  } else if (cur == WPN_PT) {
    // ---- sidearm: the stations of tools/build_models.py pistol() ----
    mat_set(MAT_PARK_R, MAT_PARK_M);
    GBOX(0, 0.0075f, 0.047f, 0.0122f, 0.0170f, 0.0985f, upper);   // slide
    GBOX(0, 0.0290f, 0.1305f, 0.0021f, 0.0052f, 0.0052f, black);  // front sight
    GBOX(0, 0.0290f, -0.0405f, 0.0100f, 0.0052f, 0.0042f, black); // rear sight
    mat_set(MAT_POLY_R, 0.0f);
    GBOX(0, -0.0170f, 0.042f, 0.0128f, 0.0098f, 0.0920f, body);   // frame
    GBOX(0, -0.0500f, 0.046f, 0.0038f, 0.0026f, 0.0250f, body);   // trigger guard
    GBOX(0, -0.0340f, 0.0735f, 0.0038f, 0.0160f, 0.0030f, body);
    GCH(14, 1, 1, gz, {  // grip
      GN(0, -0.020f, 0.006f, 0.0128f, 0.0190f, black),
      GN(0, -0.104f, -0.016f, 0.0120f, 0.0170f, black),
    });
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    GBOX(0, -0.0290f, 0.0460f, 0.0028f, 0.0120f, 0.0028f, steel); // trigger
    g_gun_trig_p = GUNP(0, -0.0300f, 0.0450f);
    g_gun_sight_p = GUNP(0, sight_y, gun_sight_z(cur));
    g_gun_sight_r = 0.0026f;      // half the rear notch
    mat_set(MAT_POLY_R, 0.0f);
  } else if (cur == WPN_GR) {
    // ---- grenade: gone from the hand once bolt01 says it was released ----
    // It sits UP in the hand, the shell proud of the fingers and the fuze clear
    // above them, and a fifth over true size: centred in the fist it was a
    // 3 cm ball the glove covered from every side but one, and from the
    // shoulder camera the figure held nothing at all.
    if (bolt01 < 0.5f) {
      v3 olive = {{0.270f, 0.300f, 0.220f}};
      v3 ka = v3_norm(v3_add(v3_scale(gy, 0.921f), v3_scale(gz, 0.386f)));
      v3 gc0 = v3_add(GUNP(0, -0.0643f, 0.0263f), v3_scale(ka, 0.020f * gg));
      const float gk = 1.2f * gg;
      mat_set(MAT_POLY_R, 0.0f);
      fnode_t nd[] = {
        {.p = v3_sub(gc0, v3_scale(ka, 0.034f * gk)), .col = olive, .bcol = olive,
         .rx = 0.014f * gk, .rz = 0.014f * gk},
        {.p = v3_sub(gc0, v3_scale(ka, 0.016f * gk)), .col = olive, .bcol = olive,
         .rx = 0.030f * gk, .rz = 0.030f * gk},
        {.p = v3_add(gc0, v3_scale(ka, 0.014f * gk)), .col = olive, .bcol = olive,
         .rx = 0.030f * gk, .rz = 0.030f * gk},
        {.p = v3_add(gc0, v3_scale(ka, 0.046f * gk)), .col = upper, .bcol = upper,
         .rx = 0.011f * gk, .rz = 0.011f * gk}};
      fig_chain(nd, 4, 12, 1, 1, gx);
    }
    g_gun_trig_p = GUNP(0, -0.0300f, 0.0450f);
    g_gun_sight_p = GUNP(0, 0, 0);
    g_gun_sight_r = 0.01f;
  } else if (cur == WPN_KN) {
    // ---- combat knife: lies along the grip cylinder, spine toward the wrist ----
    #define KNP(x_, sp_, ax_) GUNP((x_), -0.0500f + 0.386f * (sp_) + 0.921f * (ax_), \
                                   -0.0078f - 0.921f * (sp_) + 0.386f * (ax_))
    v3 ka = v3_norm(v3_add(v3_scale(gy, 0.921f), v3_scale(gz, 0.386f)));
    v3 ks = v3_norm(v3_add(v3_scale(gy, 0.386f), v3_scale(gz, -0.921f)));
    mat_set(MAT_POLY_R, 0.0f);
    scene_box(KNP(0, 0, -0.004f), v3_scale(gx, 0.0120f * gg), v3_scale(ks, 0.0130f * gg),
              v3_scale(ka, 0.0660f * gg), black);                 // handle
    scene_box(KNP(0, -0.003f, 0.0640f), v3_scale(gx, 0.0130f * gg), v3_scale(ks, 0.0265f * gg),
              v3_scale(ka, 0.0060f * gg), upper);                 // crossguard
    scene_box(KNP(0, 0, -0.0720f), v3_scale(gx, 0.0108f * gg), v3_scale(ks, 0.0110f * gg),
              v3_scale(ka, 0.0068f * gg), body);                  // pommel
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    scene_box(KNP(0, -0.0035f, 0.1480f), v3_scale(gx, 0.0024f * gg), v3_scale(ks, 0.0160f * gg),
              v3_scale(ka, 0.0780f * gg), steel);                 // blade
    scene_box(KNP(0, -0.0060f, 0.2440f), v3_scale(gx, 0.0014f * gg), v3_scale(ks, 0.0092f * gg),
              v3_scale(ka, 0.0200f * gg), steel);                 // towards the tip
    #undef KNP
    g_gun_trig_p = GUNP(0, -0.0300f, 0.0450f);   // nothing to pull; a sane contact
    g_gun_sight_p = GUNP(0, 0, 0);
    g_gun_sight_r = 0.01f;
    mat_set(MAT_POLY_R, 0.0f);
  } else {
    // ---- bolt-action precision rifle ----
    mat_set(MAT_PARK_R, MAT_PARK_M);
    GBOX(0,  0.013f,  0.030f, 0.0138f, 0.0152f, 0.115f, upper);  // receiver
    GBOX( 0.0128f, 0.0180f, 0.036f, 0.0028f, 0.0054f, 0.028f, black); // ejection port
    GBOX(-0.0128f, 0.0165f, 0.038f, 0.0028f, 0.0050f, 0.036f, black); // left relief flute
    GBOX(-0.0136f, 0.0055f, -0.056f, 0.0026f, 0.0038f, 0.0062f, acc); // thumb safety
    GBOX(-0.0138f, 0.0095f, -0.076f, 0.0032f, 0.0095f, 0.0060f, black); // folding hinge boss
    GBOX(0,  0.0315f, 0.020f, 0.0076f, 0.0028f, 0.078f, black);  // scope rail
    for (int r = 0; r < 4; r++)                                   // exposed rear/front rail teeth
      GBOX(0, 0.0358f, -0.048f + 0.014f * (float)r, 0.0088f, 0.0018f, 0.0034f, upper);
    // A DETACHABLE BOX MAGAZINE, because the reload is a magazine change now.
    // What was here was an 20 x 34 mm blob buried under the magwell — nothing
    // that could drop, be gripped or be seen, which is why the sniper's reload
    // had to be "rotate the rifle and hope": there was no object for the
    // animation to be ABOUT. This is an AICS-pattern box: 10 rounds' worth of
    // body, straight (a .308 case is nearly parallel, unlike the AR's raked
    // 5.56 curve), with a proud floor plate and a finger hook at the front.
    GBOX(0, -0.010f,  0.052f, 0.0152f, 0.0152f, 0.0242f, body);  // magwell
    GBOX(0, -0.0226f, 0.0535f, 0.0168f, 0.0040f, 0.0258f, acc);  // flared magwell collar
    {
      // Same two-component travel as the AR's: down AND toward the camera, so
      // the magazine gets BIGGER in frame as it leaves rather than sliding off
      // the bottom edge. See the note on the AR's `mdx`.
      float md = md01 * GUN_MAG_TRAVEL[WPN_SR][0],
            mdx = md01 * -GUN_MAG_TRAVEL[WPN_SR][1];
      mat_set(MAT_POLY_R, 0.0f);
      GCH(8, 1, 1, gz, {
        GN(mdx, -0.020f - md, 0.052f, 0.0206f, 0.0126f, mag),
        GN(mdx, -0.048f - md, 0.052f, 0.0202f, 0.0124f, mag),
        GN(mdx, -0.076f - md, 0.051f, 0.0196f, 0.0121f, mag),
        GN(mdx, -0.098f - md, 0.050f, 0.0188f, 0.0117f, mag),
      });
      // Stamped vertical reinforcement ribs on both sides of the AICS box.
      for (int r = 0; r < 2; r++)
        GBOX(mdx, -0.062f - md, 0.043f + 0.016f * (float)r,
             0.0138f, 0.0240f, 0.0028f, black);
      // Floor plate, on the magazine's OWN axis: this one is nearly vertical
      // (z 0.052 -> 0.050 over 78 mm of drop), so `mup` is almost gy — but it is
      // derived rather than assumed, because the AR learned that lesson as a
      // 10 mm gap between an axis-aligned plate and a tilted end cap.
      mat_set(MAT_POLY_R * 1.4f, 0.0f);
      v3 mup = v3_norm(v3_add(v3_scale(gy, 0.99968f), v3_scale(gz, -0.02564f)));
      v3 mfw = v3_norm(v3_add(v3_scale(gy, 0.02564f), v3_scale(gz, 0.99968f)));
      v3 mbt = GUNP(mdx, -0.098f - md, 0.050f);
      scene_box(v3_add(mbt, v3_scale(mup, -0.0026f * gg)),
                v3_scale(gx, 0.0138f * gg), v3_scale(mup, 0.0056f * gg),
                v3_scale(mfw, 0.0214f * gg), black);
      // The finger hook a shooter actually pulls a stuck AICS mag out by.
      mat_set(MAT_POLY_R, 0.0f);
      GBOX(mdx, -0.086f - md, 0.0742f, 0.0104f, 0.0112f, 0.0030f, black);
    }
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    // The BOLT, and it cycles now. `bolt01` lifts the handle, draws it back and
    // returns it — one rotation about the receiver's axis plus one translation,
    // so the knob traces the L a real bolt handle traces. The rear position is
    // 34 mm back, which is a short action's throw.
    {
      float lift, draw;
      gun_bolt_phase(bolt01, &lift, &draw);
      float ba = lift * BOLT_LIFT_A;      // 64 degrees of upward bolt lift
      float bz = draw * -BOLT_DRAW_M;
      #define BOLTP(U, VY, WZ) GUNP((U) * cosf(ba) - ((VY) - BOLT_PIVOT_Y) * sinf(ba), \
                                    (U) * sinf(ba) + ((VY) - BOLT_PIVOT_Y) * cosf(ba) + BOLT_PIVOT_Y, \
                                    (WZ) + bz)
      fnode_t bh[2] = {
        {BOLTP(0.012f, 0.014f, -0.030f), steel, steel, 0.0043f * gg, 0.0041f * gg, 0, 0.0f},
        {BOLTP(0.026f, 0.004f, -0.036f), steel, steel, 0.0038f * gg, 0.0039f * gg, 0, 0.0f}};
      fig_chain(bh, 2, 16, 1, 1, gy);
      fnode_t bk[2] = {   // the knob gun_bolt_knob's grip point is the midpoint of
        {BOLTP(BOLT_KNOB_U, 0.000f, BOLT_KNOB_Z), upper, upper, 0.0069f * gg, 0.0067f * gg, 0, 0.0f},
        {BOLTP(BOLT_KNOB_U, -0.008f, BOLT_KNOB_Z), upper, upper, 0.0062f * gg, 0.0062f * gg, 0, 0.0f}};
      fig_chain(bk, 2, 12, 1, 1, gz);
      // Rear bolt shroud & cocking piece sliding straight back out of the action.
      GCH(12, 1, 1, gy, {
        GN(0, 0.0155f, -0.074f + bz, 0.0076f, 0.0074f, black),
        GN(0, 0.0155f, -0.096f + bz, 0.0068f, 0.0066f, steel),
      });
      #undef BOLTP
    }
    mat_set(MAT_POLY_R, 0.0f);
    // Same two fixes as the AR's grip (see there): the top ring floated 8.7 mm
    // under the receiver it is supposed to hang from, and the bottom ended 6.5 mm
    // above the little-finger edge of the hand that holds it.
    GCH(12, 1, 1, gz, {  // grip
      GN(0, -0.0072f, -0.0233f, 0.0117f, 0.0113f, chas),
      GN(0, -0.050f, -0.046f, 0.0108f, 0.0122f, chas),
      GN(0, -0.0920f, -0.0640f, 0.0092f, 0.0102f, black),
    });
    // Trigger group — same construction as the AR's, same reason (see there):
    // the guard has to have a finger-sized hole in it. A chassis rifle's guard
    // is squarer and a match trigger's blade is straighter and flatter than a
    // service trigger's, which is the difference between the two tables.
    mat_set(MAT_POLY_R, 0.0f);
    // THE REAR UPRIGHT WAS UNDER THE GRIP. Its bottom-rear corner sat at
    // z = -0.0250, i.e. 25 mm BEHIND the receiver origin and directly below the
    // pistol grip's own top node (z = -0.0233) — so the wire ran through the
    // palm wrapped round that grip, measured 7.2 mm inside the hand's mesh by
    // the rewritten vmtrig, and no amount of clearing the DIGITS could reach it
    // because the offender was the palm. A trigger guard's bow belongs in FRONT
    // of the grip; moved there it is 16 mm clear of the blade behind it and
    // still leaves a 36 mm opening, against the 30 mm a curled finger needs.
    GCH(6, 1, 1, gy, {
      GN(0, -0.0008f, SRTRIG_Z - 0.0290f, 0.0034f, 0.0030f, body),  // in the chassis
      GN(0, -0.0505f, SRTRIG_Z - 0.0350f, 0.0032f, 0.0028f, body),
      GN(0, -0.0530f,  0.0272f, 0.0032f, 0.0028f, body),
      GN(0, -0.0006f,  0.0372f, 0.0034f, 0.0030f, body),   // buried in the magwell
    });
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    {
      v3 tpv = GUNP(0, -0.0010f, SRTRIG_Z);
      float ta = trig01 * 0.150f;    // a match trigger breaks shorter. Sign:
                                     // see the AR's note — this basis is
                                     // left-handed and -0.150 ran forward.
      #define TRIGP(VY, WZ) v3_add(tpv, v3_add(                                \
          v3_scale(gy, ((VY) * cosf(ta) - (WZ) * sinf(ta)) * gg),               \
          v3_scale(gz, ((VY) * sinf(ta) + (WZ) * cosf(ta)) * gg)))
      // Girth-scaled radii — same fix as the AR's blade above.
      fnode_t tb[3] = {
        {TRIGP(0.0014f, 0.0004f), steel, steel, 0.0028f * gg, 0.0038f * gg, 0, 0.0f},
        {TRIGP(-0.0126f, -0.0008f), steel, steel, 0.0028f * gg, 0.0036f * gg, 0, 0.0f},
        {TRIGP(-0.0232f, -0.0026f), steel, steel, 0.0026f * gg, 0.0032f * gg, 0, 0.0f}};
      fig_chain(tb, 3, 8, 1, 1, gz);
      for (int q = 0; q < 3; q++) g_gun_blade[q] = tb[q].p;
      g_gun_blade_r = 0.0038f * gg;   // the chain's own widest rz
      // The SHOOTER-side face, exactly as on the AR — see the note there. The
      // centreline is at WZ -0.0015 here with a 0.0027 half-thickness.
      g_gun_trig_p = TRIGP(-0.0165f, -0.0042f);
      g_gun_trig_n = v3_norm(v3_sub(TRIGP(-0.0165f, -0.0080f),
                                    TRIGP(-0.0165f, -0.0042f)));
      #undef TRIGP
      g_gun_guard[0] = GUNP(0, -0.0008f, SRTRIG_Z - 0.0290f);
      g_gun_guard[1] = GUNP(0, -0.0505f, SRTRIG_Z - 0.0350f);
      g_gun_guard[2] = GUNP(0, -0.0530f,  0.0272f);
      g_gun_guard[3] = GUNP(0, -0.0006f,  0.0372f);
      g_gun_guard_r  = 0.0034f * gg;
      // See the AR's box: the receiver forward of the grip only.
      g_gun_body_c   = GUNP(0, 0.0130f, 0.0575f);
      g_gun_body_e[0] = v3_scale(gx, 0.0138f * gg);
      g_gun_body_e[1] = v3_scale(gy, 0.0152f * gg);
      g_gun_body_e[2] = v3_scale(gz, 0.0875f * gg);
    }
    mat_set(MAT_POLY_R, 0.0f);
    // AWM THUMBHOLE CHASSIS (ref: commons AWM-338-white): an upper and a
    // lower spine run from receiver and grip heel into a butt block — the
    // OPENING between them IS the thumbhole, no CSG needed, and every bar is
    // buried at both ends (chain caps never end ON a block face: the
    // coplanar-cap z-fight class). The cheek piece rides two posts, the pad
    // wears a spacer plate — the reference silhouette, part for part.
    GCH(8, 1, 1, gy, {   // upper spine, receiver -> butt block
      GN(0, 0.0125f, -0.070f, 0.0092f, 0.0088f, chas),
      GN(0, 0.0090f, -0.212f, 0.0086f, 0.0082f, chas),
    });
    // Lower spine: starts DEEP inside the grip's bottom node (start cap fully
    // interior) and thinner than the grip, so the two chains cross at a steep
    // angle instead of running parallel surface-on-surface — the documented
    // parallel-chain flat-facet z-fight class.
    GCH(8, 1, 1, gy, {   // lower spine, grip heel -> butt toe
      // ...and it exits the grip's REAR at mid-height, well above the palm
      // heel: started at the heel it ran surface-on-surface under the
      // trigger hand's own up-facing palm chord (vmcheck z-fight at the one
      // spot both are horizontal).
      GN(0, -0.0560f, -0.0490f, 0.0056f, 0.0052f, chas),
      GN(0, -0.0300f, -0.2140f, 0.0072f, 0.0068f, chas),
    });
    GBOX(0, -0.0060f, -0.2225f, 0.0112f, 0.0330f, 0.0145f, chas);  // butt block
    // (rear face 4 mm ahead of the block's — a shared rear plane z-fights)
    GBOX(0,  0.0300f, -0.194f, 0.0086f, 0.0062f, 0.0390f, chas);   // cheek piece
    GBOX(0,  0.0225f, -0.172f, 0.0030f, 0.0062f, 0.0030f, black);  // riser post
    GBOX(0,  0.0225f, -0.214f, 0.0030f, 0.0062f, 0.0030f, black);  // riser post
    mat_set(MAT_POLY_R * 1.7f, 0.0f);
    // (half-height 0.0290: at 0.0310 the pad's top landed EXACTLY on the
    // butt block's own top plane y=0.027 — the shared-top z-fight again)
    GBOX(0, -0.0040f, -0.2405f, 0.0122f, 0.0290f, 0.0075f, black); // butt pad
    mat_set(MAT_POLY_R, 0.0f);
    GBOX(0, -0.0040f, -0.2340f, 0.0106f, 0.0270f, 0.0026f, acc);  // pad spacer
    // Fore-end in the AR's design language: no bolted-on rails where the
    // hand lives — a muscular shoulder at the receiver flowing through a
    // slight palm swell at the hold station into the slim AWM nose. The
    // bipod stays the fore-end's one functional accent.
    //
    // THE TOP LINE IS SET BY THE BARREL INSIDE IT, NOT BY THE TAPER. The
    // fore-end's top used to taper down (28.0 -> 22.5 mm) while the barrel
    // it wraps runs dead straight at bore height, so the two surfaces closed
    // from 1.2 mm of skin at the shoulder to 0.35 mm at the palm swell and
    // then CROSSED, at ~4 degrees, at z = 0.214 — 31 mm behind the fore-end's
    // own nose. That crossing is what the player sees as the green chassis
    // dissolving into the steel: a 12-gon and a 16-gon converging that
    // shallowly interleave crest-through-flat for centimetres before the
    // ideal surfaces meet, so the seam is a ragged green/steel speckle rather
    // than an edge, and the last 30 mm of barrel peels out through the TOP
    // skin instead of leaving through the front.
    //
    // The AR already states the rule this violates: a barrel does not show
    // through the handguard it is inside (there the barrel chain simply
    // starts where the guard ends). The SR cannot do that — its bore is drawn
    // the whole way and GUN_MUZ hangs off it — so the fore-end takes the
    // other half of the deal: its top line now runs PARALLEL to the bore, one
    // straight 27-28 mm line, and every millimetre of taper moved to the
    // bottom and the sides where the silhouette actually reads it. Measured
    // against the barrel's true 16-gon boundary (not its ideal circle), the
    // skin is +1.6 to +1.9 mm everywhere from the shoulder to the nose, and
    // the barrel now leaves through the nose CAP — a transverse plane it
    // crosses at 90 degrees, which is an edge a facet cannot smear.
    //
    // The BOTTOM of every node is unchanged on purpose: the support hand's
    // published cylinder (GUN_HOLD SR fore-end) and the bipod stub's 2 mm of
    // burial both hang off the underside, and neither has any business moving
    // because the top skin was too thin.
    GCH(12, 1, 1, gy, {
      GN(0, 0.0134f, 0.146f, 0.0144f, 0.0136f, chas),  // shoulder
      GN(0, 0.0136f, 0.172f, 0.0136f, 0.0126f, chas),
      GN(0, 0.0136f, 0.208f, 0.0135f, 0.0123f, chas),  // palm swell
      GN(0, 0.0142f, 0.245f, 0.0127f, 0.0109f, chas),  // nose
    });
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    GCH(16, 1, 1, gy, {  // HEAVY match barrel: near-straight profile
      GN(0, 0.016f, 0.130f, 0.0098f, 0.0095f, steel),
      GN(0, 0.016f, 0.170f, 0.0091f, 0.0089f, steel),
      GN(0, 0.016f, 0.260f, 0.0086f, 0.0084f, steel),
      GN(0, 0.016f, 0.350f, 0.0081f, 0.0079f, steel),
      GN(0, 0.016f, 0.420f, 0.0077f, 0.0076f, steel),
      GN(0, 0.016f, 0.452f, 0.0075f, 0.0074f, steel),
    });
    // Bipod MOUNT under the fore-end — the stud, with no legs on it.
    // The folded legs are gone. They read as two black sticks poking out of the
    // gun's underside at every hip angle and in every reload frame, they are the
    // one part of the rifle the support hand has to be kept clear of by
    // measurement (see the station note below), and a folded bipod is furniture
    // the player never uses: nothing in the sim deploys it. What is left is the
    // stud they bolted to, which is what an AWM's fore-end carries anyway and
    // still says "precision rifle" where a bare tube says "hunting gun".
    mat_set(MAT_PARK_R, MAT_PARK_M);
    // Pushed 8 mm forward, to the very front of the fore-end. The support hand
    // wraps 40 mm either side of its station and the stub used to start at
    // z = 0.218 — so a bipod leg came out through the underside of the shooter's
    // own fist, which the turntable shows in one frame and no first-person
    // screenshot ever would. A bipod belongs at the muzzle end of a handguard
    // anyway.
    // The clearance to the support hand is a MEASUREMENT, not a nudge. The palm
    // is 44 mm of half-breadth along the grip axis, so from the SR hold at
    // z = 0.178 it reaches z = 0.222; the stub's rear face is at z = 0.236. The
    // first attempt at this moved the stub 8 mm and left zero margin, and a grey
    // wedge of gun surfaced inside the palm cup between the middle and ring
    // fingers — which no first-person screenshot shows and the turntable does.
    GBOX(0, -0.006f, 0.2500f, 0.0072f, 0.0090f, 0.0140f, black);
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    GCH(16, 1, 1, gy, {  // AWM-class brake: a heavy ported can
      GN(0, 0.016f, 0.450f, 0.0104f, 0.0100f, black),
      GN(0, 0.016f, 0.470f, 0.0108f, 0.0104f, black),
      GN(0, 0.016f, 0.490f, 0.0108f, 0.0104f, black),
      GN(0, 0.016f, 0.494f, 0.0082f, 0.0080f, upper),
    });
    GBOX( 0.0102f, 0.016f, 0.464f, 0.0028f, 0.0026f, 0.0048f, black);  // rear port
    GBOX(-0.0102f, 0.016f, 0.464f, 0.0028f, 0.0026f, 0.0048f, black);  // rear port
    GBOX( 0.0102f, 0.016f, 0.481f, 0.0028f, 0.0026f, 0.0048f, black);  // front port
    GBOX(-0.0102f, 0.016f, 0.481f, 0.0028f, 0.0026f, 0.0048f, black);  // front port
    fig_tube(GUNP(0, 0.016f, 0.4905f), GUNP(0, 0.016f, 0.4985f),
             0.0075f * gg, 0.0075f * gg, 0.0042f * gg, 0.0042f * gg, 16, black, gy);   // bore
    // ---- scope: a SOLID round tube now ----
    // The old scope was four wall slabs with a hole down the middle so ADS could
    // look through it, which is both fragile geometry and not how a scope works.
    // The sight picture comes from the picture-in-picture pass instead, so the
    // tube can be what it really is: a closed body with an objective bell, an
    // ocular bell, turrets and two rings.
    mat_set(MAT_PARK_R * 0.8f, MAT_PARK_M);   // anodized alloy scope body
    GCH(20, 1, 1, gy, {
      GN(0, sight_y, SCOPE_OCU_Z, SCOPE_OCU_R, SCOPE_OCU_R, black),
      GN(0, sight_y, SCOPE_OCU_Z + 0.026f, SCOPE_TUBE_R + 0.0004f, SCOPE_TUBE_R, upper),
      GN(0, sight_y, 0.098f, SCOPE_TUBE_R - 0.0004f, SCOPE_TUBE_R + 0.0003f, upper),
      GN(0, sight_y, 0.116f, 0.0184f, 0.0181f, black),
      GN(0, sight_y, 0.147f, 0.0188f, 0.0185f, black),
    });
    GCH(16, 1, 1, gz, {  // elevation turret, knurled cap
      GN(0, sight_y + 0.011f, 0.062f, 0.0090f, 0.0087f, upper),
      GN(0, sight_y + 0.024f, 0.062f, 0.0086f, 0.0084f, upper),
      GN(0, sight_y + 0.027f, 0.062f, 0.0094f, 0.0092f, black),
      GN(0, sight_y + 0.034f, 0.062f, 0.0088f, 0.0086f, black),
    });
    GCH(16, 1, 1, gy, {  // windage turret
      GN(0.011f, sight_y, 0.062f, 0.0082f, 0.0079f, upper),
      GN(0.024f, sight_y, 0.062f, 0.0072f, 0.0073f, black),
    });
    GCH(16, 1, 1, gy, {  // left parallax & illumination turret
      GN(-0.011f, sight_y, 0.062f, 0.0086f, 0.0083f, upper),
      GN(-0.026f, sight_y, 0.062f, 0.0078f, 0.0079f, black),
    });
    GCH(20, 1, 1, gz, {  // magnification power ring collar
      GN(0, sight_y, -0.016f, 0.0160f, 0.0157f, black),
      GN(0, sight_y, -0.006f, 0.0158f, 0.0160f, black),
    });
    GBOX(-0.0135f, sight_y + 0.0125f, -0.011f, 0.0038f, 0.0036f, 0.0032f, acc); // throw lever
    // Rings AND the mount posts that carry them down to the rail. Without the
    // posts the whole optic hung in the air over the receiver with a 3 cm gap
    // under it — a scope has to be bolted to something.
    // Ocular and objective glass: recessed discs, near-mirror. Before this the
    // scope ended in a flat black 16-gon cap at each end and the "lens" was the
    // darkest thing in the frame — a scope with no glass in it.
    // THE OCULAR WAS THE OBJECTIVE'S BUG, ON THE END THE PLAYER LOOKS AT. The
    // glass chain sat 2 mm FORWARD of the scope body's own rear cap — a solid
    // `black` fan of the full 20.5 mm bell radius — so it was occluded outright
    // and what rendered was that cap: measured on the hip frame, mean L 11.4 with
    // a total range of ONE LSB across 3600 px, against L 41.9 on the tube 14 px
    // to its left. A 1.9-stop hole in the largest shape on the sniper's viewmodel,
    // for the whole hip pose and the first half of every raise — the PiP disc only
    // fades in above ads 0.55, so nothing covers it before then.
    // Same three fixes the objective already got, and for the same reasons: a
    // bright steel RETAINING RING that PROTRUDES past the cap so the black circle
    // has an edge, the glass RECESSED behind it so the lens has depth, and the
    // coating TINT rather than a near-black hole. Everything now sits REAR of
    // SCOPE_OCU_Z, so the cap can never hide it again. The ring's outer radius is
    // 1.3 mm inside the bell's, which keeps the two concentric surfaces out of
    // figcheck's 1 mm `near` tier; the PiP disc (SCOPE_OCU_R - 0.0016 = 18.9 mm)
    // still lands on the glass and covers all but a sliver of the rim when scoped.
    // Measured after: mean L 20.2, range 11.2-60.2 — form and a specular rim where
    // there was a flat hole.
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    fig_tube(GUNP(0, sight_y, SCOPE_OCU_Z - 0.0062f),
             GUNP(0, sight_y, SCOPE_OCU_Z + 0.0018f),
             0.0192f * gg, 0.0190f * gg, 0.0174f * gg, 0.0172f * gg,
             20, steel, gy);   // ocular retaining ring
    mat_set(MAT_GLASS_R, 0.0f);
    GCH(20, 1, 1, gy, {  // ocular glass, recessed inside the ring
      GN(0, sight_y, SCOPE_OCU_Z - 0.0030f, 0.0179f, 0.0177f, glass),
      GN(0, sight_y, SCOPE_OCU_Z + 0.0024f, 0.0170f, 0.0168f, glass),
    });
    // Objective. The biggest single shape on the sniper's viewmodel used to be a
    // flat dead-black disc: an unfinished cylinder end, and the darkest thing in
    // the frame for a whole match. Three things fix it and all three are how a
    // real objective reads. (1) COATING TINT, not black — multi-coated glass is a
    // dark blue-green, and at 0.045 albedo with no reflection reaching it the
    // lens rendered as a hole; 0.086/0.112/0.118 still reads as glass and has
    // something for the sky term to lift. (2) A bright steel RETAINING RING round
    // the glass, which is what actually catches the light on a real bell and
    // gives the black circle an edge. (3) The glass sits RECESSED behind that
    // ring, so the ring casts into it and the lens has depth instead of being
    // flush with the tube's end.
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    fig_tube(GUNP(0, sight_y, 0.1430f), GUNP(0, sight_y, 0.1545f),
             0.0177f * gg, 0.0175f * gg, 0.0128f * gg, 0.0126f * gg, 20, steel, gy);  // retaining ring
    mat_set(MAT_GLASS_R, 0.0f);
    GCH(20, 1, 1, gy, {  // objective glass, recessed inside the ring
      GN(0, sight_y, 0.1462f, 0.0116f, 0.0114f, lens),
      GN(0, sight_y, 0.1516f, 0.0111f, 0.0109f, lens),
    });
    mat_set(MAT_PARK_R * 0.8f, MAT_PARK_M);
    GCH(20, 1, 1, gz, {  // rear ring
      GN(0, sight_y, 0.030f, 0.0160f, 0.0157f, black),
      GN(0, sight_y, 0.042f, 0.0156f, 0.0159f, black),
    });
    GCH(20, 1, 1, gz, {  // front ring
      GN(0, sight_y, 0.076f, 0.0161f, 0.0156f, black),
      GN(0, sight_y, 0.088f, 0.0155f, 0.0158f, black),
    });
    GBOX(0, sight_y - 0.0295f, 0.036f, 0.0058f, 0.0225f, 0.0050f, black); // rear post
    GBOX(0, sight_y - 0.0295f, 0.082f, 0.0054f, 0.0225f, 0.0046f, black); // front post
    GBOX(0, sight_y - 0.0475f, 0.059f, 0.0072f, 0.0052f, 0.0295f, upper); // mount base
    mat_set(MAT_STEEL_R, MAT_STEEL_M);
    GBOX(-0.0076f, sight_y - 0.0475f, 0.041f, 0.0024f, 0.0024f, 0.0030f, steel); // clamp bolt
    GBOX(-0.0076f, sight_y - 0.0475f, 0.077f, 0.0024f, 0.0024f, 0.0030f, steel); // clamp bolt
  }
  muzzle = GUNP(GUN_MUZ[cur][0], GUN_MUZ[cur][1], GUN_MUZ[cur][2]);
  #undef GN
  #undef GCH
  #undef GBOX
  #undef GUNP
  return muzzle;
}

// (death_t is declared with pose_t, above the sim: the corpse pool hands one
// over from bot_tick and the renderer consumes it, so it cannot live down here.)

static void rot2(v3 *a, v3 *b, float ang);   // the viewmodel's rotation step

