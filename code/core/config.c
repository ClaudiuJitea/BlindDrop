// code/core/config.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Config — plain "key value" lines in config.cfg next to the executable.
// Created with defaults on first start; unknown keys are ignored with a note.
// ---------------------------------------------------------------------------

// Grouped by what the hand does: movement (leaning is a movement, not a weapon
// mode), then the weapon, then the meta keys. The KEYS tab lists them in this
// order, so the enum IS the menu layout.
typedef enum {
  IN_FWD, IN_BACK, IN_LEFT, IN_RIGHT, IN_JUMP, IN_CROUCH, IN_LEAN_L, IN_LEAN_R,
  IN_FIRE, IN_ADS, IN_RELOAD, IN_SWAP, IN_SLOT1, IN_SLOT2, IN_SLOT3, IN_SLOT4, IN_SLOT5,
  IN_SCORE, IN_VIEW,
  IN_COUNT
} action_t;

// ONE row per action. There used to be four parallel [IN_COUNT] tables — config
// key, default token, menu label and script name — each a bare positional list
// in a different part of the file, so inserting an action anywhere but at the
// end silently permuted three of the four: the KEYS tab would relabel a row, the
// config would persist a binding under another action's key, and `+forward`
// would press something else. Designated initialisers make the enum the single
// ordering authority, and a missing row is a null the compiler shows up rather
// than a shift nobody notices.
typedef struct { const char *key, *dflt, *label, *script, *pdflt; } action_def_t;
// pdflt is the GAMEPAD default (second bind table, config key "pad<key>").
// Movement has no pad default because the left stick IS movement — "none" is a
// valid pad bind and stays "none" through resolve_binds rather than falling
// back, so unbinding a pad action is possible where a dead keyboard action is
// not. Layout follows the console-shooter convention (jump A, crouch/slide B,
// lean on the bumpers, triggers fire/aim) so a Deck player's hands already
// know it.
static const action_def_t ACTION_DEF[IN_COUNT] = {
  [IN_FWD]     = {"bind_forward",    "w",      "FORWARD",          "forward",    "none"},
  [IN_BACK]    = {"bind_back",       "s",      "BACK",             "back",       "none"},
  [IN_LEFT]    = {"bind_left",       "a",      "LEFT",             "left",       "none"},
  [IN_RIGHT]   = {"bind_right",      "d",      "RIGHT",            "right",      "none"},
  [IN_JUMP]    = {"bind_jump",       "space",  "JUMP",             "jump",       "pad_a"},
  [IN_CROUCH]  = {"bind_crouch",     "lshift", "CROUCH",           "crouch",     "pad_b"},
  [IN_LEAN_L]  = {"bind_lean_left",  "q",      "LEAN LEFT",        "lean_left",  "pad_lb"},
  [IN_LEAN_R]  = {"bind_lean_right", "e",      "LEAN RIGHT",       "lean_right", "pad_rb"},
  [IN_FIRE]    = {"bind_fire",       "mouse1", "FIRE",             "fire",       "pad_rt"},
  [IN_ADS]     = {"bind_ads",        "mouse2", "AIM",              "ads",        "pad_lt"},
  [IN_RELOAD]  = {"bind_reload",     "r",      "RELOAD",           "reload",     "pad_x"},
  [IN_SWAP]    = {"bind_swap",       "x",      "SWAP WPN",         "swap",       "pad_y"},
  [IN_SLOT1]   = {"bind_slot1",      "1",      "WEAPON 1 AR",      "slot1",      "pad_left"},
  [IN_SLOT2]   = {"bind_slot2",      "2",      "WEAPON 2 SNIPER",  "slot2",      "pad_right"},
  [IN_SLOT3]   = {"bind_slot3",      "3",      "WEAPON 3 PISTOL",  "slot3",      "none"},
  [IN_SLOT4]   = {"bind_slot4",      "4",      "WEAPON 4 KNIFE",   "slot4",      "none"},
  [IN_SLOT5]   = {"bind_slot5",      "5",      "WEAPON 5 GRENADE", "slot5",      "none"},
  [IN_SCORE]   = {"bind_score",      "tab",    "SCOREBOARD",       "score",      "pad_select"},
  [IN_VIEW]    = {"bind_view",       "v",      "VIEW 1ST/3RD",     "view",       "pad_down"},
};

// Tunable movement parameters — every value the in-game MOVEMENT panel
// exposes. Table-driven so config I/O and the UI sliders share one source.
// Each maps to one perceivable quality in a unit a designer can picture
// (seconds, meters, percent); the sim derives its internal rates from these,
// so no combination of sliders can disable another mechanic.
typedef struct { const char *key, *label; float def, lo, hi, step; } param_def_t;

typedef enum {
  MV_RUN, MV_CROUCH_SP, MV_ACCEL_MS, MV_STOP_MS, MV_JUMP_H,
  MV_AIR_CTL, MV_SLIDE_BOOST, MV_SLIDE_LEN, MV_FOV_KICK, MV_COUNT
} mv_param_t;

