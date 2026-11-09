// code/app/app.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ===========================================================================
// Frame step — the part of a game loop that is GAME rather than PLATFORM.
// ===========================================================================
// A backend owns a window, a clock, a keyboard and a swap chain. Everything
// between "here is dt and the input state" and "swap the buffers" is the same
// game on every target, and it used to live inside WinMain — which meant a
// second backend had to grow a second copy of the hitch clamp, the tick
// accumulator, the press-latch rule, the listener update and the event drain.
// That is the defect class this file refuses everywhere else (ONE mv_walk for
// the player and the bots, ONE gun_shoot for whoever pulls the trigger, and
// `parity` to prove it); the loop is no different.
//
// What is deliberately NOT here: message pumps, cursor clipping, focus,
// timers, buffer swaps and the frame limiter. Those genuinely differ, and the
// Windows backend's hard-won behaviour lives in them.

// --- Key ids and the bind vocabulary --------------------------------------
// ONE index space for both windowed backends. The numbers ARE Windows virtual
// key codes, because one of the two gets them from the OS for free and the
// other has to map anyway: X11 turns a keysym into the same id, so a
// `config.cfg` means exactly the same thing on both, and the token table below
// exists once instead of once per platform. Mouse buttons live above 255,
// where no VK code reaches.
#define KEY_TAB    0x09   // VK_TAB
#define KEY_SPACE  0x20   // VK_SPACE
#define KEY_LSHIFT 0xA0   // VK_LSHIFT
#define KEY_LCTRL  0xA2   // VK_LCONTROL
#define KEY_MOUSE1 256
#define KEY_MOUSE2 257
#define KEY_MOUSE3 258
#define KEY_MOUSE4 259
#define KEY_MOUSE5 260
// The WHEEL lives in the same latch array and deliberately NOT in the bind
// vocabulary. It is an IMPULSE, not a key: there is no level to hold, so
// g_keys is never set for these two and only the press latch is — which is
// exactly the "tap shorter than one frame" case held_first already exists for.
// Keeping it out of key_from_token/token_from_key is what stops the KEYS tab's
// rebind capture from assigning a notch to an action that would then be held
// for zero frames, and what stops it from being persisted into config.cfg
// under a vocabulary config_emit's own header does not document.
#define KEY_MWUP   261
#define KEY_MWDN   262
// Gamepad buttons extend the same space (+ pad_btn_t index), so binds, press
// latches and the menu gate treat a controller exactly like a keyboard.
#define KEY_PAD0   263
#define KEY_COUNT  (KEY_PAD0 + PB_COUNT)

// The vocabulary is fixed by ACTION_DEF's defaults and by config_emit's own
// header comment; anything outside it returns 0, which is what makes
// resolve_binds fall back to the default rather than leave an action dead.
static int key_from_token(const char *tok) {
  if (strlen(tok) == 1) {
    char c = tok[0];
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    if (c >= 'A' && c <= 'Z') return c;
    if (c >= '0' && c <= '9') return c;
  }
  if (!strcmp(tok, "space"))  return KEY_SPACE;
  if (!strcmp(tok, "tab"))    return KEY_TAB;
  if (!strcmp(tok, "lshift")) return KEY_LSHIFT;
  if (!strcmp(tok, "lctrl"))  return KEY_LCTRL;
  if (!strcmp(tok, "mouse1")) return KEY_MOUSE1;
  if (!strcmp(tok, "mouse2")) return KEY_MOUSE2;
  if (!strcmp(tok, "mouse3")) return KEY_MOUSE3;
  if (!strcmp(tok, "mouse4")) return KEY_MOUSE4;
  if (!strcmp(tok, "mouse5")) return KEY_MOUSE5;
  if (!strncmp(tok, "pad_", 4))
    for (int i = 0; i < PB_COUNT; i++)
      if (!strcmp(tok, PAD_BTN_DEF[i].tok)) return KEY_PAD0 + i;
  return 0;
}

// Reverse mapping, for the menu's rebind capture and the name field. NULL =
// not a bindable key, which the backends use to decide whether to swallow the
// keystroke.
static const char *token_from_key(int key) {
  static char one[2];
  if (key >= 'A' && key <= 'Z') { one[0] = (char)(key - 'A' + 'a'); one[1] = 0; return one; }
  if (key >= '0' && key <= '9') { one[0] = (char)key; one[1] = 0; return one; }
  switch (key) {
    case KEY_SPACE:  return "space";
    case KEY_TAB:    return "tab";
    case KEY_LSHIFT: return "lshift";
    case KEY_LCTRL:  return "lctrl";
    case KEY_MOUSE1: return "mouse1";
    case KEY_MOUSE2: return "mouse2";
    case KEY_MOUSE3: return "mouse3";
    case KEY_MOUSE4: return "mouse4";
    case KEY_MOUSE5: return "mouse5";
  }
  if (key >= KEY_PAD0 && key < KEY_COUNT) return PAD_BTN_DEF[key - KEY_PAD0].tok;
  return NULL;
}

// Key LEVEL plus a press LATCH that survives until a sim tick consumes it —
// written by whichever windowed backend is compiled, read only through
// frame_input_keys. The harness drives the sim through its own level array and
// never touches these.
static int g_keys[KEY_COUNT], g_pressed[KEY_COUNT];

typedef struct {
  float mouse_dx, mouse_dy;    // raw mouse counts accumulated since the last frame
  int   held[IN_COUNT];        // action held right now
  int   held_first[IN_COUNT];  // held OR latched — a tap shorter than one frame
} frame_input_t;               //   still has to produce a press edge in the sim

typedef struct {
  double acc;         // tick accumulator; its remainder is the render alpha
  double fps_window;
  int    fps, frames;
  GLsync fence;       // previous frame's GPU work, see app_sync
} app_t;

// Everything a playable session needs once a GL context is current. The
// harness runs the identical sequence — a session that differs from the one
// under test is a session nobody tested.
// The ONE session bring-up, all four entry points (play, harness, server,
// Windows). Its own warning applies: a session that differs from the tested
// one is a session nobody tested — so what the SERVER gate skips is written
// out. With g_server, renderer_init and sfx_build_all do not run, which
// leaves unset: every GL object in R, all baked SFX buffers, and g_gun_low[]
// (MEASURED off the first weapon build's emitted geometry, read only by the
// corpse weapon placement — render-side). THE SIM MUST NOT READ ANY OF THESE;
// a future sim reader of g_gun_low would silently get 0 on the server.
static void app_start(player_t *p, int w, int h, uint32_t seed) {
  map_pick_apply(0);
  g_map_seed = seed;
  map_generate(seed);
  if (!g_server) {
    renderer_init(w, h);
    sfx_build_all();
    sfx_apply_samples();   // the embedded samples replace their synth buffers
  }
  player_spawn(p);
  match_init();
}

