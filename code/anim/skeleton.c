// code/anim/skeleton.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Skeleton locomotion — procedural humanoid animation for every third-person
// figure (bots today, any future visible player body). No keyframe clips: the
// pose is solved fresh every tick from the character's physics state.
//
// Feet are the ground truth. Each foot is either PLANTED — its world position
// is frozen, so there is zero foot sliding by construction — or SWINGING
// toward a spot predicted ahead along the velocity ("capture point": land the
// foot where the body will pass over it). A speed-adaptive gait phase
// alternates the swings while moving (left foot swings in the first window of
// the cycle, right in the second); standing still, feet only re-step to
// recenter under the hips or to follow them through a turn-in-place, which
// yields the shuffle for free. Backpedaling shortens the prediction, giving
// the wary backward tippel. Legs and arms close analytically with two-bone IK
// (knees bend toward the foot's heading, elbows out/down), the spine twists
// aim-vs-legs apart, and crouch / air / slide are eased pose blends layered
// onto the same solver, so every transition (sprint -> slide -> stand ->
// jump) stays continuous — no state-machine pops.
//
// Each tick produces a world-space pose_t (plain joint positions); the
// renderer only lerps prev/cur joints and wraps boxes around them.
// Deterministic: pure float math, no RNG stream involved.
// ---------------------------------------------------------------------------

#define ANIM_ANKLE_Y  0.075f  // ankle joint height with the sole on the floor
#define ANIM_THIGH    0.435f
#define ANIM_SHIN     0.425f
// USABLE leg reach, hip joint to ankle. A leg never locks straight, so every
// reach bound in the solver is the full leg times this fraction — the plant
// budget, the stance sink and the IK clamp all have to agree or a foot that
// one pass calls reachable the next pass drags off its plant.
#define ANIM_LEG      (ANIM_THIGH + ANIM_SHIN)
#define ANIM_REACH_K  0.965f
#define ANIM_HIP_HALF 0.095f  // half distance between the hip joints
// Hip joint height at MID-STANCE: ~93% of thigh+shin measured above the ankle,
// so the support knee passes over the foot at ~136 deg — visibly flexed rather
// than locked straight (0.872 - ANIM_ANKLE_Y over ANIM_LEG). The hip then
// drops ANIM_HIP_BOB*2 toward the stride extremes, which is both what a real
// pelvis does and what buys the horizontal leg reach a long stride needs — the
// two are the same mechanism. A hip held at one height either locks the knees
// (high) or never straightens them (low).
// 0.872, DOWN from 0.898: with THIGH+SHIN = 0.860 the old hip stood at 96%
// of full leg extension — the knees could never visibly flex (the critics'
// "compass gait") and the horizontal plant budget hmax was starved (0.23 m at
// walk), which silently reach-limited every stance. -26 mm buys visible knee
// flexion AND +34% stride reach; the chest offset below grows 0.38->0.40 so
// the HEAD barely moves (the head hitbox alignment survives).
#define ANIM_HIP_Y    0.872f
#define ANIM_HIP_BOB  0.050f
// 5-7 deg of toe-out is the population-normal foot progression angle; a
// perfectly parallel plant reads subtly robotic and toe-IN reads injured.
#define ANIM_TOE_OUT  0.14f
#define ANIM_STANCE_MIN 0.090f  // each foot stays this far on its own side of
                                // the hip midline — legs cannot cross
#define ANIM_TWIST_MAX  1.10f   // ~63 deg: how far the hips may lead the aim
// 0.44, DOWN from 0.55. The sim's crouch is a genuine 0.62 m squat (EYE_STAND
// 1.62 -> EYE_CROUCH 1.00) and the pose only delivered 0.39 m of it, so the
// rendered head stood 236.8 mm above the eye the hitbox is keyed to: measured
// with figm, the crouched skull spanned 1121.7-1361.2 mm against a head box of
// [880, 1180] — 24% of a visible head inside it — and the helmet crown stood
// 217.8 mm clear of the box entirely. A crouched enemy showing only his helmet
// was un-hittable, and aiming at the visible head was a miss. Standing is the
// contract this has to match: crown 1788.2 against a 1800 box.
#define ANIM_HIP_CR   0.44f   // crouched
#define ANIM_HIP_SL   0.19f   // sliding: the trail-leg HIP is the contact
// Arm segments, measured rather than eyeballed. ANSUR II (n=4082 male soldiers)
// puts acromion-radiale at 335 mm p50 and radiale-stylion at 267; taking the
// ~20 mm from the acromion in to the glenohumeral joint centre leaves a 315 mm
// humerus. The old 270 was 14% short, which is why the deltoid ball measured
// nearly as long as the upper arm it sat on — the mass was right and the bone
// under it was not, and that reads as a football pad, not a shoulder.
#define ANIM_UARM     0.315f
#define ANIM_FARM     0.265f
// Half the biacromial breadth (415 mm p50), pulled in slightly to the joint
// centres. The figure used to be 380 across the shoulder JOINTS — below ANSUR's
// 5th percentile — and then bolted an 84 mm ball on each side to make the
// overall breadth plausible. Width belongs in the frame, not in the pad.
// +12 mm per side for the redesign's front-silhouette breadth (the measured
// front read had not gained a pixel from the torso depth work alone) — a
// deliberate stylisation ON TOP of ANSUR, documented as such.
#define ANIM_SHO_HALF 0.212f
// Swing fraction of the FULL cycle per foot (stance = 1 - duty). Research
// duty FACTORS (stance fraction): 0.60 walk -> 0.22 sprint, i.e. swing 0.40
// -> 0.78; at 12 m/s and 2.25 cycles/s that is an 11.7-tick plant — assert
// with `kine`, not by eye.
#define ANIM_DUTY     0.40f   // walking
#define ANIM_DUTY_RUN 0.78f   // sprinting: >0.5 means BOTH feet leave the ground
#define ANIM_DUTY_REF 11.0f   // m/s at which the duty reaches ANIM_DUTY_RUN
// Fraction of the leg's full fore/aft span one stance may actually consume; the
// rest is the margin the reach clamp in skel_solve needs in order to stay unused.
#define ANIM_SPAN_K   0.90f
#define ANIM_DUTY_MAX 0.88f   // never leave a foot less than 12% of the cycle on
                              // the ground — below that a plant is a touch
#define ANIM_RUN_REF  5.5f    // m/s that counts as "full run" for pose scaling

typedef struct {
  // world-space joint positions first (a death topple rotates these around
  // the ground point)...
  v3 hip[2], knee[2], ankle[2], toe[2];  // legs, 0 = left / 1 = right
  v3 sho[2], elb[2], wri[2];             // arms
  v3 pelvis, chest, neck, head, gun_o;
  // ...then pure directions (a topple only rotates these)
  v3 right, head_f, gun_f, gun_u;
  v3 ant_sway;   // antenna whip: local tip offset from a 2nd-order follower
} pose_t;
#define POSE_NPOS 19  // leading v3 members that are positions

// TWO EXTENTS, BECAUSE THEY ANSWER TWO QUESTIONS. A joint is a point and the
// body is not, so anything that has to keep the BODY off a surface needs the
// mesh's own reach — but "how far can this part poke into a wall" and "how high
// does this part hold the body off the floor" are different numbers, and using
// one for both is why raising the wall margins made settled corpses FLOAT by
// 26-162 mm (`ragx`'s own `float` column, which exists to catch exactly that).
//   POSE_RAD   the WORST reach in any direction — a helmet is 190 mm from the
//              head node along the body's own spine, which is HORIZONTAL once
//              the body is down. The wall clamp and the push-out want this: too
//              small and geometry ends up inside a solid.
//   POSE_HALF  the mesh's HALF-THICKNESS about the joint — what actually rests
//              on the floor. The settle and the floor guard want this: too
//              large and the body hovers by the excess.
static const float POSE_RAD[POSE_NPOS] = {
  0.115f, 0.115f,   // hip
  0.105f, 0.105f,   // knee — the joint ball plus the thigh and shin tapers
  0.100f, 0.100f,   // ankle — the boot hangs 85 mm below it and 195 forward
  0.075f, 0.075f,   // toe
  0.115f, 0.115f,   // shoulder — deltoid pad
  0.090f, 0.090f,   // elbow
  0.095f, 0.095f,   // wrist — the glove, not the joint
  0.150f,           // pelvis
  0.255f,           // chest — carrier in front, PACK AND STRAPS BEHIND, and it
                    // is the pack that sets this: at 0.210 a body lying on its
                    // back had 69 mm of `packstrap` inside the platform it was
                    // lying on, for its whole life
  0.070f,           // neck
  0.190f,           // head — the helmet shell stands 120 mm past the node along
                    // the body's own spine, which is HORIZONTAL once it is down
  0.260f,           // gun anchor. A rifle is 900 mm long and no radius describes
                    // it; this keeps the receiver and magazine out of a wall and
                    // lets the barrel graze one, which is the right trade — the
                    // alternative is shoving the whole corpse a rifle-length off
                    // every piece of cover it dies against.
};
// Measured against `figm`, not authored: the boot's sole sits 76 mm under the
// ankle node (`outsole y0 = -0.9` with ANIM_ANKLE_Y 0.075) but only half a sole
// wide on its side, the glove is a ~45 mm ball on the wrist, and the PACK is the
// deepest thing on the chest — a body on its back rests on it. The gun anchor is
// ~0: the rifle is LAID on the ground by its own measured depth (g_gun_low), not
// held off it by a radius.
static const float POSE_HALF[POSE_NPOS] = {
  0.090f, 0.090f,   // hip
  0.072f, 0.072f,   // knee
  0.056f, 0.056f,   // ankle
  0.026f, 0.026f,   // toe
  0.090f, 0.090f,   // shoulder
  0.052f, 0.052f,   // elbow
  0.048f, 0.048f,   // wrist — the glove
  0.150f,           // pelvis
  0.255f,           // chest — the SAME as POSE_RAD, and measured that way: the
                    // pack really does stand this far off the spine, so a body
                    // on its back rests on it. Sweeping it (0.185 / 0.230 /
                    // 0.255) took the settled sink from -92 mm to -48 to -27
                    // with the float and the penetration unmoved — the pack
  0.055f,           // neck
  0.120f,           // head — the helmet shell, not its crown
  0.010f,           // gun anchor: laid, not rested
};

// What a corpse is doing this frame: how slack the limbs have gone, how far it
// has toppled, about which axis, how hard it just hit something, and how far it
// spun about its own vertical. Declared HERE, beside the pose it acts on,
// because it has two owners now — bot_tick hands one to the corpse pool at the
// respawn, and the renderer consumes it.
// `swing` is how far the LIMBS lag the trunk's rotation, in radians. Without it
// the death pose is one rigid shape being rotated: filmed across 60 ticks of
// free flight the cast-shadow silhouette was the same shape in every frame and
// both knees held one fixed angle for the whole arc, which is the difference
// between a ragdoll and a mannequin on a spit.
typedef struct { float slack, tilt, slam; v3 axis; float yaw, swing; } death_t;

typedef struct {
  v3    vel;                  // linear velocity
  v3    axis;                 // horizontal axis the body topples about
  float tilt, prev_tilt;      // accumulated topple angle (rad) — UNBOUNDED in
                              // the air, which is what lets a body cartwheel
  float spin;                 // ...and its rate (rad/s)
  float slam, prev_slam;      // impact crumple spike, decays
  float yaw, prev_yaw, yawv;  // spin about the body's OWN vertical, and its rate
  v3    push;                 // horizontal launch still owed, per tick. THE
                              // ORDER OF A DEATH IS ROTATE, THEN GO: applied
                              // whole on the killing tick the body had already
                              // translated 410 mm before it had rotated 10
                              // degrees — it moonwalks away from the shot still
                              // standing upright, which is the single loudest
                              // "this is not a body" tell in the whole death.
                              // Spread over DEATH_PUSH_T the spin (which IS
                              // applied whole) gets those first few ticks to
                              // itself and the throw arrives under a body that
                              // is already going over.
  int   push_t;               // ticks of it left
  float rest_off;             // per-kill offset on the attitude the body comes
                              // to rest in. Without it `rag_rest` returns the
                              // SAME 1.622 rad for every kill — five hit
                              // locations x two shot directions measured
                              // identical to three decimals — so every corpse
                              // in the arena lies in one attitude. Seeded off
                              // the impact point, so it is deterministic and
                              // still different for every round.
  float lag, prev_lag;        // a smoothed `tilt`. The DIFFERENCE tilt-lag is
                              // how far the limbs trail the trunk, which is the
                              // only articulation a rigidly-rotated pose can
                              // have and the whole reason a flung body reads as
                              // a ragdoll rather than a mannequin on a spit.
  int   grounded;
} rag_t;

// Ground sparks off a SLIDING DEAD BODY. Declared here because rag_advance —
// the one place both ragdoll owners integrate — sits far above the particle
// pool. A corpse grinding across concrete is the one contact in the game that
// had no feedback at all: vfx_skid only ever ran on the LIVING (it reads
// anim_t.slide_s, and a dead pose is frozen — the defect anim_t.live_t exists
// to catch), so the body simply slid in silence.
static void vfx_rag_sparks(const rag_t *r, v3 pos);
// (the integrator itself, `rag_step`, lives with the death constants below —
//  it needs ent_move, the arena clamp and every DEATH_* number. Only the type
//  is up here, because the PLAYER carries one too.)
_Static_assert(sizeof(pose_t) == 24 * sizeof(v3), "pose_t must stay a flat v3 array");

typedef struct {
  v3    pos, from, to;  // world ankle: current, swing start, swing goal
  float yaw;            // heading the foot was last planted with
  float t;              // swing progress, >= 1 = planted
} afoot_t;

typedef struct {
  v3      vel_s;             // smoothed world velocity
  float   spd;               // smoothed horizontal speed
  float   phase;             // gait cycle: L swings [0,DUTY), R [.5,.5+DUTY)
  float   leg_yaw;           // hip heading; trails the aim yaw
  float   crouch_s, slide_s, air_s;  // eased pose blends 0..1, S-SHAPED
  float   crouch_lin, slide_lin, air_lin;  // ...and the plain first-order eases
                             // driving them; see anim_tick for why both exist
  float   slide_t;           // seconds since the slide began (entry envelope)
  float   slide_v;           // eased slide speed factor 0..1.2 (recline depth)
  int     was_slide;
  float   land;              // landing dip impulse, decays
  float   hdown;             // hard-landing hand-to-ground reach, decays slower
  // ...and the IMPULSES those two chase. A knee absorbs a landing over ~100 ms;
  // assigning the dip outright on the touchdown tick moved the pelvis 178 mm and
  // the head 202 mm in ONE tick (21 and 24 m/s) on every landing in the game,
  // plus 372 mm of off-hand as the hard-landing brace switched on. Same class as
  // the gait-clock teleport `bob_lag` exists for.
  float   land_i, hdown_i;
  float   fall;              // most negative vel.y of the current air time
  float   brace;             // 0..1 preparation for a landing that is coming
  float   lean_f, lean_r;    // fwd/right acceleration lean (rad)
  float   fire_k;            // gun recoil kick, decays
  float   hit_k;             // TAKING a round: body flinch impulse, decays
  float   hit_s;             // ...and what the POSE sees, chasing it: assigned
                             // outright the impulse stepped the chest 42 mm and
                             // the head 23.5 mm in ONE tick, which is the same
                             // defect land_i/land and bob_lag both exist for
  v3      hit_d;             // unit horizontal direction that round travelled
  // WHERE the round landed, in the victim's own frame, latched with the
  // impulse. Without these a shoulder, a gut and a helmet produced the same
  // fold to the last millimetre — the identical defect the death torque was
  // written to fix, still present on every round that does NOT kill, which is
  // most of the rounds in a match.
  float   hit_y;             // impact height / HEIGHT_STAND, 0 boots .. 1 crown
  float   hit_x;             // lateral offset in the body's own right axis,
                             // -1 = full left, +1 = full right (0.26 m out)
  float   hit_w;             // wobble phase, ticks — the "zittern" after the
                             // fold: one damped oscillation, not a second decay
  v3      slide_dir;         // unit horizontal slide direction (travel)
  v3      slide_pd;          // pose direction: travel mirrored into the front
                             // hemisphere — legs always lead where knees bend
  float   slide_bk;          // 0 = forward/lateral slide, 1 = full butt-slide
  float   slide_ld;          // 0 = the LEFT leg leads the skid, 1 = the right.
                             // A BLEND: a slide aimed dead ahead sits exactly on
                             // the boundary and a boolean flips there
  int     was_grounded;
  float   idle_t;            // always-advancing clock for idle breathing
  float   bwd;               // eased 0..1 how much of the travel is backward
  float   wshift;            // eased single-support weight shift, -1 L .. +1 R
  float   sink;              // eased hip drop that keeps stance feet planted
  // The gait clock is allowed to TELEPORT: when a stance foot is stranded the
  // controller adopts the cycle mid-window by assigning `phase` outright. That
  // is right for the foot, and it used to be catastrophic for the body, because
  // the pelvis bob is a function of the same phase — measured, the hips rose
  // 66 mm over 26 ticks and then dropped 63 mm in ONE tick, once per step, at
  // every speed. A 7.6 m/s vertical teleport of the whole figure. `bob_lag`
  // holds the jump so presentation sees a CONTINUOUS phase and catches up over
  // ~0.2 s; the foot controller keeps the raw one.
  float   bob_lag;           // phase the visual bob still owes the gait clock
  // The same rule one level up: the ENTITY ORIGIN is allowed to teleport, the
  // BODY is not. An airborne crouch folds the whole stand/crouch difference into
  // the player's pos.y in a single tick — 0.60 m, i.e. 73 m/s of apparent
  // velocity, measured on the head — because that step is the crouch-jump
  // clearance and the collision box. `root_off` is the part of base.y that is
  // LEG TUCK rather than body height, so the pelvis and everything the spine
  // carries hang off the CONTINUOUS plane the entity would be standing on, and
  // only the FEET move. Always 0 for a bot: a bot's crouch shrinks its box from
  // the top and never touches its origin.
  float   root_off;          // m of base.y that is tuck, not ground
  // Last tick's baked tuck. AIRBORNE, every change in `tuck` is matched by a
  // step in the caller's origin — both the bake and the mid-air release move
  // pos.y — so root_off may track it exactly. A TAKEOFF is the one airborne
  // tick where that is NOT true, and comparing against the PREVIOUS tuck is
  // what tells the two apart. See the unwind in anim_tick.
  float   tuck_p;            // m of tuck the caller baked LAST tick
  // SPINE COMPLIANCE. The upper body has mass and the neck is not a weld;
  // measured on the pre-patch build, head.y - pelvis.y varied 3.98 mm across a
  // whole jump whose pelvis takes +864 m/s^2 at take-off and +758 at touchdown
  // (max |v_head - v_pelvis| 0.054 m/s over 400 ticks). See anim_tick.
  float   spc, spc_v;        // upper-body vertical give (m) and its rate
  float   spc_py, spc_pv;    // last tick's solved pelvis y, and its velocity
  // A FROZEN POSE IS NOT A MOVING ONE, and only the solver knows the difference.
  // player_tick stops calling anim_tick the moment the player dies (it just
  // copies prev_pose), so the whole state — slide_s, vel_s, the wrist on the
  // floor — freezes at its death-tick values and stays there. vfx_skid read that
  // as a live 21 m/s skid and ran a spark fountain nailed to the corpse for the
  // death screen and through the respawn. Testing an `alive` flag would fix only
  // the case we found; a stamp fixes the class, because ANY consumer can now ask
  // whether the pose it is looking at was solved this tick. The slack is 1: the
  // presentation drains inside player_tick BEFORE the player's own anim_tick.
  long    live_t;            // g_tick this pose was last solved on
  float   yaw_vel;           // smoothed aim-yaw rate — the head leads a turn
  float   vy;                // smoothed vertical velocity — stages the air pose
  v3      ant_p, ant_v;      // antenna follower: offset + velocity (2nd order)
  float   yaw_prev;          // last tick's aim yaw for the rate estimate
  float   ads_s;             // eased shouldered-weapon blend, 0 = hip, 1 = aimed
  float   ready_s;           // eased carry state: 1 = stock in the shoulder,
                             // 0 = low-ready / sprint carry (muzzle depressed)
  float   lean_s;            // eased lateral body lean, -1 = left .. +1 = right
  float   stance[2];         // 0..1 progress through the current foot contact
  float   fpitch[2];         // eased ankle pitch (+ = toe up), keeps the heel
                             // strike continuous across the swing->stance seam
  afoot_t foot[2];
  pose_t  pose, prev_pose;   // solved joints, cur + prev tick for interpolation
} anim_t;


