// code/sim/bots.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Bots + deathmatch — up to MAX_BOTS AI opponents sharing the player's world
// collision and physics helpers. All bot randomness stays on g_rng_bot, so
// player-only interactions reproduce under a fixed seed.
//
// The AI is built to feel human and fair, never twitchy: acquisition needs
// line of sight plus a vision cone (or recent gunfire close by), a reaction
// delay gates the first shot, aim error settles smoothly while tracking (its
// direction drifts a few times a second and is eased toward — no per-tick
// jitter), turning is a rate-capped proportional approach, fire comes in
// bursts with pauses and magazine reloads, and combat footwork is strafing
// with occasional direction changes plus range-keeping.
// ---------------------------------------------------------------------------

#define BOT_PATROL_PACE   0.55f   // x RUN SPEED: a bot that has seen nobody walks
#define BOT_TOPPLE_TICKS  40      // death fall duration
// 12, not 10, AND THE CURVE IS SQUARED. The wash used to be a LINEAR ramp
// capping at 0.16, i.e. a 16% lerp toward white whose mean over its whole life
// was 0.08 — on a target that is 6442 px at 8 m and 112498 px at 1.5 m, that is
// the largest surface in the frame the player is aiming at, drawn with the
// weakest cue in the file. Squared and peaking at 0.38 the flash is 2.0x hotter
// on the first two ticks and stays above the old curve for its whole life: a STRIKE
// rather than a tint, which is what an impact on a body reads as. It stays a
// WORLD effect for every shooter, like the tracer and the body spark — only the
// hitmarker and the damage flash are the player's alone.
#define BOT_FLASH_TICKS   12
#define FLASH_PEAK        0.38f
#define FLASH_HEAD_PEAK   0.52f   // a headshot is hotter as well as redder
static float ent_flash01(int flash, int head) {
  if (flash <= 0) return 0.0f;
  float f = (float)flash / (float)BOT_FLASH_TICKS;
  return f * f * (head ? FLASH_HEAD_PEAK : FLASH_PEAK);
}
#define BOT_FOV_COS       0.50f   // ~120 deg vision cone (ACQUIRING a new target)
// ...and ~210 deg for KEEPING one. The cone used to gate acquisition only, so
// `see` was pure line of sight for the incumbent: a bot that had once looked at
// you tracked you through the back of its own head, turned and shot. Standing
// silent 5 m directly BEHIND a bot that had seen you measured 11 hits and 2
// deaths over five seeds; with the arc, 7 and 1. The arc is deliberately WIDER
// than acquisition (peripheral vision keeps a moving contact it could not pick
// up cold) and wide enough that a bot never loses the target it is already
// facing — want_yaw points at the target, so only a bot that turned AWAY (to a
// noise, to another target) can drop it, which is exactly when it should.
#define BOT_KEEP_COS      (-0.26f)
#define BOT_HEAR_WALL     0.55f   // blocked sound remains useful, but softened
#define BOT_HEAR_STEP_TICKS 72
#define BOT_HEAR_SHOT_TICKS 180
// The startle turn: an ABSOLUTE rate in rad/s, deliberately NOT a multiple of
// the skill's tracking rate. A person who hears a boot behind them snaps
// around, and the tracking rate is a gun-handling handicap — borrowing it for
// the head made the slowest tier look deaf (1.03 s to come round) while a
// multiplier made the fastest one swivel like a turret. Difficulty must not
// scale a purely perceptual response either way: the player cannot see the
// difficulty setting, only the swivel, so all three tiers turn onto a sound at
// the same human speed and the tiers stay expressed where they can be read —
// reaction, aim, and how often a tactic fires.
//
// It is safe to be fast here precisely because hearing is not a targeting
// channel: no aimpoint, no target, no trigger, so the bot can only end up
// LOOKING. The proportional term (diff * 8 * dt) still owns the last few
// degrees, so the motion is snap-then-settle rather than a constant-rate spin:
// measured 180 deg onto a step directly behind, easy/normal/hard, 124/74/56
// ticks before and 55/55/55 after — 0.46 s, which is a person turning round.
#define BOT_ALERT_TURN    9.0f
// Sound localisation is a bearing, not a coordinate. The stored point is
// perturbed by this fraction of the distance (doubled through a wall, where a
// person only knows "somewhere that way"), which is a roughly constant ~9 deg
// of angular error — the same order as human localisation behind the head.
// The SCORE is left alone: it decides which source is believed, and perturbing
// it would flip the priority ordering instead of blurring the position.
#define BOT_HEAR_ERR      0.16f
#define BOT_HEAR_ERR_MAX  2.6f
// THE DEADLOCK COMMITMENT. `pin` counts ticks of wanting to move and not
// moving; past this many the bot is not manoeuvring, it is leaning on a corner.
// The answer is a DECISION, taken once from the situation and then not revisited
// — a healthy loaded bot at conversational range goes IN, anything else breaks
// the angle and rebuilds it. Both read as intent; dithering reads as a bug.
// 20, not 45: the wall-follow further down starts nudging at 30, so a higher
// threshold meant the DECISION almost never got to happen (measured 2-3 per
// 12000 ticks across 8 bots). Deciding is the response the player asked for and
// the nudge is only the low-level assist that carries it out, so the decision
// goes first — a sixth of a second of not getting anywhere is enough to know.
#define BOT_DEADLOCK_PIN  20
#define BOT_PUSH_HP       60
#define BOT_PUSH_DIST     18.0f
// AFTER A FIGHT a person does not go straight back to strolling. Losing (or
// killing) a target starts an alert window: the bot walks over and CLEARS the
// last place it knew about, rifle up, at a pace between patrol and a run, and
// only relaxes once it has been there — or once this many ticks have passed
// with nothing found. 25 s is long enough that "five seconds after a firefight"
// is nowhere near it, and the arrival test keeps it self-limiting.
#define BOT_ALERT_TICKS   3000
#define BOT_ALERT_PACE    0.78f
#define BOT_ALERT_ARRIVE  2.0f
#define BOT_THREAT_TICKS 150
#define BOT_SWAP_COOLDOWN_TICKS 240
#define BOT_TARGET_EPS 0.001f
#define BOT_SEARCH_EPS 0.001f
#define BOT_TARGET_HOLD_BONUS 450.0f
#define BOT_TARGET_HIDDEN_BREAK_MARGIN 250.0f
#define BOT_SNIPER_ENTER_DIST   24.0f
#define BOT_AR_RETURN_DIST      16.0f
#define BOT_SNIPER_ENTER_SPEED   2.5f
#define BOT_AR_RETURN_SPEED      5.0f