// THE WHEEL SCROLLS THROUGH THE WEAPONS, and it is wired to IN_SWAP rather
// than to a pair of NEXT/PREV actions on purpose: the whole
// switch — the raise timeline, the abandoned reload, the direct-slot no-op —
// lives in weapon_tick behind that ONE press edge, and a second route into it
// is the one-copy rule broken for a mouse.
// A notch sets held_first for exactly one tick and never g_keys, which is the
// same edge the bound key produces: the sim, the wire and the animation cannot
// tell the two apart. Its own function because `appframe` is the second reader
// — the headless path that proves this, since neither backend can be driven
// from a script.
static int input_wheel_swap(void) {
  return !g_menu_open && (g_pressed[KEY_MWUP] || g_pressed[KEY_MWDN]);
}

// held/held_first for one frame. The menu gate and the "held OR latched" rule
// are policy and belong here; only filling g_keys/g_pressed is the backend's.
// An action is held if EITHER its keyboard key or its pad button is down —
// the two tables coexist rather than switch, because a Deck player uses the
// touchscreen keyboard's config and the pad in the same session.
static void frame_input_keys(frame_input_t *in, const int binds[IN_COUNT],
                             const int pbinds[IN_COUNT]) {
  for (int i = 0; i < IN_COUNT; i++) {
    int held = g_keys[binds[i]] || (pbinds[i] && g_keys[pbinds[i]]);
    int latch = g_pressed[binds[i]] || (pbinds[i] && g_pressed[pbinds[i]]);
    // The BOOT SCREEN substitutes the director's own finger for the player's.
    // It is the same array the sim reads, so a swap is the real switch
    // timeline and a shoulder is the real ADS ramp — there is no second
    // animation path, and nothing in player_move_tick knows the difference.
    int hb = g_home ? home_input(i) : 0;
    in->held[i] = g_menu_open ? hb : held;
    in->held_first[i] = g_menu_open ? hb : (held || latch);
  }
  if (input_wheel_swap()) in->held_first[IN_SWAP] = 1;
}

// Config tokens -> key ids; a typo in the config falls back to the action's own
// default so no action can silently go dead. Re-run every frame: it is a
// handful of string compares, and it makes a menu rebind apply on the next
// frame without any notification path.
// The PAD table differs in one way: "none" is a deliberate absence (movement
// lives on the stick), so only a token that was SUPPOSED to be something falls
// back to the default.
static void resolve_binds(int binds[IN_COUNT], int pbinds[IN_COUNT]) {
  for (int i = 0; i < IN_COUNT; i++) {
    binds[i] = key_from_token(g_cfg.bind[i]);
    if (!binds[i]) binds[i] = key_from_token(ACTION_DEF[i].dflt);
    pbinds[i] = key_from_token(g_cfg.padbind[i]);
    if (!pbinds[i] && strcmp(g_cfg.padbind[i], "none") != 0)
      pbinds[i] = key_from_token(ACTION_DEF[i].pdflt);
  }
}

// --- Gamepad frame step — shared policy over the g_pad state the backend
// filled. Everything below runs identically on both platforms and is driven
// by the harness's `pad`/`padsync` commands for its proofs. ---

// Trigger-as-button hysteresis: ON above 25%, OFF below 15%. One threshold
// makes a trigger held right at it flicker level/edge/level at the tick rate,
// which the sim reads as a stream of semi-auto trigger pulls; the press point
// sits inside the genre's usual 0.18-0.30 band — deep enough that resting a
// finger on the trigger does not fire, and every extra millimetre of pull past
// that is added fire latency.
#define PAD_TRIG_ON  0.25f
#define PAD_TRIG_OFF 0.15f
static void pad_btn_sync(void) {
  g_pad.btn[PB_LT] = g_pad.lt > (g_pad.btn[PB_LT] ? PAD_TRIG_OFF : PAD_TRIG_ON);
  g_pad.btn[PB_RT] = g_pad.rt > (g_pad.btn[PB_RT] ? PAD_TRIG_OFF : PAD_TRIG_ON);
}

// Menu navigation repeat: a held direction (dpad or left stick) fires once,
// waits, then repeats — the same feel every console front-end has.
#define PAD_NAV_DELAY 0.35f
#define PAD_NAV_REP   0.11f
#define PAD_NAV_ACCEL 4.0f   // extra repeat speed a long hold ramps up to
#define PAD_NAV_RAMP  2.5f   // seconds of repeating it takes to get there
static float g_pad_nav_t[4], g_pad_nav_hold[4];
static void pad_nav_dir(int idx, int level, float dt, int nav) {
  if (!level) { g_pad_nav_t[idx] = 0; g_pad_nav_hold[idx] = 0; return; }
  g_nav_kbd = 0;   // the pad reclaims the footer grammar from the keyboard
  if (g_pad_nav_t[idx] == 0) nav_push(nav);
  g_pad_nav_t[idx] += dt;
  g_pad_nav_hold[idx] += dt;
  if (g_pad_nav_t[idx] > PAD_NAV_DELAY) {
    // Hold-to-accelerate: 9 Hz at first repeat, ~45 Hz after PAD_NAV_RAMP of
    // holding — a FRAG LIMIT slider spanning 1..1000 at step 1 is otherwise
    // up to ~107 s of held input, while a short tap still steps exactly once.
    float held = g_pad_nav_hold[idx] - PAD_NAV_DELAY;
    float accel = 1.0f + PAD_NAV_ACCEL * f_clamp(held / PAD_NAV_RAMP, 0.0f, 1.0f);
    nav_push(nav);
    g_pad_nav_t[idx] -= PAD_NAV_REP / accel;
    // One push per frame is the ceiling anyway, so a frame dt LONGER than the
    // accelerated interval must not bank the difference: unclamped, a 30 fps
    // frame outruns the 22 ms interval and the backlog would burst out as a
    // slug of extra repeats the moment the frame rate recovers.
    if (g_pad_nav_t[idx] > PAD_NAV_DELAY + PAD_NAV_REP)
      g_pad_nav_t[idx] = PAD_NAV_DELAY + PAD_NAV_REP;
  }
}

