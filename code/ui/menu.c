// code/ui/menu.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Settings menu — immediate mode: layout, hit-testing and drawing in one pass
// per frame. The platform feeds cursor position (real px), left-button state,
// and captured key tokens for rebinding; everything else lives here.
// ---------------------------------------------------------------------------

// GRATICULE palette, cooled (display space — UI colors bypass the tonemap).
// Monochrome discipline: near-black ink panels (ui_panel owns that color),
// cool white BONE data, and exactly three signal hues — BLAZE strictly for
// self/CTA/focus, SIGNAL for remote humans, THREAT for danger. Everything
// else is white at an alpha.
#define C_BONE   0.941f, 0.957f, 0.973f
#define C_HAZE   0.541f, 0.580f, 0.643f
#define C_BLAZE  1.000f, 0.549f, 0.102f
#define C_SIGNAL 0.310f, 0.765f, 0.910f
#define C_THREAT 1.000f, 0.294f, 0.227f
// Legacy role names, kept as the one copy every widget already reads:
#define C_TEXT   C_BONE
#define C_DIM    C_HAZE
#define C_ACCENT C_BLAZE
// Wrap a comma-triple palette macro + alpha into the float[4] the _c
// primitives take: C4(C_BLAZE, 0.8f).
#define C4(...) ((float[4]){__VA_ARGS__})
// TEAM COLOURS, the HUD's half of the kit: ALPHA red, BRAVO blue, bright
// enough to read as text on sky or wall. TEAM_RGB(t) expands to three floats.
#define TEAM_RGB(t) ((t) ? 0.345f : 1.000f), ((t) ? 0.620f : 0.345f), ((t) ? 1.000f : 0.290f)

static int   g_menu_open;
static int   g_ui_backdrop;   // frosted blur layer was painted this frame
[[maybe_unused]] static const char *g_connect_host;   // --connect HOST[:PORT] (CLI, Linux)
static const char *g_boot_connect;   // native_main kicks the join after start
static int  start_connect(const char *host, int port);   // begin a join
static void leave_match(void);                            // drop back to local
// ONE predicate for "does this session pause?": the open menu pauses a
// windowed SINGLEPLAYER session, but a networked one does NOT (you would stand
// there being shot) and never scales with hit-stop. Tick decision AND label
// read this same function so the two cannot drift.
//
// It carries no g_headless term, and that is the fix for a regression rather
// than an omission. The harness never pauses because its `wait` drives
// sim_advance directly and never enters app_frame at all — nothing here has to
// arrange that. The one harness path that DOES enter app_frame is the
// `appframe` command, which exists precisely to prove the pause; exempting it
// made the proof assert the opposite of the contract (`menu; appframe 3`
// printed ticks=3 against a documented 0). The two things the term really
// protected have their own guards where they belong: the PAUSED/LIVE label
// below and ui_audio_pause both check g_headless themselves.
static int session_pauses(void) {
  if (g_online) return 0;         // a live session is never frozen
  // ...and neither is the BOOT SCREEN, which is a menu that is not a pause: it
  // is showing the player a live camera on a live figure, and every beat in it
  // — the swap timeline, the weapon coming to the eye, the idle sway — is a
  // fixed-TICK_DT solver. Freezing here would leave a moving camera over a
  // photograph. The label follows the same predicate, so no PAUSED chip
  // appears over it, which is correct: nothing is paused.
  return g_menu_open && !g_home;
}
static int   g_ui_rebind = -1;   // action currently being rebound, -1 = none
static int   g_ui_rebind_pad;    // ...and which bind table that row edits
static int   g_ui_quit;          // set by the QUIT button
// TWO-PRESS DESTRUCTIVE ROWS, ONE LATCH, AND IT REMEMBERS WHICH ROW. A control
// that ends something the player cannot get back — the process, the connection,
// the round — asks twice. The latch is an enum rather than a flag because QUIT
// and LEAVE MATCH share the rail: with a bool, arming one and then pressing the
// other would read as a confirmation. Any navigation clears it, and ui_route
// clears it on every page change, so an armed row never survives being walked
// away from.
typedef enum { UI_ARM_NONE, UI_ARM_QUIT, UI_ARM_LEAVE, UI_ARM_RESTART } ui_arm_t;
static ui_arm_t g_ui_arm;
static int   g_ui_root_focus;  // rail row to return to, THIS menu session only
// 1 = this row was already armed, i.e. GO. 0 = the press only armed it.
static int ui_arm_take(ui_arm_t which) {
  if (g_ui_arm == which) { g_ui_arm = UI_ARM_NONE; return 1; }
  g_ui_arm = which;
  return 0;
}
static float g_ui_cx, g_ui_cy;   // cursor in real framebuffer px
static int   g_ui_mdown;         // left button level state
static int   g_ui_drag;          // active slider, 0 = none

// --- Pad navigation. pad_sync (or the harness's `nav`) pushes events; ui_menu
// consumes them in its preamble. Focus is a row index in LAYOUT order — every
// interactive row registers itself once per frame via ui_focus_row, so the
// index is stable exactly as long as the layout is, and a page change resets it.
//
// THE INDEX IS THE IDENTITY; THE MOVE IS GEOMETRIC. Stepping the index by one
// is only navigation while the layout is one column, and the entry screen is
// not: LEAVE|MATCH and SETTINGS|CONTROLS sit side by side, so ±1 walked them in
// a snake and left/right — a direction the player is already holding — reached
// nothing at all. Every row hands ui_focus_row its rect anyway, so the step is
// a nearest-rect-in-that-direction search over the rects of the LAST frame (the
// search runs in the preamble, before this frame's layout, which is the same
// place g_ui_nfocus_last is read for). The alternative is a per-page table of
// neighbours, i.e. a second copy of the layout, and it drifts the first time a
// row moves.
enum { NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT, NAV_OK, NAV_BACK, NAV_TABL, NAV_TABR };
static int g_nav_q[16], g_nav_n;
static int g_nav_kbd;   // last nav event came from the keyboard: the page hint
                        // then speaks ARROWS/ENTER/ESC instead of A/B
static void nav_push(int ev) {
  if (g_nav_n < (int)(sizeof g_nav_q / sizeof g_nav_q[0])) g_nav_q[g_nav_n++] = ev;
}
static int g_ui_focus;         // focused row within the active page
static int g_ui_nfocus;        // rows registered this frame...
static int g_ui_nfocus_last;   // ...and last frame (wrap-around runs pre-layout)
static float g_focus_sx, g_focus_sy, g_focus_sw, g_focus_sh;  // ring glide state
static int g_ui_pad_mode;      // last menu input was the pad: focus ring, no cursor
static int g_nav_act = -1;     // OK/LEFT/RIGHT pending for the focused row