// One side-effect-free source for physical step reach, shared by the real
// animation contact and the hearing proof. At configured run speed an upright
// boot reaches 11 m; crouching removes 65 percent of that reach.
static float bot_step_noise_radius(float speed, float crouch) {
  return 11.0f * f_clamp(speed / g_cfg.mv[MV_RUN], 0.0f, 1.0f) *
         (1.0f - 0.65f * crouch);
}

// --- death kinematics ------------------------------------------------------
// A corpse used to be thrown along the shooter->victim horizontal vector and
// spun about the axis perpendicular to it, scaled by damage alone: WHERE the
// round landed never entered, and neither did the shot's pitch. Measured at
// seed 1337 with a sniper at 7 m, a right-shoulder, a left-shoulder, a gut and
// a head kill produced the same fall to three decimals and a pixel-identical
// film at every station. What decides how a body goes down is torque, so that
// is what this computes: r x F about the victim's own centre of mass, with the
// HORIZONTAL part of the torque driving the existing topple (sign in the axis,
// magnitude in dspin) and the VERTICAL part driving a new spin about the body's
// own long axis — which is the only thing that can tell a right shoulder from a
// left one, because the two differ in nothing else.
// THE PIVOT IS THE HIP, NOT THE CENTRE OF MASS, and that is a gameplay choice
// with a physical reason behind it. A standing man's feet are planted, so a
// horizontal force anywhere above the ankle tips him AWAY: on a pure free-body
// model (pivot at the 55%-of-stature CoM) two thirds of the torso box sits below
// the pivot and a plain chest shot would fold the corpse toward the shooter,
// which is both wrong and a hard discontinuity two rounds of one burst can
// straddle. At the hip only a genuinely LOW hit flips it — and that one is
// right: a round through the legs buckles a man forward over them. The
// transition is also gentle by construction, because |tau| goes to zero AT the
// pivot, so both sides of it are a slow gravity fall rather than a snap.
#define DEATH_PIV_Y     (HEIGHT_STAND * 0.42f)  // 0.756 m — the hip
#define DEATH_SPIN_BASE  1.55f    // rad/s a dead-centre hit gets: "he just drops"
// THE DIRECTIONAL TERMS ARE PER UNIT FORCE — the lever alone, in metres — and
// only the THROW is scaled by damage. Folding `imp` into the rotation made the
// whole feature sniper-only: `imp` runs 1.30 on an AR body shot against 4.2 on
// a sniper's, and every bot in the game carries an AR (`bot_spawn` hard-wires
// it), so bot-vs-bot kills — most of the corpses in a match — would have got a
// 9-degree spin nobody can see. A right shoulder and a left shoulder now differ
// by the same angle whatever shot it was; DEATH_ROT_W keeps the sniper's fall
// the more violent one without owning the read.
#define DEATH_SPIN_K     2.45f    // rad/s per metre of vertical lever
#define DEATH_YAW_K     10.00f    // ...and per metre of LATERAL lever. Bigger,
                                  // because a body's longitudinal radius of
                                  // gyration (~0.21 m) is a third of its
                                  // transverse one (~0.54 m): the same torque
                                  // spins it about its own axis far more easily
                                  // than it tips it over.
#define DEATH_ROT_W(IMP) (0.78f + 0.22f * (IMP) / DEATH_IMP_MAX)
#define DEATH_YAW_MAX    3.40f    // rad/s guard, not a knob — the widest lever
                                  // the hitboxes admit is 0.30 m
#define DEATH_YAW_FRIC   4.5f     // 1/s, scaled by how FLAT the body is: 640 ms
                                  // on its boots (it pivots freely), 164 ms once
                                  // down (its whole length is on the floor).
                                  // Spent by the time the topple clamps, or the
                                  // leftover renders as a log-roll about the
                                  // spine instead of a pivot on the ground.
#define DEATH_PITCH_K    0.45f    // how much of the shot's own pitch goes into
                                  // the throw: a round fired down from a crate
                                  // drives the body into the floor
// THE THROW IS NOT PROPORTIONAL TO DAMAGE, and that was the single number that
// kept the feature sniper-only. `dmg * 0.050` gave an AR body shot 1.30 m/s —
// a shove — and every bot in the game carries an AR (`bot_spawn` hard-wires
// it), so the overwhelming majority of kills in a match got no launch at all
// while the one weapon the player rarely fires got all of it. A killing round
// is a killing round: BASE is what any kill is worth and the per-damage term is
// what still separates a 150-damage rifle from a 26-damage one.
#define DEATH_IMP_BASE   6.20f    // m/s every kill is worth. MEASURED AND LEFT
                                  // ALONE: raising it to 6.6/7.1 with the skid
                                  // sparks looked better in flight and took
                                  // ragsoak's rest_pen from 0.093 to 0.219/0.264
                                  // — bodies thrown harder come to rest INSIDE
                                  // geometry. The tumble and the skip below buy
                                  // the same read without moving where a body
                                  // ends up.
