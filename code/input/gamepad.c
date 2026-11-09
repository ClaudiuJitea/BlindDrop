// code/input/gamepad.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Gamepad — shared vocabulary and processing. The platform backends (evdev on
// Linux — Steam Input presents a virtual X360 pad under gamescope — and
// XInput + DirectInput8 on Windows) only FILL g_pad; everything that
// interprets it (deadzone,
// response curve, look rates, button edges, menu navigation) lives here in the
// shared core, for the same reason the frame loop does: a second backend is
// exactly how a deadzone ends up existing twice with one copy subtly wrong.
// ---------------------------------------------------------------------------

// Developer menu gate, the HARD half: 0 removes the DEV tab (movement/weapon
// tuning) from the build entirely, whatever the config says. Overridable from
// the build line (-DDEV_MENU=0) so a release build is a flag, not a source
// edit. The SOFT half is `g_cfg.devmode` (config key `devmode`, harness `dev
// on|off`), default 0 — the tab is opt-in per install and needs no rebuild.
// Either way the tuning config keys keep parsing, so a tuned config stays
// valid on a binary that cannot show the tab.
#ifndef DEV_MENU
#define DEV_MENU 1
#endif

typedef enum {
  PB_A, PB_B, PB_X, PB_Y, PB_LB, PB_RB, PB_LT, PB_RT,
  PB_SELECT, PB_START, PB_L3, PB_R3, PB_UP, PB_DOWN, PB_LEFT, PB_RIGHT,
  // Back paddles / extra buttons (8BitDo Ultimate family and friends).
  // Appended, never inserted: the values extend the KEY_PAD0 key space and
  // an insertion would silently renumber every existing pad bind.
  PB_P1, PB_P2, PB_P3, PB_P4,
  PB_COUNT
} pad_btn_t;

// tok = config vocabulary, label = what the menu prints (Deck naming for the
// two middle buttons). START is deliberately not game-bindable: pad_sync owns
// it as the menu key and the rebind capture refuses it, so no config can take
// "open the menu" away from a controller-only session.
typedef struct { const char *tok, *label; } pad_btn_def_t;
static const pad_btn_def_t PAD_BTN_DEF[PB_COUNT] = {
  [PB_A]      = {"pad_a",      "A"},        [PB_B]     = {"pad_b",     "B"},
  [PB_X]      = {"pad_x",      "X"},        [PB_Y]     = {"pad_y",     "Y"},
  [PB_LB]     = {"pad_lb",     "LB"},       [PB_RB]    = {"pad_rb",    "RB"},
  [PB_LT]     = {"pad_lt",     "LT"},       [PB_RT]    = {"pad_rt",    "RT"},
  [PB_SELECT] = {"pad_select", "VIEW BTN"}, [PB_START] = {"pad_start", "MENU BTN"},
  [PB_L3]     = {"pad_l3",     "L3"},       [PB_R3]    = {"pad_r3",    "R3"},
  [PB_UP]     = {"pad_up",     "DPAD UP"},  [PB_DOWN]  = {"pad_down",  "DPAD DN"},
  [PB_LEFT]   = {"pad_left",   "DPAD L"},   [PB_RIGHT] = {"pad_right", "DPAD R"},
  [PB_P1]     = {"pad_p1",     "PADDLE 1"}, [PB_P2]    = {"pad_p2",    "PADDLE 2"},
  [PB_P3]     = {"pad_p3",     "PADDLE 3"}, [PB_P4]    = {"pad_p4",    "PADDLE 4"},
};

// Pad feel parameters, table-driven like MV_DEF so config I/O and the GAMEPAD
// tab share one source. Deadzones are PER STICK — a worn left stick drifts
// while the right one is fine, and one shared number forces the bad stick's
// deadzone onto the good one. Sens is the full-deflection turn rate in deg/s,
// a unit a player can picture (240 = a half turn in 0.75 s).
typedef enum { PAD_DZ_L, PAD_DZ_R, PAD_SENS, PAD_ADS_SENS,
               PAD_AS_FRIC, PAD_AS_BUBBLE, PAD_AS_DIST,
               PAD_COUNT } pad_param_t;