// One row's geometry as the move sees it. `lr` is the one thing a rect cannot
// say: whether this row SPENDS left/right on its own value (a slider, a cycle,
// a segmented selector) or leaves it to the navigation (a tile, a button, a
// bind pill). Nothing in this menu is both value-bearing and side-by-side, so
// the two readings of that axis never compete over the same row.
#define UI_MAX_FOCUS 64   // the longest page is 18 rows; a row past this keeps
                          // its focus id and simply contributes no geometry
typedef struct { float x, y, w, h; int lr; } focus_rect_t;
static focus_rect_t g_focus_r[UI_MAX_FOCUS];
static int g_focus_rn;        // rects recorded LAST frame (the move is pre-layout)
static float g_nav_ax = -1e9f, g_nav_ay;   // the VIRTUAL CURSOR: a vertical move
                                           // keeps the x it was given and a
                                           // horizontal one keeps the y, so
                                           // walking down a two-column screen
                                           // and back up returns to the column
                                           // you left instead of to whichever
                                           // centre happens to be nearer.
                                           // < -1e8 = unset, adopt the row's own
static void nav_anchor_reset(void) { g_nav_ax = -1e9f; }

// Distance from `p` to the interval [lo, hi], 0 inside it. The perpendicular
// term of the score is an INTERVAL distance and not a centre distance because a
// full-width row CONTAINS the anchor: scored by centres, PLAY and QUIT would be
// penalised for being wide, which is the opposite of what their width means.
static float nav_gap(float p, float lo, float hi) {
  return p < lo ? lo - p : p > hi ? p - hi : 0.0f;
}

// Step the focus one row in `dir` (NAV_UP..NAV_RIGHT). Two tiers, and the
// second one is VERTICAL ONLY on purpose:
//   1. rows that overlap the current row on the perpendicular axis — the
//      aligned case, which is every page in this menu;
//   2. for up/down, the nearest row weighted by how far off-axis it is, so a
//      future staggered layout can never strand a row out of reach.
// A sideways step gets no such fallback: no overlap means nothing is beside the
// row, and a weighted LEFT would then fly across the screen into another row's
// business — reaching that row is what up/down is for. When up/down finds
// nothing it WRAPS to the far end, which is what the ±1 index did.
static void nav_move(int dir) {
  int n = g_focus_rn;
  if (n <= 0) return;
  if (g_ui_focus < 0 || g_ui_focus >= n) { g_ui_focus = 0; return; }
  int vert = dir == NAV_UP || dir == NAV_DOWN;
  float sgn = (dir == NAV_DOWN || dir == NAV_RIGHT) ? 1.0f : -1.0f;
  const focus_rect_t *c = &g_focus_r[g_ui_focus];
  if (g_nav_ax < -1e8f) {
    g_nav_ax = c->x + c->w * 0.5f;
    g_nav_ay = c->y + c->h * 0.5f;
  }
  float anch = vert ? g_nav_ax : g_nav_ay;
  float cmid = vert ? c->y + c->h * 0.5f : c->x + c->w * 0.5f;
  float clo  = vert ? c->x : c->y, cex = vert ? c->w : c->h;
  int best = -1, alt = -1;
  float bs = 0, bp = 0, as = 0;
  for (int i = 0; i < n; i++) {
    if (i == g_ui_focus) continue;
    const focus_rect_t *r = &g_focus_r[i];
    float rmid = vert ? r->y + r->h * 0.5f : r->x + r->w * 0.5f;
    float along = (rmid - cmid) * sgn;
    // Half a UI unit of slack is what makes a PAIR a pair: two tiles sharing a
    // y are neither above nor below one another, so a vertical step skips both
    // of its own siblings instead of picking the nearer half by rounding.
    if (along <= 0.5f) continue;
    float rlo = vert ? r->x : r->y, rex = vert ? r->w : r->h;
    float gap = nav_gap(anch, rlo, rlo + rex);
    if (rlo < clo + cex && clo < rlo + rex) {
      if (best < 0 || along < bs - 0.5f ||
          (along < bs + 0.5f && gap < bp)) { best = i; bs = along; bp = gap; }
    } else if (vert) {
      float sc = along + 2.0f * gap;
      if (alt < 0 || sc < as) { alt = i; as = sc; }
    }
  }
  if (best < 0) best = alt;
  if (best < 0 && vert) {
    float ext = 0;
    for (int i = 0; i < n; i++) {
      const focus_rect_t *r = &g_focus_r[i];
      float end = (r->y + r->h * 0.5f) * -sgn;   // `far` is a MACRO in windef.h
      float gap = nav_gap(anch, r->x, r->x + r->w);
      if (best < 0 || end > ext + 0.5f ||
          (end > ext - 0.5f && gap < bp)) { best = i; ext = end; bp = gap; }
    }
  }
  if (best < 0) return;
  const focus_rect_t *r = &g_focus_r[best];
  g_ui_focus = best;
  if (vert) g_nav_ay = r->y + r->h * 0.5f;   // the other axis is CARRIED
  else      g_nav_ax = r->x + r->w * 0.5f;
}

// Does the focused row spend left/right on its own value? An empty table (the
// first frame of a page) answers yes, which is the pre-geometry behaviour: park
// the event for whichever row is about to claim focus id 0.
static int nav_focus_lr(void) {
  if (g_focus_rn <= 0) return 1;
  return g_ui_focus >= 0 && g_ui_focus < g_focus_rn && g_focus_r[g_ui_focus].lr;
}

// ---- MULTIPLAYER: host a game, find games on the LAN, join one -------------
// The MULTIPLAYER page drives all three. Scanning runs only while that page is
// on screen: one 64-byte broadcast a second per interface, and a game drops off
// the list 3.5 s after its last answer.
static int start_connect(const char *host, int port);
#define MP_GAMES_MAX 8
typedef struct {
  uint32_t ip, uid, seen_ms, rtt_ms;
  uint16_t port, proto;
  uint8_t humans, cap, mode, map;
  char name[25];
} mp_game_t;
static struct {
  int fd;
  uint32_t sent_ms;
  int n;
  mp_game_t g[MP_GAMES_MAX];
} g_mp = {.fd = -1};
static int g_mp_hosting;     // this process started the server it is on
static int g_mp_host_fail;     // the last HOST GAME's server exited (port in use?)
static int g_mp_host_req;      // HOST SETTINGS asked the MULTIPLAYER page to host