// Swallow-until-release state, see pad_eat_held's forward declaration for the
// contract. A swallowed button contributes nothing — no key level, no edge,
// no navigation — until it has physically come up once: the press that
// crossed the boundary belongs to the OTHER side of it.
static int g_pad_eat[PB_COUNT];
static void pad_eat_held(void) {
  pad_btn_sync();  // triggers count as held too — an RT across an alt-tab
  for (int i = 0; i < PB_COUNT; i++)
    if (g_pad.btn[i]) g_pad_eat[i] = 1;
}

// Once per frame: trigger hysteresis, button levels into the shared key
// space (so binds see a controller as just more keys), and — with the menu
// open — the same buttons become navigation instead. START is the pad's
// escape key and is handled here exclusively; it never reaches the key space
// at all, which is what makes it unbindable by construction (a hand-edited
// `padbind_jump pad_start` resolves but the key can never go down).
static void pad_sync(float dt) {
  static int prev[PB_COUNT];
  pad_btn_sync();
  // The eat gate applies to START too: prev[] goes stale while the game is
  // unfocused (the pad is pumped but pad_sync is skipped), so the first active
  // frame sees a fresh edge for a button pad_eat_held already swallowed — and
  // an eaten START toggling the menu is exactly the class the swallow exists
  // to stop. The eat state clears on physical release in the loop below.
  if (g_pad.btn[PB_START] && !prev[PB_START] && !g_pad_eat[PB_START])
    ui_on_escape();
  for (int i = 0; i < PB_COUNT; i++) {
    int down = g_pad.btn[i];
    int edge = down && !prev[i];
    prev[i] = down;
    if (g_pad_eat[i]) {
      if (down) { g_keys[KEY_PAD0 + i] = 0; continue; }
      g_pad_eat[i] = 0;
    }
    if (edge && g_menu_open) {
      // An armed PAD capture owns every button: the press is the token, never
      // navigation — which is what makes B (and LB/RB/dpad) rebindable at
      // all. The way out is START, which cancels via ui_on_escape above. An
      // armed KEYBOARD capture still sees pad nav events, but the menu
      // preamble suspends them all while any capture is armed (B backs out,
      // the same contract ESC has) — suppression lives THERE, once.
      if (g_ui_rebind >= 0 && g_ui_rebind_pad) {
        if (i != PB_START) ui_assign_bind(PAD_BTN_DEF[i].tok);
      } else if (i == PB_A)  { g_nav_kbd = 0; nav_push(NAV_OK); }
      else if (i == PB_B)  { g_nav_kbd = 0; nav_push(NAV_BACK); }
      else if (i == PB_LB) { g_nav_kbd = 0; nav_push(NAV_TABL); }
      else if (i == PB_RB) { g_nav_kbd = 0; nav_push(NAV_TABR); }
    }
    if (i == PB_START) continue;
    g_keys[KEY_PAD0 + i] = down;
    if (edge && !g_menu_open) g_pressed[KEY_PAD0 + i] = 1;
  }
  // Directional navigation: dpad OR the left stick past half deflection.
  if (g_menu_open && !(g_ui_rebind >= 0 && g_ui_rebind_pad)) {
    float sx, sy;
    pad_deadzone(g_pad.lx, g_pad.ly, g_cfg.pad[PAD_DZ_L], &sx, &sy);
    // The eat gate applies here too: this reads LEVELS, so without it a dpad
    // held across a menu close navigates the reopened menu while every other
    // eaten button correctly stays dead. The stick is deliberately not eaten —
    // it is a level device with no swallow state.
    // THE STICK MOVES ON ITS DOMINANT AXIS, the dpad on both. Now that
    // left/right navigates, a thumb shoved diagonally would otherwise step
    // TWICE from one push — down and across — landing two rows from the one the
    // player was looking at. A hat has four discrete switches and a deliberate
    // diagonal, so it keeps both; a stick's diagonal is a miss. Vertical wins
    // the exact tie (>= against >), so the split is total.
    float ah = fabsf(sx), av = fabsf(sy);
    pad_nav_dir(0, (g_pad.btn[PB_UP]    && !g_pad_eat[PB_UP])    || (sy >  0.5f && av >= ah), dt, NAV_UP);
    pad_nav_dir(1, (g_pad.btn[PB_DOWN]  && !g_pad_eat[PB_DOWN])  || (sy < -0.5f && av >= ah), dt, NAV_DOWN);
    pad_nav_dir(2, (g_pad.btn[PB_LEFT]  && !g_pad_eat[PB_LEFT])  || (sx < -0.5f && ah >  av), dt, NAV_LEFT);
    pad_nav_dir(3, (g_pad.btn[PB_RIGHT] && !g_pad_eat[PB_RIGHT]) || (sx >  0.5f && ah >  av), dt, NAV_RIGHT);
  } else {
    for (int i = 0; i < 4; i++) g_pad_nav_t[i] = g_pad_nav_hold[i] = 0;
  }
}

// ADS slows the stick by the ZOOM RATIO (tan of the half-FOVs), which is what
// keeps a flick that crosses the same screen distance the same thumb motion
// aimed and unaimed.
//
// The sniper is NOT that formula: its magnification lives in the scope's
// picture-in-picture, and the raw tan-ratio against SCOPE_FOV_DEG (0.079)
// treats the 9° image as if it filled the screen — measured against the
// rendered disc (radius ~59 of 360 virtual units) the glass's real on-screen
// magnification is ~2.3x, so the pixel-matched scale is ~0.43 and 0.079 was
// over-slowed more than 5x. PAD_SR_SCOPE_K sits deliberately below the
// pixel-matched value so scoped aim stays deliberate; it is a constant rather
// than something derived from g_scope_r because the sim must not read render
// state and the disc only varies a few percent with pose. The blend runs on
// ads² because the SR's main view does not zoom at all and the disc itself
// only fades in late — a linear blend slows the stick before anything on
// screen says why.
#define PAD_SR_SCOPE_K 0.28f
static float pad_ads_look_scale(const player_t *p) {
  float ads = p->wp.ads_s;
  if (ads <= 0) return 1.0f;
  float fov = g_cfg.fov;
  float s;
  if (p->wp.cur == WPN_SR) {
    s = 1.0f + (PAD_SR_SCOPE_K - 1.0f) * ads * ads;
  } else {
    float fov_ads = fov * g_cfg.wp[p->wp.cur][WP_ADS_ZOOM] * 0.01f;
    float f = fov + (fov_ads - fov) * ads;
    s = tanf(f * 0.5f * DEG2RAD) / tanf(fov * 0.5f * DEG2RAD);
  }
  // The player's ADS SENS factor rides on top of the zoom ratio, blended by
  // the same ramp so hip aim is never touched.
  return s * (1.0f + (g_cfg.pad[PAD_ADS_SENS] - 1.0f) * ads);
}

