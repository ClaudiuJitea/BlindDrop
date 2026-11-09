// code/sim/player.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Player sim — fixed 120 Hz ticks, deterministic. Quake-style ground
// friction + acceleration, limited air control, AABB collision resolved
// per axis against the solids plus the y=0 floor plane. Speeds, accels,
// friction, gravity, jump and slide tuning live in g_cfg.mv (MOVEMENT panel);
// only structural constants remain #defines. There is no sprint: crouching
// while running near full speed enters a slide, launched along the current
// velocity, so it works in every horizontal direction.
// ---------------------------------------------------------------------------

// TICK_HZ / TICK_DT are defined above the audio section — the synth needs the
// tick rate to time the reloads, and one number cannot live in two places.
#define PLAYER_RADIUS  0.35f
// How far out from a solid's face a bot starts steering ALONG it rather than
// into it, and the inner band where the figure is wider than PLAYER_RADIUS and
// an elbow would otherwise be inside the wall. See the projection in bot_tick.
#define BOT_WALL_BAND  0.42f
#define BOT_WALL_ELBOW 0.06f
#define HEIGHT_STAND   1.80f
#define HEIGHT_CROUCH  1.20f
#define EYE_STAND      1.62f
#define EYE_CROUCH     1.00f
#define JUMP_BUFFER_TICKS 8   // ~66 ms: a jump pressed just before landing still fires
#define EYE_LERP_RATE  12.0f
#define SENSE_RAD_PER_COUNT 3.8397e-4f  // 0.022 deg/count * pi/180
#define PITCH_MAX      1.55f  // rad (~88.8 deg): view/aim pitch limit, every clamp in the look/aim pipeline
#define ENT_CHEST_OFF  0.28f  // aim point sits this far below the eye (upper chest)
#define GRAVITY        22.0f  // m/s^2; jump feel is tuned via JUMP HEIGHT instead
#define SLIDE_START_SPEED    0.80f  // x run speed: min speed to enter a slide
#define SLIDE_END_SPEED      1.15f  // x crouch speed: below this a slide settles into a crouch
#define SLIDE_STEER_RATE     2.6f   // /s: slide + air steering redirect toward the wish dir
#define SLIDE_COOLDOWN_TICKS 96    // 0.8 s between slide boosts: no perpetual hop-boost chains
#define FOV_KICK_RATE        9.0f   // /s: smoothing of the slide FOV kick
#define LAND_DIP_SCALE   0.015f // m of eye dip per m/s of landing impact speed
#define LAND_DIP_MAX     0.12f  // hardest landings bottom out here
#define LAND_DIP_RECOVER 15.0f  // /s: spring back, ~95% recovered in 200 ms

// --- Lean ------------------------------------------------------------------
// A lean is ONE geometric fact — the torso bends sideways about the waist by
// LEAN_ANGLE while the hips counter-shift LEAN_HIP the other way — and
// everything else is derived from it: how far the first-person eye travels, how
// far it sinks, how much the view rolls, and how far the third-person head has
// to end up if the two views are to show the same man. Writing the eye offset
// and the body's bend as independent numbers is how a camera ends up peeking
// 40 cm around a corner while the body it belongs to bends 10.
#define LEAN_ANGLE   0.54f    // rad (31 deg) of lateral spine bend at full lean
#define LEAN_ARM     0.64f    // m from the bend (waist, ~L3) up to the eye
#define LEAN_HIP     0.05f    // m the hips push out the OTHER way as a counterweight
// The neck gives back half the torso's roll: a leaning shooter keeps their eyes
// nearer level than their chest, and a full 31 degrees of camera roll is
// nauseating rather than tactical.
#define LEAN_VIEW_K  0.50f
// How much of the head's travel the TORSO hitbox takes with it (the boots take
// none, the shoulders nearly all of it, and one box has to average that).
#define LEAN_BODY_K  0.55f
// Fraction of the bend the PELVIS takes. The hips are not a rigid bar the trunk
// hinges off: leaning right puts the weight on the LEFT leg (the trunk went
// right, so the hips went left to keep the mass over the feet), and a loaded leg
// hikes its own hip. Small — 9 degrees over a 0.1 m half-width is about 1.5 cm —
// but a pelvis that stays perfectly level while the chest bends 31 degrees is
// the tell that the top half is one rigid object bolted to the bottom half.
#define LEAN_PELV_K  0.30f
#define LEAN_IN_RATE  11.0f   // /s toward the held command (~0.27 s to full)
#define LEAN_OUT_RATE 14.0f   // /s back to upright — recovering is faster than committing
#define LEAN_CLR_RATE 16.0f   // /s the wall clearance eases, so a corner cannot pop the head
#define LEAN_MARGIN  0.20f    // m of air the eye keeps between itself and a wall
#define LEAN_LOOK    0.15f    // s of velocity the clearance probe looks ahead
#define LEAN_LOOK_MAX 0.60f   // ...capped, so speed cannot anticipate a wall a room early
// Leaning is a stationary/walking move. Past this fraction of run speed the
// command fades out: at 12 m/s a 31 degree body roll fights the stride, and a
// slide has a roll of its own that this would sit on top of.
#define LEAN_SPD_LO  0.55f
#define LEAN_SPD_HI  0.90f