#define DEATH_IMP_K      0.029f   // ...plus this per point of damage
#define DEATH_IMP_MAX   12.0f     // m/s ceiling, and it BINDS: a headshot arrives
                                  // at ent_damage already multiplied (an SR head
                                  // hit is dmg=300), so the range is AR body
                                  // 6.95 / AR head 9.25 / SR body 10.55 / SR head
                                  // 17.88 clamped to 12.0. It is also the
                                  // normaliser for `impk` and DEATH_ROT_W below,
                                  // so it is not a free safety knob — moving it
                                  // moves the lift and the launch spin on every
                                  // kill. 7.4 put a sniper's corpse across 2.6 m
                                  // of floor; this puts an ORDINARY one there,
                                  // which is what makes the wall slam something
                                  // the player sees rather than something the
                                  // code can do.
#define DEATH_LIFT       2.65f    // m/s of launch straight up...
#define DEATH_LIFT_K     3.40f    // ...plus this at full impulse. GRAVITY is
                                  // 22 here, so an apex is v^2/44 and the old
                                  // 1.05-1.32 m/s peaked at 3-4 cm: the body
                                  // never left the floor and the tumble had
                                  // nowhere to happen. 3.8-4.8 m/s buys
                                  // 0.33-0.52 m and 0.35-0.44 s of air, which
                                  // is the window an airborne tumble needs to
                                  // be legible at all. Measured, not derived —
                                  // the shot's own pitch takes up to 0.5 m/s
                                  // back out of it.
#define DEATH_AIR_SPIN   5.20f    // rad/s of extra tumble at full impulse (was
                                  // 4.20). DEATH_SPIN_MAX 8.0 is the guard and
                                  // is untouched, so a body still cannot pass
                                  // ~1 rev/s of readable tumble.
#define DEATH_SPIN_MAX   8.00f    // rad/s guard: past ~1 rev/s a tumbling body
                                  // stops reading as a body
#define DEATH_AIR_FRIC   1.20f    // 1/s the tumble bleeds in FLIGHT, and the
                                  // reason is presentation, not aerodynamics:
                                  // this pose is rigid — the limbs are a slack
                                  // sprawl solved once and then rotated as a
                                  // unit — so the body passing THROUGH vertical
                                  // is a tumble and the body sitting at
                                  // vertical for six frames is a pole standing
                                  // on its head. Filmed at 3.2 rad/s of launch
                                  // spin, a headshot did 447 degrees and held
                                  // the handstand for a quarter of a second.
                                  // Damping bounds the integral (spin0/k) and
                                  // the rotation ends where a thrown body's
                                  // does: on its back, past flat, rocking.
#define DEATH_KEEP       0.55f    // share of the victim's OWN velocity the
                                  // corpse keeps. Only the UPWARD part of the
                                  // vertical: a body shot while falling should
                                  // not be driven into the floor twice.
#define DEATH_DRAG       0.35f    // 1/s of air drag, so the flight is bounded
#define DEATH_TILT_MAX   1.52f    // rad: flat on the ground. Derived below as
#define DEATH_FLAT_SHORT (F_PI_2 - DEATH_TILT_MAX)  // ...short of dead
                                  // flat, because a body rests on its shoulder
                                  // blades and its heels, not on a plane.
#define DEATH_TOPPLE_A   5.20f    // rad/s^2 gravity topple — GROUNDED ONLY now
#define DEATH_TILT_FRIC  3.40f    // 1/s the FLOOR takes the tumble out, scaled
                                  // by flatness exactly like the yaw friction.
                                  // Without it a body that lands mid-cartwheel
                                  // keeps rolling at a revolution a second for
                                  // 1.1 s (measured) — a log-roll, which is the
                                  // read this file already rejects for the yaw.
#define DEATH_LAG_RATE   0.11f    // per tick: how fast the limbs catch the trunk
#define DEATH_LAG_K      0.85f    // ...and how much of the lag arc they travel
#define DEATH_SKID_MU   11.00f    // m/s^2 of COULOMB ground friction (was
                                  // 12.00). A body now grinds noticeably
                                  // further before it stops, which is what the
                                  // spark emitter is measured against — and it
                                  // still stops inside a body length or two,
                                  // which was the point of the term. rest_pen is
                                  // unmoved by this one (12.0/11.0 both read
                                  // 0.275 at the rejected impulse). The old
                                  // purely viscous decay is an asymptote: a body
                                  // arriving at 6.8 m/s took 1.26 s and 2.2 m to
                                  // creep down to the cut-off and then snapped
                                  // to a stop, which is 1.3 s of a 5.9 s corpse
                                  // spent ice-skating with a frozen silhouette.
                                  // Real friction is a constant deceleration.
#define DEATH_LAND_SPIN  0.20f    // ...and the share of the TUMBLE that survives
                                  // first ground contact. The floor takes the
                                  // angular energy the same way it takes the
                                  // vertical: without this the leftover spin
                                  // and the restoring torque make a damped
                                  // pendulum, and a body that landed
                                  // mid-cartwheel over-rotated 52 degrees past
                                  // flat and spent 1.3 s rocking back — rubber,
                                  // not mass.
#define DEATH_BOUNCE_MIN 2.40f    // m/s of descent below which a body just lands
#define DEATH_BOUNCE_E   0.42f    // vertical restitution of the skip... (was
                                  // 0.34: the second contact is where the skid
                                  // sparks read best, so the skip earns its
                                  // keep twice now.)