static const param_def_t MV_DEF[MV_COUNT] = {
  [MV_RUN]         = {"mv_run_speed",     "RUN SPEED",      7.0f,  2.0f,   14.0f,  0.1f},
  [MV_CROUCH_SP]   = {"mv_crouch_speed",  "CROUCH SPEED",   3.0f,  0.5f,    6.0f,  0.1f},
  [MV_ACCEL_MS]    = {"mv_accel_ms",      "ACCEL TIME",   100.0f, 30.0f, 1000.0f, 10.0f},
  [MV_STOP_MS]     = {"mv_stop_ms",       "STOP TIME",    100.0f, 30.0f, 1000.0f, 10.0f},
  [MV_JUMP_H]      = {"mv_jump_height",   "JUMP HEIGHT",    1.2f,  0.3f,    3.0f,  0.1f},
  [MV_AIR_CTL]     = {"mv_air_ctl_pct",   "AIR CONTROL",   20.0f,  0.0f,  100.0f,  5.0f},
  [MV_SLIDE_BOOST] = {"mv_slide_boost_x", "SLIDE BOOST",    3.0f,  1.0f,    5.0f,  0.1f},
  [MV_SLIDE_LEN]   = {"mv_slide_len",     "SLIDE LENGTH",   8.0f,  2.0f,   20.0f,  0.5f},
  // 0.0 BY CHOICE (2026-08-19 play session), not by the old accident. History,
  // because the two zeros look identical in a diff and mean opposite things:
  // the slide FOV kick was authored, eased at FOV_KICK_RATE (about 110 ms in
  // and out), rendered every frame — and SHIPPED MULTIPLIED BY ZERO, with its
  // only editor row inside the DEV tab a release build deletes. That was a bug,
  // and 9.0 was the fix. Measured then: the identical slide frame at 22.8 m/s
  // with the kick at 12 differs from a 0.00 render by 269975 pixels (29.3% of
  // the frame, meanE 6.96) — so the effect is real and large, and it is the
  // SIZE of it that lost. Tuned live against the rest of this pass (7 m/s run,
  // 100 ms accel/stop, 8 m slide) the widening reads as a lurch rather than as
  // speed. The slider is live in the MOVEMENT panel; anyone who wants it turns
  // it up, and 9.0 is the value to turn it up TO.
  [MV_FOV_KICK]    = {"mv_fov_kick",      "FOV KICK",       0.0f,  0.0f,   20.0f,  0.5f},
};

// Weapon tuning — one algorithm, per-weapon parameter sets. WP_DEF describes
// each FEEL parameter once (label, range, config-key suffix); every weapon
// owns a row in g_cfg.wp, persisted as "<prefix>_<key>" (ar_rpm, sr_rpm, ...)
// and edited live in the WEAPON tab's per-weapon editor. Structural identity
// (mag, damage, fire mode, reload) lives in WPN_DEF below, in code.
typedef enum { WPN_AR, WPN_SR, WPN_PT, WPN_KN, WPN_GR, WPN_COUNT } wpn_id_t;

typedef enum {
  WP_RPM, WP_KICK_UP, WP_KICK_SIDE, WP_RECOVER_MS, WP_SPREAD, WP_BLOOM,
  WP_ADS_MS, WP_ADS_ZOOM, WP_PUNCH, WP_SWAY, WP_COUNT
} wp_param_t;

// The `def` column is deliberately 0 here: weapon defaults are PER-WEAPON and
// live in WP_DEFAULTS below — the one authority config_defaults and the DEV
// reset both read. A value here would be a second, driftable copy of the AR
// row, and a future reader of WP_DEF[i].def should fail loudly instead.
static const param_def_t WP_DEF[WP_COUNT] = {
  [WP_RPM]        = {"rpm",        "FIRE RATE",  0,  40.0f, 1000.0f, 10.0f},
  [WP_KICK_UP]    = {"kick_up",    "KICK UP",    0,   0.0f,    3.0f,  0.05f},
  [WP_KICK_SIDE]  = {"kick_side",  "KICK SIDE",  0,   0.0f,    1.0f,  0.05f},
  [WP_RECOVER_MS] = {"recover_ms", "RECOVERY",   0,  50.0f,  800.0f, 10.0f},
  [WP_SPREAD]     = {"spread",     "ADS SPREAD", 0,   0.0f,    0.5f,  0.01f},
  [WP_BLOOM]      = {"bloom",      "BLOOM/SHOT", 0,   0.0f,    0.4f,  0.01f},
  [WP_ADS_MS]     = {"ads_ms",     "ADS TIME",   0,  80.0f,  500.0f, 10.0f},
  [WP_ADS_ZOOM]   = {"ads_zoom",   "ADS ZOOM",   0,  40.0f,  100.0f,  1.0f},
  [WP_PUNCH]      = {"punch",      "VIEW PUNCH", 0,   0.0f,    3.0f,  0.1f},
  [WP_SWAY]       = {"sway",       "SWAY/BOB",   0,   0.0f,    3.0f,  0.1f},
};

