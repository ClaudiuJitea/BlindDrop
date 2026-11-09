// code/ui/boot_menu.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// The BOOT SCREEN's own layout. Everything above this point in the menu is a
// centred panel because every other page is a FORM — a list of settings with a
// title over it. This page is not a form, it is a poster: six destinations and
// a camera. So it gets its own grammar rather than a variant of the panel's.
//
// The left third is a rail of tiles and the rest is deliberately EMPTY, because
// the rest is the shot. Nothing is drawn over the subject except the two cards
// that name what the camera is looking at, and those sit in the corner the
// composition keeps clear (home_camera's HOME_COMPOSE is what keeps it clear —
// the rail and the framing are one decision made in two places, which is why
// both cite it).
// ---------------------------------------------------------------------------

// Text that CANNOT run out of its element. Every string on the boot screen is
// composed at runtime — a player name, a match line that grows a word when the
// difficulty changes, a weapon caption — and every one of them sits in a box
// whose width was chosen against the string that happened to be there on the
// day. The HUD's virtual space is 640 units wide at 16:9 and NARROWER at 4:3,
// so a caption that fits on a widescreen monitor is not evidence about anything.
// Shrinks the type until it fits, down to two thirds; past that the box is
// wrong and clipping the tail is the honest failure.
static void ui_text_fit(const char *t, float x, float y, float maxw, float scale,
                        float thr, float track, const float c[4]) {
  float sc = scale;
  for (int i = 0; i < 8 && ui_text_width_tr(t, sc, track * sc / scale) > maxw; i++)
    sc *= 0.94f;
  ui_text_ex(t, x, y, sc, thr, track * sc / scale, c);
}

#ifndef _WIN32
// Defined with the evdev pump in the Linux backend. Forward-declared here
// because the harness parser sits ABOVE the platform split and `padscan` is
// the one proof that has to reach below it — the node table it reports is the
// backend's, and no shared-code mock of /dev/input would prove anything about
// the machine the game is actually running on.
static void nat_pad_report(int frames);
#endif

// The boot screen's layout sits with the rest of the menu, but its director
// lives with the renderer — a camera is not a widget. These are the only two
// things the layout needs from it, and neither exposes the shot list.
static player_t *local_player(void);
static const char *home_caption(float *fade);
static float home_dip(void);

// A tile. The machined slab ui_panel established, a label at the size the
// destination deserves, and a caption saying what pressing it does. Hover and
// pad focus are the SAME state: on a screen made of six large targets, a focus
// treatment the mouse cannot reproduce reads as two different cursors.
static int ui_tile(const char *label, const char *sub, float x, float y,
                   float w, float h, float ls, int click, float cx, float cy,
                   int danger) {
  int foc = ui_focus_row(x, y, w, h, 0);
  int hot = UI_HOVER(cx >= x && cx <= x + w && cy >= y && cy <= y + h) ||
            (foc && g_ui_pad_mode);
  // STATE BY VALUE, ON ONE GEOMETRY. Two rules, and the second is the one that
  // is easy to get wrong: the hovered tile and the resting tile must be the
  // SAME SHAPE. The first version drew the rest state as ui_panel's chamfered
  // hexagon and the hover as a rounded rect, so the corners changed as the
  // cursor crossed — a tile that changes outline is a tile that moves, and six
  // of them make a rail that twitches. Both states are now the same chamfer;
  // only the fill and the type weight differ, and they differ QUIETLY. A menu
  // whose rows flash to near-white is louder than the photograph it sits on.
  // No accent edge on any tile, destructive or not: a coloured rule is a
  // decoration on a screen that has a subject.
  // Every string goes through ui_text_fit against the box it is IN, so a longer
  // player name, a wider difficulty word or a narrower aspect ratio cannot run
  // a caption out of its tile.
  float lx = x + 13;
  float lh = ls * 7.0f;
  float ty = sub ? y + (h - lh - 9) * 0.5f : y + (h - lh) * 0.5f;
  float chevw = ls > 1.6f ? ls * 7.0f : 0.0f;
  (void)danger;
  ui_panel(x, y, w, h, 0, hot ? 1.0f : 0.92f);
  // The slab warms on focus, on its OWN chamfered path so the outline never
  // moves, and the type brightens with it.
  if (hot) ui_panel_wash(x, y, w, h, 0.150f, 0.094f, 0.046f, 0.90f);
  // The brand's one accent: a bar on the tile's leading edge, lit on focus.
  ui_rect(x + 3, y + 5, hot ? 2.4f : 1.2f, h - 10,
          C_BLAZE, hot ? 1.0f : 0.34f);
  ui_text_fit(label, lx, ty, w - 26 - chevw, ls, hot ? TXT_BLACK : TXT_BOLD,
              ls * 1.05f, C4(C_BONE, hot ? 1.0f : 0.86f));
  if (chevw > 0)
    ui_text_ex("\x01", x + w - 13 - ls * 5.0f, ty + lh * 0.14f, ls * 0.8f,
               TXT_BOLD, 0, C4(C_BONE, hot ? 0.85f : 0.40f));
  if (sub)
    ui_text_fit(sub, lx, ty + lh + 4, w - 26, 0.72f, TXT_REG, 1.35f,
                C4(C_HAZE, hot ? 0.95f : 0.75f));
  return (click && hot && g_ui_rebind < 0) || ui_nav_take(foc, NAV_OK);
}

// The card that names what the camera is looking at. Two of them: the weapon in
// the hand, permanently, and the part the current shot is about, which appears
// only on the close-ups. Bottom right, which is the one corner the rail does not
// own and the subject does not stand in.
static void ui_home_cards(float vw, float vh) {
  const player_t *lp = local_player();
  int wc = lp ? lp->wp.cur : WPN_AR;
  // Descriptions, not trademarks. The file's own reference notes name the real
  // hardware; a shipped screen says what the thing IS.
  static const char *const WNAME[WPN_COUNT] = {"ASSAULT RIFLE", "SNIPER RIFLE",
                                               "SIDEARM", "COMBAT KNIFE", "FRAG GRENADE"};
  static const char *const WSPEC[WPN_COUNT] = {"5.56 x 45   /   FULL AUTO",
                                               ".338 MAGNUM   /   BOLT ACTION",
                                               "9 x 19   /   SEMI AUTO",
                                               "FIXED BLADE   /   SLASH + GUARD",
                                               "3.5 S FUSE   /   9 M BLAST"};
  // The cards live in the corner the rail does not own, and at 4:3 that corner
  // is 160 units narrower than at 16:9 — enough that a card sized for a
  // widescreen monitor lands ON the QUIT tile. Cleared against the rail's own
  // right edge and then shrunk to whatever is left, so the layout degrades by
  // getting smaller rather than by overlapping.
  #define HOME_RAIL_X 34.0f
  #define HOME_RAIL_W 208.0f
  float lim = HOME_RAIL_X + HOME_RAIL_W + 20.0f;
  float cw = 208, ch = 44;
  float cxr = vw - 34 - cw;
  if (cxr < lim) { cxr = lim; cw = vw - 34 - lim; }
  if (cw < 96) return;               // no room at all: the shot speaks for itself
  float cyr = vh - 40 - ch;
  ui_panel(cxr, cyr, cw, ch, 0, 0.84f);
  ui_rect(cxr, cyr, cw, 1.0f, C_BONE, 0.28f);
  ui_text_fit(WNAME[wc], cxr + 13, cyr + 11, cw - 26, 1.35f, TXT_BOLD, 1.5f,
              C4(C_BONE, 1));
  ui_text_fit(WSPEC[wc], cxr + 13, cyr + 27, cw - 26, 0.72f, TXT_REG, 1.3f,
              C4(C_HAZE, 0.9f));

  float f;
  const char *cap = home_caption(&f);
  if (!cap) return;
  float tw = ui_text_width_tr(cap, 0.8f, 2.4f);
  float bx = cxr + cw - tw - 12, by = cyr - 19;
  ui_rect(bx - 10, by - 4, tw + 22, 15, 0.02f, 0.03f, 0.04f, 0.66f * f);
  ui_text_ex(cap, bx, by, 0.8f, TXT_BOLD, 2.4f, C4(C_BONE, 0.92f * f));
}