// ONE sensitivity, the console 1-10 notch scale: a notch is 60 deg/s of yaw
// and 40 deg/s of pitch (the genre's 2:3 vertical ratio baked in — separate
// X/Y sliders were two technical numbers nobody wanted to co-tune, and the
// default 4 reproduces the old 240/160 exactly).
#define PAD_SENS_DEG_X 60.0f
#define PAD_SENS_DEG_Y 40.0f
static const param_def_t PAD_DEF[PAD_COUNT] = {
  // 0.00 / 0.04: the deadzones default to essentially OFF because the Deck and
  // the Legion Go S both centre cleanly, and a 0.12 dead ring is 12% of the
  // stick's travel spent doing nothing — it is the single most felt cause of
  // "the aim is mushy". Per-stick is what makes that affordable: the left stick
  // is a walk direction and tolerates 0.00, the right one carries the aim and
  // keeps a hair of guard. A drifting pad raises them in the GAMEPAD tab; that
  // is a broken-hardware fix, not an out-of-box default.
  [PAD_DZ_L]    = {"pad_dz_l",     "DEADZONE L",   0.00f,  0.00f,   0.45f,  0.01f},
  [PAD_DZ_R]    = {"pad_dz_r",     "DEADZONE R",   0.04f,  0.00f,   0.45f,  0.01f},
  [PAD_SENS]    = {"pad_sens",     "SENSITIVITY",  4.0f,   1.0f,   10.0f,   1.0f},
  // Multiplier on the FOV-derived ADS stick slowdown: 1.00 = the pure zoom
  // ratio, lower = extra deliberate aimed turns. A factor, so it composes
  // with any weapon's zoom instead of fighting it.
  [PAD_ADS_SENS] = {"pad_ads_sens", "ADS SENS",     0.70f,  0.10f,   1.00f,  0.05f},
  // Friction-only aim assist (DEV > ASSIST tab). Three sliders, each monotone
  // in an obvious direction and none cancelling another — the previous 5-knob
  // set (metre bubbles, a falloff exponent whose LOW end weakened the assist)
  // let a whole exploration session land back on the default feel (measured
  // 2026-08-12: user extremes ate 3.9 deg of a 60 deg sweep vs default's
  // 3.0). FRICTION is the slowdown STRENGTH at the bubble's centre (0 = off,
  // 1 = full stop); BUBBLE is the cone HALF-ANGLE in degrees where friction
  // engages (fixed angle — directly feelable, no atan-of-metres); DIST is the
  // reach in metres, strength fading LINEARLY with distance (half the
  // distance = half the effect). The ease shape inside the bubble is baked
  // (1 - smoothstep^2, the measured keeper).
  // Tuned 2026-08-19 to a NARROW, STRONG, LONG bubble — the opposite trade to
  // the wide-and-soft default it replaces. A 8 deg half-angle at friction 0.85
  // drags on everything near the target and the slowdown is felt as the stick
  // going heavy while you are still aiming at nothing; 2.5 deg at full strength
  // is invisible until the reticle is genuinely on a body, and then it holds.
  // DIST 80 (the slider ceiling) because the fade is LINEAR: at 60 m of reach a
  // 30 m duel — the arena's p90 sightline is ~31 m — was already running at
  // half strength, i.e. the assist quietly stopped existing at exactly the
  // range where a stick needs it most.
  [PAD_AS_FRIC]   = {"pad_as_friction", "FRICTION", 1.00f, 0.05f, 1.00f, 0.05f},
  [PAD_AS_BUBBLE] = {"pad_as_bubble",   "BUBBLE",   2.5f,  1.0f,  15.0f, 0.5f},
  [PAD_AS_DIST]   = {"pad_as_dist",     "DIST",     80.0f, 5.0f,  80.0f, 1.0f},
};
static const char *const PAD_CURVE_OPTS[3] = {"LINEAR", "STANDARD", "DYNAMIC"};

// THE ARMORY: what the player carries, as opposed to how it handles. All three
// are COSMETIC and first-person only — the sim, the bots and the wire never
// read them, so a themed rifle is the same rifle to every proof in the battery.
// (The LOADOUT below them is the exception, and is marked as one.)
// Index 0 of each is the shipped look and is an exact identity in the
// renderer: an untouched config draws byte-for-byte what it drew before.
// The tables themselves (models, colours) live beside model_draw.
#define KNIFE_MODEL_COUNT 5
static const char *const KNIFE_MODEL_NAMES[KNIFE_MODEL_COUNT] = {
  "COMBAT", "TANTO", "DAGGER", "KUKRI", "TALON"};
