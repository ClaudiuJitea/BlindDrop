// code/harness/proofs.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
static void usage(void) {
  // printf rather than puts so the two match-slider ranges come from the ONE
  // place that defines them — a hardcoded "1-20" here is the copy nobody greps.
  printf("BlindDrop — NO ARGUMENTS PLAYS THE GAME (X11 window).\n"
       "             --do/--script run the headless harness instead.\n"
       "  --connect H[:P]    join a server on host H (port P, else mp_port)\n"
       "                     while the local match runs until the first snapshot\n"
       "  --server           dedicated server: sim only, no GL/audio/window.\n"       "  --name NAME        server name shown in the MULTIPLAYER page's LAN list\n"       "  --team-size N      TEAM DEATHMATCH places per side, 1..9 (default 5)\n"
       "  --team-fill 0|1    bots fill the places no player holds (default 1)\n"
       "  --friendly-fire 0|1  teammates can hurt each other (default 0)\n"
       "  --squad N          HOSTAGE RESCUE squad, 1..3 (default 3): players\n"
       "                     first, bots in the places nobody holds\n"
       "                     with --port N --cap-bots N (bots in the arena)\n"
       "                     --cap-public N (arena size, public door)\n"
       "                     --lobbies N (independent arenas in this ONE\n"
       "                     process on this ONE port; a new one fills only\n"
       "                     once the previous is full)\n"
       "                     --skill 0|1|2 --fraglimit N\n"
       "                     --mode dm|gun|sniper|blades|lms --rounds N\n"
       "                     --map arena|foundry|bazaar|outpost|citadel|harbor|refinery|rotate and\n"
       "                     --ticks N (exit after N ticks; 0 = run forever)\n"
       "                     --net-debug (verbose: trims/skips, one line each)\n"
       "                     heartbeats and answers QUERY. --port N --ticks N\n"
       "  --windowed         play in a window rather than borderless fullscreen\n"
       "  --w N --h N        window size when playing / framebuffer size for the\n"
       "                     harness (default 1280x720; fullscreen uses the display)\n"
       "  --seed N           map seed (harness default 1337; a played session is\n"
       "                     time-seeded unless you pass this)\n"
       "  --config PATH      config file (default: config.cfg next to binary)\n"
       "  --print-config     dump effective config and exit\n"
       "  --do \"CMDS\"        run script commands, headless (also: --script FILE)\n"
       "script commands (whitespace/;-separated):\n"
       "  wait N             advance N ticks (120 Hz)\n"
       "  +A / -A            press/release action: forward back left right jump crouch\n"
       "                     lean_left lean_right fire ads reload swap slot1..slot5\n"
       "                     score view (view toggles 3rd person)\n"
       "  weapon ar|sr|pt|kn|gr switch weapon (no raise animation)\n"
       "  tap A              press action for exactly one tick\n"
       "  look DX DY         inject mouse counts (scaled by sense)\n"
       "  aim X Y Z          snap yaw/pitch to look at a world point\n"
       "  warp X Y Z         teleport the player (peek/camera framing tests)\n"
       "  pos                print tick/position/velocity/eye/grounded/lean/shots\n"
       "  lean               print lean command/clearance/eye+head offset/exposure\n"
       "  wpn                print ammo/spread/recoil/bloom/ads/reload\n"
       "  bot                print every bot: position/hp/state/target + belief\n"
       "  aimbot             snap yaw/pitch onto the nearest bot's head\n"
       "  botfreeze [on|off] toggle or set ALL bot AI (stable capture target)\n"
       "  rag N [BOT]      ragdoll per tick + summary, across the corpse handover\n"
       "  ragx N [BOT]     the RENDERED corpse against the ground and the solids:\n"
       "                   float/hang (in the air) sink/pen (in geometry),\n"
       "                   span/hgt (sprawled or balled). figv adds the joints\n"
       "  ragsoak N        ragx over EVERY corpse and dead entity in a live\n"
       "                   match for N ticks — the multi-body soak\n"
       "  flinch           6 impact locations on bot 0 (stages itself): peak\n"
       "                   joint travel per case + how far apart the 6 films are\n"
       "  hud on|off         show/hide gameplay HUD (menus remain visible)\n"
       "  showfps on|off     show/hide the FPS counter idempotently\n"
       "  armory knife|theme|gear N  cosmetic loadout (ARMORY page) by index\n"
       "  puppet on|off      drive bot 0 by script instead of AI (anim testbed)\n"
       "  puppet move DX DZ | speed S | face DEG | crouch 0|1 | lean -1..1 |\n"
       "                     ads 0|1 | jump | slide | warp X Z\n"
       "  skel N             advance N ticks printing bot 0 skeleton state\n"
       "  kine N             advance N ticks printing solved foot/hand kinematics\n"
       "  bots N             set bot count %d-%d (applies next tick)\n"
       "  skill easy|normal|hard   set bot difficulty\n"
       "  fraglimit N        set the frag limit %d-%d\n"
       "  mode dm|gun|sniper|blades|lms  set the game mode (applies on restart)\n"
       "  rounds N           LAST MAN STANDING rounds to win\n"
       "  usemap arena|foundry|bazaar|outpost|citadel|harbor|refinery|rotate  set the map (applies on restart)\n"
       "  match              print match state + one score line per entity\n"
       "  restart            queue a match restart + fresh procedural map (next tick)\n"
       "  sfx DIR            dump all synthesized sounds as WAV + level stats\n"
       "  sfxlog N           print every audio routing decision for N ticks\n"
       "  mixdown P.wav N    render the real voice mixer + limiter for N ticks\n"
       "  map                theme/wind/surface treatment + all collision solids\n"
       "  trace N            advance N ticks, printing pos each tick\n"
       "  shot PATH.png      render + write screenshot\n"
       "  capture V.rgb A.wav FPS N  RGB24+WAV capture at FPS=60|120 for N frames\n"
       "  cam X Y Z YAW PIT  override camera for later shots (cam off: back to player)\n"
       "  cambot DX DY DZ    camera at bot+offset looking at the bot (cambot off)\n"
       "  camorbit R H D L   orbit bot 0: radius, height, deg/s, look height\n"
       "                     (camorbit off returns to the previous player camera)\n"
       "  vmhand             wrist bend / forearm-to-bore / palm length, per hand\n"
       "  vmorbit YAW PIT D W  viewmodel turntable, deg+m, W=0 trigger wrist\n"
       "                     1 support wrist  2 both  3 the trigger blade\n"
       "  camaim Y           height on the bot that cambot looks at (default 0.9)\n"
       "machine-checked proofs (each must read 0 / ok):\n"
       "  figcheck N [DIST]  bot 0 figure over N ticks: open/flip/dup/zfight/\n"
       "                     near/cross/degen. cross = two surfaces passing\n"
       "                     through each other at a shallow angle (a seam the\n"
       "                     tessellation decides, not the geometry)\n"
       "                     (DIST = camera distance, to sweep the profile tiers)\n"
       "  figv               toggle printing each figcheck offender's location\n"
       "  figm               every figure part's measured extent in mm + bone lengths\n"
       "  figbury [head]     third-person weapon vs the BODY: deepest vertex\n"
       "                     inside the trunk, %% of the weapon hidden in it,\n"
       "                     and how far the BUTT PAD is buried. `head` asks\n"
       "                     the same of the SKULL against the trunk instead\n"
       "  vmcheck            same analysis, both weapons x 11 ADS steps x 7 poses\n"
       "  vmsight            hand/forearm verts inside the eye's sight cone (must be 0)\n"
       "  vmtrig             trigger guard + blade against the HAND'S MESH, per\n"
       "                     weapon x reload station x press x first/third person\n"
       "  mapcheck N         regenerate N arenas; count coplanar same-facing faces\n"
       "  parity             24 player-vs-bot gameplay rows, ok/MISMATCH each\n"
       "  bothear            authoritative shot/step hearing and investigation proof\n"
       "  botweapon          bot weapon-choice/swap/cooldown hysteresis proof\n"
       "  perfbench N        N timed frames: sim / render CPU / GPU avg p50 p95 max\n"
       "  hurt N             N damage to the local player (capture death/damage states)\n"
       "  spawncheck N       N ticks: %% of non-player deaths within 1 s / 3 s of a respawn\n"
       "  meleecheck         knife: hit, thrust, block, parry, rear and flank\n"
       "  blastcheck         grenade: damage falloff, blast edge and cover\n"
       "  botmemory          bot line-of-sight memory proof\n"
       "  botflank           bot flank anchor/commitment/abort proof\n"
       "  padbackend         normalized gamepad mapper proof (std/XInput/HID)\n"
       "  tacstat N          bot tactics census incl. belief memory/search/reacq\n"
       "  budget             scene/peak/max verts + drops (render a frame FIRST —\n"
       "                     it only accumulates while the scene is being built)\n"
       "gamepad commands (raw state exactly as a platform pump writes it):\n"
       "  pad AXIS V         set raw axis: lx ly rx ry (-1..1, +y=up) lt rt (0..1)\n"
       "  pad btn B 0|1      set button level: a b x y lb rb lt rt select start\n"
       "                     l3 r3 up down left right p1 p2 p3 p4\n"
       "  pad on|off         mark a pad connected / clear all pad state\n"
       "  padsync            one shared pad frame step (edges, nav, trig hysteresis)\n"
       "  padstat            processed pad state + actions held through the binds\n"
       "  padscan [N]        the REAL /dev/input scan for N frames (default\n"
       "                     600): the node table, which one is ACTIVE, and\n"
       "                     every hand-over. The on-device pad proof\n"
       "  padcurve           f(0..1) table for linear/standard/dynamic (proof)\n"
       "  padlook N          N ticks of right-stick look, yaw/pitch delta in deg\n"
       "menu/UI commands (uiframe and shot both run one UI logic+draw pass):\n"
       "  menu               toggle the pause menu\n"
       "  home on|off|T      the boot screen (tiles + live showcase); T jumps\n"
       "                     the director's clock to T seconds into the loop\n"
       "  cursor X Y         set UI cursor (framebuffer px)\n"
       "  mdown / mup        press / release the UI mouse button\n"
       "  wheel up|down      one mouse-wheel notch (cycles the weapon on the\n"
       "                     next appframe; ignored while a menu is open)\n"
       "  key TOKEN          feed a key token to an active rebind capture\n"
       "  nav DIR            queue pad navigation for the next uiframe/shot:\n"
       "                     up down left right ok back; tabl/tabr are ignored\n"
       "  dev on|off         show/hide the DEV TOOLS root action (config key\n"
       "                     devmode, default off; required before a DEV-page rig)\n"
       "  uistat             print menu page/control/focus/modal state + pad cfg\n"
       "  uiframe            run one UI pass without a screenshot\n"
       "  appframe N         N frames through the real frame step (app_frame);\n"
       "                     with the menu open this runs 0 ticks — the pause\n"
       "                     proof. Schedules audio voices; avoid in mixdown\n"
       "  cfg                print live config values\n"
       "  netpack N          encode/decode round-trip over N random states;\n"
       "                     prints the worst quantization error per field\n"
       "  netfuzz N          N random/garbage packets into every decoder; no\n"
       "                     crash, every accepted record in bounds (run asan)\n"
       "  netpredict N       advance N, build+encode+decode a snapshot, verify\n"
       "                     reconstruction (mm) and the resend dedup property\n"
       "  netlagcomp [R]     shot vs a target rewound R ticks: HIT with\n"
       "                     compensation, miss without, residual in mm\n"
       "  netstall [G]       the live input queue: dup-free redundancy, gap\n"
       "                     fill rule, rate cap, and a G-tick far-ahead jump\n"
       "  netanim N [S]      plants at 120 Hz vs from an S-strided snapshot\n"
       "  netfill            fill/cap invariants over a join/leave timeline\n"
       "  netloop N          N ticks through sim_advance vs the server loop's\n"
       "                     tick body from one saved state; prints MISMATCH\n"
       "                     with the first divergent tick, or ok\n"
       "  netclient H P N [ACTS]  headless client: connect to a --server on\n"
       "                     H:P, drive N input ticks holding ACTS (comma-\n"
       "                     separated action names, default forward), print\n"
       "                     handshake + reconcile + event-channel stats\n"
       "                     (localhost end-to-end, no GL). `fire` in ACTS must\n"
       "                     move ev_applied: that is the server's shot coming\n"
       "                     back as an event\n"
       "  netdeath           online death -> ragdoll -> corpse (client half)\n"
       "  netleave           online->local teardown: every count must be 0\n"
       "  netstat            print the server digest line (the SAME function\n"
       "                     the --server mode prints every 10 s)\n"
       "  netdebug           toggle per-event net diagnostics: one line per\n"
       "                     reconcile correction, backlog trim and far-ahead\n"
       "                     skip (--net-debug is the --server equivalent)\n"
       "  humans N           set the human roster to N (slots 1+ spawn idle;\n"
       "                     entity ids 21+, see the three-range rule)\n"
       "  localent N         force g_local_ent (identity-sweep instrument;\n"
       "                     diff sfxlog/HUD against the id-0 run)\n\n",
       (int)SP_BOTS_DEF.lo, (int)SP_BOTS_DEF.hi,
       (int)SP_FRAG_DEF.lo, (int)SP_FRAG_DEF.hi);
}

typedef struct {
  player_t *p;
  int held[IN_COUNT];
  long tick;
  cam_override_t cam;
} sim_ctx_t;

static void sim_advance(sim_ctx_t *s) {
  player_tick(s->p, s->held);
  net_hist_record();   // write-only lag-comp history; nothing reads it in SP,
                       // so the byte gate is untouched (the net* proofs read it)
  // Drive the AUDIO ROUTING from the headless sim too, so `sfxlog` and
  // `mixdown` exercise exactly the code the played frame loop runs (the same
  // audio_frame_drain). Off unless the harness asked for it, so it cannot
  // perturb a timing or determinism test.
  if (g_audio_route) {
    g_sfx_log_tick = s->tick;
    audio_frame_drain(s->p);
  }
  s->tick++;
  g_num_events = 0;  // event lines were printed at push time; no-op after a drain
}

static void capture_orbit_advance(cam_override_t *cam, int fps) {
  if (!cam->orbit) return;
  cam->orbit_phase += cam->orbit_rate * DEG2RAD / (float)fps;
  v3 bp = g_bots[0].pos;
  cam->eye = (v3){{bp.x + sinf(cam->orbit_phase) * cam->orbit_radius,
                   bp.y + cam->orbit_height,
                   bp.z + cosf(cam->orbit_phase) * cam->orbit_radius}};
  v3 d = v3_sub(v3_add(bp, (v3){{0, cam->orbit_look, 0}}), cam->eye);
  cam->yaw = atan2f(d.x, -d.z);
  cam->pitch = f_clamp(atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z)),
                       -PITCH_MAX, PITCH_MAX);
}

static void capture_remove_outputs(const char *rgb_path, const char *wav_path) {
  remove(rgb_path);
  remove(wav_path);
}