static void ui_home(int click, float cx, float cy) {
  float vw = g_ui_vw, vh = g_ui_vh;
  int dev = DEV_MENU && g_cfg.devmode;
  // THE CUT, and it goes down FIRST — over the shot, under the whole interface.
  // The rail is the one thing on this screen that must never blink: a menu that
  // dims every two seconds reads as a fault, while a picture that does it reads
  // as an edit.
  // ...and ONLY while the camera is running. Left ungated it dipped the paused
  // world to black on the director's schedule — a screen with no cuts in it,
  // blinking.
  float dip = g_home ? home_dip() : 0.0f;
  if (dip > 0.002f) ui_rect(0, 0, vw, vh, 0.008f, 0.010f, 0.016f, dip);
  // A graded scrim down the left edge only. The tiles have to read over
  // whatever the camera is doing, and the honest way to buy that is the same
  // one every broadcast lower-third uses: darken the ground under the type and
  // leave the picture alone. No blur — the picture IS the content here, which
  // is exactly why the frosted backdrop the other pages use is suppressed.
  // Lighter over the frosted backdrop than over a live camera: a blurred world
  // is already most of the contrast the type needs, and stacking the full scrim
  // on top of it turned the paused arena into a black wall.
  float sk = g_home ? 0.86f : 0.55f;
  ui_hgrad(0, 0, vw * 0.62f, vh, C4(0.010f, 0.014f, 0.024f, sk),
           C4(0.010f, 0.014f, 0.024f, 0.0f));
  ui_vgrad(0, 0, vw, 54, C4(0.010f, 0.014f, 0.024f, 0.60f),
           C4(0.010f, 0.014f, 0.024f, 0.0f));
  // ...and a floor under the cards, which only exist while the camera does.
  if (g_home)
    ui_vgrad(0, vh * 0.62f, vw, vh * 0.38f, C4(0.010f, 0.014f, 0.024f, 0.0f),
             C4(0.010f, 0.014f, 0.024f, 0.62f));

  float rx = HOME_RAIL_X, rw = HOME_RAIL_W;
  float sw = ui_text_width_tr("BLIND", 3.1f, 3.0f);
  float dw = ui_text_width_tr("DROP", 3.1f, 3.0f);
  ui_glow(rx + sw + 11 + dw * 0.5f, 41, 72, C_BLAZE, 0.13f);
  ui_text_ex("BLIND", rx + 1.2f, 31.4f, 3.1f, TXT_BOLD, 3.0f, C4(0, 0, 0, 0.55f));
  ui_text_ex("DROP", rx + sw + 12.2f, 31.4f, 3.1f, TXT_BOLD, 3.0f, C4(0, 0, 0, 0.55f));
  ui_text_ex("BLIND", rx, 30, 3.1f, TXT_BOLD, 3.0f, C4(C_BONE, 1));
  ui_text_ex("DROP", rx + sw + 11, 30, 3.1f, TXT_BOLD, 3.0f, C4(C_BLAZE, 1));
  ui_rect(rx, 58, rw, 1.0f, C_BONE, 0.22f);
  ui_rect(rx, 57.4f, 46, 2.2f, C_BLAZE, 1.0f);
  if (g_home) {
    ui_text_ex("CONDITIONS", rx, 66, 0.72f, TXT_REG, 1.5f, C4(C_BONE, 0.62f));
    float ndw = ui_text_width_tr("CONDITIONS", 0.72f, 1.5f);
    ui_text_ex(atm_now()->name, rx + ndw + 9, 66, 0.72f, TXT_BOLD, 1.5f, C4(C_BLAZE, 0.95f));
  }

  // The rail is bottom-anchored: the wordmark owns the top, the tiles sit where
  // the eye lands after it, and the block grows UPWARD when DEV TOOLS adds a
  // row so nothing under it ever moves.
  float th = 52, sh2 = 32, qh = 24, gap = 7;
  float rows = th + gap + sh2 + gap + sh2 + gap + sh2 + gap + qh + (dev ? sh2 + gap : 0);
  float y = vh * 0.80f - rows;
  float hw = (rw - gap) * 0.5f;

  // ASCII only: the HARDLINE atlas is printable ASCII plus its icon block, so
  // a typographic middot is two bytes of nothing and the caption silently
  // loses its separators.
  char sub[64];
  int gm = g_cfg.sp_mode >= 0 && g_cfg.sp_mode < GM_COUNT ? g_cfg.sp_mode : GM_DM;
  char goal[20];
  if (gm == GM_GUN)      snprintf(goal, sizeof goal, "%d RUNGS", GG_LEVELS);
  else if (gm == GM_LMS) snprintf(goal, sizeof goal, "%d ROUNDS", g_cfg.sp_rounds);
  else if (gm == GM_RESCUE) snprintf(goal, sizeof goal, "3 HOSTAGES");
  else                   snprintf(goal, sizeof goal, "%d FRAGS", g_cfg.sp_frag);
  // Mode, map, opponents, goal: difficulty lives on the page behind the tile,
  // because five fields no longer fit the caption's width.
  int mp = g_cfg.sp_map >= 0 && g_cfg.sp_map <= MAP_ROTATE ? g_cfg.sp_map : 0;
  if (gm == GM_RESCUE)
    snprintf(sub, sizeof sub, "%s / COMPOUND / %d GUARDS / %s", GM_NAMES[gm],
             RESC_DIFF[g_cfg.sp_diff < 0 ? 0 : g_cfg.sp_diff > 2 ? 2 : g_cfg.sp_diff].guards, goal);
  else
  snprintf(sub, sizeof sub, "%s / %s / %d BOT%s / %s", GM_NAMES[gm], MAP_PICK_NAMES[mp],
           g_cfg.sp_bots, g_cfg.sp_bots == 1 ? "" : "S", goal);
  // TWO DESTINATIONS, AND NEITHER OF THEM IS "GO BACK". The rail used to open
  // with a PLAY/RESUME tile above ONLINE and MATCH, which is three ways of
  // saying "a session" stacked on top of each other — and the top one meant
  // two different things depending on whether a match had begun. The screen is
  // a MODE CHOICE now: singleplayer or multiplayer, named the way every other
  // game names them, and the way back into a running match is the key that got
  // you here (ESC / B, see the chip line below). That is what makes the rail
  // safe: no tile on this screen can end a round by being pressed once.
  //
  // SINGLEPLAYER ROUTES rather than starting, and its caption is the ruleset it
  // would start with, so the press that opens the page is also the press that
  // shows you what you are about to play. The page's own first row is the START
  // button — two presses from boot to bullets, one more than the old PLAY tile
  // and one fewer than PLAY-then-MATCH-to-change-anything.
  if (ui_tile("SINGLEPLAYER", sub, rx, y, rw, th, 2.1f, click, cx, cy, 0)) {
    g_ui_arm = UI_ARM_NONE;
    ui_route(UI_PAGE_MATCH);
  }
  y += th + gap;

  {
    int st = g_cl.state;
    int busy = st == NC_CONNECTING || st == NC_JOINING || st == NC_PLAYING;
    const char *ml = busy ? "CONNECTING" : "MULTIPLAYER";
    const char *ms = st == NC_FAILED
                   ? (g_cl.reject == NR_FULL  ? "ALL ARENAS FULL"
                    : g_cl.reject == NR_PROTO ? "VERSION MISMATCH"
                    : g_cl.reject == NR_WORLD ? "ARENA MISMATCH"
                                              : "SERVER UNREACHABLE")
                   : "HOST / FIND GAMES / JOIN";
    // ...and online the same slot is the way OUT of the match, which is the one
    // destination a live session has and a boot screen does not. It keeps the
    // full width the mode tile has: it is the same slot, not a smaller one.
    if (g_online) {
      int armed = g_ui_arm == UI_ARM_LEAVE;
      if (ui_tile(armed ? "CONFIRM LEAVE" : "LEAVE MATCH",
                  armed ? (g_mp_hosting ? "THE GAME ENDS FOR EVERYONE" : "THE MATCH ENDS FOR YOU")
                        : g_mp_hosting ? "YOU ARE HOSTING - LEAVING ENDS IT" : "BACK TO SINGLEPLAYER",
                  rx, y, rw, sh2, 1.25f, click, cx, cy, 1)) {
        if (ui_arm_take(UI_ARM_LEAVE)) { leave_match(); ui_close_menu(); }
      }
    } else if (ui_tile(ml, ms, rx, y, rw, sh2, 1.25f, click, cx, cy, 0)) {
      g_ui_arm = UI_ARM_NONE;
      ui_route(UI_PAGE_ONLINE);
    }
  }
  y += sh2 + gap;

  // A TEAM GAME has a side to change: its own tile while one is running.
  if (teams_on() && !g_home && g_local_ent >= 0) {
    char tsub[48];
    snprintf(tsub, sizeof tsub, "YOU ARE %s \x05 CHANGE SIDE", TEAM_NAMES[team_of(g_local_ent) == 1]);
    if (ui_tile("TEAM", tsub, rx, y, rw, sh2, 1.25f, click, cx, cy, 0))
      ui_route(UI_PAGE_TEAM);
    y += sh2 + gap;
  }

  // THE ARMORY: what you carry. Its caption is the current loadout, so the
  // rail says what you will see in your hands before you open the page.
  {
    char asub[64];
    snprintf(asub, sizeof asub, "%s / %s / %s KNIFE",
             RIFLE_VAR_NAMES[armory_idx(g_cfg.rifle_var, WPN_VARS)],
             PISTOL_VAR_NAMES[armory_idx(g_cfg.pistol_var, WPN_VARS)],
             KNIFE_MODEL_NAMES[armory_idx(g_cfg.knife_model, KNIFE_MODEL_COUNT)]);
    if (ui_tile("ARMORY", asub, rx, y, rw, sh2, 1.25f, click, cx, cy, 0))
      ui_route(UI_PAGE_ARMORY);
    y += sh2 + gap;
  }

  if (ui_tile("SETTINGS", "VIDEO / AUDIO", rx, y, hw, sh2, 1.25f, click, cx, cy, 0))
    ui_route(UI_PAGE_SETTINGS);
  if (ui_tile("CONTROLS", "KEYS / PAD", rx + hw + gap, y, hw, sh2, 1.25f,
              click, cx, cy, 0))
    ui_route(UI_PAGE_CONTROLS);
  y += sh2 + gap;

  if (dev) {
    if (ui_tile("DEV TOOLS", "TUNING", rx, y, rw, sh2, 1.25f, click, cx, cy, 1))
      ui_route(UI_PAGE_DEV);
    y += sh2 + gap;
  }

  if (ui_tile(g_ui_arm == UI_ARM_QUIT ? "CONFIRM QUIT" : "QUIT", NULL,
              rx, y, rw, qh, 1.1f, click, cx, cy, 1)) {
    if (ui_arm_take(UI_ARM_QUIT)) { config_write(g_cfg_path, &g_cfg); g_ui_quit = 1; }
  }
  y += qh + gap;

  y += 12 - gap;

  // The pad grammar, and only in pad mode: the tiles say what they do, so a
  // permanent "CLICK TO PLAY" would be noise, while a controller has no visible
  // affordance at all until something names its buttons.
  if (g_ui_pad_mode) {
    const char *ph = g_nav_kbd ? "ARROWS MOVE   ENTER OK"
                   : g_ui_arm ? "A CONFIRMS" : "A OK   D-PAD MOVE";
    // THE HINT LIVES IN THE GAP, NOT ABOVE IT. `y` is the name line and the
    // tile block ends at y - 12, so the old y - 13 asked for a line one unit
    // ABOVE the block's own bottom edge — and a stroke overshoots its box by
    // about a unit, so two units of the hint's cap height were drawn INSIDE the
    // QUIT tile (measured: 333 px of the tile's own rect changed the moment pad
    // mode turned on). The free band is [y-12, y-1] and the ink is 7 units
    // tall, so y - 9 centres it with two units of air either side. The name
    // line may NOT move to make room: the hint is pad-only, and a rail that
    // reflows when a controller is touched is worse than a collision.
    // The band is measured against the FOCUS RING, not the tile: the ring is
    // drawn at fy-1.5 by fh+3, so a focused QUIT reaches 1.5 units past its own
    // bottom edge — and QUIT focused is exactly the state the hint is most
    // about. Ring bottom 289.5, name ink 299.0, ink 7 units tall: y - 8.25 is
    // the centre of what is left, 1.25 units of air either side. That is all
    // the band has; more air needs the footer line to move, which is a layout
    // change and not this fix.
    ui_text_ex(ph, rx, y - 8.25f, 0.75f, TXT_REG, 1.5f, C4(C_HAZE, 0.75f));
  }
  ui_text_ex(g_cfg.name, rx, y, 0.85f, TXT_BOLD, 1.6f, C4(C_BONE, 0.72f));
  float nw = ui_text_width_tr(g_cfg.name, 0.85f, 1.6f);
  char vl[40];
  snprintf(vl, sizeof vl, "V %s", BUILD_VERSION);
  ui_text_ex(vl, rx + nw + 12, y + 1, 0.7f, TXT_REG, 1.0f, C4(C_HAZE, 0.5f));

  // The state chip, on the same predicate the tick reads so it can never
  // say PAUSED while the world runs. The boot screen shows nothing: nothing is
  // paused there, the camera is live, and a chip saying so would be a label on
  // a screen that has no state to report.
  // IT GOES UNDER THE HAIRLINE, NOT IN THE WORDMARK'S OWN BOX. Right-aligned to
  // the rail at y 32 it was drawn ON TOP of the title: the plate spanned
  // x 179.3-242 by y 32-47, and the wordmark spans x 34-246.7 by y 30-51.7 (cap
  // height is scale * 7, so 3.1 is 21.7 units tall) — the chip covered the last
  // letters of the name. Two things decide where it goes instead. The band between
  // the rule and the tiles is EMPTY by construction, because the tile block is
  // bottom-anchored at vh * 0.80 and grows upward (y 59.2 to ~127 at vh 360, and
  // more on a taller viewport, never less). And it is LEFT-aligned, because the
  // rail's right edge is not an axis anything else on this screen uses — the
  // wordmark overruns HOME_RAIL_W by 4.7 units — while its left edge carries the
  // wordmark, the rule and every tile. The plate's edge lands on rx and the type
  // keeps the 8-unit inset it had.
  int chip = 1;
#ifndef _WIN32
  chip = !g_headless;   // a proof shot carries no label at all
#endif
  if (chip && !g_home) {
    const char *cl = session_pauses() ? "PAUSED" : g_online ? "LIVE" : NULL;
    if (cl) {
      float clw = ui_text_width_tr(cl, 0.85f, 1.8f);
      ui_rect(rx, 65, clw + 16, 15, 0.02f, 0.03f, 0.04f, 0.55f);
      ui_text_ex(cl, rx + 8, 69, 0.85f, TXT_BOLD, 1.8f,
                 C4(C_BONE, session_pauses() ? 0.85f : 1.0f));
      // ...AND THE WAY BACK IN, because no TILE says it any more. Removing
      // RESUME is what makes the rail unambiguous (nothing on it can end a
      // round), and the price is that "how do I get back to my match" has to be
      // written down somewhere. It goes NEXT TO THE CHIP rather than in the
      // footer: the chip is the state, this is the exit from that state, and the
      // footer band is the menu's own grammar line. Keyed on the DEVICE the
      // menu is being driven with, exactly like the footer hint — a handheld
      // player has no ESC, and a desktop player driving the menu with the arrow
      // keys (g_nav_kbd) has no B.
      const char *back = g_ui_pad_mode && !g_nav_kbd ? "B RETURNS TO MATCH"
                                                     : "ESC RETURNS TO MATCH";
      ui_text_ex(back, rx + clw + 24, 69, 0.7f, TXT_REG, 1.0f, C4(C_HAZE, 0.7f));
    }
  }
  if (g_home) ui_home_cards(vw, vh);
}

// THE ON-SCREEN KEYBOARD, drawn LAST and over a scrim, because it is a modal
// and the scrim is what says so. Everything in it is the menu's own vocabulary:
// the machined chamfered panel, and focus carried by VALUE — the lit key takes
// the pale fill and dark type that ui_button and ui_tile already use for the
// same state. No accent edge on any key. Thirty-six coloured cells is exactly
// the twitching rail ui_tile's comment argues against, and there is no third
// state here for an accent to mean.
static void ui_keyboard(int click, float cx, float cy) {
  const float kw = 340, pad = 13, gap = 3, keyh = 22, nameh = 26;
  float cellw = (kw - 2 * pad - (UI_KB_COLS - 1) * gap) / UI_KB_COLS;
  float actw  = (kw - 2 * pad - gap) * 0.5f;
  float body  = 12 + 18 + nameh + 12 + UI_KB_ROWS * (keyh + gap) - gap + 13;
  float kx = g_ui_vw * 0.5f - kw * 0.5f;
  float ky = g_ui_vh * 0.5f - body * 0.5f;
  if (ky < 4) ky = 4;

  // The page behind stays visible as CONTEXT and stops competing for the eye.
  // 0.80 rather than the 0.62 this started at: the settings panel is a stack of
  // sliders whose right-hand value column is bright type on a dark plate, and
  // at the lighter scrim those numbers read straight through the modal and sat
  // in the gaps between the keys.
  ui_rect(0, 0, g_ui_vw, g_ui_vh, 0.012f, 0.018f, 0.030f, 0.80f);
  ui_panel(kx, ky, kw, body, 0, 1);

  float y = ky + 12;
  ui_textb("PLAYER NAME", kx + pad, y, 1.1f, C_BONE, 0.92f);
  y += 18;

  // The field. The name is a RUNTIME string, so it is fitted to the box rather
  // than trusted to be short — 15 glyphs at the authored size overrun a 4:3
  // viewport's narrower virtual width, which is the failure ui_text_fit exists
  // for. The scale is solved here rather than delegated because the CARET has
  // to be placed at the end of the drawn text, and that needs the width the
  // text was actually drawn at.
  ui_rrect_c(kx + pad, y, kw - 2 * pad, nameh, 0,
             C4(1, 1, 1, 0.055f), C4(1, 1, 1, 0.030f));
  {
    float sc = 2.2f, trk = 0.9f;
    for (int i = 0; i < 8 &&
         ui_text_width_tr(g_cfg.name, sc, sc * trk) > kw - 2 * pad - 30; i++)
      sc *= 0.94f;
    float tx = kx + pad + 11, ty = y + (nameh - sc * 7.0f) * 0.5f;
    float tw = ui_text_width_tr(g_cfg.name, sc, sc * trk);
    ui_text_ex(g_cfg.name, tx, ty, sc, TXT_BOLD, sc * trk, C4(C_BONE, 1));
    // A STATIC caret, deliberately not a blinking one: g_ui_time advances once
    // per RENDERED frame, so a blink would make every harness `uiframe` and
    // `shot` of this screen depend on how many frames happened to precede it.
    ui_rect(tx + tw + (tw > 0 ? 3.0f : 0.0f), ty, 2, sc * 7.0f, C_BONE, 0.55f);
  }
  y += nameh + 12;

  for (int r = 0; r < UI_KB_ROWS; r++) {
    int n = ui_kb_cols(r);
    float w = r < UI_KB_GROWS ? cellw : actw;
    for (int c = 0; c < n; c++) {
      float x = kx + pad + (float)c * (w + gap);
      int over = cx >= x && cx <= x + w && cy >= y && cy <= y + keyh;
      // Hovering ALSO moves the cursor, so putting the mouse down and picking
      // the pad back up does not teleport the highlight to wherever the grid
      // was last navigated. The guard is what keeps a mouse parked over a key
      // from fighting the pad for it.
      if (over && !g_ui_pad_mode) { g_ui_kb_r = r; g_ui_kb_c = c; }
      int lit = over || ((r == g_ui_kb_r && c == g_ui_kb_c) && g_ui_pad_mode);
      char lab[8];
      if (r < UI_KB_GROWS) { lab[0] = UI_KB_ROW[r][c]; lab[1] = '\0'; }
      else snprintf(lab, sizeof lab, "%s", c == 0 ? "DELETE" : "DONE");
      if (lit)
        ui_rrect_c(x, y, w, keyh, 0, C4(0.96f, 0.97f, 0.98f, 0.96f),
                   C4(0.86f, 0.89f, 0.92f, 0.96f));
      else
        ui_rrect_c(x, y, w, keyh, 0, C4(1, 1, 1, 0.050f), C4(1, 1, 1, 0.032f));
      float ls = r < UI_KB_GROWS ? 1.5f : 1.0f;
      float lw = ui_text_width(lab, ls);
      ui_text_ex(lab, x + (w - lw) * 0.5f, y + (keyh - ls * 7.0f) * 0.5f, ls,
                 lit ? TXT_BLACK : TXT_BOLD,
                 0, lit ? C4(0.03f, 0.04f, 0.05f, 1) : C4(C_BONE, 0.88f));
      if (click && over) ui_kb_press(r, c);
    }
    y += keyh + gap;
  }
}

