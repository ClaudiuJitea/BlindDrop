// code/core/config_io.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
typedef struct {
  float sense;               // mouse sensitivity, degrees per count at 0.022 base
  // ADS SENS for the MOUSE, and deliberately not the pad's construction. The
  // pad's ADS scale is anchored on the ZOOM RATIO (a thumb pushes a rate, so a
  // 2x zoom without a 2x slowdown doubles the felt turn speed and the stick
  // becomes unusable at range); a mouse pushes an ANGLE, and a mouse player's
  // muscle memory is a distance on the mat — auto-scaling it with the zoom is
  // the thing they turn off first in every shooter that ships it. So this is a
  // PLAIN multiplier, blended by the same ADS ramp, and 1.00 means the hip
  // sensitivity carries straight through the raise. At the default the factor
  // is exactly 1.0 for every value of ads_s, which is why adding it moves no
  // artefact in the battery.
  float ads_sense;           // mouse ADS sensitivity multiplier, 1 = unchanged
  float fov;                 // horizontal FOV in degrees
  float vm_fov;              // weapon (viewmodel) vertical FOV, see vm_fov_deg
  int   vm_pos;              // hip weapon position: 0 standard, 1 low, 2 lower
  int   vsync;
  int   fps_cap;             // frame limiter, 0 = off
  int   show_fps;
  int   msaa;                // MSAA samples: 0 (off), 2, 4 or 8
  int   post_fx;             // bloom, colour grade and vignette
  int   ballistics;          // offline rounds fly (drag, drop, ricochet)
  // Read by audio_mix on the audio thread while the GAME tab's slider writes it
  // on the main thread — the one object crossing that boundary that was not yet
  // atomic (g_trig_w/g_trig_r and g_audio_run are). Making the member itself
  // atomic beats mirroring it into a second global: a mirror would be a
  // second copy of VOL_DEF.def with three publish sites to keep in sync, which
  // is the defect class this file already treats as the enemy. Cost is nil — an
  // x86 seq_cst load is a plain mov, and the store happens at most once a frame
  // while the menu is open.
  _Atomic float vol;         // master volume 0..1
  int   sp_bots;             // deathmatch opponents, range in SP_BOTS_DEF
  int   sp_diff;             // bot difficulty: 0 easy, 1 normal, 2 hard
  int   sp_frag;             // frag limit, range in SP_FRAG_DEF
  int   sp_mode;             // game_mode_t; applies at the next (re)start
  int   sp_rounds;           // LAST MAN STANDING rounds to win, SP_ROUNDS_DEF
  int   sp_map;              // a MAP_DEFS index, or MAP_ROTATE; next (re)start
  int   sp_team_size;        // TEAM DEATHMATCH: places per side, 1..TEAM_SIZE_MAX
  int   sp_team_fill;        // ...bots fill the empty places (0 = humans only)
  int   sp_team;             // ...the side this player picks offline (0 ALPHA)
  int   sp_team_ff;          // ...friendly fire: 1 = teammates can hurt each other
  // sp_team_fill and sp_team_ff are RUNTIME, not settings: offline they stay
  // at fill on / friendly fire off (a solo team game IS its bots), and a
  // server sets them from its --team-fill / --friendly-fire. The player's
  // choices for a game they host live in the host_* fields, on the
  // MULTIPLAYER page's HOST SETTINGS, apart from the SINGLEPLAYER rules.
  int   host_mode, host_map, host_bots;
  int   host_team_size, host_team_fill, host_team_ff;
  int   sp_squad, host_squad;  // HOSTAGE RESCUE: squad size 1..RESC_SQUAD_MAX
  char  name[12];            // player name; glyph policy lives in name_glyph
  int   third_person;        // camera: 0 = first person, 1 = shoulder cam
  int   knife_model;         // ARMORY: KNIFE_MODEL_NAMES index
  int   rifle_var, pistol_var; // ARMORY LOADOUT: *_VAR_NAMES index, 0 = shipped gun
  int   wpn_theme;           // ARMORY: WPN_THEME_NAMES index, 0 = factory
  int   gear_theme;          // ARMORY: GEAR_THEME_NAMES index, 0 = slate
  int   head_gear, vest_gear, pack_gear;   // ARMORY: *_GEAR_NAMES, 0 = operator
  // CAREER: lifetime totals for the local player, shown on the end screen.
  // Counted on the presentation side (vfx_on_event), so offline and online
  // matches both add to them; the boot screen's showcase match never does.
  int   st_matches, st_wins, st_kills, st_deaths, st_hs, st_best;
  int   hints;               // first-match control prompts still to show (1 = yes)
  // HUD (Settings > HUD): crosshair shape / colour / size and HUD opacity.
  // Index 0 / 1.0 of each is the shipped HUD exactly.
  int   ch_style, ch_color;
  float ch_size, hud_alpha;
  float hud_scale;           // in-match HUD size, 0.75-1.5, 1 = shipped
  int   enemy_mark;          // chevron over SEEN enemies past 12 m (0 off, 1 on)
  int   perf;                // PERF overlay: frame/CPU/GPU milliseconds
  int   devmode;             // show the DEV tab; hard-gated by DEV_MENU, def 0
  float mv[MV_COUNT];        // movement tuning, see MV_DEF
  float wp[WPN_COUNT][WP_COUNT];  // per-weapon tuning, see WP_DEF/WPN_DEF
  float pad[PAD_COUNT];      // gamepad feel, see PAD_DEF
  int   pad_curve;           // aim response curve, index into PAD_CURVE_OPTS
  int   pad_inv_y;           // inverted look on the right stick
  int   pad_assist;          // aim assist (FRICTION ONLY), pad only; tuning = PAD_DEF PAD_AS_*
  char  bind[IN_COUNT][12];  // key tokens, resolved per-platform
  char  padbind[IN_COUNT][12];  // gamepad tokens, "none" = unbound
  // Master directory address: two SEPARATE keys — the port stays
  // an ordinary range-checked numeric key, only the host is a string; host:port
  // in one field would be a parser this file has nowhere else. 64 = exactly the
  // parser's val width, so snprintf(dst, sizeof dst, "%s", val) provably never
  // truncates and the bind[12] warning class cannot return.
  char  mp_host[64];
  int   mp_port;
} config_t;