// Feel defaults per weapon: the AR is a controllable full-auto laser, the
// sniper a violent single shot — huge kick, long recovery, pinpoint in ADS.
// Re-tuned live in the WEAPON panel on 2026-08-19 against the slower body
// (7 m/s run): with less travel speed to fight, both guns were over-recoiled
// for the engagement ranges the arena actually produces.
static const float WP_DEFAULTS[WPN_COUNT][WP_COUNT] = {
  [WPN_AR] = {750, 0.25f, 0.15f, 200, 0.05f, 0.05f, 180, 80, 0.50f, 1.0f},
  // ads_zoom 80, NOT 100, and this DELIBERATELY reverses the earlier rule that
  // the sniper's ADS must leave the main view life-size because the scope's
  // picture-in-picture pass owns the magnification. The reasoning was sound and
  // the feel was not: at 100 the world outside the tube does not move at all
  // when the gun comes up, so the most committing action in the game reads as
  // if nothing happened, and the PiP circle looks pasted onto a static frame.
  // 80 is a mild pull-in UNDER the scope — the periphery still stays far wider
  // than the tube, so the "look through a scope, keep your surroundings" intent
  // survives; it just gets a beat of movement to sell the transition.
  // rpm 1000 is the slider ceiling and it is not a fire rate: the SR is
  // semi-auto (fire mode is structural, in WPN_DEF), so this only sets the
  // minimum interval between two trigger pulls — 60 ms, i.e. "as fast as you
  // can click". Anything lower was felt as the gun eating deliberate shots.
  [WPN_SR] = {1000, 1.80f, 0.20f, 500, 0.03f, 0.03f, 200, 80, 1.00f, 1.0f},
  // A sidearm: quick to the eye and sharp in the hand, every shot its own snap.
  [WPN_PT] = {420, 0.70f, 0.18f, 170, 0.08f, 0.09f, 130, 88, 0.80f, 1.0f},
  // The knife spends only three of these. ADS TIME is how fast the guard comes
  // up, VIEW PUNCH how hard a landed swing jolts the view and SWAY its idle
  // float; the swing itself is timed by MELEE below, not by rpm.
  [WPN_KN] = {60, 0.0f, 0.0f, 200, 0.0f, 0.0f, 110, 100, 0.40f, 1.0f},
  // Only SWAY matters to a grenade: timing and flight are the NADE_* numbers.
  [WPN_GR] = {60, 0.0f, 0.0f, 200, 0.0f, 0.0f, 110, 100, 0.40f, 1.0f},
};

// Structural weapon identity — not sliders: changing these changes WHAT the
// weapon is, not how it feels. hip_spread is the unaimed penalty in degrees
// (a sniper fired from the hip is a prayer).
typedef struct {
  const char *name, *prefix;
  int   mag, dmg, head_x, reload_ticks;
  // A TACTICAL RELOAD IS NOT AN EMPTY ONE, and shipping only the empty case
  // meant the FASTEST and COMMONEST reload in the game was animated and scored
  // as the slowest, rarest one: with a round still chambered the bolt never
  // locked back, so racking the charging handle and playing a "bolt release"
  // are both events that did not happen. Splitting them is where "fast" is
  // actually felt — 124 ticks against 152 is 233 ms a player gets back every
  // time they reload before running dry, which is the decision the split exists
  // to reward. A BOLT GUN works its action either way, so the sniper's two
  // durations are deliberately identical and its buffer is shared.
  int   reload_tac_ticks;
  int   full_auto;      // 0 = one shot per trigger press (bolt/semi)
  float hip_spread;     // deg added at ads 0, fading out toward full ADS
  int   sfx_fire;       // which synth buffer a shot triggers
  int   sfx_reload;     // ...and which reload, because a bolt gun does not
                        // change a magazine
  float sfx_hit_t;      // seconds to delay the hit/kill confirmation by. It has
                        // to clear the shot that caused it, and the sniper's
                        // report is three times longer than the AR's, so this
                        // cannot be one constant.
  // MUZZLE VELOCITY, and it exists for exactly one consumer: the fly-by crack
  // has to ARRIVE BEFORE the muzzle report. The round travels at ~900 m/s and
  // the report at 343, so a shot taken 19 m away puts its bow wave 35 ms ahead
  // of its bang and one at the arena diagonal 74 ms ahead — and crack-THUMP is
  // the single most recognisable "that round was aimed at ME" cue in the medium.
  // Measured before this, over 233 ticks carrying exactly one whizz and one
  // shooter, the lead was 0 ms on 192 of them and never more than 0: the crack
  // was scheduled at t/343, i.e. at the speed of SOUND. Nothing in the sim reads
  // this — the rounds are hitscan and stay hitscan.
  float v_round;        // m/s, for the fly-by crack's lead over the report
  // NO `pen` COLUMN. Penetration used to be weapon identity — 0.62 m for the AR
  // against 0.72 for the sniper — and it is one rule for both guns now
  // (PEN_PATH). Two table cells holding the same number with nothing keeping
  // them equal is the driftable second copy this file refuses everywhere else;
  // the day the two weapons should differ again, the column comes back and
  // gun_shoot reads def->pen instead of the constant.
} wpn_def_t;