static void ui_menu(void) {
  static int prev_down;
  static float pmx = -1e9f, pmy;
  // DEV is a runtime opt-in behind the existing compile-time and config gate.
  int dev = DEV_MENU && g_cfg.devmode;
  if (g_ui_page == UI_PAGE_DEV && !dev) ui_route(UI_PAGE_ROOT);

  // Pad navigation first, so the layout below already reflects the page and
  // focus these events produced. OK/LEFT/RIGHT are parked in g_nav_act for
  // whichever row holds the focus. While a rebind
  // capture or the name field is armed, navigation is suspended and B backs
  // out — the same contract ESC has.
  for (int i = 0; i < g_nav_n; i++) {
    int ev = g_nav_q[i];
    g_ui_pad_mode = 1;
    if (g_ui_kb) {
      ui_kb_nav(ev);
    } else if (g_ui_name_edit) {
      // A PAD EVENT RAISES THE KEYBOARD THE FIELD SHOULD HAVE HAD. g_ui_kb is
      // decided by the device that ARMED the field, and on a handheld the
      // touchscreen and the trackpad are cursor mode — so tapping the field
      // and then reaching for the stick used to hit a footer reading "TYPE ON
      // KEYBOARD", which is the exact dead end ui_keyboard exists to abolish.
      // A player at an actual keyboard (g_nav_kbd) still just types.
      // B STILL MEANS LEAVE ON THE TRANSITION FRAME. Inside the grid B is
      // backspace, but here the grid is not up yet, so raising it AND handing
      // it the same NAV_BACK would open the keyboard and delete a character
      // from one press.
      if (ev == NAV_BACK || ev == NAV_OK) {
        ui_name_key("enter");
      } else if (!g_nav_kbd) {
        g_ui_kb = 1;
        g_ui_kb_r = g_ui_kb_c = 0;
        ui_kb_nav(ev);
      }
    } else if (g_ui_rebind >= 0) {
      if (ev == NAV_BACK) g_ui_rebind = -1;
    } else switch (ev) {
      // All four directions through ONE mover: up/down and left/right differ in
      // the axis they search, not in what they mean. LEFT/RIGHT still belongs
      // to the focused row's VALUE first — a slider that could be left
      // sideways is a slider the pad cannot set — so it navigates only on a row
      // that does not spend it.
      case NAV_LEFT: case NAV_RIGHT:
        if (nav_focus_lr()) { g_nav_act = ev; break; }
        [[fallthrough]];
      case NAV_UP: case NAV_DOWN:
        nav_move(ev);
        g_ui_arm = UI_ARM_NONE;
        break;
      // LB/RB and PgUp/PgDn no longer cycle hidden global tabs. Visible local
      // selectors on Controls and Dev own all lateral navigation.
      case NAV_TABL: case NAV_TABR:
        g_ui_arm = UI_ARM_NONE;
        break;
      case NAV_BACK:
        if (g_ui_page == UI_PAGE_ROOT) ui_close_menu();
        else ui_route(g_ui_page == UI_PAGE_HOST ? UI_PAGE_ONLINE : UI_PAGE_ROOT);
        break;
      default: g_nav_act = ev; break;
    }
  }
  g_nav_n = 0;
  if (!g_menu_open) { g_nav_act = -1; return; }  // B closed it this frame

  int click = g_ui_mdown && !prev_down;
  prev_down = g_ui_mdown;
  // The on-screen keyboard is a MODAL, so it owns the frame's click. Without
  // this a press on a key also lands on whatever page row is behind it — the
  // same reason every control gates its own click on `g_ui_rebind < 0`, done
  // once here instead of once per widget.
  int kbclick = click;
  if (g_ui_kb) click = 0;
  if (!g_ui_mdown) g_ui_drag = 0;
  float cx = g_ui_cx / g_uis, cy = g_ui_cy / g_uis;
  // Real mouse motion or a click hands the menu back to the cursor.
  if (pmx > -1e8f && (fabsf(cx - pmx) > 0.5f || fabsf(cy - pmy) > 0.5f))
    g_ui_pad_mode = 0;
  if (click) g_ui_pad_mode = 0;
  pmx = cx; pmy = cy;
  g_ui_nfocus = 0;
  g_ui_tab = (int)g_ui_page;
  char buf[16];

  // THE ENTRY SCREEN IS NOT A PAGE OF THIS PANEL — boot OR pause. Every other
  // page is a form (a list of controls under a title) and the panel is the
  // right container for one; the entry screen is six destinations, so it
  // replaces the container rather than living inside it. There is now exactly
  // ONE of it: pressing ESC in a match used to drop into a completely different
  // design from the one the player entered the game through, which is the kind
  // of seam a menu never recovers from. The two differ in three things — what
  // the first tile says, whether the camera is running behind it, and whether
  // the word PAUSED appears — and in nothing else.
  if (g_ui_page == UI_PAGE_ROOT) {
    ui_home(click, cx, cy);
  } else {

  // Only a PAUSING session dims the world — a networked session keeps
  // world and HUD readable behind the panel (you are standing in the open).
  // Over the frosted backdrop (drawn by ui_menu_backdrop when available) a
  // graded scrim seats the panel; without blur the scrim carries it alone.
  if (session_pauses()) {
    float base = g_ui_backdrop ? 0.20f : 0.45f;
    ui_vgrad(0, 0, g_ui_vw, g_ui_vh, C4(0.016f, 0.024f, 0.043f, base + 0.12f),
             C4(0.016f, 0.024f, 0.043f, base));
  }

  // Every page shares one top anchor. Short pages end where their content ends;
  // the dense binding pages use the full authored height without moving the
  // title when the player changes page.
  #define UI_FOOTER_H 24.0f
  #define UI_PANEL_MAX 348.0f
  #define UI_BIND_ROW  11.8f   // see the keyboard binding page's loop
  // Self-sizing across FRAMES now, not within one: a rounded panel cannot be
  // bottom-patched the way the old flat quad was (ui_rect_bottom rewrote three
  // vertices; ui_panel is a hundred). The height cache is per page and the
  // layout is stable per state, so the worst case is one frame of lag after a
  // row appears — invisible, and the hit-tests never read ph.
  static float ph_page[UI_PAGE_COUNT] = {276, 200, 319, 344, 344, 200, 260, 200, 300};
  ui_page_t drawn_page = g_ui_page;
  float pw = 340, px = g_ui_vw * 0.5f - pw * 0.5f;
  float ph = ph_page[drawn_page];
  // THE TOP IS ANCHORED, ONLY THE BOTTOM MOVES. Centring each page's own height
  // would slide the title by up to 118 px. The anchor is the tallest page's
  // centring, so the chrome is nailed down and a short page simply ends sooner.
  float py = (g_ui_vh - UI_PANEL_MAX) * 0.5f;
  if (py < 6) py = 6;
  ui_panel(px, py, pw, ph, 0, 1);
  // ROOT is not one of these any more: this panel draws FORMS, and the entry
  // screen is not a form. Its slot stays so the array indexes by page.
  static const char *const PAGE_TITLE[UI_PAGE_COUNT] = {
    "", "SINGLEPLAYER", "SETTINGS", "CONTROLS", "DEV TOOLS", "ARMORY", "MULTIPLAYER",
    "CHOOSE TEAM", "HOST SETTINGS"
  };
  ui_textb(PAGE_TITLE[drawn_page], px + 16, py + 9, 1.6f, C_BONE, 1);
  ui_rect(px + 16, py + 28, pw - 32, 1, C_BONE, 0.14f);
  // The label reads the SAME predicate the tick does, so it can never
  // say PAUSED while the world ticks or LIVE while it is frozen. A networked
  // session shows LIVE where PAUSED would be — the same key means "the world
  // stops" offline and "you are standing in the open" online, and the player
  // must be told which. The harness prints neither (a proof shot carries no
  // lying label).
  int label = 1;
#ifndef _WIN32
  label = !g_headless;   // a proof shot carries no label at all
#endif
  if (label && session_pauses()) {
    float lw2 = ui_text_width_tr("PAUSED", 0.85f, 1.2f);
    ui_rrect_line(px + pw - 24 - lw2, py + 9, lw2 + 12, 12, 0, 0.5f, C_BLAZE, 0.7f);
    ui_text_ex("PAUSED", px + pw - 18 - lw2, py + 12, 0.85f, TXT_REG, 1.2f,
               C4(C_BLAZE, 0.95f));
  } else if (label && g_online) {
    float lw2 = ui_text_width_tr("LIVE", 0.85f, 1.2f);
    float bl = 0.7f + 0.3f * sinf(g_ui_time * 4.0f);
    ui_rrect_c(px + pw - 24 - lw2 - 8, py + 9, lw2 + 20, 12, 0,
               C4(C_THREAT, 0.9f), C4(0.80f, 0.18f, 0.13f, 0.9f));
    ui_diskn(px + pw - 24 - lw2 - 1, py + 15, 2, 10, C4(1, 1, 1, bl));
    ui_textb("LIVE", px + pw - 18 - lw2 + 2, py + 12, 0.85f, 1, 1, 1, 1);
  }

  float y = py + 34;
  if (drawn_page == UI_PAGE_CONTROLS) {
    static const char *const CONTROL[2] = {"KEYBOARD + MOUSE", "GAMEPAD"};
    ui_subsel(CONTROL, 2, &g_ui_control, px, pw, y, click, cx, cy);
    y += 18;
  }
  if (drawn_page == UI_PAGE_CONTROLS && g_ui_control == 0) {
  ui_section("BINDINGS", px, pw, y);
  y += 14;
  // UI_BIND_ROW, not the 15 every other list uses: IN_COUNT rows plus the
  // header and Back/hint block do not fit a 360-unit screen at 15, and what
  // that looked like was the last bind drawn straight through the permanent
  // version line — both illegible, on the row a player is most likely to be
  // rebinding last. 11.8 fits both bind pages with the five weapon slots.
  for (int i = 0; i < IN_COUNT; i++, y += UI_BIND_ROW) {
    // zebra bridges the dead middle between label and key chip
    if ((i & 1) == 0) ui_rect(px + 12, y, pw - 24, UI_BIND_ROW, C_BONE, 0.03f);
    int armed = g_ui_rebind == i && !g_ui_rebind_pad;
    const char *tok = armed ? "PRESS KEY" : g_cfg.bind[i];
    if (ui_pill(ACTION_DEF[i].label, tok, px, pw, y, armed, 0, click, cx, cy)) {
      g_ui_rebind = i;
      g_ui_rebind_pad = 0;
    }
  }
  y += 3;
  if (ui_button("RESET KEYS", px + 16, y, pw - 32, 15, click, cx, cy, 0))
    ui_binds_reset(0);
  y += 18;
  // (The ROOT page is not drawn by this panel any more — it is the tile
  //  screen, boot or pause; see ui_home. The list that used to live here
  //  was the last thing in the game with two designs for one screen.)
  } else if (drawn_page == UI_PAGE_SETTINGS) {
  {
    static const char *const SETSUB[2] = {"GENERAL", "HUD"};
    ui_subsel(SETSUB, 2, &g_ui_set_sub, px, pw, y, click, cx, cy);
    y += 18;
  }
  if (g_ui_set_sub == 1) {
    // THE HUD TAB. The crosshair is the one HUD element on screen every frame
    // of a match, and the shipped bone-white cross is the only mark a player
    // with a red-green deficiency, or on a bright desert map, was offered.
    ui_section("CROSSHAIR", px, pw, y);
    y += 14;
    ui_cycle("STYLE", CH_STYLE_NAMES, CH_STYLE_COUNT, &g_cfg.ch_style, px, pw, y, click, cx, cy);
    y += 15;
    ui_cycle("COLOUR", CH_COLOR_NAMES, CH_COLOR_COUNT, &g_cfg.ch_color, px, pw, y, click, cx, cy);
    y += 15;
    { param_def_t cd = {"", "SIZE", 1.0f, 0.5f, 2.0f, 0.1f};
      y = ui_param_row(&cd, &g_cfg.ch_size, DRAG_CHSIZE, px, y, click, cx, cy); }
    y += 4;
    ui_section("HUD", px, pw, y);
    y += 14;
    { float pct = g_cfg.hud_scale * 100.0f;
      param_def_t sd = {"", "SCALE", 100.0f, 75.0f, 150.0f, 5.0f};
      y = ui_param_row(&sd, &pct, DRAG_HUDS, px, y, click, cx, cy);
      g_cfg.hud_scale = pct * 0.01f; }
    { float pct = g_cfg.hud_alpha * 100.0f;
      param_def_t od = {"", "OPACITY", 100.0f, 20.0f, 100.0f, 5.0f};
      y = ui_param_row(&od, &pct, DRAG_HUDA, px, y, click, cx, cy);
      g_cfg.hud_alpha = pct * 0.01f; }
    ui_toggle("CONTROL HINTS", &g_cfg.hints, px, pw, y, click, cx, cy);
    y += 15;
    ui_toggle("ENEMY MARKERS", &g_cfg.enemy_mark, px, pw, y, click, cx, cy);
    y += 15;
    ui_toggle("PERF STATS", &g_cfg.perf, px, pw, y, click, cx, cy);
    y += 15;
  } else {
  // IDENTITY FIRST. The name field used to sit on the Singleplayer page, which
  // is where a MATCH is configured — but the name is not match property: it is
  // who you are, it persists across sessions, and online it is what every other
  // player reads on the scoreboard and in the killfeed. Settings is where a
  // player looks for it. The widget is unchanged; only its home moved.
  ui_section("PLAYER", px, pw, y);
  y += 14;
  {
    // Click it, type A-Z/0-9, backspace deletes, enter/esc done.
    char shown[16];
    // The pill's trailing '-' is its caret. While the on-screen keyboard is up
    // the modal carries a real one in a field five times the size, and two
    // carets on one screen is one caret too many.
    if (g_ui_name_edit && !g_ui_kb) snprintf(shown, sizeof shown, "%s-", g_cfg.name);
    else                            snprintf(shown, sizeof shown, "%s", g_cfg.name);
    // 40 px minimum: a two-letter name must not shrink the field to a stub.
    if (ui_pill("NAME", shown, px, pw, y, g_ui_name_edit, 40, click, cx, cy)) {
      if (g_ui_name_edit) {
        ui_name_key("enter");
      } else {
        g_ui_name_edit = 1;
        // THE PAD BRINGS ITS OWN KEYBOARD. Gated on the device that armed the
        // field, not on the platform: a desktop player driving this menu with
        // the ARROW KEYS (g_nav_kbd — the Steam desktop layout emulates the
        // dpad as arrows) is sitting at a keyboard and should just type, while
        // a pad on a handheld has nothing to type with. Cursor mode types too.
        g_ui_kb = g_ui_pad_mode && !g_nav_kbd;
        g_ui_kb_r = g_ui_kb_c = 0;
      }
    }
    y += 15;
  }
  y += 4;
  ui_section("MOUSE", px, pw, y);
  y += 14;
  // Both of these hand-inlined ui_param_row's body three rows above Singleplayer
  // that already uses it, repeating the px+16 / px+130 / width-100 / px+240 /
  // y+15 layout constants the helper owns. Step 0.01 selects the same "%.2f".
  { param_def_t sd = {"", "SENSITIVITY", 0.35f, 0.05f, 2.0f, 0.01f};
    y = ui_param_row(&sd, &g_cfg.sense, DRAG_SENSE, px, y, click, cx, cy); }
  // The mouse's own ADS multiplier, directly under the sensitivity it
  // multiplies — the pad page has had this pair since it existed and the mouse
  // page had only half of it. Same label as the pad's row on purpose: it is the
  // same idea, and a player who has set one knows what the other is.
  { param_def_t ad = {"", "ADS SENS", 1.0f, 0.10f, 2.0f, 0.05f};
    y = ui_param_row(&ad, &g_cfg.ads_sense, DRAG_ADS_SENSE, px, y, click, cx, cy); }

  y += 4;
  ui_section("GRAPHICS", px, pw, y);
  y += 14;
  // Step 1 selects "%.0f", which prints identically to the old "%d",(int) for the
  // integral values roundf leaves behind. FPS CAP below stays inline: its
  // top-of-track OFF sentinel is genuinely not a param row.
  { param_def_t fd = {"", "FOV", 90.0f, 60.0f, 120.0f, 1.0f};
    y = ui_param_row(&fd, &g_cfg.fov, DRAG_FOV, px, y, click, cx, cy); }

  // FPS cap: 30..500 in steps of 10; the right end of the track means OFF.
  // Not a param row (the OFF sentinel), so it registers its focus row itself.
  int capfoc = ui_focus_row(px + 10, y - 1, 320, 15, 1);
  ui_text("FPS CAP", px + 16, y + 3, 1, C_TEXT, 1);
  float cap = g_cfg.fps_cap ? (float)g_cfg.fps_cap : 510.0f;
  // A preserved out-of-range value (fps_cap 700 parses fine) enters the
  // slider's band BEFORE the step is applied: stepped first, LEFT went
  // 700 -> 690 -> clamp 510 -> the OFF sentinel — i.e. "lower the cap"
  // produced "no cap at all", the exact inversion of the input.
  cap = f_clamp(cap, 30, 510);
  int captouch = 0;
  if (ui_nav_take(capfoc, NAV_LEFT))  { cap -= 10; captouch = 1; }
  if (ui_nav_take(capfoc, NAV_RIGHT)) { cap += 10; captouch = 1; }
  cap = f_clamp(cap, 30, 510);
  cap = ui_slider(DRAG_CAP, px + 150, y, 140, cap, 30, 510, click, cx, cy);
  // Written back only on interaction — same rule as ui_param_row's snap: a
  // hand-edited fps_cap 700 (the parser allows up to 1000) must not become
  // OFF because the menu was merely opened over it.
  if (captouch || g_ui_drag == DRAG_CAP)
    g_cfg.fps_cap = cap >= 505 ? 0 : (int)(roundf(cap / 10) * 10);
  if (g_cfg.fps_cap) snprintf(buf, sizeof buf, "%d", g_cfg.fps_cap);
  else               snprintf(buf, sizeof buf, "OFF");
  ui_text(buf, px + 324 - ui_text_width(buf, 1), y + 3, 1, C_DIM, 1);
  y += 15;

  // MSAA applies on the next frame via the lazy rebuild in msaa_update.
  static const char *const MSAA_OPTS[] = {"OFF", "2X", "4X", "8X"};
  int mi = g_cfg.msaa >= 8 ? 3 : g_cfg.msaa >= 4 ? 2 : g_cfg.msaa >= 2 ? 1 : 0;
  if (ui_cycle("ANTI-ALIAS", MSAA_OPTS, 4, &mi, px, pw, y, click, cx, cy))
    g_cfg.msaa = mi ? 1 << mi : 0;
  y += 15;

  if (ui_toggle("VSYNC", &g_cfg.vsync, px, pw, y, click, cx, cy))
    platform_vsync_changed();
  y += 15;
  ui_toggle("POST EFFECTS", &g_cfg.post_fx, px, pw, y, click, cx, cy);
  y += 15;
  ui_toggle("SHOW FPS", &g_cfg.show_fps, px, pw, y, click, cx, cy);
  y += 15;
  ui_toggle("3RD PERSON", &g_cfg.third_person, px, pw, y, click, cx, cy);
  y += 15;

  y += 4;
  ui_section("AUDIO", px, pw, y);
  y += 14;
  {
    float pct = g_cfg.vol * 100.0f;
    param_def_t vd = VOL_DEF;
    vd.label = "VOLUME";
    vd.lo = 0; vd.hi = 100; vd.step = 5;
    y = ui_param_row(&vd, &pct, DRAG_VOL, px, y, click, cx, cy);
    g_cfg.vol = pct * 0.01f;
  }
  }
  } else if (drawn_page == UI_PAGE_CONTROLS) {
    // Gamepad: deadzones and aim feel on one page, the button layout on the
    // other. Everything is a plain row — the sliders and toggles ARE the
    // interface, no calibration theatre.
    static const char *const PADSUB[2] = {"STICKS + AIM", "BUTTONS"};
    ui_subsel(PADSUB, 2, &g_ui_pad_sub, px, pw, y, click, cx, cy);
    y += 18;
    if (g_ui_pad_sub == 0) {
      ui_section("STICKS", px, pw, y);
      y += 14;
      y = ui_pad_row(PAD_DZ_L, px, y, click, cx, cy);
      y = ui_pad_row(PAD_DZ_R, px, y, click, cx, cy);

      y += 4;
      ui_section("AIM", px, pw, y);
      y += 14;
      ui_cycle("RESPONSE CURVE", PAD_CURVE_OPTS, 3, &g_cfg.pad_curve, px, pw, y, click, cx, cy);
      y += 15;
      // One line of what the selected curve DOES — a bare three-way cycle
      // makes the feature undiscoverable by name alone.
      // No commas: the 5x7 font has no glyph for one and drops it silently.
      static const char *const CURVE_DESC[3] = {
        "1:1 - STICK MAPS STRAIGHT TO TURN",
        "FINE AIM NEAR CENTRE - FAST EDGES",
        "QUICK START - FINE MID - FAST EDGES",
      };
      ui_text(CURVE_DESC[g_cfg.pad_curve], px + 16, y, 1, C_DIM, 1);
      y += 12;
      y = ui_pad_row(PAD_SENS, px, y, click, cx, cy);
      y = ui_pad_row(PAD_ADS_SENS, px, y, click, cx, cy);
      ui_toggle("INVERT Y", &g_cfg.pad_inv_y, px, pw, y, click, cx, cy);
      y += 15;
      ui_toggle("AIM ASSIST", &g_cfg.pad_assist, px, pw, y, click, cx, cy);
      y += 17;
      // Three states, because "no pad" and "Steam is sitting on the pad" have
      // different fixes and only the player can apply either. Drawn on BOTH
      // sub-pages: the BUTTONS page is where a player arms a PRESS BTN capture
      // that can never complete while Steam holds the pad.
      const char *st = g_pad.connected ? "PAD CONNECTED"
                     : g_pad_denied    ? "PAD HELD BY STEAM - PICK GAMEPAD LAYOUT"
                                       : "NO PAD DETECTED";
      if (g_pad.connected)
        ui_text(st, px + (pw - ui_text_width(st, 1)) * 0.5f, y, 1, C_DIM, 1);
      else
        ui_text(st, px + (pw - ui_text_width(st, 1)) * 0.5f, y, 1, C_ACCENT, 1);
      y += 7;    // past the status line: `y` is what sizes the panel
    } else {
      // Movement rows LAST: the stick moves you, so FORWARD/BACK/LEFT/RIGHT
      // are legitimately "--" — leading with four unbound rows reads as a
      // broken screen, and the section title says where movement actually is.
      static const int PAD_ROW[IN_COUNT] = {
        IN_JUMP, IN_CROUCH, IN_LEAN_L, IN_LEAN_R, IN_FIRE, IN_ADS, IN_RELOAD,
        IN_SWAP, IN_SLOT1, IN_SLOT2, IN_SLOT3, IN_SLOT4, IN_SLOT5, IN_SCORE, IN_VIEW,
        IN_FWD, IN_BACK, IN_LEFT, IN_RIGHT,
      };
      // The pad status belongs on THIS page too — a player arming a PRESS BTN
      // capture that can never complete while Steam holds the pad deserves to
      // be told here, not one sub-page away. It goes IN the section title
      // rather than beside it: ui_section rules off to the panel edge, so a
      // right-aligned warning was struck through by its own separator and then
      // clipped by the panel.
      ui_section(g_pad.connected  ? "BUTTONS"
                 : g_pad_denied   ? "BUTTONS - PAD HELD BY STEAM"
                                  : "BUTTONS - NO PAD DETECTED",
                 px, pw, y);
      y += 14;
      // The four movement rows are GONE from this page, not merely last:
      // the stick moves you, and four dead "--" chips read as a broken
      // screen. Hand-binding movement buttons stays possible in the config.
      for (int k = 0; k < IN_COUNT - 4; k++, y += UI_BIND_ROW) {
        int i = PAD_ROW[k];
        if ((k & 1) == 0) ui_rect(px + 12, y, pw - 24, UI_BIND_ROW, C_BONE, 0.03f);
        int armed = g_ui_rebind == i && g_ui_rebind_pad;
        const char *tok = armed ? "PRESS BTN" : pad_disp(g_cfg.padbind[i]);
        if (ui_pill(ACTION_DEF[i].label, tok, px, pw, y, armed, 0, click, cx, cy)) {
          g_ui_rebind = i;
          g_ui_rebind_pad = 1;
        }
      }
      y += 2;
      ui_text_ex("MOVE + AIM LIVE ON THE STICKS", px + 16, y, 0.8f, TXT_REG, 1.0f,
                 C4(C_HAZE, 0.8f));
      y += 11;
      if (ui_button("RESET BUTTONS", px + 16, y, pw - 32, 15, click, cx, cy, 0))
        ui_binds_reset(1);
      y += 17;
    }
  } else if (drawn_page == UI_PAGE_MATCH) {
    // SINGLEPLAYER — the mode's own page, and since the entry screen stopped
    // carrying a PLAY tile this is also where a match is started. The ACTION
    // COMES FIRST, above the rules it uses, and that ordering is an input
    // decision rather than a taste: the page is reached by pressing
    // SINGLEPLAYER, so a pad lands on row 0 with the focus already on START and
    // the boot-to-bullets path is two presses. With the button in its
    // conventional place at the foot it would be four D-pad presses past three
    // sliders, every time.
    // NO SECTION HEADER OVER THE BUTTON: the panel's own title already says
    // SINGLEPLAYER two lines above it, and a rule that repeats the title is a
    // second heading for one thing.
    y += 4;
    // ONE BUTTON, TWO LABELS, and the difference is the one thing a player must
    // not have to guess: at the boot screen there is no round to lose, in a
    // pause there is. RESTART is honest about what it does to the score, and it
    // is the only control in the menu that can end a running match — which is
    // exactly why it lives on a page you have to walk to, instead of on the
    // rail where the old PLAY/RESUME tile sat.
    //
    // TWO PATHS, AND THE ONLINE ONE MUST NOT TAKE BOTH. restart_req is serviced
    // by the next sim tick and match_tick never runs online, so an online press
    // has to leave first — and leave_match's LAST statement is already
    // match_restart(), a fresh local arena, which is precisely what this button
    // is asking for. Arming the flag on top of it would restart the match twice
    // (and regenerate the map twice) for one press. The `else` is the whole
    // difference; see leave_match's own comment for why that call is where the
    // fresh arena comes from.
    // AT BOOT IT IS ONE PRESS, IN A PAUSE IT IS TWO. The page is reached by
    // pressing SINGLEPLAYER, so a pad lands on this row with the focus already
    // on it — which is what makes boot-to-bullets two presses, and which also
    // put the only control that can end a live round one press from where the
    // focus always starts. Same latch, same THREAT treatment as QUIT.
    // Online the label has to say what actually happens: leave_match's last
    // statement is match_restart(), i.e. a fresh LOCAL arena, so an online
    // press disconnects first. Arming restart_req on top of that would restart
    // the match — and regenerate the map — twice for one press.
    // ONE PRESS ONLY WHERE THERE IS NOTHING TO LOSE, and `g_home` alone does not
    // say that: --connect kicks a join at boot, so the boot screen can be up
    // with g_online already set — a trap worth spelling
    // out. Online the action must ALWAYS be leave_match: restart_req is
    // serviced by match_tick, which never runs online, so a fast path here
    // armed a flag nothing consumes and closed the menu on a button that then
    // did nothing at all.
    int one_press = g_home && !g_online;
    int rarm = g_ui_arm == UI_ARM_RESTART;
    const char *ml = one_press ? "START MATCH"
                   : g_online  ? (rarm ? "CONFIRM LEAVE" : "LEAVE SERVER AND START LOCAL")
                   : rarm      ? "CONFIRM RESTART" : "RESTART MATCH";
    if (ui_button(ml, px + 16, y, pw - 32, 18, click, cx, cy, !one_press)) {
      if (one_press || ui_arm_take(UI_ARM_RESTART)) {
        if (g_online) leave_match();
        else g_match.restart_req = 1;   // serviced by the next sim tick
        ui_close_menu();
      }
    }
    y += 24;
    ui_section("RULES", px, pw, y);
    y += 14;
    // MODE latches at the next (re)start: changing the rules of a match in
    // progress would hand somebody a promotion or a round they never earned.
    // So the row says so while the choice is still pending, right under the
    // RESTART button that applies it.
    ui_cycle("MODE", GM_NAMES, GM_COUNT, &g_cfg.sp_mode, px, pw, y, click, cx, cy);
    y += 15;
    // MAP latches the same way. ROTATE steps to the next map every match.
    // HOSTAGE RESCUE is played on its own map (map_pick_apply), so the row is
    // a fixed label there, as on HOST SETTINGS: a picker that did nothing.
    if (g_cfg.sp_mode == GM_RESCUE) {
      ui_text("MAP", px + 16, y + 3, 1, C_TEXT, 1);
      ui_text("COMPOUND", px + pw - 16 - ui_text_width("COMPOUND", 1), y + 4, 1, C_HAZE, 1);
    } else {
      ui_cycle("MAP", MAP_PICK_NAMES, MAP_COUNT + 1, &g_cfg.sp_map, px, pw, y, click, cx, cy);
    }
    y += 15;
    // Latches at the next restart, like MODE and MAP.
    ui_toggle("BALLISTICS", &g_cfg.ballistics, px, pw, y, click, cx, cy);
    y += 15;
    if (!g_home && !g_online &&
        (g_cfg.sp_mode != g_match.mode ||
         (g_cfg.sp_mode != GM_RESCUE && g_cfg.sp_map != g_map_pick))) {
      ui_text("APPLIES ON RESTART", px + 16, y + 1, 0.9f, C_BLAZE, 0.9f);
      y += 12;
    }
    if (g_cfg.sp_mode == GM_RESCUE) {
      // A mission fields its own guards (by difficulty and squad) on its own
      // map; the squad is you and bots.
      y = ui_squad_rows(&g_cfg.sp_squad, px, pw, y, click, cx, cy);
    } else if (g_cfg.sp_mode == GM_TDM) {
      // TEAM DEATHMATCH offline: places per side (1v1 .. 9v9, bots fill them
      // all) and which side this player starts on. Bot fill and friendly fire
      // are a HOST's choices: they live on MULTIPLAYER > HOST SETTINGS.
      static const char *const TSZ[TEAM_SIZE_MAX] = {
        "1 V 1", "2 V 2", "3 V 3", "4 V 4", "5 V 5", "6 V 6", "7 V 7", "8 V 8", "9 V 9"};
      int ti = g_cfg.sp_team_size - 1;
      ui_cycle("TEAM SIZE", TSZ, TEAM_SIZE_MAX, &ti, px, pw, y, click, cx, cy);
      g_cfg.sp_team_size = ti + 1;
      y += 15;
      int was = g_cfg.sp_team;
      ui_cycle("YOUR TEAM", TEAM_NAMES, 2, &g_cfg.sp_team, px, pw, y, click, cx, cy);
      if (g_cfg.sp_team != was && teams_on() && !g_online && !g_home) {
        g_team[0] = (signed char)g_cfg.sp_team;     // redeploy on the new side
        player_respawn(&g_players[0], 0);
      }
      y += 15;
    } else {
      float v = (float)g_cfg.sp_bots;
      y = ui_param_row(&SP_BOTS_DEF, &v, DRAG_BOTS, px, y, click, cx, cy);
      g_cfg.sp_bots = (int)v;
    }
    static const char *const DIFF_OPTS[3] = {"EASY", "NORMAL", "HARD"};
    ui_cycle("DIFFICULTY", DIFF_OPTS, 3, &g_cfg.sp_diff, px, pw, y, click, cx, cy);
    y += 15;
    // The win condition is the mode's: frags, rounds, or (GUN GAME) the ladder,
    // which has no knob. The rounds slider borrows DRAG_FRAG — the two rows are
    // never on screen together.
    if (g_cfg.sp_mode == GM_LMS) {
      float v = (float)g_cfg.sp_rounds;
      y = ui_param_row(&SP_ROUNDS_DEF, &v, DRAG_FRAG, px, y, click, cx, cy);
      g_cfg.sp_rounds = (int)v;
    } else if (g_cfg.sp_mode == GM_RESCUE) {
      ui_text("WIN", px + 16, y + 3, 1, C_TEXT, 1);
      const char *gw = "ALL 3 HOSTAGES TO THE LZ";
      ui_text(gw, px + pw - 16 - ui_text_width(gw, 1), y + 4, 1, C_HAZE, 1);
      y += 15;
    } else if (g_cfg.sp_mode != GM_GUN) {
      float v = (float)g_cfg.sp_frag;
      y = ui_param_row(&SP_FRAG_DEF, &v, DRAG_FRAG, px, y, click, cx, cy);
      g_cfg.sp_frag = (int)v;
    } else {
      ui_text("WIN", px + 16, y + 3, 1, C_TEXT, 1);
      const char *gw = "KNIFE KILL ON LAST RUNG";
      ui_text(gw, px + pw - 16 - ui_text_width(gw, 1), y + 4, 1, C_HAZE, 1);
      y += 15;
    }
  } else if (drawn_page == UI_PAGE_TEAM) {
    // CHOOSE TEAM. Each side's button says how many of its places humans
    // hold; a side whose places are all taken by humans is FULL and refuses
    // the press (the server refuses it too). Choosing redeploys you on that
    // side's ground and closes the menu. Online the request is a packet and
    // the next score update is the answer.
    y += 4;
    int mine = g_local_ent >= 0 ? team_of(g_local_ent) : -1;
    for (int t = 0; t < 2; t++) {
      char lb[64];
      int hu = team_humans(t), sz = team_size();
      int full = t != mine && hu >= sz;
      snprintf(lb, sizeof lb, "%s%s   %d/%d PLAYERS%s", TEAM_NAMES[t],
               t == mine ? "  (YOU)" : "", hu, sz, full ? "  FULL" : "");
      if (ui_button(lb, px + 16, y, pw - 32, 18, click, cx, cy, 0) && !full) {
        if (g_online) net_cl_send_team(t);
        else {
          g_cfg.sp_team = t;
          if (g_team[0] != t) { g_team[0] = (signed char)t; player_respawn(&g_players[0], 0); }
        }
        ui_close_menu();
      }
      y += 22;
      // Who is on it: humans first, bots after — the names a player will see
      // over the teammate chevrons.
      char roster[96];
      int k = 0, cnt = 0;
      roster[0] = '\0';
      for (int pass = 0; pass < 2; pass++)
        ENT_FOREACH(e) {
          if (team_of(e) != t || (human_of_ent(e) >= 0) != (pass == 0)) continue;
          const char *nm = ent_name(e);
          int l = (int)strlen(nm);
          if (k + l + 2 >= (int)sizeof roster - 4) { cnt++; continue; }
          k += snprintf(roster + k, sizeof roster - (size_t)k, "%s%s", k ? "  " : "", nm);
        }
      if (cnt) snprintf(roster + k, sizeof roster - (size_t)k, " +%d", cnt);
      if (!roster[0]) snprintf(roster, sizeof roster, "EMPTY");
      ui_text_ex(roster, px + 16, y, 0.75f, TXT_REG, 1.0f, C4(C_HAZE, 0.9f));
      y += 14;
    }
    ui_text_ex(team_fill() ? "BOTS TAKE THE PLACES NO PLAYER HOLDS" : "NO BOTS IN THIS GAME",
               px + 16, y, 0.8f, TXT_REG, 1.0f, C4(C_HAZE, 0.75f));
    y += 11;
    ui_text_ex(team_ff() ? "FRIENDLY FIRE ON" : "FRIENDLY FIRE OFF",
               px + 16, y, 0.8f, TXT_REG, 1.0f, C4(C_HAZE, 0.75f));
    y += 18;
    if (ui_button("PLAY", px + 16, y, pw - 32, 15, click, cx, cy, 0)) ui_close_menu();
    y += 18;
  } else if (drawn_page == UI_PAGE_ONLINE) {
    // MULTIPLAYER: host, find, join — in that order, because the first person
    // of a group to open this page is the one who hosts, and everyone after
    // them finds the game already listed under it. A join keeps the page up
    // with its status line until the server lets us in, then the menu closes
    // on its own; a refusal stays on screen with its reason.
    static int s_wait, s_badaddr;
    mp_scan_tick();
    int st = g_cl.state;
    int busy = st == NC_CONNECTING || st == NC_JOINING;
    if (g_mp_hosting && !g_online && !busy && !host_alive()) {
      g_mp_hosting = 0;          // its server exited: the port is taken, usually
      g_mp_host_fail = 1;
    }
    if (s_wait && g_online) { s_wait = 0; ui_close_menu(); }
    if (s_wait && st == NC_FAILED) s_wait = 0;
    y += 4;
    {
      int gm = g_cfg.host_mode < 0 || g_cfg.host_mode >= GM_COUNT ? 0 : g_cfg.host_mode;
      int mi = g_cfg.host_map >= 0 && g_cfg.host_map <= MAP_ROTATE ? g_cfg.host_map : 0;
      const char *hl = !HOST_SUPPORTED ? "HOSTING NEEDS THE LINUX BUILD"
                     : busy && g_mp_hosting ? "STARTING SERVER..." : "HOST A GAME";
      int go = ui_button(hl, px + 16, y, pw - 32, 18, click, cx, cy, 0);
      if (g_mp_host_req) { g_mp_host_req = 0; go = 1; }   // HOST SETTINGS' own button
      if (go && HOST_SUPPORTED && !busy && !g_online) {
        if (UI_TEXT_EDIT) ui_name_key("enter");
        s_badaddr = 0;
        if (mp_host_start()) s_wait = 1;
      }
      y += 22;
      // The game this player would host, in one line; the row opens its page.
      char hs[80];
      if (gm == GM_TDM)
        snprintf(hs, sizeof hs, "TEAM DM / %s / %dV%d%s%s", MAP_PICK_NAMES[mi],
                 g_cfg.host_team_size, g_cfg.host_team_size,
                 g_cfg.host_team_fill ? " + BOTS" : "", g_cfg.host_team_ff ? " / FF" : "");
      else if (gm == GM_RESCUE)
        snprintf(hs, sizeof hs, "RESCUE / COMPOUND / SQUAD OF %d", g_cfg.host_squad);
      else
        snprintf(hs, sizeof hs, "%s / %s / %d BOT%s", GM_NAMES[gm],
                 MAP_PICK_NAMES[mi], g_cfg.host_bots, g_cfg.host_bots == 1 ? "" : "S");
      if (ui_pill("HOST SETTINGS", hs, px, pw, y, 0, 0, click, cx, cy) && !g_online)
        ui_route(UI_PAGE_HOST);
      y += 17;
      uint32_t la[4];
      int nla = net_local_addrs(la, 4);
      if (nla > 0) {
        char ab[24], al[64];
        mp_ip_str(la[0], ab, sizeof ab);
        snprintf(al, sizeof al, "YOUR ADDRESS  %s", ab);
        ui_text_ex(al, px + 16, y, 0.85f, TXT_REG, 1.0f, C4(C_HAZE, 0.9f));
        y += 11;
      }
      y += 8;
    }
    ui_section("GAMES ON YOUR NETWORK", px, pw, y);
    y += 14;
    if (!g_mp.n) {
      static const char *const DOTS[4] = {"SEARCHING", "SEARCHING.", "SEARCHING..", "SEARCHING..."};
      ui_text(DOTS[(int)(g_ui_time * 2.0f) & 3], px + 16, y + 3, 1, C_DIM, 1);
      y += 15;
    }
    for (int i = 0; i < g_mp.n; i++, y += 15) {
      const mp_game_t *g = &g_mp.g[i];
      char info[48];
      if (g->proto != NET_PROTO) snprintf(info, sizeof info, "UPDATE NEEDED");
      else snprintf(info, sizeof info, "%s / %s  %d/%d",
                    GM_NAMES[g->mode < GM_COUNT ? g->mode : 0],
                    MAP_PICK_NAMES[g->map <= MAP_COUNT ? g->map : 0], g->humans, g->cap);
      if (ui_pill(g->name, info, px, pw, y, 0, 0, click, cx, cy) && !busy && !g_online &&
          g->proto == NET_PROTO) {
        if (UI_TEXT_EDIT) ui_name_key("enter");
        s_badaddr = 0;
        if (mp_join(g->ip, g->port)) s_wait = 1;
      }
    }
    y += 8;
    ui_section("JOIN BY ADDRESS", px, pw, y);
    y += 14;
    {
      char shown[72];
      snprintf(shown, sizeof shown, "%s%s",
               g_cfg.mp_host[0] ? g_cfg.mp_host : g_ui_addr_edit ? "" : "TYPE AN IP",
               g_ui_addr_edit ? "-" : "");
      if (ui_pill("ADDRESS", shown, px, pw, y, g_ui_addr_edit, 60, click, cx, cy)) {
        if (g_ui_addr_edit) ui_name_key("enter");
        else { if (g_ui_name_edit) ui_name_key("enter"); g_ui_addr_edit = 1; }
      }
      y += 17;
      if (ui_button("JOIN", px + 16, y, pw - 32, 15, click, cx, cy, 0) &&
          g_cfg.mp_host[0] && !busy && !g_online) {
        if (UI_TEXT_EDIT) ui_name_key("enter");
        char host[64]; int port = g_cfg.mp_port;
        snprintf(host, sizeof host, "%s", g_cfg.mp_host);
        char *colon = strchr(host, ':');
        if (colon) { *colon = '\0'; port = atoi(colon + 1); }
        s_badaddr = !start_connect(host, port > 0 && port < 65536 ? port : g_cfg.mp_port);
        if (!s_badaddr) s_wait = 1;
      }
      y += 20;
    }
    {
      char sl[96];
      sl[0] = '\0';
      if (busy) snprintf(sl, sizeof sl, "CONNECTING...");
      else if (s_badaddr) snprintf(sl, sizeof sl, "ADDRESS NOT FOUND");
      else if (g_mp_host_fail)
        snprintf(sl, sizeof sl, "COULD NOT HOST - IS PORT %d IN USE?", g_cfg.mp_port);
      else if (st == NC_FAILED)
        snprintf(sl, sizeof sl, "%s", g_cl.reject == NR_FULL  ? "GAME FULL"
                                    : g_cl.reject == NR_PROTO ? "VERSION MISMATCH - UPDATE"
                                    : g_cl.reject == NR_WORLD ? "ARENA MISMATCH"
                                                              : "NO ANSWER FROM THAT ADDRESS");
      if (sl[0]) {
        if (busy) ui_text(sl, px + 16, y, 1, C_TEXT, 1);
        else      ui_text(sl, px + 16, y, 1, C_ACCENT, 1);
        y += 12;
      }
      char nt[96];
      snprintf(nt, sizeof nt, "OVER THE INTERNET THE HOST FORWARDS UDP %d", g_cfg.mp_port);
      ui_text_ex(nt, px + 16, y, 0.8f, TXT_REG, 1.0f, C4(C_HAZE, 0.7f));
      y += 9;
    }
  } else if (drawn_page == UI_PAGE_HOST) {
    // HOST SETTINGS: the game this player serves when they press HOST A GAME.
    // Apart from SINGLEPLAYER's mode/map/bots, so setting up a team game for
    // friends never rewrites the solo match (and the reverse). Difficulty,
    // the frag limit / rounds and ballistics are the shared match rules.
    static const int HMODES[] = {GM_DM, GM_TDM, GM_GUN, GM_SNIPER, GM_BLADES, GM_LMS, GM_RESCUE};
    enum { NHM = (int)(sizeof HMODES / sizeof HMODES[0]) };
    const char *hmn[NHM];
    int hi = 0;
    for (int i = 0; i < NHM; i++) {
      hmn[i] = GM_NAMES[HMODES[i]];
      if (HMODES[i] == g_cfg.host_mode) hi = i;
    }
    ui_section("GAME", px, pw, y);
    y += 14;
    ui_cycle("MODE", hmn, NHM, &hi, px, pw, y, click, cx, cy);
    g_cfg.host_mode = HMODES[hi];
    y += 15;
    if (g_cfg.host_mode == GM_RESCUE) {          // the mission has its own map
      ui_text("MAP", px + 16, y + 3, 1, C_TEXT, 1);
      ui_text("COMPOUND", px + pw - 16 - ui_text_width("COMPOUND", 1), y + 4, 1, C_HAZE, 1);
    } else {
      ui_cycle("MAP", MAP_PICK_NAMES, MAP_COUNT + 1, &g_cfg.host_map, px, pw, y, click, cx, cy);
    }
    y += 15;
    ui_toggle("BALLISTICS", &g_cfg.ballistics, px, pw, y, click, cx, cy);
    y += 15;
    static const char *const DIFF_OPTS[3] = {"EASY", "NORMAL", "HARD"};
    ui_cycle("BOT DIFFICULTY", DIFF_OPTS, 3, &g_cfg.sp_diff, px, pw, y, click, cx, cy);
    y += 15;
    if (g_cfg.host_mode == GM_LMS) {
      float v = (float)g_cfg.sp_rounds;
      y = ui_param_row(&SP_ROUNDS_DEF, &v, DRAG_FRAG, px, y, click, cx, cy);
      g_cfg.sp_rounds = (int)v;
    } else if (g_cfg.host_mode == GM_RESCUE) {
      ui_text("WIN", px + 16, y + 3, 1, C_TEXT, 1);
      const char *gw = "ALL 3 HOSTAGES TO THE LZ";
      ui_text(gw, px + pw - 16 - ui_text_width(gw, 1), y + 4, 1, C_HAZE, 1);
      y += 15;
    } else if (g_cfg.host_mode != GM_GUN) {
      float v = (float)g_cfg.sp_frag;
      y = ui_param_row(&SP_FRAG_DEF, &v, DRAG_FRAG, px, y, click, cx, cy);
      g_cfg.sp_frag = (int)v;
    }
    y += 4;
    if (g_cfg.host_mode == GM_TDM) {
      ui_section("TEAMS", px, pw, y);
      y += 14;
      static const char *const TSZ[TEAM_SIZE_MAX] = {
        "1 V 1", "2 V 2", "3 V 3", "4 V 4", "5 V 5", "6 V 6", "7 V 7", "8 V 8", "9 V 9"};
      int ti = g_cfg.host_team_size - 1;
      if (ti < 0 || ti >= TEAM_SIZE_MAX) ti = 4;
      ui_cycle("TEAM SIZE", TSZ, TEAM_SIZE_MAX, &ti, px, pw, y, click, cx, cy);
      g_cfg.host_team_size = ti + 1;
      y += 15;
      ui_toggle("FILL TEAMS WITH BOTS", &g_cfg.host_team_fill, px, pw, y, click, cx, cy);
      y += 15;
      ui_toggle("FRIENDLY FIRE", &g_cfg.host_team_ff, px, pw, y, click, cx, cy);
      y += 15;
      ui_text_ex("PLAYERS WHO JOIN CHOOSE ALPHA OR BRAVO", px + 16, y + 1, 0.8f, TXT_REG,
                 1.0f, C4(C_HAZE, 0.75f));
      y += 12;
    } else if (g_cfg.host_mode == GM_RESCUE) {
      ui_section("SQUAD", px, pw, y);
      y += 14;
      y = ui_squad_rows(&g_cfg.host_squad, px, pw, y, click, cx, cy);
      ui_text_ex("PLAYERS JOIN THE SQUAD; BOTS TAKE THE PLACES LEFT", px + 16, y + 1, 0.8f,
                 TXT_REG, 1.0f, C4(C_HAZE, 0.75f));
      y += 12;
    } else {
      ui_section("BOTS", px, pw, y);
      y += 14;
      float v = (float)g_cfg.host_bots;
      y = ui_param_row(&SP_BOTS_DEF, &v, DRAG_BOTS, px, y, click, cx, cy);
      g_cfg.host_bots = (int)v;
    }
    y += 6;
    if (HOST_SUPPORTED &&
        ui_button("HOST A GAME", px + 16, y, pw - 32, 18, click, cx, cy, 0) && !g_online) {
      g_mp_host_req = 1;
      ui_route(UI_PAGE_ONLINE);
    }
    y += 14;
  } else if (drawn_page == UI_PAGE_ARMORY) {
    // THE LOADOUT is the one part of this page that changes how you fight, so
    // it comes first and says when it lands. Everything below it is cosmetic.
    ui_section("LOADOUT", px, pw, y);
    y += 14;
    ui_cycle("RIFLE", RIFLE_VAR_NAMES, WPN_VARS, &g_cfg.rifle_var, px, pw, y, click, cx, cy);
    y += 15;
    ui_cycle("PISTOL", PISTOL_VAR_NAMES, WPN_VARS, &g_cfg.pistol_var, px, pw, y, click, cx, cy);
    y += 15;
    ui_text_ex("APPLIES NOW IF UNFIRED, ELSE ON NEXT SPAWN", px + 16, y + 1, 0.8f,
               TXT_REG, 1.0f, C4(C_HAZE, 0.75f));
    y += 12;
    y += 4;
    // Cosmetic only, and it says so: nothing below changes how a weapon
    // handles, and every choice is visible the moment the menu closes.
    ui_section("KNIFE", px, pw, y);
    y += 14;
    ui_cycle("MODEL", KNIFE_MODEL_NAMES, KNIFE_MODEL_COUNT, &g_cfg.knife_model,
             px, pw, y, click, cx, cy);
    y += 15;
    y += 4;
    ui_section("WEAPONS", px, pw, y);
    y += 14;
    ui_cycle("THEME", WPN_THEME_NAMES, WPN_THEME_COUNT, &g_cfg.wpn_theme,
             px, pw, y, click, cx, cy);
    y += 15;
    y += 4;
    ui_section("GEAR", px, pw, y);
    y += 14;
    ui_cycle("OUTFIT", GEAR_THEME_NAMES, GEAR_THEME_COUNT, &g_cfg.gear_theme,
             px, pw, y, click, cx, cy);
    y += 15;
    ui_cycle("HEADGEAR", HEAD_GEAR_NAMES, HEAD_GEAR_COUNT, &g_cfg.head_gear,
             px, pw, y, click, cx, cy);
    y += 15;
    ui_cycle("VEST", VEST_GEAR_NAMES, VEST_GEAR_COUNT, &g_cfg.vest_gear,
             px, pw, y, click, cx, cy);
    y += 15;
    ui_cycle("PACK", PACK_GEAR_NAMES, PACK_GEAR_COUNT, &g_cfg.pack_gear,
             px, pw, y, click, cx, cy);
    y += 15;
    // How the weapon sits in view. Here rather than under Settings > Graphics,
    // which is already as tall as a 720p screen allows.
    y += 4;
    ui_section("VIEW", px, pw, y);
    y += 14;
  { param_def_t vd = {"", "WEAPON FOV", 58.0f, 50.0f, 74.0f, 1.0f};
    y = ui_param_row(&vd, &g_cfg.vm_fov, DRAG_VMFOV, px, y, click, cx, cy); }
  if (g_cfg.vm_pos < 0 || g_cfg.vm_pos > 2) g_cfg.vm_pos = 0;
  ui_cycle("WEAPON POSITION", VM_POS_NAMES, 3, &g_cfg.vm_pos, px, pw, y, click, cx, cy);
  y += 15;
    y += 4;
    ui_text("KNIFE / THEME / GEAR / VIEW: NO GAMEPLAY EFFECT", px + 16, y + 3, 0.85f, C_DIM, 1);
    y += 14;
  } else if (drawn_page == UI_PAGE_DEV) {
    // DEV — the developer tuning workbench: every knob a PLAYER never touches
    // (movement feel, weapon feel). Lives behind `dev` so an install ships
    // without it; keeping it one page also keeps the player-facing menu
    // from reading like an engine debug panel.
    static const char *const DEVSUB[3] = {"MOVEMENT", "WEAPON", "ASSIST"};
    ui_subsel(DEVSUB, 3, &g_ui_dev_sub, px, pw, y, click, cx, cy);
    y += 18;
    if (g_ui_dev_sub == 0) {
    ui_section("SPEED", px, pw, y);
    y += 14;
    y = ui_mv_row(MV_RUN, px, y, click, cx, cy);
    y = ui_mv_row(MV_CROUCH_SP, px, y, click, cx, cy);

    y += 4;
    ui_section("FEEL", px, pw, y);
    y += 14;
    y = ui_mv_row(MV_ACCEL_MS, px, y, click, cx, cy);
    y = ui_mv_row(MV_STOP_MS, px, y, click, cx, cy);

    y += 4;
    ui_section("AIR", px, pw, y);
    y += 14;
    y = ui_mv_row(MV_JUMP_H, px, y, click, cx, cy);
    y = ui_mv_row(MV_AIR_CTL, px, y, click, cx, cy);

    y += 4;
    ui_section("SLIDE", px, pw, y);
    y += 14;
    y = ui_mv_row(MV_SLIDE_BOOST, px, y, click, cx, cy);
    y = ui_mv_row(MV_SLIDE_LEN, px, y, click, cx, cy);

    y += 4;
    ui_section("VIEW", px, pw, y);
    y += 14;
    y = ui_mv_row(MV_FOV_KICK, px, y, click, cx, cy);
    } else if (g_ui_dev_sub == 1) {
    // Weapon selector: same editor, per-weapon parameter set underneath.
    const char *wopts[WPN_COUNT];
    for (int i = 0; i < WPN_COUNT; i++) wopts[i] = WPN_DEF[i].name;
    ui_subsel(wopts, WPN_COUNT, &g_ui_wpn, px, pw, y, click, cx, cy);
    y += 18;

    ui_section("RECOIL", px, pw, y);
    y += 14;
    y = ui_wp_row(WP_KICK_UP, px, y, click, cx, cy);
    y = ui_wp_row(WP_KICK_SIDE, px, y, click, cx, cy);
    y = ui_wp_row(WP_RECOVER_MS, px, y, click, cx, cy);
    y = ui_wp_row(WP_PUNCH, px, y, click, cx, cy);

    y += 4;
    ui_section("SPREAD", px, pw, y);
    y += 14;
    y = ui_wp_row(WP_SPREAD, px, y, click, cx, cy);
    y = ui_wp_row(WP_BLOOM, px, y, click, cx, cy);

    y += 4;
    // SWAY rides in HANDLING rather than its own one-row FEEL section: the
    // header pair it had put this tab past the panel's height clamp, and the
    // footer hint chips drew over the BACK row.
    ui_section("HANDLING", px, pw, y);
    y += 14;
    y = ui_wp_row(WP_RPM, px, y, click, cx, cy);
    y = ui_wp_row(WP_ADS_MS, px, y, click, cx, cy);
    y = ui_wp_row(WP_ADS_ZOOM, px, y, click, cx, cy);
    y = ui_wp_row(WP_SWAY, px, y, click, cx, cy);
    } else {
    // ASSIST — the friction-only aim-assist workbench (see PAD_DEF's PAD_AS_*
    // comment for what each knob means). Player-facing on/off stays the
    // CONTROLS toggle; these three are tuning, not preference.
    ui_section("AIM ASSIST", px, pw, y);
    y += 14;
    y = ui_pad_row(PAD_AS_FRIC, px, y, click, cx, cy);
    y = ui_pad_row(PAD_AS_BUBBLE, px, y, click, cx, cy);
    y = ui_pad_row(PAD_AS_DIST, px, y, click, cx, cy);
    }
  }

  if (drawn_page == UI_PAGE_DEV) {
    y += 8;
    if (ui_button("RESET CURRENT", px + 16, y, pw - 32, 16, click, cx, cy, 0)) {
      if (g_ui_dev_sub == 0)
        for (int i = 0; i < MV_COUNT; i++) g_cfg.mv[i] = MV_DEF[i].def;
      else if (g_ui_dev_sub == 1)
        for (int i = 0; i < WP_COUNT; i++)
          g_cfg.wp[g_ui_wpn][i] = WP_DEFAULTS[g_ui_wpn][i];
      else
        for (int i = PAD_AS_FRIC; i <= PAD_AS_DIST; i++)
          g_cfg.pad[i] = PAD_DEF[i].def;
    }
    y += 20;
  }

  // Every focused subpage owns exactly one visible way back. NAV_BACK and ESC
  // route through the same bookkeeping without manufacturing extra controls.
  if (drawn_page != UI_PAGE_ROOT) {
    y += 8;
    if (ui_button("BACK", px + 16, y, pw - 32, 16, click, cx, cy, 0))
      ui_route(drawn_page == UI_PAGE_HOST ? UI_PAGE_ONLINE : UI_PAGE_ROOT);
    y += 18;
  }

  // Close the panel under its page content: store this frame's measured
  // height for the NEXT frame's rounded panel (see the cache comment above).
  // The maximum is authored against 1280x720's 360-unit viewport; taller
  // aspect ratios keep the same anchor.
  {
    float want = f_clamp((y - py) + UI_FOOTER_H, 150.0f,
                         g_ui_vh - py - 4.0f);
    ph_page[drawn_page] = want;
    ph = want;
  }

  // LAST, over everything, and over its own scrim — see ui_keyboard.
  if (g_ui_kb) ui_keyboard(kbclick, cx, cy);

  // Hints: the menu tells the player its own grammar, and the line follows the
  // STATE — while a capture is armed, the grammar it advertises ("B BACK")
  // would be wrong for a pad capture, where B is a bindable button and only
  // START cancels. An armed capture shows its hint in CURSOR mode too: every
  // click is deliberately inert then (nothing may bind mouse garbage or
  // disarm-by-accident), and a frozen menu with no visible way out is a
  // dead-end — ESC is the way out, and only this line says so. The wording
  // must not imply click-cancels: during a KEYBOARD capture a click BINDS
  // mouse1.
  const char *h = NULL;
  if (g_ui_rebind >= 0) {
    // The device term matches the branch below: keyboard arrows drive the
    // menu in pad mode too (g_nav_kbd), and during a KEYBOARD capture the
    // advertised "B CANCEL" would BIND the B key — ESC is the cancel there.
    h = g_ui_pad_mode && !g_nav_kbd
      ? (g_ui_rebind_pad ? "PRESS A BUTTON   START CANCEL"
                         : "PRESS A KEY   B OR ENTER CANCEL")
      : (g_ui_rebind_pad ? "PRESS A PAD BUTTON   ESC CANCEL"
                         : "PRESS A KEY OR MOUSE BUTTON   ESC OR ENTER CANCEL");
  } else if (g_ui_pad_mode) {
    // The grammar follows the DEVICE: arrow keys drive the same focus system
    // (the Steam desktop layout emulates the dpad as arrows, and a desktop
    // keyboard user has no A or LB at all), so the hint speaks whichever
    // language the last navigation event actually arrived in.
    if (g_nav_kbd)
      h = g_ui_kb        ? "ARROWS MOVE   ENTER SELECT   OR JUST TYPE"
        : g_ui_name_edit ? "TYPE A-Z 0-9   ENTER DONE"
        : g_ui_arm       ? "ENTER CONFIRMS   ESC CANCELS"
                         : "ARROWS MOVE   ENTER OK   LEFT/RIGHT ADJUST   ESC BACK";
    else
      // The keyboard's own grammar, and B is the one line that has to be spelt
      // out: it DELETES here rather than backing out, which is the one place in
      // this menu where B does not mean "leave". START is the universal exit
      // and the DONE tile is the visible one, so the way out is never hidden.
      h = g_ui_kb        ? "A SELECT   B DELETE   START DONE"
        : g_ui_name_edit ? "TYPE ON KEYBOARD   A/B DONE"
        : g_ui_arm       ? "A CONFIRMS   B CANCELS"
                         : "A OK   < > ADJUST   B BACK";
  } else if (g_ui_kb) {
    h = "CLICK A KEY   OR TYPE   ENTER DONE";
  } else if (g_ui_name_edit) {
    // Cursor mode: the accent-filled pill alone does not say typing works.
    h = "TYPE A-Z 0-9   ENTER DONE";
  } else {
    // The footer band exists on EVERY frame — an empty reserved strip reads
    // as a layout mistake, and the grammar chip doubles as the exit sign.
    h = g_ui_arm ? "CLICK AGAIN CONFIRMS   ESC CANCELS"
                      : "ESC BACK";
  }
  if (h) {
    // The hint line renders as key CHIPS, split on the triple-space grouping
    // the strings already carry — the wording stays the single copy above.
    char tmp[128];
    snprintf(tmp, sizeof tmp, "%s", h);
    const char *part[6];
    int np = 0;
    char *cur2 = tmp;
    while (np < 6 && cur2) {
      part[np++] = cur2;
      char *sep = strstr(cur2, "   ");
      if (sep) { *sep = '\0'; cur2 = sep + 3; } else cur2 = NULL;
    }
    float cwd[6], tw_all = 0;
    for (int i = 0; i < np; i++) {
      cwd[i] = ui_text_width(part[i], 0.9f) + 12;
      tw_all += cwd[i] + (i ? 6.0f : 0.0f);
    }
    float hx = px + (pw - tw_all) * 0.5f, hy = py + ph - 17;
    for (int i = 0; i < np; i++) {
      ui_rrect_c(hx, hy, cwd[i], 13, 0, C4(0, 0, 0, 0.30f), C4(0, 0, 0, 0.22f));
      ui_text(part[i], hx + 6, hy + 3, 0.9f, C_HAZE, 1);
      hx += cwd[i] + 6;
    }
  }

  }   // ...end of the panel path; the boot screen took the other branch.
  if (!g_ui_pad_mode) {
    // Software cursor — a graticule cross with an open centre, so the pixel
    // under the point stays visible. Hardware cursor stays hidden so
    // screenshots and Windows behave identically.
    for (int i = 0; i < 4; i++) {
      float dx = (i == 0) - (i == 1), dy = (i == 2) - (i == 3);
      ui_capsule(cx + dx * 2.0f, cy + dy * 2.0f, cx + dx * 6.0f, cy + dy * 6.0f,
                 1.1f, 0, 0, 0, 0.8f);
      ui_capsule(cx + dx * 2.0f, cy + dy * 2.0f, cx + dx * 6.0f, cy + dy * 6.0f,
                 0.6f, 1, 1, 1, 1);
    }
    ui_rect(cx - 0.8f, cy - 0.8f, 1.6f, 1.6f, 1, 1, 1, 0.95f);  // machined dot
  }

  // The frame's focus-row census: wrap-around in next frame's preamble uses
  // it, and a page/selector switch that shrank the list must not leave focus dangling.
  g_ui_nfocus_last = g_ui_nfocus;
  g_focus_rn = g_ui_nfocus < UI_MAX_FOCUS ? g_ui_nfocus : UI_MAX_FOCUS;
  if (g_ui_focus >= g_ui_nfocus) {
    g_ui_focus = g_ui_nfocus > 0 ? g_ui_nfocus - 1 : 0;
    nav_anchor_reset();   // a shrunk list moved the focus, not the player
  }
  g_nav_act = -1;
}