static void mp_scan_stop(void) {
  if (g_mp.fd >= 0) net_udp_close(g_mp.fd);
  g_mp.fd = -1;
  g_mp.n = 0;
}
static void mp_scan_tick(void) {
  uint32_t now = net_now_ms();
  if (g_mp.fd < 0) {
    g_mp.fd = net_udp_open(0);
    if (g_mp.fd < 0) return;
    net_udp_bcast_on(g_mp.fd);
    g_mp.sent_ms = now - 5000;
  }
  if (now - g_mp.sent_ms >= 1000) {
    unsigned char q[NET_SCAN_LEN] = {0};
    memcpy(q, NET_SCAN_MAGIC, 8);
    uint32_t t[16];
    int nt = net_bcast_targets(t, 16);
    uint16_t ports[2] = {NET_PORT_DEF, (uint16_t)g_cfg.mp_port};
    int np = g_cfg.mp_port != NET_PORT_DEF ? 2 : 1;
    for (int i = 0; i < nt; i++)
      for (int k = 0; k < np; k++) net_udp_send(g_mp.fd, t[i], ports[k], q, sizeof q);
    g_mp.sent_ms = now;
  }
  unsigned char b[128];
  uint32_t ip; uint16_t port;
  int n, guard = 0;
  while ((n = net_udp_recv(g_mp.fd, &ip, &port, b, sizeof b)) > 0 && guard++ < 64) {
    if (n < NET_HERE_LEN || memcmp(b, NET_HERE_MAGIC, 8)) continue;
    np_rd r = {b, (size_t)n, 8, 0};
    mp_game_t g = {.ip = ip, .seen_ms = now, .rtt_ms = now - g_mp.sent_ms};
    g.proto = np_r16(&r);
    g.port = np_r16(&r);
    g.uid = np_r32(&r);
    g.humans = np_r8(&r);
    g.cap = np_r8(&r);
    g.mode = np_r8(&r);
    g.map = np_r8(&r);
    for (int i = 0; i < 24; i++) {          // hostile input: glyph policy, as names
      char c = (char)np_r8(&r);
      g.name[i] = c == ' ' ? ' ' : c ? name_glyph(c) : 0;
      if (c && !g.name[i]) g.name[i] = '?';
    }
    g.name[24] = '\0';
    if (r.bad || !g.port) continue;
    if (!g.name[0]) snprintf(g.name, sizeof g.name, "BLINDDROP");
    int slot = -1;
    for (int i = 0; i < g_mp.n; i++)
      if (g_mp.g[i].uid == g.uid && g_mp.g[i].port == g.port) { slot = i; break; }
    if (slot >= 0) {
      // Heard on two interfaces (loopback and the LAN for a server on this
      // machine): keep the address it was first found at, refresh the rest.
      g.ip = g_mp.g[slot].ip;
      g_mp.g[slot] = g;
    } else if (g_mp.n < MP_GAMES_MAX) {
      g_mp.g[g_mp.n++] = g;
    }
  }
  for (int i = 0; i < g_mp.n; )
    if (now - g_mp.g[i].seen_ms > 3500) g_mp.g[i] = g_mp.g[--g_mp.n];
    else i++;
}
static void mp_ip_str(uint32_t ip, char *out, size_t n) {
  const unsigned char *q = (const unsigned char *)&ip;   // network order
  snprintf(out, n, "%u.%u.%u.%u", q[0], q[1], q[2], q[3]);
}
static int mp_join(uint32_t ip, uint16_t port) {
  g_net_secret = 0;                        // client uses the server's cookie
  return net_cl_connect(ip, port, g_cfg.name);
}
// HOST GAME: the HOST SETTINGS (host_*) served; difficulty, frag limit,
// rounds and ballistics are shared with SINGLEPLAYER. HOSTAGE RESCUE is
// served as a co-op mission with the host's squad size.
static int mp_host_start(void) {
  if (!HOST_SUPPORTED) return 0;
  char port[8], bots[8], skill[8], frag[8], rounds[8], name[24], sq[8];
  int mode = g_cfg.host_mode < 0 || g_cfg.host_mode >= GM_COUNT ? 0 : g_cfg.host_mode;
  snprintf(sq, sizeof sq, "%d", g_cfg.host_squad < 1 ? 1 : g_cfg.host_squad > RESC_SQUAD_MAX
                                ? RESC_SQUAD_MAX : g_cfg.host_squad);
  int bc = g_cfg.host_bots < 0 ? 0 : g_cfg.host_bots > MAX_BOTS ? MAX_BOTS : g_cfg.host_bots;
  snprintf(port, sizeof port, "%d", g_cfg.mp_port);
  snprintf(bots, sizeof bots, "%d", bc);
  snprintf(skill, sizeof skill, "%d", g_cfg.sp_diff);
  snprintf(frag, sizeof frag, "%d", g_cfg.sp_frag);
  snprintf(rounds, sizeof rounds, "%d", g_cfg.sp_rounds);
  snprintf(name, sizeof name, "%s", g_cfg.name[0] ? g_cfg.name : "PLAYER");
  char capp[8], tsz[8], tfl[8], tff[8];
  snprintf(tff, sizeof tff, "%d", g_cfg.host_team_ff ? 1 : 0);
  snprintf(capp, sizeof capp, "%d", MAX_HUMANS);
  snprintf(tsz, sizeof tsz, "%d", g_cfg.host_team_size);
  snprintf(tfl, sizeof tfl, "%d", g_cfg.host_team_fill ? 1 : 0);
  char *argv[] = {"blinddrop", "--server", "--port", port,
                  "--mode", (char *)GM_KEYS[mode], "--map", (char *)map_pick_key(g_cfg.host_map),
                  "--cap-public", capp, "--cap-bots", bots, "--skill", skill,
                  "--fraglimit", frag, "--rounds", rounds, "--name", name,
                  "--team-size", tsz, "--team-fill", tfl, "--friendly-fire", tff,
                  "--squad", sq, g_cfg.ballistics ? "--ballistics" : NULL, NULL};
  g_mp_host_fail = 0;
  // The port must be FREE first: with another server already on it, the
  // child would fail its bind and exit while this client happily joined the
  // stranger at 127.0.0.1 and called it hosting.
  int probe = net_udp_open(g_cfg.mp_port);
  if (probe < 0) { g_mp_host_fail = 1; return 0; }
  net_udp_close(probe);
  if (!host_spawn(argv)) { g_mp_host_fail = 1; return 0; }
  g_mp_hosting = 1;
  // The handshake resends every 250 ms for 4 s, which covers the server's own
  // start-up (map generation) without a wait here.
  return start_connect("127.0.0.1", g_cfg.mp_port);
}
static void mp_host_stop(void) {
  host_kill();
  g_mp_hosting = 0;
}