// Sound ids live here so WPN_DEF can reference them; buffers are built in
// the audio section.
enum {
  SFX_FIRE_AR, SFX_FIRE_SR,          // the shot as the shooter hears it
  SFX_FAR_AR, SFX_FAR_SR,            // ...and as someone across the arena does
  SFX_HIT, SFX_HIT_HEAD,             // your hit confirmation
  SFX_KILL, SFX_KILL_HEAD,           // your kill confirmation
  SFX_HURT,                          // taking a round yourself
  SFX_IMPACT,                        // a bullet arriving in the world
  SFX_WHIZZ,                         // one going past your head
  SFX_STEP,                          // a boot landing
  SFX_DRY, SFX_RELOAD_AR, SFX_RELOAD_SR,
  // A NEW ID GOES AT THE END, ALWAYS. sfx_alloc reseeds the noise stream FROM
  // THE ID, so inserting one in the middle renumbers every sound after it and
  // silently re-rolls their noise — measured, adding this one mid-enum changed
  // dryfire and both reload buffers byte for byte while their descriptors moved
  // by nothing anybody could name. That is the same coupling the per-sound
  // stream was introduced to remove, reintroduced through the back door.
  SFX_FLESH,                         // a round arriving in a MAN
  SFX_RELOAD_AR_TAC,                 // ...and the AR's tactical reload
  SFX_FIRE_PT, SFX_RELOAD_PT,        // the pistol
  SFX_KNIFE_SWING, SFX_KNIFE_WALL,   // a blade through air, and into a wall
  SFX_KNIFE_BLOCK,                   // steel on steel
  SFX_NADE_PIN, SFX_NADE_BOUNCE,     // the pin, and steel skipping on the ground
  SFX_BLAST, SFX_BLAST_FAR,          // a detonation, close and across the arena
  SFX_NADE_SPOON, SFX_NADE_EQUIP,    // the lever's ping, and a glove closing on it
  // No two shots are the same explosion, so each gun has a set of takes (see
  // sfx_shot_id). The slap is what the nearest wall sends back, placed at run time.
  SFX_FIRE_AR2, SFX_FIRE_AR3, SFX_FIRE_AR4, SFX_FIRE_AR5,
  SFX_FIRE_SR2, SFX_FIRE_SR3,
  SFX_FIRE_PT2, SFX_FIRE_PT3, SFX_FIRE_PT4,
  SFX_FAR_PT, SFX_SLAP,
  SFX_BRASS,                         // one case landing — played per bounce
  SFX_CLINK,                         // a round ringing off body armour
  SFX_ALARM,                         // HOSTAGE RESCUE: the compound siren
  SFX_SNIP,                          // a hostage's restraints cut
  SFX_CHIME,                         // an objective completed
  // SAMPLED ONLY (code/audio/samples.c): no synth recipe, the buffer exists only
  // when the embedded pack carries it, and every caller checks for that.
  SFX_FIRE_AK1, SFX_FIRE_AK2, SFX_FIRE_AK3,             // the loadout guns' own reports
  SFX_FIRE_FAMAS1, SFX_FIRE_FAMAS2, SFX_FIRE_FAMAS3,
  SFX_FIRE_GLOCK1, SFX_FIRE_GLOCK2, SFX_FIRE_GLOCK3,
  SFX_FIRE_DEAGLE1, SFX_FIRE_DEAGLE2, SFX_FIRE_DEAGLE3,
  SFX_DEATH1, SFX_DEATH2, SFX_DEATH3, SFX_DEATH4, SFX_DEATH5,   // a man going down
  SFX_BODYFALL1, SFX_BODYFALL2, SFX_BODYFALL3,                 // ...and hitting the floor
  // More takes of the kill confirmation (SFX_KILL / SFX_KILL_HEAD are take 1):
  // every kill sounding the same sentence was the complaint, see kill_takes.
  SFX_KILL2, SFX_KILL3, SFX_KILL4, SFX_KILL_HEAD2, SFX_KILL_HEAD3,
  SFX_DIE1, SFX_DIE2,                // the player's OWN death, first person
  SFX_COUNT
};

static const wpn_def_t WPN_DEF[WPN_COUNT] = {
  // PENETRATION IS NOT IN THIS TABLE ANY MORE — see PEN_PATH. What was here was
  // a per-weapon budget plus a damage curve, and the numbers it produced are
  // worth keeping written down because they are what the current rule replaced:
  // the AR crossed a standard 0.60 m cover wall for SEVEN damage (the comment
  // that stood here claimed eleven — it predated the fade tail and was stale,
  // caught by firing the shot rather than by reading it) and the sniper for 84,
  // deliberately under the 100 hp one-shot. Both are full damage now, 26 and
  // 150, and the sniper's through-wall body shot is a kill.
  [WPN_AR] = {"AR",     "ar", 30,  26, 2, 152, 124, 1, 0.12f, SFX_FIRE_AR,
              SFX_RELOAD_AR, 0.045f, 900.0f},
  // 0.60 off the scope, not 3.00: at 3 degrees the crosshair opened as wide as a
  // fifth of the screen and a shot at 20 m could land a metre from it. Still twice
  // the pistol, so the scope remains the way to place a round.
  [WPN_SR] = {"SNIPER", "sr",  5, 150, 2, 214, 214, 0, 0.60f, SFX_FIRE_SR,
              SFX_RELOAD_SR, 0.130f, 840.0f},
  [WPN_PT] = {"PISTOL", "pt", 15,  34, 2, 118,  96, 0, 0.32f, SFX_FIRE_PT,
              SFX_RELOAD_PT, 0.050f, 370.0f},
  // One "round" that is never spent, so a reload can never start and the empty
  // checks every weapon path shares stay true without a knife exception.
  // dmg is unused: each swing in MELEE carries its own.
  [WPN_KN] = {"KNIFE",  "kn",  1,   0, 2,   1,   1, 1, 0.00f, SFX_KNIFE_SWING,
              SFX_KNIFE_SWING, 0.030f, 0.0f},
  // `mag` is the grenades you spawn with. They never reload.
  [WPN_GR] = {"GRENADE", "gr", 2,   0, 1,   1,   1, 0, 0.00f, SFX_NADE_PIN,
              SFX_NADE_PIN, 0.030f, 0.0f},
};