static config_t g_cfg;
static char     g_cfg_path[512];

// The tuning tables the SIM reads — movement and per-weapon
// feel. Offline both point at the player's own config. An online CLIENT
// repoints them at the SERVER's tables (carried in ACCEPT): predicting with
// local preferences against a server that simulates different numbers is a
// permanent structural misprediction — measured on loopback, an mv_run of 12
// against a server's 6.5 is a ~350 mm STANDING rubber-band no reconciliation
// can drain. The menu, the config file and the settings UI keep reading
// g_cfg — a joined match never touches the player's own preferences.
static const float *g_sim_mv = g_cfg.mv;
static const float (*g_sim_wp)[WP_COUNT] = g_cfg.wp;

// Q/E are the lean keys — the position every tactical shooter puts them in —
// which is why the weapon TOGGLE moved off Q: the two weapons have direct slot
// keys (1 = AR, 2 = SNIPER) now, so the toggle is the redundant one of the
// three. An existing config with `bind_swap q` collides with the lean default
// and the migration pass below moves it to x on its own.
// Composed config key for weapon w, param i: "ar_rpm", "sr_kick_up", ...
static void wp_key(char *out, size_t n, int w, int i) {
  snprintf(out, n, "%s_%s", WPN_DEF[w].prefix, WP_DEF[i].key);
}

// Valid MSAA sample counts are 0/2/4/8; anything else snaps to the next lower.
static int msaa_snap(long v) { return v >= 8 ? 8 : v >= 4 ? 4 : v >= 2 ? 2 : 0; }

// The player-name glyph policy, in ONE place: only glyphs the 5x7 font can
// draw — A-Z (lowercase upcased) and 0-9 — pass; everything else returns 0.
// Both consumers (the config parser and the menu's name editor) call this, so
// a glyph accepted in the menu can never be silently stripped on the next load.
static char name_glyph(char c) {
  if (c >= 'a' && c <= 'z') c -= 32;
  return ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? c : 0;
}