static int capture_same_file(const char *a, const char *b) {
  struct stat sa, sb;
  return stat(a, &sa) == 0 && stat(b, &sb) == 0 &&
         sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static int capture_is_symlink(const char *path) {
  struct stat st;
  return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

static void capture_run(sim_ctx_t *s, const char *rgb_path,
                        const char *wav_path, int fps, long frames) {
  if (fps != 60 && fps != 120) {
    fprintf(stderr, "capture: fps must be 60 or 120\n");
    exit(1);
  }
  if (frames <= 0) {
    fprintf(stderr, "capture: frames must be positive\n");
    exit(1);
  }

  const size_t tick_samples = 400;
  const size_t ticks_per_frame = (size_t)(TICK_HZ / fps);
  size_t nframes = (size_t)frames;
  if ((size_t)R.width > SIZE_MAX / 3 ||
      (size_t)R.height > SIZE_MAX / ((size_t)R.width * 3)) {
    fprintf(stderr, "capture: size overflow\n");
    exit(1);
  }
  size_t rgb_frame_bytes = (size_t)R.width * (size_t)R.height * 3;
  if (nframes > SIZE_MAX / rgb_frame_bytes ||
      nframes > SIZE_MAX / ticks_per_frame ||
      nframes * ticks_per_frame > SIZE_MAX / tick_samples) {
    fprintf(stderr, "capture: size overflow\n");
    exit(1);
  }
  size_t audio_frames = nframes * ticks_per_frame * tick_samples;
  if (audio_frames > (size_t)INT_MAX ||
      audio_frames > (size_t)(UINT32_MAX - 36u) / (2u * SFX_CH) ||
      audio_frames > SIZE_MAX / (SFX_CH * sizeof(float))) {
    fprintf(stderr, "capture: size overflow\n");
    exit(1);
  }
  if (capture_is_symlink(rgb_path) || capture_is_symlink(wav_path)) {
    fprintf(stderr, "capture: output paths must not be symlinks\n");
    exit(1);
  }
  if (!strcmp(rgb_path, wav_path) || capture_same_file(rgb_path, wav_path)) {
    fprintf(stderr, "capture: output paths must differ\n");
    exit(1);
  }

  struct stat video_before;
  int video_existed = stat(rgb_path, &video_before) == 0;
  FILE *video = fopen(rgb_path, "wb");
  if (!video) {
    fprintf(stderr, "capture: cannot open video\n");
    exit(1);
  }
  if (capture_same_file(rgb_path, wav_path)) {
    fclose(video);
    if (!video_existed) remove(rgb_path);
    fprintf(stderr, "capture: output paths must differ\n");
    exit(1);
  }
  FILE *audio_probe = fopen(wav_path, "wb");
  if (!audio_probe) {
    fclose(video);
    remove(rgb_path);
    fprintf(stderr, "capture: cannot open audio\n");
    exit(1);
  }
  if (fclose(audio_probe) != 0) {
    fclose(video);
    capture_remove_outputs(rgb_path, wav_path);
    fprintf(stderr, "capture: cannot open audio\n");
    exit(1);
  }

  float *audio = malloc(audio_frames * SFX_CH * sizeof *audio);
  if (!audio) {
    fclose(video);
    capture_remove_outputs(rgb_path, wav_path);
    fprintf(stderr, "capture: out of memory\n");
    exit(1);
  }

  int old_audio_route = g_audio_route;
  float old_dev_rate = g_dev_rate;
  const char *error = NULL;
  size_t audio_at = 0;
  g_audio_route = 1;
  g_dev_rate = 1.0f;
  for (long frame = 0; frame < frames && !error; frame++) {
    for (size_t tick = 0; tick < ticks_per_frame; tick++) {
      sim_advance(s);
      audio_mix(audio + audio_at * SFX_CH, (uint32_t)tick_samples, SFX_CH);
      audio_at += tick_samples;
    }
    capture_orbit_advance(&s->cam, fps);
    render_frame(s->p, 1.0f, fps, &s->cam);
    if (!write_rgb_frame(video, R.width, R.height))
      error = "capture: cannot write video";
  }
  g_audio_route = old_audio_route;
  g_dev_rate = old_dev_rate;

  if (fclose(video) != 0 && !error) error = "capture: cannot write video";
  if (!error && !write_wav(wav_path, audio, (int)audio_frames))
    error = "capture: cannot write audio";
  free(audio);

  if (error) {
    capture_remove_outputs(rgb_path, wav_path);
    fprintf(stderr, "%s\n", error);
    exit(1);
  }
  printf("capture frames=%ld fps=%d video=%s audio=%s\n",
         frames, fps, rgb_path, wav_path);
}

static void sim_print(const sim_ctx_t *s) {
  const player_t *p = s->p;
  printf("t=%ld pos=(%.3f %.3f %.3f) vel=(%.3f %.3f %.3f) eye=%.3f dip=%.3f "
         "grounded=%d crouch=%d slide=%d lean=%.3f shots=%d\n",
         s->tick, (double)p->pos.x, (double)p->pos.y, (double)p->pos.z,
         (double)p->vel.x, (double)p->vel.y, (double)p->vel.z,
         (double)p->eye, (double)p->land_dip,
         p->grounded, p->crouched, p->sliding, (double)p->lean, p->shots);
}

static int action_from_name(const char *n) {
  for (int i = 0; i < IN_COUNT; i++)
    if (!strcmp(n, ACTION_DEF[i].script)) return i;
  fprintf(stderr, "script: unknown action '%s'\n", n);
  exit(1);
}

// Point the player at a world point FROM THE EYE THE ROUND LEAVES. While leaning
// that eye is up to 0.28 m off the body's centre line, and aiming from the
// centre line instead put every scripted shot a body-width wide of the target at
// 10 m — the test would then be measuring the harness, not the peek. Two passes,
// because the eye offset is a function of the yaw this is solving for.
static void aim_at(player_t *p, v3 tgt) {
  for (int i = 0; i < 2; i++) {
    v3 e = player_eye_pos(p);
    float dx = tgt.x - e.x, dy = tgt.y - e.y, dz = tgt.z - e.z;
    p->yaw = atan2f(dx, -dz);
    p->pitch = f_clamp(atan2f(dy, sqrtf(dx * dx + dz * dz)), -PITCH_MAX, PITCH_MAX);
  }
}

static char *g_tok_pushback;

static char *take_tok(void) {
  if (g_tok_pushback) {
    char *t = g_tok_pushback;
    g_tok_pushback = NULL;
    return t;
  }
  return strtok(NULL, " \t\r\n;");
}

static char *next_tok(void) {
  char *t = take_tok();
  if (!t) { fprintf(stderr, "script: missing argument\n"); exit(1); }
  return t;
}

// Every numeric argument in the ladder used to go through a bare atol/atoi/strtof
// with the end pointer thrown away, so a typo parsed as 0 and said nothing:
// `--do "wait abc; pos"` printed t=0 — the wait was a no-op and the test looked
// like it had run. A script is a test fixture; silently doing nothing is the one
// failure mode it must not have. The whole token has to be consumed, so `120x`
// is an error rather than 120.
static long parse_n(const char *t) {
  char *e;
  errno = 0;
  long v = strtol(t, &e, 10);
  // ERANGE is as fatal as trailing junk: a silently LONG_MAX-clamped count is
  // a test that runs a different length than it says.
  if (e == t || *e || errno == ERANGE) {
    fprintf(stderr, "script: '%s' is not an integer\n", t);
    exit(1);
  }
  return v;
}
// An OPTIONAL trailing number. A command that simply calls next_n() for one
// swallows the NEXT COMMAND when the argument is absent — `figcheck 200; vmcheck`
// died with "'vmcheck' is not an integer" — so the token is peeked and pushed
// back unless it really is a number.
static long opt_n(long dflt) {
  char *t = take_tok();
  if (!t) return dflt;
  char *e;
  errno = 0;
  long v = strtol(t, &e, 10);
  if (e == t || *e || errno == ERANGE) { g_tok_pushback = t; return dflt; }
  return v;
}

static float parse_f(const char *t) {
  char *e;
  float v = strtof(t, &e);
  // Reject non-finite results (literal nan/inf and ERANGE overflow, which
  // strtof reports as +/-HUGE_VALF) — a NaN-poisoned run still exits 0 and
  // looks like a pass. Bit test, not isnan(): -ffast-math folds isnan to
  // false. errno is NOT checked here: glibc also sets ERANGE on benign
  // underflow ("1e-50"), which parses to 0 and is fine.
  if (e == t || *e || f_is_nonfinite(v)) {
    fprintf(stderr, "script: '%s' is not a finite number\n", t);
    exit(1);
  }
  return v;
}
static long  next_n(void) { return parse_n(next_tok()); }
static float next_f(void) { return parse_f(next_tok()); }

static int next_on_off(const char *command) {
  const char *arg = next_tok();
  if (!strcmp(arg, "on")) return 1;
  if (!strcmp(arg, "off")) return 0;
  fprintf(stderr, "script: %s expects on or off\n", command);
  exit(1);
}

// Everything a proof rig stages, saved by VALUE on the rig's own stack frame
// so rigs can nest (botmemory drives bothear mid-proof and asserts the
// restore). This list is the UNION of what all four rigs touch — four
// hand-maintained copies of it had already drifted apart (botweapon leaked
// g_predicting/g_ev_peak/g_ev_drops, which budget prints).
typedef struct {
  bot_t         bots[MAX_BOTS];
  player_t      players[MAX_HUMANS];
  match_t       match;
  solid_t       solids[MAX_SOLIDS];
  event_t       events[MAX_EVENTS];
  unsigned char humans_on[MAX_HUMANS];
  int      num_solids, num_events, frozen, predicting, ev_peak;
  long     ev_drops, tick;
  uint64_t rng_bot;
} proof_world_t;

static void proof_world_save(proof_world_t *w) {
  memcpy(w->bots, g_bots, sizeof w->bots);
  memcpy(w->players, g_players, sizeof w->players);
  w->match = g_match;
  memcpy(w->solids, g_solids, sizeof w->solids);
  memcpy(w->events, g_events, sizeof w->events);
  memcpy(w->humans_on, g_humans_on, sizeof w->humans_on);
  w->num_solids = g_num_solids;
  w->num_events = g_num_events;
  w->frozen     = g_bots_frozen;
  w->predicting = g_predicting;
  w->ev_peak    = g_ev_peak;
  w->ev_drops   = g_ev_drops;
  w->tick       = g_tick;
  w->rng_bot    = g_rng_bot;
}

static void proof_world_restore(const proof_world_t *w) {
  memcpy(g_bots, w->bots, sizeof w->bots);
  memcpy(g_players, w->players, sizeof w->players);
  g_match = w->match;
  memcpy(g_solids, w->solids, sizeof w->solids);
  memcpy(g_events, w->events, sizeof w->events);
  memcpy(g_humans_on, w->humans_on, sizeof w->humans_on);
  g_num_solids  = w->num_solids;
  g_num_events  = w->num_events;
  g_bots_frozen = w->frozen;
  g_predicting  = w->predicting;
  g_ev_peak     = w->ev_peak;
  g_ev_drops    = w->ev_drops;
  g_tick        = w->tick;
  g_rng_bot     = w->rng_bot;
}

static void bothear_emit_shot(int src, v3 pos) {
  weapon_t w = {.cur = WPN_AR};
  anim_t anim;
  wpn_fill_mags(&w);
  anim_reset(&anim, pos, 0.0f);
  gun_shoot(src, &w, pos, 0.0f, 0.0f, (v3){{0,0,0}}, 1, &g_rng_bot, &anim);
}

// A deterministic proof of the simulation channel. It stages only the bot,
// match and collision inputs it needs, emits through the live writer and ticks
// the live AI; none of g_events/audio is read.
static void bothear_proof(void) {
  proof_world_t w;
  proof_world_save(&w);

  memset(&g_match, 0, sizeof g_match);
  memset(g_humans_on, 0, sizeof g_humans_on);
  g_match.nbots = 1;
  g_bots_frozen = 0;
  g_num_events = 0;
  #define BOTHEAR_STAGE() do {                                               \
    memset(g_humans_on, 0, sizeof g_humans_on);                              \
    g_bots[0] = (bot_t){.pos = {{0,0,0}}, .prev_pos = {{0,0,0}},             \
                         .target = {{0,0,8}}, .hp = PLAYER_HP, .alive = 1,    \
                         .active = 1, .grounded = 1, .eye = EYE_STAND,       \
                         .tgt = -1, .wp.cur = WPN_AR};                       \
    wpn_fill_mags(&g_bots[0].wp);                                            \
    anim_reset(&g_bots[0].anim, g_bots[0].pos, g_bots[0].yaw);                \
  } while (0)

  // A wall intersects this otherwise open shot. The accepted score is also a
  // direct guard that the 0.55 attenuation is applied rather than bypassed.
  g_num_solids = 1;
  g_solids[0] = (solid_t){.min = {{4,0,-1}}, .max = {{6,3,1}}, .pen = 0};
  BOTHEAR_STAGE();
  g_humans_on[1] = 1;
  g_players[1] = (player_t){.pos = {{10,0,0}}, .hp = PLAYER_HP, .alive = 1,
                             .grounded = 1, .eye = EYE_STAND};
  g_tick = 100;
  bothear_emit_shot(ent_of_human(1), (v3){{10,1.0f,0}});
  float yaw_before = g_bots[0].yaw;
  g_tick++;
  bot_tick(0);
  bot_t *b = &g_bots[0];
  // The score is the RELATIVE loudness left at this range — (34-10.05)/34 =
  // 0.7044 — times the 0.55 wall factor. Both halves of that product are
  // asserted here, which is what keeps a distant gunshot from drowning a boot
  // in the same room (see bot_hear_update).
  int shot = b->hear_tick == 100 && b->hear_kind == BOT_NOISE_SHOT &&
             b->hear_score > 0.385f && b->hear_score < 0.390f;
  float turn_delta = fabsf(wrap_pi(b->yaw - yaw_before));
  int turn = turn_delta > 1e-4f;
  // A turn onto a SOUND runs at the startle rate, not the tracking rate. The
  // cap still has to hold — the difference between a person spinning round and
  // a turret is that the fast part ends.
  int cap = turn_delta <= BOT_ALERT_TURN * TICK_DT + 1e-4f;

  // The living source is directly in the FOV but behind a wall. This proves
  // that hearing does not assign a target and cannot cross the LOS trigger
  // gate just because the sound itself was accepted.
  memset(g_match.noise, 0, sizeof g_match.noise);
  g_num_solids = 1;
  g_solids[0] = (solid_t){.min = {{-1,0,-6}}, .max = {{1,3,-4}}, .pen = 0};
  BOTHEAR_STAGE();
  g_humans_on[1] = 1;
  g_players[1] = (player_t){.pos = {{0,0,-10}}, .hp = PLAYER_HP, .alive = 1,
                             .grounded = 1, .eye = EYE_STAND};
  g_tick = 150;
  bothear_emit_shot(ent_of_human(1), (v3){{0,1.0f,-10}});
  g_tick++;
  bot_tick(0);
  int nofire = g_bots[0].tgt == -1 && g_bots[0].react_t == 0 &&
               g_match.shots[1] == 0 &&
               g_bots[0].wp.ammo[WPN_AR] == WPN_DEF[WPN_AR].mag;

  // The first several ticks are deliberately turn-only: the investigation
  // movement gate must wait until the sound is within 60 degrees of view.
  memset(g_match.noise, 0, sizeof g_match.noise);
  g_num_solids = 0;
  g_nladders = 0;
  BOTHEAR_STAGE();
  g_tick = 200;
  bothear_emit_shot(2, (v3){{10,1.0f,0}});
  for (int i = 0; i < 32; i++) { g_tick++; bot_tick(0); }
  int move = g_bots[0].pos.x > 0.02f;

  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 300;
  bothear_emit_shot(2, (v3){{35,1.0f,0}});
  g_tick++;
  bot_tick(0);
  int shot_far = g_bots[0].hear_tick != 0;

  // Footstep reach comes from the same pure source the animation contact uses.
  // At one shared 8 m standoff, a full upright run is audible and a crouched
  // run is not; a slow step at 4 m and an upright step beyond 11 m are quiet.
  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 400;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{8,0,0}},
                 bot_step_noise_radius(g_cfg.mv[MV_RUN], 0.0f));
  g_tick++;
  bot_tick(0);
  int step = g_bots[0].hear_tick == 400 &&
             g_bots[0].hear_kind == BOT_NOISE_STEP;

  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 420;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{8,0,0}},
                 bot_step_noise_radius(g_cfg.mv[MV_RUN], 1.0f));
  g_tick++;
  bot_tick(0);
  int crouch = g_bots[0].hear_tick != 0;

  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 440;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{4,0,0}},
                 bot_step_noise_radius(g_cfg.mv[MV_RUN] * 0.2f, 0.0f));
  g_tick++;
  bot_tick(0);
  int slow_quiet = g_bots[0].hear_tick == 0;

  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 460;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{12,0,0}},
                 bot_step_noise_radius(g_cfg.mv[MV_RUN], 0.0f));
  g_tick++;
  bot_tick(0);
  int far = g_bots[0].hear_tick != 0;

  // The believed SOURCE keeps refreshing itself. This case used to assert the
  // opposite — that a same-source contact with a similar or weaker score could
  // not move the memory — and that is exactly the freeze a runner going PAST
  // produced: a falling score, every later footfall rejected, and the bot
  // walking at a point the runner left half a second ago. The 15-percent
  // hysteresis answers "which source", never "where is the source I already
  // believe".
  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 480;
  float run_radius = bot_step_noise_radius(g_cfg.mv[MV_RUN], 0.0f);
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{6,0,0}}, run_radius);
  g_tick++;
  bot_tick(0);
  v3 first_hear_pos = g_bots[0].hear_pos;
  long first_hear_tick = g_bots[0].hear_tick;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{9.0f,0,0}}, run_radius);  // WEAKER
  g_tick++;
  bot_tick(0);
  int step_refresh = g_bots[0].hear_tick > first_hear_tick &&
                     g_bots[0].hear_src == 2 &&
                     v3_len(v3_sub(g_bots[0].hear_pos, first_hear_pos)) > 1e-3f;

  // ...while a DIFFERENT source at a merely similar score does not displace the
  // incumbent. That is what the hysteresis is for, and it is what stops a bot
  // ping-ponging between two people walking either side of it.
  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 490;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{6,0,0}}, run_radius);
  g_tick++;
  bot_tick(0);
  bot_noise_emit(3, BOT_NOISE_STEP, (v3){{-5.8f,0,0}}, run_radius);
  g_tick++;
  bot_tick(0);
  int cross_hysteresis = g_bots[0].hear_src == 2;

  // LOCALISATION. The stored point is a bearing with error, never the metre the
  // boot actually landed on, and the error is bigger through a wall. Both are
  // asserted against the pure BOT_HEAR_ERR arithmetic so a silent retune of one
  // constant cannot pass. hear_score is deliberately NOT perturbed: it decides
  // which source is believed, and blurring it would flip the ordering above.
  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 495;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{0,0,10}}, 34.0f);
  g_tick++;
  bot_tick(0);
  float open_err = v3_len(v3_sub(g_bots[0].hear_pos, (v3){{0,0,10}}));
  int localised = fabsf(open_err - BOT_HEAR_ERR * 10.0f) < 1e-3f &&
                  g_bots[0].hear_pos.y == 0.0f;
  memset(g_match.noise, 0, sizeof g_match.noise);
  g_num_solids = 1;
  g_solids[0] = (solid_t){.min = {{-2,0,4}}, .max = {{2,3,5}}, .pen = 0};
  BOTHEAR_STAGE();
  g_tick = 497;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{0,0,10}}, 34.0f);
  g_tick++;
  bot_tick(0);
  // TWO-SIDED, and against the exact expression the code evaluates. A bare
  // upper bound also admitted the UNDOUBLED error (1.6 m satisfies "<= 3.2"),
  // so deleting the wall term entirely left this green. Written this way it
  // pins the ORDER as well: clamping before the doubling would give
  // 2*min(1.6, 2.6) = 3.2 where clamping after gives min(3.2, 2.6) = 2.6.
  float wall_want = f_clamp(2.0f * BOT_HEAR_ERR * 10.0f, 0.0f, BOT_HEAR_ERR_MAX);
  float wall_err = v3_len(v3_sub(g_bots[0].hear_pos, (v3){{0,0,10}}));
  int localised_wall = fabsf(wall_err - wall_want) < 1e-3f;
  g_num_solids = 0;
  g_nladders = 0;

  // THE STARTLE, at the angle the whole complaint was about: a boot lands
  // 6 m directly BEHIND a patrolling bot. The contract is a bound, not a
  // number — how long a person takes to turn round — and it is a bound the
  // tracking rate alone could not meet (measured 124/74/56 ticks by tier
  // before the startle rate existed, 60/51/48 after).
  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 700;
  g_bots[0].yaw = g_bots[0].prev_yaw = 0.0f;   // facing -z; the noise is at +z
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{0,0,6}},
                 bot_step_noise_radius(g_cfg.mv[MV_RUN], 0.0f));
  int spin = -1;
  for (int i = 0; i < 400 && spin < 0; i++) {
    g_tick++;
    bot_tick(0);
    v3 hp = g_bots[0].hear_pos;
    float want = atan2f(hp.x - g_bots[0].pos.x, -(hp.z - g_bots[0].pos.z));
    if (fabsf(wrap_pi(want - g_bots[0].yaw)) < 0.1745f) spin = i + 1;
  }
  int startle_spin = spin > 0 && spin <= 72;   // 0.6 s for 180 deg, every tier

  // REACHABILITY, which is the whole reason the branch was rewritten. A bot
  // whose target it can no longer SEE must act on a fresher noise; a bot that
  // is looking straight at its target must not — you do not turn your back on
  // the man shooting at you.
  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_humans_on[1] = 1;
  g_players[1] = (player_t){.pos = {{0,0,-10}}, .hp = PLAYER_HP, .alive = 1,
                             .grounded = 1, .eye = EYE_STAND};
  g_tick = 800;
  g_bots[0].tgt = ent_of_human(1);
  g_bots[0].belief = BOT_BELIEF_OCCLUDED_HOLD;
  g_bots[0].last_seen = g_bots[0].belief_pos = g_players[1].pos;
  g_bots[0].last_seen_tick = g_tick - 100;   // the memory is OLDER than the boot
  g_bots[0].search_t = 600;
  g_num_solids = 1;
  g_solids[0] = (solid_t){.min = {{-2,0,-6}}, .max = {{2,3,-5}}, .pen = 0};
  float memory_yaw = g_bots[0].yaw = g_bots[0].prev_yaw = F_PI;  // at +z
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{8,0,0}},
                 bot_step_noise_radius(g_cfg.mv[MV_RUN], 0.0f));
  g_tick++;
  bot_tick(0);
  // The EAR must own this tick, not merely "the yaw moved" — the belief branch
  // would also have turned this fixture (its remembered point is at -z and the
  // bot faces +z), so a bare movement test passed with the whole reachability
  // change reverted. Assert the predicate itself, and that the turn went TOWARD
  // the noise at +x rather than toward the memory.
  float heard_bearing = atan2f(8.0f - g_bots[0].pos.x, -(0.0f - g_bots[0].pos.z));
  int heard_over_memory = g_bots[0].tgt == ent_of_human(1) &&
                          g_bots[0].startled == 1 &&
                          fabsf(wrap_pi(g_bots[0].yaw - memory_yaw)) > 1e-4f &&
                          fabsf(wrap_pi(heard_bearing - g_bots[0].yaw)) <
                              fabsf(wrap_pi(heard_bearing - memory_yaw));
  g_num_solids = 0;
  g_nladders = 0;

  // Same fixture, target VISIBLE: the noise must not move it at all.
  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_humans_on[1] = 1;
  g_players[1] = (player_t){.pos = {{0,0,-10}}, .hp = PLAYER_HP, .alive = 1,
                             .grounded = 1, .eye = EYE_STAND};
  g_tick = 900;
  g_bots[0].tgt = ent_of_human(1);
  g_bots[0].scan_t = 100;
  g_bots[0].react_t = 100;   // seen, but not yet allowed to shoot
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{8,0,0}},
                 bot_step_noise_radius(g_cfg.mv[MV_RUN], 0.0f));
  g_tick++;
  bot_tick(0);
  // want_yaw is the target at -z; a startled bot would have turned toward +x.
  int seen_beats_heard = g_bots[0].yaw < 0.02f && g_bots[0].yaw > -0.02f;

  memset(g_match.noise, 0, sizeof g_match.noise);
  BOTHEAR_STAGE();
  g_tick = 500;
  bot_noise_emit(2, BOT_NOISE_STEP, (v3){{6,0,0}}, 11.0f);
  bothear_emit_shot(3, (v3){{10,1.0f,0}});
  g_tick++;
  bot_tick(0);
  int priority = g_bots[0].hear_kind == BOT_NOISE_SHOT;
  long until = g_bots[0].hear_until;
  v3 patrol_start = g_bots[0].pos;
  g_bots[0].target = v3_add(g_bots[0].pos,
                             (v3){{sinf(g_bots[0].yaw) * 8.0f, 0,
                                   -cosf(g_bots[0].yaw) * 8.0f}});
  g_tick = until + 1;
  bot_tick(0);
  int expiry = g_bots[0].hear_tick == 0 && g_bots[0].tgt < 0 &&
               v3_len(v3_sub(g_bots[0].pos, patrol_start)) > 1e-4f;

  const char *pk = priority ? "shot" : "step";
  const char *ex = expiry ? "patrol" : "heard";

  proof_world_restore(&w);
  #undef BOTHEAR_STAGE
  int events_restored = g_num_events == w.num_events &&
                        memcmp(g_events, w.events, sizeof w.events) == 0;
  printf("bothear shot=%d turn=%d move=%d nofire=%d step=%d crouch=%d far=%d "
         "priority=%s expiry=%s cap=%d refresh=%d xhyst=%d loc=%d locwall=%d "
         "spin=%d startled=%d seenwins=%d\n",
         shot, turn, move, nofire, step, crouch, far, pk, ex, cap,
         step_refresh, cross_hysteresis, localised, localised_wall, spin,
         heard_over_memory, seen_beats_heard);
  if (!shot || !turn || !move || !nofire || shot_far || !step || crouch ||
      !slow_quiet || far || !step_refresh || !cross_hysteresis || !localised ||
      !localised_wall || !startle_spin || !heard_over_memory ||
      !seen_beats_heard || !priority || !expiry ||
      !cap || !events_restored) exit(1);
}