// THE LOADOUT VARIANTS (see WPN_VARS). One row per slot that has them — [0] the
// rifle, [1] the pistol — and per variant; row 0 of each is never read (variant 0
// IS WPN_DEF and WP_DEFAULTS, untouched). Structural numbers replace the slot's;
// `feel` MULTIPLIES the slot's live tuning (WP_* order), so a developer dragging
// the AR's kick still moves all three rifles together, and each keeps its own
// character relative to the others. The reload timeline stays the slot's: the
// synthesized reload sounds and the animation's beats are cut to it.
//   AK-47     heavier round, slower and harder to hold, 30 rounds
//   FAMAS     1000 rpm, light kick, lighter round, 25 rounds
//   GLOCK-18  the full-auto pistol: 33-round stick, weak rounds, wild hip fire
//   DEAGLE    seven rounds that two-shot to the body and one-shot the head
#define WPN_VARS 3
typedef struct {
  int   mag, dmg, full_auto;
  float hip_spread, v_round;
  float feel[WP_COUNT];   // RPM KICK_UP KICK_SIDE RECOVER SPREAD BLOOM ADS ZOOM PUNCH SWAY
  float pitch, level;     // the report's playback rate and level against the slot's
} wpn_var_t;
static const wpn_var_t WPN_VAR[2][WPN_VARS] = {
  { {0},
    {30, 33, 1, 0.15f, 715.0f, {0.80f, 1.45f, 1.50f, 1.10f, 1.30f, 1.30f, 1.10f, 1.0f, 1.35f, 1.1f},
     0.86f, 1.05f},
    {25, 23, 1, 0.12f, 960.0f, {1.33f, 0.85f, 0.80f, 0.90f, 0.90f, 1.10f, 1.00f, 1.0f, 0.85f, 1.0f},
     1.12f, 0.95f} },
  { {0},
    {33, 21, 1, 0.40f, 375.0f, {2.60f, 0.32f, 1.40f, 0.80f, 1.20f, 0.55f, 0.95f, 1.0f, 0.45f, 1.0f},
     1.15f, 0.90f},
    { 7, 58, 0, 0.45f, 470.0f, {0.62f, 2.30f, 1.50f, 1.60f, 1.10f, 2.20f, 1.20f, 1.0f, 2.00f, 1.2f},
     0.72f, 1.15f} },
};

// THE GRENADE. A fragmentation grenade on a 3.5 s fuse that starts as the pin comes
// out: hold FIRE to pull it and cook, release to throw, and hold AIM as you release
// to lob it underhand. Hold it past the fuse and it goes off in your hand.
#define NADE_FUSE      448      // ticks from the press: NADE_PIN_T, then 3.33 s
// The pin takes this long; a tap still pulls it. 0.4 s, not the 0.17 s it was:
// at 20 ticks the off hand had to reach, hook and draw in under two frames of a
// 60 Hz screen, which no hand does and which read as the pin vanishing. The
// fuse grew by the same 28 ticks, so spoon-to-blast is unchanged.
#define NADE_PIN_T     48
// The throw is a real one: the arm draws back beside the head, whips over and
// lets go at full extension, follows through across the body, and the next
// grenade comes up from below. At 56/16 the hand went straight from the hold to
// the release in 0.13 s with no wind-up, and the whole thing read as a flick.
#define NADE_THROW_T   100      // the whole throw: 0.83 s
#define NADE_WIND_T    12       // the arm is fully drawn back
#define NADE_RELEASE_T 28       // ...whips through, and the hand opens (0.23 s)
#define NADE_RISE_T    66       // the hand is out of frame; the next one comes up
#define NADE_SPEED     16.5f    // m/s overhand, about a soldier's throw
#define NADE_LOB       6.5f     // m/s underhand
#define NADE_GRAVITY   9.81f
#define NADE_DRAG      0.0009f  // per tick, a small fast sphere barely feels air
#define NADE_BOUNCE    0.34f    // steel on hard ground: a dead thud, not a ball
#define NADE_FRICTION  0.62f    // tangential speed kept per bounce
#define NADE_ROLL      0.986f   // per tick while rolling
#define NADE_R         0.036f   // collision radius
// THE BLAST. Inside BLAST_CORE nobody lives; past it fragments thin out with
// distance to nothing at BLAST_R. Everything is measured to the body, and each
// of three points on it needs a clear line, so cover really is cover: a man with
// only his head showing takes a third of it.
#define BLAST_R        9.0f
#define BLAST_CORE     2.0f
#define BLAST_DMG      200.0f
#define BLAST_SHOVE    11.0f    // m/s of throw at the centre

// THE KNIFE'S THREE SWINGS. A chain alternates forehand and backhand slashes and
// closes on a thrust; a pause longer than MELEE_CHAIN_TICKS restarts it. Ticks
// are 1/120 s. The strike window is when the blade is actually moving through
// the target, and the sim sweeps real rays across the same arc the animation
// draws, from a0 to a1 (radians of yaw, + = right) and p0 to p1 (pitch), so a
// slash hits what it visibly crosses.
typedef struct {
  int   total, strike0, strike1, dmg;
  float reach, a0, a1, p0, p1;
} melee_def_t;
static const melee_def_t MELEE[3] = {
  {54, 14, 22, 50, 1.90f,  0.62f, -0.62f,  0.12f, -0.16f},   // forehand, R -> L
  {54, 14, 22, 50, 1.90f, -0.62f,  0.62f, -0.14f,  0.14f},   // backhand, L -> R
  {72, 24, 30, 75, 2.25f,  0.00f,  0.00f,  0.00f,  0.00f},   // thrust: reach, not arc
};
#define MELEE_CHAIN_TICKS 42   // pause that restarts the chain
#define MELEE_PARRY_TICKS 26   // a guard raised within 217 ms of the hit PARRIES
#define MELEE_STAGGER_TICKS 66 // 0.55 s the parried attacker cannot act
#define MELEE_GUARD_ARC 0.26f  // cos(75 deg): the guard covers the front only
#define MELEE_CHIP 10          // a blocked (not parried) hit still costs this