// --- Aim assist (pad only, toggleable, FRICTION ONLY) ------------------------
// Stick look slows inside a bubble around a target — what lets a thumb stop
// ON a 12 m/s strafer instead of skating past it. Rotation and the ADS snap
// were deliberately removed (2026-08-11): the assist never generates view
// motion of its own. The three tuning knobs live in PAD_DEF (PAD_AS_*, the
// DEV > ASSIST tab) — see the table's comment for their semantics. Hard
// rules, each load-bearing: no input, no assist (friction multiplies an
// input-proportional step — an idle pad never tracks anything); LOS-checked
// (no slowdown betraying someone behind a wall); pad only by construction
// (it lives inside pad_look — the mouse path cannot reach it).

// The look-rate multiplier at angular offset ang inside a cone of size eng,
// dist metres from the target: 1 - STRENGTH * ease(ang) * fade(dist). The
// ease is the baked 1 - smoothstep^2 (full at the centre, none at the edge);
// the fade is linear to zero at DIST. It lives HERE because pad_look applies
// it and padstat prints it, and the two copies had already drifted once.
static float pad_assist_friction(float ang, float eng, float dist) {
  float fall = smooth01(ang / eng);
  float ds = 1.0f - f_clamp(dist / g_cfg.pad[PAD_AS_DIST], 0.0f, 1.0f);
  return 1.0f - g_cfg.pad[PAD_AS_FRIC] * (1.0f - fall * fall) * ds;
}

// Nearest-by-angle live entity inside the FIXED engage cone (BUBBLE degrees —
// v2 dropped the apparent-size cone: a fixed angle is the unit the tuner
// thinks in, and the linear DIST fade owns the far-range weakening). Returns
// the entity id or -1; outputs the aim point, the angular offset, the cone
// and the target's distance.
static int pad_assist_target(const player_t *p, v3 *tp_out, float *ang_out,
                             float *eng_out, float *dist_out) {
  v3 eye = player_eye_pos(p);
  float cp = cosf(p->pitch);
  v3 fwd = {{sinf(p->yaw) * cp, sinf(p->pitch), -cosf(p->yaw) * cp}};
  float eng = g_cfg.pad[PAD_AS_BUBBLE] * DEG2RAD;
  int best = -1;
  // ENT_FOREACH, returning an ENTITY id (P1-56): iterating g_bots alone gave
  // a pad player on the deploy handheld aim assist on bots and NONE on the
  // humans he actually fights — worse than no help, it actively pulls away.
  ENT_FOREACH(e) {
    if (e == g_local_ent || !ent_alive(e)) continue;
    v3 tp = ent_aimpoint(e);
    v3 d = v3_sub(tp, eye);
    float dist = v3_len(d);
    if (dist < 1.0f || dist > g_cfg.pad[PAD_AS_DIST]) continue;
    float ang = acosf(f_clamp(v3_dot(fwd, d) / dist, -1.0f, 1.0f));
    if (ang > eng) continue;
    if (best >= 0 && ang >= *ang_out) continue;
    if (!los_clear(eye, tp)) continue;   // the trace is the expensive test: last
    best = e;
    *tp_out = tp;
    *ang_out = ang;
    *eng_out = eng;
    *dist_out = dist;
  }
  return best;
}

// Right-stick look, applied per RENDER frame like the mouse (lowest latency;
// rate * dt makes the integral frame-rate independent). The curve acts on the
// deflection magnitude and the result is scaled back along the deflection
// direction, so the curve shapes SPEED only and never bends the aim path.
static void pad_look(player_t *p, float dt) {
  float x, y;
  float m = pad_deadzone(g_pad.rx, g_pad.ry, g_cfg.pad[PAD_DZ_R], &x, &y);
  if (m <= 0) return;
  // FRICTION has no deflection gate: it multiplies an input-proportional step
  // (zero stick times anything is zero), so "no input, no assist" holds by
  // construction — and gating it carved a felt cliff into the exact
  // micro-correction band it exists for (measured: 0.01 of stick travel
  // flipped an 84% authority drop on and off).
  float assist = 1.0f;
  v3 tp = {{0, 0, 0}};                       // out-params; the inits also quiet
  float ang = 0.0f, eng = 1.0f, dst = 0.0f;  // MinGW's inlined-flow analysis
  if (g_cfg.pad_assist && p->alive &&
      pad_assist_target(p, &tp, &ang, &eng, &dst) >= 0)
    assist = pad_assist_friction(ang, eng, dst);
  float k = pad_curve_apply(m, g_cfg.pad_curve) / m *
            pad_ads_look_scale(p) * assist * dt * DEG2RAD;
  float ydir = g_cfg.pad_inv_y ? -1.0f : 1.0f;
  p->yaw += x * k * g_cfg.pad[PAD_SENS] * PAD_SENS_DEG_X;
  float dp = y * k * g_cfg.pad[PAD_SENS] * PAD_SENS_DEG_Y * ydir;
  p->pitch = f_clamp(p->pitch + dp, -PITCH_MAX, PITCH_MAX);
}

// Listener update + event drain, once per frame: the ears travel with the
// peek, then every event of the frame's ticks is routed into the mixer. TWO
// callers — app_frame (both windowed backends) and the harness's sim_advance
// (mixdown/sfxlog) — and they must be the same code, or a listener-basis
// change desyncs every audio proof from what the player hears.
static void audio_frame_drain(const player_t *p) {
  // The listener IS g_local_ent; a dedicated server has none and must not
  // route audio at all (the ring is drained per tick there instead).
  if (g_local_ent < 0) { g_num_events = 0; return; }
  g_listen = player_eye_pos(p);
  g_listen_r = (v3){{cosf(p->yaw), 0, sinf(p->yaw)}};
  for (int e = 0; e < g_num_events; e++) audio_event(&g_events[e]);
  g_num_events = 0;
}