typedef enum { UI_PAGE_ROOT, UI_PAGE_MATCH, UI_PAGE_SETTINGS,
               UI_PAGE_CONTROLS, UI_PAGE_DEV, UI_PAGE_ARMORY, UI_PAGE_ONLINE,
               UI_PAGE_TEAM, UI_PAGE_HOST, UI_PAGE_COUNT } ui_page_t;

static ui_page_t g_ui_page = UI_PAGE_ROOT;

static int g_ui_control;  // CONTROLS page: 0 keyboard/mouse, 1 gamepad
static int g_ui_tab;      // legacy uistat field; mirrors the page enum
static int g_ui_pad_sub;  // CONTROLS/GAMEPAD: 0 sticks+aim, 1 buttons
static int g_ui_dev_sub;  // DEV page: 0 movement, 1 weapon, 2 assist
static int g_ui_set_sub;  // SETTINGS page: 0 general, 1 HUD

// Swallow currently-held pad buttons until they are released — the pad
// analogue of WM_MOUSEACTIVATE's MA_ACTIVATEANDEAT and the 150 ms refocus
// click swallow. Called on focus gain and on menu close, because the pad is
// LEVEL-polled: clearing g_keys does nothing when pad_sync re-establishes the
// level on the very next frame, so without this the B that closes the menu is
// a fresh crouch press (a slide at speed), the A on RESUME is a jump, and a
// trigger held across an alt-tab fires the moment the game comes back.
static void pad_eat_held(void);

// Defined by each platform backend: react to menu open/close (cursor
// clip/visibility, input reset) and to a vsync toggle.
static void platform_menu_changed(void);
static void platform_vsync_changed(void);

static int g_ui_name_edit;  // Settings page: the name field is capturing keys
static int g_ui_addr_edit;  // MULTIPLAYER page: the address field is capturing keys
// The address field rides the name field's whole key path (both backends, the
// harness `key`), so ui_name_key hands its keys over while it has focus. An
// address is a dotted quad or a host name, optionally :port — lower case, digits,
// '.', '-' and ':' — and it is stored in mp_host, so it is remembered.
static void ui_addr_key(const char *tok) {
  size_t l = strlen(g_cfg.mp_host);
  if (!strcmp(tok, "enter") || !strcmp(tok, "escape")) { g_ui_addr_edit = 0; return; }
  if (!strcmp(tok, "back")) { if (l) g_cfg.mp_host[l - 1] = '\0'; return; }
  if (!tok[0] || tok[1] || l >= sizeof g_cfg.mp_host - 1) return;
  char c = tok[0];
  if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':') {
    g_cfg.mp_host[l] = c;
    g_cfg.mp_host[l + 1] = '\0';
  }
}
#define UI_TEXT_EDIT (g_ui_name_edit || g_ui_addr_edit)

// THE ONE FIELD IN THE GAME THAT NEEDS A KEYBOARD, ON THE ONE DEVICE THAT HAS
// NONE. The name editor took keys and nothing else, so a pad player reached it,
// armed it, and was told by the hint line to "TYPE ON KEYBOARD" — on a handheld,
// in a game whose whole point is that it ships to one. This is that keyboard,
// and it is a GRID rather than a widget tree: the glyph set is already fixed by
// name_glyph (A-Z and 0-9, nothing else can survive a save), so the layout is a
// constant and the entire state is a cursor.
//
// Four uniform rows of nine, which is exactly the 36 glyphs name_glyph accepts,
// plus an action row. DELETE and DONE are their OWN row rather than two cells
// stolen from the last one: a wide action wants a wide target, and it keeps
// every glyph row the same width — which is what makes the cursor predictable,
// the only property a grid actually has to have.
#define UI_KB_COLS 9
#define UI_KB_GROWS 4                        // glyph rows
#define UI_KB_ROWS (UI_KB_GROWS + 1)         // ...plus DELETE / DONE
static const char UI_KB_ROW[UI_KB_GROWS][UI_KB_COLS + 1] = {
  "ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ0", "123456789",
};
static int g_ui_kb;         // the on-screen keyboard is up with the name edit
static int g_ui_kb_r, g_ui_kb_c;   // cursor: row 0..UI_KB_ROWS-1, col in row

static int ui_kb_cols(int r) { return r < UI_KB_GROWS ? UI_KB_COLS : 2; }

// Feed a token to the name editor: single chars append (A-Z/0-9), "back"
// deletes, "enter"/"escape" finish. An empty name falls back to PLAYER.
static void ui_name_key(const char *tok) {
  if (g_ui_addr_edit) { ui_addr_key(tok); return; }
  size_t l = strlen(g_cfg.name);
  if (!strcmp(tok, "enter") || !strcmp(tok, "escape")) {
    if (!l) snprintf(g_cfg.name, sizeof g_cfg.name, "PLAYER");
    g_ui_name_edit = 0;
    g_ui_kb = 0;    // the keyboard exists only for as long as the field does
  } else if (!strcmp(tok, "back")) {
    if (l) g_cfg.name[l - 1] = '\0';
  } else if (tok[0] && !tok[1] && l < sizeof g_cfg.name - 1) {
    char g = name_glyph(tok[0]);
    if (g) {
      g_cfg.name[l] = g;
      g_cfg.name[l + 1] = '\0';
    }
  }
}

// A row change keeps the cursor's horizontal POSITION, not its index — the
// action row is two cells wide and the glyph rows are nine, and stepping down
// from "E" onto "DELETE" has to land under E rather than at the left edge.
static int ui_kb_remap(int c, int from, int to) {
  int n = ui_kb_cols(from), m = ui_kb_cols(to);
  int o = (int)(((float)c + 0.5f) * (float)m / (float)n);
  return o < 0 ? 0 : o >= m ? m - 1 : o;
}

// ONE press, two devices: the pad's A and a click on a key are the same act,
// and a second copy of this switch is how the two drift apart.
static void ui_kb_press(int r, int c) {
  if (r < UI_KB_GROWS) {
    char t[2] = {UI_KB_ROW[r][c], 0};
    ui_name_key(t);
  } else {
    ui_name_key(c == 0 ? "back" : "enter");
  }
}

