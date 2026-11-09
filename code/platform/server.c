// code/platform/server.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Dedicated server: ONLY sim + (from Phase 3) UDP. No GL, no
// audio, no window — app_start's g_server gate skips both, and C's
// declaration order already proves no sim path can need a GL context (the
// first gl* call of the whole TU is far below the sim). One instance = one
// process; all state globals stay singletons by design.
// ---------------------------------------------------------------------------
#include <signal.h>
static volatile sig_atomic_t g_srv_stop;
static void srv_sig(int sig) { (void)sig; g_srv_stop = 1; }

// tracks heartbeats and answers QUERY with the instance list. No sim, no GL.

static int server_main(uint32_t seed) {
  // The cap chain — checked, because it holds nowhere by itself: a
  // cap_public above MAX_HUMANS promises a slot net_srv_admit can never hand
  // out, which is what the old cap_hard=10 did.
  if (g_srv.cap_public < 1 || g_srv.cap_public > MAX_HUMANS) {
    fprintf(stderr, "server: need 1 <= --cap-public (%d) <= %d\n",
            g_srv.cap_public, MAX_HUMANS);
    return 1;
  }
  if (g_srv.lobbies < 1 || g_srv.lobbies > SRV_LOBBIES) {
    fprintf(stderr, "server: --lobbies out of range 1..%d\n", SRV_LOBBIES);
    return 1;
  }
  if (g_srv.cap_bots < 0 || g_srv.cap_bots > MAX_BOTS) {
    fprintf(stderr, "server: --cap-bots out of range 0..%d\n", MAX_BOTS);
    return 1;
  }
  // No humans until a join; there is no "me" on a server. g_headless stays 0:
  // the event contract is a HARNESS contract, not a log format — at 1,
  // ev_push would write one line per event into game.log forever, and that
  // file has no rotation because its volume is bounded by design (decision 9).
  g_local_ent = -1;
  // Match parameters from argv, not from config. Until the match_t
  // migration sp_* is the one authoritative copy the sim reads each
  // tick; it is GLOBAL, so the per-lobby bot target is re-established by
  // net_srv_rebalance at the top of every arena's tick, below.
  g_cfg.sp_bots = net_bot_target(0, g_srv.cap_public, g_srv.cap_bots);
  g_cfg.sp_diff = g_srv.skill < 0 ? 0 : g_srv.skill > 2 ? 2 : g_srv.skill;
  g_cfg.sp_frag = g_srv.fraglimit;
  g_cfg.sp_mode = g_srv.mode;
  g_cfg.sp_map = g_srv.map;
  g_cfg.sp_rounds = parse_int_def(g_srv.rounds, &SP_ROUNDS_DEF);
  g_cfg.sp_team_size = g_srv.team_size;
  g_cfg.sp_team_fill = g_srv.team_fill;
  g_cfg.sp_team_ff = g_srv.team_ff;
  g_cfg.sp_squad = g_srv.squad;
  // A team game seats exactly two teams' worth of humans; bots are the
  // team math's, not the --cap-bots fill's (see tdm_bot_want).
  if (g_srv.mode == GM_TDM)
    g_srv.cap_public = 2 * g_srv.team_size < MAX_HUMANS ? 2 * g_srv.team_size : MAX_HUMANS;
  // A mission seats its squad and no more; the guards and the squad's bots
  // are the mission's own (rescue_bot_count).
  if (g_srv.mode == GM_RESCUE) g_srv.cap_public = g_srv.squad;
  signal(SIGINT, srv_sig);
  signal(SIGTERM, srv_sig);
  // EVERY ARENA IS BROUGHT UP THE SAME WAY, through the one shared entry
  // point. app_start skips the renderer and the SFX bake when g_server is set,
  // so on a server it is exactly map + spawn + match_init — all per-lobby
  // state. Each arena walks one more step of the seed chain, so sixteen
  // lobbies are sixteen different maps and lobby 0 keeps the seed it was
  // given (which is what holds the single-lobby behaviour identical).
  uint32_t lseed = seed;
  for (int l = 0; l < g_srv.lobbies; l++) {
    g_lobby = l;
    app_start(&g_players[0], 16, 16, lseed);
    g_humans_on[0] = 0;   // no humans until a join; there is no "me" here
    lseed = map_seed_next(lseed);
  }
  g_lobby = 0;
  g_srv_started = time(NULL);
  g_srv_uid = seed ^ (uint32_t)g_srv_started * 2654435761u;
  if (!g_srv.name[0]) snprintf(g_srv.name, sizeof g_srv.name, "BLINDDROP");
  net_secret_init();
  g_srv_fd = net_udp_open(g_srv.port);
  if (g_srv_fd < 0) {
    fprintf(stderr, "server: cannot bind UDP port %d (in use?)\n", g_srv.port);
    return 1;
  }
  slog("server port=%d lobbies=%d cap=%d bots=%d skill=%d fraglimit=%d mode=%s map=%s v=%s",
       g_srv.port, g_srv.lobbies, g_srv.cap_public, g_srv.cap_bots,
       g_cfg.sp_diff, g_cfg.sp_frag, GM_KEYS[g_cfg.sp_mode], map_pick_key(g_cfg.sp_map),
       BUILD_VERSION);
  // Fixed 120 Hz on the absolute clock: sleep-until beats sleep-for (no
  // drift), and the backlog is MEASURED (worst `behind` goes into the
  // digest) instead of silently eaten. ~13 µs of sim per tick, 0.16% of a
  // core — the VPS bottleneck is bandwidth, never CPU.
  struct timespec next;
  clock_gettime(CLOCK_MONOTONIC, &next);
  long ticks = 0, next_digest = TICK_HZ * 10;
  while (!g_srv_stop) {
    // Catch-up detection BEFORE the consume: waking from a stall inside the
    // nanosleep must not let the first tick's backlog drop eat the queued
    // inputs the following catch-up ticks are about to replay.
    {
      struct timespec nw;
      clock_gettime(CLOCK_MONOTONIC, &nw);
      long late0 = (nw.tv_sec - next.tv_sec) * 1000000000L +
                   (nw.tv_nsec - next.tv_nsec);
      g_srv_catchup = late0 > (long)(1e9 / TICK_HZ);
    }
    net_srv_poll();                    // ONE socket, routed into the arenas
    // EVERY ARENA TICKS, IN ORDER. The body is exactly the single-arena loop
    // that was here before; only the g_lobby assignment and the rebalance are
    // new. Measured 6.1 us per populated arena tick, so sixteen of them spend
    // ~1.2% of the 8.33 ms budget — the bottleneck is upstream bandwidth.
    for (int l = 0; l < g_srv.lobbies; l++) {
      g_lobby = l;
      // g_cfg.sp_bots is global while match_t is not migrated, so the
      // arena's own bot target has to be re-established before its tick reads
      // it — without this, lobby 15's rebalance decides lobby 0's bot count.
      net_srv_rebalance();
      server_tick_once(NULL);
      if (ticks % 2 == 0) net_srv_broadcast();        // snapshots at 60 Hz
      if (ticks % 30 == 0) net_srv_scores_broadcast(); // score rows at 4 Hz
      if (ticks % 6 == 0) net_srv_rescue_broadcast();  // a mission's state at 20 Hz
      if (ticks % TICK_HZ == 0)        // reset the per-second input budget
        for (int h = 0; h < MAX_HUMANS; h++) g_srv_in_budget[h] = 0;
    }
    g_lobby = 0;
    ticks++;
    if (g_srv.ticks && ticks >= g_srv.ticks) break;
    if (ticks >= next_digest) {
      // ONE LINE PER OCCUPIED ARENA — diagnostics, not statistics. The old
      // `stat arenas=/players=` rollup that stood beside it is GONE with the
      // report rows it fed (peak players, peak arenas): the report is playtime
      // only now, and a heartbeat nothing reads is log volume nothing reads.
      // net_digest itself is untouched and still reads the CURRENT lobby —
      // that is what keeps the `netstat` proof (which calls the same
      // function) byte-identical.
      for (int l = 0; l < g_srv.lobbies; l++) {
        g_lobby = l;
        int humans = 0;
        for (int h = 0; h < MAX_HUMANS; h++) if (g_humans_on[h]) humans++;
        if (!humans) continue;
        char dg[160];
        net_digest(dg, sizeof dg);
        slog("lobby %d %s", l, dg);
      }
      g_lobby = 0;
      next_digest += TICK_HZ * 10;
    }
    next.tv_nsec += (long)(1e9 / TICK_HZ);
    while (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long late_ns = (now.tv_sec - next.tv_sec) * 1000000000L +
                   (now.tv_nsec - next.tv_nsec);
    if (late_ns > 0) {
      long behind = late_ns / (long)(1e9 / TICK_HZ);
      if (behind > g_srv_behind) g_srv_behind = behind;
      // More than a second behind (a laptop lid, a VM pause): jump the clock
      // instead of fast-forwarding 120+ silent catch-up ticks per second.
      if (behind > TICK_HZ) next = now;
    } else {
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
  }
  char dg[160];
  net_digest(dg, sizeof dg);
  slog("%s", dg);
  return 0;
}

int main(int argc, char **argv) {
  int w = 1280, h = 720;
  uint32_t seed = DEFAULT_SEED;
  char *script = NULL;
  int print_cfg = 0, have_seed = 0;

  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const char *arg = i + 1 < argc ? argv[i + 1] : NULL;
    if (!strcmp(a, "--help")) { usage(); return 0; }
    else if (!strcmp(a, "--print-config")) print_cfg = 1;
    else if (!strcmp(a, "--server")) g_server = 1;
    else if (arg && !strcmp(a, "--port"))       g_srv.port = atoi(argv[++i]);
    else if (arg && !strcmp(a, "--cap-bots"))    g_srv.cap_bots = atoi(argv[++i]);
    else if (arg && !strcmp(a, "--cap-public"))  g_srv.cap_public = atoi(argv[++i]);
    else if (arg && !strcmp(a, "--lobbies"))    g_srv.lobbies = atoi(argv[++i]);
    else if (arg && !strcmp(a, "--skill"))      g_srv.skill = atoi(argv[++i]);
    else if (arg && !strcmp(a, "--fraglimit"))  g_srv.fraglimit = atoi(argv[++i]);
    else if (arg && !strcmp(a, "--rounds"))     g_srv.rounds = atoi(argv[++i]);
    else if (!strcmp(a, "--ballistics"))        g_ballistics_srv = 1;
    else if (arg && !strcmp(a, "--map")) {
      int m = map_parse(argv[++i]);
      if (m < 0) { fprintf(stderr, "unknown --map '%s' (arena|foundry|bazaar|outpost|citadel|harbor|refinery|compound|rotate)\n", argv[i]); return 1; }
      g_srv.map = m;
    }
    else if (arg && !strcmp(a, "--mode")) {
      int m = gm_parse(argv[++i]);
      if (m < 0) {
        fprintf(stderr, "unknown --mode '%s' (dm|gun|sniper|blades|lms|rescue|tdm)\n", argv[i]);
        return 1;
      }
      g_srv.mode = m;
    }
    else if (arg && !strcmp(a, "--squad")) {
      g_srv.squad = atoi(argv[++i]);
      if (g_srv.squad < 1 || g_srv.squad > RESC_SQUAD_MAX) {
        fprintf(stderr, "--squad must be 1..%d\n", RESC_SQUAD_MAX); return 1;
      }
    }
    else if (arg && !strcmp(a, "--ticks"))      g_srv.ticks = atol(argv[++i]);
    else if (arg && !strcmp(a, "--team-size")) {
      g_srv.team_size = atoi(argv[++i]);
      if (g_srv.team_size < 1 || g_srv.team_size > TEAM_SIZE_MAX) {
        fprintf(stderr, "--team-size must be 1..%d\n", TEAM_SIZE_MAX); return 1;
      }
    }
    else if (arg && !strcmp(a, "--team-fill"))  g_srv.team_fill = atoi(argv[++i]) ? 1 : 0;
    else if (arg && !strcmp(a, "--friendly-fire")) g_srv.team_ff = atoi(argv[++i]) ? 1 : 0;
    else if (arg && !strcmp(a, "--name")) {     // sanitized like a player name
      const char *q = argv[++i]; int k = 0;
      for (; *q && k < (int)sizeof g_srv.name - 1; q++) {
        char g = *q == ' ' ? ' ' : name_glyph(*q);
        if (g) g_srv.name[k++] = g;
      }
      g_srv.name[k] = '\0';
    }
    else if (!strcmp(a, "--net-debug"))         g_net_debug = 1;  // game.log: one
                                                // line per trim/skip, see decl
    else if (arg && !strcmp(a, "--connect"))    g_connect_host = argv[++i];  // HOST[:PORT]
    else if (arg && !strcmp(a, "--w")) w = atoi(argv[++i]);
    else if (arg && !strcmp(a, "--h")) h = atoi(argv[++i]);
    else if (arg && !strcmp(a, "--seed")) { seed = (uint32_t)strtoul(argv[++i], NULL, 10); have_seed = 1; }
    else if (!strcmp(a, "--windowed")) g_fullscreen = 0;
    else if (arg && !strcmp(a, "--config")) snprintf(g_cfg_path, sizeof g_cfg_path, "%s", argv[++i]);
    else if (arg && !strcmp(a, "--do")) script = strdup(argv[++i]);
    else if (arg && !strcmp(a, "--script")) {
      FILE *f = fopen(argv[++i], "rb");
      if (!f) { fprintf(stderr, "cannot read %s\n", argv[i]); return 1; }
      fseek(f, 0, SEEK_END);
      long len = ftell(f);
      fseek(f, 0, SEEK_SET);
      if (len <= 0) { fprintf(stderr, "cannot size %s (pipe? empty?)\n", argv[i]); fclose(f); return 1; }
      script = malloc((size_t)len + 1);
      if (!script || fread(script, 1, (size_t)len, f) != (size_t)len) { fclose(f); return 1; }
      script[len] = '\0';
      fclose(f);
    } else { fprintf(stderr, "unknown arg '%s' (--help)\n", a); return 1; }
  }

  // Clamp the FBO size: >=1 for GL, <=16384 keeps every allocation sane and
  // the screenshot's single IDAT chunk under the PNG 2^31-1 length cap.
  w = w < 1 ? 1 : w > 16384 ? 16384 : w;
  h = h < 1 ? 1 : h > 16384 ? 16384 : h;

  if (!g_cfg_path[0]) {
    char dir[480];
    exe_dir(dir, sizeof dir);
    snprintf(g_cfg_path, sizeof g_cfg_path, "%sconfig.cfg", dir);
  }
  config_load_or_create(g_cfg_path, &g_cfg);
  if (print_cfg) {
    printf("config %s\n", g_cfg_path);
    config_emit(stdout, &g_cfg);   // the SAME emitter config.cfg is written with
    return 0;
  }

  // THE SERVER FORK: before the play/harness split — a server is neither.
  // Random seed unless pinned, same contract as play mode.
  if (g_server) {
    if (script) { fprintf(stderr, "--server and --do/--script are exclusive\n"); return 1; }
    if (!have_seed) seed = seed_from_clock();
    return server_main(seed);
  }

  // THE FORK. No script means PLAY: this binary is the game, and the harness is
  // something you have to ask for. The two are mutually exclusive right here,
  // which is what lets every proof command keep its determinism without a
  // single lock — a harness session has no window, no audio thread and no
  // second source of ticks, and a played session prints nothing.
  if (!script) {
    // Singleplayer contract: every launch gets a fresh procedural arena, unless
    // --seed asked for a specific one (which is worth keeping in native mode —
    // "play the arena that test reproduced" is a real thing to want).
    if (!have_seed) seed = seed_from_clock();
    g_boot_connect = g_connect_host;   // native_main kicks the join after start
    return native_main(w, h, seed);
  }

  g_headless = 1;
  egl_init();
  fbo_init(w, h);
  player_t *player_s = &g_players[0];   // human 0 IS the local player (P1-1)
  app_start(player_s, w, h, seed);
  sim_ctx_t sim = {.p = player_s};

  run_script(script, &sim);
  free(script);
  return 0;
}