// Eye travel and drop at lean amount l, both DERIVED from the bend above so the
// camera and the skeleton can never disagree: 0.28 m out and 0.09 m down at full
// lean. They follow the ARC, not a straight line — the head is on the end of a
// 0.64 m lever, so it barely sinks over the first third of the travel and then
// drops away, and interpolating either one linearly puts the first-person eye
// and the third-person head up to 6 mm apart in mid-lean for no reason.
static float lean_eye_off(float l)  { return LEAN_ARM * sinf(l * LEAN_ANGLE) - LEAN_HIP * l; }
static float lean_eye_drop(float l) { return LEAN_ARM * (1.0f - cosf(l * LEAN_ANGLE)); }

// ACCEL/STOP TIME sliders are "ms to cover 95% of the speed gap" — three time
// constants of the exponential approach the ground move performs each tick.
static float mv_alpha(float ms) {
  return 1.0f - expf(-3.0f * TICK_DT * 1000.0f / ms);
}

// The same first-order filter parameterized as a RATE per second instead of a
// settling time — the form 20-odd presentation springs want. It was spelled out
// inline at every one of them, which is how the file ended up with two different
// meanings for "ease at rate k": NOTE that vm_tick's f_clamp(rate * TICK_DT,0,1)
// is the LINEAR first-order form, a genuinely different filter (~6% bigger alpha
// at k = 14). The viewmodel springs are tuned against it and must NOT be
// converted to this one.
static float ease_k(float rate) { return 1.0f - expf(-rate * TICK_DT); }

// Weapon behavior constants shared by every weapon — per-weapon structure is
// in WPN_DEF, per-weapon feel in g_cfg.wp.
#define GUN_SWITCH_TICKS 36      // 0.3 s weapon raise on swap
#define GUN_BLOOM_CAP    0.6f    // deg: full-auto spread growth tops out here
#define GUN_MOVE_SPREAD  0.5f    // deg added at full run speed
#define GUN_AIR_SPREAD   4.0f    // x: total spread multiplier while airborne
#define GUN_REC_CAP      12.0f   // deg: recoil climb hard cap
#define GUN_PUNCH_SHOT   0.4f    // deg of visual punch per shot (x VIEW PUNCH)
// THE PUNCH IS A SPRING AND THE AUTHORED NUMBER IS ITS PEAK.
//
// These three constants were solved on the DISCRETE recurrence, not on the
// continuous one, and the difference is not academic: at TICK_DT = 1/120 a
// continuous zeta of 0.55 needs c = 69, and stepping THAT explicitly multiplies
// the velocity by (1 - 69/120) = 0.43 every tick — the first attempt measured a
// peak of 0.113 deg where 0.400 was asked for. Simulated instead, k = 1200 /
// c = 32 peaks 4 ticks after the shot tick (33 ms — one to two rendered frames at
// any cap this game offers), undershoots 16% on the way back, and is inside 6%
// of the peak by tick 20 (167 ms). The peak of that response is v0 * 0.01266,
// so GUN_PUNCH_V = 1/0.01266 and GUN_PUNCH_SHOT * VIEW PUNCH stays the number
// of DEGREES the view actually reaches. The undershoot is what separates a
// recoil impulse from a lift, and the old one-pole decay had none of it — it
// also ran AFTER gun_shoot in the same tick, so 34% of the angle was gone
// before any frame could show it.
#define GUN_PUNCH_K      1200.0f
#define GUN_PUNCH_C      32.0f
#define GUN_PUNCH_V      79.0f   // deg/s of spring velocity per deg of peak
// A shot ROLLS the view, and roll is the one punch axis that is free: it pivots
// about the view axis, so the crosshair pixel does not move and the
// point-of-aim contract holds BY CONSTRUCTION. Measured before this, the entire
// on-screen event of an AR round was an 8 px vertical translation of which 5 px
// was permanent recoil climb — 3 px of transient for the loudest thing the
// player does. Signed off the spray pattern's own step so the roll reinforces
// the horizontal pattern the player is learning instead of alternating blindly.
#define GUN_ROLL_K       0.85f   // deg of roll per deg of punch
#define DEG2RAD          0.017453293f