#define DEATH_BOUNCE_H   0.74f    // ...and the horizontal it keeps through it
#define DEATH_BOUNCE_SPIN 0.42f   // rad/s of tumble per m/s of arrival
#define DEATH_WALL_MIN   2.00f    // m/s into a wall before it counts as a slam
#define DEATH_WALL_E     0.42f    // horizontal restitution off it: a body that
                                  // STOPS at cover is a body that fell over
                                  // near it. Being thrown back off it is the
                                  // whole "slammed into the wall" read.
#define DEATH_WALL_SPIN  0.55f    // rad/s of tumble per m/s the wall took
#define DEATH_SLAM_SPIN  0.30f    // ...of the old rotation that survives it
#define DEATH_REST_VAR   0.30f    // rad of per-kill variation in HOW FAR SHORT
                                  // of dead flat a body comes to rest — 0 to 17
                                  // degrees on top of DEATH_FLAT_SHORT's own 3.
                                  // One-sided by construction (see rag_rest):
                                  // the side is the direction it fell, and only
                                  // the amount varies. Without it `rag_rest`
                                  // returned the same 1.622 rad for every kill
                                  // in the game and six corpses on one floor
                                  // were one silhouette repeated six times.
#define DEATH_PUSH_T     6        // ticks the horizontal launch is spread over
                                  // — 50 ms. Long enough that the topple leads
                                  // the travel, short enough that it still
                                  // reads as one impulse rather than a shove.
#define DEATH_COM_Y      0.85f    // m up the standing skeleton the trunk's mass
                                  // sits — pelvis to chest. Only used to ask
                                  // WHERE the body is once it has gone over, so
                                  // it wants the trunk, not a true centroid.
#define DEATH_LEDGE_A    7.0f     // m/s^2 a body with nothing under its origin
                                  // slides toward the nearest edge of whatever
                                  // its collider box is still catching on. Sized
                                  // to clear a PLAYER_RADIUS overhang in about a
                                  // third of a second — long enough to read as
                                  // the body tipping over the lip, short enough
                                  // that it is never a hover.
#define DEATH_SKID_FRIC  2.7f     // 1/s of ground friction on a sliding corpse,
#define DEATH_SKID_STOP  0.32f    // and the speed below which it is at rest.
                                  // NAMED because the corpse pool finishes the
                                  // skid the entity started: PLAYER_RESPAWN_TICKS
                                  // is 120 (1 s), not the 3 s the handover used
                                  // to claim, and at up to 10.4 m/s of throw a
                                  // body needs 160 ticks to reach the cut-off —
                                  // so it is essentially ALWAYS still moving when
                                  // it is handed over, and freezing it there is a
                                  // visible snap to a stop.

// ONE RAGDOLL, INTEGRATED IN ONE PLACE. The state used to live twice: the
// entity carried ten `d*` fields and integrated them in `bot_tick`'s dead
// branch, while the corpse pool carried a velocity and a grounded flag and
// finished only the horizontal SKID. A body handed over mid-flight therefore
// stopped tumbling, stopped falling and stopped bouncing at the respawn tick —
// and with a real launch behind it, mid-flight is exactly when the handover
// happens. Same class as `mv_walk` and `gun_shoot`: one integrator, two owners.

// How FLAT the body is, 0 upright to 1 lying. `tilt` is unbounded now, so the
// old `tilt / 1.45` ramp would read a body that had cartwheeled onto its head
// (tilt = pi, upright again) as maximally flat and shrink its collider to a
// crate. |sin| is the actual projection and is right at every wrap.
static float rag_flat(float tilt) { return fabsf(sinf(tilt)); }

// The attitude a body at this tilt comes to rest in: the nearest LYING one.
// Written as nearest-multiple arithmetic rather than the old constant clamp
// because an airborne tumble is unbounded — a body that has gone past vertical
// has to keep falling the way it is already going, not unwind through 200
// degrees to reach a fixed 1.52 rad.
// NOTE the side is chosen from the POSITION, not from the direction of travel,
// so it is discontinuous at 0: rag_rest(0) = +1.52 and rag_rest(-1e-7) = -1.52.
// `spin` DOES go negative — a body that over-rotates past a lying attitude is
// pulled back by this integrator — but `tilt` cannot follow it below the first
// one, because the caller snaps to `rest` on the crossing and the accel below
// `rest` points the other way. The 5.8-degree difference between arriving from
// below (1.520) and from above (1.622) is the design, not the seam: a body
// stops short of dead flat on whichever side it came from.
// The surface directly under a point — the floor plane, or the top of whatever
// solid's footprint contains it. This is what "is there anything under the body"
// means, as opposed to what `ent_move` answers, which is "does the collider's
// BOX overlap anything".
static float rag_support_eps(v3 p, float eps) {
  float y = 0.0f;
  BP_BOX(i, p.x, p.z, p.x, p.z) {
    const solid_t *s = &g_solids[i];
    if (p.x < s->min.x || p.x > s->max.x || p.z < s->min.z || p.z > s->max.z)
      continue;
    if (s->max.y <= p.y + eps && s->max.y > y) y = s->max.y;
  }
  return y;
}
// The pose solve's tolerance. `ragx` asks the same question with a tighter one —
// it is MEASURING the answer, so it may not round a surface into existence the
// way the solve is allowed to — but through the same function, because a proof
// and the thing it proves disagreeing about which surfaces exist is how a proof
// passes a pose it should have failed.
static float rag_support(v3 p) { return rag_support_eps(p, 0.02f); }