// Scoreboard — hold the SCOREBOARD bind (default TAB). One slim panel:
// name, kills, deaths, K/D and accuracy, sorted by kills (ties broken by
// fewer deaths); the player's row is tinted. Forced on when the match ends.
// THE LOCAL PLAYER'S MATCH, for the end-screen summary: what the scoreboard
// does not already carry (kills, deaths and accuracy are g_match's). `life` is
// kills since the last death; `best` the match's longest such run. Reset when
// a match goes live (see hud_match_summary), counted by vfx_on_event.
static struct { int hs, life, best, counted; } g_ms;
static float g_sb_bottom = 180.0f, g_sb_w = 300.0f;   // last board drawn
static void ui_scoreboard(void) {
  // ENT_FOREACH, not `1 + nbots` (P1-63): idx[i] = i assumed a DENSE id space
  // — humans on 21+ simply vanished from the board while empty bot slots
  // showed as 0/0 rows.
  int n = 0;
  int idx[MAX_ENTS];
  ENT_FOREACH(e) idx[n++] = e;
  // The mode's own number ranks first where it IS the score: the GUN GAME
  // rung, LAST MAN STANDING rounds. A deathmatch board is unchanged.
  int gm = game_mode();
  int mcol = gm == GM_GUN || gm == GM_LMS;
  const int *ms = g_match.mscore;
  for (int i = 1; i < n; i++) {  // insertion sort: [mode desc,] kills desc, deaths asc
    int v = idx[i], j = i;
    while (j > 0 && ((mcol && ms[idx[j - 1]] < ms[v]) ||
                     ((!mcol || ms[idx[j - 1]] == ms[v]) &&
                      (g_match.kills[idx[j - 1]] < g_match.kills[v] ||
                       (g_match.kills[idx[j - 1]] == g_match.kills[v] &&
                        g_match.deaths[idx[j - 1]] > g_match.deaths[v]))))) {
      idx[j] = idx[j - 1];
      j--;
    }
    idx[j] = v;
  }
  // TEAM DEATHMATCH: my side first, then theirs, each in score order (a
  // stable partition of the sorted list).
  if (teams_on() && g_local_ent >= 0) {
    int tmp[MAX_ENTS], k = 0, mt = team_of(g_local_ent);
    for (int pass = 0; pass < 2; pass++)
      for (int i = 0; i < n; i++)
        if ((team_of(idx[i]) == mt) == (pass == 0)) tmp[k++] = idx[i];
    memcpy(idx, tmp, sizeof(int) * (size_t)n);
  }
  // A FULL ROSTER DOES NOT FIT, and it must not be allowed to try. 28 entities
  // at 11 units plus the header is 354 against a 360-unit screen, and the
  // winner banner pushes the panel down another 40 — so the bottom rows were
  // drawn off the viewport entirely. Those are the LOSING rows, i.e. the ones a
  // player is most often looking for. Bound the list to what fits and SAY what
  // was left out; a silently truncated board reads as a board with fewer
  // players in the match.
  float top = g_match.over ? 96.0f : 24.0f;
  // ...and at match end the personal summary (hud_match_summary, 44 units)
  // needs its own room under the board.
  float sum_h = g_match.over && !rescue_view() ? 44.0f : 0.0f;
  int fit = (int)((g_ui_vh - top - 12.0f - 46.0f - sum_h) / 11.0f);
  if (fit < 1) fit = 1;
  int shown = n < fit ? n : fit;
  int hidden = n - shown;
  // Online the panel grows a PING column (the row data arrives via NP_SCORES
  // at 4 Hz); offline the layout stays byte-identical to the historical board.
  int show_ping = g_online;
  float pw = (show_ping ? 336 : 300) + (mcol ? 40 : 0),
        ph = 42 + (float)shown * 11 + 8 + (hidden ? 10.0f : 0.0f);
  float px = g_ui_vw * 0.5f - pw * 0.5f;
  // Centred on the REAL virtual height — 180 is only the centre when the
  // height is a multiple of 360 (the same trap the settings panel documents:
  // the Deck's 800 px viewport is 400 virtual units tall).
  float py = g_ui_vh * 0.5f - ph * 0.5f;
  if (py < top) py = top;
  ui_panel(px, py, pw, ph, 0, 1);
  // ONE PROCESS, MANY ARENAS: online the board says WHICH one, because
  // "am I in the same arena as my friend" is otherwise unanswerable from
  // inside the game — every lobby shares the host and the port.
  char title[48];
  if (g_online && g_cl.lobbies > 1)
    snprintf(title, sizeof title, "%s \x05 %s \x05 %d/%d", GM_NAMES[gm],
             MAP_DEFS[map_id_valid(g_map_id)].name,
             (g_cl.lobby + 1) & 31, g_cl.lobbies & 31);
  else snprintf(title, sizeof title, "%s \x05 %s", GM_NAMES[gm],
                MAP_DEFS[map_id_valid(g_map_id)].name);
  ui_textb(title, px + 12, py + 9, 1.2f, C_BONE, 1);
  char sub[32];
  // "N PLAYERS · M BOTS" makes 1-human-vs-7-bots legible instead of sad:
  // the player knows exactly what he fights, and a second human arriving is an
  // event. In a networked session the instance code sits on the line below
  // (added in the UI phase); here the composition is what matters.
  int nh = 0, nb = 0;
  ENT_FOREACH(e) { if (human_of_ent(e) >= 0) nh++; else nb++; }
  if (gm == GM_TDM) snprintf(sub, sizeof sub, "ALPHA %d \x05 BRAVO %d",
                            g_team_score[0] & 0xFFFF, g_team_score[1] & 0xFFFF);
  else if (nh > 1) snprintf(sub, sizeof sub, "%d PLAYERS \x05 %d BOTS", nh & 63, nb & 63);
  else if (gm == GM_GUN) snprintf(sub, sizeof sub, "KNIFE KILL WINS");
  else if (gm == GM_LMS) snprintf(sub, sizeof sub, "FIRST TO %d ROUNDS", mode_round_goal());
  else        snprintf(sub, sizeof sub, "FIRST TO %d",
                       g_online && g_net_fraglimit ? g_net_fraglimit
                                                   : g_cfg.sp_frag);
  ui_text_ex(sub, px + pw - 12 - ui_text_width_tr(sub, 0.9f, 0.8f), py + 10, 0.9f,
             TXT_REG, 0.8f, C4(C_HAZE, 1));
  ui_rect(px + 12, py + 24, pw - 24, 1, C_BONE, 0.14f);
  float y = py + 30;
  // Numeric columns are right-aligned to a per-column edge — the rank number
  // already was, and left-aligned digits left ragged edges ("100%" overshot
  // its header while "0%" fell short). ACC's edge is the panel's own right
  // margin, so the board closes flush like the subtitle line above it.
  float xn = px + 34, xk = px + 180, xd = px + 208, xkd = px + 244, xa = px + 288,
        xp = px + 324;
  ui_text_ex("K", xk - ui_text_width_tr("K", 0.85f, 0.8f), y, 0.85f, TXT_REG, 0.8f,
             C4(C_HAZE, 1));
  ui_text_ex("D", xd - ui_text_width_tr("D", 0.85f, 0.8f), y, 0.85f, TXT_REG, 0.8f,
             C4(C_HAZE, 1));
  ui_text_ex("K/D", xkd - ui_text_width_tr("K/D", 0.85f, 0.8f), y, 0.85f, TXT_REG,
             0.8f, C4(C_HAZE, 1));
  ui_text_ex("ACC", xa - ui_text_width_tr("ACC", 0.85f, 0.8f), y, 0.85f, TXT_REG,
             0.8f, C4(C_HAZE, 1));
  if (show_ping)
    ui_text_ex("PING", xp - ui_text_width_tr("PING", 0.85f, 0.8f), y, 0.85f,
               TXT_REG, 0.8f, C4(C_HAZE, 1));
  float xm = px + pw - 12;
  const char *mh = gm == GM_GUN ? "RUNG" : "RND";
  if (mcol)
    ui_text_ex(mh, xm - ui_text_width_tr(mh, 0.85f, 0.8f), y, 0.85f, TXT_REG, 0.8f,
               C4(C_BLAZE, 1));
  y += 10;
  // A human is MARKED, not a bot: there are one or two humans and twenty
  // bot names, so the rarer mark reads as a badge — the chevron glyph, tinted;
  // other humans go SIGNAL, the local player keeps his BLAZE highlight bar.
  // Rank numbers make the sort readable at a glance.
  int humans_present = nh;   // the same walk two lines up; one count, not two
  for (int i = 0; i < shown; i++) {
    int e = idx[i];
    char buf[16];
    int is_human = human_of_ent(e) >= 0;
    int marked = is_human && humans_present > 1;   // only mark when it means smth
    if (e == g_local_ent) {
      ui_rrect_c(px + 6, y - 2, pw - 12, 11, 0,
                 C4(C_BLAZE, 0.16f), C4(C_BLAZE, 0.10f));
      ui_rect(px + 6, y - 2, 2, 11, C_BLAZE, 0.95f);
    } else if ((i & 1) == 0) {
      ui_rect(px + 6, y - 2, pw - 12, 11, C_BONE, 0.03f);
    }
    // The side, in its kit colour: a bar and the name, ALPHA red, BRAVO blue.
    int st = teams_on() ? team_of(e) : -1;
    if (st >= 0 && e != g_local_ent) ui_rect(px + 6, y - 2, 2, 11, TEAM_RGB(st), 0.9f);
    snprintf(buf, sizeof buf, "%d", i + 1);
    ui_text_ex(buf, px + 26 - ui_text_width(buf, 0.9f), y + 0.5f, 0.9f, TXT_REG, 0,
               e == g_local_ent ? C4(C_BLAZE, 1) : C4(C_HAZE, 0.8f));
    float nx = xn + (marked ? 8.0f : 0.0f);
    if (st >= 0) {
      if (marked) ui_text("\x01", xn, y, 1, TEAM_RGB(st), 1);
      if (e == g_local_ent) ui_textb(ent_name(e), nx, y, 1, TEAM_RGB(st), 1);
      else                  ui_text(ent_name(e), nx, y, 1, TEAM_RGB(st), 1);
    } else if (e == g_local_ent) {
      if (marked) ui_text("\x01", xn, y, 1, C_BLAZE, 1);
      ui_textb(ent_name(e), nx, y, 1, C_BLAZE, 1);
    } else if (marked) {
      ui_text("\x01", xn, y, 1, C_SIGNAL, 1);
      ui_text(ent_name(e), nx, y, 1, C_SIGNAL, 1);
    } else {
      ui_text(ent_name(e), xn, y, 1, C_BONE, 1);
    }
    snprintf(buf, sizeof buf, "%d", g_match.kills[e]);
    ui_textb(buf, xk - ui_text_width(buf, 1), y, 1, C_BONE, 1);
    snprintf(buf, sizeof buf, "%d", g_match.deaths[e]);
    ui_text(buf, xd - ui_text_width(buf, 1), y, 1, C_HAZE, 1);
    snprintf(buf, sizeof buf, "%.1f", (double)g_match.kills[e] /
             (double)(g_match.deaths[e] ? g_match.deaths[e] : 1));
    ui_text(buf, xkd - ui_text_width(buf, 1), y, 1, C_HAZE, 1);
    // Capped: one grenade can wound three people, so splash-heavy play would
    // otherwise read past 100%.
    snprintf(buf, sizeof buf, "%d%%", g_match.shots[e] ?
             (g_match.hits[e] >= g_match.shots[e] ? 100
              : g_match.hits[e] * 100 / g_match.shots[e]) : 0);
    ui_text(buf, xa - ui_text_width(buf, 1), y, 1, C_HAZE, 1);
    if (show_ping) {
      // Humans carry a measured conn RTT; bots are simulated in-process and a
      // number there would be an invented statistic — they get a dash.
      if (is_human) snprintf(buf, sizeof buf, "%d", g_net_ping[e]);
      else          snprintf(buf, sizeof buf, "-");
      ui_text(buf, xp - ui_text_width(buf, 1), y, 1, C_HAZE, 1);
    }
    if (mcol) {
      if (gm == GM_GUN) snprintf(buf, sizeof buf, "%d/%d", gg_level(e) + 1, GG_LEVELS);
      else              snprintf(buf, sizeof buf, "%d", g_match.mscore[e]);
      ui_textb(buf, xm - ui_text_width(buf, 1), y, 1, C_BONE, 1);
    }
    y += 11;
  }
  if (hidden) {
    char more[24];
    snprintf(more, sizeof more, "+%d MORE", hidden);
    ui_text(more, px + 12, y + 1, 0.9f, C_HAZE, 1);
  }
  g_sb_bottom = py + ph;
  g_sb_w = pw;
}