// The keyboard OWNS navigation while it is up, the same way an armed rebind
// capture does — a grid's four directions are its own, and handing them to the
// page's nearest-rect mover would step the cursor to whatever happens to be
// geometrically closest instead of to the next letter.
//
// B is BACKSPACE here and not "back". Inside a text field, undoing the last
// thing you did IS deleting the last character, it is the correction a player
// makes most often, and the cost of guessing wrong is one keypress; the way OUT
// is the DONE tile and START, both of which are visible or already universal.
static void ui_kb_nav(int ev) {
  int n = ui_kb_cols(g_ui_kb_r);
  switch (ev) {
    case NAV_LEFT:  g_ui_kb_c = (g_ui_kb_c + n - 1) % n; break;
    case NAV_RIGHT: g_ui_kb_c = (g_ui_kb_c + 1) % n; break;
    case NAV_UP: {
      int r = (g_ui_kb_r + UI_KB_ROWS - 1) % UI_KB_ROWS;
      g_ui_kb_c = ui_kb_remap(g_ui_kb_c, g_ui_kb_r, r);
      g_ui_kb_r = r;
      break;
    }
    case NAV_DOWN: {
      int r = (g_ui_kb_r + 1) % UI_KB_ROWS;
      g_ui_kb_c = ui_kb_remap(g_ui_kb_c, g_ui_kb_r, r);
      g_ui_kb_r = r;
      break;
    }
    case NAV_OK: ui_kb_press(g_ui_kb_r, g_ui_kb_c); break;
    case NAV_BACK: ui_name_key("back"); break;
    default: break;    // LB/RB carry no meaning on a grid
  }
}

[[maybe_unused]] static const char *ui_page_name(ui_page_t page) {
  static const char *const NAME[UI_PAGE_COUNT] = {
    "root", "match", "settings", "controls", "dev", "armory", "online", "team", "host"
  };
  return page >= 0 && page < UI_PAGE_COUNT ? NAME[page] : "root";
}

static void ui_route(ui_page_t page) {
  if (page < 0 || page >= UI_PAGE_COUNT) page = UI_PAGE_ROOT;
  if (UI_TEXT_EDIT) ui_name_key("enter");
  if (page != UI_PAGE_ONLINE && page != UI_PAGE_HOST) mp_scan_stop();   // the scan lives with its page
  g_ui_rebind = -1;
  g_ui_drag = 0;
  // THE RAIL REMEMBERS WHERE YOU WERE. Coming back from SETTINGS used to land
  // the focus on SINGLEPLAYER — the tile whose page's first row is the only
  // control that can end a running match, so "change one setting and come back"
  // ended two presses from a wiped scoreboard. A sub-page still opens at its
  // own first row: that is where its action lives.
  // Scoped to ONE menu session — ui_close_menu clears it, because the rail's
  // row 1 is MULTIPLAYER offline and LEAVE MATCH online, so a remembered index
  // from a boot-screen visit would open a later pause menu on a different
  // control than the one it was stored for.
  if (g_ui_page == UI_PAGE_ROOT && page != UI_PAGE_ROOT) g_ui_root_focus = g_ui_focus;
  g_ui_page = page;
  g_ui_tab = (int)page;
  g_ui_focus = page == UI_PAGE_ROOT ? g_ui_root_focus : 0;
  nav_anchor_reset();   // the carried column belongs to the page that set it
  g_ui_arm = UI_ARM_NONE;
  g_focus_sw = 0;   // the focus ring snaps to its new page, no cross-page glide
}

// The pause reaches the audio timeline too (see g_audio_pause at the mixer):
// only a windowed session pauses, so only there may the mixer freeze — a
// harness mixdown must keep playing through a scripted `menu`.
static void ui_audio_pause(int on) {
#ifndef _WIN32
  if (g_headless) return;
#endif
  atomic_store_explicit(&g_audio_pause, on, memory_order_relaxed);
}

static void ui_close_menu(void) {
  if (UI_TEXT_EDIT) ui_name_key("enter");
  mp_scan_stop();
  g_menu_open = 0;
  g_ui_root_focus = 0;   // next session's rail is not the same rows
  // ...and the BOOT SCREEN is gone for the session. Every later menu open is a
  // PAUSE inside a match that has begun, which is a different screen with a
  // different job; leaving the flag set would run a hero camera over a fight
  // the player stopped to look at.
  g_home = 0;
  g_ui_rebind = -1;
  g_ui_drag = 0;
  g_ui_arm = UI_ARM_NONE;
  pad_eat_held();  // the button that closed the menu must not act in the game
  ui_audio_pause(0);
  config_write(g_cfg_path, &g_cfg);
  platform_menu_changed();
}

static void ui_open_menu(void) {
  ui_route(UI_PAGE_ROOT);
  g_menu_open = 1;
  g_nav_n = 0;  // events queued while closed must not replay into the session
  // The audio timeline freezes with the SIM, so it follows the same predicate:
  // a networked session's world keeps ticking, so its audio must too (a paused
  // mid-reload would otherwise play its bolt cue into a frozen world — but
  // online the world is not frozen).
  if (!g_online) ui_audio_pause(1);
  platform_menu_changed();
}

// ESC: finish name entry / cancel a rebind, else close (and save), else open.
static void ui_on_escape(void) {
  if (UI_TEXT_EDIT) ui_name_key("escape");
  else if (g_ui_rebind >= 0) g_ui_rebind = -1;
  else if (g_menu_open && g_ui_page == UI_PAGE_HOST) ui_route(UI_PAGE_ONLINE);
  else if (g_menu_open && g_ui_page != UI_PAGE_ROOT) ui_route(UI_PAGE_ROOT);
  else if (g_menu_open) ui_close_menu();
  else ui_open_menu();
}

// Assign a captured key token to the action being rebound. If another action
// already uses the token, the two swap — no bind can silently go dead. The
// armed row selects WHICH table: a keyboard row takes keyboard/mouse tokens, a
// pad row takes pad tokens, and the wrong device leaves the capture armed
// (pressing a pad button while a keyboard row waits must not bind garbage).
// pad_start is refused because START is the menu key by construction.
static void ui_assign_bind(const char *tok) {
  if (g_ui_rebind < 0) return;
  int is_pad = !strncmp(tok, "pad_", 4);
  if (is_pad != g_ui_rebind_pad) return;
  if (is_pad && !strcmp(tok, "pad_start")) return;
  char (*tbl)[12] = g_ui_rebind_pad ? g_cfg.padbind : g_cfg.bind;
  char old[12];
  snprintf(old, sizeof old, "%s", tbl[g_ui_rebind]);
  for (int i = 0; i < IN_COUNT; i++)
    if (i != g_ui_rebind && !strcmp(tbl[i], tok))
      snprintf(tbl[i], sizeof tbl[i], "%s", old);
  snprintf(tbl[g_ui_rebind], sizeof tbl[g_ui_rebind], "%s", tok);
  g_ui_rebind = -1;
}