// Advances the sim by `dt` seconds and renders one frame; the backend swaps.
// Returns the number of ticks that ran, which is what tells a backend whether
// its press latches were consumed — clearing them when NO tick ran would drop
// a click that arrived inside a sub-tick gap, and that is the whole reason
// held_first exists.
// WHICH player_t the camera, audio and viewmodel read. In singleplayer and
// while a join is still forming this is g_players[0] (the local match); once a
// networked session has its first authoritative fix, g_local_ent is the
// server's id for us and the local player lives in that human slot.
static player_t *local_player(void) {
  int h = g_local_ent > MAX_BOTS ? g_local_ent - MAX_BOTS : 0;
  return &g_players[h];
}

// Solve the pose for every on-roster figure from its current (snapshot-driven
// or predicted) state, so scene_build can draw them. Online replaces the AI +
// present halves the SP tick runs; foreign entities move by snapshot, not by
// local AI. Once per rendered frame.
// ONLINE DEATHS HAVE NO SIM TO WATCH THEM. Offline, ent_damage launches the
// ragdoll and player_present_tick hands it to the corpse pool on the same
// tick. Online neither runs on the client — client_online_tick REPLACES
// player_tick — so a killed player was simply dropped from the draw list
// (scene_build skips !alive, and no corpse was ever created) and vanished
// mid-stride: no topple, no slide, no dissolve. The corpse is presentation,
// so it is built locally exactly as it is offline; only the trigger differs,
// an alive flag going 0 in a snapshot instead of ent_damage's own branch.
static void client_death_launch(rag_t *r, v3 pos, v3 vel, int grounded, int ent) {
  const typeof(g_wound[0]) *w = &g_wound[ent];
  // A death whose EV_HIT* was lost (or arrived outside the resend window)
  // still has to look like a death, so the fallback is a plausible round from
  // in front at chest height rather than a body that drops straight down.
  int fresh = w->tick > 0 && g_tick - w->tick <= 60;
  v3 at   = fresh ? w->at   : v3_add(pos, (v3){{0.0f, 1.05f, 0.0f}});
  v3 from = fresh ? w->from : v3_add(pos, (v3){{0.0f, 1.35f, 2.5f}});
  rag_launch(r, pos, vel, grounded, from, at, fresh ? w->dmg : 34, w->hs);
}

static void client_anim_all(void) {
  for (int h = 0; h < MAX_HUMANS; h++) {
    if (!g_humans_on[h]) continue;
    player_t *p = &g_players[h];
    int ent = ent_of_human(h);
    if (g_cl_alive_prev[ent] && !p->alive) {
      client_death_launch(&g_player_rag[h], p->pos, p->vel, p->grounded, ent);
      g_player_corpse_q[h] = 1;
    }
    g_cl_alive_prev[ent] = p->alive ? 1 : 0;
    if (!p->alive) { player_present_tick(p, ent); continue; }   // THE one dead
                                                                // branch: pose
                                                                // freeze + the
                                                                // corpse handover
    anim_tick(&g_player_anim[h], p->pos, p->vel, p->yaw, p->pitch, p->grounded,
              p->crouched && !p->sliding, p->tuck_y, p->sliding, p->wp.ads_s,
              p->lean, 0, ent_of_human(h));
  }
  for (int b = 0; b < g_match.nbots; b++) {
    bot_t *bt = &g_bots[b];
    if (!bt->active) continue;
    // A RESPAWN HAS TO UNDO THE DEATH, and offline nothing has to say so: a
    // bot respawns through bot_spawn, whose `memset(b, 0, sizeof *b)` clears
    // b->rag and b->topple along with everything else. The client never runs
    // bot_spawn — it is handed a live entity by the snapshot — so the ragdoll
    // state SURVIVED the respawn, and scene_draw_bot feeds b->rag through
    // rag_death for a LIVING bot too: the respawned bot stood back up still
    // wearing its own collapse. Humans never hit this because scene_build
    // passes NULL for the death_t while alive.
    if (bt->alive && g_cl_body[b + 1]) {
      bt->rag = (rag_t){0};
      bt->topple = bt->prev_topple = 0.0f;
      g_cl_body[b + 1] = 0;                  // the body goes back to the server
      anim_reset(&bt->anim, bt->pos, bt->yaw);
    }
    if (!bt->alive && !g_cl_body[b + 1]) {
      client_death_launch(&bt->rag, bt->pos, bt->vel, bt->grounded, b + 1);
      g_cl_body[b + 1] = 1;                  // ...and this client owns it now
    }
    if (!bt->alive) {   // scene_draw_bot renders b->rag, so a bot ragdolls as
      bt->anim.prev_pose = bt->anim.pose;   // an ENTITY here, as it does offline
      // ...and offline `bot_tick` OPENS with the prev_ pair and closes the
      // dead branch with grounded. Those four lines are the whole difference
      // and none of them were here. prev_topple stayed 0 while topple ran to
      // 1, so rag_death's slack — the collapse blend fig_death_pose keys off —
      // came out as the raw sub-tick alpha and the body stood up and fell over
      // again on every rendered frame. prev_pos was worse: nothing advances it
      // for a DEAD bot (net_cl_extrapolate skips them), so the render lerped
      // the corpse between its ragdoll position and a point it had left
      // seconds earlier — measured live against a dedicated server, a frozen
      // 2.3 m swing every frame — and rag_advance's own `dp` carried that same
      // stale delta straight into the POSE joints, which is why the pelvis
      // came out metres under the floor.
      bt->prev_pos = bt->pos;
      bt->prev_topple = bt->topple;
      rag_advance(&bt->rag, &bt->pos, bt->prev_pos, &bt->topple, &bt->anim);
      bt->grounded = bt->rag.grounded;
      continue;
    }
    anim_tick(&bt->anim, bt->pos, bt->vel, bt->yaw, bt->pitch, bt->grounded,
              bt->crouch, 0.0f, bt->sliding, bt->wp.ads_s, 0.0f, 0, b + 1);
  }
}