static config_t config_defaults(void) {
  // Display/feel scalars, re-tuned live 2026-08-19 (DEV session, the values a
  // full play pass landed on):
  //   sense 1.00 — the old 0.35 is a THIRD of a normal shooter's mouse feel; it
  //     made a 180 a hand-over-hand drag and every new player's first act was to
  //     go find the slider.
  //   fov 100 — a 90 base on a handheld held at arm's length is a tunnel, and
  //     100 is where the genre's PC default sits.
  //   vsync 0 + fps_cap 120 — an explicit frame ceiling instead of "whatever the
  //     panel does". The device is a 120 Hz handheld, so this IS its refresh, and
  //     the cap does the pacing without vsync's latency. On a 60 Hz desktop panel
  //     the cap is simply never reached and the compositor still paces it.
  //   msaa 8 — the whole art contract is flat-shaded facets and long straight
  //     silhouette edges, which is the one content that MSAA is cheap on and
  //     that crawls visibly without it. llvmpipe never sees this (the harness
  //     renders into an FBO), and a GPU that cannot afford 8 has the slider.
  config_t c = {.post_fx = 1, .hints = 1, .vm_fov = 58.0f, .ch_size = 1.0f, .hud_alpha = 1.0f, .hud_scale = 1.0f, .sense = 1.0f, .ads_sense = 1.0f, .fov = 100.0f, .vsync = 0, .fps_cap = 120, .show_fps = 1, .msaa = 8,
                // sp_diff 2 = HARD. The bot count came down to 1, so difficulty
                // is what carries the challenge now — see SP_BOTS_DEF.
                .vol = VOL_DEF.def, .sp_bots = (int)SP_BOTS_DEF.def, .sp_diff = 2,
                .sp_frag = (int)SP_FRAG_DEF.def, .name = "PLAYER",
                .sp_mode = GM_DM, .sp_rounds = (int)SP_ROUNDS_DEF.def, .sp_map = 0,
                .sp_team_size = 5, .sp_team_fill = 1, .sp_team = 0, .sp_team_ff = 0,
                .host_mode = GM_TDM, .host_map = 0, .host_bots = 3,
                .host_team_size = 5, .host_team_fill = 1, .host_team_ff = 0,
                .sp_squad = 3, .host_squad = 3,
                // LINEAR (0), changed from STANDARD 2026-08-19. The power curve
                // was chosen to widen the precision band most thumbs live in —
                // but it does that by making the stick's OUTPUT non-linear in
                // its deflection, and the friction assist below is a second
                // non-linearity acting on the same input. Stacked, small
                // corrections near a target became unpredictable in a way
                // neither knob could be tuned out of. With the assist bubble
                // narrowed to 2.5 deg the curve's job is done by the assist,
                // and LINEAR is what makes the stick answerable. STANDARD is
                // one menu click away. Aim assist defaults ON like every
                // console shooter's — stick aim against a strafing target
                // without it is a handicap, not a difficulty choice.
                .pad_curve = 0, .pad_assist = 1,
                // NO SERVER IS COMPILED IN: the game contacts nothing on its own.
                // Multiplayer dials only what the player configures here or
                // passes with --connect.
                .mp_host = "", .mp_port = NET_PORT_DEF};
  for (int i = 0; i < MV_COUNT; i++) c.mv[i] = MV_DEF[i].def;
  for (int w = 0; w < WPN_COUNT; w++)
    for (int i = 0; i < WP_COUNT; i++) c.wp[w][i] = WP_DEFAULTS[w][i];
  for (int i = 0; i < PAD_COUNT; i++) c.pad[i] = PAD_DEF[i].def;
  for (int i = 0; i < IN_COUNT; i++) {
    snprintf(c.bind[i], sizeof c.bind[i], "%s", ACTION_DEF[i].dflt);
    snprintf(c.padbind[i], sizeof c.padbind[i], "%s", ACTION_DEF[i].pdflt);
  }
  return c;
}

// Composed pad-bind config key for action i: "padbind_forward", ...
static void padbind_key(char *out, size_t n, int i) {
  snprintf(out, n, "pad%s", ACTION_DEF[i].key);
}