// EVERY BIND BACK TO ITS COMPILED DEFAULT, for ONE device. A player who has
// swapped four keys around had no way back from inside the game: the only reset
// was deleting config.cfg, which is not something a menu may require you to
// know. One function, both tables, so the two device tabs cannot drift.
static void ui_binds_reset(int pad) {
  char (*tbl)[12] = pad ? g_cfg.padbind : g_cfg.bind;
  for (int i = 0; i < IN_COUNT; i++)
    snprintf(tbl[i], sizeof tbl[i], "%s",
             pad ? ACTION_DEF[i].pdflt : ACTION_DEF[i].dflt);
  g_ui_rebind = -1;
}

// Pad token -> menu label ("pad_lb" -> "LB", "none" -> "--").
static const char *pad_disp(const char *tok) {
  if (!strcmp(tok, "none")) return "--";
  for (int i = 0; i < PB_COUNT; i++)
    if (!strcmp(tok, PAD_BTN_DEF[i].tok)) return PAD_BTN_DEF[i].label;
  return tok;
}

enum { DRAG_SENSE = 1, DRAG_FOV, DRAG_CAP, DRAG_VOL, DRAG_BOTS, DRAG_FRAG,
       DRAG_ADS_SENSE,  // 7, the one id left before the MV block starts
       DRAG_MV0 = 8, DRAG_HUDS = 27, DRAG_CHSIZE = 28, DRAG_HUDA = 29, DRAG_VMFOV = 30, DRAG_WP0 = 32,
       DRAG_PAD0 = 56 };  // +i = tuning slider i

// Register one interactive row (layout order) and draw the focus ring when the
// pad drives the menu. Returns whether this row has focus — the row then also
// answers ui_nav_take for the OK/LEFT/RIGHT event of the frame. `lr` says
// whether this row's own value eats left/right; the rect is what NEXT frame's
// nav_move navigates over, which is why it is recorded even with the pad
// nowhere near the menu — the FIRST pad event must already have geometry.
static int ui_focus_row(float x, float y, float w, float h, int lr) {
  int fid = g_ui_nfocus++;
  int foc = fid == g_ui_focus;
  if (fid < UI_MAX_FOCUS) g_focus_r[fid] = (focus_rect_t){x, y, w, h, lr};
  if (foc && g_ui_pad_mode) {
    // The ring GLIDES: its rect is smoothed toward the focused row on the
    // presentation clock (fixed-step in the harness), which is what makes pad
    // navigation read as one cursor moving instead of four rects teleporting.
    // Drawn under the row's own content — this call happens first in a row.
    if (g_focus_sw <= 0) {
      g_focus_sx = x; g_focus_sy = y; g_focus_sw = w; g_focus_sh = h;
    }
    // THE GLIDE HAS TO SETTLE INSIDE ONE REPEAT INTERVAL. At 24 a one-row step
    // needs ~12 rendered frames while PAD_NAV_REP repeats from 9 Hz upward, so a
    // held d-pad kept the ring permanently mid-flight: its hairline scrolling
    // THROUGH the row labels and no row ever reading as focused. At 60 it lands
    // in about five frames and still reads as one cursor moving.
    g_focus_sx = ui_smooth(g_focus_sx, x, 60);
    g_focus_sy = ui_smooth(g_focus_sy, y, 60);
    g_focus_sw = ui_smooth(g_focus_sw, w, 60);
    g_focus_sh = ui_smooth(g_focus_sh, h, 60);
    // The ring rides ui_panel's 6-point chamfer path (TR/BL cut): it is the
    // loudest element on a settings page and was the one square peer left
    // beside the chamfered slab. Same wash, same hairline, same accent bar.
    {
      float fx2 = g_focus_sx - 3, fy2 = g_focus_sy - 1.5f;
      float fw2 = g_focus_sw + 6, fh2 = g_focus_sh + 3;
      float qx[6], qy[6];
      float c = ui_cham_path(fx2, fy2, fw2, fh2, qx, qy);
      ui_cham_fill(qx, qy, (const float[4]){1.000f, 0.549f, 0.102f, 0.08f});
      ui_cham_edge(qx, qy, 0.5f, 1.000f, 0.549f, 0.102f, 0.85f);
      ui_rect(fx2, fy2, 2, fh2 - c, C_BLAZE, 0.95f);
    }
  }
  return foc;
}

static int ui_nav_take(int focused, int act) {
  if (!focused || g_nav_act != act) return 0;
  g_nav_act = -1;
  return 1;
}

// ONE CURSOR AT A TIME. In pad mode the software cursor is not drawn, so a
// trackpad or a touch that left the pointer somewhere else lights a second
// control with nothing on screen to explain it — two cursors, one invisible.
// The on-screen keyboard already gated its hover this way; every control does
// now. The mouse takes the menu back the moment it MOVES (g_ui_pad_mode = 0).
#define UI_HOVER(cond) (!g_ui_pad_mode && (cond))
static float ui_slider(int id, float x, float y, float w, float v,
                       float lo, float hi, int click, float cx, float cy) {
  int hot = UI_HOVER(cx >= x - 4 && cx <= x + w + 4 && cy >= y && cy <= y + 13);
  if (click && hot && g_ui_rebind < 0) g_ui_drag = id;
  if (g_ui_drag == id) v = lo + f_clamp((cx - x) / w, 0, 1) * (hi - lo);
  float t = f_clamp((v - lo) / (hi - lo), 0, 1);
  float ky = y + 6;
  // Hard track: hairline with quarter ticks, BLAZE fill, a square knob.
  ui_rect(x, ky - 0.7f, w, 1.4f, C_BONE, 0.14f);
  for (int i = 1; i < 4; i++)
    ui_rect(x + w * 0.25f * (float)i, ky - 2.6f, 1, 2, C_BONE, 0.18f);
  ui_rect(x, ky - 0.7f, t * w, 1.4f, C_BLAZE, 0.9f);
  if (g_ui_drag == id || hot) ui_rect(x + t * w - 2, ky - 5, 4, 10, C_BLAZE, 1);
  else                        ui_rect(x + t * w - 1.7f, ky - 4.5f, 3.4f, 9, C_BONE, 0.95f);
  return v;
}