// Deterministic weapon-policy proof. It exercises the production decision and
// request helpers directly, then restores every staged simulation/event field.
// THE KNIFE'S RULES, each on its own staged exchange: a swing through a stationary
// bot 1.2 m ahead of the player. The arithmetic of melee_land is the claim, so it
// is run through the real melee_tick and its rays, not called directly.
static int melee_stage(int chain, float yaw_off, int guard_t, int *stag) {
  player_t *p = local_player();
  bot_t *b = &g_bots[0];
  v3 f = {{sinf(p->yaw), 0, -cosf(p->yaw)}};
  b->pos = v3_add(p->pos, v3_scale(f, 1.2f));
  b->yaw = p->yaw + F_PI + yaw_off;
  b->alive = b->active = 1;
  b->hp = 100;
  b->wp = (weapon_t){.cur = WPN_KN, .guard_t = guard_t};
  b->wp.ads_s = b->wp.ads_lin = guard_t ? 1.0f : 0.0f;
  weapon_t a = {.cur = WPN_KN, .melee_n = chain};
  v3 eye = player_eye_pos(p);
  for (int i = 0; i < 90; i++)
    melee_tick(g_local_ent, &a, eye, p->yaw, p->pitch, NULL, i == 0);
  *stag = a.stagger_t;
  return b->hp;
}

// THE DAMAGE AREA, run through the real nade_blast: falloff in the open from a
// staged bot, the edge of the radius, and a wall between the charge and the man.
static int blast_stage(v3 at, v3 victim) {
  bot_t *b = &g_bots[0];
  b->pos = victim;
  b->prev_pos = victim;
  b->alive = b->active = 1;
  b->hp = 100;
  b->vel = (v3){{0, 0, 0}};
  nade_blast(at, 2);          // credited to a bystander, never the victim
  return b->alive ? b->hp : 0;
}

static void blastcheck_proof(void) {
  v3 at = {{0, 0.05f, 18}};
  int hp[6], i = 0;
  static const float D[6] = {1.0f, 3.0f, 5.0f, 7.0f, 8.8f, 10.5f};
  for (; i < 6; i++) hp[i] = blast_stage(at, (v3){{D[i], 0, 18}});
  // Behind the tallest solid in the arena, 1 m from it, with the charge 1 m in
  // front: 2 m + the solid's depth, and it must take nothing.
  int bs = -1;
  float bh = 0.0f;
  for (int s = 0; s < g_num_solids; s++) {
    const solid_t *q = &g_solids[s];
    float w = q->max.x - q->min.x;
    if (q->min.y < 0.1f && q->max.y > 2.2f && w > 0.3f && w < 4.5f &&
        fabsf(q->max.z) < 40.0f && q->max.y > bh) { bh = q->max.y; bs = s; }
  }
  int covered = -1;
  if (bs >= 0) {
    const solid_t *q = &g_solids[bs];
    float z = (q->min.z + q->max.z) * 0.5f;
    covered = blast_stage((v3){{q->min.x - 1.0f, 0.05f, z}}, (v3){{q->max.x + 1.0f, 0, z}});
  }
  int mono = 1;
  for (i = 1; i < 6; i++) if (hp[i] < hp[i - 1]) mono = 0;
  int ok = mono && hp[0] <= 0 && hp[5] == 100 && (hp[4] > 90) && covered == 100;
  printf("blastcheck hp@1m=%d @3m=%d @5m=%d @7m=%d @8.8m=%d @10.5m=%d behind_cover=%d %s\n",
         hp[0], hp[1], hp[2], hp[3], hp[4], hp[5], covered, ok ? "ok" : "FAIL");
  bot_spawn(0);
}

static void meleecheck_proof(void) {
  int st0, st1, st2, st3, st4;
  int open = melee_stage(0, 0.0f, 0, &st0);
  int thrust = melee_stage(2, 0.0f, 0, &st0);
  int block = melee_stage(0, 0.0f, MELEE_PARRY_TICKS + 40, &st1);
  int parry = melee_stage(1, 0.0f, 5, &st2);
  int rear = melee_stage(0, F_PI, 5, &st3);
  int flank = melee_stage(0, 1.1f, MELEE_PARRY_TICKS + 40, &st4);
  int ok = open == 100 - MELEE[0].dmg && thrust == 100 - MELEE[2].dmg &&
           block == 100 - MELEE_CHIP && st1 == 0 && parry == 100 && st2 > 0 &&
           rear == 100 - MELEE[0].dmg && st3 == 0 && flank == 100 - MELEE_CHIP;
  printf("meleecheck open=%d thrust=%d block=%d parry=%d stagger=%d rear=%d "
         "flank=%d %s\n", open, thrust, block, parry, st2, rear, flank,
         ok ? "ok" : "FAIL");
  bot_spawn(0);
}

static void botweapon_proof(void) {
  proof_world_t w;
  proof_world_save(&w);

  g_tick = 100;
  g_num_events = 0;
  bot_t *b = &g_bots[0];
  *b = (bot_t){.pos = {{3,0,4}}, .active = 1, .alive = 1, .grounded = 1,
               .eye = EYE_STAND, .tgt = -1, .wp.cur = WPN_AR};
  wpn_fill_mags(&b->wp);

  b->wp.ammo[WPN_AR] = 0;
  bot_request_weapon(b, 0, bot_desired_weapon(b, 1, 10.0f, 0.0f));
  const char *empty = WPN_DEF[b->wp.cur].prefix;

  b->wp = (weapon_t){.cur = WPN_AR};
  wpn_fill_mags(&b->wp);
  bot_request_weapon(b, 0, bot_desired_weapon(b, 1, 24.0f, 0.0f));
  const char *far = WPN_DEF[b->wp.cur].prefix;

  b->wp = (weapon_t){.cur = WPN_SR};
  wpn_fill_mags(&b->wp);
  bot_request_weapon(b, 0, bot_desired_weapon(b, 1, 15.9f, 0.0f));
  const char *near = WPN_DEF[b->wp.cur].prefix;

  b->wp = (weapon_t){.cur = WPN_SR};
  wpn_fill_mags(&b->wp);
  bot_request_weapon(b, 0, bot_desired_weapon(b, 1, 24.0f, 5.1f));
  const char *moving = WPN_DEF[b->wp.cur].prefix;

  b->wp = (weapon_t){.cur = WPN_SR};
  wpn_fill_mags(&b->wp);
  b->hear_tick = g_tick;
  b->hear_until = g_tick + 1;
  // An investigation is a NON-engaging state. The engaging=1 row below is the
  // regression guard: a live hear memory (any gunshot inside 34 m, i.e. every
  // firefight) used to reach this branch first and pin the bot to the AR, so
  // the sniper preference was suppressed by an audio memory nothing steered
  // with. Sight wins the weapon choice; the ear only decides when nothing does.
  bot_request_weapon(b, 0, bot_desired_weapon(b, 0, 24.0f, 0.0f));
  const char *heard = WPN_DEF[b->wp.cur].prefix;
  b->wp = (weapon_t){.cur = WPN_SR};
  wpn_fill_mags(&b->wp);
  int heard_engaged = bot_desired_weapon(b, 1, 24.0f, 0.0f) == WPN_SR;

  b->hear_tick = b->hear_until = 0;
  b->wp = (weapon_t){.cur = WPN_AR, .switch_t = 1};
  wpn_fill_mags(&b->wp);
  int raise = !bot_request_weapon(b, 0, WPN_SR) && b->wp.cur == WPN_AR;

  b->wp = (weapon_t){.cur = WPN_SR, .reload_t = 7, .reload_tac = 1,
                      .dry_done = 1, .idx = 4};
  wpn_fill_mags(&b->wp);
  b->wp.cool[WPN_SR] = 17;
  int swapped_away = bot_request_weapon(b, 0, WPN_AR);
  int cancelled = g_num_events == 1 && g_events[0].type == EV_RELOAD_CANCEL &&
                  g_events[0].src == 1 && g_events[0].a.x == b->pos.x &&
                  g_events[0].a.y == b->pos.y && g_events[0].a.z == b->pos.z &&
                  g_events[0].wpn == WPN_SR && b->wp.reload_t == 0 &&
                  b->wp.reload_tac == 0 && b->wp.dry_done == 0 && b->wp.idx == 0;
  b->wp.switch_t = 0;
  int swapped_back = bot_request_weapon(b, 0, WPN_SR);
  int cooldown = swapped_away && swapped_back && b->wp.cool[WPN_SR] == 17;

  b->wp = (weapon_t){.cur = WPN_AR};
  wpn_fill_mags(&b->wp);
  int hysteresis = bot_desired_weapon(b, 1, 23.9f, 0.0f) == WPN_AR &&
                   bot_desired_weapon(b, 1, 24.0f, 0.0f) == WPN_SR;
  b->wp.cur = WPN_SR;
  hysteresis = hysteresis && bot_desired_weapon(b, 1, 16.0f, 0.0f) == WPN_SR &&
               bot_desired_weapon(b, 1, 15.9f, 0.0f) == WPN_AR;

  // A policy-triggered SR swap is still raising on its next tick. Stage the
  // live bot path at a distant visible target: unlike the player, this used to
  // start ADS during the raise and collapse the weapon/IK blend.
  memset(&g_match, 0, sizeof g_match);
  memset(g_humans_on, 0, sizeof g_humans_on);
  g_match.nbots = 1;
  g_humans_on[0] = 1;
  g_players[0] = (player_t){.pos = {{0,0,-30}}, .hp = PLAYER_HP, .alive = 1,
                             .grounded = 1, .eye = EYE_STAND};
  g_num_solids = 0;
  g_nladders = 0;
  g_num_events = 0;
  g_bots_frozen = 0;
  *b = (bot_t){.pos = {{0,0,0}}, .hp = PLAYER_HP, .alive = 1, .active = 1,
               .grounded = 1, .eye = EYE_STAND, .tgt = 0, .scan_t = 100,
               .react_t = 2, .burst = 3,
               .wp = {.cur = WPN_AR, .switch_t = 2}};
  wpn_fill_mags(&b->wp);
  bot_tick(0);
  int ads_raise = b->wp.switch_t > 0 && b->wp.ads_lin == 0.0f && b->wp.ads_s == 0.0f;

  // A real auditory investigation has no visible target and therefore
  // engaging=0. Exercise the live policy call inside bot_tick: a loaded AR
  // must replace the SR while the bot investigates, without manufacturing a
  // target from the sound.
  memset(&g_match, 0, sizeof g_match);
  memset(g_humans_on, 0, sizeof g_humans_on);
  g_match.nbots = 1;
  g_num_solids = 0;
  g_nladders = 0;
  g_num_events = 0;
  *b = (bot_t){.pos = {{0,0,0}}, .hp = PLAYER_HP, .alive = 1, .active = 1,
               .grounded = 1, .eye = EYE_STAND, .tgt = -1, .scan_t = 100,
               .burst = 3, .hear_pos = {{0,0,-8}}, .hear_tick = g_tick - 1,
               .hear_until = g_tick + 100, .hear_kind = BOT_NOISE_STEP,
               .hear_score = 3.0f, .wp = {.cur = WPN_SR}};
  wpn_fill_mags(&b->wp);
  bot_tick(0);
  int live_heard = b->tgt == -1 && b->wp.cur == WPN_AR &&
                   b->wp.switch_t > 0 && b->swap_cd > 0;

  // Emergency selection is a combat safety rule, not a policy preference: a
  // visible threat and an empty current magazine must bypass the AI swap
  // cooldown and change to the loaded weapon before reload can start.
  memset(&g_match, 0, sizeof g_match);
  memset(g_humans_on, 0, sizeof g_humans_on);
  g_match.nbots = 1;
  g_humans_on[0] = 1;
  g_players[0] = (player_t){.pos = {{0,0,-10}}, .hp = PLAYER_HP, .alive = 1,
                             .grounded = 1, .eye = EYE_STAND};
  g_num_solids = 0;
  g_nladders = 0;
  g_num_events = 0;
  *b = (bot_t){.pos = {{0,0,0}}, .hp = PLAYER_HP, .alive = 1, .active = 1,
               .grounded = 1, .eye = EYE_STAND, .tgt = 0, .scan_t = 100,
               .react_t = 2, .burst = 3, .swap_cd = 100,
               .wp = {.cur = WPN_AR}};
  b->wp.ammo[WPN_AR] = 0;
  b->wp.ammo[WPN_SR] = WPN_DEF[WPN_SR].mag;
  b->wp.cool[WPN_AR] = 7;
  b->wp.cool[WPN_SR] = 11;
  bot_tick(0);
  int live_emergency = b->wp.cur == WPN_SR && b->wp.switch_t > 0 &&
                       b->wp.reload_t == 0 &&
                       b->swap_cd == BOT_SWAP_COOLDOWN_TICKS &&
                       b->wp.ammo[WPN_AR] == 0 &&
                       b->wp.ammo[WPN_SR] == WPN_DEF[WPN_SR].mag &&
                       b->wp.cool[WPN_AR] == 6 && b->wp.cool[WPN_SR] == 10;

  // A close-range preference may not holster a loaded SR in favour of an
  // empty AR. Drive the same visible-target bot_tick path rather than calling
  // the pure selector in isolation.
  memset(&g_match, 0, sizeof g_match);
  g_match.nbots = 1;
  g_num_events = 0;
  *b = (bot_t){.pos = {{0,0,0}}, .hp = PLAYER_HP, .alive = 1, .active = 1,
               .grounded = 1, .eye = EYE_STAND, .tgt = 0, .scan_t = 100,
               .react_t = 2, .burst = 3, .wp = {.cur = WPN_SR}};
  b->wp.ammo[WPN_AR] = 0;
  b->wp.ammo[WPN_SR] = WPN_DEF[WPN_SR].mag;
  b->wp.cool[WPN_SR] = 13;
  bot_tick(0);
  int live_loaded_sr = b->wp.cur == WPN_SR && b->wp.switch_t == 0 &&
                       b->wp.reload_t == 0 && b->swap_cd == 0 &&
                       b->wp.ammo[WPN_AR] == 0 &&
                       b->wp.ammo[WPN_SR] == WPN_DEF[WPN_SR].mag &&
                       b->wp.cool[WPN_SR] == 12;

  proof_world_restore(&w);
  printf("botweapon empty=%s far=%s near=%s moving=%s heard=%s engaged_deaf=%d "
         "raise=%d cooldown=%s hysteresis=%d\n", empty, far, near, moving,
         heard, heard_engaged, raise, cooldown ? "kept" : "lost", hysteresis);
  if (strcmp(empty, "sr") || strcmp(far, "sr") || strcmp(near, "ar") ||
      strcmp(moving, "ar") || strcmp(heard, "ar") || !heard_engaged ||
      !raise || !cooldown ||
      !hysteresis || !cancelled || !ads_raise || !live_heard ||
      !live_emergency || !live_loaded_sr) exit(1);
}

// The FLANK's contract, staged. A tactic that fires is not automatically a
// tactic that means anything: what makes this one a flank rather than a wide
// strafe is that the anchor is genuinely OFF the axis the bot is already on,
// that the bot commits to it instead of re-deciding, and that needing cover
// still outranks wanting a better angle. Each of those is one assertion here.
// It drives the production helpers, not a copy of them.
static void botflank_proof(void) {
  proof_world_t w;
  proof_world_save(&w);

  memset(&g_match, 0, sizeof g_match);
  memset(g_humans_on, 0, sizeof g_humans_on);
  g_match.nbots = 1;
  g_num_solids = 0;
  g_nladders = 0;
  g_num_events = 0;
  g_bots_frozen = 0;
  g_tick = 100;
  // STAGE THE RATE AND THE STREAM. The claim is a per-tick probability, so a rig
  // that drives the real gate otherwise reports on its own luck: this one passed
  // from a cold start and failed after a 7200-tick 20-bot match had advanced
  // g_rng_bot. The leg takes its skill row as a PARAMETER, so the rig hands it
  // one with tac pinned — the mechanism is what is under test here, and the
  // per-tier RATES are tacstat's job. proof_world_t already carries the stream,
  // so setting it is staging rather than leaking.
  g_rng_bot = 0x9E3779B97F4A7C15ull;
  bot_skill_t rig = BOT_SKILL[g_cfg.sp_diff];
  rig.tac = 1.0f;
  const bot_skill_t *sk = &rig;

  // The anchor swings around the TARGET, both ways, and stays in the arena.
  bot_t *b = &g_bots[0];
  *b = (bot_t){.pos = {{0,0,12}}, .hp = PLAYER_HP, .alive = 1, .active = 1,
                .grounded = 1, .eye = EYE_STAND, .tgt = -1, .wp.cur = WPN_AR};
  v3 tpos = {{0,0,0}};
  v3 left, right;
  int have_l = bot_flank_spot(b, tpos, 1, &left);
  int have_r = bot_flank_spot(b, tpos, -1, &right);
  if (!have_l) left = b->pos;    // never read an unset candidate: the axis and
  if (!have_r) right = b->pos;   // radius tests below both fail on b->pos
  float base = atan2f(b->pos.x - tpos.x, b->pos.z - tpos.z);
  float al = atan2f(left.x - tpos.x, left.z - tpos.z);
  float ar = atan2f(right.x - tpos.x, right.z - tpos.z);
  // >= 45 deg of bearing change is the difference between a flank and a shuffle,
  // and the two sides must land on OPPOSITE sides of the current bearing.
  int axis = have_l && have_r &&
             fabsf(wrap_pi(al - base)) > F_PI_4 &&
             fabsf(wrap_pi(ar - base)) > F_PI_4 &&
             wrap_pi(al - base) * wrap_pi(ar - base) < 0.0f;
  // The RANGE band, which the in-arena/fits filters do not re-check and which
  // is what stops a point-blank fight producing a 2 m shuffle and a 25 m one
  // sending the bot to the far wall. Restating the filters instead was a
  // tautology: it could only fail where `axis` already had.
  float dl = sqrtf((left.x - tpos.x) * (left.x - tpos.x) +
                   (left.z - tpos.z) * (left.z - tpos.z));
  float dr = sqrtf((right.x - tpos.x) * (right.x - tpos.x) +
                   (right.z - tpos.z) * (right.z - tpos.z));
  int inside = have_l && have_r &&
               dl > 7.99f && dl < 14.01f && dr > 7.99f && dr < 14.01f;

  // COMMITMENT: once claimed, the anchor does not move while the bot walks to
  // it. Force the claim by zeroing the cooldown and driving the live leg.
  float wx = 0, wz = 0, pace = 0;
  b->last_seen = tpos;
  b->strafe_dir = 1;
  b->flank_cd = 0;
  int committed = 0, moved = 0;
  for (int i = 0; i < 40000 && !committed; i++) {
    bot_flank_tick(b);
    if (bot_flank_steer(b, sk, 12.0f, &wx, &wz, &pace)) committed = 1;
  }
  v3 anchor = b->flank_p;
  int stable = 1;
  for (int i = 0; i < 60; i++) {
    bot_flank_tick(b);
    if (!bot_flank_steer(b, sk, 12.0f, &wx, &wz, &pace)) { stable = 0; break; }
    if (v3_len(v3_sub(b->flank_p, anchor)) > 1e-6f) { stable = 0; break; }
    if (pace < 0.999f) { stable = 0; break; }
    b->pos.x += wx * 0.05f;   // walk it, so arrival is reachable
    b->pos.z += wz * 0.05f;
    moved = 1;
  }
  int commit = committed && stable && moved;

  // COVER OUTRANKS THE ANGLE: an empty magazine ends the flank on the spot,
  // and it may not restart while the cooldown it set is running.
  // The release has to happen in the CLOCK, not the leg — that is the whole
  // point of the split, since a bot out of contact never reaches the leg.
  b->wp.reload_t = 30;
  bot_flank_tick(b);
  int aborted = b->flank_t == 0 && b->flank_cd > 0 &&
                !bot_flank_steer(b, sk, 12.0f, &wx, &wz, &pace);
  b->wp.reload_t = 0;
  int held_off = 1;
  for (int i = 0; i < 200; i++) {
    bot_flank_tick(b);
    if (bot_flank_steer(b, sk, 12.0f, &wx, &wz, &pace)) { held_off = 0; break; }
  }

  // ARRIVAL releases, and it is COUNTED: standing on the anchor is not a flank
  // in progress, and an uncounted arrival is a census that cannot tell a
  // completed swing from a commitment that quietly timed out.
  b->flank_cd = 0;
  b->flank_t = 300;
  b->flank_p = b->pos;
  int arrived0 = b->flank_arrived;
  int arrival = !bot_flank_steer(b, sk, 12.0f, &wx, &wz, &pace) &&
                b->flank_t == 0 && b->flank_cd > 0 &&
                b->flank_arrived == arrived0 + 1;

  // A LOST TARGET ends the swing: the anchor was planned around somebody who is
  // no longer there, and a frozen commitment resumed against the next enemy is
  // the "it ran off to nowhere" tell.
  b->flank_cd = 0;
  b->flank_t = 300;
  bot_belief_clear(b);
  int dropped = b->flank_t == 0;

  proof_world_restore(&w);
  printf("botflank axis=%d inside=%d commit=%d abort=%d cooldown=%d arrive=%d "
         "dropped=%d\n",
         axis, inside, commit, aborted, held_off, arrival, dropped);
  if (!axis || !inside || !commit || !aborted || !held_off || !arrival ||
      !dropped) exit(1);
}