static float rag_rest(float tilt, float off) {
  float k = floorf((tilt - F_PI_2) / F_PI + 0.5f);
  float t = (2.0f * k + 1.0f) * F_PI_2;
  // The offset is NON-NEGATIVE and takes the side's own sign. Allowed to go
  // negative it can push `rest` past `t` to the far side of the body's current
  // attitude, and then the integrator chases it across `t`, `rag_rest` answers
  // with the other side, and the body rocks between the two forever — measured,
  // `rag`'s `settle` went to -1 on every AR kill, i.e. no corpse in the game
  // ever came to rest again.
  float d = DEATH_FLAT_SHORT + off;
  return t + (tilt > t ? d : -d);
}

// One tick of the ragdoll, for whoever owns it. Order matters: drag, gravity,
// sweep, then the three impact responses (wall, arena edge, ground) read back
// off what the sweep did to the velocity, because `move_axis` ZEROES the
// component that hit something — so the axis that stopped IS the contact
// normal, and no second collision query is needed to find it.
static void rag_step(rag_t *r, v3 *pos) {
  r->prev_tilt = r->tilt;
  r->prev_slam = r->slam;
  r->prev_yaw  = r->yaw;
  r->prev_lag  = r->lag;
  // The collider shrinks as the body goes down: one lying flat must not be held
  // upright by knee-high cover it is draped over.
  float h = HEIGHT_STAND - 1.30f * rag_flat(r->tilt);
  int wasg = r->grounded;
  if (!r->grounded) {
    float k = expf(-DEATH_DRAG * TICK_DT);
    r->vel.x *= k;
    r->vel.z *= k;
    r->spin *= expf(-DEATH_AIR_FRIC * TICK_DT);
  }
  if (r->push_t > 0) {
    r->vel.x += r->push.x / (float)DEATH_PUSH_T;
    r->vel.z += r->push.z / (float)DEATH_PUSH_T;
    r->push_t--;
  }
  r->vel.y -= GRAVITY * TICK_DT;
  v3 v0 = r->vel;
  r->grounded = 0;
  ent_move(pos, &r->vel, &r->grounded, h);
  float wx = (v0.x != 0.0f && r->vel.x == 0.0f) ? v0.x : 0.0f;
  float wz = (v0.z != 0.0f && r->vel.z == 0.0f) ? v0.z : 0.0f;
  // The arena shell stops a body by MOVING it, not by zeroing its velocity, so
  // it needs its own read — without it a corpse thrown at the perimeter presses
  // into the wall at full speed forever and never slams.
  // arena_clamp stays as the positional backstop it has always been, but needs
  // NO rebound read of its own: the perimeter is eight real 3.2 m solids, so
  // move_axis has already stopped the body and zeroed the component at the
  // identical coordinate (s->min.x - PLAYER_RADIUS IS the clamp's own bound).
  // Instrumented over 2 seeds x 2500 ticks x 20 bots hard the clamp displaced a
  // ragdoll zero times, and a body would have to be above 3.2 m at the wall to
  // reach it against apexes of half a metre.
  arena_clamp(pos);
  float ws = sqrtf(wx * wx + wz * wz);
  if (ws > DEATH_WALL_MIN) {
    r->vel.x -= wx * DEATH_WALL_E;      // the stopped component is 0, so this
    r->vel.z -= wz * DEATH_WALL_E;      // is exactly -e times the arrival
    r->slam = fmaxf(r->slam, fminf(1.0f, ws * 0.22f));
    // The wall does not merely eat the rotation, it CONVERTS the travel into
    // it: a body that hits cover shoulder-first whips over it. Same sign as
    // whatever it was already doing, because the direction of the fall lives in
    // `axis` and a negative rate would fight the topple integrator.
    r->spin = fminf(DEATH_SPIN_MAX,
                    r->spin * DEATH_SLAM_SPIN + ws * DEATH_WALL_SPIN);
    r->yawv *= DEATH_SLAM_SPIN;
  }
  // A BODY DOES NOT BALANCE ON A CORNER. `ent_move` stands the collider on any
  // solid its AABB overlaps, so a body whose origin is up to PLAYER_RADIUS past
  // a platform edge is held at that platform's height with its entire mass over
  // the void — measured with `ragx`, a corpse 2.45 m in the air reporting
  // grounded=1 for 122 of 300 ticks, which is the "schwebende Leiche" exactly.
  // Support is tested under the ORIGIN instead, and a body that has none there
  // slides off the way a body actually does: clearing `grounded` alone is not
  // enough, because the next tick's sweep puts the still-overlapping box back on
  // the same ledge and the body hovers with the flag flickering. It needs a real
  // push over the edge, so the shortest way OUT of the supporting footprint is
  // what it accelerates along.
  // ...and the point to ask about is the body's MASS, not its origin. The origin
  // is at the feet and the tip pivots the whole skeleton about it, so a body that
  // has gone over by `tilt` carries its trunk DEATH_COM_Y * sin(tilt) away
  // horizontally, along the direction it fell (a point h above the pivot lands at
  // h*sin(tilt) along cross(axis, up) — that is just Rodrigues on (0,h,0)).
  // Measured with ragx: a body killed on the 0.55 m platform slid to the lip and
  // then lay stretched out over the void with its boots on the last 19 cm of
  // concrete — every rendered vertex 0.55 m clear of the floor under it, and
  // grounded the whole time.
  if (r->grounded) {
    v3 perp = {{-r->axis.z, 0.0f, r->axis.x}};
    float pl = sqrtf(perp.x * perp.x + perp.z * perp.z);
    float reach = DEATH_COM_Y * sinf(r->tilt);
    v3 com = *pos;
    if (pl > 1e-4f) {
      com.x += perp.x / pl * reach;
      com.z += perp.z / pl * reach;
    }
    if (pos->y > rag_support(com) + 0.03f) {
      r->grounded = 0;
      // Clearing the flag is not enough on its own: next tick's sweep puts the
      // still-overlapping box back on the same ledge and the body hovers with
      // the flag flickering. It needs a real push over the lip, so it
      // accelerates along the shortest way OUT of the footprint holding it up.
      BP_BOX(i, pos->x - PLAYER_RADIUS, pos->z - PLAYER_RADIUS,
             pos->x + PLAYER_RADIUS, pos->z + PLAYER_RADIUS) {
        const solid_t *sd = &g_solids[i];
        if (fabsf(sd->max.y - pos->y) > 0.05f) continue;
        if (pos->x < sd->min.x - PLAYER_RADIUS || pos->x > sd->max.x + PLAYER_RADIUS ||
            pos->z < sd->min.z - PLAYER_RADIUS || pos->z > sd->max.z + PLAYER_RADIUS)
          continue;
        float dxl = pos->x - sd->min.x, dxh = sd->max.x - pos->x;
        float dzl = pos->z - sd->min.z, dzh = sd->max.z - pos->z;
        float m = fminf(fminf(dxl, dxh), fminf(dzl, dzh));
        if (m == dxl)      r->vel.x -= DEATH_LEDGE_A * TICK_DT;
        else if (m == dxh) r->vel.x += DEATH_LEDGE_A * TICK_DT;
        else if (m == dzl) r->vel.z -= DEATH_LEDGE_A * TICK_DT;
        else               r->vel.z += DEATH_LEDGE_A * TICK_DT;
        break;
      }
    }
  }
  if (!wasg && r->grounded) r->spin *= DEATH_LAND_SPIN;
  if (r->grounded && v0.y < -DEATH_BOUNCE_MIN) {
    // A SKIP. Not because a corpse is bouncy — it is not — but because a body
    // arriving at 6 m/s does not stop on contact, it slaps, lifts a shoulder
    // and comes down again a metre further on. Clearing `grounded` matters as
    // much as the velocity: on the bounce tick the body must take neither the
    // skid friction nor the gravity topple, or the skip is a stutter.
    r->vel.y = -v0.y * DEATH_BOUNCE_E;
    r->vel.x *= DEATH_BOUNCE_H;
    r->vel.z *= DEATH_BOUNCE_H;
    r->spin = fminf(DEATH_SPIN_MAX, r->spin + (-v0.y) * DEATH_BOUNCE_SPIN);
    r->slam = fmaxf(r->slam, fminf(0.60f, -v0.y * 0.09f));
    r->grounded = 0;
  }
  r->slam *= expf(-4.5f * TICK_DT);
  if (r->grounded) {  // skidding: friction bleeds the travel off
    float hs = sqrtf(r->vel.x * r->vel.x + r->vel.z * r->vel.z);
    if (hs > 0.0f) {
      // Viscous AND Coulomb. The viscous term alone only ever asymptotes, so a
      // body arriving fast crept for over a second with an unchanging
      // silhouette and then teleported to a stop at the cut-off; a constant
      // deceleration is what actually stops a body on concrete inside a body
      // length or two.
      float nh = hs * expf(-DEATH_SKID_FRIC * TICK_DT) - DEATH_SKID_MU * TICK_DT;
      if (nh < DEATH_SKID_STOP) nh = 0.0f;
      r->vel.x *= nh / hs;
      r->vel.z *= nh / hs;
    }
  }
  // TOPPLE. In the air the body is FREE — whatever the shot and the wall put
  // into it keeps turning. The old code clamped the tilt at DEATH_TILT_MAX
  // unconditionally, so a thrown body rotated 87 degrees and then translated
  // like a plank for the rest of its flight: the single most inert thing in the
  // whole death, and invisible in a still frame. On the ground gravity takes
  // over and drives it to the nearest lying attitude.
  if (r->grounded) {
    float rest = rag_rest(r->tilt, r->rest_off);
    float d = rest - r->tilt;
    r->spin += (d >= 0.0f ? DEATH_TOPPLE_A : -DEATH_TOPPLE_A) * TICK_DT;
    r->spin *= expf(-DEATH_TILT_FRIC * (0.30f + rag_flat(r->tilt)) * TICK_DT);
    float nt = r->tilt + r->spin * TICK_DT;
    if ((d >= 0.0f && nt >= rest) || (d < 0.0f && nt <= rest)) {
      // Arrived. A rock-back on a hard arrival was tried and removed: with
      // DEATH_LAND_SPIN cutting the tumble on first contact and the grounded
      // integrator damped, the measured peak arrival over 5 seeds x 20 bots
      // hard is 2.35 rad/s, so no gate that reads as "violent" is reachable —
      // it could only ever have been documentation.
      r->tilt = rest;
      r->spin = 0.0f;
    } else {
      r->tilt = nt;
    }
  } else {
    r->tilt += r->spin * TICK_DT;
  }
  // The limb lag is a one-pole chase of the trunk angle: it opens while the
  // body is turning fast and closes to zero the moment it stops, so it costs
  // nothing on a settled corpse and needs no separate decay.
  r->lag += (r->tilt - r->lag) * DEATH_LAG_RATE;
  // The spin about the body's own vertical bleeds only on ground contact, and
  // faster the FLATTER the body is — that is floor friction and nothing
  // subtler: a body still on its boots pivots on two small contacts, one on its
  // back is dragging its whole length. 640 ms upright, 164 ms flat.
  if (r->grounded)
    r->yawv *= expf(-DEATH_YAW_FRIC * (0.35f + rag_flat(r->tilt)) * TICK_DT);
  r->yaw += r->yawv * TICK_DT;
}

