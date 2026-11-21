// code/core/config.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Config — plain "key value" lines in config.cfg next to the executable.
// Created with defaults on first start; unknown keys are ignored with a note.
// ---------------------------------------------------------------------------

// Grouped by what the hand does: movement (leaning is a movement, not a weapon
// mode), then the weapon, then the meta keys. The KEYS tab lists them in this
// order, so the enum IS the menu layout.
// THE ARENA (0.8.9) is the handmade TRAINING ZONE, MAP_DEFS[MAP_ARENA_ID]. The
// procedural arena it replaced is still map 0 ("proc"), hidden from the
// menus and the rotation: the proof battery was measured on it and runs on it.
#define MAP_ARENA_ID 8
typedef enum {
  IN_FWD, IN_BACK, IN_LEFT, IN_RIGHT, IN_JUMP, IN_CROUCH, IN_LEAN_L, IN_LEAN_R,
  IN_FIRE, IN_ADS, IN_RELOAD, IN_ALTFIRE, IN_SWAP, IN_SWAP_PREV, IN_SLOT1, IN_SLOT2, IN_SLOT3, IN_SLOT4, IN_SLOT5, IN_SLOT6, IN_SLOT7,
  IN_SCORE, IN_VIEW, IN_INSPECT,
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
  // CS2's secondary fire, on a key of its own because AIM is this game's right
  // button: the FAMAS's and the Glock's three-round burst, the R8's fanned
  // hammer. A toggle; the HUD shows the mode beside the gun's name.
  [IN_ALTFIRE] = {"bind_altfire",    "b",      "FIRE MODE",        "altfire",    "none"},
  // NEXT and PREVIOUS through the selection list (sim/weapon.c sel_step) — the
  // mouse wheel is these two: wheel down next, wheel up previous, as in CS2.
  [IN_SWAP]    = {"bind_swap",       "x",      "NEXT WEAPON",      "swap",       "pad_y"},
  [IN_SWAP_PREV] = {"bind_swap_prev", "z",     "PREV WEAPON",      "swapprev",   "none"},
  [IN_SLOT1]   = {"bind_slot1",      "1",      "WEAPON 1 AR",      "slot1",      "pad_left"},
  [IN_SLOT2]   = {"bind_slot2",      "2",      "WEAPON 2 SNIPER",  "slot2",      "pad_right"},
  [IN_SLOT3]   = {"bind_slot3",      "3",      "WEAPON 3 PISTOL",  "slot3",      "none"},
  [IN_SLOT4]   = {"bind_slot4",      "4",      "WEAPON 4 KNIFE",   "slot4",      "none"},
  // ONE KEY PER GRENADE, the way CS2's own grenade keys go to one grenade: 5 the
  // frag, 6 the smoke, 7 the incendiary. A key for a kind you have none of draws
  // whichever grenade you do have, so 5 is still "a grenade" in a pinch.
  [IN_SLOT5]   = {"bind_slot5",      "5",      "WEAPON 5 FRAG",    "slot5",      "none"},
  [IN_SLOT6]   = {"bind_slot6",      "6",      "WEAPON 6 SMOKE",   "slot6",      "none"},
  [IN_SLOT7]   = {"bind_slot7",      "7",      "WEAPON 7 FIRE",    "slot7",      "none"},
  [IN_SCORE]   = {"bind_score",      "tab",    "SCOREBOARD",       "score",      "pad_select"},
  [IN_VIEW]    = {"bind_view",       "v",      "VIEW 1ST/3RD",     "view",       "pad_down"},
  // Presentation only: the viewmodel turns the knife or pistol over for a look.
  [IN_INSPECT] = {"bind_inspect",    "f",      "INSPECT WEAPON",   "inspect",    "pad_up"},
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
// The CS2 guns with their own sampled reports (beyond the AK, FAMAS, Glock and
// Deagle, which have theirs above). Order is the enum's: append only.
#define GUN_TAKE_LIST(X) \
  X(M4A1S, "m4a1s") X(GALIL, "galil") X(SG553, "sg553") X(AUG, "aug") \
  X(MAC10, "mac10") X(MP9, "mp9") X(MP7, "mp7") X(MP5SD, "mp5sd") X(UMP45, "ump45") \
  X(P90, "p90") X(BIZON, "bizon") X(USPS, "usps") X(P250, "p250") \
  X(FIVESEVEN, "fiveseven") X(TEC9, "tec9") X(CZ75, "cz75") X(R8, "r8") \
  X(SSG08, "ssg08") X(G3SG1, "g3sg1") X(SCAR20, "scar20") \
  X(NOVA, "nova") X(XM1014, "xm1014") X(SAWEDOFF, "sawedoff") X(MAG7, "mag7") \
  X(M249, "m249") X(NEGEV, "negev") X(ELITES, "elites")
// Every gun with its own reload: the shipped three and the loadout four, then
// the CS2 arsenal. (slot row, variant) for each is RL_GUN_VAR in routing.c.
#define RL_GUN_LIST(X) X(M4A4, "m4a4") X(AK, "ak") X(FAMAS, "famas") X(P2000, "p2000") \
  X(GLOCK, "glock") X(DEAGLE, "deagle") X(AWP, "awp") GUN_TAKE_LIST(X)
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
  SFX_SMOKE_POP, SFX_SMOKE_HISS,     // a smoke canister popping, and its gas
  SFX_FLAME_POP, SFX_FLAME_BURN,     // an incendiary catching, and the fire's crackle
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
  // THE CS2 ARSENAL'S OWN REPORTS, three takes per gun (GUN_TAKE_LIST; the pack
  // names are fire_<name>1..3, generated by tools/gen_sfx.py). Sampled-only
  // like the AK's: a gun whose takes are not in the pack borrows its stand-in's
  // (see VAR_SOUND in code/audio/routing.c), re-pitched.
  #define GUN_TAKE_ENUM(id, name) SFX_T_##id, SFX_T_##id##_2, SFX_T_##id##_3,
  GUN_TAKE_LIST(GUN_TAKE_ENUM)
  #undef GUN_TAKE_ENUM
  // 0.8.3 — EVERY GUN ITS OWN RELOAD. Built in code/audio/samples.c from the
  // gun's own one-shots (rl_<name>_out / _in / _bolt or _slide, generated by
  // tools/gen_sfx.py) on its slot's beats; a gun with none of its own clips in
  // the pack keeps the slot's reload. RLT_ is the AR slot's tactical reload.
  #define RL_GUN_ENUM(id, name) SFX_RL_##id, SFX_RLT_##id,
  RL_GUN_LIST(RL_GUN_ENUM)
  #undef RL_GUN_ENUM
  // FOOTSTEPS BY WHAT IS UNDERFOOT (routing.c audio_step_surface): sampled-only,
  // a surface with no takes in the pack falls back to the synth SFX_STEP.
  SFX_STEP_CONC1, SFX_STEP_CONC2, SFX_STEP_CONC3, SFX_STEP_CONC4,
  SFX_STEP_METAL1, SFX_STEP_METAL2, SFX_STEP_METAL3,
  SFX_STEP_SAND1, SFX_STEP_SAND2, SFX_STEP_SAND3,
  SFX_STEP_WOOD1, SFX_STEP_WOOD2,
  SFX_RL_SHELL,                      // a shotgun shell thumbed into the tube
  // A MAP'S OWN AIR (routing.c audio_ambience): one long loop per map, MAP_DEFS order.
  SFX_AMB_ARENA, SFX_AMB_FOUNDRY, SFX_AMB_BAZAAR, SFX_AMB_OUTPOST,
  SFX_AMB_CITADEL, SFX_AMB_HARBOR, SFX_AMB_REFINERY, SFX_AMB_COMPOUND,
  SFX_COUNT
};
#define SFX_RL_FIRST SFX_RL_M4A4   // the first id after the CS2 takes

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
  [WPN_PT] = {"PISTOL", "pt", 13,  34, 2, 118,  96, 0, 0.32f, SFX_FIRE_PT,   // the P2000: 13 rounds
              SFX_RELOAD_PT, 0.050f, 370.0f},
  // One "round" that is never spent, so a reload can never start and the empty
  // checks every weapon path shares stay true without a knife exception.
  // dmg is unused: each swing in MELEE carries its own.
  [WPN_KN] = {"KNIFE",  "kn",  1,   0, 2,   1,   1, 1, 0.00f, SFX_KNIFE_SWING,
              SFX_KNIFE_SWING, 0.030f, 0.0f},
  // `mag` is the grenades you spawn with. They never reload.
  [WPN_GR] = {"GRENADE", "gr", 3,   0, 1,   1,   1, 0, 0.00f, SFX_NADE_PIN,
              SFX_NADE_PIN, 0.030f, 0.0f},
};