// ENEMY MARKERS (Settings > HUD, default off). The red helmet is the game's
// only hostile cue, and it is a COLOUR: under a red-green deficiency, against
// the warm sand and brick maps, or past ~25 m where the helmet is a few pixels,
// it stops carrying. This is a SHAPE — a downward chevron over the head, an
// outline the eye reads without hue — and it only ever marks what the player
// can already see: a clear line from the eye to the head, inside the frustum,
// alive. It fades in from 12 m (closer, the figure itself is the cue) and is
// off in the rescue, which is a game about not being seen.
static int rescue_proj(v3 w, float *sx, float *sy);   // world -> HUD units
static void perf_draw(void);   // the PERF overlay, defined with app_frame
static void hud_enemy_marks(const player_t *p) {
  if (!g_cfg.enemy_mark || rescue_view() || g_match.over || !p->alive) return;
  v3 eye = {{p->pos.x, p->pos.y + p->eye, p->pos.z}};
  ENT_FOREACH(e) {
    if (e == g_local_ent || !ent_alive(e)) continue;
    // A teammate is not an enemy: he already carries the team chevron, and
    // a second, hostile one over the same head said both things at once.
    if (team_mates(g_local_ent, e)) continue;
    v3 head = ent_aimpoint(e);
    head.y += 0.62f;
    v3 d = v3_sub(head, eye);
    float dist = v3_len(d);
    if (dist < 12.0f || dist > GUN_RANGE) continue;
    if (!los_clear(eye, head)) continue;
    float sx, sy;
    if (!rescue_proj(head, &sx, &sy)) continue;
    float a = f_clamp((dist - 12.0f) / 6.0f, 0.0f, 1.0f) * 0.92f;
    float w = 3.4f, hgt = 2.6f, y0 = sy - 6.0f;
    // Outline first, then the core: readable on sky and on shadow alike.
    ui_capsule(sx - w, y0 - hgt, sx, y0, 1.7f, 0.02f, 0.03f, 0.04f, 0.55f * a);
    ui_capsule(sx, y0, sx + w, y0 - hgt, 1.7f, 0.02f, 0.03f, 0.04f, 0.55f * a);
    // In TEAM DEATHMATCH the core is the ENEMY SIDE's colour: an ALPHA
    // player's own chevrons are red, so a red enemy mark would read as a mate.
    int et = teams_on() ? team_of(e) : -1;
    if (et >= 0) {
      ui_capsule(sx - w, y0 - hgt, sx, y0, 0.85f, TEAM_RGB(et), a);
      ui_capsule(sx, y0, sx + w, y0 - hgt, 0.85f, TEAM_RGB(et), a);
    } else {
      ui_capsule(sx - w, y0 - hgt, sx, y0, 0.85f, C_THREAT, a);
      ui_capsule(sx, y0, sx + w, y0 - hgt, 0.85f, C_THREAT, a);
    }
  }
}