// Clamp a foot target so it never pokes into world geometry: cast from the
// body center at the target's height toward it and stop short of the first
// solid. The body's own collider is narrower than a stretched leg, so a
// slide or a step along a wall would otherwise bury the foot in the box.
static v3 foot_clamp_m(v3 base, v3 tgt, float margin) {
  v3 o = {{base.x, tgt.y, base.z}};
  v3 d = {{tgt.x - o.x, 0, tgt.z - o.z}};
  float l = sqrtf(d.x * d.x + d.z * d.z);
  if (l < 1e-4f) return tgt;
  d.x /= l;
  d.z /= l;
  float best = l;
  // Prefilter before the full slab test: the ray is horizontal and only a hit
  // inside [0, l + margin) can lower `best`, so a solid that misses the
  // segment's padded XZ bounds — or the ray's own y plane — cannot change the
  // result. The 1e-3 padding keeps the reject strictly conservative against
  // ulp-scale disagreement with ray_aabb's arithmetic under -ffast-math.
  // (~30 of these casts per tick across every figure's feet and wrists.)
  float reach = l + margin;
  float bx0 = (d.x > 0 ? o.x : o.x + d.x * reach) - 1e-3f;
  float bx1 = (d.x > 0 ? o.x + d.x * reach : o.x) + 1e-3f;
  float bz0 = (d.z > 0 ? o.z : o.z + d.z * reach) - 1e-3f;
  float bz1 = (d.z > 0 ? o.z + d.z * reach : o.z) + 1e-3f;
  BP_BOX(i, bx0, bz0, bx1, bz1) {
    const solid_t *s = &g_solids[i];
    if (o.y < s->min.y - 1e-3f || o.y > s->max.y + 1e-3f) continue;
    if (bx1 < s->min.x || bx0 > s->max.x || bz1 < s->min.z || bz0 > s->max.z)
      continue;
    v3 n;
    float t = ray_aabb(o, d, s->min, s->max, &n);
    if (t > 0 && t - margin < best) best = t - margin;
  }
  if (best < 0) best = 0;
  if (best < l) {
    tgt.x = o.x + d.x * best;
    tgt.z = o.z + d.z * best;
  }
  return tgt;
}

static v3 foot_clamp_world(v3 base, v3 tgt) { return foot_clamp_m(base, tgt, 0.06f); }

// Distance from `p` straight down to the first surface under it, capped at
// `maxd`. Only the AIR pose asks: a falling body reaches for the FLOOR, and
// where the floor is is the one thing the pose cannot get out of the physics
// state it is handed — the height you took off from is not the height you land
// on the moment a crate is involved. Cheap by construction (an XZ point-in-box
// test before any arithmetic) and only reached while a figure is airborne.
static float ground_drop(v3 p, float maxd) {
  float best = p.y >= 0.0f && p.y < maxd ? p.y : maxd;   // the arena floor
  BP_BOX(i, p.x, p.z, p.x, p.z) {
    const solid_t *s = &g_solids[i];
    if (p.x < s->min.x || p.x > s->max.x || p.z < s->min.z || p.z > s->max.z) continue;
    float d = p.y - s->max.y;
    if (d >= -1e-3f && d < best) best = d;
  }
  return best < 0.0f ? 0.0f : best;
}

// Two-bone IK: returns the middle joint between root a and target t for
// segment lengths l1/l2; the bend goes toward `pole` (projected perpendicular
// to the a->t axis). Callers keep |t-a| inside l1+l2 — the clamp here is a
// last-resort guard, not the reach model.
static v3 ik_solve(v3 a, v3 t, float l1, float l2, v3 pole) {
  v3 d = v3_sub(t, a);
  float dl = f_clamp(v3_len(d), fabsf(l1 - l2) + 0.01f, l1 + l2 - 0.008f);
  d = v3_norm(d);
  v3 side = v3_sub(pole, v3_scale(d, v3_dot(pole, d)));
  if (v3_dot(side, side) < 1e-8f) side = v3_cross(d, (v3){{0, 1, 0}});
  if (v3_dot(side, side) < 1e-8f) side = (v3){{1, 0, 0}};
  side = v3_norm(side);
  float cosa = f_clamp((l1 * l1 + dl * dl - l2 * l2) / (2.0f * l1 * dl), -1.0f, 1.0f);
  float proj = l1 * cosa;
  float h = sqrtf(fmaxf(0.0f, l1 * l1 - proj * proj));
  return v3_add(a, v3_add(v3_scale(d, proj), v3_scale(side, h)));
}

// Off hand reaching for the ground (slide brace / hard-landing catch): roll the
// off shoulder down by `roll`, aim the wrist at `want`, and clamp to the arm's
// REAL length — that clamp is what keeps the elbow bent instead of straightening
// the arm into a rigid pole. Blended in by `k`. Both callers used to carry their
// own copy of the reach constant, which is the one number here that may not
// differ between them. Deliberately NOT shared with the corpse drape below: that
// one is 0.94 of the reach off an already-solved shoulder, and generalising over
// it would re-split the very constant this unifies.
static v3 brace_hand(v3 chest, v3 Rc, v3 want, float roll, v3 wri, float k) {
  v3 sho = v3_add(chest, v3_add(v3_scale(Rc, -ANIM_SHO_HALF * cosf(roll)),
                                v3_scale((v3){{0, 1, 0}}, -ANIM_SHO_HALF * sinf(roll))));
  v3 bd = v3_sub(want, sho);
  float bl = v3_len(bd), reach = (ANIM_UARM + ANIM_FARM) * 0.97f;
  if (bl > reach) want = v3_add(sho, v3_scale(bd, reach / bl));
  return v3_lerp(wri, want, k);
}