// THE LOADOUT VARIANTS (see WPN_VARS): the Counter-Strike 2 arsenal, every gun a
// real one. One row per slot that has them — [0] the primary (rifles AND the
// SMGs: CS2's primary slot holds both), [1] the pistol, [2] the sniper — and per
// variant; cell 0 of each row is never read (variant 0 IS WPN_DEF and
// WP_DEFAULTS, untouched). Structural numbers replace the slot's; `feel`
// MULTIPLIES the slot's live tuning (WP_* order), so a developer dragging the
// AR's kick still moves every primary together and each keeps its own character
// relative to the others. The reload timeline stays the slot's: the synthesized
// reload sounds and the animation's beats are cut to it.
//
// THE NUMBERS ARE CS2's, ON THIS GAME'S SCALE. Damage is CS2's base damage times
// the slot's own ratio (variant 0 against its CS2 namesake: the M4A4's 33 is
// 26 here, the P2000's 35 is 34), so the guns keep CS2's ORDER and SPACING —
// the M4A1-S out-hits the M4A4, the UMP-45 out-hits every SMG, the P250 every
// 9 mm. Fire rate is CS2's RPM over the slot's (750 primary, 353 pistol). The
// guns that were here first (AK, FAMAS, Deagle, SCAR-20) keep their tuned rows.
//   primary   0 M4A4  1 AK-47  2 FAMAS  3 M4A1-S  4 GALIL AR  5 SG 553  6 AUG
//             7 MAC-10  8 MP9  9 MP7  10 MP5-SD  11 UMP-45  12 P90  13 PP-BIZON
//             14 NOVA  15 XM1014  16 SAWED-OFF  17 MAG-7  18 M249  19 NEGEV
//   pistol    0 P2000  1 GLOCK-18  2 DESERT EAGLE  3 USP-S  4 P250  5 FIVE-SEVEN
//             6 TEC-9  7 CZ75-AUTO  8 R8 REVOLVER  9 DUAL BERETTAS
//   sniper    0 AWP  1 SCAR-20  2 SSG 08  3 G3SG1
// `supp` marks a suppressed gun (M4A1-S, USP-S, MP5-SD): no muzzle star, a
// quieter, duller report, and the round still the gun's.
#define RV_N 20
#define PV_N 10
#define SV_N 4
#define WPN_VARS RV_N          // the widest row: the table's second dimension
#define SR_VARS  SV_N
#define SV_SCAR  1    // the sniper slot's SCAR-20 (semi-automatic: no bolt to work)
#define SV_G3    3    // ...and the G3SG1, its semi-automatic twin
typedef struct {
  int   mag, dmg, full_auto;
  float hip_spread, v_round;
  float feel[WP_COUNT];   // RPM KICK_UP KICK_SIDE RECOVER SPREAD BLOOM ADS ZOOM PUNCH SWAY
  float pitch, level;     // the report's playback rate and level against the slot's
  int   supp;             // suppressed
  // THE HEAVY GUNS' AND THE ODD ONES' MECHANICS (0 everywhere else):
  int   pellets;          // a shotgun's pellets per shell (dmg is PER PELLET)
  float pel_spread;       // ...the cone they leave in, degrees
  int   shell_t;          // ticks (120 Hz) per shell: a TUBE loads one at a time (0 = a magazine)
  int   pump;             // the fore-end is worked after every shot (mesh group 3)
  int   alt;              // FIRE MODE: 1 a three-round burst, 2 the R8's hammer
  int   dual;             // two guns, fired in turn (the Dual Berettas)
} wpn_var_t;
#define ALT_BURST 1
#define ALT_R8    2
// Rows: [0] the primary, [1] the pistol, [2] the sniper (see wpn_var_row).
static const wpn_var_t WPN_VAR[3][WPN_VARS] = {
  { {0},
    // AK-47
    {30, 33, 1, 0.15f, 715.0f, {0.80f, 1.45f, 1.50f, 1.10f, 1.30f, 1.30f, 1.10f, 1.0f, 1.35f, 1.1f},
     0.86f, 1.05f, 0, 0, 0, 0, 0, 0, 0},
    // FAMAS
    {25, 23, 1, 0.12f, 960.0f, {1.33f, 0.85f, 0.80f, 0.90f, 0.90f, 1.10f, 1.00f, 1.0f, 0.85f, 1.0f},
     1.12f, 0.95f, 0, 0, 0, 0, 0, ALT_BURST, 0},
    // M4A1-S: 20 rounds that hit harder than the A4's, slower, flatter, silenced
    {20, 30, 1, 0.11f, 880.0f, {0.80f, 0.85f, 0.80f, 1.00f, 0.80f, 0.90f, 1.00f, 1.0f, 0.75f, 1.0f},
     1.25f, 0.42f, 1, 0, 0, 0, 0, 0, 0},
    // GALIL AR: 35 rounds, cheap and loose
    {35, 24, 1, 0.14f, 915.0f, {0.89f, 1.10f, 1.25f, 1.00f, 1.20f, 1.20f, 1.05f, 1.0f, 1.05f, 1.05f},
     1.06f, 1.00f, 0, 0, 0, 0, 0, 0, 0},
    // SG 553: the scoped 5.56, slower and tighter in the glass, armour-piercing
    {30, 25, 1, 0.16f, 940.0f, {0.73f, 1.05f, 0.90f, 1.10f, 0.70f, 1.00f, 1.25f, 0.70f, 1.00f, 1.0f},
     0.96f, 1.02f, 0, 0, 0, 0, 0, 0, 0},
    // AUG: the scoped bullpup, lighter round, the steadier of the two
    {30, 22, 1, 0.15f, 970.0f, {0.80f, 0.90f, 0.80f, 1.00f, 0.65f, 0.95f, 1.25f, 0.72f, 0.90f, 1.0f},
     1.04f, 0.96f, 0, 0, 0, 0, 0, 0, 0},
    // MAC-10: a hose. 800 rpm, wild sideways, the best on the move
    {30, 23, 1, 0.07f, 280.0f, {1.07f, 0.85f, 1.45f, 0.80f, 1.45f, 1.30f, 0.80f, 1.05f, 0.70f, 0.9f},
     1.20f, 0.88f, 0, 0, 0, 0, 0, 0, 0},
    // MP9: 857 rpm, light rounds, very steady from the hip
    {30, 21, 1, 0.06f, 380.0f, {1.14f, 0.80f, 1.20f, 0.85f, 1.25f, 1.20f, 0.80f, 1.05f, 0.65f, 0.9f},
     1.24f, 0.86f, 0, 0, 0, 0, 0, 0, 0},
    // MP7: the 4.6 mm PDW, the most rifle-like of the SMGs
    {30, 24, 1, 0.07f, 735.0f, {1.00f, 0.80f, 1.00f, 0.90f, 1.10f, 1.10f, 0.85f, 1.05f, 0.70f, 0.95f},
     1.16f, 0.88f, 0, 0, 0, 0, 0, 0, 0},
    // MP5-SD: integrally suppressed, the softest kick of any gun
    {30, 22, 1, 0.07f, 300.0f, {1.00f, 0.70f, 0.90f, 0.90f, 1.05f, 1.00f, 0.85f, 1.05f, 0.55f, 0.95f},
     1.30f, 0.36f, 1, 0, 0, 0, 0, 0, 0},
    // UMP-45: slow heavy .45, the hardest-hitting SMG
    {25, 28, 1, 0.08f, 285.0f, {0.89f, 1.00f, 1.10f, 1.00f, 1.25f, 1.20f, 0.90f, 1.05f, 0.95f, 1.0f},
     0.92f, 0.95f, 0, 0, 0, 0, 0, 0, 0},
    // P90: 50 rounds on top, 857 rpm
    {50, 21, 1, 0.07f, 715.0f, {1.14f, 0.70f, 1.10f, 0.90f, 1.25f, 1.10f, 0.90f, 1.05f, 0.65f, 1.0f},
     1.20f, 0.88f, 0, 0, 0, 0, 0, 0, 0},
    // PP-BIZON: 64 rounds in the helical drum, weak and sloppy
    {64, 21, 1, 0.08f, 330.0f, {1.00f, 0.65f, 1.05f, 0.85f, 1.30f, 1.15f, 0.85f, 1.05f, 0.60f, 0.95f},
     1.10f, 0.86f, 0, 0, 0, 0, 0, 0, 0},
    // THE SHOTGUNS: damage is per pellet (CS2's x 0.79), rpm CS2's over 750, and
    // the pellets lose most of it past ~10 m (PELLET_FALL in ballistics.c).
    // NOVA: pump, 8 in the tube, 9 pellets
    { 8, 21, 0, 0.30f, 400.0f, {0.09f, 2.40f, 0.80f, 1.80f, 1.20f, 0.20f, 1.10f, 1.0f, 2.20f, 1.1f},
     0.80f, 1.20f, 0, 9, 3.6f, 60, 1, 0, 0},
    // XM1014: semi-automatic, 7 in the tube, 6 pellets, as fast as you pull
    { 7, 16, 1, 0.30f, 400.0f, {0.23f, 1.90f, 0.90f, 1.30f, 1.20f, 0.30f, 1.10f, 1.0f, 1.80f, 1.1f},
     0.84f, 1.15f, 0, 6, 3.9f, 54, 0, 0, 0},
    // SAWED-OFF: pump, 7 shells, 8 pellets in a wide cone, brutal up close
    { 7, 25, 0, 0.36f, 380.0f, {0.095f, 2.60f, 1.00f, 1.80f, 1.40f, 0.20f, 1.00f, 1.0f, 2.40f, 1.1f},
     0.76f, 1.25f, 0, 8, 5.2f, 60, 1, 0, 0},
    // MAG-7: pump, a five-shell box in the grip, 8 pellets, a tight cone
    { 5, 24, 0, 0.30f, 400.0f, {0.095f, 2.50f, 0.80f, 1.80f, 1.10f, 0.20f, 1.10f, 1.0f, 2.30f, 1.1f},
     0.82f, 1.18f, 0, 8, 3.0f, 0, 1, 0, 0},
    // M249: a hundred rounds of 5.56 off the belt, heavy and steady
    {100, 25, 1, 0.20f, 915.0f, {1.00f, 0.90f, 1.30f, 1.20f, 1.40f, 1.10f, 1.40f, 1.0f, 1.00f, 1.3f},
     0.94f, 1.10f, 0, 0, 0, 0, 0, 0, 0},
    // NEGEV: 150 rounds at 800 rpm, wild for the first burst, then a laser
    {150, 28, 1, 0.22f, 915.0f, {1.07f, 0.95f, 1.40f, 1.20f, 1.50f, 0.70f, 1.40f, 1.0f, 1.05f, 1.3f},
     0.90f, 1.12f, 0, 0, 0, 0, 0, 0, 0} },
  { {0},
    // GLOCK-18: CS2's 20 rounds, semi-automatic, quick and light
    {20, 29, 0, 0.30f, 375.0f, {1.13f, 0.80f, 0.90f, 0.90f, 1.05f, 0.90f, 0.95f, 1.0f, 0.75f, 1.0f},
     1.15f, 0.90f, 0, 0, 0, 0, 0, ALT_BURST, 0},
    // DESERT EAGLE
    { 7, 58, 0, 0.45f, 470.0f, {0.62f, 2.30f, 1.50f, 1.60f, 1.10f, 2.20f, 1.20f, 1.0f, 2.00f, 1.2f},
     0.72f, 1.15f, 0, 0, 0, 0, 0, 0, 0},
    // USP-S: 12 rounds, silenced, the most accurate sidearm
    {12, 34, 0, 0.26f, 290.0f, {1.00f, 0.85f, 0.85f, 1.00f, 0.85f, 0.90f, 1.00f, 1.0f, 0.70f, 1.0f},
     1.30f, 0.40f, 1, 0, 0, 0, 0, 0, 0},
    // P250: 13 rounds of hard-hitting 9 mm
    {13, 37, 0, 0.34f, 360.0f, {1.13f, 1.10f, 1.05f, 1.05f, 1.05f, 1.10f, 1.00f, 1.0f, 1.05f, 1.0f},
     1.02f, 1.00f, 0, 0, 0, 0, 0, 0, 0},
    // FIVE-SEVEN: 20 rounds of 5.7 mm, armour-piercing and flat
    {20, 31, 0, 0.30f, 650.0f, {1.13f, 0.95f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.0f, 0.95f, 1.0f},
     1.18f, 0.92f, 0, 0, 0, 0, 0, 0, 0},
    // TEC-9: 18 rounds as fast as you can pull, loose
    {18, 32, 0, 0.42f, 360.0f, {1.42f, 1.15f, 1.30f, 0.95f, 1.35f, 1.30f, 0.95f, 1.0f, 1.00f, 1.05f},
     1.08f, 1.00f, 0, 0, 0, 0, 0, 0, 0},
    // CZ75-AUTO: twelve rounds, full-auto, gone in a second
    {12, 30, 1, 0.38f, 370.0f, {1.70f, 0.60f, 1.30f, 0.80f, 1.30f, 0.80f, 0.95f, 1.0f, 0.60f, 1.0f},
     1.12f, 0.92f, 0, 0, 0, 0, 0, 0, 0},
    // R8 REVOLVER: eight .44s, slow, one to the body is nearly a kill
    { 8, 84, 0, 0.40f, 450.0f, {0.34f, 2.60f, 1.40f, 1.80f, 0.90f, 2.60f, 1.25f, 1.0f, 2.40f, 1.2f},
     0.78f, 1.12f, 0, 0, 0, 0, 0, ALT_R8, 0},
    // DUAL BERETTAS: two 92s, thirty rounds, fired left and right in turn
    {30, 37, 0, 0.40f, 380.0f, {1.42f, 0.90f, 1.20f, 1.00f, 1.30f, 1.10f, 1.10f, 1.0f, 0.85f, 1.1f},
     1.10f, 0.95f, 0, 0, 0, 0, 0, 0, 1},
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0} },
  // The SR's rpm 1000 is "as fast as you can click"; x0.24 is the SCAR-20's
  // 240 rpm, so a held-down clicker cannot turn it into a battle rifle.
  { {0},
    // SCAR-20
    {20, 80, 0, 0.45f, 790.0f, {0.24f, 0.45f, 1.20f, 0.55f, 1.30f, 2.50f, 0.90f, 1.0f, 0.55f, 1.0f},
     1.10f, 0.90f, 0, 0, 0, 0, 0, 0, 0},
    // SSG 08: the Scout. Ten rounds, light, quick to the eye, a head kill and
    // never a body one
    {10, 90, 0, 0.40f, 870.0f, {1.00f, 0.60f, 0.80f, 0.70f, 1.00f, 0.80f, 0.75f, 1.0f, 0.65f, 0.9f},
     1.15f, 0.85f, 0, 0, 0, 0, 0, 0, 0},
    // G3SG1: the SCAR-20's twin, a hair heavier
    {20, 80, 0, 0.45f, 800.0f, {0.24f, 0.50f, 1.25f, 0.58f, 1.30f, 2.50f, 0.95f, 1.0f, 0.60f, 1.0f},
     1.02f, 0.95f, 0, 0, 0, 0, 0, 0, 0},
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0} },
};
// Which WPN_VAR row slot `id` reads, -1 for a slot without variants...
static int wpn_var_row(int id) {
  return id == WPN_AR ? 0 : id == WPN_PT ? 1 : id == WPN_SR ? 2 : -1;
}
// ...and how many variants it has (1 = only the shipped weapon).
static int wpn_var_n(int id) {
  return id == WPN_AR ? RV_N : id == WPN_PT ? PV_N : id == WPN_SR ? SV_N : 1;
}
// The semi-automatic snipers: a charging handle, no bolt to work.
static int sr_semi(int var) { return var == SV_SCAR || var == SV_G3; }