// Click-to-cycle option pill, same look and placement as ui_toggle's ON/OFF;
// the pill is sized to the longest option so nothing ever overflows it.
static int ui_cycle(const char *label, const char *const *opts, int n, int *idx,
                    float px, float pw, float y, int click, float cx, float cy) {
  int foc = ui_focus_row(px + 10, y - 1, pw - 20, 15, 1);
  int chg = 0;
  if (ui_nav_take(foc, NAV_LEFT)) { *idx = (*idx + n - 1) % n; chg = 1; }
  if (ui_nav_take(foc, NAV_RIGHT) || ui_nav_take(foc, NAV_OK)) {
    *idx = (*idx + 1) % n;
    chg = 1;
  }
  ui_text(label, px + 16, y + 3, 1, C_TEXT, 1);
  const char *tok = opts[*idx];
  float bw = ui_text_width("OFF", 1);
  for (int i = 0; i < n; i++) {
    float w = ui_text_width(opts[i], 1);
    if (w > bw) bw = w;
  }
  bw += 16;
  float bx = px + pw - 16 - bw;
  // THE WHOLE ROW IS THE TARGET, not the value chip at the far right: the
  // stripe and the focus ring are row-wide, so a click on the label did
  // nothing at all — a fingernail target on a handheld touchscreen.
  // HEIGHT 14, HALF-OPEN, and both halves of that matter: the settings rows are
  // pitched 15 and the bind rows 14, so a 15-tall INCLUSIVE band shares its
  // bottom edge with the next row's top — two rows hot on one click. 14 and
  // half-open sits inside the tighter pitch and still covers the value chip
  // (drawn at y+1, 12 tall) plus the label beside it.
  int hot = UI_HOVER(cx >= px + 10 && cx <= px + pw - 10 &&
                     cy >= y && cy < y + 14);
  ui_rrect_c(bx, y + 1, bw, 12, 0, C4(1, 1, 1, hot ? 0.16f : 0.07f),
             C4(1, 1, 1, hot ? 0.12f : 0.05f));
  // Dim is the OFF look, not the index-0 look: a cycle of real alternatives
  // (LINEAR/STANDARD/DYNAMIC, EASY/NORMAL/HARD) has no "off" state, and its
  // first option rendering as disabled reads as a bug.
  if (strcmp(tok, "OFF") != 0)
    ui_textb(tok, bx + (bw - ui_text_width(tok, 1)) * 0.5f, y + 4, 1, C_BONE, 1);
  else
    ui_text(tok, bx + (bw - ui_text_width(tok, 1)) * 0.5f, y + 4, 1, C_DIM, 1);
  if (click && hot && g_ui_rebind < 0) { *idx = (*idx + 1) % n; return 1; }
  return chg;
}

// HOSTAGE RESCUE's squad row and the guard count it brings (both pages).
static float ui_squad_rows(int *squad, float px, float pw, float y, int click, float cx, float cy) {
  static const char *const SQ[RESC_SQUAD_MAX] = {"SOLO", "2 OPERATORS", "3 OPERATORS"};
  int si = *squad - 1;
  if (si < 0 || si >= RESC_SQUAD_MAX) si = RESC_SQUAD_MAX - 1;
  ui_cycle("SQUAD", SQ, RESC_SQUAD_MAX, &si, px, pw, y, click, cx, cy);
  *squad = si + 1;
  y += 15;
  char gl[40];
  snprintf(gl, sizeof gl, "%d ON COMPOUND", resc_guard_count(g_cfg.sp_diff, *squad));
  ui_text("GUARDS", px + 16, y + 3, 1, C_TEXT, 1);
  ui_text(gl, px + pw - 16 - ui_text_width(gl, 1), y + 4, 1, C_HAZE, 1);
  return y + 15;
}


// A toggle IS a two-option cycle, and keeping it as its own copy of the pill
// widget is how the two drift: the font is monospace, so max("OFF","ON") is
// "OFF" and the box comes out the same width; `if (*idx)` reproduces
// `if (*val)`; `(*idx + 1) % 2` is `!*val`. Every caller's int is normalised to
// 0/1 at config load and only ever flipped by `!`, so opts[] cannot be indexed
// out of range.
static int ui_toggle(const char *label, int *val, float px, float pw, float y,
                     int click, float cx, float cy) {
  static const char *const ONOFF[2] = {"OFF", "ON"};
  return ui_cycle(label, ONOFF, 2, val, px, pw, y, click, cx, cy);
}

// Right-aligned value pill: label on the left, a box at the panel's right edge
// holding `tok`. `active` = being edited, which fills the box with the accent and
// darkens the text. Returns 1 if it was clicked this frame; the caller decides
// what that means (arm a rebind, open the name field). The KEYS rows and the
// MATCH name field were two copies of this, down to the +12 padding, the -16
// inset, the 12 px height, the y+1/y+3/y+4 offsets and the 0.95/0.14/0.07 alphas
// — nine magic numbers with no name, twice.
static int ui_pill(const char *label, const char *tok, float px, float pw,
                   float y, int active, float minw, int click,
                   float cx, float cy) {
  int foc = ui_focus_row(px + 10, y - 1, pw - 20, 15, 0);
  ui_text(label, px + 16, y + 3, 1, C_TEXT, 1);
  float bw = ui_text_width(tok, 1) + 12;
  if (bw < minw) bw = minw;
  float bx = px + pw - 16 - bw;
  int hot = UI_HOVER(cx >= px + 10 && cx <= px + pw - 10 &&
                     cy >= y && cy < y + 14);   // the row, not the chip; see ui_cycle
  if (active) {
    // An armed capture breathes BLAZE — a live socket, not a filled box.
    float bl = 0.75f + 0.25f * sinf(g_ui_time * 6.0f);
    ui_rrect_c(bx, y + 1, bw, 12, 0, C4(C_BLAZE, 0.95f * bl), C4(C_BLAZE, 0.80f * bl));
    ui_textb(tok, bx + 6, y + 4, 1, 0.10f, 0.07f, 0.03f, 1);
  } else {
    ui_rrect_c(bx, y + 1, bw, 12, 0, C4(1, 1, 1, hot ? 0.16f : 0.07f),
               C4(1, 1, 1, hot ? 0.12f : 0.05f));
    ui_text(tok, bx + 6, y + 4, 1, C_TEXT, 1);
  }
  return (click && hot && g_ui_rebind < 0 && !g_ui_drag) ||
         ui_nav_take(foc, NAV_OK);
}