// ONE emitter for the whole config, because there used to be two: config_write
// and the harness's --print-config printed the same keys in the same format
// from two hand-maintained lists, and they had already drifted — the dump was
// silently missing sp_bots, sp_difficulty, sp_fraglimit and player_name, i.e.
// the entire MATCH tab, on the command whose only job is to show the effective
// config. A new key now reaches both by construction.
static void config_emit(FILE *f, const config_t *c) {
  fprintf(f,
    "# BlindDrop config — auto-generated defaults, edit freely.\n"
    "# key tokens: a-z 0-9 space tab lshift lctrl | mouse1=left mouse2=right mouse3=middle\n"
    "#             mouse4/mouse5 = thumb buttons (back/forward)\n"
    "sense %.3f\n"
    "ads_sense %.3f\n"
    "fov %.0f\n"
    "weapon_fov %.0f\n"
    "weapon_pos %d\n"
    "vsync %d\n"
    "fps_cap %d\n"
    "show_fps %d\n"
    "msaa %d\n"
    "third_person %d\n"
    "devmode %d\n",
    (double)c->sense, (double)c->ads_sense, (double)c->fov, (double)c->vm_fov, c->vm_pos, c->vsync, c->fps_cap,
    c->show_fps, c->msaa,
    c->third_person, c->devmode);
  fprintf(f, "%s %.2f\n", VOL_DEF.key, (double)c->vol);
  fprintf(f, "sp_bots %d\nsp_difficulty %d\nsp_fraglimit %d\nplayer_name %s\n",
          c->sp_bots, c->sp_diff, c->sp_frag, c->name);
  fprintf(f, "sp_mode %s\n%s %d\n", GM_KEYS[c->sp_mode >= 0 && c->sp_mode < GM_COUNT
                                             ? c->sp_mode : GM_DM],
          SP_ROUNDS_DEF.key, c->sp_rounds);
  fprintf(f, "sp_map %s\n", map_pick_key(c->sp_map));
  fprintf(f, "sp_team_size %d\nsp_team %d\n", c->sp_team_size, c->sp_team);
  fprintf(f, "host_mode %s\nhost_map %s\nhost_bots %d\n",
          GM_KEYS[c->host_mode >= 0 && c->host_mode < GM_COUNT ? c->host_mode : GM_DM],
          map_pick_key(c->host_map), c->host_bots);
  fprintf(f, "host_team_size %d\nhost_team_fill %d\nhost_friendly_fire %d\n",
          c->host_team_size, c->host_team_fill, c->host_team_ff);
  fprintf(f, "sp_squad %d\nhost_squad %d\n", c->sp_squad, c->host_squad);
  fprintf(f, "post_fx %d\nmp_host %s\nmp_port %d\n", c->post_fx, c->mp_host, c->mp_port);
  fprintf(f, "knife_model %d\nweapon_theme %d\ngear_theme %d\n",
          c->knife_model, c->wpn_theme, c->gear_theme);
  fprintf(f, "rifle_model %d\npistol_model %d\n", c->rifle_var, c->pistol_var);
  fprintf(f, "head_gear %d\nvest_gear %d\npack_gear %d\n",
          c->head_gear, c->vest_gear, c->pack_gear);
  fprintf(f, "hints %d\n", c->hints);
  fprintf(f, "crosshair_style %d\ncrosshair_color %d\ncrosshair_size %.2f\nhud_opacity %.2f\nhud_scale %.2f\nenemy_markers %d\nperf_overlay %d\n",
          c->ch_style, c->ch_color, (double)c->ch_size, (double)c->hud_alpha, (double)c->hud_scale,
          c->enemy_mark, c->perf);
  fprintf(f, "stat_matches %d\nstat_wins %d\nstat_kills %d\nstat_deaths %d\n"
             "stat_headshots %d\nstat_best_streak %d\n",
          c->st_matches, c->st_wins, c->st_kills, c->st_deaths, c->st_hs, c->st_best);
  fprintf(f, "ballistics %d\n", c->ballistics);
  for (int i = 0; i < MV_COUNT; i++)
    fprintf(f, "%s %.2f\n", MV_DEF[i].key, (double)c->mv[i]);
  for (int w = 0; w < WPN_COUNT; w++)
    for (int i = 0; i < WP_COUNT; i++) {
      char key[32];
      wp_key(key, sizeof key, w, i);
      fprintf(f, "%s %.2f\n", key, (double)c->wp[w][i]);
    }
  for (int i = 0; i < PAD_COUNT; i++)
    fprintf(f, "%s %.2f\n", PAD_DEF[i].key, (double)c->pad[i]);
  fprintf(f, "pad_curve %d\npad_inv_y %d\npad_assist %d\n",
          c->pad_curve, c->pad_inv_y, c->pad_assist);
  for (int i = 0; i < IN_COUNT; i++)
    fprintf(f, "%s %s\n", ACTION_DEF[i].key, c->bind[i]);
  for (int i = 0; i < IN_COUNT; i++) {
    char pk[24];
    padbind_key(pk, sizeof pk, i);
    fprintf(f, "%s %s\n", pk, c->padbind[i]);
  }
}

// Write-to-temp-then-rename: fopen(path, "w") truncates immediately, and this
// runs at exactly the moments a handheld gets hard-powered-off (menu close,
// quit, window close) — a crash between truncate and fclose used to cost every
// binding and slider, silently, because the loader tolerates a partial file.
// rename() is atomic on POSIX; MoveFileExA(REPLACE_EXISTING) is the Windows
// equivalent. On any write error the old file stays untouched.
static void config_write(const char *path, const config_t *c) {
  char tmp[512];
  // Checked, not fire-and-forget: a truncated temp path would write some OTHER
  // file and then rename it over nothing. Also what keeps -Wformat-truncation
  // quiet in the non-LTO -O2 warning gate (CI runs that gate at -Werror).
  int n = snprintf(tmp, sizeof tmp, "%s.tmp", path);
  if (n < 0 || (size_t)n >= sizeof tmp) {
    fprintf(stderr, "config: path too long, not writing: %s\n", path);
    return;
  }
  FILE *f = fopen(tmp, "w");
  if (!f) { fprintf(stderr, "config: cannot write %s\n", tmp); return; }
  config_emit(f, c);
  int bad = ferror(f);
  if (fclose(f) != 0) bad = 1;
  if (bad) {
    fprintf(stderr, "config: write failed, keeping old %s\n", path);
    remove(tmp);
    return;
  }
#ifdef _WIN32
  if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING))
#else
  if (rename(tmp, path) != 0)
#endif
    fprintf(stderr, "config: cannot replace %s\n", path);
}