// THE GRENADE. A fragmentation grenade on a 3.5 s fuse that starts as the pin comes
// out: hold FIRE to pull it and cook, release to throw, and hold AIM as you release
// to lob it underhand. Hold it past the fuse and it goes off in your hand.
#define NADE_FUSE      448      // ticks from the press: NADE_PIN_T, then 3.33 s
// The pin takes this long; a tap still pulls it. 0.4 s, not the 0.17 s it was:
// at 20 ticks the off hand had to reach, hook and draw in under two frames of a
// 60 Hz screen, which no hand does and which read as the pin vanishing. The
// fuse grew by the same 28 ticks, so spoon-to-blast is unchanged.
#define NADE_PIN_T     48
// THE SMOKE GRENADE (sim/smoke.c holds the cloud). Three carried per life
// (more from a supply crate). It has no cook: the pin and the throw are the frag's, but it pops only
// once it comes to REST (or SMOKE_FLIGHT_MAX after the throw, wherever it is),
// never sooner than SMOKE_ARM_T.
#define SMOKE_CARRY      3
#define SMOKE_FLIGHT_MAX (6 * TICK_HZ)
#define SMOKE_ARM_T      (TICK_HZ * 3 / 4)
// THE INCENDIARY (sim/fire.c holds the fire). Three per life, no cook, like
// the smoke. It goes off the moment it strikes a FLOOR (anything facing up) after a
// short arming time, and lays a pool of fire that spreads over the ground. One
// still in the air INC_FLIGHT_MAX after the throw bursts there and is wasted —
// CS2's rule, so it cannot be air-burst onto a head.
#define INC_CARRY        3
#define INC_FLIGHT_MAX   (2 * TICK_HZ)
#define INC_ARM_T        (TICK_HZ / 8)
// The three kinds the one grenade slot holds (weapon_t.gr_kind).
#define NADE_FRAG  0
#define NADE_SMOKE 1
#define NADE_INC   2
#define NADE_KINDS 3
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
static uint16_t g_human_loadout_all[SRV_LOBBIES][MAX_HUMANS];
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
  int teams, wrule;           // --teams 0|1, --weapons all|snipers|blades
  char ladder[80];            // --ladder ar1,pt0,...: GUN GAME's (gg_parse)
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
} g_srv = {.port = NET_PORT_DEF, .map = MAP_ARENA_ID, .cap_bots = 2, .cap_public = 6,
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

// GAME MODES (0.8.6: eight became four). One enum, one name table, one wire
// value (NP_SCORES carries it as a byte, so the order is protocol). Two match
// RULES ride beside it instead of being modes of their own: TEAMS (ALPHA vs
// BRAVO, in DEATHMATCH and ELIMINATION) and the WEAPONS rule (all, snipers
// only, knives + nades; same two modes). The old keys still parse
// (gm_parse_legacy): tdm, tlms, sniper, blades, lms. Every rule a mode changes
// is asked through the mode_* helpers in sim/modes.c, never by comparing
// g_match.mode at a call site.
typedef enum { GM_DM, GM_GUN, GM_LMS, GM_RESCUE, GM_COUNT } game_mode_t;
static const char *const GM_NAMES[GM_COUNT] = {
  "DEATHMATCH", "GUN GAME", "ELIMINATION", "HOSTAGE RESCUE"};
static const char *const GM_KEYS[GM_COUNT] = {"dm", "gun", "elim", "rescue"};
// The modes TEAMS and WEAPONS apply to.
static int gm_has_rules(int m) { return m == GM_DM || m == GM_LMS; }
static int gm_rounds(int m) { return m == GM_LMS; }   // ELIMINATION plays to rounds
// THE WEAPONS RULE (0.8.9): ALL, or ONE GUN + NADES. A pick is any single gun of
// the Armory (a slot and its variant, the GUN GAME rung code) plus the grenades,
// with the knife always at the belt: "AK-47 + NADES", "AWP + NADES" (the old
// SNIPERS ONLY), "KNIFE + NADES" (the old KNIVES + NADES, which keeps its frag
// rules). Stored as 0 = ALL, else 1 + the rung code (one byte on the wire); the
// key is "all" or the rung's ladder key ("ar1", "sr0", "kn0").
#define WR_ALL 0
#define WR_MAKE(slot, var) (1 + GG_RUNG(slot, var))
#define WR_SNIPERS WR_MAKE(WPN_SR, 0)
#define WR_BLADES  WR_MAKE(WPN_KN, 0)
// TEAMS: two sides, ALPHA and BRAVO. A winner >= MAX_ENTS is a TEAM
// (MAX_ENTS + team), which is how EV_END, NP_SCORES and the banner carry it;
// in a team ELIMINATION a side's round is carried the same way in round_win.
#define TEAM_SIZE_MAX 9
// HOSTAGE RESCUE's squad: one to three operators, humans first, bots the rest.
#define RESC_SQUAD_MAX 3
static const char *const TEAM_NAMES[2] = {"ALPHA", "BRAVO"};
static int won_by_me(int winner);   // with the team state, by match_t
// GUN GAME's ladder: GG_RUNG_KILLS kills with each weapon promote you to the
// next, and one kill with the last wins. The grenade rung never runs dry
// (mode_nade_refill) and a KNIFE kill knocks the victim back a whole rung.
// THE LADDER IS THE PLAYER'S (0.8.6): up to GG_MAX rungs, each one gun — a
// slot and its variant, so "AK-47" and "M4A4" are different rungs. Offline it
// is the MATCH page's; online the server's (the host's, from --ladder), sent
// in NP_SCORES. A rung is one byte: slot << 5 | variant.
#define GG_MAX 10
#define GG_RUNG(slot, var) ((slot) << 5 | (var))
#define GG_SLOT(r) ((r) >> 5 & 7)
#define GG_VAR(r)  ((r) & 31)
static const uint8_t GG_DEFAULT[] = {GG_RUNG(WPN_AR, 0), GG_RUNG(WPN_PT, 0), GG_RUNG(WPN_SR, 0),
                                     GG_RUNG(WPN_GR, 0), GG_RUNG(WPN_KN, 0)};
#define GG_DEFAULT_N ((int)sizeof GG_DEFAULT)
static int gg_rung_valid(int r) {
  int s = GG_SLOT(r);
  return r >= 0 && r < 256 && s < WPN_COUNT && GG_VAR(r) < wpn_var_n(s);
}
// The ladder as text, for the config and the server's command line:
// "ar1,pt0,sr0,gr0,kn0" (slot key, variant). Unknown or bad entries are
// skipped; an empty result is the caller's to replace with GG_DEFAULT.
static int gg_parse(const char *s, uint8_t out[GG_MAX]) {
  static const char *const SK[WPN_COUNT] = {"ar", "sr", "pt", "kn", "gr"};
  int n = 0;
  while (*s && n < GG_MAX) {
    while (*s == ',' || *s == ' ') s++;
    if (!*s) break;
    for (int k = 0; k < WPN_COUNT; k++)
      if (s[0] == SK[k][0] && s[1] == SK[k][1]) {
        int v = (s[2] >= '0' && s[2] <= '9') ? atoi(s + 2) : 0;
        if (gg_rung_valid(GG_RUNG(k, v))) out[n++] = (uint8_t)GG_RUNG(k, v);
        break;
      }
    while (*s && *s != ',') s++;
  }
  return n;
}
static void gg_format(const uint8_t *r, int n, char *buf, size_t cap) {
  static const char *const SK[WPN_COUNT] = {"ar", "sr", "pt", "kn", "gr"};
  size_t at = 0;
  buf[0] = '\0';
  for (int i = 0; i < n && at + 8 < cap; i++)
    at += (size_t)snprintf(buf + at, cap - at, "%s%s%d", i ? "," : "", SK[GG_SLOT(r[i]) % WPN_COUNT],
                           GG_VAR(r[i]));
}
#define GG_RUNG_KILLS 2
// THE WEAPONS RULE's helpers (the encoding is above, by WR_ALL).
static int wr_valid(int w) {
  return w == WR_ALL || (w > 0 && w <= 256 && gg_rung_valid(w - 1) && GG_SLOT(w - 1) != WPN_GR);
}
static int wr_slot(int w) { return GG_SLOT(w - 1); }
static int wr_var(int w) { return GG_VAR(w - 1); }
// "all", a ladder key ("ar1"), or the old keys "snipers" / "blades". -1 if bad.
static int wr_parse(const char *s) {
  if (!strcmp(s, "all")) return WR_ALL;
  if (!strcmp(s, "snipers")) return WR_SNIPERS;
  if (!strcmp(s, "blades")) return WR_BLADES;
  uint8_t r[GG_MAX];
  if (strchr(s, ',') || gg_parse(s, r) != 1) return -1;
  int w = 1 + r[0];
  return wr_valid(w) ? w : -1;
}
static const char *wr_key(int w, char *buf, size_t cap) {
  if (!wr_valid(w) || w == WR_ALL) { snprintf(buf, cap, "all"); return buf; }
  uint8_t r = (uint8_t)(w - 1);
  gg_format(&r, 1, buf, cap);
  return buf;
}
static int map_parse(const char *s);        // the MAP setting; both live with
static const char *map_pick_key(int pick);  // MAP_DEFS, by map_generate
static int gm_parse(const char *s) {
  for (int i = 0; i < GM_COUNT; i++) if (!strcmp(s, GM_KEYS[i])) return i;
  return -1;
}
// A mode key, old or new, with the rules an old key implied (teams, weapons;
// either may be NULL). -1 for an unknown key.
static int gm_parse_legacy(const char *s, int *teams, int *wrule) {
  int m = gm_parse(s), t = -1, w = -1;
  if (m < 0) {
    if (!strcmp(s, "lms"))         m = GM_LMS;
    else if (!strcmp(s, "tdm"))    { m = GM_DM;  t = 1; }
    else if (!strcmp(s, "tlms"))   { m = GM_LMS; t = 1; }
    else if (!strcmp(s, "sniper")) { m = GM_DM;  t = 0; w = WR_SNIPERS; }
    else if (!strcmp(s, "blades")) { m = GM_DM;  t = 0; w = WR_BLADES; }
  }
  if (m >= 0 && t >= 0 && teams) *teams = t;
  if (m >= 0 && w >= 0 && wrule) *wrule = w;
  return m;
}