// The reload's two defining beats as fractions of reload_ticks. The audio
// events and the viewmodel's punch impulses are two consumers of one timeline —
// restating the fractions in both is how the A/V contract on the mechanic's
// defining moment drifts.
//
// PER WEAPON, because the two beats are not the same two events on the two guns:
// the AR seats a magazine and drops a bolt, the sniper seats a magazine and then
// WORKS ITS ACTION. Both were once punched at the AR's 0.560/0.960 while the
// sniper was a five-round stripper load, so its first punch landed on the fourth
// of five identical round presses — i.e. on nothing — and its loudest event got
// no beat at all. Filmed at 13 stations, 22-29% of the frame was in motion from
// prog 0.24 to 0.48 with nothing happening at the action, which is what "the
// rifle waves about" looks like. The rows below are the events the buffers
// actually have; they must be re-read from the SFX_RELOAD_* builders whenever
// those move, or the beat lands in silence again.
static const float RELOAD_BEAT[WPN_COUNT][2] = {
  {0.560f, 0.960f},   // AR: the magazine SEATS, then the bolt goes home
  // The sniper changes a MAGAZINE now, and its beats moved with it: the box
  // seats at 0.56 like the AR's, then the bolt is worked and comes down locked
  // at 0.955. What was here — 0.860 and 0.970 — belonged to a five-round
  // stripper load, where the two loudest events were "bolt forward" and "bolt
  // down" and there was no magazine at all.
  {0.560f, 0.955f},
  {0.560f, 0.960f},   // pistol: the magazine seats, then the slide drops
  {0.560f, 0.960f},   // knife: never reloads; the row keeps the table total
  {0.560f, 0.960f},   // grenade: likewise
};

// The magazine's insert parameter, 0 at the start of the travel window and 1 at
// the seat. Named once because gun_anim needs it three times inside one ternary
// chain and writing the clamp out three times is how the curve came to be
// (1-u)*(1-u)*(1-u) instead of the 1-u^3 its own comment specifies.
// THE WINDOW END IS THE SEAT BEAT: 0.40 + 0.16 = RELOAD_BEAT[*][0] = 0.560
// exactly. Move the beat without this window and the magazine either parks
// early (nothing left to slam) or teleports its remaining travel in one tick.
#define GUN_MAG_U(prog) f_clamp(((prog) - 0.40f) / 0.16f, 0.0f, 1.0f)
// The TACTICAL AR runs a different clock AND a different seat: its buffer
// authors THE SEAT as the LAST event at 0.880 of reload_tac_ticks (atk/eng
// land on it — the "weapon is live" contract, because nothing follows: no
// charging handle, no bolt release). The pose must read the same fraction or
// the magazine visibly slams ~330 ms before the seat sound — the exact A/V
// desync the one-timeline rule exists to prevent, reproduced on the COMMONEST
// reload in the game.
#define RELOAD_SEAT_TAC 0.880f
#define GUN_MAG_U_TAC(prog) \
  f_clamp(((prog) - 0.40f) / (RELOAD_SEAT_TAC - 0.40f), 0.0f, 1.0f)

static const param_def_t VOL_DEF = {"volume", "VOLUME", 0.7f, 0.0f, 1.0f, 0.05f};
// The level the LIMITER sees, fixed for every volume setting, so the confirmation
// ladder and the duck step are measured against one bus whatever the player has
// the knob at. It is the shipped default, which is what makes a default session
// bit-identical to the one every number in this file was tuned on.
#define VOL_REF 0.70f

// Singleplayer deathmatch settings (MATCH tab). Entity ids everywhere — the
// THREE-RANGE rule:
//   e == 0                      -> human 0 (the local player in SP)
//   1 <= e <= MAX_BOTS          -> bot e - 1
//   e >  MAX_BOTS               -> human e - MAX_BOTS
// Humans at 0 and 21+, bots at 1..20: in singleplayer every id is identical
// to the historical layout, so every nbots loop and the byte gate stay valid.
// RULE WITH TEETH: every array indexed by an ENTITY ID must be MAX_ENTS —
// a MAX_BOTS(+1)-sized one is an out-of-bounds the moment id > MAX_BOTS
// exists (g_skid_t and the match counters were exactly that). g_bots[],
// BOT_NAMES[], SP_BOTS_DEF's range and tacstat's was_sl/was_air are
// BOT-indexed and must STAY MAX_BOTS — a blanket sweep is wrong.
#define MAX_BOTS   20
#define MAX_HUMANS 8
#define MAX_ENTS   (MAX_BOTS + MAX_HUMANS)
// g_rng_gun is declared above this point (the RNG section precedes the roster
// constants) and must be sized MAX_HUMANS — it is indexed by human slot. The
// comment there says so; this is what makes raising MAX_HUMANS a compile error
// instead of an out-of-bounds write nobody sees.
static_assert(sizeof g_rng_gun / sizeof g_rng_gun[0] == MAX_HUMANS,
              "g_rng_gun is PER HUMAN: size it MAX_HUMANS");