// THE BOTMEMORY FIXTURE, once. Three stages below stage the same thing — an
// empty arena, one bot at the origin, one human at `hpos` — and differ only in
// what the bot already believes. `seed` carries exactly that difference; every
// property a bot needs in order to be shootable and to shoot back (health,
// alive/active/grounded, eye height, a full magazine, a settled skeleton) is
// filled here, so a stage cannot forget one and measure a corpse.
static void botmem_stage(bot_t seed, v3 hpos) {
  memset(&g_match, 0, sizeof g_match);
  memset(g_humans_on, 0, sizeof g_humans_on);
  g_match.nbots = 1;
  g_humans_on[0] = 1;
  g_num_solids = 0;
  g_nladders = 0;
  g_num_events = 0;
  seed.prev_pos = seed.pos;
  seed.hp = PLAYER_HP;
  seed.alive = 1;
  seed.active = 1;
  seed.grounded = 1;
  seed.eye = EYE_STAND;
  seed.wp.cur = WPN_AR;
  g_bots[0] = seed;
  wpn_fill_mags(&g_bots[0].wp);
  anim_reset(&g_bots[0].anim, g_bots[0].pos, g_bots[0].yaw);
  g_players[0] = (player_t){.pos = hpos, .hp = PLAYER_HP,
                            .alive = 1, .grounded = 1, .eye = EYE_STAND};
}

// Deterministic proof that a bot degrades a lost target through belief states
// without firing at hidden knowledge, keeps the entity target past the old
// abrupt memory window, and returns to visible on reacquisition.
static void botmemory_proof(void) {
  proof_world_t w;
  proof_world_save(&w);

  g_bots_frozen = 0;
  g_predicting = 1;
  g_tick = 100;
  botmem_stage((bot_t){.target = {{0,0,-8}}, .tgt = -1}, (v3){{0,0,-8}});

  g_tick++;
  bot_tick(0);
  int acquired = g_bots[0].tgt == 0 &&
                 g_bots[0].belief == BOT_BELIEF_VISIBLE &&
                 g_bots[0].lose_t == 0;

  g_num_solids = 1;
  g_solids[0] = (solid_t){.min = {{-2,0,-5}}, .max = {{2,3,-4}}, .pen = 0};
  // hidden_fire once meant "fired at a remembered position at all" — the
  // PRE-FIRE tactic makes exactly that a designed behaviour. The invariant
  // that survives is DISCIPLINE: a hidden shot may leave only inside an armed
  // pre-fire window (b->prefiring), never as free blind fire. Expected 0.
  int hidden_fire = 0;
  int hidden_retained = 1;
  int old_shots = g_match.shots[1];

  g_tick++;
  bot_tick(0);
  int after_loss = g_bots[0].tgt == 0 &&
                   g_bots[0].belief == BOT_BELIEF_OCCLUDED_HOLD &&
                   g_bots[0].lose_t > 0;
  v3 loss_pos = g_bots[0].pos;
  // Keep the LIVE target sealed away after the loss. Search assertions below
  // must be about the remembered point, not an accidental flank on this fixture.
  g_players[0].pos = (v3){{12, 0, -8}};
  g_solids[1] = (solid_t){.min = {{10,0,-10}}, .max = {{14,3,-6}}, .pen = 0};
  g_num_solids = 2;
  if (g_match.shots[1] != old_shots && !g_bots[0].prefiring) hidden_fire = 1;
  old_shots = g_match.shots[1];
  g_match.nbots = 2;
  g_bots[1] = (bot_t){.pos = {{5,0,-8}}, .hp = PLAYER_HP, .alive = 1,
                       .active = 1, .grounded = 1, .eye = EYE_STAND};
  int visible_decoy_held = 1;

  for (int i = 1; i < 60; i++) {
    g_tick++;
    bot_tick(0);
    if (g_bots[0].tgt != 0) visible_decoy_held = 0;
    if (g_bots[0].tgt != 0 || g_bots[0].lose_t <= 0) hidden_retained = 0;
    if (g_match.shots[1] != old_shots && !g_bots[0].prefiring) hidden_fire = 1;
    old_shots = g_match.shots[1];
  }
  int after_60 = g_bots[0].tgt == 0 &&
                 (g_bots[0].belief == BOT_BELIEF_INVESTIGATE ||
                  g_bots[0].belief == BOT_BELIEF_SEARCH_SWEEP) &&
                 g_bots[0].lose_t >= 60;
  int search_hold_still = v3_len(v3_sub(g_bots[0].pos, loss_pos)) < 0.20f;
  int fixed_search_anchor =
      v3_len(v3_sub(g_bots[0].search_pos, g_bots[0].last_seen)) > 0.50f &&
      v3_len(v3_sub(g_bots[0].search_pos, g_bots[0].belief_pos)) > 0.50f;

  for (int i = 60; i < 300; i++) {
    g_tick++;
    bot_tick(0);
    if (g_bots[0].tgt != 0 || g_bots[0].lose_t <= 0) hidden_retained = 0;
    if (g_match.shots[1] != old_shots && !g_bots[0].prefiring) hidden_fire = 1;
    old_shots = g_match.shots[1];
  }
  int after_300 = g_bots[0].tgt == 0 &&
                  (g_bots[0].belief == BOT_BELIEF_INVESTIGATE ||
                   g_bots[0].belief == BOT_BELIEF_SEARCH_SWEEP) &&
                  g_bots[0].lose_t >= 300;
  int lose_t = g_bots[0].lose_t;
  int reacquire0 = g_bots[0].reacquire_count;

  g_num_solids = 0;
  g_nladders = 0;
  // Reacquisition needs the target back in the LINE and back in the ARC, so the
  // fixture now states the second precondition instead of leaving it to
  // wherever the search walk happened to leave the bot facing. The arc itself
  // is proved on its own below (rearblind/frontseen); this case stays what it
  // always was — restored line of sight returns the belief to VISIBLE, once.
  g_bots[0].yaw = g_bots[0].prev_yaw =
      atan2f(g_players[0].pos.x - g_bots[0].pos.x,
             -(g_players[0].pos.z - g_bots[0].pos.z));
  g_tick++;
  bot_tick(0);
  int reacquired = g_bots[0].tgt == 0 &&
                   g_bots[0].belief == BOT_BELIEF_VISIBLE &&
                   g_bots[0].lose_t == 0 &&
                   g_bots[0].reacquire_count == reacquire0 + 1;
  int search_moved = v3_len(v3_sub(loss_pos, g_bots[0].pos)) > 0.50f;

  // THE RETENTION ARC. A target with a clear line but standing directly BEHIND
  // the bot is not visible — the acquisition cone used to gate only the first
  // sighting, so an incumbent was tracked through the back of the head, which
  // is the oldest "the AI is cheating" tell there is. The same fixture at the
  // FRONT is the control: nothing about ordinary tracking may change.
  botmem_stage((bot_t){.yaw = 0.0f, .tgt = 0, .belief = BOT_BELIEF_VISIBLE,
                       .scan_t = 100, .react_t = 100, .burst = 3},
               (v3){{0,0,8}});                                  // BEHIND (+z)
  g_tick++;
  bot_tick(0);
  int rear_blind = g_bots[0].belief != BOT_BELIEF_VISIBLE && g_match.shots[1] == 0;
  g_bots[0].belief = BOT_BELIEF_VISIBLE;
  g_bots[0].lose_t = 0;
  g_bots[0].scan_t = 100;
  g_players[0].pos = (v3){{0,0,-8}};                             // IN FRONT
  g_tick++;
  bot_tick(0);
  int front_seen = g_bots[0].belief == BOT_BELIEF_VISIBLE && g_bots[0].lose_t == 0;

  // AFTER THE FIGHT. Losing a target must not hand the bot straight back to
  // patrol — it walks over and CLEARS the last place it knew about first, and
  // only then stands down. Without this the bot dropped into a stroll, rifle
  // slung, the tick its memory expired: seconds after a firefight, out of its
  // own cover, as if nothing had happened.
  botmem_stage((bot_t){.tgt = 0, .belief = BOT_BELIEF_VISIBLE, .scan_t = 4000,
                       .react_t = 100, .burst = 3},
               (v3){{0,0,-12}});
  g_tick++;
  bot_tick(0);                       // sees him: last_seen is (0,0,-12)
  g_players[0].alive = 0;            // ...and the fight ends
  g_tick++;
  bot_tick(0);
  int alert_armed = g_bots[0].tgt < 0 && g_bots[0].alert_t > 0 &&
                    v3_len(v3_sub(g_bots[0].alert_p, (v3){{0,0,-12}})) < 0.5f;
  float alert_d0 = v3_len(v3_sub(g_bots[0].pos, g_bots[0].alert_p));
  for (int i = 0; i < 240; i++) { g_tick++; bot_tick(0); }
  // It is CLOSING on the spot, not wandering off on a patrol leg.
  int alert_closes =
      v3_len(v3_sub(g_bots[0].pos, g_bots[0].alert_p)) < alert_d0 - 1.0f;
  int alert_down = 0;
  for (int i = 0; i < 2400 && !alert_down; i++) {
    g_tick++;
    bot_tick(0);
    alert_down = g_bots[0].alert_t == 0;   // ...and it does stand down again
  }

  // A visible incumbent remains authoritative outside the acquisition cone.
  // Its hold bonus must participate in the same arbitration as a new frontal
  // candidate; a targetless bot must still reject the same side candidate.
  memset(&g_match, 0, sizeof g_match);
  memset(g_humans_on, 0, sizeof g_humans_on);
  g_match.nbots = 2;
  g_humans_on[0] = 1;
  g_num_solids = 0;
  g_nladders = 0;
  g_num_events = 0;
  g_players[0] = (player_t){.pos = {{5,0,0}}, .hp = PLAYER_HP,
                             .alive = 1, .grounded = 1, .eye = EYE_STAND};
  g_bots[0] = (bot_t){.pos = {{0,0,0}}, .prev_pos = {{0,0,0}},
                       .hp = PLAYER_HP, .alive = 1, .active = 1,
                       .grounded = 1, .eye = EYE_STAND, .yaw = 0.0f,
                       .tgt = 0, .belief = BOT_BELIEF_VISIBLE, .scan_t = 1,
                       .react_t = 100, .burst = 3, .wp.cur = WPN_AR};
  wpn_fill_mags(&g_bots[0].wp);
  anim_reset(&g_bots[0].anim, g_bots[0].pos, g_bots[0].yaw);
  g_bots[1] = (bot_t){.pos = {{0,0,-8}}, .prev_pos = {{0,0,-8}},
                       .hp = PLAYER_HP, .alive = 1, .active = 1,
                       .grounded = 1, .eye = EYE_STAND};
  g_tick++;
  bot_tick(0);
  int visible_incumbent_held = g_bots[0].tgt == 0;

  g_match.nbots = 1;
  g_bots[0].tgt = -1;
  g_bots[0].scan_t = 1;
  g_tick++;
  bot_tick(0);
  int acquisition_fov_held = g_bots[0].tgt == -1;

  // Replanning must consume the finite candidate set. The fixture makes all
  // six physical anchors valid and distinct; all six must be consumed before
  // the seventh replan wraps to one of them.
  g_num_solids = 0;
  g_nladders = 0;
  g_bots[0] = (bot_t){.pos = {{0,0,0}}, .last_seen = {{0,0,-8}},
                       .belief_pos = {{1,0,-7}},
                       .last_seen_vel = {{0,0,-1}}, .search_dir = 1};
  v3 search_anchor[7];
  for (int i = 0; i < 7; i++) {
    bot_search_plan_next(&g_bots[0], &BOT_SKILL[g_cfg.sp_diff]);
    search_anchor[i] = g_bots[0].search_pos;
  }
  int search_progress = 1;
  for (int i = 0; i < 6; i++)
    for (int j = i + 1; j < 6; j++)
      if (v3_len(v3_sub(search_anchor[i], search_anchor[j])) <= 0.05f)
        search_progress = 0;
  int search_wrapped = 0;
  for (int i = 0; i < 6; i++)
    if (v3_len(v3_sub(search_anchor[i], search_anchor[6])) <= 0.05f)
      search_wrapped = 1;

  // Seed stronger incumbent evidence, then deliver two weaker crossfire hits
  // in both orders. Neither order may downgrade or dislodge the incumbent.
  g_bots[1] = (bot_t){.pos = {{3,0,-8}}, .hp = PLAYER_HP, .alive = 1,
                       .active = 1, .grounded = 1, .eye = EYE_STAND};
  g_players[0] = (player_t){.pos = {{-3,0,-8}}, .hp = PLAYER_HP,
                             .alive = 1, .grounded = 1, .eye = EYE_STAND};
  g_match.nbots = 2;
  g_humans_on[0] = 1;
  bot_t threat_seed = (bot_t){.pos = {{0,0,0}}, .hp = PLAYER_HP,
                               .alive = 1, .active = 1, .grounded = 1,
                               .eye = EYE_STAND, .tgt = 2,
                               .belief = BOT_BELIEF_OCCLUDED_HOLD,
                               .threat_src = 2, .threat_t = BOT_THREAT_TICKS,
                               .threat_score = 48, .wp.cur = WPN_AR};
  g_bots[0] = threat_seed;
  ent_damage(1, 1, 2, 2, g_bots[1].pos, g_bots[0].pos);
  ent_damage(1, 2, 2, 0, g_players[0].pos, g_bots[0].pos);
  int crossfire_a = g_bots[0].tgt == 2 && g_bots[0].threat_src == 2 &&
                    g_bots[0].threat_score == 48 &&
                    g_bots[0].threat_t <= BOT_THREAT_TICKS;
  g_bots[0] = threat_seed;
  ent_damage(1, 2, 2, 0, g_players[0].pos, g_bots[0].pos);
  ent_damage(1, 1, 2, 2, g_bots[1].pos, g_bots[0].pos);
  int crossfire_b = g_bots[0].tgt == 2 && g_bots[0].threat_src == 2 &&
                    g_bots[0].threat_score == 48 &&
                    g_bots[0].threat_t <= BOT_THREAT_TICKS;
  int crossfire_stable = crossfire_a && crossfire_b;

  // Equal, same-tick evidence is independent of event order and resolves to
  // the lower entity ID from an empty evidence store.
  bot_t empty_threat = threat_seed;
  empty_threat.tgt = -1;
  empty_threat.threat_src = -1;
  empty_threat.threat_t = 0;
  empty_threat.threat_score = 0;
  g_bots[0] = empty_threat;
  ent_damage(1, 1, 2, 2, g_bots[1].pos, g_bots[0].pos);
  ent_damage(1, 1, 2, 0, g_players[0].pos, g_bots[0].pos);
  int equal_order_a = g_bots[0].tgt == 0 && g_bots[0].threat_src == 0;
  g_bots[0] = empty_threat;
  ent_damage(1, 1, 2, 0, g_players[0].pos, g_bots[0].pos);
  ent_damage(1, 1, 2, 2, g_bots[1].pos, g_bots[0].pos);
  int equal_order_b = g_bots[0].tgt == 0 && g_bots[0].threat_src == 0;
  int threat_equal_id = equal_order_a && equal_order_b;

  // Equal evidence from a later tick is newer and wins before entity ID.
  // Same-tick equal evidence remains entity-ID deterministic in the crossfire
  // check above.
  g_bots[0] = threat_seed;
  g_bots[0].tgt = 0;
  g_bots[0].threat_src = 0;
  g_bots[0].threat_t = BOT_THREAT_TICKS - 1;
  ent_damage(1, 3, 2, 2, g_bots[1].pos, g_bots[0].pos);
  int threat_recency = g_bots[0].tgt == 2 && g_bots[0].threat_src == 2 &&
                       g_bots[0].threat_score == 48 &&
                       g_bots[0].threat_t == BOT_THREAT_TICKS;

  proof_world_restore(&w);
  int nested_ev_peak = g_ev_peak;
  long nested_ev_drops = g_ev_drops;
  int nested_predicting = g_predicting;
  bothear_proof();
  int nested_proof_restored = g_ev_peak == nested_ev_peak &&
                              g_ev_drops == nested_ev_drops &&
                              g_predicting == nested_predicting;
  printf("botmemory acquired=%d loss=%d t60=%d t300=%d reacquired=%d "
         "rearblind=%d frontseen=%d alert=%d/%d/%d "
         "retained=%d hidden_fire=%d crossfire=%d holdstill=%d "
         "searchanchor=%d searchprogress=%d searchwrap=%d searchmove=%d "
         "decoyhold=%d incumbent=%d acquirefov=%d equalid=%d recency=%d "
         "proofrestore=%d lose_t=%d\n",
         acquired, after_loss, after_60, after_300, reacquired,
         rear_blind, front_seen, alert_armed, alert_closes, alert_down,
         hidden_retained, hidden_fire, crossfire_stable, search_hold_still,
         fixed_search_anchor, search_progress, search_wrapped, search_moved,
         visible_decoy_held, visible_incumbent_held, acquisition_fov_held,
         threat_equal_id, threat_recency, nested_proof_restored, lose_t);
  if (!rear_blind || !front_seen || !alert_armed || !alert_closes ||
      !alert_down ||
      !acquired || !after_loss || !after_60 || !after_300 || !reacquired ||
      !hidden_retained || hidden_fire || !crossfire_stable ||
      !search_hold_still || !fixed_search_anchor || !search_progress ||
      !search_wrapped ||
      !search_moved || !visible_decoy_held || !visible_incumbent_held ||
      !acquisition_fov_held || !threat_equal_id || !threat_recency ||
      !nested_proof_restored) exit(1);
}