#define WPN_THEME_COUNT 11
static const char *const WPN_THEME_NAMES[WPN_THEME_COUNT] = {
  "FACTORY", "DESERT", "WOODLAND", "ARCTIC", "URBAN", "TIGER",
  "BLACKOUT", "CRIMSON", "COBALT", "FADE", "GILDED"};
#define GEAR_THEME_COUNT 5
static const char *const GEAR_THEME_NAMES[GEAR_THEME_COUNT] = {
  "SLATE", "COYOTE", "RANGER", "SHADOW", "ARCTIC"};
// The third-person body's own pieces (fig_kit and the figure builders). Index
// 0 of each is the shipped operator, so an untouched config is unchanged.
#define HEAD_GEAR_COUNT 4
enum { HEAD_HELMET, HEAD_CAP, HEAD_WRAP, HEAD_BARE };
static const char *const HEAD_GEAR_NAMES[HEAD_GEAR_COUNT] = {
  "HELMET", "CAP", "HEAD WRAP", "BARE"};
#define VEST_GEAR_COUNT 3
enum { VEST_CARRIER, VEST_RIG, VEST_NONE };
static const char *const VEST_GEAR_NAMES[VEST_GEAR_COUNT] = {
  "PLATE CARRIER", "CHEST RIG", "NONE"};
#define PACK_GEAR_COUNT 2
enum { PACK_ASSAULT, PACK_NONE };
static const char *const PACK_GEAR_NAMES[PACK_GEAR_COUNT] = {"ASSAULT PACK", "NONE"};
static int armory_idx(int v, int n) { return v >= 0 && v < n ? v : 0; }
// THE LOADOUT, and unlike everything above it is NOT cosmetic: each slot's
// variant is its own gun — damage, magazine, fire mode, rate and recoil (see
// WPN_VAR). Index 0 is the shipped weapon and an exact identity everywhere, so
// an untouched config and every bot fight with what they always fought with.
// A change takes effect on the next spawn; online it rides each input to the
// server (net_input_t.loadout), which is the one that applies it. WPN_VARS is
// defined beside WPN_VAR.
static const char *const RIFLE_VAR_NAMES[WPN_VARS] = {"M4 CARBINE", "AK-47", "FAMAS"};
static const char *const PISTOL_VAR_NAMES[WPN_VARS] = {"DUTY PISTOL", "GLOCK-18", "DESERT EAGLE"};
// Crosshair options. The colours are chosen for VALUE as much as hue: each
// reads against both the bright sand/sky and the dark interiors, and the set
// spans the hues that survive the common colour-vision deficiencies (yellow
// and cyan stay distinct from each other and from the world under protan and
// deutan simulation, where the default bone white and red do not separate
// from a red-helmeted enemy as well).
#define CH_STYLE_COUNT 5
#define CH_COLOR_COUNT 6
static const char *const CH_STYLE_NAMES[CH_STYLE_COUNT] = {"CROSS", "DOT", "CROSS+DOT", "CIRCLE", "T"};
static const char *const CH_COLOR_NAMES[CH_COLOR_COUNT] = {"WHITE", "YELLOW", "CYAN", "GREEN", "MAGENTA", "RED"};
static const float CH_COLORS[CH_COLOR_COUNT][3] = {
  {0.941f, 0.957f, 0.973f}, {1.00f, 0.90f, 0.20f}, {0.30f, 0.95f, 1.00f},
  {0.40f, 1.00f, 0.42f},    {1.00f, 0.40f, 0.95f}, {1.00f, 0.32f, 0.26f},
};
static const char *const VM_POS_NAMES[3] = {"STANDARD", "LOW", "LOWER"};