// Each remote human's LOADOUT as their last input carried it (net_input_t), read
// by the server at that human's next spawn. The local human's comes from g_cfg.
static uint8_t g_human_loadout_all[SRV_LOBBIES][MAX_HUMANS];
#define g_human_loadout (g_human_loadout_all[g_lobby])

// Dedicated-server mode. DECLARED ABOVE THE PLATFORM SPLIT —
// g_headless sits inside #ifndef _WIN32 and copying that idiom here breaks
// the MinGW build exactly in app_start's gate. Runtime flag, set only by
// --server in main(); the Windows target never sets it but must compile it.
static int g_server;
// THE port, for every role: server listen and the client's mp_port. ONE
// number, because the two used to differ (--port defaulted to 27015, mp_port
// to 27000) and a server started without --port therefore listened where no
// client ever dialled.
//
// 31415 was chosen against four criteria, in this order:
//   - BELOW 32768. /proc/sys/net/ipv4/ip_local_port_range is 32768-60999 on
//     both this container and the VPS: a listener inside that window can lose
//     the bind to an outbound socket the kernel handed the same port first.
//     That is the only one of the four that is a correctness argument.
//   - Not IANA-assigned and not any game's default. 27015 is Valve's (Source,
//     CS, TF2) and is swept continuously by Shodan/Censys and by A2S_INFO
//     reflection scanners — a public 27015 collects hostile UDP from day one
//     for no reason other than the number. This is noise reduction, NOT a
//     security measure: the protection is NET_CONNECT_MIN, the cookie and the
//     input budget below, all of which hold on any port.
//   - Not in Steam's 27000-27100 block, which a player's own Steam client
//     uses and which some routers special-case.
//   - Memorable (pi), because a human types it into mp_port.
#define NET_PORT_DEF 31415
// Server parameters COME FROM ARGV, not from the config: argv is the
// interface the deploy needs anyway — the Makefile's setsid + respawn wrapper
// passes SERVER_ARGS on the command line — and it deletes the config-trap quartet
// (parser switch, emit list, unknown-key deletion, live slider for a
// bind-time value). 1 <= cap_public <= MAX_HUMANS is CHECKED at
// startup — a chain that holds nowhere by itself, and whose upper end is the
// one the old cap_hard=10 quietly violated (the binary has no ninth human).
static struct {   // consumed by the Linux server half only
  int port, cap_bots, cap_public, skill, fraglimit, lobbies, mode, rounds, map;
  long ticks;          // 0 = run forever; N = exit after N ticks (test/RSS runs)
  char name[24];       // --name: what a LAN scan lists this server as
  int team_size, team_fill;   // --team-size N, --team-fill 0|1 (TEAM DEATHMATCH)
  int team_ff;                // --friendly-fire 0|1
  int squad;                  // --squad N: HOSTAGE RESCUE squad size 1..3
// DESIGNATED, and not merely as a style preference. This was positional, and
// adding `lobbies` to the int line above put it BEFORE `ticks` while the
// initializer still carried the old order — so the default became lobbies = 0,
// ticks = 1 and the server ran exactly ONE tick and exited cleanly, over and
// over, under the deploy's respawn loop (the Makefile's setsid-ed
// `while :; do ./game …; sleep 2; done` wrapper — it was systemd's
// Restart=always when this bit; the failure shape is identical). Nothing in the
// harness saw it because every rig passes --ticks explicitly; the deploy's own
// probe caught it on the first run — today that is `make server-deploy`'s
// loopback CONNECTED probe plus the crash-loop gate that counts `exit rc=` lines
// in game.log after it. A positional initializer over a struct whose field list
// is edited is a silent trap, and this one only has to be sprung once.
} g_srv = {.port = NET_PORT_DEF, .cap_bots = 2, .cap_public = 6,
           .skill = 1, .fraglimit = 12, .lobbies = 1, .mode = 0, .rounds = 3,
           .ticks = 0, .team_size = 5, .team_fill = 1, .squad = 3};

// WHICH entity is "me" — the one identity that is allowed to differ per
// process: 0 in singleplayer, the own id on a networked client,
// -1 on a dedicated server (no branch gated on it may ever fire there).
// Every "is that MY shot / MY sound / MY kill" comparison reads this; a
// literal == 0 in such a place is a silent wrong-player effect the moment an
// id != 0 exists. INDEX semantics ("which slot") stay numeric and do NOT
// read this — the ent_trace exclude/hit values are the canonical example.
static int g_local_ent = 0;
// Networked-session flag (tentative definition; merges with the UI section's).
// The client/server set it; session_pauses() and the online tick read it.
static int g_online;

// The human roster. Human 0 always exists (the local player in SP, the first
// slot on a server); slots 1.. join and leave in networked sessions. Kept
// OUTSIDE match_t on purpose: a match_restart memsets match state, and the
// roster must survive it (P1-61's rescue, made structural).
static unsigned char g_humans_on_all[SRV_LOBBIES][MAX_HUMANS] =
    {[0 ... SRV_LOBBIES - 1] = {1}};