// Advance a DEAD body one tick: limb slack ramp, the one-copy integrator, and
// the frozen world-space pose travelling with the origin. Two owners (the
// entity's dead branch and the corpse pool) and ONE copy of this glue — the
// slack ramp and the translate gate drifting between them would desynchronise
// the entity phase from the corpse phase at exactly the handover the two
// integrators already fought over once.
static void rag_advance(rag_t *r, v3 *pos, v3 prev, float *slack, anim_t *a) {
  *slack += 1.0f / BOT_TOPPLE_TICKS;
  if (*slack > 1.0f) *slack = 1.0f;
  rag_step(r, pos);
  vfx_rag_sparks(r, *pos);   // contact feedback, both owners, one call site
  a->prev_pose = a->pose;
  v3 dp = v3_sub(*pos, prev);
  if (v3_dot(dp, dp) > 1e-12f) {
    v3 *j = (v3 *)&a->pose;
    for (int k = 0; k < POSE_NPOS; k++) j[k] = v3_add(j[k], dp);
  }
}

// Difficulty is one row of numbers — the shape of the behavior never changes,
// only how fast it perceives, settles and shoots.
typedef struct {
  float react_ms;            // first sight -> first shot delay
  float err0, err1;          // aim error deg: on acquire -> fully settled
  float settle_ms;           // 95% of the error decay while tracking
  float turn;                // rad/s max tracking turn rate
  float rec_comp;            // 0..1 how much of its own recoil it pulls back out
  int   burst_lo, burst_hi;  // shots per burst
  float pause_lo, pause_hi;  // ms between bursts
  int   memory_ticks, hold_ticks, search_ticks;
  // (movement is NOT a difficulty knob: every tier moves on the player's numbers,
  //  and there is no aim LEAD either — see the aiming code for why)
  float tac;                 // 0..1 how often it slides / peeks / jumps out
} bot_skill_t;