// FIRST-MATCH CONTROL HINTS. The genre's core verbs are discoverable, but this
// game's distinctive ones — the aim button as a knife guard, lean, the slide —
// are not, and the CONTROLS page is a menu nobody opens before playing. One
// prompt at a time, bottom centre, naming the player's OWN binding (pad labels
// when a pad is connected). Each clears when the player does it, or after
// HINT_SHOW_TICKS so it never nags; when the list is done `hints` goes to 0 in
// the config and stays there until Settings > CONTROL HINTS turns it back on.
#define HINT_DELAY_TICKS 240     // 2 s into a life before the first prompt
#define HINT_SHOW_TICKS  960     // 8 s per prompt at most
typedef struct { int act, act2; const char *text; } hint_def_t;
static const hint_def_t HINTS[] = {
  {IN_ADS,    -1,        "AIM DOWN SIGHTS"},
  {IN_LEAN_L, IN_LEAN_R, "LEAN AROUND COVER"},
  {IN_CROUCH, -1,        "WHILE RUNNING TO SLIDE"},
  {IN_SWAP,   -1,        "SWAP WEAPON  /  1-5 PICK ONE"},
  {IN_SCORE,  -1,        "SCOREBOARD"},
};
#define HINT_COUNT ((int)(sizeof HINTS / sizeof HINTS[0]))