// Duty = the fraction of the gait cycle a foot spends on the ground; it rises
// with speed past 0.5, which is where the FLIGHT time of a run comes from. It is
// the contract behind "plants land exactly half a stance ahead", and three
// separate consumers depend on it being the same number to the bit: the pelvis
// bob's phase, the foot roll's stance progress and the plant-ahead budget. They
// each carried their own copy of the expression.
// Cadence in CYCLES/s (steps/2). A pure function of the smoothed state, because
// three consumers need the same number to the bit: the gait clock, the
// plant-ahead budget and the duty's reach bound below.
static float anim_hz(const anim_t *a) {
  float hz = a->spd <= 5.0f
                 ? 0.95f * powf(fmaxf(a->spd, 0.5f) * (1.0f / 1.4f), 0.33f)
                 : 1.45f * powf(a->spd * 0.2f, 0.50f);
  // Backpedalling steps quicker than walking, but only slightly. This
  // multiplier used to be swamped by the forced swing rate in the foot
  // controller; now it IS the step rate — but only since `anim_duty` stopped
  // capping the backward duty AFTER the reach bound. Until then this was the
  // CLOCK's ratio and not the feet's: at 4 m/s the clock read 1.22x forward
  // while the boots measured 4.80 against 2.40 steps/s, i.e. 2.00x. Measure a
  // cadence claim by counting PLANTS, never by reading
  // this function. 0.34 measured a flat 1.31x the
  // forward rate at every speed — too busy to read as a walk. 0.22 lands at
  // 1.17-1.19x through the 0.8-6 m/s band a player actually backs off at,
  // which still reads as its own gait. Going lower is not free: the clock
  // then disagrees harder with what the legs can physically turn over and
  // the sprint end regresses (measured at 12 m/s, 0.12 took the pelvis's rms
  // jerk from 12145 back up to 20047 and lifted the planted sole to 92 mm).
  return f_clamp(hz, 0.95f, 2.30f) *
         (1.0f + 0.22f * a->bwd + 0.22f * a->crouch_s);
}
// Horizontal reach budget for a plant: how far from the hip a foot may be
// placed and still be reachable when the hip bobs to its highest.
// How much of "full run" this speed is, for pose scaling (hip drop, stride).
static float anim_runT(const anim_t *a) {
  return f_clamp(a->spd / ANIM_RUN_REF, 0.0f, 1.0f);
}
static float anim_hmax(const anim_t *a) {
  float runT = anim_runT(a);
  float hipy = ANIM_HIP_Y + (ANIM_HIP_CR - ANIM_HIP_Y) * a->crouch_s -
               0.06f * runT * (1.0f - 0.65f * a->bwd) * (1.0f - a->crouch_s) -
               ANIM_HIP_BOB * 0.5f;
  float mxr = ANIM_LEG * ANIM_REACH_K;
  float drop = hipy - ANIM_ANKLE_Y;
  return sqrtf(fmaxf(0.04f, mxr * mxr - drop * drop));
}
static float anim_duty(const anim_t *a) {
  float d = ANIM_DUTY + (ANIM_DUTY_RUN - ANIM_DUTY) *
            f_clamp(a->spd / ANIM_DUTY_REF, 0.0f, 1.0f);
  // A STANCE CAN ONLY LAST AS LONG AS THE LEG CAN SPAN. The foot is frozen in
  // the world for (1-duty)/hz seconds, so the hip travels spd*(1-duty)/hz over
  // it and the leg can only cover 2*hmax of that. The authored duty violated
  // that bound at every speed above a walk (1.41 m of travel against 0.85 m of
  // leg at 8 m/s), and the consequence was not subtle: the emergency re-step —
  // which exists as a RESCUE — became the actual gait, taking the measured
  // cadence 36-82% above the very human curve `anim_hz` is fitted to. Binding
  // the duty to the budget hands the cadence back to the clock and pays for the
  // stride in FLIGHT time, which is where a fast run gets it from anyway.
  // Backpedalling does not fly AT THE SPEEDS IT CAN AFFORD NOT TO: cap the
  // AUTHORED swing fraction so one foot stays down. This is a cap on the
  // author's number and it belongs BEFORE the reach bound, never after it.
  // After it — where it used to sit — it silently undid the bound on the one
  // gait it applied to: a 0.50 duty at 12 m/s asks a stance to span 2.19 m of
  // travel against 0.64 m of leg, so the emergency re-step became the actual
  // backward gait exactly as it once was the forward one. Measured on the
  // clock's own ruler, the backpedal ran at 1.38-1.55x its own cadence with
  // 6-15 phase teleports per 200 ticks and `bob_lag` chasing up to 0.297 of a
  // cycle; the fix takes all three to 0.92-1.15x, 0 and 0.000, drops the
  // 12 m/s step rate 8.40 -> 5.40 steps/s (the clock says 5.48) and the
  // pelvis's rms jerk 4173 -> 2323 m/s^3, at an unchanged 6.8 mm plant float.
  d = d + (fminf(d, 0.50f) - d) * a->bwd;
  float hz = anim_hz(a);
  if (a->spd > 0.5f && hz > 1e-3f) {
    // THE PLANT DOES NOT STRADDLE THE BODY EVENLY. `ahead_t` lands it
    // 0.5*(1-0.18*bwd)*(1-0.18*crouch) of a stance ahead, so the REAR half is
    // the long one, and it — not the mean — is what has to fit inside the leg.
    // Writing the budget as a symmetric 2*hmax was 18% optimistic for exactly
    // the two gaits that shorten the lead, which is why backpedalling still
    // tripped the rescue at every speed after the bound went in. Forward and
    // uncrouched `rear` is 0.5 and this is the old expression bit for bit
    // (2*h/v and h/(0.5*v) round to the same float), which the artefact gate
    // asserts: every forward/idle/crouch/slide proof and all five `trace`
    // batteries come back byte-identical.
    float rear = 1.0f - 0.5f * (1.0f - 0.18f * a->bwd) * (1.0f - 0.18f * a->crouch_s);
    float need = 1.0f - (anim_hmax(a) * ANIM_SPAN_K * hz) / (rear * a->spd);
    if (need > d) d = fminf(need, ANIM_DUTY_MAX);
  }
  return d;
}
// Solve the full pose from the current animation state. base = feet-center
// world position (the entity's physics origin), yaw/pitch = aim.
static void skel_solve(anim_t *a, v3 base, float yaw, float pitch) {
  pose_t *P = &a->pose;
  float ly = a->leg_yaw;
  v3 R = {{cosf(ly), 0, sinf(ly)}};
  v3 F = {{sinf(ly), 0, -cosf(ly)}};
  v3 U = {{0, 1, 0}};
  float runT = anim_runT(a);
  float ground = (1.0f - a->air_s) * (1.0f - a->slide_s);

  // Pelvis: height blends stand/crouch/slide, bobs twice per gait cycle,
  // dips on landings, shifts laterally over the stance foot.
  float movT = f_clamp(a->spd / 0.6f, 0.0f, 1.0f);
  // Airborne, crouch means TUCK: the legs come up, the torso stays on its
  // ballistic arc. Blending the hip toward crouch height mid-flight visibly
  // stalled the head's rise (+14 mm/tick against the base's +33) — the
  // "pop" of a crouch-jump. The gate eases back in over ~90 ms on landing,
  // so touching down crouched still settles into the crouch smoothly.
  float hip_h = ANIM_HIP_Y + (ANIM_HIP_CR - ANIM_HIP_Y) * a->crouch_s *
                (1.0f - 0.90f * a->air_s);
  // Standing still the knees nearly straighten (the lowered gait hip reads as
  // sitting on an invisible stool when idle); moving takes the athletic flex.
  hip_h += 0.024f * (1.0f - movT) * ground * (1.0f - a->crouch_s) *
           (1.0f - a->slide_s);
  hip_h -= 0.06f * runT * (1.0f - 0.65f * a->bwd) * (1.0f - a->crouch_s);  // spring-mass
  hip_h -= 0.016f * a->bwd * (1.0f - a->crouch_s);  // backpedal stays coiled
  // Pelvis rise/fall, phased to the GAIT and not to an arbitrary offset: highest
  // as the body passes over the planted foot (knee straightens, cheapest posture
  // to hold), lowest at the stride extremes (where the leg needs the horizontal
  // reach). Two oscillations per cycle — one per step.
  // Presentation phase: the gait clock minus whatever jump it has not paid back
  // yet. Only the foot CONTROLLER may see a discontinuous phase.
  float bph = a->phase - a->bob_lag;
  bph -= floorf(bph);
  float bob_drop = 0.0f;  // how far the gait bob has pulled the pelvis DOWN
  {
    float duty = anim_duty(a);
    float phm = duty + 0.5f * (1.0f - duty);  // phase of left-foot mid-stance
    // Human CoM travel is ~2.7-4.8 cm peak-to-peak walking and 6-9 cm running,
    // so the amplitude scales with speed rather than being one constant.
    float latk = a->spd > 0.5f
                     ? fabsf(a->vel_s.x * R.x + a->vel_s.z * R.z) / a->spd
                     : 0.0f;
    // Backward there is no heel-strike vault to ride over — the contact is a
    // ball strike and the body rolls back across the foot — so the CoM stays
    // measurably flatter than it does walking forward at the same speed. This
    // is the cheapest half of a calm backpedal and it costs no geometry: at
    // 6 m/s it takes the pelvis's peak vertical speed from 1.48 to 1.02 m/s
    // and the head's rms vertical speed from 0.269 to 0.166, i.e. UNDER the
    // forward walk's own 0.206, while the stride keeps its full length.
    // Crouch does not DELETE the bob, it halves it. The hard (1 - crouch_s)
    // gate here and on `glist` below made a crouch-walk the one gait in the
    // game whose upper body does not move at all: measured over 200 ticks at
    // 1.4 and 2.5 m/s the pelvis and the head were both frozen to 0.0 mm
    // peak-to-peak and 0.000 m/s rms while the legs stepped 2.4-3.0 times a
    // second — the textbook glide, and the deeper ANIM_HIP_CR makes it MORE
    // visible, not less. Published crouch gait keeps a CoM excursion of the
    // same order as a walk's; 0.45x of the standing amplitude puts the pelvis
    // at 19 mm p-p and the head's rms vertical speed at 0.071 m/s against the
    // 3 m/s walk's 45 mm and 0.152, at an unchanged 13.0 mm plant float.
    float bobA = (0.014f + 0.016f * runT) * movT * ground *
                 (1.0f - 0.55f * a->crouch_s) * (1.0f - 0.40f * latk) *
                 (1.0f - 0.35f * a->bwd);
    // WALKING IS A PENDULUM, RUNNING IS A SPRING — nearly opposite phase.
    // Walking the CoM vaults over a stiff stance leg and is HIGHEST at
    // mid-stance; running the stance leg compresses under the landing and the
    // CoM is LOWEST at mid-stance. One phase applied at every speed is why a
    // sprint read as a sped-up walk. Blending a quarter-period shift on runT
    // flips the pattern continuously (the bob has two cycles per stride, so
    // 0.25 of gait phase is half a bob period = the full flip).
    bob_drop = bobA * (1.0f - cosf(12.566371f * (bph - phm - 0.25f * runT)));
    hip_h -= bob_drop;
  }
  hip_h += (ANIM_HIP_SL - hip_h) * a->slide_s;
  hip_h -= expf(-9.0f * a->slide_t) * 0.14f * a->slide_s;  // violent 100-150 ms drop-in
  // Exit beats 2+3: as the skid bleeds out the pelvis rises over the
  // gathering trail leg — mid and late slide must not photograph alike.
  float sl_exit = a->slide_s * (1.0f - f_clamp(a->slide_v, 0.0f, 1.0f));
  hip_h += 0.09f * sl_exit;
  // A landing is absorbed THROUGH THE KNEES, so a body that is still folded
  // from an air tuck has none of that travel left to spend: stacking the full
  // dip on an unpaid root_off is what put the pelvis 24-76 mm BELOW the ankles
  // when the touchdown clamp above was replaced by a decay. Scaling by what the
  // tuck still owes costs nothing anywhere else — root_off is 0 for every bot
  // and for every landing that was not an air crouch, so `knee` is 1 and this
  // is the old expression bit for bit.
  { float knee = 1.0f - f_clamp(a->root_off * (1.0f / (HEIGHT_STAND - HEIGHT_CROUCH)),
                                0.0f, 1.0f);
    hip_h -= a->land * 0.16f * knee;
    hip_h -= a->hdown * 0.26f * knee; }  // hard landing: absorb through the knees
  // Idle life: a standing soldier is never still. Breathing (~15/min) plus a
  // very slow weight shift — tiny amplitudes, but a perfectly frozen figure
  // reads as a paused game, and these cost nothing.
  float still = (1.0f - movT) * ground * (1.0f - a->slide_s);
  hip_h += sinf(a->idle_t * 1.60f) * 0.0055f * still;
  // Measured pelvis lateral travel was 72 mm peak-to-peak at a 1.4 m/s walk and
  // 38 mm at 8 m/s, against a human ~40 and ~20 — roughly 1.8x at every speed,
  // which reads as a waddle. The trunk `glist` below (3.2 deg, inside the real
  // 2-4) is what actually carries the weight shift on screen; the pelvis only
  // has to agree with it.
  float shift = sinf(bph * F_TAU) * 0.024f * (1.0f - 0.55f * runT) *
                ground * movT;
  shift += sinf(a->idle_t * 0.55f) * 0.016f * still;  // idle weight shift
  // ...and the weight goes over the foot that is actually carrying it. Gated on
  // `still` for the same reason the gait shift is gated the other way: while the
  // body is translating, `shift` is already driven by the stride and the two
  // would fight. This is what a pivot turn is made of — measured, the pelvis's
  // lateral travel through a 180 turn in place goes 5.8 -> 66.4 mm, and a steady
  // 12 m/s run is byte-identical (still == 0).
  shift += a->wshift * 0.055f * still;
  // ...and the other half of that gate: the sim ALSO folds the air tuck into
  // base.y (anim_t.root_off), so the untucked plane the body actually hangs off
  // is base.y - root_off. Everything BELOW this line keeps the raw base.y on
  // purpose — the two uses down there are the GROUND (the swing-height gate and
  // the foot-roll pivot), and the ground did not move; only the collider's
  // floor did.
  v3 root = base;
  root.y -= a->root_off;
  float hitf = a->hit_d.x * F.x + a->hit_d.z * F.z;
  float hitr = a->hit_d.x * R.x + a->hit_d.z * R.z;
  // WHERE the round landed, applied on top of which way it was going.
  //   hs_   the fold, RINGING. A pure decay reads as a shove; a body that is
  //         hit jolts, overshoots once and rings out, which is the "zittern"
  //         half of this. It is a clock rather than a second spring so the ring
  //         has the same shape whatever the impulse was.
  //   hlev  the LEVER the round had about the waist. A helmet is 1.0 m above the
  //         hips and a thigh is 0.3 m below them, so the same round pitches the
  //         trunk hard one way high on the body and the other way low on it —
  //         which is the difference between "he rocks back" and "his legs go".
  //   hrol  the impact's own offset off the centre line, rolling the shoulder
  //         line away from it. Without this a left shoulder, a right shoulder
  //         and a sternum were one animation to the millimetre.
  float hs_ = a->hit_s * (1.0f + 0.38f * sinf(a->hit_w * 0.62f) *
                                 expf(-a->hit_w * 0.075f));
  // 0.55, not 0.42: the pivot is the WAIST, and on a 1.79 m figure the waist is
  // at 1.00 m, i.e. 0.56 of stature. At 0.42 a gut shot counted as a high hit
  // and the pelvis measured the same 2.9 mm for every impact on the body.
  float hlev = f_clamp((a->hit_y - 0.55f) * 2.70f, -1.0f, 1.10f);
  float hrol = a->hit_x;
  v3 pelvis = v3_add(root, v3_add(v3_scale(U, hip_h + 0.07f), v3_scale(R, shift)));
  // Squatting sends the hips BACK, not straight down — that is what keeps a real
  // crouch balanced over the feet instead of tipping forward off them.
  pelvis = v3_add(pelvis, v3_scale(F, -0.105f * a->crouch_s));
  // Running, the mass has to lead the contact. With the pelvis pinned over the
  // collider the figure ran with its hips BEHIND the front foot and its torso
  // upright — the silhouette of someone skidding to a stop, held for the whole
  // sprint. Leaning the spine alone cannot fix that; the hips have to travel.
  // Backpedalling the same rule points the other way: hips shift INTO the
  // (backward) travel so the head-plumb line lands between the boots.
  pelvis = v3_add(pelvis, v3_scale(F, 0.062f * runT * ground * (1.0f - a->bwd)
                                    - 0.038f * a->bwd * movT * ground));
  // Hips trail the legs — which means they LEAD into the travel on a
  // backward butt-slide (slide_pd is the mirrored leg direction).
  pelvis = v3_add(pelvis, v3_scale(a->slide_pd, (-0.082f - 0.052f * a->slide_v) * a->slide_s));
  // Lean, part 1 of 2: the hips push OUT against the bend. Nobody leans by
  // hinging a rigid body at the waist — the pelvis counterweights, which is why
  // the feet can stay where they are and why the lean has any balance in it at
  // all. The bend itself is applied to everything above the waist at the very
  // end of the solve, once the head is known.
  if (fabsf(a->lean_s) > 1e-4f) {
    v3 Ra = {{cosf(yaw), 0, sinf(yaw)}};   // aim-relative: you lean where you look
    pelvis = v3_add(pelvis, v3_scale(Ra, -LEAN_HIP * a->lean_s));
  }

  // Keep stance feet PLANTED by sinking the body, not by lifting the foot: when
  // a stance leg cannot reach its plant — hard acceleration strands a foot
  // behind the hip — a real body drops its hip and stays on the ground. Capped
  // so it can never collapse, and eased so a re-step cannot pop it.
  {
    float mx = ANIM_LEG * ANIM_REACH_K, want = 0.0f;
    for (int i = 0; i < 2; i++) {
      if (a->foot[i].t < 1.0f) continue;  // stance legs only
      float s = i ? 1.0f : -1.0f;
      v3 hip = v3_add(pelvis, v3_add(v3_scale(R, s * ANIM_HIP_HALF),
                                     v3_scale(U, -0.07f)));
      v3 d = v3_sub(a->foot[i].pos, hip);
      float h = sqrtf(d.x * d.x + d.z * d.z);
      float need = sqrtf(fmaxf(0.0f, mx * mx - h * h));
      float have = hip.y - a->foot[i].pos.y;
      if (have > need) want = fmaxf(want, have - need);
    }
    a->sink += (fminf(want, 0.13f) - a->sink) * ease_k(18.0f);
    pelvis.y -= a->sink;
  }
  // A ROUND LOW ON THE BODY PUSHES THE HIPS, NOT THE CHEST. The lever about the
  // waist is the wrong way round there for a trunk fold, and what a gut or a
  // thigh hit actually does is take the stance out from under the man. `hlev` is
  // negative below the waist, so one number serves both ends of the body and the
  // pelvis stops being the one joint that measured the same 2.9 mm whatever the
  // round hit.
  {
    float low = fmaxf(0.0f, -hlev);
    pelvis = v3_add(pelvis,
        v3_add(v3_scale(F, 0.088f * hs_ * hitf * low),
               v3_scale(R, 0.070f * hs_ * hitr * low - 0.045f * hs_ * hrol)));
    pelvis.y -= 0.056f * hs_ * low;
  }

  // Spine: leans into acceleration and speed, hunches when crouched. In a
  // slide it reclines against the travel direction — deep at full boost,
  // sitting back up as the slide bleeds off — and tips a little toward the
  // trailing balance arm. Sliding backward that same term becomes the
  // forward crunch over the knees, just shallower (nobody lies down onto
  // their own shins).
  // Flinch, in the hips' own frame: the trunk folds around the impact and the
  // head ducks. Two components rather than one magnitude, because a round from
  // the side has to read differently from one in the back.
  // ...and it LEANS INTO THE RIFLE when the rifle comes up. Most of the reach
  // that puts an eye behind a sight belongs to the trunk, not the neck: the head
  // sits 0.66 up the spine, so 0.14 rad of forward lean carries it ~90 mm
  // forward on its own, which is 90 mm the neck does not have to crane. Taking
  // all of it in the neck instead (measured: 170 mm of pure head translation)
  // put the eye on the axis and made a turtle out of the figure.
  float lean_f = a->lean_f + runT * 0.42f * (1.0f - a->bwd) + 0.46f * a->crouch_s +
                 0.14f * a->ads_s +
                 0.23f * hs_ * hitf * (0.35f + 0.85f * hlev) +
                 0.50f * a->hdown +      // fold over the knees on a hard landing
                 0.10f * a->bwd;         // backpedal: trunk pitches FORWARD —
                 // reclining behind the hips while both feet land ahead of the
                 // CoM is the geometry of a fall, held every stride
  float rec_mag = (0.45f + 0.55f * a->slide_v) * (1.0f - 0.45f * a->slide_bk)
                  - 0.55f * (1.0f - f_clamp(a->slide_v, 0.0f, 1.0f));
  // (negative recline late in the slide = the forward pitch over the lead
  // foot that reads as "standing up out of it")
  // The slide recline acts on the FORE/AFT component of the travel only. Applied
  // to the full travel vector it tipped the torso sideways in a lateral slide —
  // away from the travel, which lifts the bracing shoulder and left the hand
  // hanging 11 cm off the ground exactly when it should be planted on it.
  float sfd = a->slide_dir.x * F.x + a->slide_dir.z * F.z;
  // Gait list: the trunk tips 2-4 deg toward the stance side in phase with
  // the pelvis shift — 4 cm of pure pelvis slide under a plumb trunk is
  // sub-pixel at 10 m, the list is what actually reads.
  float glist = sinf(bph * F_TAU) * 0.055f * movT * ground *
                (1.0f - 0.55f * a->crouch_s) * (1.0f - a->slide_s);
  v3 spine = v3_norm(v3_add(U,
      v3_add(v3_scale(F, lean_f * (1.0f - a->slide_s) - rec_mag * a->slide_s * sfd),
             v3_scale(R, a->lean_r * (1.0f - a->slide_s) + glist
                         + 0.11f * hs_ * hitr
                         - 0.40f * hs_ * hrol   // rolls AWAY from the wound
                         - 0.30f * a->slide_s * (0.35f + 0.65f * fabsf(sfd))))));
  v3 chest = v3_add(pelvis, v3_scale(spine, 0.40f));
  // ...and everything above the waist rides the compliance (see anim_tick):
  // neck, head, both shoulders, the gun anchor and both wrists are all built
  // FROM chest, so one addition here squashes the abdomen and leaves every
  // limb's shape, and therefore every figcheck invariant, untouched. The legs
  // hang off `pelvis` and do not move at all.
  chest.y += a->spc;
  // Torso twists most of the way toward the aim, plus a gait-phase counter-
  // rotation against the hips: shoulder swing is what makes a run read as a run
  // once the figure is only 50 px tall and the legs are a blur.
  // ...and the gait term is NEGATIVE, because thorax and pelvis counter-rotate.
  // Both this and `pyaw2` below used +sin of the same phase, so the shoulder
  // girdle and the pelvis yawed the SAME way at the same moment and the trunk
  // twist between them was only +-2.9 deg — there was no counter-rotation in the
  // figure at all, despite pyaw2's comment claiming to be it. Anti-phase makes
  // the relative twist +-15.5 deg (31 peak-to-peak), which is the published
  // running range (20-30), and it is what cancels the legs' angular momentum.
  float cyaw = ly + wrap_pi(yaw - ly) * 0.78f -
               sinf(bph * F_TAU) * 0.16f * runT * ground;
  v3 Rc = {{cosf(cyaw), 0, sinf(cyaw)}};
  v3 neck = v3_add(chest, v3_scale(spine, 0.10f));
  // The head keeps looking where the eyes are pointed: it follows the spine only
  // partly, and in a deep slide recline it stays almost upright rather than
  // lying back with the torso.
  v3 head_up = v3_norm(v3_add(v3_scale(spine, 0.45f * (1.0f - 0.7f * a->slide_s)),
                              v3_scale(U, 0.55f + 0.40f * a->slide_s)));
  // The head only follows the aim so far: at a full 90 degree look-up, 0.65 of
  // the pitch would lay the skull on the spine and slide the helmet off the
  // back of it. Real aiming past ~35 degrees is done with the eyes.
  float hp = f_clamp(pitch * 0.62f, -0.62f, 0.62f);
  // ...and it LEADS a turn: eyes -> head -> trunk -> pelvis is the human
  // ordering, so the head runs ~0.2 s of yaw rate ahead of the aim (capped
  // ~14 deg). Cheap, and it is what makes a turn read as intent.
  float hyaw = yaw + f_clamp(a->yaw_vel * 0.20f, -0.25f, 0.25f);
  v3 head_f = {{sinf(hyaw) * cosf(hp), sinf(hp), -cosf(hyaw) * cosf(hp)}};
  // THE NECK GETS LONGER IN A SLIDE, and that is geometry rather than taste.
  // The neck sits 0.10 along the spine from the chest and the skull 0.16 above
  // the neck, and both of those are measured on a spine that is UPRIGHT. Recline
  // the trunk for a slide and the spine's contribution rotates out of vertical:
  // the same two bones put the skull ~50 mm lower over the chest than they do
  // standing, which is 50 mm straight into the collar. Give PART of it back
  // along the head's own up axis, which is very nearly vertical in a slide by
  // construction (`head_up` above), so this lifts the skull off the carrier
  // without laying it back.
  // 0.022, not 0.050: giving back the FULL 50 mm plus 75 % of the forward
  // displacement (below) photographed a giraffe — a full head-length of bare
  // neck cylinder between collar and chin from any side camera, shipped in a
  // trailer cut and caught by a viewer inside a day. The two terms trade
  // against each other (both pull the skull away from the collar), so they
  // are tuned TOGETHER against a close side camera on a live slide: the
  // skull should ride just clear of the collar, not stand on a stalk.
  v3 head = v3_add(v3_add(neck, v3_scale(head_up, 0.16f + 0.022f * a->slide_s)),
                   v3_scale(head_f, 0.02f));
  // ...AND IT COMES FORWARD OUT OVER THE CHEST, which is the half that lifting
  // it could never fix. The recline is applied to `spine`, and `chest` and
  // `neck` are both built along it (0.40 and 0.10), so a slide carries the head's
  // ANCHOR half a spine-length BACKWARD as well as down. Raising the skull from
  // there just slides it up the front of the pack: measured, `figbury head`
  // read 1.5% with the jaw 7 mm in — nearly clear — while the picture still
  // showed a helmet fused into the backpack with no neck anywhere, because the
  // defect is not the skull being inside the trunk, it is the skull being
  // BEHIND the shoulder line where it has nothing to sit on but the pack.
  // Ray parity cannot see that and a side camera sees nothing else.
  // A man sliding feet-first does the opposite of what this was doing: the trunk
  // goes back and the neck flexes FORWARD, so the head stays over the chest with
  // the eyes down-range. Give back PART of what the recline carried back, along
  // the trunk's own forward — 0.50 is the chest+neck fraction of the spine.
  // 0.45, not 0.75: see the giraffe note above — at 0.75 the skull hung so far
  // ahead of the reclined collar that the neck spanned a long bare diagonal.
  head = v3_add(head, v3_scale(F, 0.50f * rec_mag * a->slide_s * sfd * 0.45f));
  // CHEEK WELD, and it is three components rather than one, because an eye that
  // is not ON the sight line is a man holding a rifle up in front of his face.
  // Measured from figm's own `weld` rows before this: 161 mm above the axis
  // and 85 mm to the weak side of it, 182 mm off in total. The head comes
  // DOWN to the sight, ACROSS to the strong side where the weapon actually
  // rides, and FORWARD —
  // the forward term is what keeps the jaw out of its own collar while the
  // other two take 90 mm out of the head's height, and it is also what a real
  // shooter does: the neck reaches over the stock, the chin does not sink into
  // the chest. The cant that goes with it is `anim_head_cant`.
  // ...and the crouch term is the one the eye actually notices. Crouching
  // hunches the spine 0.46 rad on top of the ADS lean, which pitches the trunk
  // and drops the head BELOW the sight line — measured, the eye went from
  // 21 mm above the axis standing to 8 below it crouched on the carbine and 36
  // below on the sniper, which is the "eye sitting oddly in front of the scope"
  // a crouched figure reads as. 30 mm of lift, scaled by BOTH blends so it can
  // only exist in a crouched aim, puts crouched within 2 mm of standing.
  float weld_dn = -0.090f * a->ads_s + 0.030f * a->ads_s * a->crouch_s;
  // ...AND THE WELD'S DROP IS TAKEN BACK IN A SLIDE, for the same geometric
  // reason the crouch tuck below is, only worse. A slide reclines the trunk, so
  // the chest — and the neck sitting 100 mm along that reclined spine — is
  // already low and already behind the head; the cheek weld's 90 mm then has
  // nowhere to go but into the collar. Measured with `figbury head` on a
  // sliding puppet with the weapon up: **63 % of the skull inside the trunk and
  // the jaw 111 mm INSIDE it**, against 0.9 % standing and 0.2 % crouched. From
  // a side camera that is a helmet resting straight on the plate carrier with
  // no neck anywhere, which is exactly the read the crouch work already fixed
  // once and this path was never included in.
  // Only the DROP is taken back. The forward and across components stay at full
  // value: the forward one is what keeps the jaw out of its own collar, and
  // deleting it would trade a buried skull for a craned one.
  // Written as a subtraction off the standing value rather than as a rescaled
  // expression so that at slide_s == 0 it is bit-identical to what it replaced —
  // this solver amplifies one ULP into centimetres, and every non-sliding pose
  // in the battery has to stay byte-exact.
  weld_dn -= weld_dn * (0.85f * a->slide_s);
  head = v3_add(head, v3_add(v3_scale(U, weld_dn),
                    v3_add(v3_scale(Rc,   0.068f * a->ads_s),
                           // 0.135, not 0.100: `figm weld` reports where along
                           // the weapon the eye sits, and at 0.100 it was
                           // 76 mm in front of the butt pad — a cheek on the
                           // PAD rather than on the comb. 109 mm now, for both
                           // rifles, which is what a length of pull is for.
                           v3_scale(F,    0.135f * a->ads_s))));
  // Crouching, the head TUCKS onto the shoulders. A squatting shooter does not
  // hold his chin at full standing extension above his own pelvis, and this is
  // the cheap half of putting the rendered head back inside the crouched HEAD
  // HITBOX (which tops out at eye + 0.18 = 1.18 m). Taking all of it in the hip
  // instead would need ANIM_HIP_CR 0.33 — a heels-up squat, and a much bigger
  // change to how a crouch reads and moves.
  // ...and a round takes the head with it, by however much lever it had: a
  // helmet hit snaps the head, a thigh hit barely moves it.
  // The crouch drop is TAKEN BACK during a slide. A slide is crouched by
  // definition, so the two used to stack — 75 mm of crouch plus the tuck below
  // — and the head sank into its own shoulders with no neck left anywhere: from
  // the boot screen's tracking camera the figure read as a man whose head had
  // been pushed into his chest. The recline already lowers the head; taking it
  // twice is what looked broken.
  // ...and it is taken back AGAIN when the weapon comes up, for a reason that is
  // geometry rather than taste: nothing in the hold lowers the stock in a crouch
  // (the gun anchor's only crouch term pushes it FORWARD), so a head that drops
  // 75 mm on top of the 26 mm cheek weld is not welded to anything — it is 75 mm
  // BELOW its own stock, and it spends that distance inside its own collar.
  // Measured with `figbury head`: standing 0.0% of the skull in the trunk,
  // crouched 2.9% with the jaw 4 mm clear, crouched AND shouldered 13.1% with
  // the jaw 20 mm INSIDE it — a helmet sitting straight on the plate carrier
  // with no neck anywhere. A shouldered shooter's head is on the stock wherever
  // the stock is; the tuck is what he does when the rifle is DOWN.
  head = v3_add(head, v3_scale(U, -0.075f * a->crouch_s * (1.0f - 0.80f * a->slide_s)
                                                        * (1.0f - 0.85f * a->ads_s) -
                                  0.048f * hs_ * (0.30f + 0.90f * hlev)));
  head = v3_add(head, v3_scale(Rc, -0.052f * hs_ * hrol));
  // Slide: the head TUCKS with speed instead of riding serenely on the recline.
  head = v3_add(head, v3_scale(U, -0.016f * a->slide_s *
                                  f_clamp(a->slide_v, 0.0f, 1.0f)));
  // Gaze stabilisation: a walking human's head barely rises and falls — the
  // ankles, knees and spine eat the pelvis bob, which is why a head that bobs
  // the full amount instantly reads as a puppet on a stick. Give it back most of
  // the drop the pelvis just took, and let the neck/spine absorb the difference.
  // Gaze stabilisation, but MEASURED: giving back 0.70 of the drop left the head
  // moving 10.5 mm at a 1.4 m/s walk and 18.2 mm at 8 m/s against a pelvis that
  // moves 37 and 60 mm. Published human gait has the head within 10-25% of the
  // CoM's own vertical excursion (about 40 mm walking), i.e. the neck damps the
  // bob, it does not delete it — and a head that holds still over a bobbing
  // pelvis is exactly the "gliding" read. 0.40 leaves the head at ~22/36 mm.
  head.y += bob_drop * 0.40f;
  neck.y += bob_drop * 0.20f;

  // Legs: ankle comes from the foot controller, knee from IK. In a slide the
  // lead leg rides heel-first (toe pitched up, knee soft), the trail leg
  // folds tight with the knee pointing up.
  // (which leg leads a slide comes from a->slide_ld — the same blend the foot
  //  controller uses, not a second boolean copy of the same decision)
  for (int i = 0; i < 2; i++) {
    float s = i ? 1.0f : -1.0f;
    // Pelvis rolls with the gait — the hip drops on the SWING side (Trendelenburg
    // in reverse). ~5 degrees; without it the hip line is a rigid bar and the
    // whole walk reads mechanical however good the feet are.
    // Only ever DROPS a hip, never lifts one: lifting the stance hip lengthens
    // the leg it is standing on, and the reach clamp then pays for it by pulling
    // the planted foot off the ground (measured: 38 of 278 stance samples floated
    // past 2 cm the moment this was symmetric).
    float proll = fminf(s * sinf(bph * F_TAU), 0.0f) * 0.055f *
                  movT * ground * (1.0f - a->slide_s);
    // Pelvic transverse rotation (+-6 deg with the gait): the shoulders are
    // welded to the rifle, so the counter-rotation that sells a run lives in
    // the HIPS — visual only, the foot homes never see it.
    float pyaw2 = sinf(bph * F_TAU) * 0.11f * movT * ground *
                  (1.0f - a->slide_s) * (0.35f + 0.65f * runT);
    v3 Rp = {{cosf(ly + pyaw2), 0, sinf(ly + pyaw2)}};
    v3 hoff = v3_add(v3_scale(Rp, s * ANIM_HIP_HALF), v3_scale(U, -0.07f + proll));
    // The pelvis takes its share of the lean (see LEAN_PELV_K): the hip on the
    // side the trunk went drops, the one carrying the weight hikes.
    if (fabsf(a->lean_s) > 1e-4f) {
      v3 la = {{sinf(yaw), 0, -cosf(yaw)}};
      hoff = v3_rot_axis(hoff, la, a->lean_s * LEAN_ANGLE * LEAN_PELV_K);
    }
    v3 hip = v3_add(pelvis, hoff);
    v3 ank = a->foot[i].pos;
    v3 dd = v3_sub(ank, hip);  // never let a segment stretch past its bones
    // Strike pre-flex: a runner lands on a knee already bent 15-20 deg, not a
    // pole. Early stance shortens the effective leg, easing out by mid-stance.
    float preflex = (a->foot[i].t >= 1.0f)
                        ? 0.045f * (1.0f - a->stance[i]) * runT * ground
                        : 0.0f;
    float dl = v3_len(dd), mx = ANIM_LEG * (ANIM_REACH_K - preflex);
    if (dl > mx) ank = v3_add(hip, v3_scale(dd, mx / dl));
    float fy = a->foot[i].yaw;
    v3 footF = {{sinf(fy), 0, -cosf(fy)}};
    // Trail-leg knee points up when folded; on a butt-slide it flattens
    // toward the lead leg's soft bend. Backward, both heels dig — toes up.
    float trail_u = 0.68f - 0.34f * a->slide_bk;
    float sld = i ? a->slide_ld : 1.0f - a->slide_ld;
    float uw = 0.22f + 0.5f * a->air_s +
               (sld * 0.45f + (1.0f - sld) * trail_u) * a->slide_s;
    v3 pole = v3_norm(v3_add(v3_scale(footF, 1.0f - 0.55f * a->slide_s),
                             v3_scale(U, uw)));
    // --- ankle pitch -------------------------------------------------------
    // Swinging: toe pointed just after toe-off, toe up for the heel strike.
    // Toe-DOWN is gated on how far the foot has actually left the ground, or it
    // drives the sole through the floor (the boot reads as sliced open).
    float want = 0.0f;  // target ankle pitch in radians, positive = toe UP
    if (a->foot[i].t < 1.0f) {
      float st = 2.0f * f_clamp(a->foot[i].t, 0.0f, 1.0f) - 1.0f;
      if (st < 0.0f)
        st *= f_clamp((a->foot[i].pos.y - base.y - ANIM_ANKLE_Y) / 0.045f, 0.0f, 1.0f);
      want = st * 0.55f * (0.45f + 0.55f * runT);
      // Backpedalling the foot stays pointed through the whole swing so it
      // arrives forefoot-first — that plant is the whole mincing-steps read.
      want = want * (1.0f - a->bwd) - 0.26f * a->bwd;
    } else if (a->air_s > 0.5f) {
      // Airborne: plantarflexed off the push (toe down), DORSIflexed on the
      // fall — the lead foot visibly reaches for the landing.
      want = -0.24f * f_clamp(a->vy / 4.0f, 0.0f, 1.0f) +
             0.30f * f_clamp(-a->vy / 4.0f, 0.0f, 1.0f);
    } else if (ground > 0.5f && movT > 0.05f) {
      // PLANTED: roll the foot through the contact instead of holding it flat.
      // A foot that stays flat for its whole stance is the loudest "this is an
      // animation" tell there is — real contact is heel strike, roll flat, heel
      // off. Stance progress comes from the gait phase: the foot's swing window
      // is [ws, ws+duty), so the rest of the cycle is its stance.
      float duty = anim_duty(a);
      float ws = i ? 0.5f : 0.0f;
      // PRESENTATION READS bph, NEVER THE RAW CLOCK. Stance progress drives the
      // ankle roll and the strike pre-flex (i.e. the leg-length clamp), and when
      // the PARTNER foot strands and adopts the cycle mid-window the raw phase
      // teleports: this foot's progress jumped 0.77 -> 0.00 in one tick, which
      // is 38 degrees of ankle-pitch target on a boot standing still on the
      // ground. `bob_lag` already carries that discontinuity for everything else.
      float wp = bph - ws;
      if (wp < 0) wp += 1.0f;
      float sp = f_clamp((wp - duty) / fmaxf(1.0f - duty, 0.05f), 0.0f, 1.0f);
      a->stance[i] = sp;
      float g = movT * ground;
      // Forward: heel strike, roll flat, heel off. Backward: ball strike and the
      // heel never comes down — the pitch stays negative for the whole contact,
      // pivoting about the toe, which is what keeps a mincing step on its toes.
      float fw = 0.0f;
      if (sp < 0.28f) fw = (1.0f - sp / 0.28f) * 0.30f * g;                // strike
      else if (sp > 0.42f) fw = -((sp - 0.42f) / 0.58f) * 0.60f * g;       // heel off
      // Backpedal is a ROCKER, not a held pointe: ball strike (toe down),
      // then the body rolls back over the foot and the ankle DORSIFLEXES
      // through stance — holding plantarflexion for the whole contact was
      // the one thing real backward gait does not do.
      float bk = -(0.18f - 0.30f * sp) * g;
      want = fw * (1.0f - a->bwd) + bk * a->bwd;
      // Crouch-walking rides the balls of the feet — the heels barely settle.
      want -= 0.16f * a->crouch_s * g;
    }
    // Sliding: the lead foot rides heel-first with the toe well up. Expressing
    // that as a PITCH rather than as a toe offset is what keeps the heel on the
    // ground — the pivot logic below rotates about whichever end is loaded, so a
    // toe-up foot lifts its toe instead of burying its heel (measured: the heel
    // used to sink 1.9 cm into the floor).
    want += (sld * 0.50f + (1.0f - sld) * (0.06f + 0.42f * a->slide_bk)) *
            a->slide_s;
    // Ease, never snap: the foot does not reach its plant at a fixed point of the
    // cycle (measured sp ~0.17 at 2 m/s), so switching from the swing formula to
    // the stance one on contact would jump the pitch by ~11 degrees in one tick.
    // Easing turns that jump into the heel-strike slap itself.
    a->fpitch[i] += (want - a->fpitch[i]) * ease_k(22.0f);
    float pitch_f = a->fpitch[i];
    // Rotating the foot about the ANKLE would drive the loaded end into the
    // ground; rotate it about the contact point that carries the weight — the
    // heel while the toe is up, the toe tip while the heel is up — so the part
    // touching the floor stays exactly where the foot controller planted it.
    if (fabsf(pitch_f) > 1e-4f) {
      v3 fr = v3_cross(U, footF);
      if (v3_dot(fr, fr) > 1e-6f) {
        fr = v3_norm(fr);
        // The outsole's OWN extremes (see the boot chain in scene_draw_figure):
        // heel rand at -0.086 along the foot axis, toe end at +0.194. These were
        // -0.064 and 0.200, i.e. a heel pivot 22 mm inside the rendered heel and
        // a toe pivot 6 mm past the rendered toe, so the "loaded contact point
        // never moves" claim was true of a boot that no longer existed.
        float d = pitch_f > 0.0f ? -0.086f : 0.194f;  // heel : toe tip
        // ...but a pivot is a CONTACT point, so it only exists while the boot is
        // ON the floor. Rotating a foot 30 cm up in a swing about a point on the
        // ground threw the ankle 106 mm (93 forward, 45 up): the swing then
        // lands ahead of the spot the controller planted, and a jump's tuck is
        // bent by its own ankle pitch. Fade the pivot back onto the ankle over
        // one boot height — a foot at contact is byte-identical to before.
        float con = 1.0f - f_clamp((ank.y - base.y - ANIM_ANKLE_Y) *
                                   (1.0f / 0.060f), 0.0f, 1.0f);
        v3 piv = v3_add(ank, v3_scale(footF, d * con));
        piv.y = ank.y + (base.y - ank.y) * con;
        ank = v3_add(piv, v3_rot_axis(v3_sub(ank, piv), fr, -pitch_f));
        footF = v3_norm(v3_rot_axis(footF, fr, -pitch_f));
      }
    }
    P->hip[i] = hip;
    P->ankle[i] = ank;
    P->knee[i] = ik_solve(hip, ank, ANIM_THIGH, ANIM_SHIN, pole);
    // NO -10 mm DROP ON THE TOE JOINT. It was a cosmetic tilt and it cost the
    // sole contract: the boot is swept along normalize(toe - ankle), so a toe
    // joint 10 mm below the ankle tilts the whole sole plane 3.3 degrees
    // nose-down at NEUTRAL — measured with `kine`, a standing boot's toe end sat
    // 8.5 mm underground. The joint is the toe TIP here (175 mm forward of the
    // ankle), and on a level foot the tip is level with nothing above it.
    P->toe[i] = v3_add(ank, v3_scale(footF, 0.175f));
  }

  // Arms hold the rifle two-handed on the aim axis; recoil pulls it back.
  // THE WEAPON DOES NOT FOLLOW THE EYES ALL THE WAY. Looking at his own boots a
  // man tips his head and his eyes; the rifle goes to a steep low ready and
  // stops there. With the weapon on the full look pitch, the shoulder camera
  // looking straight down filmed a rifle hanging muzzle-down at the hip (the
  // carry's 34 degrees then tipped it PAST vertical, toward the legs) and the
  // firing elbow thrown out sideways by a pole whose "back" term had turned
  // into "up": a chicken wing reaching for a gun it was no longer holding.
  // The same on the way up. So the pose aims on a soft-limited pitch — linear
  // through the normal range, easing into ~46 deg down and ~40 deg up — while
  // the head (hp, below) and the sim aim keep the real one.
  float gp = pitch;
  if (gp < -0.45f) gp = -0.45f - 0.35f * tanhf((-gp - 0.45f) / 0.35f);
  else if (gp > 0.40f) gp = 0.40f + 0.30f * tanhf((gp - 0.40f) / 0.30f);
  v3 aimd = {{sinf(yaw) * cosf(gp), sinf(gp), -cosf(yaw) * cosf(gp)}};
  // CARRY: a soldier who is not aiming does not walk around shouldered. The
  // gun frame (NOT the sim aim — bots only fire once ready_s has raised)
  // rotates down into a low-ready when still and into a diagonal tucked
  // sprint carry with speed; everything hanging off the gun frame (wrists,
  // muzzle, tracer anchor for third persons) follows for free.
  v3 gd = aimd;
  v3 carry_pin = {{0, 0, 0}};
  float rl = 1.0f - a->ready_s;
  if (rl > 0.003f) {
    v3 gr0 = v3_cross(aimd, U);
    gr0 = v3_dot(gr0, gr0) > 1e-6f ? v3_norm(gr0) : Rc;
    float down = (0.60f - 0.22f * runT) * rl;      // ~34 deg still, ~22 sprinting
    // ...but never past ~60 deg below level in all: aiming low already, the
    // carry only has to take the rest of the way to a steep low ready.
    down = fminf(down, fmaxf(1.05f + gp, 0.0f) * rl);
    // Crossed enough that the fore-end actually arrives at the weak hand: at
    // the old 4.6 deg the guard stayed on the strong side and the support
    // wrist target sat at 0.60 m from a 0.58 m arm — a straight rod of an arm
    // pointing at a rifle it never touches.
    float yin = (0.50f - 0.10f * runT) * rl;       // muzzle across the body
    gd = v3_rot_axis(aimd, gr0, -down);
    gd = v3_norm(v3_rot_axis(gd, U, yin));
    // PIN THE BUTTSTOCK THROUGH THE ROTATION. Both rotations above are applied
    // through the ANCHOR, which sits at the grip — so dropping the muzzle 34
    // degrees and crossing it 29 swings the buttstock the other way, up and
    // rearward, straight into the ribs. Measured with `figbury`, the AR's butt
    // pad sat 71 mm INSIDE the trunk with 14.6% of the whole weapon buried in
    // it: from outside, a rifle sawn off behind the ejection port. A real low
    // ready pivots about the SHOULDER POCKET — the butt stays and the muzzle
    // falls — so the same two rotations are applied to the butt's own offset
    // from the anchor and the anchor is translated by the difference.
    // The offset is a MEASUREMENT of the model, not a knob: pose_gun_frame puts
    // the anchor 0.005 ahead of and 0.098 under the grip station, GUN_GRIP is
    // ~(0, -0.036, -0.015) averaged over the two weapons, and the butt pad sits
    // at model (0, 0.017, -0.198) drawn at GUN_GIRTH_3P. One number for both
    // weapons because the POSE does not know which one it is holding — the AR's
    // own figure is -0.243/-0.045 and the sniper's -0.201/-0.048.
    // UP DID NOT FOLLOW THE MODEL, AND THAT IS MEASURED RATHER THAN LAZY.
    // Raising the AR's stock group onto the receiver's axis (see the buffer
    // tube in gun_build) moved that pad 7 mm in model units — 7.35 mm at
    // GUN_GIRTH_3P — so the AR's own figure went -0.052 -> -0.045 and the
    // MIDPOINT this constant is would follow to -0.046. Swept against
    // `figbury`, that midpoint is not where the proof's optimum is: over
    // -0.050/-0.048/-0.046/-0.044 the AR's buried fraction is 0.6 % at every
    // one of them and only its deepest vertex moves (12.6/12.1/11.7/11.2 mm),
    // while the SNIPER — whose stock did not move — goes 0.3/0.5/0.7/0.8 % and
    // 4.6/5.1/5.5/6.0 mm. A millimetre of the AR's worst vertex is not worth
    // doubling how much of the sniper is inside its owner, so the number stays
    // where the measurement says and this comment records the offset it is
    // knowingly carrying.
    #define POSE_BUTT_FW (-0.234f)
    #define POSE_BUTT_UP (-0.050f)
    v3 up0 = v3_norm(v3_cross(gr0, aimd));
    v3 b0 = v3_add(v3_scale(aimd, POSE_BUTT_FW), v3_scale(up0, POSE_BUTT_UP));
    v3 b1 = v3_rot_axis(v3_rot_axis(b0, gr0, -down), U, yin);
    carry_pin = v3_sub(b0, b1);
  }
  // A SLIDE IS A ONE-HANDED CARRY, MUZZLE UP. The support hand is already on
  // the ground behind the hip — that contact is most of what sells a slide, and
  // it is what leaves the weapon in one hand — but the weapon itself kept the
  // low-ready angle, so it lay along a reclined belly and vanished: filmed from
  // the tracking camera the figure was a crouched mass with no rifle anywhere
  // in it. What a man actually does with a rifle he cannot control two-handed
  // is get the muzzle out of the way and up. 66 degrees about the weapon's own
  // right, blended with the slide, and the anchor rises with it so the receiver
  // clears the chest instead of ploughing through it.
  if (a->slide_s > 0.001f) {
    v3 gr1 = v3_cross(gd, U);
    gr1 = v3_dot(gr1, gr1) > 1e-6f ? v3_norm(gr1) : Rc;
    gd = v3_norm(v3_rot_axis(gd, gr1, 1.15f * a->slide_s));
  }
  // Sliding, the torso reclines and its belly swings forward into the space the
  // rifle occupies, so the hold is pushed out and up while the recline lasts.
  // Crouching brings the weapon UP and out rather than tucking it against the
  // chest: with the forearm resting flat on the plate carrier the two surfaces
  // become coplanar, which is precisely what z-fights (a soak found 5 such
  // triangles the moment bots started crouching).
  // CARRIAGE. The weapon used to sit on the sagittal midline 70 mm to the right
  // of the spine with both wrists on the bore axis, which is why the two hands
  // measured the same x to four decimal places on every tick: a symmetric
  // two-handed clamp in front of the sternum. Nobody holds a rifle that way. The
  // buttstock goes in the STRONG-SIDE shoulder pocket, so the whole weapon lives
  // 115 mm off the midline; the firing hand is at the grip with the elbow up and
  // out, and the support hand reaches across and forward under the handguard
  // with its elbow down and inboard. Bringing the hold 35 mm closer to the chest
  // also puts both arms back inside their own reach: the humerus grew 45 mm in
  // the proportion pass and, with the hand targets left where they were, all of
  // that went into elbow droop — the arms came out straighter than before.
  // THE STOCK LIVES IN THE SHOULDER POCKET, and the anchor math has to put it
  // there: the butt pad sits ~0.24 m behind the grip and ~0.10 m below this
  // anchor, so at ready the anchor must ride at Rc ~0.15 (the pocket is at
  // ANIM_SHO_HALF 0.212, minus half the pad's width and the cheek's inboard
  // pull) and only slightly below the shoulder line — the old -0.075 U parked
  // the butt a full 17 cm under the pocket, floating in front of the plate
  // carrier with the head aiming over it. ADS pulls it inboard onto the cheek
  // and up to the eye line. The carry (rl) drops and crosses the weapon but
  // must NOT pull it closer (-0.06 gd drove the rotated butt INTO the chest).
  v3 gun_o = v3_add(v3_add(chest, carry_pin),
             v3_add(v3_scale(gd, 0.185f + 0.175f * a->ready_s + 0.000f * a->ads_s
                                                + 0.105f * a->slide_s
                                                + 0.045f * a->crouch_s
                                                - 0.05f * runT      // tucked in at sprint
                                                - 0.05f * a->fire_k),
             // The carry rides 165 mm FURTHER OUTBOARD and 30 mm higher than
             // the ready, where it used to ride 65 mm inboard and 110 mm lower.
             // Measured with figbury: the AR went from 14.6% of the weapon
             // buried in the trunk to 5.7%, the deepest point from 95 mm to 46,
             // and the butt pad from 20 mm inside to clear of every part of the
             // body. What is left is the magazine's floor plate against the
             // strong-side thigh, which is what a carried rifle does.
             // ...and ADS pulls it 90 mm INBOARD, not 50: the head can only
             // come so far across before the neck is doing something a neck
             // does not do, so the last of the lateral gap between eye and
             // sight line is closed from the weapon's side.
             v3_add(v3_scale(Rc, 0.150f - 0.072f * a->ads_s + 0.165f * rl
                                       + 0.070f * a->slide_s),
             // ...and 65 mm STRAIGHT OUT OF THE CHEST at the low ready, along
             // the trunk's own forward rather than along the (dropped, crossed)
             // bore. The outboard push above moves the weapon ACROSS the plate
             // carrier and never off it, which the AR survives — its receiver is
             // flat — and the sniper does not: the optic tower stands 90 mm over
             // the bore and the carry rotation tips that tower INBOARD, straight
             // into the sternum. `figbury` on the sniper's low ready: 12.9% of
             // the whole weapon inside the trunk, deepest 40 mm at chest height,
             // i.e. the scope. A man at the low ready holds the rifle in front of
             // himself; this is that 65 mm, and it is the one term in the hold
             // that is perpendicular to the body rather than along the weapon.
             v3_add(v3_scale(F, 0.065f * rl),
                    // THE STOCK STAYS IN THE POCKET WHEN THE RIFLE COMES UP.
                    // Going from ready to ADS used to LIFT the whole weapon
                    // 170 mm and pull it 95 mm inboard onto the cheek, which
                    // took the butt pad clean off the shoulder: measured with
                    // `puppet ads`, it sat 43 mm clear of the body in ADS
                    // against an 11 mm bite at the ready — a rifle held in
                    // front of a man rather than against him. Real shouldering
                    // rotates about the pocket and brings the HEAD down to the
                    // stock; the pose has the bore locked to the aim axis
                    // already, so all that is left is not to translate away
                    // from the shoulder. 0.110/0.050 puts the ADS bite at
                    // 11.8 mm, i.e. exactly where the ready pose has it.
                    // ...and 0.185/0.090 is where the WELD put it. The stock
                    // staying seated was necessary and not sufficient: `figm
                    // weld` measured the eye 182 mm off the sight line (161 up,
                    // 85 to the weak side), i.e. a rifle held up in FRONT of a
                    // face rather than behind an eye. The last 35 mm of lift and
                    // 40 mm of inboard pull close the half of that gap the head
                    // cannot reach without craning, and the butt still bites
                    // 10 mm into the deltoid — seated, not floating.
                    v3_scale(U, -0.020f + 0.205f * a->ads_s + 0.20f * a->slide_s
                                + 0.070f * rl)))));   // ...and 70 mm of carry LIFT,
             // not 30: the shortened magazine still dipped 49 mm into the
             // strong-side thigh at the low ready, which is the one part of a
             // carried rifle that hangs into a leg. 22 mm now — a magazine
             // touching a trouser, which is what a carried rifle does.
  // A braced weapon is held steady against the body's bob for the same reason
  // the head is: the shooter is keeping the sights on a point, not riding the
  // hips. The arms take up the slack through the elbow IK.
  gun_o.y += bob_drop * 0.55f;
  // Sliding at a deep sideways recline the push-out ran up the tilted chest
  // axis and parked the rifle beside the helmet. The grip stays below the
  // chin, whatever the recline does.
  if (a->slide_s > 0.05f) {
    float gy_max = head.y - 0.22f;
    if (gun_o.y > gy_max) gun_o.y += (gy_max - gun_o.y) * a->slide_s;
  }
  // Small lateral wander with the stride: a carried weapon is never perfectly
  // still relative to the body. (The third-person weapon is decoupled from the
  // tracer origin, which leaves the eye, so this cannot desync the shot.)
  gun_o = v3_add(gun_o, v3_scale(Rc, sinf(bph * F_TAU) * 0.014f *
                                     movT * ground));
  gun_o.y -= 0.11f * a->hdown + 0.05f * a->land;  // muzzle dips with the landing
  // Air: the arms are ALIVE in a jump — the rifle rides up with the push-off,
  // presses down as the fall builds (the body reaching for balance), and
  // tucks slightly closer the whole flight.
  {
    float rK = f_clamp(a->vy / 4.0f, 0.0f, 1.0f);
    float fK = f_clamp(-a->vy / 3.0f, 0.0f, 1.0f);
    gun_o = v3_add(gun_o, v3_scale(U, (0.060f * rK - 0.045f * fK) * a->air_s));
    gun_o = v3_add(gun_o, v3_scale(gd, -0.030f * a->air_s));
  }
  // Breathing shows up in the weapon too — a standing figure is mostly what you
  // see of a distant enemy, and a perfectly rigid one reads as scenery.
  gun_o = v3_add(gun_o, v3_add(v3_scale(U, sinf(a->idle_t * 1.60f) * 0.0045f * still),
                               v3_scale(Rc, sinf(a->idle_t * 0.92f) * 0.0035f * still)));
  // Wrist targets track the WEAPON's own stations, not the anchor plane: the
  // bore is ~0.10 below gun_o and the grip another 0.05 under that, so a
  // trigger wrist at -0.050 U hovered 10 cm above the grip and the support
  // wrist at -0.030 U sat ON TOP of the handguard — both forearms then came
  // down onto the rifle from above, the tray-carry look. The support hand
  // belongs UNDER the fore-end (GH_FORE is ~0.26 ahead of the anchor).
  // ...AND THE TRIGGER WRIST HAS TO CLEAR fig_hand's OWN NEAR CLAMP. This is
  // the number `pose_gun_frame` was once solved for and that quietly regressed
  // when GUN_GIRTH_3P went 1.22 -> 1.05 with the blading pass: a 14% smaller
  // weapon puts its grip 14% nearer the anchor, and the wrist ended up 25.3 mm
  // from the grip AXIS against fig_hand's 39.2 mm floor. Past that floor the
  // clamp fires, and what it does is slide the whole grip CYLINDER out from
  // under the rifle — so the palm and all five digits get built around a
  // cylinder the weapon does not draw, which is precisely "the finger is not
  // on the trigger". `vmtrig`'s own columns say it: `3p ar live` read
  // wrist=25.3 clamp=1 on every frame, in this tree and in HEAD. -0.100/-0.075
  // reads wrist=56.1 clamp=0, with the guard bound going +2.6 -> +4.9 and the
  // firing elbow still at 80 degrees over 375 mm of a 580 mm arm.
  v3 wriR = v3_add(gun_o, v3_add(v3_scale(gd, -0.100f),
             v3_add(v3_scale(U, -0.075f), v3_scale(Rc, 0.012f))));
  // In the CARRY the support hand slides BACK to the magwell end of the
  // guard: with the muzzle rotated 34 degrees down, the far fore-end station
  // sits ~0.76 m from the weak shoulder against a 0.58 m arm — the IK then
  // points a straight arm at a target it can never reach and the hand dangles
  // in space beside the rifle.
  // ...AND IT HAS TO BE INSIDE THE ARM. Measured with figm's elbow columns: at
  // the old 0.250 the support wrist sat 602 mm from the weak shoulder against a
  // 580 mm arm — the IK was OVER-EXTENDED, so it pointed a dead-straight arm at
  // a handguard it could not touch and fig_hand stretched the palm across the
  // gap. A straight arm is the one shape a supporting arm never makes. Pulled
  // back until it is INSIDE the arm in both states: 537 mm at the ready and 528
  // in the carry, i.e. 135 and 132 degrees of elbow — a bend the silhouette can
  // read from across the arena. 96% of the arm was not enough: near full
  // extension the IK's pole vector has almost no authority, so widening the pole
  // did nothing at all until the reach came down to about 92%.
  v3 wriL = v3_add(gun_o, v3_add(v3_scale(gd, 0.170f + 0.030f * rl),
             v3_add(v3_scale(U, -0.040f + 0.050f * rl),
                    v3_scale(Rc, -0.020f))));
  if (a->slide_s > 0.001f) {
    // Off hand braces ON THE GROUND behind the hip — that contact is most of what
    // sells a slide. Aim at the floor and clamp to arm's length rather than at a
    // fixed shoulder offset: clamping is what keeps the elbow bent instead of
    // straightening the arm into a rigid pole, and the torso roll above (-0.30 in
    // R) drops that shoulder far enough for the glove to actually get down there.
    // Placed relative to the BODY, not to the travel direction. Keying it off
    // `slide_pd` put the hand almost under the hips on a leftward slide (the
    // lateral and travel terms cancelled) — the hand is on the bracing arm's own
    // side and slightly behind the hip in every slide direction, which is where
    // an arm that length can actually plant.
    v3 want = v3_add(base, v3_add(v3_scale(R, -0.34f), v3_scale(F, -0.20f)));
    // The wrist rides a fist-height over the floor, so the KNUCKLES are the
    // contact — this is the hand the player reads as dragging on the ground
    // (at 0.004 the old flat-palm wrap wound the fingers under the floor).
    want.y = base.y + 0.072f;  // an override, not an offset: R and F are horizontal
    // The hand plants hard while the slide is fast and LIFTS as it bleeds out
    // (first beat of the exit: hand up, then rock over the lead foot, then
    // rise) — a brace held at walking speed reads as glued.
    float bk2 = a->slide_s * f_clamp(0.42f + 0.75f * a->slide_v, 0.0f, 1.0f);
    wriL = brace_hand(chest, Rc, want, 0.80f * a->slide_s, wriL, bk2);
  }
  // Hard landing: a hand goes down. `land` is the impact impulse (1.0 ~ 10 m/s),
  // so this only fires on a real drop, not on every step.
  if (a->hdown > 0.02f && a->slide_s < 0.2f) {
    v3 want = v3_add(base, v3_add(v3_scale(R, -0.30f), v3_scale(F, 0.26f)));
    want.y = base.y + 0.045f;
    // Note the 0.20 roll here against the 0.24 the shoulder line below uses for
    // the same hdown — historically two different approximations, and changing
    // either moves the braced hand, which is the "11 cm off the ground" defect.
    wriL = brace_hand(chest, Rc, want, 0.20f * a->hdown, wriL, a->hdown);
  }
  // Torso roll has to move the SHOULDER LINE, not only the spine: `Rc` is built
  // from a yaw and is therefore always horizontal, so a reclined, rolled torso
  // still carried perfectly level shoulders — and the bracing arm could never
  // reach the ground however far the spine tipped.
  float troll = 0.80f * a->slide_s + 0.24f * a->hdown;
  float trc = cosf(troll), trs = sinf(troll);
  // BLADE THE SHOULDERS. This is the one thing the stance was missing and the
  // reason every other number fought every other number: with the shoulder line
  // held SQUARE to the target, the butt pad can be in the shoulder pocket or the
  // support hand can be on the handguard, but never both. The arithmetic says so
  // — the AR's fore-end sits 0.61 m ahead of its butt at GUN_GIRTH_3P and the
  // figure's arm is 0.58, so a pocketed stock puts that hand 0.72 m from a weak
  // shoulder that cannot reach 0.58. A real shooter solves it by turning: the
  // support shoulder leads and the firing shoulder drops back, which brings the
  // weak hand 100 mm nearer the handguard AND opens the pocket the stock sits
  // in. 30 degrees on the shoulder line alone (the deltoids hang off these
  // points, so they follow), eased in with the carry -> ready blend so a walking
  // figure stays square and a shouldered one is bladed.
  // ...and a RUNNING figure blades too, which the carry-gated version could not
  // express: at `ready_s` 0 the shoulders were square, and square shoulders are
  // exactly the arithmetic above with no solution. Measured at a 7 m/s sprint,
  // the support wrist sat 590 mm from a 580 mm arm and the elbow came out at
  // 162 degrees — a dead-straight arm pointing at a handguard it cannot touch,
  // the same defect the shouldered stance was fixed for and still live in the
  // one pose a player spends most of a round in. A man running with a rifle
  // across his body turns his shoulders into it; 0.20 rad is a fifth of the
  // shouldered blade and it is enough.
  float blade = 0.52f * a->ready_s + 0.20f * runT * (1.0f - a->ready_s);
  float blc = cosf(blade), bls = sinf(blade);
  v3 fwdh = v3_norm((v3){{aimd.x, 0.0f, aimd.z}});
  for (int i = 0; i < 2; i++) {
    float s = i ? 1.0f : -1.0f;
    v3 sho = v3_add(chest, v3_add(v3_add(v3_scale(Rc, s * ANIM_SHO_HALF * trc * blc),
                                 v3_add(v3_scale(U, s * ANIM_SHO_HALF * trs),
                                        v3_scale(fwdh, -s * ANIM_SHO_HALF * bls))),
                                  v3_scale(spine, 0.05f)));
    v3 wri = i ? wriR : wriL;
    // Elbows are driven wide and down: a rifle stance reads as a triangle, and
    // an elbow tucked against the ribs is exactly where an upper arm starts
    // ploughing through the chest.
    // The two elbows do OPPOSITE things in a rifle stance and used to do the
    // same thing: both were driven wide and down, which is the shape of someone
    // holding a tray. The firing elbow rides UP and OUT (that is what opens the
    // shoulder pocket for the stock); the support elbow drops DOWN and INBOARD
    // under the handguard, tucked toward the ribs, carrying the weapon's weight.
    // ...but only at the READY. In the carry the firing hand is down at the
    // hip, and the same wide pole stuck the elbow straight out sideways at
    // shoulder height — a chicken wing that the shoulder camera, looking down
    // on the figure, shows from directly above. A man carrying a rifle at low
    // ready lets the firing elbow hang, a little out and behind him; it rises
    // and opens with the weapon as it comes up (ready_s). The "behind" term is
    // the HORIZONTAL forward: on the aim it turned into "up" looking down.
    float rk_ = a->ready_s;
    v3 pole = i ? v3_norm(v3_add(v3_scale(Rc, 0.45f + 0.85f * rk_ + 0.30f * a->crouch_s),
                          v3_add(v3_scale(U, -1.00f + 0.70f * rk_ + 0.55f * a->air_s),
                                 v3_add(v3_scale(aimd, -0.18f * rk_),
                                        v3_scale(fwdh, -0.35f * (1.0f - rk_))))))
                : v3_norm(v3_add(v3_scale(Rc, -0.60f - 0.20f * a->crouch_s
                                              - 0.85f * a->slide_s
                                              - 0.40f * a->air_s),
                          v3_add(v3_scale(U, -0.95f + 0.75f * a->slide_s
                                              + 0.50f * a->air_s),
                                 v3_scale(aimd, -0.10f - 0.30f * a->slide_s))));
    // (airborne both elbows flare up and out — the braced-in-flight look)
    // A WRIST NEVER GETS FURTHER FROM ITS SHOULDER THAN THE ARM CAN REACH, and
    // the guard is set at 96% rather than 100 for the reason this file already
    // records: near full extension a two-bone solve is straight whatever the
    // pole vector says, so the elbow stops being steerable before it stops
    // being solvable. Past the limit the target is pulled STRAIGHT BACK along
    // its own line to the shoulder — direction preserved, so a hand still
    // points where the pose meant it to and `fig_hand` bridges the last few
    // millimetres exactly as it does everywhere else.
    {
      v3 aw = v3_sub(wri, sho);
      float awl = v3_len(aw), amax = (ANIM_UARM + ANIM_FARM) * 0.96f;
      if (awl > amax) wri = v3_add(sho, v3_scale(aw, amax / awl));
    }
    P->sho[i] = sho;
    P->wri[i] = wri;
    P->elb[i] = ik_solve(sho, wri, ANIM_UARM, ANIM_FARM, pole);
  }

  P->ant_sway = a->ant_p;
  P->pelvis = pelvis;
  P->chest = chest;
  P->neck = neck;
  P->head = head;
  P->gun_o = gun_o;
  P->right = Rc;
  P->head_f = head_f;
  P->gun_f = gd;
  v3 gr = v3_cross(gd, U);
  if (v3_dot(gr, gr) < 1e-6f) gr = Rc;
  P->gun_u = v3_norm(v3_cross(v3_norm(gr), gd));

  // Lean, part 2 of 2: ONE rigid rotation of everything above the waist, about
  // the aim's horizontal forward axis. The pivot sits LEAN_ARM below the HEAD on
  // the head's own column, and that is the whole trick — it is the same lever the
  // first-person camera swings on, so the third-person head lands exactly where
  // the first-person eye does instead of the two views disagreeing about where
  // this man's face is. (`lean` in the harness prints both and the gap between
  // them.) The legs are deliberately untouched: the feet are planted, and the
  // knees absorb the hip shift through the IK they already run, which is what a
  // lean does to a real stance.
  if (fabsf(a->lean_s) > 1e-4f) {
    float ang = a->lean_s * LEAN_ANGLE;
    v3 axis = {{sinf(yaw), 0, -cosf(yaw)}};
    v3 piv = {{P->head.x, P->head.y - LEAN_ARM, P->head.z}};
    #define FIG_LEAN(p) ((p) = v3_add(piv, v3_rot_axis(v3_sub((p), piv), axis, ang)))
    for (int i = 0; i < 2; i++) {
      FIG_LEAN(P->sho[i]);
      FIG_LEAN(P->elb[i]);
      FIG_LEAN(P->wri[i]);
    }
    FIG_LEAN(P->chest);
    FIG_LEAN(P->neck);
    FIG_LEAN(P->head);
    FIG_LEAN(P->gun_o);
    #undef FIG_LEAN
    P->right = v3_rot_axis(P->right, axis, ang);
    P->head_f = v3_rot_axis(P->head_f, axis, ang);
    // The weapon travels and CANTS with the shooter but keeps pointing where the
    // entity is aiming — the round leaves along gun_f, and a leaning bot that
    // shoots 31 degrees off its own sight line is a bug, not a flourish.
    v3 gu = v3_rot_axis(P->gun_u, axis, ang);
    gu = v3_sub(gu, v3_scale(P->gun_f, v3_dot(gu, P->gun_f)));
    if (v3_dot(gu, gu) > 1e-6f) P->gun_u = v3_norm(gu);
  }
}