// Raw controller state, written by the platform pump. Sticks are -1..1 with
// +x right and +y UP (XInput's convention; the evdev pump negates its
// down-positive Y axes so no consumer ever sees both conventions). Triggers
// are 0..1. btn[] holds digital LEVELS; LT/RT land there through the
// hysteresis in pad_btn_sync so "trigger as button" cannot flicker at the
// threshold.
typedef struct {
  int   connected;
  float lx, ly, rx, ry;   // sticks
  float lt, rt;           // triggers
  int   btn[PB_COUNT];
} pad_state_t;
static pad_state_t g_pad;

// Portable GameInput-shaped sample. Platform adapters write this one logical
// vocabulary; the shared mapper below is the only place native button masks
// become the game's pad_state_t contract.
typedef struct {
  int connected;
  float lx, ly, rx, ry;
  float lt, rt;
  uint32_t buttons;
} pad_standard_state_t;

enum {
  PAD_STANDARD_MENU  = 0x0001u, PAD_STANDARD_VIEW  = 0x0002u,
  PAD_STANDARD_A     = 0x0004u, PAD_STANDARD_B     = 0x0008u,
  PAD_STANDARD_X     = 0x0010u, PAD_STANDARD_Y     = 0x0020u,
  PAD_STANDARD_UP    = 0x0040u, PAD_STANDARD_DOWN  = 0x0080u,
  PAD_STANDARD_LEFT  = 0x0100u, PAD_STANDARD_RIGHT = 0x0200u,
  PAD_STANDARD_LB    = 0x0400u, PAD_STANDARD_RB    = 0x0800u,
  PAD_STANDARD_L3    = 0x1000u, PAD_STANDARD_R3    = 0x2000u,
  PAD_STANDARD_P1    = 0x4000u, PAD_STANDARD_P2    = 0x8000u,
  PAD_STANDARD_P3    = 0x10000u, PAD_STANDARD_P4   = 0x20000u,
};

static void pad_apply_standard(pad_state_t *dst, const pad_standard_state_t *src) {
  static const struct { uint32_t bit; pad_btn_t button; } button_map[] = {
    {PAD_STANDARD_MENU, PB_START}, {PAD_STANDARD_VIEW, PB_SELECT},
    {PAD_STANDARD_A, PB_A},        {PAD_STANDARD_B, PB_B},
    {PAD_STANDARD_X, PB_X},        {PAD_STANDARD_Y, PB_Y},
    {PAD_STANDARD_UP, PB_UP},      {PAD_STANDARD_DOWN, PB_DOWN},
    {PAD_STANDARD_LEFT, PB_LEFT},  {PAD_STANDARD_RIGHT, PB_RIGHT},
    {PAD_STANDARD_LB, PB_LB},      {PAD_STANDARD_RB, PB_RB},
    {PAD_STANDARD_L3, PB_L3},      {PAD_STANDARD_R3, PB_R3},
    {PAD_STANDARD_P1, PB_P1},      {PAD_STANDARD_P2, PB_P2},
    {PAD_STANDARD_P3, PB_P3},      {PAD_STANDARD_P4, PB_P4},
  };
  memset(dst, 0, sizeof *dst);
  if (!src->connected) return;
  dst->connected = 1;
  dst->lx = f_clamp(src->lx, -1.0f, 1.0f);
  dst->ly = f_clamp(src->ly, -1.0f, 1.0f);
  dst->rx = f_clamp(src->rx, -1.0f, 1.0f);
  dst->ry = f_clamp(src->ry, -1.0f, 1.0f);
  dst->lt = f_clamp(src->lt, 0.0f, 1.0f);
  dst->rt = f_clamp(src->rt, 0.0f, 1.0f);
  for (size_t i = 0; i < sizeof button_map / sizeof button_map[0]; i++)
    dst->btn[button_map[i].button] = (src->buttons & button_map[i].bit) != 0;
}

// XInput's ABI packet is represented separately so its native mask/range
// conversion can be tested by the Linux harness instead of living untested in
// the Windows-only polling loop.
typedef struct {
  uint16_t buttons;
  uint8_t lt, rt;
  int16_t lx, ly, rx, ry;
} pad_xinput_raw_t;