#define g_humans_on (g_humans_on_all[g_lobby])
static char g_human_name_all[SRV_LOBBIES][MAX_HUMANS][12];  // [0] mirrors g_cfg.name
#define g_human_name (g_human_name_all[g_lobby])
static int human_of_ent(int e) { return e == 0 ? 0 : e > MAX_BOTS ? e - MAX_BOTS : -1; }
static int ent_of_human(int h) { return h == 0 ? 0 : MAX_BOTS + h; }

// THE roster iterator: 0, 1..nbots, then the active high human
// slots. Every roster loop that used `e <= nbots` walks this instead — a loop
// that doesn't is a human who cannot be seen, hit or scored, silently.
static int g_match_nbots_fwd(void);   // match_t is defined later
static int ent_next(int e) {
  if (e < 0) {
    if (g_humans_on[0]) return 0;   // off on a dedicated server until a join
    e = 0;                          // fall through to the bot range
  }
  if (e < g_match_nbots_fwd()) return e + 1;
  for (int h = e > MAX_BOTS ? e - MAX_BOTS + 1 : 1; h < MAX_HUMANS; h++)
    if (g_humans_on[h]) return MAX_BOTS + h;
  return -1;
}
// Seeded from ent_next(-1), not from a literal 0: on a dedicated server human
// slot 0 is empty, and starting at 0 anyway put a phantom ALIVE entity into
// every snapshot, hitscan sweep and spawn-point test. In singleplayer
// g_humans_on[0] is 1, so the first id is still 0 and the walk is unchanged.
#define ENT_FOREACH(e) for (int e = ent_next(-1); e >= 0; e = ent_next(e))

// The two match sliders, ONE definition each. Their ranges used to be written
// out three times — config parse, MATCH tab, and the harness's `bots`/`fraglimit`
// commands — and the harness is the copy nobody would think to grep, so raising
// the frag cap took three coordinated edits or silently did not take at all.
// Same class the file already solved with MV_DEF/WP_DEF everywhere else.
// 1 bot / 10 frags, not 3 / 30: the opening match is a DUEL and it ends inside
// a few minutes. Three bots on a 48 m arena is a crowd — you die to someone you
// never saw, which reads as unfair rather than hard — and a 30-frag first match
// outlasts the point at which a new player has decided. Both are one slider
// away in the MATCH tab, and the difficulty default carries the challenge
// instead (see config_defaults' sp_diff).
static const param_def_t SP_BOTS_DEF = {"sp_bots",      "BOTS",        1, 0, MAX_BOTS, 1};
static const param_def_t SP_FRAG_DEF = {"sp_fraglimit", "FRAG LIMIT", 10, 1, 1000,     1};
// LAST MAN STANDING plays to rounds, not frags: a frag limit of 10 read as "ten
// rounds" is a forty-minute match. Its own slider, its own key.
static const param_def_t SP_ROUNDS_DEF = {"sp_rounds",  "ROUNDS TO WIN", 3, 1, 20,     1};

// GAME MODES. One enum, one name table, one wire value (NP_SCORES carries it as
// a byte, so the order is protocol: append, never reorder). Every rule a mode
// changes is asked through the mode_* helpers next to match_t, never by
// comparing g_match.mode at a call site — that is how the weapon gate, the
// respawn gate and the win condition stay in one place.
typedef enum { GM_DM, GM_GUN, GM_SNIPER, GM_BLADES, GM_LMS, GM_RESCUE, GM_TDM, GM_COUNT } game_mode_t;
static const char *const GM_NAMES[GM_COUNT] = {
  "DEATHMATCH", "GUN GAME", "SNIPERS ONLY", "KNIVES + NADES", "LAST MAN STANDING",
  "HOSTAGE RESCUE", "TEAM DEATHMATCH"};
static const char *const GM_KEYS[GM_COUNT] = {"dm", "gun", "sniper", "blades", "lms", "rescue", "tdm"};
// TEAM DEATHMATCH: two sides, ALPHA and BRAVO. A winner >= MAX_ENTS is a TEAM
// (MAX_ENTS + team), which is how EV_END, NP_SCORES and the banner carry it.
#define TEAM_SIZE_MAX 9
// HOSTAGE RESCUE's squad: one to three operators, humans first, bots the rest.
#define RESC_SQUAD_MAX 3
static const char *const TEAM_NAMES[2] = {"ALPHA", "BRAVO"};
static int won_by_me(int winner);   // with the team state, by match_t
// GUN GAME's ladder: GG_RUNG_KILLS kills with each weapon promote you to the
// next, and one kill with the last (the knife) wins — nine kills, about a
// deathmatch's ten. At one kill a rung a six-bot match was over in fifteen
// seconds. The grenade rung never runs dry (mode_nade_refill) and a KNIFE kill
// knocks the victim back a whole rung.
static const int GG_LADDER[] = {WPN_AR, WPN_PT, WPN_SR, WPN_GR, WPN_KN};
#define GG_LEVELS ((int)(sizeof GG_LADDER / sizeof GG_LADDER[0]))
#define GG_RUNG_KILLS 2
static int map_parse(const char *s);        // the MAP setting; both live with
static const char *map_pick_key(int pick);  // MAP_DEFS, by map_generate
static int gm_parse(const char *s) {
  for (int i = 0; i < GM_COUNT; i++) if (!strcmp(s, GM_KEYS[i])) return i;
  return -1;
}