static void botmemoryobserve_proof(void) {
  proof_world_t w;
  proof_world_save(&w);

  memset(&g_match, 0, sizeof g_match);
  memset(g_humans_on, 0, sizeof g_humans_on);
  g_match.nbots = 1;
  g_humans_on[0] = 1;
  g_tick = 100;
  g_bots[0] = (bot_t){.hp = PLAYER_HP, .alive = 1, .active = 1, .grounded = 1,
                       .eye = EYE_STAND, .tgt = 0};
  g_players[0] = (player_t){.hp = PLAYER_HP, .alive = 1, .grounded = 1,
                             .eye = EYE_STAND};

  float dx = g_cfg.mv[MV_RUN] * TICK_DT * 0.5f;
  g_players[0].pos = (v3){{0, 0, -8}};
  bot_belief_observe(&g_bots[0], 0);
  g_bots[0].last_seen = ent_pos(0);

  g_tick++;
  g_players[0].pos.x += dx;
  bot_belief_observe(&g_bots[0], 0);
  g_bots[0].last_seen = ent_pos(0);

  g_tick++;
  g_bots[0].belief = BOT_BELIEF_OCCLUDED_HOLD;
  bot_belief_predict(&g_bots[0]);

  g_tick++;
  g_players[0].pos.x += dx * 2.0f;
  bot_belief_observe(&g_bots[0], 0);

  float speed = sqrtf(g_bots[0].last_seen_vel.x * g_bots[0].last_seen_vel.x +
                      g_bots[0].last_seen_vel.z * g_bots[0].last_seen_vel.z);

  proof_world_restore(&w);
  printf("botmemoryobserve speed=%.3f\n", speed);
  if (speed > 0.001f) exit(1);
}

static uint32_t proof_hash_bytes(uint32_t h, const void *data, size_t n) {
  const unsigned char *p = (const unsigned char *)data;
  for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
  return h;
}

static void proof_hash_qpos(np_wr *w, v3 p) {
  np_w16(w, (uint16_t)net_q_pos(p.x));
  np_w16(w, (uint16_t)net_q_pos(p.y));
  np_w16(w, (uint16_t)net_q_pos(p.z));
}

static uint32_t netloop_report_hash(void) {
  unsigned char buf[4096];
  np_wr w = {buf, sizeof buf, 0, 0};

  np_w32(&w, (uint32_t)g_tick);
  np_w32(&w, g_map_seed);
  np_w8(&w, (uint8_t)g_match.nbots);
  np_w8(&w, (uint8_t)g_match.over);
  np_w8(&w, (uint8_t)g_match.winner);
  np_w8(&w, (uint8_t)g_match.restart_req);

  ENT_FOREACH(e) {
    net_ent_t ne;
    net_ent_from_live(&ne, e);
    net_ent_encode(&w, &ne);
    np_w16(&w, (uint16_t)g_match.kills[e]);
    np_w16(&w, (uint16_t)g_match.deaths[e]);
    np_w16(&w, (uint16_t)g_match.shots[e]);
    np_w16(&w, (uint16_t)g_match.hits[e]);
  }

  for (int i = 0; i < g_match.nbots; i++) {
    const bot_t *b = &g_bots[i];
    np_w8(&w, (uint8_t)b->active);
    np_w8(&w, (uint8_t)b->alive);
    np_w8(&w, (uint8_t)b->belief);
    np_w8(&w, (uint8_t)(b->tgt + 1));
    np_w16(&w, (uint16_t)b->lose_t);
    np_w16(&w, (uint16_t)b->search_index);
    np_w16(&w, (uint16_t)b->search_t);
    np_w16(&w, (uint16_t)b->reacquire_count);
    proof_hash_qpos(&w, b->belief_pos);
    proof_hash_qpos(&w, b->search_pos);
    proof_hash_qpos(&w, b->last_seen);
  }

  return w.bad ? 0 : proof_hash_bytes(2166136261u, buf, w.at);
}

static int padbackend_state_equal(const pad_state_t *a, const pad_state_t *b) {
  if (a->connected != b->connected || a->lx != b->lx || a->ly != b->ly ||
      a->rx != b->rx || a->ry != b->ry || a->lt != b->lt || a->rt != b->rt)
    return 0;
  for (int i = 0; i < PB_COUNT; i++)
    if (a->btn[i] != b->btn[i]) return 0;
  return 1;
}

static int padbackend_state_neutral(const pad_state_t *p) {
  return padbackend_state_equal(p, &(pad_state_t){0});
}

static void padbackend_expect(int ok, const char *name, int *cases) {
  (*cases)++;
  if (!ok) {
    fprintf(stderr, "padbackend FAIL %s\n", name);
    exit(1);
  }
}

static void padbackend_expect_standard_button(pad_state_t *out, uint32_t bit,
                                              pad_btn_t expected,
                                              const char *name, int *cases) {
  pad_apply_standard(out, &(pad_standard_state_t){
    .connected = 1, .buttons = bit,
  });
  int one_hot = out->connected;
  for (int b = 0; b < PB_COUNT; b++)
    if (out->btn[b] != (b == (int)expected)) one_hot = 0;
  padbackend_expect(one_hot, name, cases);
}

static void padbackend_expect_xinput_button(pad_state_t *out, uint16_t bit,
                                            pad_btn_t expected,
                                            const char *name, int *cases) {
  pad_apply_xinput(out, &(pad_xinput_raw_t){.buttons = bit});
  int one_hot = out->connected;
  for (int b = 0; b < PB_COUNT; b++)
    if (out->btn[b] != (b == (int)expected)) one_hot = 0;
  padbackend_expect(one_hot, name, cases);
}

// Native Windows adapters are intentionally absent from this Linux harness,
// so this exercises the portable mapping contract through exactly the
// interfaces they will use.
// One-hot sweep for the HID layout tables: press ONE raw bit, demand exactly
// the expected logical button (or none — paddles, home) and exactly the
// expected trigger levels. The first HID battery asserted only partial
// set/clear lists, and four deliberately swapped vendor tables passed it.
static void padbackend_expect_hid_button(const pad_hid_raw_t *tpl, int bit,
                                         int expected, float elt, float ert,
                                         const char *name, int *cases) {
  pad_hid_raw_t r = *tpl;
  r.buttons = 1u << bit;
  pad_state_t out;
  pad_apply_hid(&out, &r);
  int ok = out.connected && out.lt == elt && out.rt == ert;
  for (int b = 0; b < PB_COUNT; b++)
    if (out.btn[b] != (b == expected)) ok = 0;
  padbackend_expect(ok, name, cases);
}
static void padbackend_proof(void) {
  int cases = 0;
  pad_state_t out = {0};
  pad_standard_state_t sample = {
    .connected = 1, .lx = 0.5f, .ly = -0.25f, .rx = -0.75f, .ry = 1.0f,
    .buttons = 0x0004u,
  };

  pad_apply_standard(&out, &sample);
  padbackend_expect(out.connected && out.lx == 0.5f && out.ly == -0.25f &&
                    out.rx == -0.75f && out.ry == 1.0f && out.btn[PB_A],
                    "normal-a-stick", &cases);

  pad_apply_standard(&out, &(pad_standard_state_t){0});
  padbackend_expect(padbackend_state_neutral(&out), "zero", &cases);

  pad_apply_standard(&out, &(pad_standard_state_t){
    .connected = 1, .lx = -2.0f, .ly = 2.0f, .rx = -1.0f, .ry = 1.0f,
  });
  padbackend_expect(out.lx == -1.0f && out.ly == 1.0f &&
                    out.rx == -1.0f && out.ry == 1.0f, "stick-bounds", &cases);

  pad_apply_standard(&out, &(pad_standard_state_t){
    .connected = 1, .lt = -0.5f, .rt = 1.5f,
  });
  padbackend_expect(out.lt == 0.0f && out.rt == 1.0f, "trigger-bounds", &cases);

  padbackend_expect_standard_button(&out, 0x0001u, PB_START,  "menu-start",  &cases);
  padbackend_expect_standard_button(&out, 0x0002u, PB_SELECT, "view-select", &cases);
  padbackend_expect_standard_button(&out, 0x0004u, PB_A,      "a",           &cases);
  padbackend_expect_standard_button(&out, 0x0008u, PB_B,      "b",           &cases);
  padbackend_expect_standard_button(&out, 0x0010u, PB_X,      "x",           &cases);
  padbackend_expect_standard_button(&out, 0x0020u, PB_Y,      "y",           &cases);
  padbackend_expect_standard_button(&out, 0x0040u, PB_UP,     "dpad-up",     &cases);
  padbackend_expect_standard_button(&out, 0x0080u, PB_DOWN,   "dpad-down",   &cases);
  padbackend_expect_standard_button(&out, 0x0100u, PB_LEFT,   "dpad-left",   &cases);
  padbackend_expect_standard_button(&out, 0x0200u, PB_RIGHT,  "dpad-right",  &cases);
  padbackend_expect_standard_button(&out, 0x0400u, PB_LB,     "lb",          &cases);
  padbackend_expect_standard_button(&out, 0x0800u, PB_RB,     "rb",          &cases);
  padbackend_expect_standard_button(&out, 0x1000u, PB_L3,     "l3",          &cases);
  padbackend_expect_standard_button(&out, 0x2000u, PB_R3,     "r3",          &cases);

  pad_apply_xinput(&out, &(pad_xinput_raw_t){
    .lx = -32768, .ly = 0, .rx = 32767, .ry = -16384,
    .lt = 0, .rt = 255,
  });
  padbackend_expect(out.lx == -1.0f && out.ly == 0.0f && out.rx == 1.0f &&
                    out.ry < -0.49f && out.ry > -0.51f && out.lt == 0.0f &&
                    out.rt == 1.0f, "xinput-ranges", &cases);
  padbackend_expect_xinput_button(&out, 0x0001u, PB_UP, "xi-dpad-up", &cases);
  padbackend_expect_xinput_button(&out, 0x0002u, PB_DOWN, "xi-dpad-down", &cases);
  padbackend_expect_xinput_button(&out, 0x0004u, PB_LEFT, "xi-dpad-left", &cases);
  padbackend_expect_xinput_button(&out, 0x0008u, PB_RIGHT, "xi-dpad-right", &cases);
  padbackend_expect_xinput_button(&out, 0x0010u, PB_START, "xi-start", &cases);
  padbackend_expect_xinput_button(&out, 0x0020u, PB_SELECT, "xi-select", &cases);
  padbackend_expect_xinput_button(&out, 0x0040u, PB_L3, "xi-l3", &cases);
  padbackend_expect_xinput_button(&out, 0x0080u, PB_R3, "xi-r3", &cases);
  padbackend_expect_xinput_button(&out, 0x0100u, PB_LB, "xi-lb", &cases);
  padbackend_expect_xinput_button(&out, 0x0200u, PB_RB, "xi-rb", &cases);
  padbackend_expect_xinput_button(&out, 0x1000u, PB_A, "xi-a", &cases);
  padbackend_expect_xinput_button(&out, 0x2000u, PB_B, "xi-b", &cases);
  padbackend_expect_xinput_button(&out, 0x4000u, PB_X, "xi-x", &cases);
  padbackend_expect_xinput_button(&out, 0x8000u, PB_Y, "xi-y", &cases);

  // --- pad_apply_hid: the DirectInput layout mapper, per confirmed device
  // shape. The 8BitDo rows ARE the Ultimate 2 over Bluetooth (VID 2DC8,
  // D-input mode): sticks X/Y + Z/Rz, POV dpad, DIGITAL triggers on bits
  // 8/9, Android button numbering with paddle gaps at 2/5.
  const uint32_t AX_XYZRZ = 1u << PH_X | 1u << PH_Y | 1u << PH_Z | 1u << PH_RZ;
  const uint32_t AX_SIX = AX_XYZRZ | 1u << PH_RX | 1u << PH_RY;
  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .vid = 0x2DC8, .axes = AX_XYZRZ, .pov = -1,
    .ax = {[PH_X] = 0.5f, [PH_Y] = 0.5f, [PH_Z] = -0.25f, [PH_RZ] = 1.0f},
  });
  padbackend_expect(out.connected && out.lx == 0.5f && out.ly == -0.5f &&
                    out.rx == -0.25f && out.ry == -1.0f &&
                    out.lt == 0.0f && out.rt == 0.0f,
                    "hid-8bitdo-bt-sticks", &cases);

  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .vid = 0x2DC8, .axes = AX_XYZRZ, .pov = -1,
    .buttons = 1u << 10 | 1u << 11 | 1u << 3 | 1u << 13 | 1u << 2,
  });
  padbackend_expect(out.btn[PB_SELECT] && out.btn[PB_START] &&
                    out.btn[PB_X] && out.btn[PB_L3] && out.btn[PB_P3] &&
                    !out.btn[PB_A] && !out.btn[PB_B] &&
                    !out.btn[PB_Y] && !out.btn[PB_R3],
                    "hid-8bitdo-bt-buttons", &cases);

  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .vid = 0x2DC8, .axes = AX_XYZRZ, .pov = -1,
    .buttons = 1u << 8 | 1u << 9,
  });
  padbackend_expect(out.lt == 1.0f && out.rt == 1.0f,
                    "hid-8bitdo-bt-trig", &cases);

  // One-hot over every vendor table, every mapped AND every gap bit. The
  // Sony/generic templates rest their analog trigger axes at -1 so a raw
  // zero does not read as a half-pulled trigger.
  const pad_hid_raw_t TPL_8BITDO = {.connected = 1, .vid = 0x2DC8,
                                    .axes = AX_XYZRZ, .pov = -1};
  const pad_hid_raw_t TPL_SONY = {.connected = 1, .vid = 0x054C,
                                  .axes = AX_SIX, .pov = -1,
                                  .ax = {[PH_RX] = -1.0f, [PH_RY] = -1.0f}};
  const pad_hid_raw_t TPL_SWITCH = {.connected = 1, .vid = 0x057E, .pov = -1,
    .axes = 1u << PH_X | 1u << PH_Y | 1u << PH_RX | 1u << PH_RY};
  const pad_hid_raw_t TPL_XBOX = {.connected = 1, .axes = AX_XYZRZ, .pov = -1};
  struct { const pad_hid_raw_t *tpl; const char *name;
           struct { int8_t bit, pb; float lt, rt; } row[20]; } sweeps[] = {
    {&TPL_8BITDO, "hid-onehot-8bitdo", {
      {0, PB_A, 0, 0},  {1, PB_B, 0, 0},  {2, PB_P3, 0, 0}, {3, PB_X, 0, 0},
      {4, PB_Y, 0, 0},  {5, PB_P4, 0, 0}, {6, PB_LB, 0, 0}, {7, PB_RB, 0, 0},
      {8, -1, 1, 0},    {9, -1, 0, 1},    {10, PB_SELECT, 0, 0},
      {11, PB_START, 0, 0}, {12, -1, 0, 0}, {13, PB_L3, 0, 0},
      {14, PB_R3, 0, 0}, {15, -1, 0, 0}, {16, PB_P2, 0, 0},
      {17, PB_P1, 0, 0}, {-1, 0, 0, 0}}},
    {&TPL_SONY, "hid-onehot-sony", {
      {0, PB_X, 0, 0},  {1, PB_A, 0, 0},  {2, PB_B, 0, 0},  {3, PB_Y, 0, 0},
      {4, PB_LB, 0, 0}, {5, PB_RB, 0, 0}, {6, -1, 1, 0},    {7, -1, 0, 1},
      {8, PB_SELECT, 0, 0}, {9, PB_START, 0, 0}, {10, PB_L3, 0, 0},
      {11, PB_R3, 0, 0}, {12, -1, 0, 0}, {-1, 0, 0, 0}}},
    {&TPL_SWITCH, "hid-onehot-switch", {
      {0, PB_A, 0, 0},  {1, PB_B, 0, 0},  {2, PB_X, 0, 0},  {3, PB_Y, 0, 0},
      {4, PB_LB, 0, 0}, {5, PB_RB, 0, 0}, {6, -1, 1, 0},    {7, -1, 0, 1},
      {8, PB_SELECT, 0, 0}, {9, PB_START, 0, 0}, {10, PB_L3, 0, 0},
      {11, PB_R3, 0, 0}, {12, -1, 0, 0}, {13, -1, 0, 0}, {-1, 0, 0, 0}}},
    {&TPL_XBOX, "hid-onehot-generic", {
      {0, PB_A, 0, 0},  {1, PB_B, 0, 0},  {2, PB_X, 0, 0},  {3, PB_Y, 0, 0},
      {4, PB_LB, 0, 0}, {5, PB_RB, 0, 0}, {6, PB_SELECT, 0, 0},
      {7, PB_START, 0, 0}, {8, PB_L3, 0, 0}, {9, PB_R3, 0, 0},
      {10, -1, 0, 0}, {-1, 0, 0, 0}}},
  };
  for (size_t s = 0; s < sizeof sweeps / sizeof sweeps[0]; s++)
    for (int i = 0; i < 20 && sweeps[s].row[i].bit >= 0; i++)
      padbackend_expect_hid_button(sweeps[s].tpl, sweeps[s].row[i].bit,
                                   sweeps[s].row[i].pb, sweeps[s].row[i].lt,
                                   sweeps[s].row[i].rt, sweeps[s].name, &cases);

  // Same vendor over the 2.4G dongle in D-input mode: all six axes exist and
  // the right stick moves to Rx/Ry; Z/Rz must then be IGNORED, not triggers.
  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .vid = 0x2DC8, .axes = AX_SIX, .pov = -1,
    .ax = {[PH_RX] = 0.5f, [PH_RY] = -1.0f, [PH_Z] = 0.9f, [PH_RZ] = 0.9f},
  });
  padbackend_expect(out.rx == 0.5f && out.ry == 1.0f &&
                    out.lt == 0.0f && out.rt == 0.0f,
                    "hid-8bitdo-dongle", &cases);

  // DualShock shape: right stick Z/Rz, ANALOG triggers Rx/Ry resting at -1.
  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .vid = 0x054C, .axes = AX_SIX, .pov = -1,
    .ax = {[PH_Z] = 0.5f, [PH_RZ] = -0.5f, [PH_RX] = -1.0f, [PH_RY] = 0.0f},
    .buttons = 1u << 1 | 1u << 0 | 1u << 8,
  });
  padbackend_expect(out.rx == 0.5f && out.ry == 0.5f &&
                    out.lt == 0.0f && out.rt == 0.5f &&
                    out.btn[PB_A] && out.btn[PB_X] && out.btn[PB_SELECT] &&
                    !out.btn[PB_B], "hid-sony", &cases);

  // Xbox-through-DInput shape: right stick Rx/Ry, both triggers folded onto
  // one Z axis (LT +, RT -).
  const uint32_t AX_X360 = 1u << PH_X | 1u << PH_Y | 1u << PH_Z |
                           1u << PH_RX | 1u << PH_RY;
  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .axes = AX_X360, .pov = -1,
    .ax = {[PH_RX] = 0.25f, [PH_RY] = 0.75f, [PH_Z] = 1.0f},
  });
  padbackend_expect(out.rx == 0.25f && out.ry == -0.75f &&
                    out.lt == 1.0f && out.rt == 0.0f, "hid-x360-lt", &cases);
  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .axes = AX_X360, .pov = -1, .ax = {[PH_Z] = -1.0f},
  });
  padbackend_expect(out.lt == 0.0f && out.rt == 1.0f, "hid-x360-rt", &cases);

  // Switch Pro shape: sticks on X/Y + Rx/Ry, everything else digital.
  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .vid = 0x057E, .pov = -1,
    .axes = 1u << PH_X | 1u << PH_Y | 1u << PH_RX | 1u << PH_RY,
    .ax = {[PH_RX] = 1.0f}, .buttons = 1u << 6 | 1u << 8 | 1u << 9,
  });
  padbackend_expect(out.rx == 1.0f && out.lt == 1.0f && out.rt == 0.0f &&
                    out.btn[PB_SELECT] && out.btn[PB_START], "hid-switch", &cases);

  // Slider triggers (rest -1), and the generic Xbox-order button guess.
  pad_apply_hid(&out, &(pad_hid_raw_t){
    .connected = 1, .axes = AX_XYZRZ | 1u << PH_S0 | 1u << PH_S1, .pov = -1,
    .ax = {[PH_S0] = 1.0f, [PH_S1] = -1.0f}, .buttons = 1u << 6 | 1u << 7,
  });
  padbackend_expect(out.lt == 1.0f && out.rt == 0.0f &&
                    out.btn[PB_SELECT] && out.btn[PB_START],
                    "hid-generic-sliders", &cases);

  // POV decoding: hundredths of a degree CW from north, -1 centered, and the
  // 8-sector rounding that makes 35999 read as "up" again.
  struct { int pov; int up, dn, lf, rt_; } povs[] = {
    {-1, 0, 0, 0, 0},    {0, 1, 0, 0, 0},     {4500, 1, 0, 0, 1},
    {9000, 0, 0, 0, 1},  {18000, 0, 1, 0, 0}, {27000, 0, 0, 1, 0},
    {31500, 1, 0, 1, 0}, {35999, 1, 0, 0, 0},
  };
  for (size_t i = 0; i < sizeof povs / sizeof povs[0]; i++) {
    pad_apply_hid(&out, &(pad_hid_raw_t){
      .connected = 1, .axes = AX_XYZRZ, .pov = povs[i].pov,
    });
    padbackend_expect(out.btn[PB_UP] == povs[i].up &&
                      out.btn[PB_DOWN] == povs[i].dn &&
                      out.btn[PB_LEFT] == povs[i].lf &&
                      out.btn[PB_RIGHT] == povs[i].rt_, "hid-pov", &cases);
  }

  // A pump that reports nothing must fully clear a previously live state —
  // the mapper is all-or-nothing, never a field merge.
  pad_apply_hid(&out, &(pad_hid_raw_t){0});
  padbackend_expect(padbackend_state_neutral(&out), "hid-disconnect", &cases);

  printf("padbackend ok cases=%d\n", cases);
}