static void pad_apply_xinput(pad_state_t *dst, const pad_xinput_raw_t *raw) {
  uint32_t buttons = 0;
  const uint16_t b = raw->buttons;
  if (b & 0x0001u) buttons |= PAD_STANDARD_UP;
  if (b & 0x0002u) buttons |= PAD_STANDARD_DOWN;
  if (b & 0x0004u) buttons |= PAD_STANDARD_LEFT;
  if (b & 0x0008u) buttons |= PAD_STANDARD_RIGHT;
  if (b & 0x0010u) buttons |= PAD_STANDARD_MENU;
  if (b & 0x0020u) buttons |= PAD_STANDARD_VIEW;
  if (b & 0x0040u) buttons |= PAD_STANDARD_L3;
  if (b & 0x0080u) buttons |= PAD_STANDARD_R3;
  if (b & 0x0100u) buttons |= PAD_STANDARD_LB;
  if (b & 0x0200u) buttons |= PAD_STANDARD_RB;
  if (b & 0x1000u) buttons |= PAD_STANDARD_A;
  if (b & 0x2000u) buttons |= PAD_STANDARD_B;
  if (b & 0x4000u) buttons |= PAD_STANDARD_X;
  if (b & 0x8000u) buttons |= PAD_STANDARD_Y;
  pad_apply_standard(dst, &(pad_standard_state_t){
    .connected = 1,
    .lx = (float)raw->lx / 32767.0f, .ly = (float)raw->ly / 32767.0f,
    .rx = (float)raw->rx / 32767.0f, .ry = (float)raw->ry / 32767.0f,
    .lt = (float)raw->lt / 255.0f, .rt = (float)raw->rt / 255.0f,
    .buttons = buttons,
  });
}

// HID/DirectInput-shaped sample. DirectInput can tell WHICH axes a device has
// (object enumeration) and WHAT their range is (DIPROP_RANGE — the backend
// normalises against the range it set, so there is nothing to "calibrate"),
// but not what they MEAN: one pad's right stick lives on Z/Rz (DualShock,
// 8BitDo over Bluetooth) where another's is Rx/Ry (Xbox via DInput, Switch
// Pro), and the analog triggers may be axes, sliders, or missing entirely
// with digital bits standing in. That decision is pure logic on the axis
// PRESENCE mask plus a vendor id for the two genuinely ambiguous families
// (Chromium's gamepad mapper is the same shape), so it lives HERE in shared
// code where padbackend proves every layout — the guessed-table version of
// this mapper shipped three wrong-axis bugs the compile-only Windows target
// could never catch.
enum {  // Generic Desktop usage order == the DIJOYSTATE member order
  PH_X, PH_Y, PH_Z, PH_RX, PH_RY, PH_RZ, PH_S0, PH_S1, PH_AXES
};
typedef struct {
  int      connected;
  uint32_t axes;          // presence bitmask, 1 << PH_*
  float    ax[PH_AXES];   // -1..1 as reported (+y DOWN, HID's convention)
  int      pov;           // hundredths of a degree CW from north, -1 = centered
  uint32_t buttons;       // bit i = raw HID button index i
  uint16_t vid;           // USB vendor id, 0 = unknown
} pad_hid_raw_t;

typedef struct {
  // rgbButtons index of A B X Y LB RB SELECT START L3 R3 P1 P2 P3 P4
  // (-1 = absent), the order of PAD_HID_BTN_BITS below. Positional
  // semantics (A = south).
  int8_t  btn[14];
  int8_t  lt, rt;         // digital trigger bits, drive lt/rt when pressed
  uint8_t right_rxry;     // both axis pairs present: right stick is Rx/Ry
  uint8_t trig_digital;   // never read analog trigger axes on this vendor
} pad_hid_map_t;
static const uint32_t PAD_HID_BTN_BITS[14] = {
  PAD_STANDARD_A,  PAD_STANDARD_B,  PAD_STANDARD_X,    PAD_STANDARD_Y,
  PAD_STANDARD_LB, PAD_STANDARD_RB, PAD_STANDARD_VIEW, PAD_STANDARD_MENU,
  PAD_STANDARD_L3, PAD_STANDARD_R3, PAD_STANDARD_P1,   PAD_STANDARD_P2,
  PAD_STANDARD_P3, PAD_STANDARD_P4,
};