static const char *hint_key(int act) {
  if (g_pad.connected) return pad_disp(g_cfg.padbind[act]);
  const char *t = g_cfg.bind[act];
  return !strcmp(t, "mouse1") ? "LMB" : !strcmp(t, "mouse2") ? "RMB"
       : !strcmp(t, "mouse3") ? "MMB" : !strcmp(t, "lshift") ? "SHIFT"
       : !strcmp(t, "lctrl")  ? "CTRL" : t;
}

static void hud_hints(const player_t *p, float ccx) {
  // Timed in SIM ticks, not frames: the same 8 s at 60 or 120 fps, frozen
  // while the menu pauses the match. t0 is the tick the current prompt
  // started counting from (a delay ahead of it is a t0 in the future).
  static int idx, was_on;
  static long t0;
  if (!g_cfg.hints) { was_on = 0; return; }
  if (!was_on || g_tick < t0 - HINT_DELAY_TICKS) {       // fresh, or a new match's clock
    idx = 0; t0 = g_tick + HINT_DELAY_TICKS; was_on = 1;
  }
  if (g_home || rescue_view() || g_match.over || !p->alive || g_menu_open) return;
  if (idx >= HINT_COUNT) { g_cfg.hints = 0; return; }
  const hint_def_t *h = &HINTS[idx];
  int done = p->prev_held[h->act] || (h->act2 >= 0 && p->prev_held[h->act2]);
  if (h->act == IN_CROUCH) done = p->sliding;            // the slide, not a crouch
  if (h->act == IN_SWAP)
    done = done || p->prev_held[IN_SLOT1] || p->prev_held[IN_SLOT2] ||
           p->prev_held[IN_SLOT3] || p->prev_held[IN_SLOT4] || p->prev_held[IN_SLOT5];
  long shown_t = g_tick - t0;
  if (shown_t > 0 && (done || shown_t > HINT_SHOW_TICKS)) {
    idx++;
    t0 = g_tick + HINT_DELAY_TICKS / 4;                  // a breath between prompts
    return;
  }
  if (shown_t <= 0) return;
  float a = f_clamp((float)shown_t / 30.0f, 0.0f, 1.0f);
  char key[24];
  if (h->act2 >= 0) snprintf(key, sizeof key, "%s / %s", hint_key(h->act), hint_key(h->act2));
  else              snprintf(key, sizeof key, "%s", hint_key(h->act));
  for (char *c = key; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
  float kw = ui_text_width_tr(key, 1.0f, 1.4f), tw = ui_text_width_tr(h->text, 0.95f, 1.4f);
  float w = kw + 14 + 10 + tw, x = ccx - w * 0.5f, y = g_ui_vh - 74.0f;
  ui_rect(x - 10, y - 5, w + 20, 21, 0.0f, 0.0f, 0.0f, 0.45f * a);
  ui_rrect_line(x, y - 2, kw + 14, 15, 0, 0.8f, C_BLAZE, 0.9f * a);
  ui_text_ex(key, x + 7, y + 2, 1.0f, TXT_BOLD, 1.4f, C4(C_BLAZE, a));
  ui_text_ex(h->text, x + kw + 24, y + 2.5f, 0.95f, TXT_REG, 1.4f, C4(C_BONE, 0.95f * a));
}

// THE END SCREEN SAYS HOW *YOU* DID. The board ranks everyone; this strip under
// it is the local player's own match (headshots and the best run without
// dying, which no column carries) and the career it just added to — the one
// thing in the game that carries over from one session to the next.
static void hud_match_summary(void) {
  static int was_over = -1;
  int over = g_match.over && !rescue_view();
  if (was_over == 1 && !g_match.over) g_ms = (typeof(g_ms)){0};   // live again
  was_over = g_match.over;
  if (!over) return;
  int me = g_local_ent;
  float w = g_sb_w > 300.0f ? g_sb_w : 300.0f;
  float x = g_ui_vw * 0.5f - w * 0.5f;
  float y = g_sb_bottom + 6.0f;
  if (y > g_ui_vh - 44.0f) y = g_ui_vh - 44.0f;
  ui_panel(x, y, w, 38, 0, 1);
  int sh = g_match.shots[me], hi = g_match.hits[me];
  char l1[96], l2[96];
  // The board's own accuracy rule (integer, floored, capped at 100), so the
  // two numbers on one screen can never disagree by a rounding.
  snprintf(l1, sizeof l1, "YOUR MATCH   %d K   %d D   %d HS   %d%% ACC   BEST RUN %d",
           g_match.kills[me], g_match.deaths[me], g_ms.hs,
           sh ? (hi >= sh ? 100 : hi * 100 / sh) : 0, g_ms.best);
  float kd = g_cfg.st_deaths ? (float)g_cfg.st_kills / (float)g_cfg.st_deaths
                             : (float)g_cfg.st_kills;
  snprintf(l2, sizeof l2, "CAREER  %d PLAYED  %d WON  %.2f K/D  %d HS  BEST RUN %d",
           g_cfg.st_matches, g_cfg.st_wins, (double)kd, g_cfg.st_hs, g_cfg.st_best);
  // Never past the panel's edge: a long name list or big career numbers
  // shrink the line instead of running it out of its box.
  float l1s = 1.0f, l1w = ui_text_width(l1, l1s);
  if (l1w > w - 24.0f) l1s *= (w - 24.0f) / l1w;
  float l2s = 0.9f, l2w = ui_text_width(l2, l2s);
  if (l2w > w - 24.0f) l2s *= (w - 24.0f) / l2w;
  ui_text(l1, x + 12, y + 8, l1s, C_BONE, 1);
  ui_text(l2, x + 12, y + 23, l2s, C_HAZE, 1);
}

// MSAA render target — rebuilt lazily whenever the setting or framebuffer
// size changes, so the menu toggle takes effect on the next frame without a
// platform hook. If the driver refuses the sample count, MSAA falls back off.
static void msaa_update(void) {
  if (R.msaa_cfg == g_cfg.msaa && R.msaa_w == R.width && R.msaa_h == R.height) return;
  if (R.msaa_fbo) {
    glDeleteFramebuffers(1, &R.msaa_fbo);
    glDeleteRenderbuffers(2, R.msaa_rb);
    R.msaa_fbo = 0;
  }
  R.msaa_cfg = g_cfg.msaa;
  R.msaa_w = R.width;
  R.msaa_h = R.height;
  if (!g_cfg.msaa) return;
  GLint max_samples = 1;
  glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
  GLsizei n = g_cfg.msaa < max_samples ? g_cfg.msaa : max_samples;
  glGenFramebuffers(1, &R.msaa_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, R.msaa_fbo);
  glGenRenderbuffers(2, R.msaa_rb);
  glBindRenderbuffer(GL_RENDERBUFFER, R.msaa_rb[0]);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, n, GL_RGBA16F, R.width, R.height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, R.msaa_rb[0]);
  glBindRenderbuffer(GL_RENDERBUFFER, R.msaa_rb[1]);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, n, GL_DEPTH_COMPONENT24, R.width, R.height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, R.msaa_rb[1]);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "msaa: %dx framebuffer incomplete - disabled\n", (int)n);
    glDeleteFramebuffers(1, &R.msaa_fbo);
    glDeleteRenderbuffers(2, R.msaa_rb);
    R.msaa_fbo = 0;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, R.present_fbo);
}