// One online client tick: assemble the local input, predict + send it. Foreign
// state has already been applied by net_cl_poll before the tick loop.
static void client_online_tick(player_t *p, const int held[IN_COUNT]) {
  float wx = 0.0f, wy = 0.0f;
  if (g_pad.connected && p->alive && !g_menu_open) {
    float mx, my;
    if (pad_deadzone(g_pad.lx, g_pad.ly, g_cfg.pad[PAD_DZ_L], &mx, &my) > 0) { wx = mx; wy = my; }
  }
  // Presentation-only REDEPLOY countdown: the sim's respawn_t is server state
  // (set on the snapshot's death edge); the real respawn arrives as an alive
  // flip, never from this counter reaching zero.
  if (!p->alive && p->respawn_t > 0) p->respawn_t--;
  // The CLIENT half of the match-over countdown, for the same reason: this
  // fork replaces player_tick, so match_tick — where the server runs the same
  // one copy — never executes here. Display only; it cannot request a
  // restart (see match_over_countdown).
  match_over_countdown();
  net_cl_tick(held, wx, wy, p->yaw, p->pitch);
  net_cl_extrapolate();        // foreign entities advance at tick cadence too
  rescue_client_tick();
}

// Begin a join: the LOCAL match keeps running (the caller is already in
// one), the handshake is driven each frame by app_frame's poll below, and the
// switch to the server happens only when its first snapshot lands. Returns 0 if
// the socket could not even open.
static int start_connect(const char *host, int port) {
  uint32_t ip = net_resolve(host);
  if (!ip) return 0;
  g_net_secret = 0;                          // client uses the server's cookie
  return net_cl_connect(ip, (uint16_t)port, g_cfg.name);
}

// --connect HOST[:PORT]: begin a join while the local match keeps
// running. ONE parse for both native mains — a main that never set
// g_boot_connect is a no-op here.
static void boot_connect_kick(void) {
  if (!g_boot_connect || !g_boot_connect[0]) return;
  char host[64]; int port = g_cfg.mp_port;
  snprintf(host, sizeof host, "%s", g_boot_connect);
  char *colon = strchr(host, ':');
  if (colon) { *colon = '\0'; port = atoi(colon + 1); }
  start_connect(host, port);
}

// LEAVING AN ONLINE MATCH IS A TEARDOWN, NOT JUST A DISCONNECT (fixed
// 2026-08-18). net_cl_apply_ent switches a human slot ON for every foreign
// player a snapshot mentions, and NOTHING ever switched them back off:
// net_cl_disconnect wipes the transport, and match_restart deliberately
// preserves the roster (it must — a server's players have to survive a
// rematch). So after LEAVE MATCH every remote player was still in the local
// match: respawned by match_restart, walked by ent_next, shootable, rendered,
// and on the scoreboard — one of them wearing YOUR name, because the join
// writes it into the slot the SERVER gave you, not into slot 0.
//
// The ENTRY into an online match was always clean (the snapshot prunes the
// roster, the score rows are explicitly zeroed); only the exit never got the
// same treatment. Three call sites used to inline the same four lines and all
// three carried the bug, so this is now the ONE copy — the file's governing
// rule applied to a state transition rather than to an algorithm.
static void leave_match(void) {
  if (g_cl.state != NC_OFF) net_cl_disconnect();
  if (g_mp_hosting) mp_host_stop();     // the host leaving ends the game
  g_online = 0;
  g_local_ent = 0;
  // Everything above slot 0 belonged to the SERVER's roster. Slot 0 is the
  // local player and the only human a singleplayer match may know about.
  // ent_name(0) reads g_cfg.name directly, so slot 0 needs no name written
  // here — clearing 1.. is what removes the ghost wearing the player's name.
  for (int h = 1; h < MAX_HUMANS; h++) {
    g_humans_on[h] = 0;
    g_human_name[h][0] = '\0';
    memset(&g_players[h], 0, sizeof g_players[h]);
    memset(&g_player_anim[h], 0, sizeof g_player_anim[h]);
    memset(&g_player_rag[h], 0, sizeof g_player_rag[h]);
    g_player_corpse_q[h] = 0;
    memset(g_human_held[h], 0, sizeof g_human_held[h]);
    g_human_wish[h][0] = g_human_wish[h][1] = 0.0f;
    g_human_move[h] = 1;   // back to the singleplayer default (one input =
    g_human_view[h] = -1;  // one move is a networked rule; locally nothing
                           // gates a move)
  }
  g_humans_on[0] = 1;                    // the local player exists again
  // Scoreboard furniture is cleared HERE, not only in net_cl_disconnect:
  // that call sits behind an `if (state != NC_OFF)` guard, so a session whose
  // transport had already died left the last server's ping column standing in
  // the local match. The teardown must not depend on how the link ended.
  memset(g_net_ping, 0, sizeof g_net_ping);
  g_net_fraglimit = 0;
  g_net_rounds = 0;
  memset(g_spawn_seq, 0, sizeof g_spawn_seq);
  memset(g_cl_body, 0, sizeof g_cl_body);   // no body of the server's survives
  memset(g_cl_alive_prev, 0, sizeof g_cl_alive_prev);
  match_scores_clear();
  match_restart();     // fresh local arena; it also runs vfx_match_reset,
}                      // which clears the corpses of players who are gone

// --- THE PERF OVERLAY (Settings > HUD > PERF STATS) ------------------------
// The numbers this file's comments quote come from llvmpipe in a container; the
// target is a 120 Hz handheld, and nothing measured it THERE. This does, on the
// device, with no tooling: frame time (average and the worst of the last
// second), CPU split into sim ticks and render submission, and GPU time from
// GL_TIME_ELAPSED queries in a ring of four, read only once their result is
// available so the counter never stalls the pipeline it is measuring.
static double perf_now(void) {
#ifdef _WIN32
  static LARGE_INTEGER f;
  LARGE_INTEGER c;
  if (!f.QuadPart) QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return (double)c.QuadPart / (double)f.QuadPart;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}