// A foot that is not STEPPING rides WITH the body: feed the velocity forward
// FIRST, then ease the local offset toward `tgt` — easing a world position toward
// a target that itself moves at body speed lags a constant v/k metres and drags
// the legs behind the figure. The feed-forward can also push the foot past a
// wall-clamped target, so BOTH the target and the resulting position are
// foot_clamp_world-rayed out of the solids. Written out twice (airborne, slide)
// before this, which is two places for the clamp discipline to be dropped from.
static void foot_ride(afoot_t *f, v3 base, v3 tgt, v3 vel, float yaw, float kf) {
  tgt = foot_clamp_world(base, tgt);
  f->pos.x += vel.x * TICK_DT;
  f->pos.y += vel.y * TICK_DT;
  f->pos.z += vel.z * TICK_DT;
  f->pos = v3_lerp(f->pos, tgt, kf);
  f->pos = foot_clamp_world(base, f->pos);  // never through geometry
  f->from = f->to = f->pos;
  f->t = 1.0f;
  f->yaw += wrap_pi(yaw - f->yaw) * kf;
}

// Advance the animation one 120 Hz tick from the entity's physics state.
static void anim_tick(anim_t *a, v3 base, v3 vel, float yaw, float pitch,
                      int grounded, int crouch, float tuck, int slide, float ads,
                      float lean, int ready, int ent) {
  a->live_t = g_tick;   // this pose is solved THIS tick — see anim_t.live_t
  a->prev_pose = a->pose;
  // The lean arrives already eased — the player's sim eases it, the puppet eases
  // it with the same two constants — so this is an ASSIGNMENT, not a second
  // filter. Easing here again would put the body a few centimetres from where
  // the camera says the head is, and the `lean` proof measures exactly that gap
  // (it reads 0.049 m mid-ramp with a 25/s follower here, 0.000 with this).
  a->lean_s = lean;
  // A third-person figure that is AIMING has to look like it: the stock comes
  // into the shoulder, the weapon rises onto the eye line and the head drops
  // onto the comb. Without this the body was byte-for-byte identical whether
  // the entity was hip-firing or scoped, which is information the player needs
  // — the enemy who is shouldered is the one about to hit you.
  a->ads_s += (ads - a->ads_s) * ease_k(14.0f);
  // Carry state: the raise snaps (~110 ms — a shot must never leave a lowered
  // rifle for long), the lower is deliberate (~0.7 s after the fight ends).
  {
    float want = (ready || ads > 0.05f) ? 1.0f : 0.0f;
    a->ready_s += (want - a->ready_s) * ease_k(want > 0.5f ? 20.0f : 4.5f);
  }

  // Smoothed motion signals; the raw-vs-smoothed velocity gap drives lean.
  v3 dv = v3_sub(vel, a->vel_s);
  float kv = ease_k(14.0f);
  a->vel_s = v3_add(a->vel_s, v3_scale(dv, kv));
  float hspd = sqrtf(a->vel_s.x * a->vel_s.x + a->vel_s.z * a->vel_s.z);
  // ASYMMETRIC, and only on the way DOWN. `a->spd` is the pose's idea of how
  // fast the body is going and it is doubly smoothed (vel_s at 14/s, then this
  // at 14/s again), which costs 225 ms on a stop: braking from 12 m/s the sim is
  // under 1.2 m/s by tick 346 and this still reads 6.5, so the figure holds a
  // full sprint pose — runT saturated, 12.6 deg of forward run lean, running
  // cadence — for 23 ticks after the body has physically halted, and the stop
  // has no beat in it at all. This SHORTENS that lag, it does not remove it:
  // measured on a 12 m/s stop the pose still reads 1.2 m/s 18 ticks (150 ms)
  // after the sim does, against 25 ticks (208 ms) before. What is left is
  // `vel_s`'s own 14/s stage, deliberately untouched — it also feeds bwd,
  // lean_f and lean_r, so making that asymmetric is a separate change with its
  // own battery. Raising the fall rate to 30/s costs nothing
  // anywhere else BY CONSTRUCTION: at steady speed hspd == spd and the branch is
  // a no-op, and while ACCELERATING hspd > spd so the rise keeps the old rate
  // exactly. Verified byte-identical over 200 ticks of steady 12 m/s run and
  // over the whole acceleration from rest.
  a->spd += (hspd - a->spd) * (hspd > a->spd ? kv : ease_k(30.0f));

  a->idle_t += TICK_DT;
  // WHICH FOOT IS CARRYING THE BODY. A stationary figure with one foot in the
  // air is in single support and its weight is over the other one; the pose had
  // no term for that at all, so a 180 turn in place moved the pelvis 5.8 mm
  // laterally and 7.6 mm vertically over 39 ticks of stepping — a rig rotating
  // about its own axis, with no weight in it. Eased at 6.5/s so the transfer
  // leads the step the way a real one does rather than snapping with `t`.
  {
    float w = 0.0f;
    if (a->foot[0].t < 1.0f && a->foot[1].t >= 1.0f) w =  1.0f;
    else if (a->foot[1].t < 1.0f && a->foot[0].t >= 1.0f) w = -1.0f;
    a->wshift += (w - a->wshift) * ease_k(6.5f);
  }
  a->vy += (vel.y - a->vy) * ease_k(16.0f);
  {
    float yv = wrap_pi(yaw - a->yaw_prev) / TICK_DT;
    a->yaw_vel += (yv - a->yaw_vel) * ease_k(12.0f);
    a->yaw_prev = yaw;
  }
  // THE THREE BIG POSE BLENDS ARE SMOOTHSTEPPED, NOT RAW FIRST-ORDER EASES.
  // An exponential ease is at its fastest on its FIRST tick, so a blend that
  // moves the pelvis 0.68 m (stand -> slide) put 90 mm of it into that one tick:
  // a 10.3 m/s vertical velocity step out of nothing, and the same shape on
  // every crouch. Keeping the ease as the linear driver and shaping it with
  // smooth01 leaves both endpoints and both time constants exactly where they
  // were, and makes the blend start and stop at zero rate. Same idiom as the
  // weapon's ads_lin -> ads_s. Measured peak pelvis jerk: slide entry
  // 161560 -> 69007 m/s^3, crouch in/out 48410 -> 9315.
  float kb = ease_k(9.0f);
  a->crouch_lin += (((crouch && !slide) ? 1.0f : 0.0f) - a->crouch_lin) * kb;
  a->crouch_s = smooth01(a->crouch_lin);
  // Slide blend is asymmetric: the drop-in snaps (~60 ms), standing back up
  // out of the skid is deliberately softer.
  float ksl = ease_k(slide ? 17.0f : 8.0f);
  a->slide_lin += ((slide ? 1.0f : 0.0f) - a->slide_lin) * ksl;
  a->slide_s = smooth01(a->slide_lin);
  float ks = ease_k(11.0f);
  a->air_lin += ((grounded ? 0.0f : 1.0f) - a->air_lin) * ks;
  a->air_s = smooth01(a->air_lin);
  // How much of base.y is leg tuck rather than ground. AIRBORNE this is an
  // ASSIGNMENT, not a follower: the caller's origin steps WITH the tuck, so
  // tracking it exactly is what makes the body continuous — the two steps
  // cancel to the bit and the world position does not move at all.
  // TOUCHDOWN is the one transition where the origin does NOT step: ent_move
  // puts the collider's floor on the ground and the tuck simply stops being a
  // tuck (player_tick zeroes it there). Neither obvious answer works alone.
  // Dropping the whole 0.60 m in one tick OVERSHOOTS, because the air pose has
  // the crouch hip-drop gated off and that gate only opens as air_s decays —
  // measured landing on a crate mid-run, the pelvis jumped +0.378 m and then
  // sank 0.37 m back over the next 90 ms, a bounce the old code did not have.
  // CLAMPING TO A CONSTANT IS THE SAME BUG WITH A SMALLER NUMBER. What is left
  // to pay is not (tuck - (HIP_Y - HIP_CR)): the crouch pose only delivers that
  // (HIP_Y - HIP_CR) as the air gate opens, and on the touchdown tick itself
  // air_s is still ~1, so almost none of it has arrived. Snapping root_off from
  // 0.60 to 0.168 therefore discarded 0.43 m of body height in ONE tick, against
  // a compensation that takes ~0.2 s to turn up. Measured on the PLAYER's own
  // rendered pose head (the puppet cannot test this — a bot's root_off is always
  // 0): +425 mm in one tick (51 m/s) on a jump-and-hold-crouch landing and
  // +401 mm on a 7 m drop, each followed by a 0.25 s sink back down. That is
  // 8% BIGGER than the -394 mm step the pre-round-1 build had at the crouch
  // PRESS — the teleport was relocated, not removed.
  // The whole residual now leaves on a decay, and the landing dip below is
  // scaled by what is still unpaid (a body already folded by an air tuck has no
  // knee travel left to absorb with), which is what stopped the pure decay from
  // putting the pelvis under the boots. 18/s rather than air_s's own 11/s is
  // measured, not assumed: it is the rate at which the two metrics stop
  // trading. Worst one-tick head step 425 -> 62 mm (soft) and 401 -> 40 mm
  // (hard), with min(pelvis - ankle) IMPROVED 0.165 -> 0.175 m and
  // 0.046 -> 0.066 m. Every other landing type is byte-identical: root_off is
  // 0 for a bot, for a plain jump and for a crouch held from the ground.
  if (!grounded) {
    // ...and the one airborne tick where the origin does NOT step with the
    // tuck is the TAKEOFF: on the ground tuck is already 0, so assigning it
    // here collapses whatever the previous landing's unwind still owes in ONE
    // tick, with nothing to cancel it. Measured on the buffered-jump input the
    // game explicitly supports (crouch held, jump_buf firing on the touchdown
    // tick): a +503 mm one-tick body step at the very next takeoff — 3.4x the
    // +149 mm the touchdown CLAMP used to bound it to, i.e. replacing that
    // clamp with an unwind moves the teleport HERE unless this branch keeps
    // unwinding too. The mid-air RELEASE must still snap (player_tick drops
    // pos.y by the same 0.60 m on that tick, and the two have to cancel), and
    // last tick's tuck is the only thing that tells a release from a takeoff.
    if (tuck > 0.0f)           a->root_off = tuck;   // baked: the origin stepped with it
    else if (a->tuck_p > 0.0f) a->root_off = 0.0f;   // released: the origin stepped back
    else                       a->root_off -= a->root_off * ease_k(18.0f);  // took off mid-unwind
  } else {
    a->root_off -= a->root_off * ease_k(18.0f);
    // The landing has happened; let go. NOT because it would fight `land` —
    // both consumers (the swing scissor and the airborne lift) sit inside the
    // airborne branch and nothing grounded reads `brace` at all — but so a jump
    // taken within a few ticks of a touchdown does not leave the ground already
    // braced for the next one.
    a->brace -= a->brace * ease_k(16.0f);
  }
  a->tuck_p = tuck;

  float fx = sinf(a->leg_yaw), fz = -cosf(a->leg_yaw);
  float rx = cosf(a->leg_yaw), rz = sinf(a->leg_yaw);
  // How much of the travel is BACKWARD in the hip frame. Backpedalling is its
  // own gait, not a forward walk played in reverse: short quick steps on the
  // balls of the feet, heels never coming down, weight back.
  {
    float fc = a->spd > 0.5f ? (a->vel_s.x * fx + a->vel_s.z * fz) / a->spd : 0.0f;
    a->bwd += (f_clamp(-fc, 0.0f, 1.0f) - a->bwd) * ease_k(7.0f);
  }
  float kl = ease_k(8.0f);
  a->lean_f += (f_clamp((dv.x * fx + dv.z * fz) * 0.075f, -0.30f, 0.30f) - a->lean_f) * kl;
  {
    // Accel term decays at constant speed, so a held strafe stood plumb: add
    // a steady lean INTO lateral travel (~7 deg at 6 m/s of lateral speed).
    float latv = a->vel_s.x * rx + a->vel_s.z * rz;
    float want_r = f_clamp((dv.x * rx + dv.z * rz) * 0.065f, -0.18f, 0.18f) +
                   f_clamp(latv * 0.020f, -0.14f, 0.14f);
    a->lean_r += (want_r - a->lean_r) * kl;
  }
  a->fire_k *= expf(-16.0f * TICK_DT);
  // ...and the other half of a firefight. Being SHOT had no body channel at all:
  // ent_damage set an aim kick (fl_y/fl_p), a tactic flag (hurt_t) and an error
  // bump, and the skeleton did not have a field for it — measured over 90 ticks
  // around one AR hit, the victim's trunk pitch changed 0.00 deg, its roll
  // 0.00 deg and its head 0.0 mm. The shooter gets a hitmarker, a kill feed row
  // and a four-rung audio ladder; the target's body said nothing, which is the
  // one place in this game where the reward loop has no animation in it.
  // ~0.4 s of decay, i.e. long enough to read and short enough not to be a
  // stagger. Now: 5.58 deg of trunk pitch, 2.26 deg of roll, 27.2 mm of head.
  a->hit_k *= expf(-11.0f * TICK_DT);
  // ...and the POSE chases the impulse, exactly as `land` chases `land_i` a few
  // lines below and for the same reason: a body does not fold in one tick.
  // Measured on an AR body hit: worst one-tick head step 17.7 -> 9.7 mm and
  // chest 29.0 -> 16.3 mm, at the cost of 26% of the peak (head duck 17.7 ->
  // 13.0 mm) — the onset is bought out of a decaying impulse, so it cannot be
  // free. Do NOT buy the amplitude back by raising this rate, which just
  // restores the step (140/s measured 16.3 mm, i.e. no onset at all); scale the
  // three call-site gains instead and re-film.
  a->hit_s += (a->hit_k - a->hit_s) * ease_k(45.0f);
  // The wobble clock. A fold that only decays reads as a push; what a hit
  // actually looks like on a standing man is a jolt that overshoots once and
  // rings out. It is a CLOCK rather than a second spring so the ring is the
  // same shape at every impulse size, and it advances only while there is an
  // impulse left to ring.
  a->hit_w = a->hit_k > 0.002f ? a->hit_w + 1.0f : 0.0f;
  // A LANDING HAS TO ARRIVE. Both stages were pure exponentials, so the body
  // never finished standing up: measured on a 7.4 m/s touchdown the pelvis
  // reached its 184.6 mm low 9 ticks in (right — that part is a real absorb) and
  // then took 275 ms to give half of it back, 675 ms for 90% and 1933 ms for
  // 99%. Three quarters of a second in which the figure is neither landing nor
  // standing and nothing on screen is readable as either — the exact opposite of
  // how a heavy body spends its time, which is long at the extremes and short in
  // the middle. Subtracting a floor as well as scaling makes the impulse reach
  // exactly zero in bounded time; the floors are ~0.4% of a full-scale impulse,
  // so the deep part of the curve is untouched (depth 184.6 -> 176.4 mm, low
  // point 9 -> 8 ticks) and only the invisible tail is cut: the pelvis's deficit
  // at +40 ticks goes 69.6 -> 27.8 mm, at +60 ticks 33.9 -> 3.3 mm, and both
  // impulses read exactly 0.000 by +80 ticks (667 ms) instead of dribbling for
  // two seconds.
  a->land_i = fmaxf(0.0f, a->land_i * expf(-7.0f * TICK_DT) - 0.0045f);
  a->hdown_i = fmaxf(0.0f, a->hdown_i * expf(-3.5f * TICK_DT) - 0.0060f);  // outlives `land`: a hand reaching for
                                        // the ground has to be readable, not a
                                        // blink
  // The impulse decays exactly as before; what the POSE sees CHASES it, so the
  // dip has an onset instead of a step. Rates chosen so the peak of each lands
  // within 4% of what the old direct assignment produced (measured 0.688/0.246
  // against 0.713/0.248) — the landing is just as deep, it arrives over 80 ms.
  a->land += (a->land_i - a->land) * ease_k(20.0f);
  a->hdown += (a->hdown_i - a->hdown) * ease_k(14.0f);
  // SPINE COMPLIANCE: everything above the waist LAGS the hips. Every vertical
  // event in the game — take-off, touchdown, the slide drop-in, standing back up
  // out of a skid — was transmitted to the skull 1:1, which is the loudest
  // "rig, not body" cue left in the figure: measured over a whole jump the
  // head-to-pelvis distance moved 3.98 mm against a pelvis taking 864 m/s^2 at
  // launch and 758 at contact, i.e. the torso is a rod bolted to the pelvis.
  // A damped 1.9 Hz follower on the pelvis's OWN vertical acceleration buys the
  // overlap and follow-through an impact needs for one float and no geometry:
  // -27.0 mm of give at take-off (the mass is left behind by the launch, back to
  // zero over 36 ticks), -15.7 mm at touchdown with a +2.4 mm rebound, and
  // +21.1 mm entering a slide, where the hips are yanked out from under the
  // chest at 9.2 m/s.
  // DEAD-BANDED at 90 m/s^2, and that is what makes it safe: the gait bob is a
  // tuned number (head 35.6 mm p-p against a 60.0 mm pelvis at 12 m/s) and a
  // 12 m/s run peaks at 70.8 m/s^2 of pelvis acceleration, a 4 m/s walk at 31.1,
  // so no gait can reach the band and every steady-state locomotion measurement
  // in this file stays byte-identical (verified: 200 ticks of 12 m/s run, 0
  // ticks differing).
  // Driven off LAST tick's solved pelvis — `a->pose` is still tick N-1 here,
  // `prev_pose` having just been copied from it — which is what keeps the filter
  // integrating exactly once per tick and leaves skel_solve a pure function of
  // the state. That is not tidiness: the `lean` proof RE-SOLVES a copy of the
  // player's anim to measure `gap`, and a filter stepped inside skel_solve would
  // be stepped twice by it. (`gap` itself is safe by construction — it is the
  // LATERAL difference of two solves that share this offset, and the lean pivot
  // is head.y - LEAN_ARM, so the rotation moves the head by LEAN_ARM*sin(ang)
  // whatever height the head is at.)
  {
    float pv = a->pose.pelvis.y;
    float vv = (pv - a->spc_py) * (1.0f / TICK_DT);
    float acc = (vv - a->spc_pv) * (1.0f / TICK_DT);
    a->spc_py = pv;
    a->spc_pv = vv;
    float ex = acc - f_clamp(acc, -90.0f, 90.0f);   // impacts only, never gait
    const float w0 = F_TAU * 1.9f, zt = 0.50f;
    a->spc_v += (-w0 * w0 * a->spc - 2.0f * zt * w0 * a->spc_v - 0.090f * ex) * TICK_DT;
    a->spc += a->spc_v * TICK_DT;
    // Hard stops, with the velocity killed at them: an impact bigger than any
    // measured here has to squash the trunk, never pull the chest through the
    // pelvis or the neck out of its collar. Asymmetric because a body compresses
    // further than it stretches.
    if (a->spc < -0.055f) { a->spc = -0.055f; if (a->spc_v < 0) a->spc_v = 0; }
    if (a->spc >  0.030f) { a->spc =  0.030f; if (a->spc_v > 0) a->spc_v = 0; }
  }

  if (slide && !a->was_slide) a->slide_t = 0;  // fresh slide: entry envelope
  a->was_slide = slide;
  if (slide) {
    a->slide_t += TICK_DT;
    float hl = sqrtf(vel.x * vel.x + vel.z * vel.z);
    if (hl > 0.5f) a->slide_dir = (v3){{vel.x / hl, 0, vel.z / hl}};
    a->slide_v += (f_clamp(hl / 8.0f, 0.0f, 1.2f) - a->slide_v) * ease_k(10.0f);
  } else {
    a->slide_v *= expf(-6.0f * TICK_DT);
  }
  if (v3_dot(a->slide_dir, a->slide_dir) < 1e-6f) a->slide_dir = (v3){{fx, 0, fz}};
  if (slide) {
    // Knees only bend one way: mirror the travel direction into the front
    // hemisphere for everything the LEGS do. Forward/lateral slides keep the
    // baseball pose; sliding backward becomes a butt-slide — legs out front,
    // hips leading into the travel, torso crunched forward over the knees.
    // Continuous across the lateral boundary (reflection at 0 is identity).
    float fdot = a->slide_dir.x * fx + a->slide_dir.z * fz;
    a->slide_bk = f_clamp(-fdot, 0.0f, 1.0f);
    a->slide_pd = a->slide_dir;
    if (fdot < 0) {
      a->slide_pd.x -= 2.0f * fdot * fx;
      a->slide_pd.z -= 2.0f * fdot * fz;
    }
  }
  if (v3_dot(a->slide_pd, a->slide_pd) < 1e-6f) a->slide_pd = (v3){{fx, 0, fz}};
  // WHICH LEG LEADS IS CONTINUOUS. It is decided by the sign of the skid's
  // LATERAL component, and a slide aimed straight ahead sits on that sign
  // change, so a player steering through dead-ahead mid-slide swapped the two
  // legs' targets outright — 0.43 m of `along` and 60 mm of foot height in one
  // step. Measured: the lead boot went 28 mm BACKWARD in world space while the
  // body moved 92 mm forward, and the two feet then scissored through each
  // other for ten ticks. Blend over a narrow band; the pose is symmetric, so
  // 50/50 at dead-straight is exactly what the boolean gave either side of it.
  {
    float lat = a->slide_pd.x * rx + a->slide_pd.z * rz;
    float w = smooth01(f_clamp(lat * (1.0f / 0.36f) + 0.5f, 0.0f, 1.0f));
    a->slide_ld += (w - a->slide_ld) * ease_k(12.0f);
  }

  // Landing: remember the fall speed for the dip, plant both feet where the
  // body came down — the next steps recenter them naturally.
  if (!grounded) a->fall = fminf(a->fall, vel.y);
  if (grounded && !a->was_grounded) {
    // Scaled up by the attack's own peak loss (see the ease rates above), so the
    // rendered dip depth is unchanged: an attack that starts at zero never
    // reaches the impulse's initial value while the impulse is already decaying.
    // A LANDING MADE NO SOUND. Measured, a 1.2 m jump touching down at
    // -7.27 m/s routed zero voices over 90 ticks — the eye dipped 55 mm and
    // nothing else happened, on a move every entity in the arena makes several
    // times a second (17 jumps in 6 s across 20 bots). It is pushed from HERE,
    // the animation solver's own ground-contact edge, so one push covers the
    // player, every bot and the puppet — the same reason EV_STEP lives here.
    if (-a->fall > 1.6f)
      ev_push((event_t){.type = EV_LAND, .a = base, .f = -a->fall, .src = ent});
    a->land_i = fminf(1.0f, -a->fall * 0.10f) * 1.75f;
    // Only a real drop, not every hop: 6 m/s is about a 1.8 m fall.
    a->hdown_i = f_clamp((-a->fall - 6.0f) * 0.22f, 0.0f, 1.0f) * 1.60f;
    for (int i = 0; i < 2; i++) {
      afoot_t *f = &a->foot[i];
      f->pos.y = base.y + ANIM_ANKLE_Y;
      f->from = f->to = f->pos;
      f->t = 1.0f;
      f->yaw = a->leg_yaw + (i ? 1.0f : -1.0f) * ANIM_TOE_OUT;
    }
  }
  if (grounded) a->fall = 0;   // covers the landing edge above too
  a->was_grounded = grounded;

  // Hip heading. Hips do NOT simply face the aim: a human running sideways turns
  // the legs toward where the body is actually travelling and twists the spine
  // back toward the target — up to about 55 degrees, which is the limit of a
  // comfortable torso twist. Hips locked to the aim while the body slides
  // sideways is what forces the far leg to step across its partner (measured:
  // the ankles crossed on 44% of ticks moving diagonally).
  //
  // Backpedalling is the deliberate exception: the turn fades out past ~100
  // degrees of offset, so walking backward keeps the hips square and mincing
  // instead of spinning them around.
  float want_yaw = yaw;
  if (!slide && a->spd > 1.0f) {
    float mvy = atan2f(a->vel_s.x, -a->vel_s.z);
    float dm = wrap_pi(mvy - yaw), ad = fabsf(dm);
    float w = f_clamp((ad - 0.18f) / 0.40f, 0.0f, 1.0f) *
              (1.0f - f_clamp((ad - 2.45f) / 0.55f, 0.0f, 1.0f)) *
              f_clamp((a->spd - 1.0f) / 2.5f, 0.0f, 1.0f);
    want_yaw = yaw + f_clamp(dm, -ANIM_TWIST_MAX, ANIM_TWIST_MAX) * w;
  }
  float dyaw = wrap_pi(want_yaw - a->leg_yaw);
  if (!slide && a->spd > 0.6f)
    a->leg_yaw += dyaw * ease_k(9.0f);
  else if (fabsf(dyaw) > 0.45f)
    a->leg_yaw += (dyaw - (dyaw > 0 ? 0.28f : -0.28f)) * ease_k(10.0f);

  // Gait phase. Cadence follows the sports-science curve, not a line: cadence
  // grows ~speed^0.33 through walking/jogging (3->4 m/s is +33% speed but only
  // ~+4% frequency — STRIDE LENGTH absorbs speed, paid by flight time) and
  // steepens toward a sprint (elite: 4.56 steps/s at 10.6 m/s). The old linear
  // 0.90+0.16v over-cadenced every mid speed ~20%, which is exactly the
  // mincing mid-speed look. Anchors: 1.9 steps/s at 1.4 m/s, 2.9 at 5, 4.5
  // at 12 (hz here is CYCLES/s = steps/2).
  int moving = grounded && !slide && a->spd > 0.35f;
  // One cadence number, shared with the plant-ahead budget and the duty's reach
  // bound (anim_hz). A second copy here is how the backpedal multiplier ended up
  // tuned in one place and stale in the other.
  float hz = anim_hz(a);
  if (moving) {
    a->phase += hz * TICK_DT;
    if (a->phase >= 1.0f) a->phase -= 1.0f;
    a->bob_lag *= expf(-13.0f * TICK_DT);   // ~0.2 s to pay the jump back
  }

  if (!grounded) {
    // Airborne: legs tuck while rising, reach down as the fall builds.
    // Tuck peaks where the body is SLOWEST — at the apex — and unwinds at both
    // ends: legs extended at push-off (they just straightened to launch) and
    // extended again on the way down (reaching for the floor). Driving it
    // linearly from vel.y instead tucks hardest at takeoff, which is backwards
    // and made rise/apex/fall read as one dangling pose.
    // (`atuck` = the stylistic AIR tuck envelope, 0..1. Named apart from the
    // `tuck` PARAMETER, which is metres of real crouch tuck baked into base.y.)
    // ANTICIPATION, and the only place a procedural rig is allowed any. The
    // take-off is a velocity the sim delivers in one tick and the animation may
    // never see coming — buying counter-motion there would cost input latency,
    // which this game does not trade. The GROUND's arrival, however, is knowable
    // from state the sim already has, and preparing for it gates nothing: the
    // collider, the hitbox and the camera are all untouched by where the boots
    // are. It was also the biggest hole in the whole descent. Measured on a 3 m
    // fall, the air pose saturates ~100 ms after apex (`fallK` tops out at
    // 1.6 m/s of descent) and is then CONSTANT: over the last 30 airborne ticks
    // the feet moved 0.3 mm/tick relative to the root, worst 0.8 — a frozen
    // statue for a quarter of a second and then boots arriving with no
    // preparation at all. `brace` ramps over the last 0.24 s to contact and
    // splits the legs into a landing stance: measured foot separation at contact
    // 0.444 -> 0.536 m, reached over 24 ticks instead of not at all.
    float tti = 9.9f;
    if (vel.y < -0.5f) tti = ground_drop(base, 4.0f) / -vel.y;
    a->brace += (f_clamp(1.0f - tti * (1.0f / 0.24f), 0.0f, 1.0f) - a->brace) *
                ease_k(22.0f);
    float atuck = f_clamp(1.0f - fabsf(vel.y) / 5.0f, 0.05f, 1.00f);
    // Rise / apex / fall are DIFFERENT poses, staged on vertical velocity:
    // legs trail behind after the push-off, tuck peaks at the apex (the
    // existing envelope), and on the way down they re-extend with the lead
    // foot reaching FORWARD for the contact. One pose on a parabola was the
    // "hanging from a winch" read.
    float riseK = f_clamp(vel.y / 4.0f, 0.0f, 1.0f);
    float fallK = f_clamp(-vel.y / 1.6f, 0.0f, 1.0f);
    atuck *= 1.0f - 0.90f * fallK;  // falling UNTUCKS: feet lead the body down
    float kf = ease_k(17.0f);       // the tuck SNAPS up — an energetic jump
                                    // pulls the knees, it does not float them
    for (int i = 0; i < 2; i++) {
      float s = i ? 1.0f : -1.0f;
      float lead = i ? 0.10f : -0.06f;  // slight scissor reads as a real jump
      float along = lead - 0.10f * atuck - 0.15f * riseK +
                    (i ? 0.30f : 0.10f) * fallK +
                    (i ? 0.10f : -0.09f) * a->brace;   // scissor into the plant
      // Height above the collider's FLOOR. The tuck the pose wants is the same
      // however it was produced — but an air crouch has already paid part of it
      // in base.y (root_off: the sim raised the collider's floor by the whole
      // stand/crouch difference and the body stayed on the untucked plane), so
      // subtract what is already in there. Stacking the two put the ankles level
      // with the hips. root_off is 0 for a bot, for a plain jump AND for a jump
      // with crouch held from the ground — none of those move an origin — so all
      // three keep the exact expression they always had, bit for bit.
      // Clamped at the floor rather than allowed to go negative: that floor IS
      // the crouch-jump clearance, and boots hanging under the box you just
      // cleared a ledge with is the thing this whole change exists to avoid.
      float lift = atuck * (i ? 0.46f : 0.38f) + 0.06f * riseK +
                   0.22f * a->crouch_s - 0.055f * a->brace - a->root_off;
      if (lift < 0.0f) lift = 0.0f;
      v3 tgt = {{base.x + rx * s * 0.13f + fx * along,
                 base.y + ANIM_ANKLE_Y + lift,
                 base.z + rz * s * 0.13f + fz * along}};
      // Y matters as much as XZ here, which is why the full `vel` goes in:
      // leaving vel.y out lagged the foot ~v/k = 0.45 m below a rising body,
      // which cancelled the whole tuck, pushed hip-to-ankle past the bone budget
      // and left the reach clamp holding the legs dead straight through the
      // entire jump.
      foot_ride(&a->foot[i], base, tgt, vel, a->leg_yaw, kf);
    }
  } else if (slide) {
    // Slide: heel-first lead leg extends along the pose direction (further
    // the faster we go, pulling in as the slide bleeds off), trail leg folds
    // tight under the hips; both skid with the body — that IS the contact.
    // Backward (butt-slide, slide_bk -> 1): the trail leg joins the lead out
    // front with the heel down instead of folding under.
    v3 pd = a->slide_pd;
    float kf = ease_k(22.0f);
    float syaw = atan2f(pd.x, -pd.z);
    for (int i = 0; i < 2; i++) {
      float s = i ? 1.0f : -1.0f;
      float ld = i ? a->slide_ld : 1.0f - a->slide_ld;  // how much THIS leg leads
      float along = ld * (0.44f + 0.22f * a->slide_v) +
                    (1.0f - ld) * (0.01f + 0.42f * a->slide_bk);
      float footy = ld * 0.075f +
                    (1.0f - ld) * (0.135f - 0.060f * a->slide_bk);
      v3 tgt = {{base.x + pd.x * along + rx * s * 0.17f,
                 base.y + footy,
                 base.z + pd.z * along + rz * s * 0.17f}};
      // Skid HORIZONTALLY only. This branch is `else if (slide)` under
      // `if (!grounded)`, so the body is on the floor and move_axis has already
      // zeroed vel.y — but zeroing it here explicitly is what makes that a
      // statement rather than a bet on another function's post-condition.
      foot_ride(&a->foot[i], base, tgt, (v3){{vel.x, 0.0f, vel.z}}, syaw, kf);
    }
  } else {
    // Grounded stepping. Swing targets land ahead along the velocity so the
    // body passes over the planted foot; backpedaling shortens the reach.
    float runT = anim_runT(a);
    // A planted foot is frozen in the world, so the leg has to span the whole
    // distance the body travels while it is down: stance_time * speed. Walking
    // duty (0.44) leaves one foot down 56% of every half cycle, which past
    // ~4.5 m/s is further than a leg can reach — the ankle clamp then drags the
    // foot off its plant and the run reads as a stiff, skimming split. Running
    // duty is therefore >0.5: both feet leave the ground, the flight phase eats
    // the extra ground, and the knees stay soft.
    float duty = anim_duty(a);
    // Land the foot EXACTLY half a stance ahead. One foot's stance is (1-duty)
    // of the CYCLE and the cycle is 1/hz, so a stance lasts (1-duty)/hz and half
    // of it is 0.5*(1-duty)/hz. The 0.25 that used to be here is a QUARTER of a
    // stance: the plant straddled the body asymmetrically, the near end carried
    // slack the far end needed, and the far end is exactly where the reach clamp
    // starts dragging the planted foot.
    float ahead_t = 0.5f * (1.0f - duty) / hz *
                    (1.0f - 0.18f * a->bwd) * (1.0f - 0.18f * a->crouch_s);
    // Horizontal reach budget for a plant: how far from the hip a foot may be
    // placed and still be reachable when the hip bobs to its highest. ONE copy
    // (anim_hmax) — `eth`, `amax` and the plant clamp below must read the SAME
    // span budget anim_duty's reach bound sizes the stance against, or the
    // emergency re-step becomes the gait again.
    float hmax = anim_hmax(a);
    for (int i = 0; i < 2; i++) {
      afoot_t *f = &a->foot[i], *o = &a->foot[i ^ 1];
      float s = i ? 1.0f : -1.0f;
      // Stance WIDENS with whatever lateral drift the hip twist could not absorb:
      // a strafing player stands wider, and the extra width is the room the feet
      // need before they would cross. Without it the residual drift eats the
      // 26 cm default stance inside one contact.
      float lath = fabsf(a->vel_s.x * rx + a->vel_s.z * rz);
      // Crouched, the stance narrows: a wide-legged squat waddles.
      float half = (0.14f + 0.11f * f_clamp(lath / 3.0f, 0.0f, 1.0f)) *
                   (1.0f + 0.14f * a->crouch_s);
      // Idle stagger: nobody stands with their boots on a line. The lead
      // (support-side) foot sits ~7 cm forward, the trigger-side foot ~5 cm
      // back — it fades in as the body stops, and the re-step logic walks the
      // feet into it on its own.
      float idle_k = 1.0f - f_clamp(a->spd / 0.7f, 0.0f, 1.0f);
      float stag = (i ? -0.050f : 0.070f) * idle_k * (1.0f - a->crouch_s * 0.4f);
      v3 home = {{base.x + rx * s * (half + 0.012f * idle_k) + fx * stag,
                  base.y + ANIM_ANKLE_Y,
                  base.z + rz * s * (half + 0.012f * idle_k) + fz * stag}};
      float ws = i ? 0.5f : 0.0f;
      float wp = a->phase - ws;
      if (wp < 0) wp += 1.0f;
      int in_win = moving && wp < duty;
      if (f->t >= 1.0f) {
        float ddx = f->pos.x - home.x, ddz = f->pos.z - home.z;
        float d = sqrtf(ddx * ddx + ddz * ddz);
        float ye = wrap_pi(a->leg_yaw - f->yaw);
        int idle_fix = !moving && o->t >= 1.0f && (d > 0.10f || fabsf(ye) > 0.45f);
        // A planted foot also has to re-step once the body has slid far enough
        // sideways that the foot would end up across the midline — the clamp
        // above only constrains where a step LANDS, not where the body goes
        // afterwards.
        float plat = (f->pos.x - base.x) * rx + (f->pos.z - base.z) * rz;
        int crossing = moving && s * plat < 0.02f;
        // An imminent crossing overrides the "don't lift both feet" guard: a
        // shuffle step with both feet briefly light is how a real player deals
        // with sideways drift, and it beats dragging one leg through the other.
        // THE RESCUE MAY NOT FIRE BEFORE THE PLAN IT IS RESCUING IS EXHAUSTED.
        // `eth` is the emergency re-step threshold, and it reads the SAME span
        // budget `anim_duty`'s reach bound sizes the stance against — anything
        // tighter yanks a foot the clock still had room for, which is a step
        // the gait never planned and therefore a phase teleport. The backpedal
        // used to take a 0.10 discount here, pulling the trigger at 0.80*hmax
        // against a plan drawn to 0.90; that one line made the emergency branch
        // the entire backward gait (every step ended in it) and drove `a->phase`
        // to teleport 6-15 times per 200 ticks with `bob_lag` chasing 0.30 of a
        // cycle. `oth`, the partner gate, is the other half: at 0.75 the stance
        // foot was pinned ~4 ticks past its reach every stride waiting for the
        // other foot, `sink` chased 152 mm of over-reach at ~15 mm/tick and then
        // snapped back 12-15 mm in ONE tick — a 2.45 m/s velocity step through
        // the whole body, once per step, and most of the backpedal's agitation.
        // Both are measured, not slack: loosening either degrades the plant again.
        // On the shipped build, worst planted-sole float and the share of ticks
        // with BOTH soles >20 mm clear, before -> after, at 6/9/12 m/s:
        //   backward  6.8/64.2/197.7 -> 6.8/6.8/6.8 mm · 0.0/28.5/52.0 -> 0.0/5.5/11.0%
        //   forward  13.9/37.0/145.9 -> 18.6/18.2/17.8 mm · 28.5/57.0/100.0 -> 42.5/46.0/51.5%
        // The forward row is the headline: a 12 m/s sprint used to report 100%
        // flight, which sounds like the design until you notice the PLANTED sole
        // was 146 mm off the floor — the figure had simply stopped touching the
        // ground. It now has a real flight phase AND a real contact.
        float eth = hmax * ANIM_SPAN_K;
        float oth = 0.25f + 0.25f * a->bwd;
        int emerg = moving && !in_win && (crossing || (d > eth && o->t >= oth));
        if (emerg || (in_win && wp > 0.05f)) {  // adopt the cycle mid-window
          // Carry the discontinuity instead of inflicting it on the body.
          float dph = (ws + 0.002f) - a->phase;
          while (dph > 0.5f) dph -= 1.0f;
          while (dph < -0.5f) dph += 1.0f;
          a->bob_lag += dph;
          a->phase = ws + 0.002f;
          wp = 0.002f;
          in_win = moving;
        }
        if (in_win || idle_fix) {
          f->from = f->pos;
          f->t = 0.0001f;
        }
      }
      if (f->t < 1.0f) {
        float tprev = f->t;
        v3 toprev = f->to;
        // Cap the plant's ahead DISTANCE in metres, not only in time: at 6 m/s
        // the time budget put the boot 0.55 m ahead of the pelvis with a
        // straight knee — a braking lunge every stride, 2x the human ~0.25 m.
        // Cap the ahead DISTANCE at the same fraction of the leg's span the
        // duty bound uses — a fixed 0.30 m was slack at a walk and impossible at
        // a sprint, and the two budgets have to be one number or they fight.
        float amax = hmax * ANIM_SPAN_K, aspd = sqrtf(v3_dot(a->vel_s, a->vel_s));
        float at2 = aspd > 1e-4f ? fminf(ahead_t, amax / aspd) : ahead_t;
        v3 to = {{home.x + a->vel_s.x * at2, home.y, home.z + a->vel_s.z * at2}};
        float ox = to.x - base.x, oz = to.z - base.z;
        float ol = sqrtf(ox * ox + oz * oz);
        // Never aim a plant outside the leg's horizontal reach at the HIGHEST
        // the hip will bob to during that stance — sqrt(reach^2 - drop^2). A
        // fixed limit (this was 0.52 m) is either slack at walking speed or
        // impossible at sprint, and impossible means the reach clamp drags the
        // planted foot.
        if (ol > hmax * 0.97f) {
          to.x = base.x + ox / ol * hmax * 0.97f;
          to.z = base.z + oz / ol * hmax * 0.97f;
        }
        // A foot may never step across the body's midline. Predicting the plant
        // from the velocity alone sends BOTH feet the same way, so the far foot
        // has to travel across its partner — measured, the ankles crossed on 49%
        // of ticks moving diagonally, down to -0.68 m of separation. Clamping the
        // target's lateral component (hip frame) keeps each foot on its own side;
        // since the swing interpolates linearly between two clamped endpoints,
        // the whole path stays there too.
        float lat = (to.x - base.x) * rx + (to.z - base.z) * rz;
        if (s * lat < ANIM_STANCE_MIN) {
          float d2 = s * ANIM_STANCE_MIN - lat;
          to.x += d2 * rx;
          to.z += d2 * rz;
        } else if (s * lat > 0.30f) {
          // Cap the OUTWARD component too: uncapped lateral plants make every
          // fast strafe a ~1 m near-straight split with the pelvis parked
          // midway — weight committed to neither leg. Shorter lateral steps
          // at higher cadence is what a body actually does.
          float d2 = s * 0.30f - lat;
          to.x += d2 * rx;
          to.z += d2 * rz;
        }
        f->to = foot_clamp_world(base, to);
        // A committed swing's target may WANDER but never jump: once the foot
        // is a quarter through its flight, cap target motion per tick. The
        // lerp scales target deltas by e, so an uncapped 1 m re-aim late in
        // the swing IS a teleport of the boot (measured 104.7 m/s for a tick).
        if (tprev > 0.25f) {
          // The cap is on the target's WANDER, not on its travel. `to` is built
          // from `home`, which is body-relative, so a committed swing's target
          // legitimately translates with the body at v*dt every tick — 0.100 m
          // at the default 12 m/s run speed, against a 0.055 m allowance. Above
          // 0.055 m/tick (= 6.6 m/s, exactly where measured contact stops) the
          // limiter therefore held the plant BACK while the body ran on, the
          // reach clamp in skel_solve paid for it by lifting the ankle, and the
          // whole figure hovered: at 12 m/s the lowest point of the boot never
          // came within 25.8 mm of the floor in 240 ticks, median 89.8 mm, with
          // a world-frozen foot rendered 141.7 mm above its own plant and a
          // 7.5 steps/s mince. Let the target ride the body for free and cap
          // only what it does on top of that.
          float allow = 0.055f + sqrtf(a->vel_s.x * a->vel_s.x +
                                       a->vel_s.z * a->vel_s.z) * TICK_DT;
          v3 dto = v3_sub(f->to, toprev);
          float dl2 = v3_len(dto);
          if (dl2 > allow)
            f->to = v3_add(toprev, v3_scale(dto, allow / dl2));
        }
        if (in_win) f->t = fmaxf(f->t, wp / duty);  // phase-locked swing
        else f->t += TICK_DT / 0.18f;                    // idle/finish-up step
        // NOTE: the backpedal used to force `f->t += (TICK_DT/0.118f)*bwd`
        // here so the planted partner could not drift out front. That capped
        // the swing at ~14 ticks against the clock's own ~33, and since the
        // stance is reach-limited to ~12-16 ticks at ANY speed the foot cycle
        // collapsed to ~30: the gait ran at 2.4x its own cadence (8.60 steps/s
        // at 4 m/s against 4.20 walking FORWARD at the same speed) and every
        // single step ended in the `emerg` branch above teleporting `phase`.
        // That is what read as busy. The drift it was aiming at was really the
        // alternation gate (`oth`) holding the stance foot past its reach; with
        // that relaxed the planted sole lifts 6.8 mm instead of 70.5, so
        // nothing has to sprint the swing. The swing is the clock's again.
        // ...and the clock may never SLAM the progress forward: when the
        // partner's emerg adoption teleports the phase, the phase-locked term
        // above would jump this foot most of a swing in ONE tick. Cap the
        // advance at ~2.2x the nominal swing rate — the clock catches the
        // foot up over a few ticks instead of teleporting it.
        {
          float nom = TICK_DT * hz / fmaxf(duty, 0.20f);
          f->t = fminf(f->t, tprev + 2.2f * nom);
        }
        f->yaw += wrap_pi(a->leg_yaw + s * ANIM_TOE_OUT - f->yaw) * ease_k(14.0f);
        if (f->t >= 1.0f) {
          f->t = 1.0f;
          f->pos = f->to;
          f->yaw = a->leg_yaw + s * ANIM_TOE_OUT;
          // A FOOT LANDED. This is the one moment in the whole animation that
          // makes a sound, and it is worth having: after the weapon, footsteps
          // are what a shooter is actually listening to — they are the only
          // continuous information channel about someone you cannot see.
          // Emitted from the solver rather than from a timer so the sound lands
          // on the frame the boot does, and gated on real travel so a stance
          // correction while standing still does not thud. No grounded test:
          // this whole branch is the grounded-stepping arm of the air/slide/
          // ground split, so grounded holds by construction here.
          if (v3_dot(a->vel_s, a->vel_s) > 0.6f) {
            float step_speed = sqrtf(v3_dot(a->vel_s, a->vel_s));
            ev_push((event_t){.type = EV_STEP, .a = f->pos, .src = ent,
                              .f = step_speed, .g = a->crouch_s});
            bot_noise_emit(ent, BOT_NOISE_STEP, f->pos,
                           bot_step_noise_radius(step_speed, a->crouch_s));
          }
        } else {
          float tc = f_clamp(f->t, 0.0f, 1.0f);
          // Forward swings ease OUT (fast off the toe — the pendulum);
          // the backpedal's short quick steps read twitchy with that same
          // fast start, so they blend back toward the symmetric curve.
          float eo = 1.0f - (1.0f - tc) * (1.0f - tc);
          float e = eo + (smooth01(tc) - eo) * 0.7f * a->bwd;
          // Accelerating hard drives the knee higher — the same speed reached by
          // coasting and by sprinting out of a standstill should not look alike.
          // The backpedal's low foot clearance is deliberate — you cannot see
          // behind you, so the boot feels for the ground — but it is an ARC
          // ASPECT, not an absolute, and halving the cadence doubled the step
          // it has to cross. Measured at 4 m/s the step went 0.465 -> 0.833 m
          // while the 0.74 reduction left the clearance at 53.5 mm, i.e. the
          // arc went from 12.3% of the step to 6.4% and the boot skimmed.
          // 0.60 puts it back to 9.0% — still half the forward walk's 18.9%.
          float lift = moving ? (0.035f + 0.205f * runT +
                                 0.24f * f_clamp(a->lean_f, 0.0f, 0.26f)) *
                                (1.0f - 0.60f * a->bwd)
                              : 0.028f;
          // A SPRINT HAS HEEL RECOVERY, and a straight line from plant to plant
          // cannot express it. The swing foot travelled x and z linearly on the
          // same `e` the sine lift uses, so it could never come back UNDER the
          // hip — the leg scissored on horizontal shins at every speed and no
          // value of `lift` bought the tuck, because the tuck is a path
          // property. The retraction is zero at both endpoints, so the plant and
          // the lift-off are byte-identical to before; only the middle of the
          // arc moves, which is the only part of a swing nobody was looking at.
          // Ramped above 4 m/s rather than folded into `runT`: `runT` also
          // carries the hip drop, the stride, the bob amplitude, the spine twist
          // and the weapon wander, and widening it would move all five.
          float sprT = f_clamp((a->spd - 4.0f) * (1.0f / 8.0f), 0.0f, 1.0f);
          lift += 0.11f * sprT * moving;
          float pull = sinf(F_PI * e) * 0.46f * sprT * (1.0f - a->bwd);
          f->pos.x = f->from.x + (f->to.x - f->from.x) * e;
          f->pos.z = f->from.z + (f->to.z - f->from.z) * e;
          f->pos.x += (base.x - f->pos.x) * pull;
          f->pos.z += (base.z - f->pos.z) * pull;
          f->pos.y = base.y + ANIM_ANKLE_Y + sinf(F_PI * e) * lift;
        }
      }
    }
  }

  {
    // Antenna whip: a 3 Hz, zeta 0.45 follower on the chest's acceleration.
    // The one piece of gear that shows motion HISTORY — it keeps moving a
    // beat after the body stops, which is the cheapest "alive" cue the back
    // view can buy. Purely presentational; skel_solve copies it into the pose.
    const float w0 = F_TAU * 3.0f;
    v3 drive = v3_scale(dv, -0.030f / TICK_DT * 0.01f);
    drive.y -= (vel.y - a->vy) * 0.020f;
    drive.x += -sinf(a->leg_yaw) * a->yaw_vel * 0.012f;
    drive.z += cosf(a->leg_yaw) * a->yaw_vel * 0.012f;
    a->ant_v = v3_add(a->ant_v,
        v3_scale(v3_add(v3_add(v3_scale(a->ant_p, -w0 * w0),
                               v3_scale(a->ant_v, -2.0f * 0.45f * w0)),
                        v3_scale(drive, w0 * w0)), TICK_DT));
    a->ant_p = v3_add(a->ant_p, v3_scale(a->ant_v, TICK_DT));
    float m = v3_len(a->ant_p);
    if (m > 0.11f) a->ant_p = v3_scale(a->ant_p, 0.11f / m);
  }
  skel_solve(a, base, yaw, pitch);
}