// CAPTURE BOT 0's FIGURE INTO THE PROBE BUFFER at full profile resolution.
// figm and figbury both want exactly this, and a tier change must not be able
// to make the two proofs measure different figures — g_cam_eye ON the body is
// what forces the finest tier (see the g_cam_cull rule).
static const bot_t *probe_capture_figure(int wpn) {
  g_nprobe = 0;
  g_probe_on = 1;
  scene_reset();
  const bot_t *b = &g_bots[0];
  g_cam_eye = b->pos;
  scene_draw_figure(&b->anim, b->pos, b->yaw, NULL, 1.0f, 0.0f,
                    (v3){{1, 1, 1}}, 0, wpn, gun_anim(&b->wp, 1.0f));
  g_probe_on = 0;
  return b;
}

// Every figure part's measured extent in mm, the bone lengths, the elbow angles
// with the reach each one solves, and the cheek-weld rows.
static void figm_proof(void) {
  // Measure the figure the player actually sees, part by part, in the
  // figure's own frame (x right, y up from the ground, z forward), in mm.
  // Widths were authored against a front view and came out right; lengths
  // and depths were guessed and did not. This is the instrument that turns
  // "looks amateurish" into a signed delta against ANSUR II.
  const bot_t *b = probe_capture_figure(WPN_AR);
  float cy = cosf(b->yaw), sy = sinf(b->yaw);
  float glo[3] = {1e9f, 1e9f, 1e9f}, ghi[3] = {-1e9f, -1e9f, -1e9f};
  // THE ELBOWS, as angles. "The arm looks awkward" is not actionable and a
  // screenshot cannot answer it: what decides whether a shouldered stance
  // reads is whether the firing arm makes a TRIANGLE, and a triangle is an
  // angle. Printed with the reach it is solving, because a nearly-straight
  // arm is almost never a pole-vector problem — it is a wrist target the
  // shoulder cannot fold to reach.
  {
    const pose_t *EP = &b->anim.pose;
    float al = ANIM_UARM + ANIM_FARM;
    // ...and how far the PALM has to stretch to bridge whatever the IK
    // could not reach. fig_hand wraps the weapon's published cylinder while
    // the wrist stays where the two-bone solve put it, so the gap between
    // them is drawn as a longer hand — the number that decides whether a
    // grip reads as a grip or as a glove pointing at a rifle.
    {
      v3 hc, ha, org2, rt2, up2, fw2; float hr;
      pose_gun_frame(EP, WPN_AR, &org2, &rt2, &up2, &fw2);
      float gsave = g_gun_girth;
      g_gun_girth = GUN_GIRTH_3P;
      gun_hold(WPN_AR, GH_FORE3, org2, rt2, up2, fw2, &hc, &ha, &hr);
      g_gun_girth = gsave;
      v3 d = v3_sub(EP->wri[0], hc);
      float along = v3_dot(d, ha);
      v3 rad = v3_sub(d, v3_scale(ha, along));
      printf("figm palm  support_gap=%.0fmm (along=%.0f radial=%.0f)\n",
             (double)(v3_len(d) * 1000.0f), (double)(along * 1000.0f),
             (double)((v3_len(rad) - hr) * 1000.0f));
    }
    // THE WELD, and it is the one thing about an aimed pose a screenshot
    // argues about and cannot settle: is the EYE on the sight line, or is
    // the head merely near the rifle? The sight line is the bore raised by
    // the weapon's own sight height (scaled, because the third-person model
    // draws at GUN_GIRTH_3P), and the eye is measured off it in the
    // WEAPON'S frame — up/side, plus the eye relief along the bore. Behind
    // an optic a shooter's eye sits 70-90 mm back and dead on the axis; a
    // hundred millimetres off to the side is a man looking PAST his rifle.
    for (int wi = 0; wi < WPN_COUNT; wi++) {
      v3 e, hr_, hu_, hf_;
      pose_head_frame(EP, anim_head_cant(&b->anim), &e, &hr_, &hu_, &hf_);
      v3 org3, rt3, up3, fw3;
      pose_gun_frame(EP, wi, &org3, &rt3, &up3, &fw3);
      // The APERTURE, not the anchor's cross-section: relief measured from
      // the origin plane says nothing about whether the eye is behind the
      // glass, and the sniper's ocular sits 52 mm BEHIND that plane while
      // the carbine's aperture sits 184 mm in front of it.
      v3 sp3 = v3_add(org3,
                 v3_add(v3_scale(up3, gun_sight_y(wi) * GUN_GIRTH_3P),
                        v3_scale(fw3, gun_sight_z(wi) * GUN_GIRTH_3P)));
      v3 d3 = v3_sub(e, sp3);
      float along = v3_dot(d3, fw3);
      v3 lat = v3_sub(d3, v3_scale(fw3, along));
      // ...and WHERE ALONG THE WEAPON the eye sits, in the model's own
      // units, which is the number that says whether the head is over the
      // comb or parked behind the butt pad. The AR's pad is at -0.243 and
      // the sniper's at -0.201; a shouldered head puts the eye 100-150 mm
      // in front of its pad, i.e. eye_z around -0.12 and -0.08.
      printf("figm weld %-3s eye_z=%+.3f cant=%.1fdeg eye_up=%.0fmm "
             "eye_side=%.0fmm relief=%.0fmm off_axis=%.0fmm\n",
             WPN_DEF[wi].prefix,
             (double)(v3_dot(v3_sub(e, org3), fw3) / GUN_GIRTH_3P),
             (double)(anim_head_cant(&b->anim) * (180.0f / F_PI)),
             (double)(v3_dot(lat, up3) * 1000.0f),
             (double)(v3_dot(lat, rt3) * 1000.0f),
             (double)(-along * 1000.0f), (double)(v3_len(lat) * 1000.0f));
    }
    printf("figm elbow");
    for (int i = 0; i < 2; i++) {
      v3 u = v3_sub(EP->sho[i], EP->elb[i]), f = v3_sub(EP->wri[i], EP->elb[i]);
      float lu = v3_len(u), lf = v3_len(f);
      float ang = (lu > 1e-5f && lf > 1e-5f)
                ? acosf(f_clamp(v3_dot(u, f) / (lu * lf), -1.0f, 1.0f)) * (180.0f / F_PI)
                : 180.0f;
      float reach = v3_len(v3_sub(EP->wri[i], EP->sho[i]));
      printf("  %s=%.0fdeg reach=%.0f/%.0fmm", i ? "R" : "L",
             (double)ang, (double)(reach * 1000.0f), (double)(al * 1000.0f));
    }
    printf("\n");
  }
  printf("figm part            w_mm   d_mm   h_mm    y0     y1\n");
  for (int i = 0; i < g_nprobe; i++) {
    if (!g_probe[i].name) continue;
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    int seen = 0;
    // Merge every range sharing this name, so a part drawn once per side
    // reports its true SPAN (both deltoids together = bideltoid breadth).
    for (int j = i; j < g_nprobe; j++) {
      if (!g_probe[j].name || strcmp(g_probe[j].name, g_probe[i].name)) continue;
      for (int f = g_probe[j].v0; f < g_probe[j].v1; f += VERT_FLOATS) {
        float dx = g_scene[f] - b->pos.x, dz = g_scene[f + 2] - b->pos.z;
        float l[3] = {dx * cy + dz * sy, g_scene[f + 1] - b->pos.y,
                      dx * sy - dz * cy};
        for (int k = 0; k < 3; k++) {
          if (l[k] < lo[k]) lo[k] = l[k];
          if (l[k] > hi[k]) hi[k] = l[k];
          if (l[k] < glo[k]) glo[k] = l[k];
          if (l[k] > ghi[k]) ghi[k] = l[k];
        }
        seen = 1;
      }
      if (j > i) g_probe[j].name = NULL;   // consumed into this row
    }
    if (!seen) continue;
    printf("figm %-14s %6.1f %6.1f %6.1f %6.1f %6.1f\n", g_probe[i].name,
           (double)((hi[0] - lo[0]) * 1000.0f), (double)((hi[2] - lo[2]) * 1000.0f),
           (double)((hi[1] - lo[1]) * 1000.0f), (double)(lo[1] * 1000.0f),
           (double)(hi[1] * 1000.0f));
  }
  printf("figm TOTAL          %6.1f %6.1f %6.1f %6.1f %6.1f\n",
         (double)((ghi[0] - glo[0]) * 1000.0f), (double)((ghi[2] - glo[2]) * 1000.0f),
         (double)((ghi[1] - glo[1]) * 1000.0f), (double)(glo[1] * 1000.0f),
         (double)(ghi[1] * 1000.0f));
  // Skeleton lengths, which no vertex bbox can report.
  const pose_t *Q = &b->anim.pose;
  printf("figm bones uarm=%.1f farm=%.1f thigh=%.1f shin=%.1f "
         "sho_span=%.1f hip_span=%.1f hip_y=%.1f head_y=%.1f\n",
         (double)(v3_len(v3_sub(Q->elb[1], Q->sho[1])) * 1000.0f),
         (double)(v3_len(v3_sub(Q->wri[1], Q->elb[1])) * 1000.0f),
         (double)(v3_len(v3_sub(Q->knee[1], Q->hip[1])) * 1000.0f),
         (double)(v3_len(v3_sub(Q->ankle[1], Q->knee[1])) * 1000.0f),
         (double)(v3_len(v3_sub(Q->sho[1], Q->sho[0])) * 1000.0f),
         (double)(v3_len(v3_sub(Q->hip[1], Q->hip[0])) * 1000.0f),
         (double)((Q->pelvis.y - b->pos.y) * 1000.0f),
         (double)((Q->head.y - b->pos.y) * 1000.0f));
}

// The third-person weapon (or the SKULL) against the BODY, by ray parity over
// one closed part at a time — a union is not a solid.
static void figbury_proof(void) {
  // HOW MUCH OF THE THIRD-PERSON WEAPON IS INSIDE THE BODY. The carry pose
  // is the pose a player sees for most of a match — his own back in third
  // person, and every enemy who is not currently shooting at him — and the
  // one way it can be wrong that no existing proof can see is the weapon
  // sinking into the trunk. figcheck cannot: it tests the mesh against
  // ITSELF for manifoldness and coplanarity, and two closed meshes that
  // pass through each other are a perfectly valid mesh. vmtrig cannot: it
  // tests the hand against the weapon, which is the opposite pair.
  //
  // Ray parity over the BODY, exactly as vmtrig does it over the weapon,
  // and deliberately over the TRUNK only — a forearm crossing a rifle is a
  // grip, not a defect, and the hands are supposed to be on it.
  // `butt` is the number the eye actually reads: the rearmost point of the
  // model, i.e. the butt pad. A stock resting against the ribs is a few mm
  // in; a stock that has vanished is 100+.
  static const char *const BODYP[] = {"torso", "carrier", "placard", "pack",
                                      "packstrap", "trap", "strap", "magpouch",
                                      "thigh", "deltoid", "pauldron",
                                      "sho_ball", "dropleg", "neck"};
  // ...and the SAME question about the HEAD, because it is the same defect
  // class and no other proof can see it either: crouching, the head tucks
  // onto the shoulders on purpose, and three separate drops (the crouch
  // tuck, the ADS cheek weld and the spine's hunch) stack into it. Whether
  // the sum leaves a neck or buries the skull in its own collar is a
  // millimetre question. `neck` is NOT in the body set here — a skull
  // overlapping the neck tube it sits on is the join, not a defect — and
  // `hd_mm` reports the LOWEST subject vertex (the jaw) rather than a
  // rearmost-along-bore that means nothing for a head.
  static const char *const HEADP[] = {"skull", "helm_shell", "helm_ear",
                                      "goggle", "ear", "chinstrap", "nvg"};
  static const char *const HBODY[] = {"torso", "carrier", "placard", "pack",
                                      "packstrap", "trap", "strap", "deltoid",
                                      "pauldron", "sho_ball"};
  int hmode = 0;
  { char *sub = take_tok();
    if (sub && !strcmp(sub, "head")) hmode = 1;
    else if (sub) g_tok_pushback = sub;   // peeked, not consumed, as figcheck's DIST is
  }
  const char *const *BSET = hmode ? HBODY : BODYP;
  unsigned BSETN = hmode ? sizeof HBODY / sizeof *HBODY
                         : sizeof BODYP / sizeof *BODYP;
  printf(hmode ? "figbury sub  deepest_mm  hidden%%   jaw_mm  verts   "
                 "jaw_at(x,y,z mm, figure frame)\n"
               : "figbury wpn  deepest_mm  hidden%%  butt_mm  verts   "
                 "butt_at(x,y,z mm, figure frame)\n");
  for (int wi = 0; wi < (hmode ? 1 : WPN_COUNT); wi++) {
    const bot_t *b = probe_capture_figure(wi);
    // ONE RANGE PER PART, NOT A UNION, and that is the whole correctness of
    // this proof. Ray parity counts crossings: a point inside the torso AND
    // inside the plate carrier crosses two closed surfaces, the count comes
    // out EVEN, and the deepest-buried vertex on the weapon reports as being
    // in open air. Measured on the first version — the AR's butt pad sits
    // 84 mm right of the spine at chest height and read as 55 mm OUTSIDE the
    // body. A union is only a solid when its parts do not overlap, and a
    // plate carrier is worn ON a torso by definition.
    int brng[96][2], nb = 0, srng[24][2], ns = 0;
    v3 blo[96], bhi[96];
    for (int i = 0; i < g_nprobe && nb < 96; i++) {
      if (!g_probe[i].name) continue;
      if (hmode) {
        for (unsigned k = 0; k < sizeof HEADP / sizeof *HEADP && ns < 24; k++)
          if (!strcmp(g_probe[i].name, HEADP[k])) {
            srng[ns][0] = g_probe[i].v0; srng[ns][1] = g_probe[i].v1; ns++;
            break;
          }
      } else if (!strcmp(g_probe[i].name, "gun")) {
        srng[0][0] = g_probe[i].v0; srng[0][1] = g_probe[i].v1; ns = 1;
      }
      for (unsigned k = 0; k < BSETN; k++)
        if (!strcmp(g_probe[i].name, BSET[k])) {
          brng[nb][0] = g_probe[i].v0; brng[nb][1] = g_probe[i].v1;
          // A per-part box, so the O(parts x verts x tris) sweep only pays
          // for the parts a vertex could possibly be inside.
          v3 lo = {{1e9f, 1e9f, 1e9f}}, hi = {{-1e9f, -1e9f, -1e9f}};
          for (int f = g_probe[i].v0; f + VERT_FLOATS <= g_probe[i].v1;
               f += VERT_FLOATS) {
            v3 v = SCENE_V(f);
            for (int c = 0; c < 3; c++) {
              if (v.e[c] < lo.e[c]) lo.e[c] = v.e[c];
              if (v.e[c] > hi.e[c]) hi.e[c] = v.e[c];
            }
          }
          blo[nb] = lo; bhi[nb] = hi;
          nb++;
          break;
        }
    }
    if (!ns || !nb) { printf("figbury %s  n/a\n",
                             hmode ? "head" : WPN_DEF[wi].prefix); continue; }
    // The rearmost vertex ALONG THE BORE is the butt: the frame comes from
    // the same pose_gun_frame the model was placed with, so this is the
    // model's own rear and not a world-axis accident.
    v3 org, rt, up, fw;
    pose_gun_frame(&b->anim.pose, wi, &org, &rt, &up, &fw);
    float cy2 = cosf(b->yaw), sy2 = sinf(b->yaw);
    float deep = 0.0f, butt_z = 1e9f, butt_sd = 0.0f;
    v3 buttp = {{0, 0, 0}}, deepp = {{0, 0, 0}};
    int inside = 0, total = 0;
    for (int sr = 0; sr < ns; sr++)
    for (int f = srng[sr][0]; f + VERT_FLOATS <= srng[sr][1]; f += VERT_FLOATS) {
      v3 v = SCENE_V(f);
      float sd = 1e9f;
      for (int q = 0; q < nb; q++) {
        if (v.x < blo[q].x || v.x > bhi[q].x || v.y < blo[q].y ||
            v.y > bhi[q].y || v.z < blo[q].z || v.z > bhi[q].z) continue;
        float d = trig_mesh_sd(v, brng[q], 2);
        if (d < sd) sd = d;
      }
      if (sd > 1e8f) sd = 1.0f;   // outside every part's box: comfortably out
      total++;
      if (sd < 0.0f) inside++;
      if (-sd > deep) { deep = -sd; deepp = v; }
      float z = hmode ? v.y : v3_dot(v3_sub(v, org), fw);
      if (z < butt_z) { butt_z = z; butt_sd = sd; buttp = v; }
    }
    // ...and WHERE it is, in the figure's own frame, which is the number a
    // pose edit is actually aimed at: x right of the spine, y above the
    // ground, z forward of it.
    float dx2 = buttp.x - b->pos.x, dz2 = buttp.z - b->pos.z;
    float bl[3] = {dx2 * cy2 + dz2 * sy2, buttp.y - b->pos.y,
                   dx2 * sy2 - dz2 * cy2};
    float dx3 = deepp.x - b->pos.x, dz3 = deepp.z - b->pos.z;
    float dl[3] = {dx3 * cy2 + dz3 * sy2, deepp.y - b->pos.y,
                   dx3 * sy2 - dz3 * cy2};
    // The butt's own station along the bore, in MODEL units — the number a
    // weapon-aware anchor needs and the one every comment in this file has
    // guessed at. Printed, not assumed.
    printf("figbury-butt %s z=%+.4f\n", WPN_DEF[wi].prefix,
           (double)(butt_z / GUN_GIRTH_3P));
    printf("figbury %-4s %10.1f %8.1f %8.1f  %d/%d   "
           "(%.0f %.0f %.0f) deep_at(%.0f %.0f %.0f)\n",
           hmode ? "head" : WPN_DEF[wi].prefix,
           (double)(deep * 1000.0f),
           (double)(total ? 100.0f * (float)inside / (float)total : 0.0f),
           (double)(-butt_sd * 1000.0f), inside, total,
           (double)(bl[0] * 1000.0f), (double)(bl[1] * 1000.0f),
           (double)(bl[2] * 1000.0f), (double)(dl[0] * 1000.0f),
           (double)(dl[1] * 1000.0f), (double)(dl[2] * 1000.0f));
  }
}