// Tolerant float parse: accepts a decimal comma (hand-edited configs on
// German Windows), keeps the previous value on garbage instead of clamping
// nonsense to a limit.
static float parse_float(const char *s, float prev, float lo, float hi) {
  // buf is DELIBERATELY not val's width (64): 11 chars is the historical
  // numeric-token budget, and keeping it pins every float key's parse
  // byte-identical across the val widening — a hand-written value longer than
  // 11 chars parsed differently would be a silent behavior change.
  char buf[12];
  snprintf(buf, sizeof buf, "%s", s);
  for (char *q = buf; *q; q++)
    if (*q == ',') *q = '.';
  char *end;
  float v = strtof(buf, &end);
  if (end == buf) { fprintf(stderr, "config: bad number '%s'\n", s); return prev; }
  // Reject NaN bit-wise: strtof accepts the literal "nan", f_clamp passes NaN
  // straight through (both comparisons false), and downstream (int) casts of it
  // are UB. The test must NOT be isnan()/v != v — -ffast-math's finite-math
  // assumption lets the compiler fold those to false on the shipped build.
  // +/-inf (exactly 0x7F800000) deliberately stays: it clamps to the bound.
  if (f_is_nan(v)) {
    fprintf(stderr, "config: bad number '%s'\n", s);
    return prev;
  }
  return f_clamp(v, lo, hi);
}

// Integer twin of parse_float (no decimal comma: an int never has one). Three
// copies of the same strtol-and-complain block sat in config_load_or_create with
// the error STRING repeated in each, next to a function that already modelled it.
// Keeps the previous value on garbage, exactly like the float path, which is what
// the old `else if` chain did by skipping the assignment.
static long parse_long(const char *s, long prev) {
  char *end;
  long v = strtol(s, &end, 10);
  if (end == s) { fprintf(stderr, "config: bad number '%s'\n", s); return prev; }
  return v;
}
// ...and the clamped form, mirroring parse_float's (prev, lo, hi) signature so
// the open-coded `(int)(v < lo ? lo : v > hi ? hi : v)` ladders go away with it.
static int parse_int(const char *s, int prev, long lo, long hi) {
  long v = parse_long(s, prev);
  return (int)(v < lo ? lo : v > hi ? hi : v);
}
// Same clamp against a slider's own published range, for the two places that
// have a param_def_t to point at. Integer math throughout: round-tripping a long
// through float would quietly lose the top of a 1..1000 range's precision.
static int parse_int_def(long v, const param_def_t *d) {
  long lo = (long)d->lo, hi = (long)d->hi;
  return (int)(v < lo ? lo : v > hi ? hi : v);
}