// Fresh spawn: feet planted at their home spots, pose solved once so the
// very first rendered frame already stands correctly.
static void anim_reset(anim_t *a, v3 pos, float yaw) {
  memset(a, 0, sizeof *a);
  a->leg_yaw = yaw;
  a->was_grounded = 1;
  for (int i = 0; i < 2; i++) {
    float s = i ? 1.0f : -1.0f;
    v3 home = {{pos.x + cosf(yaw) * s * 0.13f, pos.y + ANIM_ANKLE_Y,
                pos.z + sinf(yaw) * s * 0.13f}};
    a->foot[i].pos = a->foot[i].from = a->foot[i].to = home;
    a->foot[i].yaw = yaw;
    a->foot[i].t = 1.0f;
  }
  skel_solve(a, pos, yaw, 0.0f);
  // Prime BOTH histories that get differentiated, or the first tick after a
  // spawn measures a step out of nothing: the pelvis against zero (a 116 m/s
  // trunk step) and the aim yaw against zero — a body spawning at yaw = pi
  // registered ~377 rad/s of turn rate, i.e. the shoulders led a turn nobody
  // made, once per respawn.
  a->spc_py = a->pose.pelvis.y;
  a->yaw_prev = yaw;
  a->prev_pose = a->pose;
}

// The humans' own third-person bodies — same skeleton as the bots, ticked
// from player physics at the end of the world tick, human 0's drawn only when
// the camera is not first-person. PER HUMAN (P1-1): as singletons, eight
// humans shared one skeleton, one ragdoll and one handover flag — the second
// simultaneous death never reached the corpse pool and blinked away, exactly
// the defect the pool was built for.
static anim_t g_player_anim_all[SRV_LOBBIES][MAX_HUMANS];
#define g_player_anim (g_player_anim_all[g_lobby])
// THE PLAYER'S OWN CORPSE. A bot ragdolls as an ENTITY for the second between
// its death and its respawn and only then hands the body to the pool; the
// player cannot, because every camera in the game — first person, third person,
// the audio listener — is derived from `p->pos`, and flinging that 6 m across
// the arena is a death cam nobody asked for. So the player's body enters the
// pool on the killing tick instead, with a full launch on it, while the entity
// stands still and invisible until it respawns. The pool ramps the limb slack
// itself for exactly this caller.
static rag_t g_player_rag_all[SRV_LOBBIES][MAX_HUMANS];
#define g_player_rag (g_player_rag_all[g_lobby])
static int   g_player_corpse_q_all[SRV_LOBBIES][MAX_HUMANS];  // launch awaits handover
#define g_player_corpse_q (g_player_corpse_q_all[g_lobby])

// The humans themselves: a human IS a player, so he gets the
// player's code path — that is the one-copy rule. ~1.5 KB per human with the
// anim/rag arrays above. Slot 0 is what the entry points hand around as `p`.
static player_t g_players_all[SRV_LOBBIES][MAX_HUMANS];
#define g_players (g_players_all[g_lobby])