typedef enum {
  BOT_BELIEF_VISIBLE,
  BOT_BELIEF_OCCLUDED_HOLD,
  BOT_BELIEF_INVESTIGATE,
  BOT_BELIEF_SEARCH_SWEEP
} bot_belief_state_t;

static const bot_skill_t BOT_SKILL[3] = {
  //  react  err0  err1  settle turn  reco  burst   pause      mem hold search tac
  { 480, 9.0f, 4.2f, 1300, 3.5f, 0.20f, 2, 4,  550, 950, 360, 42, 216, 0.25f },
  { 300, 6.5f, 2.6f,  950, 5.5f, 0.60f, 3, 6,  380, 700, 540, 60, 312, 0.60f },
  { 180, 4.0f, 0.7f,  700, 8.5f, 0.92f, 4, 8,  240, 480, 720, 78, 432, 1.00f },
};

struct bot_t {
  v3    pos, prev_pos, vel;
  float yaw, prev_yaw, pitch;
  int   active, hp, alive, grounded;
  int   respawn_t;                     // dead: ticks until respawn
  float topple, prev_topple;           // 0..1 collapse blend (limbs go slack)
  // Death ragdoll: the shot's momentum, then gravity, air drag, ground bounce
  // and wall rebound, integrated by the shared `rag_step` the corpse pool also
  // runs. The point is that mass and inertia are visible — a body is flung,
  // cartwheels, skips off the floor and piles up against whatever stops it.
  rag_t rag;
  int   flash, flash_head;             // hit flash ticks + was it a headshot
  v3    target;                        // patrol waypoint
  int   stuck;                         // ticks of wanting to move but not moving
  anim_t anim;                         // skeleton locomotion state + solved pose
  // harness puppet: scripted movement intents replace the AI for this bot
  int   puppet, pup_crouch, pup_jump, pup_slide, pup_ready, pup_ads, sliding;
  float pup_mx, pup_mz, pup_speed, pup_face;
  float pup_lean_t, pup_lean;          // scripted lean: target and eased value
  // combat
  int   tgt;                           // current enemy entity id, -1 = none
  int   lose_t;                        // ticks since the target was last seen
  v3    last_seen;                     // where the target was last spotted
  bot_belief_state_t belief;
  v3    last_seen_vel;
  v3    belief_pos;
  float belief_radius;
  float belief_err_a;                  // WHICH WAY it is wrong, frozen at loss
  long  last_seen_tick;
  v3    search_pos;
  int   search_index, search_dir, search_t, reacquire_count;
  unsigned search_visited;
  int   threat_src, threat_t, threat_score;
  int   react_t;                       // ticks until the first shot may leave
  int   scan_t;                        // ticks until the next target rescan
  int   burst, pause_t;                // trigger discipline (a skill trait)
  float err;                           // current aim error magnitude (deg)
  float off_p, off_y, goal_p, goal_y;  // eased aim-error offset + drift goal
  int   drift_t;                       // ticks until the offset goal resamples
  int   strafe_dir, strafe_t;          // -1/0/+1 and ticks until re-roll
  // tactics
  int   hurt_t;                        // ticks left of "was just shot at"
  float fl_y, fl_p;                    // flinch: transient aim kick, decays
  int   crouch;                        // holding low behind cover
  weapon_t wp;                         // the player's weapon state, per bot
  int   crouch_t;                      // >0 hold left, <0 cooldown before the next
  float eye;                           // eased hitbox eye height (see bot_eye)
  v3    cov_p;                         // the cover spot it committed to
  int   cov_valid;                     // a spot is chosen and not yet reached
  int   cov_retry;                     // ticks until a FAILED probe may re-run
  int   cov_phase;                     // ring start, advanced when a spot fails
  int   in_cover;                      // moving to break the line right now
  int   slide_cd, jump_cd;             // tactical cooldowns
  v3    flank_p;                       // the swing anchor it committed to
  int   flank_t;                       // >0 = committed to the flank, in ticks
  int   flank_cd;                      // ticks before the next flank may start
  int   flank_arrived;                 // lifetime arrivals; census only
  v3    dl_p;                          // deadlock: where the decision sends it
  int   dl_t, dl_cd, dl_push;          // commitment / cooldown / which way
  v3    alert_p;                       // the spot a finished fight left behind
  int   alert_t;                       // ticks of post-contact wariness left
  int   prefire_t, prefire_cd;         // pre-fire burst window / its cooldown
  int   prefiring;                     // this tick fires on BELIEF, not sight
  int   peek;                          // this tick started a corner peek move
  int   startled;                      // this tick is steered by the EAR
  int   stare;                         // consecutive engage-ready-no-shot ticks
  int   pin;                           // consecutive wants-to-move-but-cannot ticks
  int   swap_cd;                       // AI-policy cooldown; never a weapon timer
  // Authoritative auditory investigation. A zero hear_tick means no memory.
  v3    hear_pos;
  long  hear_tick, hear_until;
  bot_noise_kind_t hear_kind;
  float hear_score;
  int   hear_src;                      // WHOSE noise is believed (refresh rule)
  float hear_err_a;                    // which way it mislocates, held per contact
  // Grid route (bot_route, routed maps only): the destination last planned
  // for, the waypoint toward it, and ticks until it is re-planned.
  v3    route_dest, route_wp;
  int   route_t;
};