// Six impact locations on bot 0: peak joint travel per case, and how far apart
// the six films are.
static void flinch_proof(sim_ctx_t *s) {
  // WHAT A ROUND DOES TO A MAN WHO SURVIVES IT, per impact location. The
  // death torque was written because four hit locations produced a
  // byte-identical film; the FLINCH had exactly the same defect and it is
  // on far more rounds — a sniper one-shots, so almost every flinch in a
  // match is an AR round on a living body.
  // Six impacts on a frozen puppet, each from the same muzzle so only the
  // LOCATION differs. Per case: the peak displacement of every joint against
  // the same body's un-hit pose, the joint that moved most, and — the line
  // that matters — the smallest peak-to-peak difference against any OTHER
  // case. `distinct` must not be near zero, or the six are one animation.
  // IT STAGES ITSELF. Run bare, the bot walks and the command measures a
  // GAIT (peak 3780 mm at an ankle); run under `botfreeze on` — which is
  // what this file prescribes for every pose test — the solver does not
  // advance and every field reads 0.0. A proof whose output depends on
  // staging nobody stated is the `reach` failure again, so the staging is
  // part of the command: puppet mode, standing still, restored after.
  // Save EVERY field the staging overwrites — a restore that returns 3 of
  // 8 leaves a script's puppet drive silently parked for everything after.
  typedef struct {
    int puppet, crouch, jump, slide, ready;
    float mx, mz, speed, face;
  } flinch_drive_t;
  flinch_drive_t was_drive[MAX_BOTS];
  int was_frz = g_bots_frozen;
  g_bots_frozen = 0;
  // Match ticks and every pose solver must keep running, so botfreeze is
  // not an isolation tool here. Puppet every bot instead: bot_tick still
  // advances its body/animation, but no live AI can acquire and shoot the
  // subject. The whole drive is saved/restored because `puppet off` keeps
  // its old scripted inputs and this proof must not briefly revive them.
  for (int bi = 0; bi < MAX_BOTS; bi++) {
    bot_t *pb = &g_bots[bi];
    was_drive[bi] = (flinch_drive_t){
      pb->puppet, pb->pup_crouch, pb->pup_jump, pb->pup_slide, pb->pup_ready,
      pb->pup_mx, pb->pup_mz, pb->pup_speed, pb->pup_face
    };
    pb->puppet = 1;
    pb->pup_crouch = pb->pup_jump = pb->pup_slide = 0;
    pb->pup_ready = 1;
    pb->pup_mx = pb->pup_mz = pb->pup_speed = 0.0f;
    pb->pup_face = pb->yaw;
  }
  struct { const char *name; float dx, dy; } CASE[6] = {
    {"head",  0.00f, 1.66f}, {"chest", 0.00f, 1.30f},
    {"gut",   0.00f, 1.00f}, {"thigh", 0.00f, 0.62f},
    {"shoL", -0.22f, 1.42f}, {"shoR",  0.22f, 1.42f},
  };
  static const char *JN[POSE_NPOS] = {
    "hipL","hipR","kneeL","kneeR","ankL","ankR","toeL","toeR",
    "shoL","shoR","elbL","elbR","wriL","wriR",
    "pelvis","chest","neck","head","gun"};
  float peak[6][POSE_NPOS] = {{0}};
  bot_t *b = &g_bots[0];
  // Staging leaves the MATCH live (botfreeze cannot be used — the solver
  // must advance), so a stray round from another bot can re-latch anim_hit
  // over the staged case or kill the subject outright: a 26 m respawn
  // teleport then prints as a 26134 mm "flinch" that passes the distinct
  // contract with flying colours. A poisoned proof must fail loudly.
  int base_hp = b->hp;
  for (int c = 0; c < 6; c++) {
    // Same start state for every case: reset the solver, let the idle pose
    // settle, then latch the base. Without the reset the previous case's
    // ring is still in `hit_s` and every case measures its predecessor.
    anim_reset(&b->anim, b->pos, b->yaw);
    b->anim.hit_k = b->anim.hit_s = b->anim.hit_w = 0.0f;
    // `died` LATCHES: a kill early in the settle window respawns the bot
    // (120 ticks) before the case-end check, with hp back at 100 — so the
    // end-of-case alive/hp test alone reads clean while the respawn
    // teleport sat inside the observation window.
    int died = 0;
    for (int i = 0; i < 90; i++) { sim_advance(s); died |= !b->alive; }
    pose_t base = b->anim.pose;
    const v3 *bj = (const v3 *)&base;
    v3 rt = {{cosf(b->yaw), 0.0f, sinf(b->yaw)}};
    v3 at = {{b->pos.x + rt.x * CASE[c].dx, b->pos.y + CASE[c].dy,
              b->pos.z + rt.z * CASE[c].dx}};
    v3 from = {{at.x, at.y, at.z + 6.0f}};
    anim_hit(&b->anim, b->pos, b->yaw, from, at, c == 0);
    for (int i = 0; i < 45; i++) {
      sim_advance(s);
      died |= !b->alive;
      const v3 *pj = (const v3 *)&b->anim.pose;
      for (int k = 0; k < POSE_NPOS; k++) {
        float d = v3_len(v3_sub(pj[k], bj[k]));
        if (d > peak[c][k]) peak[c][k] = d;
      }
    }
    if (died || b->hp != base_hp) {
      printf("flinch: STAGING VIOLATED — bot 0 %s during case %s (t=%ld); "
             "numbers would be poisoned, aborting\n",
             died ? "was killed" : "took a real round",
             CASE[c].name, g_tick);
      exit(1);
    }
  }
  for (int c = 0; c < 6; c++) {
    int top = 0;
    float sum = 0.0f;
    for (int k = 0; k < POSE_NPOS; k++) {
      sum += peak[c][k];
      if (peak[c][k] > peak[c][top]) top = k;
    }
    float dmin = 1e9f;
    for (int o = 0; o < 6; o++) {
      if (o == c) continue;
      float d = 0.0f;
      for (int k = 0; k < POSE_NPOS; k++)
        d += fabsf(peak[c][k] - peak[o][k]);
      if (d < dmin) dmin = d;
    }
    printf("flinch %-6s peak=%.1fmm<%s> head=%.1f chest=%.1f pelvis=%.1f "
           "sum=%.1f distinct=%.1f\n", CASE[c].name,
           (double)(peak[c][top] * 1000.0f), JN[top],
           (double)(peak[c][17] * 1000.0f), (double)(peak[c][15] * 1000.0f),
           (double)(peak[c][14] * 1000.0f), (double)(sum * 1000.0f),
           (double)(dmin * 1000.0f));
  }
  // Leave bot 0 as it was found, un-flinched: this command burns 810 ticks,
  // and any later command in the same script would otherwise be measuring a
  // poisoned bot.
  b->anim.hit_k = b->anim.hit_s = b->anim.hit_w = 0.0f;
  anim_reset(&b->anim, b->pos, b->yaw);
  for (int bi = 0; bi < MAX_BOTS; bi++) {
    bot_t *pb = &g_bots[bi];
    pb->puppet = was_drive[bi].puppet;
    pb->pup_crouch = was_drive[bi].crouch;
    pb->pup_jump = was_drive[bi].jump;
    pb->pup_slide = was_drive[bi].slide;
    pb->pup_ready = was_drive[bi].ready;
    pb->pup_mx = was_drive[bi].mx;
    pb->pup_mz = was_drive[bi].mz;
    pb->pup_speed = was_drive[bi].speed;
    pb->pup_face = was_drive[bi].face;
  }
  g_bots_frozen = was_frz;
}

// The trigger guard and the blade against the HAND'S MESH — the pair figcheck
// and vmtrig's siblings cannot see, per weapon x reload station x press x view.
static void vmtrig_proof(sim_ctx_t *s) {
  // TRIGGER PROOF — A TUBE AGAINST A MESH, not a vertex against a tube.
  //
  // The defect this command exists to catch is a THIN weapon part swallowed
  // by a FAT finger, and the point-in-solid test it used to run is
  // structurally unable to see it: the hand's vertex lattice is ~5 mm around
  // a ring by ~8 mm along a digit, so a 3.4 mm guard wire passes clean
  // through the holes. Measured before this rewrite, the nearest hand vertex
  // to the AR guard's centreline was 3.56 mm against the old 3.13 mm
  // threshold — the sieve missed by 0.29 mm while the guard was half buried
  // in the middle finger, and the command printed thru=0 the whole time.
  // Its other number was worse: `reach` was the distance from the index
  // chain's END-CAP CENTRE to the blade's contact point, and that cap centre
  // IS the contact point plus a constant — so the printed +2.6 mm was one
  // literal in fig_hand read back to itself, identical for both weapons,
  // both press states and both ADS states. A proof whose output cannot move
  // is not a proof.
  //
  // Here the WEAPON's own published centrelines are sampled and each sample
  // is asked how far it is from the HAND's surface, signed by ray parity
  // against the hand's closed manifold — which figcheck and vmcheck are what
  // guarantee. Negative = the steel is inside the flesh.
  //   pad   = the blade's contact point against the hand's surface. A pad
  //           presses and deforms, so a millimetre or two negative is right.
  //   guard = the trigger guard's bow. Must be >= 0: it is a wire in open
  //           air, and nothing can hide it crossing a finger.
  //   blade = the trigger blade. Same rule, minus the pad's own squash.
  //   wrist = how far the wrist sits from the grip axis, and `clamp` whether
  //           fig_hand had to slide the weapon out from under it — the one
  //           number that turns "the third-person hand is not on the grip"
  //           into a printed fact.
  // ADS is not swept: the hand and the weapon are built in the same
  // gun-local frame, so every ads row was byte-identical to its hip row.
  // The RELOAD stations are, because they move the trigger hand (the SR's
  // takes it to the bolt knob), and so is the THIRD-PERSON figure, which
  // this command never ran at all and where the defect is worst.
  player_t tmp = *s->p;
  v3 eye = {{0, 0, 0}}, rgt = {{1, 0, 0}}, upv = {{0, 1, 0}}, fwd = {{0, 0, -1}};
  static const float ST[] = {0.0f, 0.15f, 0.35f, 0.55f, 0.75f, 0.90f};
  static const char *STN[] = {"hip ", "rl15", "rl35", "rl55", "rl75", "rl90"};
  static const char *STN3[] = {"live", "dead"};
  float worst_guard = 1e9f, worst_blade = 1e9f, worst_pad = 1e9f;
  float worst_body = 0.0f, worst_inwp = 0.0f;
  for (int view = 0; view < 2; view++) {
    for (int wi = 0; wi < WPN_COUNT; wi++) {
      for (int st = 0; st < (int)(sizeof ST / sizeof ST[0]); st++) {
        // The figure gets two stations, and the second is the one that
        // matters: a CORPSE. fig_hand's reach clamp exists for the poses the
        // viewmodel can never produce — "on death sprawls both wrists to the
        // chest while the rifle leaves with the trigger hand" — and passing
        // a NULL death_t here would have left exactly that pose untested.
        // The live row is whatever carriage the script left bot 0 in, so
        // drive the puppet first if a particular one is wanted; `wrist` is
        // printed so the row is attributable either way.
        if (view == 1 && st > 1) continue;
        for (int pr = 0; pr <= 1; pr++) {
          tmp.wp.cur = wi;
          tmp.wp.ads_lin = tmp.wp.ads_s = tmp.wp.prev_ads_s = 0.0f;
          tmp.wp.trig_s = tmp.wp.prev_trig_s = (float)pr;
          tmp.wp.switch_t = 0; tmp.wp.prev_switch = 0.0f;
          tmp.vm.kick = tmp.vm.prev_kick = 0.0f;
          int rt = WPN_DEF[wi].reload_ticks;
          tmp.wp.reload_t = st ? (int)((1.0f - ST[st]) * (float)rt) : 0;

          g_nprobe = 0; g_probe_on = 1;
          scene_reset();
          if (view == 0) {
            vm_build(&tmp, 1.0f, eye, rgt, upv, fwd);
          } else {
            // The same hand on the same rifle, built by the figure: a
            // girth-scaled weapon, an IK wrist and a coarser digit profile.
            g_cam_eye = g_bots[0].pos;
            // A fully toppled, fully slack body about a horizontal axis,
            // with the vertical spin the new death kinematics can leave on
            // it — the state the sprawl and the wall clamp both act in.
            death_t dth = {1.0f, DEATH_TILT_MAX, 0.0f, {{1.0f, 0, 0}}, 0.6f, 0.0f};
            scene_draw_figure(&g_bots[0].anim, g_bots[0].pos, g_bots[0].yaw,
                              st ? &dth : NULL, 1.0f, 0.0f, (v3){{1, 1, 1}},
                              0, wi, gun_anim(&tmp.wp, 1.0f));
          }
          g_probe_on = 0;
          // The hand's triangles: every probe range whose name is a hand
          // part. One mesh, so the palm, all four digits, the thumb and the
          // forearm are covered without an allow-list to forget.
          int rng[16], nrng = 0;
          for (int i = 0; i < g_nprobe && nrng < 14; i++) {
            if (!g_probe[i].name) continue;
            if (strcmp(g_probe[i].name, "vm_hands") &&
                strcmp(g_probe[i].name, "trigfinger") &&
                strcmp(g_probe[i].name, "hand")) continue;
            rng[nrng++] = g_probe[i].v0;
            rng[nrng++] = g_probe[i].v1;
          }
          // ...and the TRIGGER FINGER against the receiver box. The palm's
          // top corner is legitimately buried in the receiver — a hand's
          // index MCP sits right under it, and burial is how every other
          // joint in this model is closed — so only the thin digit that
          // reaches INTO the weapon is asked. Dropping this test in the
          // tube-vs-mesh rewrite left the one remaining piece of "material"
          // the finger can cross unmeasured.
          float body = 0.0f;
          for (int i = 0; i < g_nprobe; i++) {
            if (!g_probe[i].name || strcmp(g_probe[i].name, "trigfinger"))
              continue;
            for (int f = g_probe[i].v0; f + 2 < g_probe[i].v1; f += VERT_FLOATS) {
              v3 rel = v3_sub(SCENE_V(f), g_gun_body_c);
              float deep = 1e9f;
              for (int k2 = 0; k2 < 3; k2++) {
                float h = v3_len(g_gun_body_e[k2]);
                if (h < 1e-6f) { deep = 0.0f; break; }
                float o = h - fabsf(v3_dot(rel, v3_scale(g_gun_body_e[k2], 1.0f / h)));
                if (o < deep) deep = o;
              }
              if (deep > body) body = deep;   // deepest vertex inside the box
            }
          }
          // ...and the finger against the WHOLE WEAPON, which is the only
          // test that cannot miss a part nobody thought to publish. The
          // guard and the blade are the two thin tubes and they are tested
          // as tubes; everything else the rifle is made of — receiver,
          // magwell, magazine, grip — is a fat closed chain, and a digit
          // inside one of those is exactly the "finger reaches through the
          // weapon material" the user keeps seeing. Ray parity over the union
          // works because vmcheck proves every chain is closed.
          int grn[4], ngrn = 0;
          for (int i = 0; i < g_nprobe && ngrn < 2; i++)
            if (g_probe[i].name && (!strcmp(g_probe[i].name, "vm_gun") ||
                                    !strcmp(g_probe[i].name, "gun"))) {
              grn[ngrn++] = g_probe[i].v0; grn[ngrn++] = g_probe[i].v1;
            }
          float inwp = 0.0f;
          // THE WHOLE HAND, not just the aimed digit: the other three
          // fingers wrap the pistol grip a few millimetres from the magwell
          // and the magazine, and they are what the player is actually
          // looking at in first person. A legitimate grip bite is HAND_BITE
          // (3 mm, x the girth); anything deeper is a crossing.
          for (int q = 0; q < nrng; q += 2)
            for (int f = rng[q]; f + 2 < rng[q + 1]; f += VERT_FLOATS) {
              float d = -trig_mesh_sd(SCENE_V(f), grn, ngrn);
              if (d > inwp) inwp = d;
            }
          float pad = trig_mesh_sd(g_gun_trig_p, rng, nrng);
          float guard = trig_tube_clear(g_gun_guard, 4, g_gun_guard_r, rng, nrng);
          // The blade is measured on its CENTRELINE (radius 0), not on its
          // surface: a pad legitimately closes OVER a trigger shoe, so the
          // steel's surface sinking a few millimetres into the flesh is the
          // contact rather than a defect — what must not happen is the
          // blade's own axis going deep, which is the shoe being swallowed
          // whole. Measuring the surface instead made the bound depend on
          // the tube's radius and therefore on GUN_GIRTH_3P.
          float blade = trig_tube_clear(g_gun_blade, 3, 0.0f, rng, nrng);
          printf("vmtrig %s %s %s trig=%d pad=%+.1f guard=%+.1f blade=%+.1f "
                 "body=%.1f inwpn=%.1f wrist=%.1f clamp=%d\n",
                 view ? "3p" : "vm", WPN_DEF[wi].prefix,
                 view ? STN3[st] : STN[st], pr,
                 (double)(pad * 1000.0f), (double)(guard * 1000.0f),
                 (double)(blade * 1000.0f), (double)(body * 1000.0f),
                 (double)(inwp * 1000.0f), (double)(g_fig_wrist_mm),
                 g_fig_clamped);
          if (guard < worst_guard) worst_guard = guard;
          if (blade < worst_blade) worst_blade = blade;
          if (pad < worst_pad) worst_pad = pad;
          if (body > worst_body) worst_body = body;
          if (inwp > worst_inwp) worst_inwp = inwp;
        }
      }
    }
  }
  // The guard is a wire in open air and may never be inside the hand at all.
  // The blade may be inside it by the pad's own squash and no more.
  // The guard is a WIRE IN OPEN AIR and may never be inside the hand at all.
  // The blade is different in kind: a pad closes OVER a trigger shoe, so its
  // surface legitimately sinks a few millimetres in — what must not happen
  // is the CENTRELINE going deep, which is the blade being swallowed whole.
  // -4.5 mm of centreline burial. The shoe is 4.2 mm in half-width and the
  // fingertip is 19 mm thick (23 at the third-person girth), so at 4 mm the
  // steel's face is inside the pad and its far side is 8 mm from coming out
  // the other side — which is a pad closing on a trigger, not a swallowed
  // blade. Before this pass the AR measured 6.8 mm of centreline inside the
  // finger with the whole shoe hidden in the flesh.
  printf("vmtrig worst pad=%+.1f mm guard=%+.1f mm (must be >= 0) "
         "blade=%+.1f mm (must be >= %.1f) body=%.1f inwpn=%.1f mm\n",
         (double)(worst_pad * 1000.0f), (double)(worst_guard * 1000.0f),
         (double)(worst_blade * 1000.0f), -4.5, (double)(worst_body * 1000.0f),
         (double)(worst_inwp * 1000.0f));
}