// Quarter-res ping-pong chain for the frosted menu backdrop — lazily rebuilt
// on size change, exactly like the MSAA target. blur_fbo[0] == 0 means "no
// blur": ui_menu falls back to its heavier scrim, never to a broken frame.
static void blur_update(void) {
  int bw = R.width / 4, bh = R.height / 4;
  if (bw < 8) bw = 8;
  if (bh < 8) bh = 8;
  if (R.blur_fbo[0] && R.blur_w == bw && R.blur_h == bh) return;
  if (R.blur_fbo[0]) {
    glDeleteFramebuffers(2, R.blur_fbo);
    glDeleteTextures(2, R.blur_tex);
    R.blur_fbo[0] = R.blur_fbo[1] = 0;
  }
  R.blur_w = bw;
  R.blur_h = bh;
  glGenTextures(2, R.blur_tex);
  glGenFramebuffers(2, R.blur_fbo);
  for (int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, R.blur_tex[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, bw, bh, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, R.blur_fbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           R.blur_tex[i], 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glDeleteFramebuffers(2, R.blur_fbo);
      glDeleteTextures(2, R.blur_tex);
      R.blur_fbo[0] = R.blur_fbo[1] = 0;
      break;
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, R.present_fbo);
  glBindTexture(GL_TEXTURE_2D, R.font_tex);
}

// Resolve the multisampled scene into the present buffer. ONE copy: the
// backdrop needs it mid-frame (so the blur has something to read) and the
// frame end needs it again, and a blit written twice is a rectangle that
// eventually differs on one of the two paths.
static void msaa_resolve(void) {
  if (!R.msaa_fbo) return;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, R.msaa_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R.scene_fbo);
  glBlitFramebuffer(0, 0, R.width, R.height, 0, 0, R.width, R.height,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

// Frosted-glass backdrop for the PAUSING menu (a networked session never
// gets one): flush the HUD batch, resolve the scene into the present
// buffer, pull it down to quarter res, run four growing dual-Kawase taps,
// and paint the result back as one full-screen image quad — then hand GL
// state back to the UI batch. Fails soft to the scrim.
static void ui_menu_backdrop(void) {
  g_ui_backdrop = 0;
  blur_update();
  if (!R.blur_fbo[0] || !R.blur_fbo[1]) return;
  ui_flush();
  // The HUD draws into the present buffer AFTER the post chain, so that buffer
  // already holds the finished, display-referred frame.
  glBindFramebuffer(GL_READ_FRAMEBUFFER, R.present_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R.blur_fbo[0]);
  glBlitFramebuffer(0, 0, R.width, R.height, 0, 0, R.blur_w, R.blur_h,
                    GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glDisable(GL_BLEND);
  glUseProgram(R.prog_blur);
  glUniform1i(R.u_blur_tex, 0);
  glBindVertexArray(R.vao_sky);   // attribute-less fullscreen triangle
  glViewport(0, 0, R.blur_w, R.blur_h);
  int src = 0;
  for (int i = 0; i < 4; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, R.blur_fbo[1 - src]);
    glBindTexture(GL_TEXTURE_2D, R.blur_tex[src]);
    glUniform2f(R.u_blur_px, (float)(i + 1) / (float)R.blur_w,
                (float)(i + 1) / (float)R.blur_h);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    src = 1 - src;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, R.present_fbo);
  glViewport(0, 0, R.width, R.height);
  glEnable(GL_BLEND);
  glUseProgram(R.prog_hud);
  glBindVertexArray(R.vao_hud);
  glUniform1i(R.u_img, 1);
  glBindTexture(GL_TEXTURE_2D, R.blur_tex[src]);
  ui_begin();
  // The scene's v axis runs bottom-up, the HUD's top-down: v is flipped.
  // Tinted slightly cool and dim — glass takes light, the scope learned that.
  ui_quad(0, 0, g_ui_vw, g_ui_vh, 0, 1, 1, 0, (float[4]){0.82f, 0.85f, 0.90f, 1.0f});
  ui_flush();
  glUniform1i(R.u_img, 0);
  glBindTexture(GL_TEXTURE_2D, R.font_tex);
  ui_begin();
  g_ui_backdrop = 1;
}