#define PERF_N 120
static struct {
  float frame[PERF_N], sim[PERF_N], cpu[PERF_N], gpu[PERF_N];
  int at, n, gpu_n;
  GLuint q[4];
  int q_live[4], q_head, q_init;
} g_perf;
static void perf_gpu_begin(void) {
  if (!g_cfg.perf || !gl_gen_queries || !gl_begin_query || !gl_end_query ||
      !gl_query_iv || !gl_query_u64) return;
  if (!g_perf.q_init) { gl_gen_queries(4, g_perf.q); g_perf.q_init = 1; }
  // Harvest every finished query first, oldest first.
  for (int k = 0; k < 4; k++) {
    int i = (g_perf.q_head + k) % 4;
    if (!g_perf.q_live[i]) continue;
    GLint ok = 0;
    gl_query_iv(g_perf.q[i], GL_QUERY_RESULT_AVAILABLE_Q, &ok);
    if (!ok) continue;
    GLuint64 ns = 0;
    gl_query_u64(g_perf.q[i], GL_QUERY_RESULT_Q, &ns);
    g_perf.gpu[g_perf.gpu_n++ % PERF_N] = (float)((double)ns * 1e-6);
    g_perf.q_live[i] = 0;
  }
  int i = g_perf.q_head;
  if (g_perf.q_live[i]) return;          // ring full: skip this frame's sample
  gl_begin_query(GL_TIME_ELAPSED_Q, g_perf.q[i]);
  g_perf.q_live[i] = 2;                  // 2 = open, 1 = waiting for result
}
static void perf_gpu_end(void) {
  int i = g_perf.q_head;
  if (g_perf.q_live[i] != 2) return;
  gl_end_query(GL_TIME_ELAPSED_Q);
  g_perf.q_live[i] = 1;
  g_perf.q_head = (i + 1) % 4;
}
static void perf_push(float frame_ms, float sim_ms, float cpu_ms) {
  int i = g_perf.at;
  g_perf.frame[i] = frame_ms; g_perf.sim[i] = sim_ms; g_perf.cpu[i] = cpu_ms;
  g_perf.at = (i + 1) % PERF_N;
  if (g_perf.n < PERF_N) g_perf.n++;
}
static void perf_draw(void) {
  if (!g_cfg.perf || !g_perf.n) return;
  float fa = 0, fm = 0, sa = 0, ca = 0, ga = 0;
  for (int i = 0; i < g_perf.n; i++) {
    fa += g_perf.frame[i]; sa += g_perf.sim[i]; ca += g_perf.cpu[i];
    if (g_perf.frame[i] > fm) fm = g_perf.frame[i];
  }
  fa /= (float)g_perf.n; sa /= (float)g_perf.n; ca /= (float)g_perf.n;
  int gn = g_perf.gpu_n < PERF_N ? g_perf.gpu_n : PERF_N;
  for (int i = 0; i < gn; i++) ga += g_perf.gpu[i];
  if (gn) ga /= (float)gn;
  char l1[64], l2[64], l3[64];
  snprintf(l1, sizeof l1, "FRAME %.2f MS  WORST %.2f", (double)fa, (double)fm);
  snprintf(l2, sizeof l2, "CPU SIM %.2f  RENDER %.2f", (double)sa, (double)ca);
  if (gn) snprintf(l3, sizeof l3, "GPU %.2f MS  BUDGET 8.33", (double)ga);
  else    snprintf(l3, sizeof l3, "GPU N/A  BUDGET 8.33");
  float x = 8, y = 20;
  ui_rect(x - 4, y - 3, 150, 40, 0.0f, 0.0f, 0.0f, 0.55f);
  // Over budget reads in the threat colour: the line a tester is looking for.
  int over = fa > 8.34f || ga > 8.34f;
  ui_text_sh(l1, x, y, 0.75f, TXT_REG, 0.6f, over ? C4(C_THREAT, 1) : C4(C_BONE, 0.95f));
  ui_text_sh(l2, x, y + 12, 0.75f, TXT_REG, 0.6f, C4(C_HAZE, 0.95f));
  ui_text_sh(l3, x, y + 24, 0.75f, TXT_REG, 0.6f, C4(C_HAZE, 0.95f));
}