// Deterministic horizontal drift per shot index (x KICK SIDE): right pull
// first, a long left sweep, back right — learnable like a CS pattern.
static const float GUN_PATTERN[32] = {
   0.00f,  0.05f, -0.10f,  0.15f,  0.30f,  0.45f,  0.55f,  0.40f,
   0.20f, -0.10f, -0.35f, -0.55f, -0.70f, -0.60f, -0.40f, -0.15f,
   0.10f,  0.35f,  0.55f,  0.70f,  0.60f,  0.35f,  0.05f, -0.25f,
  -0.50f, -0.65f, -0.55f, -0.30f,  0.00f,  0.30f,  0.50f,  0.60f,
};

typedef struct {
  int   cur;             // WPN_AR / WPN_SR — which weapon is in the hands
  int   ammo[WPN_COUNT]; // per-weapon rounds; transient state below is shared
  int   var[WPN_COUNT];  // LOADOUT variant per slot (WPN_VAR); only AR/PT have any
  int   fresh;           // nothing fired or reloaded since the spawn: a loadout
                         // change may still be taken without waiting a life
  // PER WEAPON, like ammo: the cooldown is the weapon's mechanism cycling (the
  // SR's 1.2 s bolt), not the shooter's readiness. Shared, a quick-switch
  // could zero the bolt cycle — measured 1.92x the designed SR fire rate — and
  // NOT zeroing a shared field would gate a freshly raised AR on the SR's
  // leftover 144 ticks. All entries tick down in gun_cool, so the bolt cycles
  // in real time while holstered.
  int   cool[WPN_COUNT]; // ticks until that weapon can fire again
  int   idx;             // spray pattern index, resets once recoil recovered
  int   reload_t;        // ticks of reload remaining, 0 = not reloading
  int   reload_tac;      // ...and whether it started with a round still up
  // The knife. melee_t counts UP through a swing of MELEE[melee_v] (0 = idle);
  // melee_n is the place in the chain and melee_gap the pause that resets it.
  // guard_t counts how long the guard has been up, which is what decides a
  // parry. stagger_t is the lockout after being parried, block_k the jolt a
  // blocked hit puts through the guard, decaying.
  int   melee_t, melee_v, melee_n, melee_gap, melee_done, guard_t, stagger_t;
  // The grenade: gr_cook counts up from the press (0 = pin in), gr_t counts up
  // through the throw, gr_fuse is what the throw leaves on the clock, gr_lob the
  // throw being underhand, and gr_prev the weapon to go back to when none remain.
  int   gr_cook, gr_t, gr_fuse, gr_lob, gr_prev;
  int   gr_regen;        // ticks toward the next grenade (mode_nade_refill)
  float block_k, prev_block_k;
  int   switch_t;        // ticks of weapon raise remaining after a swap
  float prev_switch;     // last tick's raise fraction, for interpolation
  float rec_p, rec_y, prev_rec_p, prev_rec_y;  // deg, applied to view AND aim
  float bloom;           // deg of accumulated spread, decays between bursts
  // THE DRY CLICK IS LATCHED, NOT EDGE-GATED. It used to be pushed only on a
  // press edge, so on a FULL-AUTO weapon — where prev_held is 1 on every tick
  // of the burst — the branch was unreachable and the single commonest way to
  // run out of ammo made no sound at all (measured: `+fire; wait 320` over a
  // whole magazine printed zero dryfire events). One latch, cleared on release,
  // on a reload and on a swap, gives exactly one click when the mechanism runs
  // dry under a held trigger and still exactly one per press for semi/bolt.
  int   dry_done;
  float punch, prev_punch;                     // deg, visual kick only
  // ...and its VELOCITY, because the punch is a spring rather than a decay.
  // One-pole decay ran in gun_recover, i.e. AFTER gun_shoot in the same tick,
  // so 34% of the authored angle was gone before any frame could render it:
  // `wpn` read 0.264 deg on the tick after an AR shot against an authored 0.4,
  // and at the fps_cap of 30 the menu offers, 0.08-0.11 deg = one pixel. The
  // step now runs in gun_cool (the START of the tick, shared by the player and
  // every bot) so this tick's impulse survives to the render intact, and a
  // second-order return gives the snap-and-settle a monotone decay cannot.
  float punch_v;
  // The ROLL half of the same impulse, on its own spring so it can be signed
  // off the spray pattern independently of the pitch. Render-only, like punch.
  float punch_roll, prev_punch_roll, punch_roll_v;
  float ads_lin, ads_s, prev_ads_s;            // raw ramp + smoothstepped
  // Trigger travel, 0 = at rest against the blade, 1 = broken. Eased in
  // gun_recover so the player and every bot get the same finger; the render
  // twin is here rather than in vm_t because a BOT has no viewmodel and its
  // trigger finger still has to move.
  float trig_s, prev_trig_s;
} weapon_t;