static void config_load_or_create(const char *path, config_t *c) {
  *c = config_defaults();
  FILE *f = fopen(path, "r");
  if (!f) {
    // Only a genuinely missing file means "first start" — a locked or
    // unreadable existing config must never be overwritten with defaults.
    if (errno == ENOENT) config_write(path, c);
    else fprintf(stderr, "config: cannot read %s — using defaults\n", path);
    return;
  }
  char line[128];
  while (fgets(line, sizeof line, f)) {
    int cut_tail = 0;
    if (!strchr(line, '\n') && !feof(f)) {  // overlong line: discard the tail
      int ch;
      while ((ch = fgetc(f)) != EOF && ch != '\n') {}
      cut_tail = 1;  // loud below — a silently shortened host is a wrong host
    }
    char *hash = strchr(line, '#');
    int cut_hash = hash && hash > line && hash[-1] != ' ' && hash[-1] != '\t' &&
                   hash[-1] != '\n' && hash[-1] != '\r';  // '#' glued to a token
    if (hash) *hash = '\0';
    // val is 64 wide for string-valued keys (mp_host — design
    // WIDTH MATTERS: %11s cut "192.168.1.100" to the VALID address "192.168.1.1", a
    // silent wrong-machine connect). Width and format are TWO COPIES of one
    // number: widen %63s and val[64] together or it is a stack smash, not a
    // truncation. parse_float's buf[12] above stays 12 on purpose.
    char key[24], val[64];
    int used = -1;
    if (sscanf(line, "%23s %63s%n", key, val, &used) != 2) continue;
    // Loud, not silent: a value that hit the 63-char format cap, a line whose
    // tail fell off at 127 chars, or a '#' glued into a token would all reach
    // g_cfg quietly mutilated while the file shows the full override.
    if (used > 0) {
      const char *rest = line + used;
      while (*rest == ' ' || *rest == '\t' || *rest == '\n' || *rest == '\r') rest++;
      if (*rest) fprintf(stderr, "config: value of '%s' truncated or has trailing garbage\n", key);
    }
    if (cut_tail) fprintf(stderr, "config: line for '%s' longer than 127 chars — tail dropped\n", key);
    if (cut_hash) fprintf(stderr, "config: '#' cuts the value of '%s' (quote it is not supported)\n", key);
    if      (!strcmp(key, "sense")) c->sense = parse_float(val, c->sense, 0.001f, 10.0f);
    // Range wider than the slider's on both ends, the same policy every other
    // scalar here follows: a hand-edited file is allowed to ask for more than
    // the menu offers, and the menu must not quietly rewrite it (see
    // ui_param_row's snap-on-interaction-only rule).
    else if (!strcmp(key, "ads_sense")) c->ads_sense = parse_float(val, c->ads_sense, 0.01f, 10.0f);
    else if (!strcmp(key, "fov"))   c->fov   = parse_float(val, c->fov, 50.0f, 130.0f);
    else if (!strcmp(key, "weapon_fov")) c->vm_fov = parse_float(val, c->vm_fov, 50.0f, 74.0f);
    else if (!strcmp(key, "weapon_pos")) c->vm_pos = parse_int(val, c->vm_pos, 0, 2);
    else if (!strcmp(key, "vsync") || !strcmp(key, "fps_cap") ||
             !strcmp(key, "show_fps") || !strcmp(key, "msaa")) {
      // The 0/1 flags stay on `? 1 : 0` rather than a parse_int(0, 1) clamp: a
      // hand-edited `vsync -1` means "on" today and a clamp would make it "off".
      if      (key[0] == 'v') c->vsync    = parse_long(val, c->vsync) ? 1 : 0;
      else if (key[0] == 'm') c->msaa     = msaa_snap(parse_long(val, c->msaa));
      else if (key[0] == 'f') c->fps_cap  = parse_int(val, c->fps_cap, 0, 1000);
      else                    c->show_fps = parse_long(val, c->show_fps) ? 1 : 0;
    }
    else if (!strcmp(key, "third_person")) {
      c->third_person = parse_long(val, c->third_person) ? 1 : 0;
    }
    else if (!strcmp(key, "knife_model"))
      c->knife_model = parse_int(val, c->knife_model, 0, KNIFE_MODEL_COUNT - 1);
    else if (!strcmp(key, "rifle_model"))
      c->rifle_var = parse_int(val, c->rifle_var, 0, WPN_VARS - 1);
    else if (!strcmp(key, "pistol_model"))
      c->pistol_var = parse_int(val, c->pistol_var, 0, WPN_VARS - 1);
    else if (!strcmp(key, "weapon_theme"))
      c->wpn_theme = parse_int(val, c->wpn_theme, 0, WPN_THEME_COUNT - 1);
    else if (!strcmp(key, "gear_theme"))
      c->gear_theme = parse_int(val, c->gear_theme, 0, GEAR_THEME_COUNT - 1);
    else if (!strcmp(key, "head_gear"))
      c->head_gear = parse_int(val, c->head_gear, 0, HEAD_GEAR_COUNT - 1);
    else if (!strcmp(key, "vest_gear"))
      c->vest_gear = parse_int(val, c->vest_gear, 0, VEST_GEAR_COUNT - 1);
    else if (!strcmp(key, "pack_gear"))
      c->pack_gear = parse_int(val, c->pack_gear, 0, PACK_GEAR_COUNT - 1);
    else if (!strcmp(key, "hints")) c->hints = parse_long(val, c->hints) ? 1 : 0;
    else if (!strcmp(key, "crosshair_style")) c->ch_style = parse_int(val, c->ch_style, 0, CH_STYLE_COUNT - 1);
    else if (!strcmp(key, "crosshair_color")) c->ch_color = parse_int(val, c->ch_color, 0, CH_COLOR_COUNT - 1);
    else if (!strcmp(key, "crosshair_size"))  c->ch_size = parse_float(val, c->ch_size, 0.5f, 2.0f);
    else if (!strcmp(key, "hud_opacity"))     c->hud_alpha = parse_float(val, c->hud_alpha, 0.2f, 1.0f);
    else if (!strcmp(key, "hud_scale"))       c->hud_scale = parse_float(val, c->hud_scale, 0.75f, 1.5f);
    else if (!strcmp(key, "enemy_markers"))   c->enemy_mark = parse_long(val, c->enemy_mark) ? 1 : 0;
    else if (!strcmp(key, "perf_overlay"))    c->perf = parse_long(val, c->perf) ? 1 : 0;
    else if (!strncmp(key, "stat_", 5)) {
      int *dst = !strcmp(key, "stat_matches")     ? &c->st_matches
               : !strcmp(key, "stat_wins")        ? &c->st_wins
               : !strcmp(key, "stat_kills")       ? &c->st_kills
               : !strcmp(key, "stat_deaths")      ? &c->st_deaths
               : !strcmp(key, "stat_headshots")   ? &c->st_hs
               : !strcmp(key, "stat_best_streak") ? &c->st_best : NULL;
      if (dst) *dst = parse_int(val, *dst, 0, 1000000000);
    }
    // The DEV tab's runtime opt-in. It stays a config key with no menu toggle
    // on purpose: a knob that can switch itself on from inside the menu is one
    // stray click away from a player standing in the tuning workbench.
    else if (!strcmp(key, "devmode")) {
      c->devmode = parse_long(val, c->devmode) ? 1 : 0;
    }
    else if (!strcmp(key, VOL_DEF.key)) {
      c->vol = parse_float(val, c->vol, VOL_DEF.lo, VOL_DEF.hi);
    }
    else if (!strcmp(key, "sp_bots") || !strcmp(key, "sp_difficulty") ||
             !strcmp(key, "sp_fraglimit")) {
      if      (key[3] == 'b') c->sp_bots = parse_int_def(parse_long(val, c->sp_bots),
                                                         &SP_BOTS_DEF);
      // sp_diff is a ui_cycle enum, not a slider, so it has no param_def_t.
      else if (key[3] == 'd') c->sp_diff = parse_int(val, c->sp_diff, 0, 2);
      else                    c->sp_frag = parse_int_def(parse_long(val, c->sp_frag),
                                                         &SP_FRAG_DEF);
    }
    else if (!strcmp(key, "sp_mode")) {
      int m = gm_parse(val);
      if (m >= 0) c->sp_mode = m;
    }
    else if (!strcmp(key, "host_mode")) {
      int m = gm_parse(val);
      if (m >= 0) c->host_mode = m;
    }
    else if (!strcmp(key, "sp_map")) {
      int m = map_parse(val);
      if (m >= 0) c->sp_map = m;
    }
    else if (!strcmp(key, "sp_team_size")) c->sp_team_size = parse_int(val, c->sp_team_size, 1, TEAM_SIZE_MAX);
    else if (!strcmp(key, "sp_team")) c->sp_team = parse_int(val, c->sp_team, 0, 1);
    else if (!strcmp(key, "host_map")) {
      int m = map_parse(val);
      if (m >= 0) c->host_map = m;
    }
    else if (!strcmp(key, "host_bots")) c->host_bots = parse_int(val, c->host_bots, 0, MAX_BOTS);
    else if (!strcmp(key, "host_team_size")) c->host_team_size = parse_int(val, c->host_team_size, 1, TEAM_SIZE_MAX);
    else if (!strcmp(key, "host_team_fill")) c->host_team_fill = parse_int(val, c->host_team_fill, 0, 1);
    else if (!strcmp(key, "host_friendly_fire")) c->host_team_ff = parse_int(val, c->host_team_ff, 0, 1);
    else if (!strcmp(key, "sp_squad")) c->sp_squad = parse_int(val, c->sp_squad, 1, RESC_SQUAD_MAX);
    else if (!strcmp(key, "host_squad")) c->host_squad = parse_int(val, c->host_squad, 1, RESC_SQUAD_MAX);
    else if (!strcmp(key, SP_ROUNDS_DEF.key)) {
      c->sp_rounds = parse_int_def(parse_long(val, c->sp_rounds), &SP_ROUNDS_DEF);
    }
    else if (!strcmp(key, "player_name")) {
      int n = 0;
      for (const char *q = val; *q && n < (int)sizeof c->name - 1; q++) {
        char g = name_glyph(*q);
        if (g) c->name[n++] = g;
      }
      if (n) c->name[n] = '\0';  // empty/garbage keeps the default
    }
    else if (!strcmp(key, "post_fx")) {
      c->post_fx = parse_long(val, c->post_fx) ? 1 : 0;
    }
    else if (!strcmp(key, "ballistics")) {
      c->ballistics = parse_long(val, c->ballistics) ? 1 : 0;
    }
    else if (!strcmp(key, "mp_host")) {
      snprintf(c->mp_host, sizeof c->mp_host, "%s", val);   // 64 == val width
    }
    else if (!strcmp(key, "mp_port")) {
      c->mp_port = parse_int(val, c->mp_port, 1, 65535);
    }
    else {
      int hit = 0;
      for (int i = 0; i < MV_COUNT; i++)
        if (!strcmp(key, MV_DEF[i].key)) {
          c->mv[i] = parse_float(val, c->mv[i], MV_DEF[i].lo, MV_DEF[i].hi);
          hit = 1;
        }
      for (int w = 0; w < WPN_COUNT && !hit; w++)
        for (int i = 0; i < WP_COUNT && !hit; i++) {
          char full[32];
          wp_key(full, sizeof full, w, i);
          // "wp_<key>" is the pre-sniper single-weapon form: map it to the AR
          // so existing tuned configs keep their feel.
          if (!strcmp(key, full) ||
              (w == WPN_AR && !strncmp(key, "wp_", 3) && !strcmp(key + 3, WP_DEF[i].key))) {
            c->wp[w][i] = parse_float(val, c->wp[w][i], WP_DEF[i].lo, WP_DEF[i].hi);
            hit = 1;
          }
        }
      for (int i = 0; i < PAD_COUNT && !hit; i++)
        if (!strcmp(key, PAD_DEF[i].key)) {
          c->pad[i] = parse_float(val, c->pad[i], PAD_DEF[i].lo, PAD_DEF[i].hi);
          hit = 1;
        }
      // Migration: pad_sens_x in deg/s (the pre-notch scheme) maps onto the
      // 1-10 scale; pad_sens_y is derived now and its old value is dropped.
      if (!hit && !strcmp(key, "pad_sens_x")) {
        c->pad[PAD_SENS] = (float)parse_int(val, (int)(c->pad[PAD_SENS] * PAD_SENS_DEG_X),
                                            60, 600) / PAD_SENS_DEG_X;
        c->pad[PAD_SENS] = roundf(f_clamp(c->pad[PAD_SENS], PAD_DEF[PAD_SENS].lo,
                                          PAD_DEF[PAD_SENS].hi));
        hit = 1;
      }
      if (!hit && !strcmp(key, "pad_sens_y")) hit = 1;
      if (!hit && !strcmp(key, "pad_curve")) { c->pad_curve = parse_int(val, c->pad_curve, 0, 2); hit = 1; }
      if (!hit && !strcmp(key, "pad_inv_y")) { c->pad_inv_y = parse_long(val, c->pad_inv_y) ? 1 : 0; hit = 1; }
      if (!hit && !strcmp(key, "pad_assist")) { c->pad_assist = parse_long(val, c->pad_assist) ? 1 : 0; hit = 1; }
      for (int i = 0; i < IN_COUNT && !hit; i++)
        if (!strcmp(key, ACTION_DEF[i].key)) {
          // Lowercase single-character tokens: key_from_token maps 'W' and 'w'
          // to the same key id, so a hand-edited uppercase token would pass the
          // dedup below as distinct while colliding on the physical key.
          char t0 = val[0];
          if (t0 >= 'A' && t0 <= 'Z' && !val[1]) t0 = (char)(t0 - 'A' + 'a');
          // %.*s, not %s: val is 64 wide now, the token stays 12 — an
          // unbounded %s here is a real -Wformat-truncation the LTO build and
          // MinGW both swallow (the ASan build is where it showed).
          snprintf(c->bind[i], sizeof c->bind[i], "%.*s", (int)sizeof c->bind[i] - 1, val);
          c->bind[i][0] = t0;
          hit = 1;
        }
      for (int i = 0; i < IN_COUNT && !hit; i++) {
        char pk[24];
        padbind_key(pk, sizeof pk, i);
        if (!strcmp(key, pk)) {
          snprintf(c->padbind[i], sizeof c->padbind[i], "%.*s",
                   (int)sizeof c->padbind[i] - 1, val);
          hit = 1;
        }
      }
      if (!hit) fprintf(stderr, "config: ignoring unknown key '%s'\n", key);
    }
  }
  fclose(f);

  // Bind migration: configs written before ADS/RELOAD existed can collide
  // with the new defaults (the old jump default WAS mouse2, which ADS now
  // claims). On a collision the action squatting on another action's default
  // moves back to its own default; otherwise the higher-indexed one does.
  // Repeated until stable, because a reset can create a new collision.
  for (int pass = 0; pass < IN_COUNT; pass++) {
    int changed = 0;
    for (int i = 0; i < IN_COUNT && !changed; i++)
      for (int j = i + 1; j < IN_COUNT && !changed; j++) {
        if (strcmp(c->bind[i], c->bind[j]) != 0) continue;
        int fix = !strcmp(c->bind[i], ACTION_DEF[j].dflt) ? i : j;
        fprintf(stderr, "config: '%s' bound to both %s and %s — %s reset to '%s'\n",
                c->bind[i], ACTION_DEF[i].key, ACTION_DEF[j].key, ACTION_DEF[fix].key, ACTION_DEF[fix].dflt);
        snprintf(c->bind[fix], sizeof c->bind[fix], "%s", ACTION_DEF[fix].dflt);
        changed = 1;
      }
    if (!changed) break;
  }

  // Same collision rule for the pad table, except "none" may repeat freely —
  // it is the absence of a bind, not a key two actions could fight over.
  for (int pass = 0; pass < IN_COUNT; pass++) {
    int changed = 0;
    for (int i = 0; i < IN_COUNT && !changed; i++)
      for (int j = i + 1; j < IN_COUNT && !changed; j++) {
        if (!strcmp(c->padbind[i], "none")) continue;
        if (strcmp(c->padbind[i], c->padbind[j]) != 0) continue;
        int fix = !strcmp(c->padbind[i], ACTION_DEF[j].pdflt) ? i : j;
        fprintf(stderr, "config: '%s' bound to both pad %s and %s — %s reset to '%s'\n",
                c->padbind[i], ACTION_DEF[i].key, ACTION_DEF[j].key,
                ACTION_DEF[fix].key, ACTION_DEF[fix].pdflt);
        snprintf(c->padbind[fix], sizeof c->padbind[fix], "%s", ACTION_DEF[fix].pdflt);
        changed = 1;
      }
    if (!changed) break;
  }
}