static uint32_t pad_hid_pov_bits(int pov) {
  if (pov < 0) return 0;   // centered
  static const uint32_t dir[8] = {
    PAD_STANDARD_UP,   PAD_STANDARD_UP | PAD_STANDARD_RIGHT,
    PAD_STANDARD_RIGHT, PAD_STANDARD_DOWN | PAD_STANDARD_RIGHT,
    PAD_STANDARD_DOWN, PAD_STANDARD_DOWN | PAD_STANDARD_LEFT,
    PAD_STANDARD_LEFT, PAD_STANDARD_UP | PAD_STANDARD_LEFT,
  };
  return dir[((pov + 2250) % 36000) / 4500];
}

static void pad_apply_hid(pad_state_t *dst, const pad_hid_raw_t *src) {
  // The three vendor rows are the measured exceptions; everything unknown
  // gets the sequential Xbox order — SDL's own documented guess for a pad
  // it has no db entry for.
  static const pad_hid_map_t MAP_XBOX =   // generic fallback
    {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1},           -1, -1, 0, 0};
  // 8BitDo D-input mode (the Ultimate 2 family's only Bluetooth protocol):
  // Android numbering with the four paddles at 17/16/2/5 (the 2C variants
  // carry only the 2/5 pair, which then reads as PADDLE 3/4 — still
  // bindable, just numbered like the bigger model's rear pair is absent).
  static const pad_hid_map_t MAP_8BITDO =
    {{0, 1, 3, 4, 6, 7, 10, 11, 13, 14, 17, 16, 2, 5},          8,  9, 1, 1};
  static const pad_hid_map_t MAP_SONY =   // DualShock 4 / DualSense
    {{1, 2, 0, 3, 4, 5, 8, 9, 10, 11, -1, -1, -1, -1},          6,  7, 0, 0};
  static const pad_hid_map_t MAP_SWITCH = // Pro Controller (south IS its B)
    {{0, 1, 2, 3, 4, 5, 8, 9, 10, 11, -1, -1, -1, -1},          6,  7, 1, 1};
  if (!src->connected) { memset(dst, 0, sizeof *dst); return; }
  const pad_hid_map_t *m = src->vid == 0x2DC8 ? &MAP_8BITDO
                         : src->vid == 0x054C ? &MAP_SONY
                         : src->vid == 0x057E ? &MAP_SWITCH : &MAP_XBOX;
  const uint32_t ax = src->axes;
  const int zrz  = (ax & 1u << PH_Z)  && (ax & 1u << PH_RZ);
  const int rxry = (ax & 1u << PH_RX) && (ax & 1u << PH_RY);
  float rx = 0, ry = 0;
  float lt = -1, rt = -1;                    // < 0 = no analog source found
  if (rxry && (m->right_rxry || !zrz)) {
    rx = src->ax[PH_RX];
    ry = src->ax[PH_RY];
    // Xbox-through-DInput folds both triggers onto one Z axis: LT deflects
    // +, RT deflects -, both pressed = centre. Per-trigger analog is
    // unrecoverable from that; splitting on sign is the standard reading.
    if (!m->trig_digital && (ax & 1u << PH_Z) && !(ax & 1u << PH_RZ)) {
      lt = f_clamp(src->ax[PH_Z], 0.0f, 1.0f);
      rt = f_clamp(-src->ax[PH_Z], 0.0f, 1.0f);
    }
  } else if (zrz) {
    rx = src->ax[PH_Z];
    ry = src->ax[PH_RZ];
    if (!m->trig_digital && rxry) {          // DualShock shape: analog rest -1
      lt = (src->ax[PH_RX] + 1.0f) * 0.5f;
      rt = (src->ax[PH_RY] + 1.0f) * 0.5f;
    }
  }
  if (lt < 0 && !m->trig_digital && (ax & 1u << PH_S0) && (ax & 1u << PH_S1)) {
    lt = (src->ax[PH_S0] + 1.0f) * 0.5f;     // slider triggers, rest -1
    rt = (src->ax[PH_S1] + 1.0f) * 0.5f;
  }
  if (lt < 0) lt = rt = 0;
  // Digital trigger bits ride on top of (or stand in for) the analog value;
  // the max keeps a vendor reporting both from flickering at the threshold.
  if (m->lt >= 0 && src->buttons >> m->lt & 1u) lt = 1.0f;
  if (m->rt >= 0 && src->buttons >> m->rt & 1u) rt = 1.0f;
  uint32_t buttons = pad_hid_pov_bits(src->pov);
  for (int i = 0; i < 14; i++)
    if (m->btn[i] >= 0 && src->buttons >> m->btn[i] & 1u)
      buttons |= PAD_HID_BTN_BITS[i];
  pad_apply_standard(dst, &(pad_standard_state_t){
    .connected = 1,
    .lx = src->ax[PH_X], .ly = -src->ax[PH_Y],   // HID +y is DOWN, ours is UP
    .rx = rx, .ry = -ry,
    .lt = lt, .rt = rt, .buttons = buttons,
  });
}