// FULL MAGAZINES, EVERY WEAPON. There is no reserve pool, so a spawn (and every
// proof that stages one) fills both mags — seventeen copies of this loop is
// seventeen chances to forget the second weapon when a third arrives.
static int wvar(const weapon_t *w, int id) {
  return (id == WPN_AR || id == WPN_PT) ? armory_idx(w->var[id], WPN_VARS) : 0;
}
// The structural row for slot `id` as THIS shooter carries it. Variant 0 is
// WPN_DEF itself, by pointer, so the shipped guns cannot drift.
static const wpn_def_t *wdef(const weapon_t *w, int id) {
  int v = wvar(w, id);
  if (!v) return &WPN_DEF[id];
  static const char *const *names[2] = {RIFLE_VAR_NAMES, PISTOL_VAR_NAMES};
  static wpn_def_t t[2][WPN_VARS];
  static int built;
  if (!built) {
    for (int k = 0; k < 2; k++)
      for (int i = 1; i < WPN_VARS; i++) {
        const wpn_var_t *a = &WPN_VAR[k][i];
        wpn_def_t d = WPN_DEF[k ? WPN_PT : WPN_AR];
        d.name = names[k][i];
        d.mag = a->mag; d.dmg = a->dmg; d.full_auto = a->full_auto;
        d.hip_spread = a->hip_spread; d.v_round = a->v_round;
        t[k][i] = d;
      }
    built = 1;
  }
  return &t[id == WPN_PT][v];
}
// ...and its FEEL: the slot's live tuning row, scaled by the variant. Variant 0
// hands back the live row itself.
static const float *gun_feel(const weapon_t *w, int id, float buf[WP_COUNT]) {
  int v = wvar(w, id);
  if (!v) return g_sim_wp[id];
  const float *m = WPN_VAR[id == WPN_PT][v].feel;
  for (int i = 0; i < WP_COUNT; i++) buf[i] = g_sim_wp[id][i] * m[i];
  return buf;
}
// The loadout as one nibble: rifle variant in bits 0-1, pistol in bits 2-3.
static int loadout_pack(int rv, int pv) {
  return armory_idx(rv, WPN_VARS) | armory_idx(pv, WPN_VARS) << 2;
}
static void loadout_apply(weapon_t *w, int lo) {
  w->var[WPN_AR] = armory_idx(lo & 3, WPN_VARS);
  w->var[WPN_PT] = armory_idx(lo >> 2 & 3, WPN_VARS);
}
static void wpn_fill_mags(weapon_t *w) {
  for (int i = 0; i < WPN_COUNT; i++) w->ammo[i] = wdef(w, i)->mag;
}
// WHO DECIDES human h's loadout: the server reads what that client's inputs
// carry, an offline game reads the menu, and a client never does — its own
// weapons arrive in the snapshot (-1).
static int human_loadout_src(int h) {
  if (h < 0 || h >= MAX_HUMANS) return -1;
  if (g_server) return g_human_loadout[h];
  if (!g_online) return loadout_pack(g_cfg.rifle_var, g_cfg.pistol_var);
  return -1;
}
// Take the loadout now. Only the two slots that have variants are refilled —
// a grenade already thrown stays thrown.
static void loadout_take(weapon_t *w, int lo) {
  loadout_apply(w, lo);
  w->ammo[WPN_AR] = wdef(w, WPN_AR)->mag;
  w->ammo[WPN_PT] = wdef(w, WPN_PT)->mag;
}