// A headless client against a real --server on localhost: handshake, reconcile
// and event-channel stats, end to end and without GL.
static void netclient_proof(void) {
  // Headless end-to-end client: connect to a running
  // `--server` on HOST:PORT, drive N ticks of scripted input, and print
  // the handshake result plus prediction/reconciliation stats. Testable on
  // 127.0.0.1 with the server in a second process — the whole transport
  // without GL. Blocks up to ~5 s for the handshake.
  const char *host = next_tok();
  int port = (int)next_n();
  long nticks = next_n();
  // Optional ACTS: comma-separated script action names held for the whole
  // run, e.g. "fire" or "forward,fire,ads"; default "forward", which is
  // what this proof used to hardcode. That hole is exactly why a server
  // simulating shots no client could see or hear shipped: nothing in the
  // battery had ever pulled a trigger over the wire.
  //
  // Peeked and pushed back like opt_n, and only accepted when EVERY comma
  // part is a known action — otherwise `netclient H P 60; pos` would eat
  // the next command. Unknown names are not fatal here for the same reason.
  int held[IN_COUNT] = {0};
  char *at = take_tok();
  int got_acts = 0;
  if (at) {
    int tmp[IN_COUNT] = {0}, ok = 1;
    char buf[128];
    snprintf(buf, sizeof buf, "%s", at);
    for (char *p = buf, *q = buf; ok; q++) {
      if (*q == ',' || !*q) {
        int last = !*q;
        *q = 0;
        int a = -1;
        for (int k = 0; k < IN_COUNT; k++)
          if (!strcmp(p, ACTION_DEF[k].script)) a = k;
        if (a < 0) ok = 0; else tmp[a] = 1;
        p = q + 1;
        if (last) break;
      }
    }
    if (ok) { memcpy(held, tmp, sizeof held); got_acts = 1; }
    else g_tok_pushback = at;
  }
  if (!got_acts) held[IN_FWD] = 1;       // the historical default
  uint32_t ip = net_resolve(host);
  if (!ip) { printf("netclient resolve FAIL %s\n", host); }
  else {
    net_cl_connect(ip, (uint16_t)port, g_cfg.name);
    uint32_t t0 = net_now_ms();
    // pump the handshake (the server runs in its own process/loop)
    while (g_cl.state == NC_CONNECTING || g_cl.state == NC_JOINING) {
      net_cl_poll();
      if (net_now_ms() - t0 > 5000) break;
      struct timespec ts = {0, 5000000L}; nanosleep(&ts, NULL);
    }
    if (g_cl.state != NC_PLAYING) {
      printf("netclient state=%s reject=%d (no server on %s:%d?)\n",
             g_cl.state == NC_FAILED ? "FAILED" : "TIMEOUT", g_cl.reject,
             host, port);
    } else {
      // Drive input at ~120 Hz wall-clock, predict locally, let snapshots
      // reconcile. Simple forward-walk input so motion actually happens.
      g_cl_corrections = 0; g_cl_worst_err_mm = 0.0f;
      g_cl_ev_applied = 0; g_cl_ev_dupes = 0;
      int h = human_of_ent(g_cl.my_ent);
      // CLOCK-LOCKED TO TICK_DT, not a free-running nanosleep. The rig used
      // to sleep a flat 8 ms per iteration and then do its work, so it
      // predicted fewer sim ticks per second than the server simulated and
      // every rate comparison came out skewed — a client that predicted a
      // whole magazine while the server had fired two thirds of one looked
      // like lost events until this was ruled out. The real client cannot
      // have this bug: app_frame accumulates real time and runs exactly one
      // predicted tick per TICK_DT, which is what this now mirrors.
      struct timespec next;
      clock_gettime(CLOCK_MONOTONIC, &next);
      for (long i = 0; i < nticks; i++) {
        player_t *p = h >= 0 ? &g_players[h] : &g_players[0];
        // Firing blind at a wall proves the event channel, not the hit
        // path. With `fire` held, aim like a player: at the nearest live
        // bot's chest as THIS client renders it — dead-reckoned foreign
        // state — so the WAN run exercises prediction, lag comp and the
        // hit/score replication end to end.
        if (held[IN_FIRE] && h >= 0) {
          int tgt = -1; float best = 1e9f;
          for (int bi = 0; bi < g_match.nbots; bi++) {
            if (!g_bots[bi].active || !g_bots[bi].alive) continue;
            v3 dd = v3_sub(g_bots[bi].pos, p->pos);
            float d2 = dd.x * dd.x + dd.z * dd.z;
            if (d2 < best) { best = d2; tgt = bi + 1; }
          }
          if (tgt > 0) aim_at(p, ent_aimpoint(tgt));  // the one aim helper
        }
        // Movement rides the HELD buttons only — the rig used to hardwire
        // a full forward wish besides, so "netclient … fire" never stood
        // still and no scenario could separate standing from walking.
        net_cl_tick(held, 0.0f, 0.0f, p->yaw, p->pitch);   // predict + send
        net_cl_poll();                     // recv snapshots -> reconcile
        rescue_client_tick();              // a mission's presentation half
        net_cl_extrapolate();              // the rig aims at the same dead-
                                           // reckoned targets the player
                                           // sees — view ticks assume it
        // The same clock nudge the played client runs: hold the
        // server-side queue depth at target instead of letting scheduler
        // drift walk it into the trim. The rig proves the mechanism.
        float qe = NET_QDEPTH_TARGET - g_cl.qdepth_ema;
        double rate = 1.0 + (double)f_clamp(qe * 0.004f, -0.03f, 0.01f);
        next.tv_nsec += (long)(1e9 / TICK_HZ / rate);
        while (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
      }
      v3 fp = h >= 0 ? g_players[h].pos : (v3){{0,0,0}};
      // ev_applied counts events the SERVER sent and this client accepted;
      // ev_dupes counts resends the high-water mark rejected. Firing with
      // `fire` in ACTS must move ev_applied — a zero there is the server
      // simulating shots in silence, which is what the event channel exists to prevent.
      char actbuf[96] = {0};
      for (int k = 0; k < IN_COUNT; k++)
        if (held[k]) {
          if (actbuf[0]) strncat(actbuf, ",", sizeof actbuf - strlen(actbuf) - 1);
          strncat(actbuf, ACTION_DEF[k].script, sizeof actbuf - strlen(actbuf) - 1);
        }
      printf("netclient CONNECTED ent=%d acts=%s world=%08x server_tick=%u "
             "corrections=%d worst_err=%.1fmm ev_applied=%d ev_dupes=%d "
             "ev_high=%u hp=%d alive=%d pred_ammo=%d pos=(%.2f %.2f %.2f) "
             "ping=%d qdepth=%.1f kills=%d deaths=%d shots=%d hits=%d "
             "fraglimit=%d reject=%d\n",
             g_cl.my_ent, actbuf, g_cl.world, g_cl.server_tick, g_cl_corrections,
             (double)g_cl_worst_err_mm, g_cl_ev_applied, g_cl_ev_dupes,
             g_cl.ev_high,
             h >= 0 ? g_players[h].hp : -1, h >= 0 ? g_players[h].alive : -1,
             h >= 0 ? g_players[h].wp.ammo[g_players[h].wp.cur] : -1,
             (double)fp.x, (double)fp.y, (double)fp.z,
             g_net_ping[g_cl.my_ent], (double)g_cl.qdepth_ema,
             g_match.kills[g_cl.my_ent], g_match.deaths[g_cl.my_ent],
             g_match.shots[g_cl.my_ent], g_match.hits[g_cl.my_ent],
             g_net_fraglimit, g_cl.reject);
      // A mission online: what NP_RESCUE delivered (the client runs none of it).
      if (g_match.mode == GM_RESCUE)
        printf("netclient rescue view=%d sim=%d squad=%d guards=%d nbots=%d alarm=%d "
               "freed=%d extracted=%d result=%d clock=%d msg=\"%s\" h0=(%.1f %.1f %.1f) st=%d,%d,%d "
               "mates=%d\n",
               rescue_view(), rescue_on(), g_resc.squad, g_resc.nguards, g_match.nbots,
               g_resc.alarm, g_resc.freed, g_resc.extracted, g_resc.result, g_resc.clock,
               g_resc.msg, (double)g_resc.h[0].pos.x, (double)g_resc.h[0].pos.y,
               (double)g_resc.h[0].pos.z, g_resc.h[0].state, g_resc.h[1].state, g_resc.h[2].state,
               g_match.nbots - g_resc.nguards);
    }
    net_cl_disconnect();
  }
}

// An online death all the way through to a corpse, client half.
static void netdeath_proof(void) {
  // THE ONLINE DEATH PATH, as a gate. Offline a kill runs ent_damage
  // (ragdoll launch) and player_present_tick (corpse handover) on one
  // tick. Online NEITHER runs on the client — client_online_tick replaces
  // player_tick — and the only thing the client is ever told is an alive
  // flag going 0 in a snapshot. Before 2026-08-18 that produced nothing at
  // all: scene_build skips a dead human and no corpse was built, so a
  // killed player VANISHED mid-stride. Staged rather than driven through a
  // socket, because the defect is in the presentation half of the client,
  // not in the transport.
  int h = 1, ent = ent_of_human(h);
  g_online = 1;
  g_humans_on[h] = 1;
  player_t *p = &g_players[h];
  player_spawn(p);
  p->pos = (v3){{0.0f, 0.0f, 6.0f}};
  p->prev_pos = p->pos;
  p->vel = (v3){{0.0f, 0.0f, 0.0f}};
  p->alive = 1; p->grounded = 1; p->hp = 100;
  anim_reset(&g_player_anim[h], p->pos, p->yaw);
  g_cl_alive_prev[ent] = 1;
  // The wound the server would have sent: a round from the front, chest
  // high — the EV_HIT* the client latches in g_wound.
  g_wound[ent] = (typeof(g_wound[0])){
      v3_add(p->pos, (v3){{0.0f, 1.10f, 0.0f}}),
      v3_add(p->pos, (v3){{0.0f, 1.50f, 4.0f}}), g_tick, 46, 0};
  p->alive = 0;              // <- the snapshot flag going 0, nothing else
  client_anim_all();         // the tick that has to notice it
  int born = 0; v3 p0 = {{0, 0, 0}};
  for (int c = 0; c < MAX_CORPSES; c++)
    if (g_corpses[c].used) { born++; p0 = g_corpses[c].pos; }
  // ...and it has to FLY. A corpse that is created and never integrated is
  // the same bug one layer down, and it looks identical in a still frame.
  for (int i = 0; i < 240; i++) { g_tick++; vfx_tick(); }
  int live = 0, gnd = 0; float moved = 0.0f, tilt = 0.0f;
  for (int c = 0; c < MAX_CORPSES; c++)
    if (g_corpses[c].used) {
      live++;
      v3 d = v3_sub(g_corpses[c].pos, p0);
      moved = sqrtf(v3_dot(d, d));
      gnd = g_corpses[c].rag.grounded;
      tilt = g_corpses[c].rag.tilt;
    }
  // AND THE OTHER HALF: a bot dies, then RESPAWNS. Offline bot_spawn's
  // memset clears the ragdoll; online nothing does, and scene_draw_bot
  // feeds b->rag through rag_death for a LIVING bot too — so the bot stood
  // back up still collapsed on the floor. Reported from a live server on
  // 2026-08-18; the numbers below read dead_tilt>0, up_tilt=1.9 before the
  // fix and dead_tilt>0, up_tilt=0.00 after it.
  g_match.nbots = 1;
  bot_t *bt = &g_bots[0];
  memset(bt, 0, sizeof *bt);
  bt->active = 1; bt->alive = 1; bt->hp = 100;
  bt->pos = (v3){{2.0f, 0.0f, 6.0f}};
  bt->prev_pos = bt->pos;
  anim_reset(&bt->anim, bt->pos, bt->yaw);
  g_cl_body[1] = 0;                   // nobody owns this body yet
  g_wound[1] = (typeof(g_wound[0])){
      v3_add(bt->pos, (v3){{0.0f, 1.10f, 0.0f}}),
      v3_add(bt->pos, (v3){{0.0f, 1.50f, 4.0f}}), g_tick, 46, 0};
  bt->alive = 0;                      // the snapshot says he is down
  for (int i = 0; i < 60; i++) { g_tick++; client_anim_all(); }
  float dead_tilt = bt->rag.tilt, dead_top = bt->topple;
  bt->alive = 1;                      // ...and now the snapshot respawns him
  bt->pos = (v3){{-3.0f, 0.0f, -2.0f}};
  client_anim_all();
  printf("netdeath bot dead_tilt=%.2f dead_topple=%.2f "
         "up_tilt=%.2f up_topple=%.2f\n",
         (double)dead_tilt, (double)dead_top,
         (double)bt->rag.tilt, (double)bt->topple);
  printf("netdeath corpses=%d live=%d moved=%.3f tilt=%.2f grounded=%d queue=%d\n",
         born, live, (double)moved, (double)tilt, gnd, g_player_corpse_q[h]);
  // THE OTHER HALF OF "IT LOOKS BROKEN" IS THE RENDER LERP, and no state
  // dump can see it. scene_draw_bot lerps prev_topple->topple and
  // prev_yaw->yaw by the SUB-TICK alpha, and alpha is re-drawn every
  // rendered frame. Offline `bot_tick` advances both prev_ pairs once per
  // tick; ONLINE NOTHING DID — client_anim_all never wrote them and the
  // net layer only maintained the position pair. So prev_topple sat at 0
  // while topple ran to 1, and prev_yaw kept whatever the slot was born
  // with for the whole session: the body's collapse amount and its facing
  // jumped across their FULL range on every frame — a corpse that stands
  // up and falls over again 60 times a second, and a respawned bot whose
  // body swings between two headings while it walks. Reported from a live
  // server 2026-08-19; measured here before the fix: slackgap 1.000 (the
  // entire collapse) and yawgap 2.500 rad, after it 0.000 and 0.100.
  memset(bt, 0, sizeof *bt);
  bt->active = 1; bt->alive = 1; bt->hp = 100;
  bt->pos = bt->prev_pos = (v3){{2.0f, 0.0f, 6.0f}};
  anim_reset(&bt->anim, bt->pos, bt->yaw);
  g_cl_body[1] = 0;
  // TWO snapshots, because one delta is legitimate: after the second the
  // prev_ pair must hold the FIRST one's heading, not the slot's birth
  // value. Driven through net_cl_apply_ent, which is where the client
  // maintains prev_pos and is therefore where prev_yaw belongs.
  net_ent_t se = {.id = 1, .flags = 1 | 2, .hp = 100,
                  .x = net_q_pos(bt->pos.x), .y = net_q_pos(bt->pos.y),
                  .z = net_q_pos(bt->pos.z), .yaw = net_q_yaw(2.4f)};
  net_cl_apply_ent(&se);
  se.yaw = net_q_yaw(2.5f);
  net_cl_apply_ent(&se);
  float yawgap = fabsf(bt->yaw - bt->prev_yaw);
  bt->alive = 0;                      // ...and down he goes again
  for (int i = 0; i < 60; i++) { g_tick++; client_anim_all(); }
  float sl0 = rag_death(&bt->rag, bt->prev_topple, bt->topple, 0.0f).slack;
  float sl1 = rag_death(&bt->rag, bt->prev_topple, bt->topple, 1.0f).slack;
  // THE POSITION PAIR IS THE THIRD ONE, and it needs a snapshot to show:
  // the server ragdolls its dead bots too and sends them, so this is the
  // record that used to drag the settled body onto a second solution 5 m
  // away while prev_pos held the first — a full-amplitude swing on every
  // rendered frame. posgap is one TICK of corpse motion (0 on a settled
  // one); before the fix it read the whole distance between the two
  // integrators' resting places.
  v3 settled = bt->pos;
  se.flags = 0;                       // still down, and the server says so
  se.x = net_q_pos(settled.x + 5.0f);
  net_cl_apply_ent(&se);
  client_anim_all();
  v3 d = v3_sub(bt->pos, bt->prev_pos);
  v3 dw = v3_sub(bt->pos, settled);
  // JOINING ONTO A BODY THAT IS ALREADY DOWN. There is no death edge to
  // see here, so the launch has to fire on the STATE, and until it does
  // the SERVER owns the position — read as an edge, this client integrated
  // a zeroed rag_t from a position no snapshot was allowed to write and
  // laid the body out at the world origin. `join` is how far the body ends
  // up from where the server put it.
  memset(bt, 0, sizeof *bt);
  memset(g_cl_body, 0, sizeof g_cl_body);
  bt->active = 1; bt->alive = 0;
  se.flags = 0; se.hp = 0;
  se.x = net_q_pos(-12.0f); se.y = net_q_pos(0.0f); se.z = net_q_pos(9.0f);
  net_cl_apply_ent(&se);
  client_anim_all();
  v3 jd = v3_sub(bt->pos, (v3){{-12.0f, 0.0f, 9.0f}});
  printf("netdeath join dist=%.3f owned=%d\n",
         (double)sqrtf(v3_dot(jd, jd)), g_cl_body[1]);
  printf("netdeath lerp slackgap=%.3f yawgap=%.3f posgap=%.3f drift=%.3f\n",
         (double)fabsf(sl1 - sl0), (double)yawgap,
         (double)sqrtf(v3_dot(d, d)), (double)sqrtf(v3_dot(dw, dw)));
}

// Random and garbage packets into every decoder: no crash, and every accepted
// record in bounds. Run it under ASan.
static void netfuzz_proof(void) {
  // Random and bit-flipped bytes into EVERY decoder — no crash, no wild
  // read. netpack proves the roundtrip of WELL-FORMED output; a public
  // port never gets well-formed output. Run under build/game-asan: this is
  // where a missing bounds check becomes a report instead of luck.
  long n = next_n();
  uint64_t rs = 0xf00dface ^ (uint64_t)n * 40503u;
  long accepted_in = 0, accepted_ent = 0;
  for (long i = 0; i < n; i++) {
    unsigned char buf[200];
    int len = (int)(rng_next(&rs) % (sizeof buf + 1));   // 0..200, incl. short
    for (int b = 0; b < len; b++) buf[b] = (unsigned char)rng_next(&rs);
    // Half the packets get a VALID header so the snapshot/scores decoders
    // run past their magic check into the section parsing — pure random
    // bytes almost never match PROTO+type and the deep paths went unfuzzed.
    if ((i & 1) && len >= 3) {
      buf[0] = NET_PROTO & 0xff; buf[1] = NET_PROTO >> 8;
      buf[2] = (i & 2) ? NP_SNAPSHOT : NP_SCORES;
    }
    np_rd r = {buf, (size_t)len, 0, 0};
    net_input_t in;
    if (net_input_decode(&r, &in)) accepted_in++;
    np_rd r2 = {buf, (size_t)len, 0, 0};
    net_ent_t ne;
    if (net_ent_decode(&r2, &ne)) { accepted_ent++;
      if (ne.id >= MAX_ENTS || ne.weapon >= WPN_COUNT) accepted_ent = -1000000; }
    // The scores reader takes the same hostility: garbage must neither
    // crash nor hand back a row whose id could index out of bounds.
    net_score_t sc[MAX_ENTS]; net_name_t nmf[MAX_HUMANS];
    uint8_t ov, wi; uint16_t fr; int nnf = 0;
    int nsc = net_scores_read(buf, (size_t)len, &ov, &wi, &fr, NULL, sc, MAX_ENTS,
                              nmf, MAX_HUMANS, &nnf);
    for (int k = 0; k < nsc; k++)
      if (sc[k].id >= MAX_ENTS) accepted_ent = -1000000;
    for (int k = 0; k < nnf; k++)
      if (nmf[k].id >= MAX_ENTS || nmf[k].name[11]) accepted_ent = -1000000;
    // The snapshot reader drives net_ent_decode AND net_ev_decode with
    // attacker-controlled counts; net_ev_to_live is the ONE guard between
    // a hostile event record and an array index (g_player_anim, WPN_DEF,
    // g_bots) — both used to be outside this battery entirely.
    uint32_t fst, fli, fwh, fsd; uint8_t fqd; int ffull, fnev = 0;
    net_ent_t fents[MAX_ENTS]; net_ev_t fevs[NET_EV_SNAP_MAX];
    int fcnt = net_snapshot_read(buf, (size_t)len, &fst, &fli, &fwh, &fsd, NULL,
                                 &fqd, fents, MAX_ENTS, &ffull,
                                 fevs, NET_EV_SNAP_MAX, &fnev);
    for (int k = 0; k < fcnt; k++)
      if (fents[k].id >= MAX_ENTS || fents[k].weapon >= WPN_COUNT)
        accepted_ent = -1000000;
    for (int k = 0; k < fnev; k++) {
      event_t le;
      net_ev_to_live(&le, &fevs[k]);
      if (le.src >= MAX_ENTS || le.dst >= MAX_ENTS ||
          le.wpn < 0 || le.wpn >= WPN_COUNT || le.type > EV_BLAST)
        accepted_ent = -1000000;
    }
    // Cookie validation must never index or trap on garbage inputs.
    (void)net_cookie_valid(rng_next(&rs), (uint16_t)rng_next(&rs),
                           rng_next(&rs), (uint32_t)i);
  }
  printf("netfuzz n=%ld no_crash=ok in_accepted=%ld ent_accepted=%ld "
         "(all accepted ent ids < MAX_ENTS, weapon < WPN_COUNT)\n",
         n, accepted_in, accepted_ent < 0 ? -1 : accepted_ent);
}