// A gamepad NODE exists but the game may not open it. Set by the Linux scan
// when a node whose sysfs capabilities say "gamepad" refuses open()/grab:
// that is what Steam Input's device hiding looks like from outside (it holds
// the physical pad and zeroes the ACL mask), and the fix — pick the Gamepad
// layout so Steam creates its virtual pad — is something only the PLAYER can
// do, so the GAMEPAD tab has to say it. Windows never sets it.
static int g_pad_denied;

// Defined with the UI, needed by the pad move gate in player_tick: analog
// move bypasses the held[] array that frame_input_keys already gates on the
// menu. Tentative definition — merges with the UI section's.
static int g_menu_open;
// The boot screen ("HOME"): a live showcase behind a tile rail, not a paused
// frame behind a panel. Declared this early because the two things that make a
// showcase possible at all are read far below the UI — bot_tick must not think
// while it is up (or the player is shot dead in the main menu) and
// session_pauses() must not freeze the world (or the showcase is a photograph
// with a moving camera over it). Both are one term each; see the HOME director.
static int g_home;
// The boot screen runs its own bot count. The player's `sp_bots` is his MATCH
// setting and the tile still shows it; a demo reel wants a firefight behind the
// camera, and one opponent is not one — but six of them turned every wide shot
// into a scrum. Three is a firefight; six is a crowd. Read through sp_bot_count() at both of
// the two places the count is consumed, so it cannot be applied at one and
// missed at the other.
#define HOME_BOTS 3
static int sp_bot_count(void);

// Radial SCALED deadzone: direction preserved, magnitude remapped so motion
// starts at exactly zero AT the deadzone edge and reaches 1 at full
// deflection. PAD_OUTER is 1.0 — deliberately NOT the 0.95-style outer
// threshold other games use: with the deadzone at 0 the pipeline is then
// EXACTLY the identity (stick 0.5 = half the turn rate, proven by `padlook`),
// which is what "linear" promises. The classic outer threshold exists for
// worn hardware that never reports 1.0, but every source this game reads —
// Steam Input's virtual pad, XInput, calibrated evdev ranges — normalises
// full deflection to 1.0, and diagonals are clamped in the pump. A per-axis
// deadzone instead of this radial one makes small diagonal aim corrections
// snap to the axes, which is the classic "cross-shaped deadzone" complaint.
#define PAD_OUTER 1.0f
static float pad_deadzone(float x, float y, float dz, float *ox, float *oy) {
  float m = sqrtf(x * x + y * y);
  if (m <= dz) { *ox = *oy = 0; return 0; }
  float n = (m - dz) / (PAD_OUTER - dz);
  if (n > 1) n = 1;
  *ox = x / m * n;
  *oy = y / m * n;
  return n;
}

// Aim response curve on the deflection MAGNITUDE (never per axis — a per-axis
// curve bends the aim direction toward the diagonals). All three map 0->0 and
// 1->1, so the curve never changes the top speed, only how the stick travels
// toward it: LINEAR is 1:1, STANDARD is the classic power curve (fine control
// near centre, most of the speed in the outer half), DYNAMIC is the reverse
// S — responsive immediately, a flat precision band mid-travel, then quick
// again to the stop (the closed form is the inverse of smoothstep).
static float pad_curve_apply(float m, int curve) {
  m = f_clamp(m, 0.0f, 1.0f);
  switch (curve) {
    case 1:  return m * m;
    case 2:  return 0.5f - sinf(asinf(1.0f - 2.0f * m) * (1.0f / 3.0f));
    default: return m;
  }
}