// Viewmodel animation springs — all ticked at 120 Hz with prev_ twins for
// render interpolation, so the gun's motion is deterministic and
// screenshot-stable. Presentation only: nothing here feeds back into aim.
typedef struct {
  float sway_p, sway_y, prev_sway_p, prev_sway_y;  // rad: look-velocity lag
  float lag_x, lag_y, lag_z;                       // m: inertia vs acceleration
  float prev_lag_x, prev_lag_y, prev_lag_z;
  float bob_x, bob_y, prev_bob_x, prev_bob_y;      // m: walk figure-8
  float bob_phase, bob_amp;
  float kick, kick_v, prev_kick;                   // m: fire kick spring
  float roll, prev_roll;                           // rad: slide/strafe lean
  float lean_lag, prev_lean_lag;                   // trails player_t.lean: the
                                                   // weapon is on an arm, not
                                                   // bolted to the helmet
  float last_yaw, last_pitch;
  v3    last_vel;
  // SECOND-ORDER STATE. Every channel above used to be a first-order filter:
  // it can lag, but it cannot overshoot, and a weapon on two arms does — it
  // swings past and settles. Each now carries a velocity and is integrated as
  // x'' = w^2 (target - x) - 2 zeta w x' (see vm_spring).
  float sway_pv, sway_yv, lag_xv, lag_yv, lag_zv, roll_v, lean_v;
  float twist, twist_v, prev_twist;   // rad: per-shot torque about the bore
  float wall, prev_wall, wall_v;      // 0..1: pulled back off a wall in front
  float breath;                       // phase of the idle figure-8
  float cam_roll, cam_roll_v, prev_cam_roll;  // rad: slide bank of the CAMERA
  int   was_grounded;
} vm_t;

typedef struct {
  v3    pos, vel;        // pos = feet center
  v3    prev_pos;
  float yaw, pitch;
  float eye, prev_eye;   // eye height above feet, smoothed
  int   crouched, grounded, prev_grounded;
  int   sliding;
  int   slide_cd;        // ticks until the next slide may boost again
  int   slide_queued;    // crouch was pressed airborne: boost the landing slide
  int   jump_buf;        // ticks a buffered jump press stays valid
  // Metres of LEG TUCK currently folded into pos.y. An airborne crouch raises
  // the collider's FLOOR by the whole stand/crouch difference in one tick (that
  // step IS the crouch-jump clearance) and drops the eye by the same amount, so
  // the SIM is continuous — but pos.y is also what presentation reads as "the
  // ground this figure stands on", and that teleported with it. Published
  // rather than re-derived: `crouched && !grounded` is NOT the same predicate
  // (crouching first and then jumping bakes nothing), and a presentation layer
  // guessing at a physics discontinuity is exactly the bug this replaces.
  float tuck_y;
  float fov_kick, prev_fov_kick;  // 0..1 slide FOV widening, smoothed
  float land_dip, prev_land_dip;  // m the eye is compressed by a landing
  // Lean: the eased COMMAND (-1 left .. +1 right), the eased wall CLEARANCE
  // (0..1, how much of that command the geometry actually allows) and their
  // product, which is the only one presentation and hit detection may read.
  float lean_in, lean_clr;
  float lean, prev_lean;
  int   prev_held[IN_COUNT];
  int   shots;           // total bullets fired
  int   alive, hp;       // deathmatch state
  int   respawn_t;       // dead: ticks until respawn
  // THE PLAYER'S OWN BODY FLASHES TOO. Every bot got a hit flash from the day
  // ent_damage was written and the player's third-person figure was drawn with a
  // hard 0.0 — the identical parity gap the file already closed for anim_hit
  // ("the player does not flinch" was a parity gap, invisible in exactly the
  // view where a player would see it). Same two fields, same helper, same decay.
  int   flash, flash_head;
  weapon_t wp;
  vm_t  vm;
} player_t;