static double g_perf_tr;   // when this frame's render submission began
static int app_frame_body(app_t *a, player_t *p, double dt, const frame_input_t *in);
static int app_frame(app_t *a, player_t *p, double dt, const frame_input_t *in) {
  if (!g_cfg.perf) return app_frame_body(a, p, dt, in);
  double t0 = perf_now();
  g_perf_tr = 0.0;
  int r = app_frame_body(a, p, dt, in);
  double t1 = perf_now();
  double tr = g_perf_tr > 0.0 ? g_perf_tr : t1;
  perf_push((float)(dt * 1000.0), (float)((tr - t0) * 1000.0), (float)((t1 - tr) * 1000.0));
  return r;
}
static int app_frame_body(app_t *a, player_t *p, double dt, const frame_input_t *in) {
  if (dt > 0.1) dt = 0.1;  // hitch clamp: never spiral the sim
  // Presentation motion clock: real dt while playing; the harness never gets
  // here for uiframe/shot, so those keep the fixed 1/60 default and stay
  // deterministic. Purely presentation — the sim never reads it.
  g_ui_dt = dt > 0.05 ? 0.05f : (float)dt;

  // Drive an in-progress handshake even while still playing locally:
  // the switch to online happens on the first snapshot, inside net_cl_poll.
  if (!g_online && g_cl.state != NC_OFF && g_cl.state != NC_FAILED) net_cl_poll();

  // ONLINE FORK: the local player is predicted and sent, foreign entities
  // come from snapshots, and the AI/present halves of the SP tick do not run.
  // A dropped connection degrades to the local match. Gated on
  // g_online, so the singleplayer path below is byte-identical and untouched.
  if (g_online) {
    net_cl_poll();
    if (g_cl.state == NC_FAILED) {          // server gone: fall back to local
      leave_match();                        // the ONE teardown (see there)
      // fall through to the singleplayer path this frame
    } else {
      p = local_player();
      if (!g_menu_open) { player_look(p, in->mouse_dx, in->mouse_dy, g_cfg.sense); pad_look(p, (float)dt); }
      // CLOCK NUDGE: scale the accumulator so the SERVER-side input
      // queue rests at NET_QDEPTH_TARGET. Crystals disagree by ~100 ppm and
      // jitter parks the queue wherever the last burst left it; unregulated,
      // the depth either drains to starvation (missed moves) or climbs into
      // the backlog trim (discarded moves) — both are visible corrections.
      // +-0.4%/tick-of-error, capped at -3%/+1%: a 10-tick spike drains in
      // ~3 s, and at most 1% fast-forward keeps starvation recovery gentle.
      // Purely a PACING change — TICK_DT and the sim never see it.
      {
        float e = NET_QDEPTH_TARGET - g_cl.qdepth_ema;
        a->acc += dt * (1.0 + (double)f_clamp(e * 0.004f, -0.03f, 0.01f));
      }
      int nt = 0;
      while (a->acc >= TICK_DT) {
        g_tick++;
        client_online_tick(p, nt ? in->held : in->held_first);
        // VFX ARE TICK-DRIVEN, so an online tick has to run them too. Every
        // lifetime in this file is counted in ticks — tracers, muzzle flashes,
        // impact sparks, decals, corpses, the hitmarker and the hit-stop
        // window — and this fork ran none of them: effects spawned from
        // snapshot events and then stood still for the rest of the match.
        // It stays INSIDE the tick loop and BEFORE the present half below,
        // which is the same order the singleplayer world tick uses and the
        // reason corpse_t.born exists.
        vfx_tick();
        // THE PRESENT HALF IS PART OF THE TICK, not of the frame. It used to
        // sit below this loop and run exactly once per RENDERED frame, but
        // everything it drives is a fixed-TICK_DT solver: anim_tick's foot
        // planting and spring followers, rag_advance's integrator and its
        // 1/BOT_TOPPLE_TICKS collapse ramp. At 60 fps a body fell at half
        // gravity and walked at half cadence; at 240 fps at double. Worse, it
        // stepped prev_pose/prev_topple on the FRAME clock while the net layer
        // stepped prev_pos on the TICK clock, so the render lerp mixed two
        // clocks and the figure jittered. Here it is the same position the
        // singleplayer world tick gives player_present_tick: after vfx_tick,
        // inside the loop, zero or more times per frame.
        client_anim_all();
        a->acc -= TICK_DT;
        nt++;
      }
      audio_frame_drain(p);
      a->frames++; a->fps_window += dt;
      if (a->fps_window >= 0.25) { a->fps = (int)((double)a->frames / a->fps_window + 0.5); a->frames = 0; a->fps_window = 0; }
      g_perf_tr = perf_now(); perf_gpu_begin();
      render_frame(p, (float)(a->acc / TICK_DT), a->fps, NULL);
      perf_gpu_end();
      return nt;
    }
  }

  // Mouse look is applied per RENDER frame (lowest latency); the fixed-step
  // sim below then uses the current yaw for its wish direction.
  if (!g_menu_open) {
    player_look(p, in->mouse_dx, in->mouse_dy, g_cfg.sense);
    pad_look(p, (float)dt);
  }

  // THE MENU IS THE PAUSE (this is a singleplayer game): while it is open the
  // accumulator FREEZES — no ticks run, and the frozen sub-tick remainder
  // holds the render alpha, so the world stands as a stable freeze-frame (all
  // prev/cur interpolation twins and every tick-side VFX timer hold together).
  // Unpause needs no drain and cannot burst: both backends update their clock
  // every iteration regardless of menu state, so the first live frame's dt is
  // one frame's worth. The HARNESS is untouched by construction — its `wait`
  // drives sim_advance directly and never enters app_frame (only the
  // `appframe` proof command does, which is how the pause itself is tested) —
  // so `menu; wait N` still advances the sim in every existing proof script.
  // HIT-STOP scales the accumulator, so it changes WHEN ticks run and never
  // which. It is decremented once per RENDERED frame rather than per tick, for
  // the same reason the muzzle flash's bright frame is a render latch: at a low
  // fps_cap a tick-counted window would be spent before a single frame showed
  // it. That also makes the wall-clock length frame-rate dependent by design —
  // 5 frames of slow motion is the same felt beat at 30 fps and at 240.
  // session_pauses(), not g_menu_open: a NETWORKED session keeps
  // ticking with the menu open (you stand there and get shot) and never scales
  // with hit-stop. The two conditions share one predicate so tick and label
  // cannot disagree.
  if (!session_pauses())
    a->acc += dt * (g_hitstop > 0 && !g_online ? (double)HITSTOP_SCALE
                  : g_home ? (double)home_timescale() : 1.0);
  if (g_hitstop > 0 && !session_pauses()) g_hitstop--;
  int ticks = 0;
  while (a->acc >= TICK_DT) {
    player_tick(p, ticks ? in->held : in->held_first);
    a->acc -= TICK_DT;
    ticks++;
  }

  audio_frame_drain(p);

  a->frames++;
  a->fps_window += dt;
  if (a->fps_window >= 0.25) {
    a->fps = (int)((double)a->frames / a->fps_window + 0.5);
    a->frames = 0;
    a->fps_window = 0;
  }

  g_perf_tr = perf_now(); perf_gpu_begin();
  render_frame(p, (float)(a->acc / TICK_DT), a->fps, NULL);
  perf_gpu_end();
  return ticks;
}

// One frame of the shared input glue plus the frame itself: pad frame step,
// bind resolution, the held/held_first snapshot, app_frame, and the
// latch-clear contract — clear g_pressed ONLY when a tick consumed it, or a
// tap shorter than a frame is dropped (the exact trap the app_frame comment
// documents). This block was line-identical in both backend loops; the mouse
// deltas stay parameters because their accumulators are backend-owned (filled
// by wndproc / the XI2 pump). Returns 1 when the session should end.
static int app_pump(app_t *a, player_t *p, double dt, float mdx, float mdy) {
  int binds[IN_COUNT], pbinds[IN_COUNT];
  pad_sync((float)dt);
  resolve_binds(binds, pbinds);
  frame_input_t in = {.mouse_dx = mdx, .mouse_dy = mdy};
  frame_input_keys(&in, binds, pbinds);
  if (app_frame(a, p, dt, &in))
    memset(g_pressed, 0, sizeof g_pressed);  // a tick consumed the latches
  return g_ui_quit;
}

// Cap the driver's render-ahead queue at one frame: block until the PREVIOUS
// frame's GPU work is done, then fence this one. Called immediately after the
// buffer swap. Without it a cheap CPU frame lets vsynced drivers buffer 2-3
// frames of input latency.
static void app_sync(app_t *a) {
  if (a->fence) {
    glClientWaitSync(a->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 100000000ull);
    glDeleteSync(a->fence);
  }
  a->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