static bot_t g_bots_all[SRV_LOBBIES][MAX_BOTS];
#define g_bots (g_bots_all[g_lobby])
static int   g_bots_frozen;  // harness switch: no wander, no combat
static int home_calm(void);  // ...and the boot screen's per-shot one
// The boot screen's bots kill each other for the camera's benefit; the match
// they are doing it inside must not END while the player is reading the tiles.
// Same shape as sp_bot_count and for the same reason: the player's own
// `sp_frag` is his MATCH setting and the tile still shows it.
static int sp_frag_limit(void) { return g_home ? 100000 : g_cfg.sp_frag; }

static int rescue_bot_count(void);   // HOSTAGE RESCUE fields its own guards
static int sp_bot_count(void) {
  int rc = rescue_bot_count();
  if (rc >= 0) return rc;
  int n = g_home ? HOME_BOTS : g_cfg.sp_bots;
  return n < MAX_BOTS ? n : MAX_BOTS;
}

typedef struct {
  int  nbots;                                            // active bots
  // ENTITY-id indexed — must follow MAX_ENTS, never MAX_BOTS(+1): these sit
  // directly before over/winner, so one out-of-range write here ends the
  // match with a garbage winner. A sixth counter accidentally sized MAX_BOTS
  // would make all five OOB at once.
  int  kills[MAX_ENTS], deaths[MAX_ENTS];
  int  shots[MAX_ENTS], hits[MAX_ENTS];                  // accuracy source
  bot_noise_t noise[BOT_NOISE_COUNT][MAX_ENTS];          // latest per kind/source
  int  over, winner, last_killer, restart_req;
  // THE MODE LAYER. `mode` is latched from g_cfg.sp_mode at every (re)start, so
  // the MATCH tab's cycle never changes the rules of a match already running;
  // online it is whatever NP_SCORES last said. mscore is the mode's own number
  // per entity — the GUN GAME rung, or LAST MAN STANDING rounds won — and is a
  // replicated counter like kills (match_counters_clear owns it too).
  int  mode;
  int  mscore[MAX_ENTS];
  // LAST MAN STANDING between rounds: round_t counts the intermission down
  // (0 = a round is live), round_win is who took the last one (-1 = nobody).
  int  round_t, round_win, round_no;
  int  round_clock;                  // ticks the live round has run (server)
} match_t;

// How long a NETWORKED match holds the final scoreboard before the arena
// regenerates. 10 s at 120 Hz: long enough to read a 20-name board, short
// enough that a public instance is never parked. ONE threshold, read by the
// server (which owns the restart), by the client (which only draws the
// countdown) and by nothing at all offline, where the rematch is still a
// fire press.
#define MATCH_OVER_TICKS 1200

static match_t g_match_all[SRV_LOBBIES];
#define g_match (g_match_all[g_lobby])
// THE OVER COUNTDOWN IS PER LOBBY, and that is the entire point of it living
// here. It used to be one `static int over_t` inside match_tick, while a
// dedicated server ticks EVERY arena through match_tick once per server tick
// (the g_lobby loop in server_main). So on a --lobbies 16 instance the
// fifteen arenas that were NOT over each ran the `= 0` reset once per tick
// and the counter could never climb past 1: the auto-rematch never fired in
// public, and the arena that finished sat frozen on its final scoreboard for
// the life of the process.
//
// Kept OUT of match_t even though that struct is already per-lobby:
// match_restart memsets it and its TAIL LAYOUT IS LOAD-BEARING (see the
// ENTITY-id comment on the counter arrays above — a field appended there is
// one out-of-range write away from a garbage winner). Same reason
// g_humans_on lives outside it. Never collapse this back to a single static.
static int g_over_t_all[SRV_LOBBIES];
#define g_over_t (g_over_t_all[g_lobby])
static int g_match_nbots_fwd(void) { return g_match.nbots; }  // for ent_next