// Action row: left-aligned label with a chevron marker, quiet until hot —
// the direct-action-list grammar (never tiles). Hot goes stark WHITE with
// black text — the coldest, hardest state the palette has, reserved for
// "this will happen if you press"; danger keeps THREAT.
static int ui_button(const char *label, float x, float y, float w, float h,
                     int click, float cx, float cy, int danger) {
  int foc = ui_focus_row(x, y, w, h, 0);
  int hot = UI_HOVER(cx >= x && cx <= x + w && cy >= y && cy <= y + h);
  float ty = y + (h - 7) * 0.5f;
  if (hot) {
    if (danger) {
      ui_rrect_c(x, y, w, h, 0, C4(C_THREAT, 0.95f), C4(0.85f, 0.20f, 0.15f, 0.95f));
      ui_text("\x01", x + 8, ty, 1, 1, 1, 1, 0.9f);
      ui_textb(label, x + 17, ty, 1, 1, 1, 1, 1);
    } else {
      ui_rrect_c(x, y, w, h, 0, C4(0.96f, 0.97f, 0.98f, 0.96f),
                 C4(0.86f, 0.89f, 0.92f, 0.96f));
      ui_text("\x01", x + 8, ty, 1, 0.05f, 0.06f, 0.08f, 0.9f);
      ui_textb(label, x + 17, ty, 1, 0.03f, 0.04f, 0.05f, 1);
    }
  } else {
    ui_rrect_c(x, y, w, h, 0, C4(1, 1, 1, 0.045f), C4(1, 1, 1, 0.03f));
    if (danger) {
      ui_rect(x, y, 2, h, C_THREAT, 0.8f);
      ui_text("\x01", x + 8, ty, 1, C_THREAT, 0.75f);
      ui_text(label, x + 17, ty, 1, C_TEXT, 0.92f);
    } else {
      ui_text("\x01", x + 8, ty, 1, C_HAZE, 0.8f);
      ui_text(label, x + 17, ty, 1, C_TEXT, 0.92f);
    }
  }
  return (click && hot && g_ui_rebind < 0) || ui_nav_take(foc, NAV_OK);
}

// Section eyebrow: small, wide-tracked, cool — a label and one hairline.
// Structure whispers; only data and focus get color.
static void ui_section(const char *title, float px, float pw, float y) {
  ui_text_ex(title, px + 16, y - 1, 0.8f, TXT_BOLD, 2.2f, C4(C_HAZE, 0.95f));
  float tw = ui_text_width_tr(title, 0.8f, 2.2f);
  ui_rect(px + 16 + tw + 8, y + 2, pw - 32 - tw - 8, 1, C_BONE, 0.10f);
}

// One tuning-parameter row: label, slider over the def's range, value.
// Values snap to the parameter's step and display in matching precision.
static float ui_param_row(const param_def_t *d, float *val, int drag_id,
                          float px, float y, int click, float cx, float cy) {
  char buf[16];
  int foc = ui_focus_row(px + 10, y - 1, 320, 15, 1);
  int touched = 0;
  if (ui_nav_take(foc, NAV_LEFT))  { *val = f_clamp(*val - d->step, d->lo, d->hi); touched = 1; }
  if (ui_nav_take(foc, NAV_RIGHT)) { *val = f_clamp(*val + d->step, d->lo, d->hi); touched = 1; }
  ui_text(d->label, px + 16, y + 3, 1, C_TEXT, 1);
  float v = ui_slider(drag_id, px + 150, y, 140, *val, d->lo, d->hi, click, cx, cy);
  // Snap to the step grid ONLY on interaction. Unconditionally, merely OPENING
  // the menu rewrote every drawn row — a hand-edited `sense 0.004` (legal to
  // the parser, whose range is deliberately wider than the slider's) rounded
  // to 0.000 on the 0.01 grid and the close's config_write persisted the
  // destroyed value. The config layer's own philosophy is "keep the previous
  // value on garbage instead of clamping"; the menu must not be the one path
  // that quietly does the opposite.
  float st = d->step;
  if (g_ui_drag == drag_id || touched) *val = roundf(v / st) * st;
  snprintf(buf, sizeof buf, st < 0.099f ? "%.2f" : st < 1.0f ? "%.1f" : "%.0f",
           (double)*val);
  // ONE value column: right-aligned on the chip edge every other row uses.
  ui_text(buf, px + 324 - ui_text_width(buf, 1), y + 3, 1, C_DIM, 1);
  return y + 15;
}

static float ui_mv_row(int i, float px, float y, int click, float cx, float cy) {
  return ui_param_row(&MV_DEF[i], &g_cfg.mv[i], DRAG_MV0 + i, px, y, click, cx, cy);
}

static int g_ui_wpn;  // which weapon the DEV/WEAPON view edits (WPN_AR / WPN_SR)

static float ui_wp_row(int i, float px, float y, int click, float cx, float cy) {
  return ui_param_row(&WP_DEF[i], &g_cfg.wp[g_ui_wpn][i], DRAG_WP0 + i,
                      px, y, click, cx, cy);
}

static float ui_pad_row(int i, float px, float y, int click, float cx, float cy) {
  return ui_param_row(&PAD_DEF[i], &g_cfg.pad[i], DRAG_PAD0 + i,
                      px, y, click, cx, cy);
}

// Horizontal sub-selector (device, STICKS/BUTTONS, MOVEMENT/WEAPON, weapon):
// one focus row, LEFT/RIGHT or A cycles, the mouse clicks a cell directly.
// This is the original weapon selector generalised — it was about to be copied
// twice more.
static void ui_subsel(const char *const *opts, int n, int *idx, float px,
                      float pw, float y, int click, float cx, float cy) {
  int foc = ui_focus_row(px + 10, y - 1, pw - 20, 15, 1);
  if (ui_nav_take(foc, NAV_LEFT)) *idx = (*idx + n - 1) % n;
  if (ui_nav_take(foc, NAV_RIGHT) || ui_nav_take(foc, NAV_OK))
    *idx = (*idx + 1) % n;
  float selw = (pw - 32) / (float)n;
  // Segmented control: one quiet container, the selected cell stark WHITE
  // with black bold text — same hard state grammar as the buttons.
  ui_rrect_c(px + 16, y - 1, pw - 32, 15, 0, C4(0, 0, 0, 0.30f), C4(0, 0, 0, 0.22f));
  for (int i = 0; i < n; i++) {
    float sx = px + 16 + (float)i * selw;
    int hot = UI_HOVER(cx >= sx && cx <= sx + selw && cy >= y && cy <= y + 13);
    float lw = ui_text_width(opts[i], 1);
    if (*idx == i) {
      ui_rrect_c(sx + 1, y, selw - 2, 13, 0, C4(0.96f, 0.97f, 0.98f, 0.96f),
                 C4(0.86f, 0.89f, 0.92f, 0.96f));
      ui_textb(opts[i], sx + (selw - lw) * 0.5f, y + 3, 1, 0.03f, 0.04f, 0.05f, 1);
    } else {
      if (hot) ui_rrect_c(sx + 1, y, selw - 2, 13, 0, C4(1, 1, 1, 0.10f),
                          C4(1, 1, 1, 0.07f));
      ui_text(opts[i], sx + (selw - lw) * 0.5f, y + 3, 1, C_HAZE, 1);
    }
    if (click && hot && g_ui_rebind < 0 && !g_ui_drag) *idx = i;
  }
}


