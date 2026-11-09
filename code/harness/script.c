// code/harness/script.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
static void run_script(char *script, sim_ctx_t *s) {
  g_tok_pushback = NULL;
  for (char *t = strtok(script, " \t\r\n;"); t; t = take_tok()) {
    if (!strcmp(t, "wait")) {
      long n = next_n();
      while (n-- > 0) sim_advance(s);
    } else if (!strcmp(t, "bothear")) {
      bothear_proof();
    } else if (!strcmp(t, "botweapon")) {
      botweapon_proof();
    } else if (!strcmp(t, "perfbench")) {
      // perfbench N: N frames of the live match, each one sim tick + a full
      // render, timed with a GPU timer query and a glFinish so every number is
      // that frame's own (no pipelining). Prints avg / p50 / p95 / max of the
      // sim CPU, the render submission CPU and the GPU. Run on the TARGET:
      //   ./game --do "bots 20; skill hard; restart; wait 600; perfbench 600"
      long n = next_n();
      if (n < 1) n = 1;
      if (n > 4000) n = 4000;
      float *sm = malloc(sizeof(float) * 3 * (size_t)n);
      if (!sm) { fprintf(stderr, "perfbench: out of memory\n"); exit(1); }
      float *cp = sm + n, *gp = cp + n;
      GLuint q = 0;
      int gq = gl_gen_queries && gl_begin_query && gl_end_query && gl_query_u64;
      if (gq) gl_gen_queries(1, &q);
      for (long i = 0; i < n; i++) {
        double t0 = perf_now();
        sim_advance(s);
        double t1 = perf_now();
        if (gq) gl_begin_query(GL_TIME_ELAPSED_Q, q);
        render_frame(s->p, 1.0f, 120, &s->cam);
        if (gq) gl_end_query(GL_TIME_ELAPSED_Q);
        double t2 = perf_now();
        glFinish();
        double t3 = perf_now();
        GLuint64 ns = 0;
        if (gq) gl_query_u64(q, GL_QUERY_RESULT_Q, &ns);
        sm[i] = (float)((t1 - t0) * 1e3);
        cp[i] = (float)((t2 - t1) * 1e3);
        gp[i] = gq ? (float)((double)ns * 1e-6) : (float)((t3 - t2) * 1e3);
      }
      const char *nm[3] = {"sim", "render_cpu", gq ? "gpu" : "gpu_wait"};
      float *arr[3] = {sm, cp, gp};
      for (int k = 0; k < 3; k++) {
        float *a = arr[k], sum = 0, mx = 0;
        for (long i = 0; i < n; i++) { sum += a[i]; if (a[i] > mx) mx = a[i]; }
        // Insertion sort is fine at n <= 4000 for a one-shot report.
        for (long i = 1; i < n; i++) {
          float v = a[i]; long j = i - 1;
          while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; }
          a[j + 1] = v;
        }
        printf("perfbench %-10s avg=%.3f p50=%.3f p95=%.3f max=%.3f ms\n", nm[k],
               (double)(sum / (float)n), (double)a[n / 2], (double)a[(n * 95) / 100],
               (double)mx);
      }
      printf("perfbench frames=%ld res=%dx%d budget=8.333 ms (120 Hz)\n", n, R.width, R.height);
      free(sm);
    } else if (!strcmp(t, "hurt")) {
      // hurt N: N damage to the local player from bot 0's position (the
      // death/damage states for the visual capture set).
      long n = next_n();
      v3 at = g_players[0].pos; at.y += 1.2f;
      ent_damage(g_local_ent, (int)n, 0, 1, g_bots[0].pos, at);
    } else if (!strcmp(t, "spawncheck")) {
      // Deaths within 3 s of a respawn, over N ticks of whatever match is set
      // up (the local player is excluded: in a script it stands still).
      long n = next_n();
      memset(g_sc_spawn, 0, sizeof g_sc_spawn);
      g_sc_deaths = g_sc_quick = g_sc_slow = 0;
      while (n-- > 0) sim_advance(s);
      printf("spawncheck deaths=%d within1s=%d pct1=%.1f within3s=%d pct3=%.1f\n",
             g_sc_deaths, g_sc_quick, g_sc_deaths ? 100.0 * g_sc_quick / g_sc_deaths : 0.0,
             g_sc_slow, g_sc_deaths ? 100.0 * g_sc_slow / g_sc_deaths : 0.0);
    } else if (!strcmp(t, "meleecheck")) {
      meleecheck_proof();
    } else if (!strcmp(t, "blastcheck")) {
      blastcheck_proof();
    } else if (!strcmp(t, "botflank")) {
      botflank_proof();
    } else if (!strcmp(t, "botmemory")) {
      botmemory_proof();
    } else if (!strcmp(t, "botmemoryobserve")) {
      botmemoryobserve_proof();
    } else if (!strcmp(t, "padbackend")) {
      padbackend_proof();
    } else if (t[0] == '+' || t[0] == '-') {
      s->held[action_from_name(t + 1)] = t[0] == '+';
    } else if (!strcmp(t, "tap")) {
      int a = action_from_name(next_tok());
      s->held[a] = 1;
      sim_advance(s);
      s->held[a] = 0;
    } else if (!strcmp(t, "look")) {
      float dx = next_f(), dy = next_f();
      player_look(s->p, dx, dy, g_cfg.sense);
    } else if (!strcmp(t, "home")) {
      // The BOOT SCREEN, on demand. It is the one screen the harness cannot
      // reach through `menu` alone, because the native backends are what turn
      // it on — and it is also the one screen whose whole content is a moving
      // camera, so `home on; wait N; shot` is how any frame of it gets looked
      // at. `home T` jumps the director's clock to T seconds, which is how a
      // specific shot gets photographed without waiting for it.
      char *ha = next_tok();
      if (!strcmp(ha, "off")) {
        g_home = 0;
      } else {
        if (!g_menu_open) { g_ui_cx = g_ui_cy = -40.0f; ui_open_menu(); }
        ui_route(UI_PAGE_ROOT);
        home_enter(s->p);
        if (strcmp(ha, "on")) {
          // ...AND STAGE AGAIN AFTERWARDS. home_enter stages for whatever loop
          // the clock is on, which is loop 0, and the jump then lands the
          // director in a different one — so `home 52` framed loop 1's shots
          // against loop 0's corner and photographed a wall with the soldier
          // somewhere off the side of it. In the game that never happens
          // (home_camera re-stages at every cut, under the dip), so this was a
          // defect in the instrument alone: every framing checked through
          // `home T` at a T past the first loop was checked against the wrong
          // arena corner.
          g_home_t0 = g_ui_time - (float)atof(ha);
          home_stage(s->p);
        }
      }
    } else if (!strcmp(t, "menu")) {
      // The native backends seed the software cursor from the real pointer on
      // menu open; headless has no pointer, and the unseeded (0,0) default
      // painted a clipped white cursor fragment into the corner of every menu
      // screenshot. Park it offscreen — `cursor X Y` places it deliberately.
      if (!g_menu_open) g_ui_cx = g_ui_cy = -40.0f;
      ui_on_escape();  // same semantics as ESC in the game
    } else if (!strcmp(t, "cursor")) {
      g_ui_cx = next_f();
      g_ui_cy = next_f();
    } else if (!strcmp(t, "wheel")) {
      // One notch of the mouse wheel, consumed by the next `appframe`. The
      // latch is set regardless of the menu, so `menu; wheel up; appframe`
      // proves input_wheel_swap's OWN gate rather than a second copy of it
      // in this command. (Both backends also refuse to latch behind a menu,
      // and clear every latch on the toggle — nat_input_clear/win_input_clear.)
      char *d = next_tok();
      g_pressed[strcmp(d, "down") ? KEY_MWUP : KEY_MWDN] = 1;
    } else if (!strcmp(t, "mdown")) {
      g_ui_mdown = 1;
    } else if (!strcmp(t, "mup")) {
      g_ui_mdown = 0;
    } else if (!strcmp(t, "key")) {
      char *tok = next_tok();
      if (UI_TEXT_EDIT) ui_name_key(tok);  // a text field has key focus
      else ui_assign_bind(tok);
    } else if (!strcmp(t, "uiframe")) {
      render_frame(s->p, 1.0f, 120, &s->cam);
      if (g_ui_quit) puts("ui: quit");
    } else if (!strcmp(t, "appframe")) {
      // N frames through the REAL shared frame step — the only headless caller
      // of app_frame, and therefore the proof of the menu-pause contract:
      // `menu; appframe 3` must run 0 ticks, closing the menu resumes with no
      // catch-up burst. dt = TICK_DT drains exactly one tick per call, so the
      // count is deterministic. Events route through the live audio path here
      // (voices scheduled, never mixed) — keep it out of mixdown batteries.
      long n = next_n();
      static app_t app;
      // The script's own held state feeds through, exactly like wait/trace —
      // an empty input here would silently RELEASE every +action for the
      // duration and re-arm its press edge after, i.e. the battery would
      // measure an input sequence the game never ran.
      // ...and the BOOT SCREEN's director is the second producer of input in
      // this file (frame_input_keys is the first). It has to be asked here too,
      // and asked PER FRAME: sampled once before the loop, a 0.2-second beat
      // that starts anywhere inside an `appframe N` never happens at all, and
      // the screen photographs a weapon that never moves — which is the one
      // thing the screen is for. The script's own held state is constant across
      // the call by design (see above); only the director's finger moves.
      // ...and the MOUSE WHEEL is the third: `wheel up` leaves the same latch
      // a notch does, read through the same predicate frame_input_keys reads,
      // and cleared on the same terms app_pump clears it on — only once a tick
      // has actually consumed it, or a notch that lands in a sub-tick gap is
      // dropped. That is what makes this path a proof of the real one rather
      // than a second copy of its policy.
      int ran = 0;
      while (n-- > 0) {
        frame_input_t in = {0};
        for (int i = 0; i < IN_COUNT; i++)
          in.held[i] = in.held_first[i] = s->held[i] || home_input(i);
        if (input_wheel_swap()) in.held_first[IN_SWAP] = 1;
        int r = app_frame(&app, s->p, TICK_DT, &in);
        if (r) g_pressed[KEY_MWUP] = g_pressed[KEY_MWDN] = 0;
        ran += r;
      }
      s->tick += ran;
      printf("appframe ticks=%d acc=%.4f menu=%d\n", ran, app.acc, g_menu_open);
    } else if (!strcmp(t, "pad")) {
      // Raw pad state, exactly what a platform pump would have written — so
      // every consumer (deadzone, curve, triggers, menu nav, stick viz) runs
      // the real shared-core path, deterministically.
      char *k = next_tok();
      if      (!strcmp(k, "on"))  g_pad.connected = 1;
      else if (!strcmp(k, "off")) memset(&g_pad, 0, sizeof g_pad);
      else if (!strcmp(k, "btn")) {
        char *b = next_tok();
        long v = next_n();
        char tok[16];
        snprintf(tok, sizeof tok, "pad_%s", b);
        int found = 0;
        for (int i = 0; i < PB_COUNT; i++)
          if (!strcmp(tok, PAD_BTN_DEF[i].tok)) { g_pad.btn[i] = v != 0; found = 1; }
        if (!found) { fprintf(stderr, "script: unknown pad button '%s'\n", b); exit(1); }
        // lt/rt as buttons drive the analog value too — pad_btn_sync would
        // otherwise recompute the level from the axis and silently revert.
        if (!strcmp(b, "lt")) g_pad.lt = v ? 1.0f : 0.0f;
        if (!strcmp(b, "rt")) g_pad.rt = v ? 1.0f : 0.0f;
        g_pad.connected = 1;
      } else {
        float v = next_f();
        g_pad.connected = 1;
        if      (!strcmp(k, "lx")) g_pad.lx = v;
        else if (!strcmp(k, "ly")) g_pad.ly = v;
        else if (!strcmp(k, "rx")) g_pad.rx = v;
        else if (!strcmp(k, "ry")) g_pad.ry = v;
        else if (!strcmp(k, "lt")) g_pad.lt = v;
        else if (!strcmp(k, "rt")) g_pad.rt = v;
        else { fprintf(stderr, "script: unknown pad field '%s'\n", k); exit(1); }
      }
    } else if (!strcmp(t, "padscan")) {
      // THE ON-DEVICE PROOF FOR THE MULTI-NODE PUMP, and the one pad proof
      // that cannot be faked. Every other one drives g_pad through the
      // harness's own `pad` command, which is exactly what cannot answer the
      // question this fix is about: does THIS machine's node table contain the
      // pad in my hands, and does the hand-over pick it. It runs the REAL
      // evdev scan against the REAL /dev/input for N frames (default 600 =
      // 5 s, two rescan cadences) and prints the table it converged on plus
      // every hand-over on the way. Linux only — there is no harness on the
      // Windows build to reach the XInput half with.
#ifndef _WIN32
      nat_pad_report((int)opt_n(600));
#else
      printf("padscan unavailable on this backend\n");
#endif
    } else if (!strcmp(t, "padsync")) {
      pad_sync(TICK_DT);
    } else if (!strcmp(t, "padlook")) {
      // N ticks of right-stick look. 120 ticks at full deflection must print
      // dyaw == pad_sens_x — the sens unit IS this proof. (With aim assist on
      // AND a live bot in the bubble, friction scales it: rate proofs run
      // against a pad_assist 0 config or an empty cone.)
      long n = next_n();
      float y0 = s->p->yaw, p0 = s->p->pitch;
      while (n-- > 0) pad_look(s->p, TICK_DT);
      printf("padlook dyaw=%.3f dpitch=%.3f yaw=%.4f pitch=%.4f\n",
             (double)((s->p->yaw - y0) / DEG2RAD),
             (double)((s->p->pitch - p0) / DEG2RAD),
             (double)s->p->yaw, (double)s->p->pitch);
    } else if (!strcmp(t, "padstat")) {
      // The pad pipeline's proof: deadzoned move/aim vectors, the curve's
      // output, the ADS look scale, trigger hysteresis levels, and — through
      // the REAL resolve_binds/frame_input_keys — which actions the pad holds.
      float mx, my, ax, ay;
      float mm = pad_deadzone(g_pad.lx, g_pad.ly, g_cfg.pad[PAD_DZ_L], &mx, &my);
      float am = pad_deadzone(g_pad.rx, g_pad.ry, g_cfg.pad[PAD_DZ_R], &ax, &ay);
      printf("pad conn=%d move=(%.3f %.3f) m=%.3f aim=(%.3f %.3f) m=%.3f "
             "curve=%s out=%.3f adsk=%.3f lt=%.2f rt=%.2f ltb=%d rtb=%d btn=",
             g_pad.connected, (double)mx, (double)my, (double)mm,
             (double)ax, (double)ay, (double)am,
             PAD_CURVE_OPTS[g_cfg.pad_curve],
             (double)pad_curve_apply(am, g_cfg.pad_curve),
             (double)pad_ads_look_scale(s->p),
             (double)g_pad.lt, (double)g_pad.rt,
             g_pad.btn[PB_LT], g_pad.btn[PB_RT]);
      int any = 0;
      for (int i = 0; i < PB_COUNT; i++)
        if (g_pad.btn[i]) printf("%s%s", any++ ? "," : "", PAD_BTN_DEF[i].tok + 4);
      if (!any) printf("-");
      {
        // Aim-assist probe, independent of stick state: which ENTITY the
        // bubble would engage (id, not bot index, since P1-56), the angular
        // offset vs the cone, the target's distance, and the friction the
        // look rate would take.
        v3 atp;
        float aang = 0, aeng = 0, adist = 0;
        int at = pad_assist_target(s->p, &atp, &aang, &aeng, &adist);
        float fric = at >= 0 ? pad_assist_friction(aang, aeng, adist) : 1.0f;
        printf(" assist=%d tgt=%d ang=%.2f eng=%.2f dist=%.1f fric=%.2f",
               g_cfg.pad_assist, at,
               (double)(aang / DEG2RAD), (double)(aeng / DEG2RAD),
               (double)adist, (double)fric);
      }
      int kb[IN_COUNT], pb[IN_COUNT];
      resolve_binds(kb, pb);
      frame_input_t fin = {0};
      frame_input_keys(&fin, kb, pb);
      printf(" held=");
      any = 0;
      for (int i = 0; i < IN_COUNT; i++)
        if (fin.held[i]) printf("%s%s", any++ ? "," : "", ACTION_DEF[i].script);
      if (!any) printf("-");
      printf("\n");
    } else if (!strcmp(t, "padcurve")) {
      // Every curve, tabulated: endpoints must be 0 and 1 and each row must be
      // monotone — the proof that no curve changes top speed, only the travel.
      for (int c = 0; c < 3; c++) {
        printf("padcurve %-8s", PAD_CURVE_OPTS[c]);
        for (int i = 0; i <= 10; i++)
          printf(" %.3f", (double)pad_curve_apply((float)i / 10.0f, c));
        printf("\n");
      }
    } else if (!strcmp(t, "nav")) {
      char *k = next_tok();
      static const char *const NAVN[8] = {"up", "down", "left", "right",
                                          "ok", "back", "tabl", "tabr"};
      int hit = -1;
      for (int i = 0; i < 8; i++)
        if (!strcmp(k, NAVN[i])) hit = i;
      if (hit < 0) { fprintf(stderr, "script: unknown nav '%s'\n", k); exit(1); }
      nav_push(hit);
    } else if (!strcmp(t, "uistat")) {
      // qarm/nedit/kb: the MODAL states (armed QUIT confirm, name capture, the
      // on-screen keyboard) — without them the harness could only assert any of
      // them by pixel-diffing. kb prints the CURSOR as well as the flag: the
      // grid's whole state is that pair, so a nav proof needs no screenshot.
      printf("ui menu=%d tab=%d page=%s control=%s padsub=%d devsub=%d wpn=%d focus=%d/%d padmode=%d "
             "rebind=%d rebindpad=%d arm=%d nedit=%d kb=%d kbcur=%d,%d name=%s dev=%d curve=%s "
             "dz=(%.2f %.2f) sens=%.0f "
             "adss=%.2f assist=%d inv=%d conn=%d\n",
             g_menu_open, g_ui_tab, ui_page_name(g_ui_page),
             g_ui_control ? "gamepad" : "keyboard",
             g_ui_pad_sub, g_ui_dev_sub, g_ui_wpn,
             g_ui_focus, g_ui_nfocus_last, g_ui_pad_mode, g_ui_rebind,
             g_ui_rebind_pad, (int)g_ui_arm, g_ui_name_edit,
             g_ui_kb, g_ui_kb_r, g_ui_kb_c, g_cfg.name,
             DEV_MENU && g_cfg.devmode, PAD_CURVE_OPTS[g_cfg.pad_curve],
             (double)g_cfg.pad[PAD_DZ_L], (double)g_cfg.pad[PAD_DZ_R],
             (double)g_cfg.pad[PAD_SENS],
             (double)g_cfg.pad[PAD_ADS_SENS], g_cfg.pad_assist,
             g_cfg.pad_inv_y, g_pad.connected);
    } else if (!strcmp(t, "netloop")) {
      // THE Phase-2 proof: the same N ticks once through sim_advance and once
      // through the server loop's own tick body, from an identical saved sim
      // state, per-tick hash compared — MISMATCH prints the first divergent
      // tick. "36 000 ticks ran" was no proof: an output that cannot move is
      // not a proof (parity's own rule). VFX and the per-human anim/corpse
      // state are deliberately NOT restored — the sim never reads presentation
      // back, which this equality also demonstrates. BOT anim is not in that
      // set: anim_t sits inside bot_t, so it is saved, restored and hashed with
      // the rest of the bot, and a solver that diverged would show up here.
      long n = next_n();
      if (n > 4096) n = 4096;
      struct {
        player_t players[MAX_HUMANS];
        bot_t bots[MAX_BOTS];
        match_t match;
        long tick;
        uint64_t rng, rng_gun[MAX_HUMANS], rng_bot;
        uint32_t map_seed;
        int map_id;
        unsigned char on[MAX_HUMANS];
      } sv;
      memcpy(sv.players, g_players, sizeof g_players);
      memcpy(sv.bots, g_bots, sizeof g_bots);
      sv.match = g_match;
      sv.tick = g_tick;
      sv.rng = g_rng; sv.rng_bot = g_rng_bot;
      memcpy(sv.rng_gun, g_rng_gun, sizeof g_rng_gun);
      sv.map_seed = g_map_seed;
      sv.map_id = g_map_id;
      memcpy(sv.on, g_humans_on, sizeof g_humans_on);
      static uint32_t ha[4096], hb[4096];
      #define NL_HASH(dst) do {                                             \
        uint32_t fh = 2166136261u;                                          \
        const unsigned char *pp;                                            \
        pp = (const unsigned char *)g_players;                              \
        for (size_t z = 0; z < sizeof g_players; z++) fh = (fh ^ pp[z]) * 16777619u; \
        pp = (const unsigned char *)g_bots;                                 \
        for (size_t z = 0; z < sizeof g_bots; z++) fh = (fh ^ pp[z]) * 16777619u;    \
        pp = (const unsigned char *)&g_match;                               \
        for (size_t z = 0; z < sizeof g_match; z++) fh = (fh ^ pp[z]) * 16777619u;   \
        (dst) = fh;                                                         \
      } while (0)
      for (long i = 0; i < n; i++) { sim_advance(s); NL_HASH(ha[i]); }
      // restore — map first (a fraglimit restart advances the seed and
      // regenerates solids; map_generate also reseeds every RNG stream),
      // then the exact RNG/entity/match state over it.
      g_map_id = sv.map_id;
      map_generate(sv.map_seed);
      g_map_seed = sv.map_seed;
      memcpy(g_players, sv.players, sizeof g_players);
      memcpy(g_bots, sv.bots, sizeof g_bots);
      g_match = sv.match;
      g_tick = sv.tick;
      g_rng = sv.rng; g_rng_bot = sv.rng_bot;
      memcpy(g_rng_gun, sv.rng_gun, sizeof g_rng_gun);
      memcpy(g_humans_on, sv.on, sizeof g_humans_on);
      g_num_events = 0;
      for (long i = 0; i < n; i++) { server_tick_once(s->held); NL_HASH(hb[i]); }
      #undef NL_HASH
      long bad = -1;
      for (long i = 0; i < n && bad < 0; i++)
        if (ha[i] != hb[i]) bad = i;
      if (bad >= 0)
        printf("netloop n=%ld MISMATCH at tick %ld (sim %08x vs server %08x)\n",
               n, bad, ha[bad], hb[bad]);
      else
        printf("netloop n=%ld ok hash=%08x\n", n, n ? netloop_report_hash() : 0);
      s->tick += n;   // the second pass advanced the world again
    } else if (!strcmp(t, "netpack")) {
      // Encode/decode round-trip over random states, worst error PER FIELD.
      // A quantization error is invisible until someone's aim is 0.2 deg off,
      // so the number has to be printed, not asserted away. Deterministic PRNG
      // (seeded, no wall clock) so the proof reproduces.
      long n = next_n();
      uint64_t rs = 0x1234abcd ^ (uint64_t)n * 2654435761u;
      float e_yaw = 0, e_pitch = 0, e_wish = 0, e_pos = 0, e_vel = 0;
      int roundtrip_fail = 0;
      for (long i = 0; i < n; i++) {
        float fy = (rng_next(&rs) / 4294967296.0f - 0.5f) * F_TAU;
        float fp = (rng_next(&rs) / 4294967296.0f - 0.5f) * 2.0f * PITCH_MAX;
        float fw = rng_next(&rs) / 4294967296.0f * 2.0f - 1.0f;
        float fx = (rng_next(&rs) / 4294967296.0f - 0.5f) * 640.0f;
        float fv = (rng_next(&rs) / 4294967296.0f - 0.5f) * 640.0f;
        net_input_t in = {.tick = (uint32_t)i, .view = rng_next(&rs),
                          .buttons = (uint32_t)rng_next(&rs),
                          .loadout = (uint8_t)loadout_pack(i % WPN_VARS, i / WPN_VARS % WPN_VARS),
                          .yaw = net_q_yaw(fy), .pitch = net_q_pitch(fp),
                          .wish_x = net_q_wish(fw), .wish_y = net_q_wish(-fw)};
        unsigned char buf[64];
        np_wr w = {buf, sizeof buf, 0, 0};
        net_input_encode(&w, &in);
        np_rd r = {buf, w.at, 0, 0};   // record = fields only, cursor at 0
        net_input_t out;
        if (!net_input_decode(&r, &out) || out.tick != in.tick ||
            out.view != in.view ||
            out.buttons != (in.buttons & ((1u << IN_COUNT) - 1u)) ||
            out.loadout != in.loadout ||   // rides the buttons word's top nibble
            out.yaw != in.yaw) roundtrip_fail = 1;
        float dy = fabsf(net_dq_yaw(out.yaw) - wrap_pi(fy));
        float dp = fabsf(net_dq_pitch(out.pitch) - f_clamp(fp, -PITCH_MAX, PITCH_MAX));
        float dw = fabsf(net_dq_wish(out.wish_x) - f_clamp(fw, -1.0f, 1.0f));
        if (dy > e_yaw) e_yaw = dy;
        if (dp > e_pitch) e_pitch = dp;
        if (dw > e_wish) e_wish = dw;
        net_ent_t ne = {.id = (uint8_t)(rng_next(&rs) % MAX_ENTS), .flags = 0xf,
                        .hp = 100, .weapon = (uint8_t)(i % WPN_COUNT), .spawn_seq = 3,
                        .loadout = (uint8_t)loadout_pack(i / 3 % WPN_VARS, i / 9 % WPN_VARS),
                        .x = net_q_pos(fx), .y = net_q_pos(fx * 0.5f), .z = net_q_pos(-fx),
                        .vx = net_q_vel(fv), .vy = net_q_vel(0), .vz = net_q_vel(-fv),
                        .yaw = net_q_yaw(fy), .pitch = net_q_pitch(fp),
                        .lean = net_q_wish(fw)};
        np_wr w2 = {buf, sizeof buf, 0, 0};
        net_ent_encode(&w2, &ne);
        np_rd r2 = {buf, w2.at, 0, 0};
        net_ent_t neo;
        if (!net_ent_decode(&r2, &neo) || neo.id != ne.id || neo.hp != ne.hp ||
            neo.weapon != ne.weapon || neo.loadout != ne.loadout)
          roundtrip_fail = 1;
        float dpos = fabsf(net_dq_pos(neo.x) - f_clamp(fx, -327.0f, 327.0f));
        float dvel = fabsf(net_dq_vel(neo.vx) - f_clamp(fv, -327.0f, 327.0f));
        if (dpos > e_pos) e_pos = dpos;
        if (dvel > e_vel) e_vel = dvel;
      }
      printf("netpack n=%ld roundtrip=%s worst_yaw=%.5frad worst_pitch=%.5frad "
             "worst_wish=%.5f worst_pos=%.4fm worst_vel=%.4fm/s\n",
             n, roundtrip_fail ? "FAIL" : "ok",
             (double)e_yaw, (double)e_pitch, (double)e_wish,
             (double)e_pos, (double)e_vel);
    } else if (!strcmp(t, "netfuzz")) {
      netfuzz_proof();
    } else if (!strcmp(t, "netpredict")) {
      // The whole snapshot path, in one process: build a snapshot from live
      // sim state, encode, decode, and check the reconstructed entity
      // positions match the live ones within quantization (1 cm). Then apply
      // the SAME snapshot bytes twice and confirm the second yields identical
      // records — the dedup property that stops a resent packet
      // double-spawning VFX or double-counting a kill. Output moves (worst
      // error in mm), so it is a proof, not a ritual.
      long n = next_n();
      // Capture INSIDE the loop: the harness drains its event list every
      // advance, so capturing once at the end sees one tick's worth and the
      // section round-trips empty — a proof that passes without proving.
      for (long i = 0; i < n; i++) sim_advance(s);
      static unsigned char snap[2048];
      // ev_ack 0 = "this client has acknowledged nothing", so every event still
      // inside the resend window rides along and the section is exercised.
      size_t len = net_snapshot_build(snap, sizeof snap, (uint32_t)g_tick, 0, 2);
      net_ent_t a[MAX_ENTS], b[MAX_ENTS];
      net_ev_t ea[NET_EV_SNAP_MAX], eb[NET_EV_SNAP_MAX];
      uint32_t st, li, wh;
      int nea = 0, neb = 0;
      int na = net_snapshot_read(snap, len, &st, &li, &wh, NULL, NULL, NULL, a, MAX_ENTS,
                                 NULL, ea, NET_EV_SNAP_MAX, &nea);
      int nb = net_snapshot_read(snap, len, NULL, NULL, NULL, NULL, NULL, NULL, b, MAX_ENTS,
                                 NULL, eb, NET_EV_SNAP_MAX, &neb);
      float worst_mm = 0.0f;
      int mism = na != nb, live = 0;
      ENT_FOREACH(e) {
        v3 lp = ent_pos(e);
        int found = -1;
        for (int j = 0; j < na; j++) if (a[j].id == e) { found = j; break; }
        if (found < 0) { mism = 1; continue; }
        float dx = net_dq_pos(a[found].x) - lp.x;
        float dz = net_dq_pos(a[found].z) - lp.z;
        float mm = sqrtf(dx * dx + dz * dz) * 1000.0f;
        if (mm > worst_mm) worst_mm = mm;
        live++;
      }
      for (int j = 0; j < na && !mism; j++)
        if (a[j].id != b[j].id || a[j].x != b[j].x || a[j].hp != b[j].hp) mism = 1;
      // The event section round-trips too, and identically on the second read —
      // the ids are what the client dedups on, so an unstable id is a resend
      // that plays its sound twice.
      if (nea != neb) mism = 1;
      for (int j = 0; j < nea && !mism; j++)
        if (ea[j].id != eb[j].id || ea[j].type != eb[j].type ||
            ea[j].ax != eb[j].ax || ea[j].src != eb[j].src) mism = 1;
      printf("netpredict n=%ld ents=%d evs=%d bytes=%zu server_tick=%u world=%08x "
             "dedup=%s worst_pos=%.2fmm\n",
             n, live, nea, len, st, wh, mism ? "MISMATCH" : "stable", (double)worst_mm);
    } else if (!strcmp(t, "netlagcomp")) {
      // Rewind the target N ticks into the past and prove the shot HITS with
      // compensation and MISSES without — both halves, plus the residual
      // distance in mm so the number can move. Covers a LEANING shooter
      // (origin from the leaned eye) and a leaning/strafing target. Staged
      // with a puppet running at run speed so the history has real motion.
      long rewind = opt_n(20);
      // stage: bot 0 as a fast puppet crossing in front, freeze the player
      g_bots_frozen = 0;
      // run enough ticks that the history ring is full of motion
      for (int i = 0; i < 40; i++) { sim_advance(s); }
      // aim the player at where bot 0 WAS `rewind` ticks ago
      long past = g_tick - rewind;
      long slot = past % NET_HIST;
      v3 tpos = g_hist[1][slot].pos;   // entity 1 = bot 0
      v3 eye = player_eye_pos(s->p);
      v3 dir = v3_sub((v3){{tpos.x, tpos.y + g_hist[1][slot].eye - ENT_CHEST_OFF, tpos.z}}, eye);
      float dl = v3_len(dir);
      if (dl > 1e-4f) dir = v3_scale(dir, 1.0f / dl);
      int part_c = -1, part_n = -1;
      float d_c = GUN_RANGE, d_n = GUN_RANGE;
      float hc = net_hist_trace(1, past, 0.0f, eye, dir, &part_c);   // compensated
      // uncompensated: trace against the CURRENT box
      v3 nowoff = {{0, 0, 0}};
      float hn = ent_trace_box(g_bots[0].pos, bot_eye(&g_bots[0]), nowoff,
                               eye, dir, &part_n);
      d_c = hc > 0 ? hc : -1; d_n = hn > 0 ? hn : -1;
      // residual: how far the aimed ray passes from the CURRENT target centre
      v3 cur = {{g_bots[0].pos.x, g_bots[0].pos.y + bot_eye(&g_bots[0]) - ENT_CHEST_OFF,
                 g_bots[0].pos.z}};
      v3 rel = v3_sub(cur, eye);
      float along = v3_dot(rel, dir);
      v3 closest = v3_add(eye, v3_scale(dir, along));
      float resid_mm = v3_len(v3_sub(cur, closest)) * 1000.0f;
      printf("netlagcomp rewind=%ld comp_hit=%s uncomp_hit=%s resid=%.1fmm "
             "(past tick %ld, now %ld)\n",
             rewind, d_c > 0 ? "HIT" : "miss", d_n > 0 ? "HIT" : "miss",
             (double)resid_mm, past, g_tick);
    } else if (!strcmp(t, "netstall")) {
      // The input-queue bounds, proven against the LIVE queue machinery (the real
      // net_srv_queue_input + net_srv_consume_inputs on a scratch connection),
      // not against a copy of the rules. Four properties: redundancy resends
      // are FREE (no budget), a gap HOLDS the last applied state (fill rule),
      // a same-second burst caps at 1.25x real-time, and a far-ahead jump
      // keeps only the NET_IN_MAX window and counts what it skipped.
      long gap = opt_n(60);
      net_conn_t csave = g_conns[0];
      int bsave = g_srv_in_budget[1];
      unsigned dsave = g_srv_dropped_inputs;
      int hsave[IN_COUNT]; memcpy(hsave, g_human_held[1], sizeof hsave);
      float ysave = g_players[1].yaw, psave = g_players[1].pitch;
      memset(&g_conns[0], 0, sizeof g_conns[0]);
      g_conns[0].used = 1; g_conns[0].human = 1;
      g_srv_in_budget[1] = 0;
      net_input_t in = {.buttons = (uint16_t)(1u << IN_FIRE)};
      int fresh = 0;                       // A: ticks 0..3, sent twice
      for (int rep = 0; rep < 2; rep++)
        for (uint32_t tk = 0; tk < 4; tk++) {
          in.tick = tk;
          fresh += net_srv_queue_input(&g_conns[0], 1, &in);
        }
      for (int k = 0; k < 6; k++)          // B: 6 world ticks vs 4 inputs
        net_srv_consume_inputs();
      int hold = g_human_held[1][IN_FIRE]; // ...the 2-tick gap held the state
      uint32_t applied = g_conns[0].last_input_tick;
      unsigned drop0 = g_srv_dropped_inputs;  // C: 200-tick same-second burst
      int acc = 0;
      in.buttons = 0;
      for (uint32_t tk = 4; tk < 204; tk++) {
        in.tick = tk;
        acc += net_srv_queue_input(&g_conns[0], 1, &in);
      }
      int rate_drop = (int)(g_srv_dropped_inputs - drop0);
      memset(&g_conns[0], 0, sizeof g_conns[0]);   // D: far-ahead jump
      g_conns[0].used = 1; g_conns[0].human = 1;
      g_srv_in_budget[1] = 0;
      in.tick = 0; net_srv_queue_input(&g_conns[0], 1, &in);
      drop0 = g_srv_dropped_inputs;
      in.tick = (uint32_t)gap + NET_IN_MAX;
      net_srv_queue_input(&g_conns[0], 1, &in);
      int skip = (int)(g_srv_dropped_inputs - drop0);
      // E: the backlog TRIM — the path the live WAN rubber-banding rode, and
      // the one the old netstall never touched. Queue NET_IN_BACKLOG+4 fresh
      // ticks, consume once: the depth must trim to the threshold (oldest
      // first), the trimmed FIRE press must survive through btn_carry, and
      // the trim counter must say so in the digest.
      memset(&g_conns[0], 0, sizeof g_conns[0]);
      g_conns[0].used = 1; g_conns[0].human = 1;
      g_srv_in_budget[1] = 0;
      memset(g_human_held[1], 0, sizeof g_human_held[1]);
      unsigned trim0 = g_srv_trim_drops;
      for (uint32_t tk = 0; tk < (uint32_t)NET_IN_BACKLOG + 4; tk++) {
        in.tick = tk;
        in.buttons = tk == 1 ? (uint16_t)(1u << IN_FIRE) : 0;  // press inside
        net_srv_queue_input(&g_conns[0], 1, &in);              // the trim zone
      }
      net_srv_consume_inputs();
      int trimmed_n = (int)(g_srv_trim_drops - trim0);
      int carry_ok = g_human_held[1][IN_FIRE];   // edge survived the trim
      uint32_t trim_applied = g_conns[0].last_input_tick;
      printf("netstall dup_free=%d applied_tick=%u gap_hold=%s | burst "
             "accepted=%d dropped=%d | jump_skipped=%d | trim trimmed=%d "
             "applied=%u carry=%s (cap=%d backlog=%d rate=%d/s redun=%d)\n",
             fresh, applied, hold ? "ok" : "LOST", acc, rate_drop, skip,
             trimmed_n, trim_applied, carry_ok ? "ok" : "LOST",
             NET_IN_MAX, NET_IN_BACKLOG,
             TICK_HZ * NET_IN_RATE_NUM / NET_IN_RATE_DEN, NET_IN_REDUN);
      g_conns[0] = csave;                  // leave no trace in the session
      g_srv_in_budget[1] = bsave; g_srv_dropped_inputs = dsave;
      memcpy(g_human_held[1], hsave, sizeof hsave);
      g_players[1].yaw = ysave; g_players[1].pitch = psave;
      g_human_move[1] = 1;                 // back to the always-move default
      g_human_view[1] = -1;                // and no stale lag-comp anchor
      g_srv_trim_drops = trim0;            // segment E leaves no trace either
    } else if (!strcmp(t, "netanim")) {
      // Foreign anim_tick from a DECIMATED snapshot stream: run one puppet
      // at 120 Hz counting plants, then re-drive a fresh
      // anim from position samples taken every K ticks (a 60 Hz snapshot with
      // loss) and count plants there. The rule: a cadence claim is measured by
      // COUNTING PLANTS, never by frames. Close counts mean "poses re-solve
      // locally, free" holds; far apart means it needs the fallbacks in open point 7.
      long n = next_n();
      int stride = (int)opt_n(2);      // 2 = 60 Hz from 120 Hz
      if (stride < 1) stride = 1;
      // reference: the puppet already running (bot 0), count its EV_STEP via a
      // plant-edge detector on foot t crossing.
      int plants_full = 0, plants_dec = 0;
      anim_t ra; anim_reset(&ra, g_bots[0].pos, g_bots[0].yaw);
      v3 pprev = g_bots[0].pos;   // yaw comes live from the puppet, not sampled
      int last_stance = -1, last_stance_d = -1;
      for (long i = 0; i < n; i++) {
        sim_advance(s);
        // full-rate re-solve from the live puppet's own state
        int st = g_bots[0].anim.foot[0].t < g_bots[0].anim.foot[1].t ? 0 : 1;
        if (last_stance >= 0 && st != last_stance) plants_full++;
        last_stance = st;
        // decimated: only every `stride` ticks feed a fresh solver, with
        // velocity from the position delta (what a client reconstructs)
        if (i % stride == 0) {
          v3 pos = g_bots[0].pos;
          v3 vel = v3_scale(v3_sub(pos, pprev), (float)TICK_HZ / stride);
          anim_tick(&ra, pos, vel, g_bots[0].yaw, g_bots[0].pitch,
                    g_bots[0].grounded, 0, 0.0f, 0, 0.0f, 0.0f, 0, 1);
          int sd = ra.foot[0].t < ra.foot[1].t ? 0 : 1;
          if (last_stance_d >= 0 && sd != last_stance_d) plants_dec++;
          last_stance_d = sd;
          pprev = pos;
        }
      }
      printf("netanim n=%ld stride=%d plants_full=%d plants_decimated=%d "
             "(counting plants, not frames)\n",
             n, stride, plants_full, plants_dec);
    } else if (!strcmp(t, "netfill")) {
      // The fill and cap rules over a join/leave timeline: a join never passes cap_public,
      // bots pad to cap_public and never past cap_bots, and — the half a
      // fixed-count model cannot express — the bots COME BACK when humans go.
      int cb = g_srv.cap_bots, cp = g_srv.cap_public;
      int humans = 0, bots = net_bot_target(0, cp, cb);   // empty server
      int bots_empty = bots;
      int max_public = 0, max_arena = bots;
      int over_public = 0, bots_over = 0, arena_over = 0;
      const int seq[] = {0,0,0,0,0,0,0,0,0,0,0, -1,-1,-1,-1,-1,-1,-1};
      for (int i = 0; i < (int)(sizeof seq / sizeof seq[0]); i++) {
        if (seq[i] < 0) { if (humans > 0) humans--; }
        else if (net_join_ok(humans, cp)) humans++;
        bots = net_bot_target(humans, cp, cb);   // the one rule, every step
        if (humans > cp) over_public++;
        if (bots > cb || bots < 0) bots_over++;
        if (humans + bots > cp) arena_over++;
        if (humans > max_public) max_public = humans;
        if (humans + bots > max_arena) max_arena = humans + bots;
      }
      int inv = cp >= 1 && cp <= MAX_HUMANS;
      printf("netfill cap_bots=%d cap_public=%d inv(1<=cp<=%d)=%s "
             "public_max=%d arena_max=%d over_public=%d bots_over=%d "
             "arena_over=%d bots_empty=%d bots_refill=%d\n",
             cb, cp, MAX_HUMANS, inv ? "ok" : "FAIL", max_public, max_arena,
             over_public, bots_over, arena_over, bots_empty, bots);
    } else if (!strcmp(t, "netdeath")) {
      netdeath_proof();
    } else if (!strcmp(t, "netleave")) {
      // THE ONLINE->LOCAL TRANSITION, as a gate. Stage the roster a
      // live match leaves behind — foreign humans switched on by
      // net_cl_apply_ent, names, score rows, a local entity that is NOT 0 —
      // then leave, and count what survived. Every number here read WRONG
      // before 2026-08-18: ghosts=3 (remote players still standing in the
      // singleplayer arena), names=3, and one of those ghosts wore the
      // player's own name. Staged rather than driven through a socket
      // because the defect is in the teardown, not in the transport.
      int me = 2;                       // the slot a server would have given us
      for (int h = 1; h <= 3; h++) {
        g_humans_on[h] = 1;
        // %c, not %d: h is 1..3 here, and the -O1 build (the ASan one) cannot
        // prove that — it reported a -Wformat-truncation the -O2 gate does not.
        snprintf(g_human_name[h], sizeof g_human_name[h], "GHOST%c", (char)('0' + h));
        g_players[h].alive = 1;
        g_players[h].hp = 100;
        int e = ent_of_human(h);
        g_match.kills[e] = 7; g_match.deaths[e] = 3;
        g_match.shots[e] = 50; g_match.hits[e] = 9;
        g_net_ping[e] = 42;
        g_spawn_seq[e] = 5;
        g_human_move[h] = 0;
      }
      snprintf(g_human_name[me], sizeof g_human_name[me], "%s", g_cfg.name);
      g_online = 1;
      g_local_ent = ent_of_human(me);
      leave_match();
      int ghosts = 0, names = 0, rows = 0, pings = 0, seqs = 0, moved = 0;
      for (int h = 1; h < MAX_HUMANS; h++) {
        if (g_humans_on[h]) ghosts++;
        if (g_human_name[h][0]) names++;
        if (!g_human_move[h]) moved++;
        int e = ent_of_human(h);
        if (g_match.kills[e] || g_match.deaths[e] ||
            g_match.shots[e] || g_match.hits[e]) rows++;
        if (g_net_ping[e]) pings++;
        if (g_spawn_seq[e]) seqs++;
      }
      // A ghost is walkable by ent_next, so the roster walk is the honest
      // check: it is what makes one shootable, rendered and scored.
      int walked = 0;
      for (int e = ent_next(-1); e >= 0; e = ent_next(e)) if (e > MAX_BOTS) walked++;
      printf("netleave ghosts=%d walked=%d names=%d rows=%d pings=%d seq=%d "
             "move_stuck=%d online=%d local_ent=%d self=%d\n",
             ghosts, walked, names, rows, pings, seqs, moved, g_online,
             g_local_ent, g_humans_on[0]);
    } else if (!strcmp(t, "netclient")) {
      netclient_proof();
    } else if (!strcmp(t, "netstat")) {
      // The SAME function the server's 10 s digest prints — that is
      // the one-copy rule on observability, checked rather than asserted.
      char dg[160];
      net_digest(dg, sizeof dg);
      printf("netstat %s\n", dg);
    } else if (!strcmp(t, "humans")) {
      // Test-roster control (the Phase-1 gate's two-human test): activate
      // human slots so the total is N. A joining slot gets the bot pattern of
      // P1-69 — zeroed score row, fresh spawn — and a name; its input stays
      // neutral (g_human_held is zero), so it stands where it spawned.
      long n = next_n();
      if (n < 1) n = 1;
      if (n > MAX_HUMANS) n = MAX_HUMANS;
      for (int h = 1; h < MAX_HUMANS; h++) {
        int on = h < n;
        if (on && !g_humans_on[h]) {
          g_humans_on[h] = 1;
          int e = ent_of_human(h);
          g_match.kills[e] = g_match.deaths[e] = 0;
          g_match.shots[e] = g_match.hits[e] = 0;
          snprintf(g_human_name[h], sizeof g_human_name[h], "HUM%d", h & 7);
          // (& 7: value-range for -Wformat-truncation; MAX_HUMANS is 8)
          player_spawn(&g_players[h]);
          player_respawn(&g_players[h], e);
        } else if (!on && g_humans_on[h]) {
          g_humans_on[h] = 0;
          int e = ent_of_human(h);
          g_match.kills[e] = g_match.deaths[e] = 0;   // P1-70: leave clears
          g_match.shots[e] = g_match.hits[e] = 0;     // the score row too
        }
      }
      printf("humans %ld\n", n);
    } else if (!strcmp(t, "localent")) {
      // P1-76's instrument: the byte gate always runs with g_local_ent == 0
      // and is blind to every P0-6 site. Forcing a different id and diffing
      // sfxlog/mixdown/HUD state against the id-0 run is the only cheap probe
      // for that otherwise invisible defect class (a centred dry click, a
      // mispanned hurt). Diagnostic only — never in a recorded baseline.
      // THE ONE UNVALIDATED WRITER, until now. Every other path is bounded
      // (ACCEPT validates, a dedicated server pins -1, SP pins 0) and the
      // scoreboard indexes g_match.kills[g_local_ent] behind a `>= 0` test
      // only — so a script typo was an out-of-bounds read.
      long le = next_n();
      g_local_ent = (le < -1 || le >= MAX_ENTS) ? -1 : (int)le;
      printf("localent %d\n", g_local_ent);
    } else if (!strcmp(t, "cfg")) {
      printf("cfg sense=%.2f ads_sense=%.2f fov=%d vsync=%d fps_cap=%d show_fps=%d msaa=%d binds=",
             (double)g_cfg.sense, (double)g_cfg.ads_sense, (int)g_cfg.fov, g_cfg.vsync,
             g_cfg.fps_cap, g_cfg.show_fps, g_cfg.msaa);
      for (int i = 0; i < IN_COUNT; i++) printf("%s%s", i ? "," : "", g_cfg.bind[i]);
      printf(" padbinds=");
      for (int i = 0; i < IN_COUNT; i++) printf("%s%s", i ? "," : "", g_cfg.padbind[i]);
      printf(" menu=%d rebind=%d tab=%d\n", g_menu_open, g_ui_rebind, g_ui_tab);
      printf("mv");
      for (int i = 0; i < MV_COUNT; i++)
        printf(" %s=%.2f", MV_DEF[i].key, (double)g_cfg.mv[i]);
      printf("\nwp vol=%.2f", (double)g_cfg.vol);
      for (int w = 0; w < WPN_COUNT; w++)
        for (int i = 0; i < WP_COUNT; i++) {
          char key[32];
          wp_key(key, sizeof key, w, i);
          printf(" %s=%.2f", key, (double)g_cfg.wp[w][i]);
        }
      printf("\npad");
      for (int i = 0; i < PAD_COUNT; i++)
        printf(" %s=%.2f", PAD_DEF[i].key, (double)g_cfg.pad[i]);
      printf("\n");
    } else if (!strcmp(t, "pos")) {
      sim_print(s);
    } else if (!strcmp(t, "lean")) {
      // The lean's proof. Three things a screenshot cannot answer: how much of
      // the command the geometry actually granted, whether the FIRST-person eye
      // and the THIRD-person head moved by the same amount (they are derived
      // from one bend, so `gap` is a contract and must stay near zero), and
      // whether the exposure the peek buys the enemy exists at all — `see` is
      // bot 0's real line of sight to the player. A peek that lets you shoot out
      // without letting anything shoot back is not a lean, it is a wallhack.
      const player_t *pl = s->p;
      anim_t tmp = g_player_anim[0];
      v3 hl = tmp.pose.head;              // head as solved WITH the lean
      tmp.lean_s = 0.0f;
      skel_solve(&tmp, pl->pos, pl->yaw, pl->pitch);
      v3 h0 = tmp.pose.head;              // ...and the same pose standing up
      v3 Ra = {{cosf(pl->yaw), 0, sinf(pl->yaw)}};
      float head_lat = (hl.x - h0.x) * Ra.x + (hl.z - h0.z) * Ra.z;
      v3 eb = player_eye_base(pl), ey = player_eye_pos(pl);
      float eye_lat = (ey.x - eb.x) * Ra.x + (ey.z - eb.z) * Ra.z;
      int see = 0;
      float sd;
      if (g_match.nbots > 0 && g_bots[0].active && g_bots[0].alive)
        see = bot_can_see(&g_bots[0], 0, &sd);
      printf("lean t=%ld cmd=%.3f clr=%.3f lean=%.3f body=%.3f roll=%.1f "
             "eye=(%.3f %.3f %.3f) off=%.3f drop=%.3f head_off=%.3f head_drop=%.3f "
             "gap=%.3f hbox=%.3f see=%d\n",
             s->tick, (double)pl->lean_in, (double)pl->lean_clr, (double)pl->lean,
             (double)g_player_anim[0].lean_s,
             (double)(pl->lean * LEAN_ANGLE * LEAN_VIEW_K / DEG2RAD),
             (double)ey.x, (double)ey.y, (double)ey.z,
             (double)eye_lat, (double)(ey.y - eb.y),
             (double)head_lat, (double)(hl.y - h0.y),
             (double)fabsf(eye_lat - head_lat),
             (double)lean_eye_off(pl->lean), see);
    } else if (!strcmp(t, "vmcheck")) {
      // The viewmodel's geometry proof. Same analysis `figcheck` runs on the
      // body, swept over every state that MOVES a part relative to its
      // neighbours — hip/ADS, the reload dip and roll, the swap raise, the fire
      // kick — because a coplanar pair or an inside-out box usually only appears
      // at one point in a blend, which is exactly what a single screenshot
      // cannot rule out. open/flip/dup/degen must be 0 and zfight must be 0.
      player_t tmp = *s->p;
      v3 eye = {{0, 0, 0}}, rgt = {{1, 0, 0}}, upv = {{0, 1, 0}}, fwd = {{0, 0, -1}};
      fchk_res_t worst = {0};
      // `cross` is tracked as its own maximum over the sweep and deliberately
      // kept OUT of the score that picks the worst state. Folding it in moved
      // which state wins, which silently moved the `near` quadruple that
      // tools/ci-proofs.sh gates on — a new advisory tier has no business
      // re-baselining an old one. Two independent worsts, both deterministic.
      int worst_cross = 0;
      int worst_score = -1, tris = 0;
      const char *worst_state = "";
      char label[64];
      for (int wi = 0; wi < WPN_COUNT; wi++) {
        tmp.wp.cur = wi;
        for (int a = 0; a <= 10; a++) {
          // Phases 4/5 are a full lean each way, at rest and mid-ramp (the lag
          // term is only nonzero while the lean is MOVING, so a settled lean
          // would never exercise it). The lean is a rigid transform of the whole
          // pass — org and the gun basis, hands included — so it cannot open a
          // seam in principle; this is here so that stays true of whatever the
          // lean grows into.
          // Phase 6 is the reload at 27%, i.e. the magazine fully OUT. A
          // reload sampled only at its midpoint tests the magazine already back
          // in the well; the whole point of giving it its own travel curve is
          // that it now occupies stations the mesh has never been checked at.
          for (int phase = 0; phase < 7; phase++) {
            tmp.wp.ads_lin = (float)a / 10.0f;
            tmp.wp.ads_s = tmp.wp.prev_ads_s = smooth01(tmp.wp.ads_lin);
            tmp.wp.reload_t = phase == 1 ? WPN_DEF[wi].reload_ticks / 2
                            : phase == 6 ? (WPN_DEF[wi].reload_ticks * 73) / 100
                            : 0;
            tmp.wp.switch_t = phase == 2 ? GUN_SWITCH_TICKS / 2 : 0;
            tmp.wp.prev_switch = (float)tmp.wp.switch_t / GUN_SWITCH_TICKS;
            tmp.vm.kick = tmp.vm.prev_kick = phase == 3 ? 1.0f : 0.0f;
            tmp.lean = tmp.prev_lean = phase == 4 ? 1.0f : phase == 5 ? -1.0f : 0.0f;
            tmp.vm.lean_lag = tmp.vm.prev_lean_lag = tmp.lean * 0.45f;
            // Probes on, exactly as figcheck does it: a bare triangle index
            // is not a diagnosis. `vm_gun`/`vm_hands` is the granularity
            // vm_build publishes, and it is the one that answers the question
            // this sweep is usually asked — is the offending pair inside the
            // weapon, or is it a hand closing on it.
            g_nprobe = 0;
            g_probe_on = 1;
            vm_build(&tmp, 1.0f, eye, rgt, upv, fwd);
            probe(NULL);
            g_probe_on = 0;
            fchk_res_t r = fchk_scene();
            tris = r.tris;
            int sc = r.open * 100 + r.flip * 100 + r.dup * 100 + r.degen * 100 +
                     r.zfight * 10 + r.near;
            if (r.cross > worst_cross) worst_cross = r.cross;
            if (sc > worst_score) {
              worst_score = sc;
              worst = r;
              snprintf(label, sizeof label, "%s ads=%.1f %s", WPN_DEF[wi].prefix,
                       (double)tmp.wp.ads_lin,
                       phase == 1 ? "reload" : phase == 2 ? "swap" :
                       phase == 3 ? "kick" : phase == 4 ? "lean_r" :
                       phase == 5 ? "lean_l" : phase == 6 ? "magout" : "idle");
              worst_state = label;
            }
          }
        }
      }
      printf("vmcheck tris=%d worst=[%s] open=%d flip=%d dup=%d zfight=%d "
             "near=%d cross=%d degen=%d\n", tris, worst_state, worst.open,
             worst.flip, worst.dup, worst.zfight, worst.near, worst_cross,
             worst.degen);
    } else if (!strcmp(t, "vmsight")) {
      // SIGHT-CLEARANCE PROOF. No part of the shooter's own hands or forearms
      // may enter the cone the eye looks through. Neither figcheck nor vmcheck
      // can see this — both test the mesh against itself, and a hand parked in
      // front of a red dot is a perfectly valid closed manifold. It is also the
      // worst thing a viewmodel can do, and it shipped: the AR's dot rendered on
      // the support hand's knuckles in ADS.
      // The aperture is published by gun_build / vm_build (g_gun_sight_p/_r), so
      // it can never drift from the geometry it describes, and the hand range
      // comes from the same `probe` mechanism `figm` uses.
      player_t tmp = *s->p;
      v3 eye = {{0, 0, 0}}, rgt = {{1, 0, 0}}, upv = {{0, 1, 0}}, fwd = {{0, 0, -1}};
      int total_bad = 0;
      for (int wi = 0; wi < WPN_COUNT; wi++) {
        // A scoped weapon is immune BY CONSTRUCTION and saying so beats skipping
        // it: the sniper's sight picture is a HUD disc composited after every
        // world pass, so no geometry — hand, tube or otherwise — can land on top
        // of it. Testing it against the ocular's own 213 mrad cone measures the
        // scope tube and reports a defect that cannot exist.
        if (wi == WPN_SR) {   // the only weapon with a PiP scope
          printf("vmsight %s n/a — sight picture is a composited disc, "
                 "occlusion impossible\n", WPN_DEF[wi].prefix);
          continue;
        }
        tmp.wp.cur = wi;
        int wbad = 0;
        float wmargin = 1e9f;
        float wads = 0.0f;
        for (int a2 = 4; a2 <= 10; a2++) {   // only aimed states have a sight picture
          tmp.wp.ads_lin = (float)a2 / 10.0f;
          tmp.wp.ads_s = tmp.wp.prev_ads_s = smooth01(tmp.wp.ads_lin);
          tmp.wp.reload_t = 0; tmp.wp.switch_t = 0; tmp.wp.prev_switch = 0.0f;
          tmp.vm.kick = tmp.vm.prev_kick = 0.0f;
          g_nprobe = 0; g_probe_on = 1;
          vm_build(&tmp, 1.0f, eye, rgt, upv, fwd);
          g_probe_on = 0;
          v3 ap = v3_sub(g_gun_sight_p, eye);
          float apd = v3_dot(ap, fwd);
          if (apd < 0.01f) continue;
          float tan_ap = g_gun_sight_r / apd;   // the aperture's angular radius
          for (int i = 0; i < g_nprobe; i++) {
            if (!g_probe[i].name || strcmp(g_probe[i].name, "vm_hands")) continue;
            for (int f = g_probe[i].v0; f < g_probe[i].v1; f += VERT_FLOATS) {
              v3 d = {{g_scene[f] - eye.x, g_scene[f + 1] - eye.y,
                       g_scene[f + 2] - eye.z}};
              float z = v3_dot(d, fwd);
              if (z <= apd) continue;          // nearer than the aperture: harmless
              v3 lat = v3_sub(d, v3_scale(fwd, z));
              float tan_v = sqrtf(v3_dot(lat, lat)) / z;
              // Positive margin = outside the cone. Reported in milliradians so
              // the number means something at any screen size.
              float m = (tan_v - tan_ap) * 1000.0f;
              if (m < 0.0f) wbad++;
              if (m < wmargin) { wmargin = m; wads = tmp.wp.ads_lin; }
            }
          }
        }
        printf("vmsight %s occluding_verts=%d worst_margin=%.1f mrad at ads=%.1f\n",
               WPN_DEF[wi].prefix, wbad, (double)wmargin, (double)wads);
        total_bad += wbad;
      }
      printf("vmsight total=%d (must be 0)\n", total_bad);
    } else if (!strcmp(t, "vmtrig")) {
      vmtrig_proof(s);
    } else if (!strcmp(t, "parity")) {
      // Every gameplay quantity that must be IDENTICAL for the player and for a
      // bot, computed by calling the shared code with each one's state. If a
      // number ever differs, some path grew its own copy of a constant again —
      // which is exactly how bots ended up running at a third of the player's
      // speed with their own fire rate and no recoil at all.
      const player_t *p = s->p;
      bot_t *b = &g_bots[0];
      const float *mv = g_cfg.mv;
      weapon_t pw = p->wp, bw = b->wp;
      pw.cur = bw.cur = WPN_AR;
      pw.ads_s = bw.ads_s = 0.0f;
      pw.bloom = bw.bloom = 0.0f;
      v3 rest = {{0, 0, 0}}, run = {{mv[MV_RUN], 0, 0}};
      int ppart = 0, bpart = 0;
      v3 dn = {{0, -1, 0}};
      v3 pabove = {{p->pos.x, p->pos.y + 4.0f, p->pos.z}};
      v3 babove = {{b->pos.x, b->pos.y + 4.0f, b->pos.z}};
      v3 noff = {{0, 0, 0}};  // hitbox parity is measured upright, both sides
      float ph = ent_trace_box(p->pos, EYE_STAND, noff, pabove, dn, &ppart);
      float bh = ent_trace_box(b->pos, EYE_STAND, noff, babove, dn, &bpart);
      float pc = ent_trace_box(p->pos, EYE_CROUCH, noff, pabove, dn, &ppart);
      float bc = ent_trace_box(b->pos, EYE_CROUCH, noff, babove, dn, &bpart);
      // Two kinds of row, DECLARED in the output. `shared` rows put the same
      // expression in both columns because the one-copy refactor removed the
      // bot-side twin they originally compared — their MISMATCH branch is
      // unreachable, and printing "ok" implied a check that cannot fail ("a
      // proof whose output cannot move is not a proof"). They stay as
      // DOCUMENTATION of which quantities are shared by construction; the live
      // rows (spreads through the shared gun, hitboxes through ent_trace_box
      // at each entity's own state) are the ones that can still catch a drift.
      struct { const char *n; double pv, bv; int shared; } row[] = {
        {"run_speed",     mv[MV_RUN],                       mv[MV_RUN], 1},
        {"crouch_speed",  mv[MV_CROUCH_SP],                 mv[MV_CROUCH_SP], 1},
        {"accel_ms",      mv[MV_ACCEL_MS],                  mv[MV_ACCEL_MS], 1},
        {"jump_speed",    mv_jump_speed(),                  mv_jump_speed(), 1},
        {"air_ctl",       mv[MV_AIR_CTL],                   mv[MV_AIR_CTL], 1},
        {"slide_boost",   mv_slide_boost(),                 mv_slide_boost(), 1},
        {"slide_gate",    mv[MV_RUN] * SLIDE_START_SPEED,   mv[MV_RUN] * SLIDE_START_SPEED, 1},
        {"fire_cool",     (int)(TICK_HZ * 60.0f / g_cfg.wp[WPN_AR][WP_RPM] + 0.5f),
                          (int)(TICK_HZ * 60.0f / g_cfg.wp[WPN_AR][WP_RPM] + 0.5f), 1},
        {"mag",           WPN_DEF[WPN_AR].mag,              WPN_DEF[WPN_AR].mag, 1},
        {"reload_ticks",  WPN_DEF[WPN_AR].reload_ticks,     WPN_DEF[WPN_AR].reload_ticks, 1},
        {"dmg",           WPN_DEF[WPN_AR].dmg,              WPN_DEF[WPN_AR].dmg, 1},
        {"spread_rest",   gun_spread(&pw, rest, 1),         gun_spread(&bw, rest, 1), 0},
        {"spread_run",    gun_spread(&pw, run, 1),          gun_spread(&bw, run, 1), 0},
        {"spread_air",    gun_spread(&pw, run, 0),          gun_spread(&bw, run, 0), 0},
        {"kick_up",       g_cfg.wp[WPN_AR][WP_KICK_UP],     g_cfg.wp[WPN_AR][WP_KICK_UP], 1},
        {"recover_ms",    g_cfg.wp[WPN_AR][WP_RECOVER_MS],  g_cfg.wp[WPN_AR][WP_RECOVER_MS], 1},
        {"eye_stand",     EYE_STAND,                        EYE_STAND, 1},
        {"eye_crouch",    EYE_CROUCH,                       EYE_CROUCH, 1},
        {"height_stand",  HEIGHT_STAND,                     HEIGHT_STAND, 1},
        {"height_crouch", HEIGHT_CROUCH,                    HEIGHT_CROUCH, 1},
        {"radius",        PLAYER_RADIUS,                    PLAYER_RADIUS, 1},
        {"hp",            PLAYER_HP,                        PLAYER_HP, 1},
        {"hitbox_stand",  4.0f - ph,                        4.0f - bh, 0},
        {"hitbox_crouch", 4.0f - pc,                        4.0f - bc, 0},
      };
      int bad = 0, live = 0;
      for (unsigned i = 0; i < sizeof row / sizeof *row; i++) {
        int eq = fabs(row[i].pv - row[i].bv) < 1e-5;
        if (!eq) bad++;
        if (!row[i].shared) live++;
        printf("parity %-14s player=%10.4f bot=%10.4f %s\n", row[i].n,
               row[i].pv, row[i].bv,
               !eq ? "MISMATCH" : row[i].shared ? "shared" : "ok");
      }
      printf("parity total rows=%u live=%d mismatches=%d\n",
             (unsigned)(sizeof row / sizeof *row), live, bad);
    } else if (!strcmp(t, "wpn")) {
      const weapon_t *w = &s->p->wp;
      const vm_t *v = &s->p->vm;
      printf("wpn t=%ld cur=%s ammo=%d spread=%.3f rec=(%.3f %.3f) bloom=%.3f ads=%.3f "
             "reload=%d switch=%d punch=%.3f vm sway=(%.4f %.4f) lag=(%.4f %.4f %.4f) "
             "kick=%.4f roll=%.4f scope=(%.1f %.1f %.1f)\n",
             s->tick, WPN_DEF[w->cur].prefix, w->ammo[w->cur], (double)weapon_spread(s->p),
             (double)w->rec_p, (double)w->rec_y, (double)w->bloom,
             (double)w->ads_s, w->reload_t, w->switch_t, (double)w->punch,
             (double)v->sway_p, (double)v->sway_y,
             (double)v->lag_x, (double)v->lag_y, (double)v->lag_z,
             (double)v->kick, (double)v->roll,
             (double)g_scope_cx, (double)g_scope_cy, (double)g_scope_r);
    } else if (!strcmp(t, "weapon")) {
      const char *n = next_tok();
      int found = 0;
      for (int i = 0; i < WPN_COUNT; i++)
        if (!strcmp(n, WPN_DEF[i].prefix)) {
          s->p->wp.cur = i;       // instant, no raise anim: test convenience
          s->p->wp.switch_t = 0;
          s->p->wp.reload_t = 0;
          memset(s->p->wp.cool, 0, sizeof s->p->wp.cool);
          // ...and bot 0 with it while the puppet drives it. Bots spawn with the
          // AR and only swap to the sniper on their own range policy, so with
          // the AI off there was no way to FILM the sniper's third-person carry
          // at all — the one pose `figbury` reports worst and the one a player
          // stares at for most of a match.
          if (g_bots[0].puppet) {
            g_bots[0].wp.cur = i;
            g_bots[0].wp.switch_t = 0;
            g_bots[0].wp.reload_t = 0;
            memset(g_bots[0].wp.cool, 0, sizeof g_bots[0].wp.cool);
          }
          found = 1;
        }
      if (!found) { fprintf(stderr, "script: unknown weapon '%s'\n", n); exit(1); }
    } else if (!strcmp(t, "aim")) {
      v3 tgt;
      tgt.x = next_f();
      tgt.y = next_f();
      tgt.z = next_f();
      aim_at(s->p, tgt);
    } else if (!strcmp(t, "warp")) {
      // Teleport the player. Peek tests need the body at an exact standoff from
      // an exact corner — walking there with +forward makes the measurement a
      // function of the accel curve, which is not what is under test.
      s->p->pos.x = next_f();
      s->p->pos.y = next_f();
      s->p->pos.z = next_f();
      s->p->prev_pos = s->p->pos;
      s->p->vel = (v3){{0, 0, 0}};
      s->p->grounded = 1;
      s->p->tuck_y = 0.0f;   // a teleport is a clean slate; anim_reset zeroes
                             // the figure's half of this on the next line
      anim_reset(&g_player_anim[0], s->p->pos, s->p->yaw);
    } else if (!strcmp(t, "aimbot")) {
      int bi = -1;
      float bd = 1e18f;
      for (int i = 0; i < g_match.nbots; i++) {
        if (!g_bots[i].active || !g_bots[i].alive) continue;
        float dx = g_bots[i].pos.x - s->p->pos.x, dz = g_bots[i].pos.z - s->p->pos.z;
        float dd = dx * dx + dz * dz;
        if (dd < bd) { bd = dd; bi = i; }
      }
      if (bi >= 0) {
        v3 h = {{g_bots[bi].pos.x,
                 g_bots[bi].pos.y + bot_eye(&g_bots[bi]) + 0.03f,  // head center
                 g_bots[bi].pos.z}};
        aim_at(s->p, h);
      }
    } else if (!strcmp(t, "bot")) {
      for (int i = 0; i < g_match.nbots; i++) {
        const bot_t *b = &g_bots[i];
        printf("bot %d %s t=%ld pos=(%.3f %.3f %.3f) yaw=%.2f hp=%d state=%s "
               "st=%s cov=%d tgt=%s respawn=%d",
               i, ent_name(i + 1), s->tick,
               (double)b->pos.x, (double)b->pos.y, (double)b->pos.z,
               (double)b->yaw, b->hp,
               !b->alive ? "dead" : g_bots_frozen ? "frozen"
                                  : b->tgt >= 0 ? "combat" : "patrol",
               b->sliding ? "slide" : b->crouch ? "crouch" : "stand", b->in_cover,
               b->alive && b->tgt >= 0 ? ent_name(b->tgt) : "none",
               b->alive ? 0 : b->respawn_t);
        printf(" belief_state=%s lose=%d belief=(%.3f %.3f %.3f) rad=%.2f "
               "search=(%.3f %.3f %.3f) reacq=%d",
               bot_belief_label(b), b->lose_t,
               (double)b->belief_pos.x, (double)b->belief_pos.y,
               (double)b->belief_pos.z, (double)b->belief_radius,
               (double)b->search_pos.x, (double)b->search_pos.y,
               (double)b->search_pos.z, b->reacquire_count);
        // The FALL, which no screenshot answers: how far it has toppled, about
        // which horizontal axis, how far it has spun about its own vertical and
        // what the round threw it with. Dead rows only, so every living row is
        // the bytes it always was and existing artefacts still match.
        if (!b->alive)
          printf(" tilt=%.3f spin=%.2f dyaw=%.3f ax=(%.2f %.2f) "
                 "dv=(%.2f %.2f %.2f) slam=%.2f gnd=%d",
                 (double)b->rag.tilt, (double)b->rag.spin, (double)b->rag.yaw,
                 (double)b->rag.axis.x, (double)b->rag.axis.z,
                 (double)b->rag.vel.x, (double)b->rag.vel.y, (double)b->rag.vel.z,
                 (double)b->rag.slam, b->rag.grounded);
        putchar('\n');
      }
    } else if (!strcmp(t, "rag")) {
      // THE FLIGHT, TICK BY TICK, ACROSS THE HANDOVER. A ragdoll is the one
      // thing in this game a screenshot is worst at: peak height, air time,
      // bounces, wall rebounds and total travel are all quantities, and the
      // handover to the corpse pool happens in the middle of them — which is
      // exactly where the two integrators used to disagree. One row per tick
      // for whichever body is in flight, entity or corpse, plus a summary line
      // with the numbers the brief is actually about.
      long n = next_n();
      int bi = (int)opt_n(0);             // which BOT INDEX, default 0
      if (bi < 0) bi = 0;
      if (bi >= MAX_BOTS) bi = MAX_BOTS - 1;
      v3 p0 = {{0, 0, 0}};
      float apex = 0.0f, travel = 0.0f, spin_max = 0.0f, slam_max = 0.0f;
      int air = 0, bounces = 0, first = 1, settle = -1;
      float pvy = 0.0f, tilt = 0.0f, yaw = 0.0f;
      v3 prev = {{0, 0, 0}};
      for (long i = 0; i < n; i++) {
        sim_advance(s);
        const bot_t *b = &g_bots[bi];
        const rag_t *r = NULL;
        v3 p = {{0, 0, 0}};
        int life = -1;
        if (!b->alive && b->active) { r = &b->rag; p = b->pos; }
        else {   // handed over: follow ITS corpse (harness_corpse_of)
          const corpse_t *cp = harness_corpse_of(bi);
          if (cp) { life = cp->life; r = &cp->rag; p = cp->pos; }
        }
        if (!r) continue;
        if (first) { p0 = p; prev = p; first = 0; }
        v3 d = v3_sub(p, prev);
        travel += sqrtf(d.x * d.x + d.z * d.z);
        prev = p;
        if (p.y > apex) apex = p.y;
        if (fabsf(r->spin) > spin_max) spin_max = fabsf(r->spin);
        if (r->slam > slam_max) slam_max = r->slam;
        if (!r->grounded) air++;
        // A bounce is the frame the ground hands velocity BACK — that is the
        // only unambiguous signature, and it is why the counter reads dv rather
        // than the grounded flag (which the bounce itself clears).
        if (pvy < -0.05f && r->vel.y > 0.05f) bounces++;
        pvy = r->vel.y;
        tilt = r->tilt;
        yaw = r->yaw;
        if (settle < 0 && r->grounded && r->spin == 0.0f &&
            r->vel.x == 0.0f && r->vel.z == 0.0f) settle = (int)i;
        printf("rag t=%ld p=(%.2f %.2f %.2f) tilt=%.3f spin=%.2f yaw=%.3f "
               "v=(%.2f %.2f %.2f) slam=%.2f gnd=%d %s\n",
               (long)s->tick, (double)p.x, (double)p.y, (double)p.z,
               (double)r->tilt, (double)r->spin, (double)r->yaw,
               (double)r->vel.x, (double)r->vel.y, (double)r->vel.z,
               (double)r->slam, r->grounded, life >= 0 ? "corpse" : "entity");
      }
      v3 dt = v3_sub(prev, p0);
      printf("rag apex=%.3f air=%d/%ld bounces=%d slam_max=%.2f spin_max=%.2f "
             "travel=%.2f net=%.2f tilt=%.3f yaw=%.3f settle=%d\n",
             (double)apex, air, n, bounces, (double)slam_max, (double)spin_max,
             (double)travel, (double)sqrtf(dt.x * dt.x + dt.z * dt.z),
             (double)tilt, (double)yaw, settle);
    } else if (!strcmp(t, "ragsoak")) {
      // `ragx` follows ONE body; a match has six corpses on the floor at once,
      // in cover, on platforms and against the perimeter, and the worst of them
      // is the one the player is looking at. This is the same analysis over
      // EVERY dead entity and EVERY corpse, every tick, in a live match — the
      // soak that says whether the fixes hold on arena geometry nobody staged.
      long n = next_n();
      float wfloat = 0.0f, wsink = 0.0f, wpen = 0.0f, whang = 0.0f;
      long nin = 0, bodies = 0, samples = 0, ptick = -1;
      float wbpen = 0.0f, wrpen = 0.0f, wrsink = 0.0f;
      const char *pnam = "-", *snam = "-", *rnam = "-";
      int pgnd = 0, pset = 0;
      for (long i = 0; i < n; i++) {
        sim_advance(s);
        // e < 0 scans the corpse pool, e >= 0 dead-but-unrespawned BOTS.
        // Humans need no third range (P1-65): a human hands his body to the
        // pool ON the killing tick (corpse_add with his own anim and real
        // wp.cur), so every human corpse is already covered by the e < 0
        // branch — there is no human-as-dead-entity window to scan.
        for (int e = -MAX_CORPSES; e < MAX_BOTS; e++) {
          const rag_t *r;
          ragx_t x;
          scene_reset();
          g_nprobe = 0;
          if (e < 0) {
            const corpse_t *cp = &g_corpses[-e - 1];
            if (!cp->used) continue;
            g_probe_on = 1;
            g_cam_eye = cp->pos;
            harness_draw_corpse(cp);
            r = &cp->rag;
          } else {
            const bot_t *b = &g_bots[e];
            if (b->alive || !b->active) continue;
            g_probe_on = 1;
            g_cam_eye = b->pos;
            scene_draw_bot(b, 1.0f);
            r = &b->rag;
          }
          samples++;
          probe(NULL);
          g_probe_on = 0;
          x = ragx_scan(0);
          if (r->grounded && x.clr > wfloat) wfloat = x.clr;
          if (r->grounded && r->spin == 0.0f && r->vel.x == 0.0f &&
              r->vel.z == 0.0f) {
            bodies++;
            if (x.hang > whang) whang = x.hang;
            // SETTLED bodies are the ones a player walks up to and looks at.
            // A quarter of a metre of elbow inside a wall for two ticks of a
            // 6 m/s landing is a different defect from a corpse that LIES there
            // with its chest in the concrete, and one number cannot say both.
            if (x.bpen > wrpen) { wrpen = x.bpen; rnam = ragx_part(x.fbpen); }
            if (x.clr < wrsink) wrsink = x.clr;
          }
          if (x.clr < wsink) { wsink = x.clr; snam = ragx_part(x.fclr); }
          if (x.pen > wpen) wpen = x.pen;
          if (x.bpen > wbpen) {
            wbpen = x.bpen; pnam = ragx_part(x.fbpen); ptick = (long)s->tick;
            pgnd = r->grounded;
            // The COMPLETE settled predicate (the truncated x-only copy could
            // label a body still skidding along z — per-axis collision zeroes
            // only the axis that hit — as at rest).
            pset = r->spin == 0.0f && r->vel.x == 0.0f && r->vel.z == 0.0f;
          }
          if (x.nin) nin++;
        }
      }
      printf("ragsoak ticks=%ld samples=%ld settled=%ld float=%.3f hang=%.3f "
             "sink=%.3f<%s> pen=%.3f body_pen=%.3f<%s>@t%ld gnd=%d rest=%d "
             "in=%ld rest_pen=%.3f<%s> rest_sink=%.3f\n", n, samples, bodies,
             (double)wfloat, (double)whang, (double)wsink, snam, (double)wpen,
             (double)wbpen, pnam, ptick, pgnd, pset, nin, (double)wrpen, rnam,
             (double)wrsink);
    } else if (!strcmp(t, "flinch")) {
      flinch_proof(s);
    } else if (!strcmp(t, "ragx")) {
      // WHERE THE BODY ACTUALLY IS, measured on the geometry rather than on the
      // origin `rag` prints. `rag` can report a body at rest on y = 0 while the
      // rendered corpse sits 30 cm in the air or half inside a wall, because the
      // origin is not the mesh: the corpse solve tips, drapes, clamps and settles
      // the pose AFTER the integrator has finished with the origin. This is the
      // proof for the two complaints the integrator cannot see.
      //   float  worst clearance of the lowest rendered vertex, GROUNDED ticks
      //          only (a body in flight is legitimately clear of everything)
      //   sink   worst NEGATIVE clearance — geometry under the surface
      //   pen    deepest vertex inside a solid, `in_ticks` how many ticks had any
      long n = next_n();
      // `bi` is a BOT INDEX (P0-2) — see the identical note in `rag`.
      int bi = (int)opt_n(0);
      if (bi < 0) bi = 0;
      if (bi >= MAX_BOTS) bi = MAX_BOTS - 1;
      float wfloat = 0.0f, wsink = 0.0f, wpen = 0.0f, wbfloat = 0.0f, whang = 0.0f;
      long nin_ticks = 0, gticks = 0;
      for (long i = 0; i < n; i++) {
        sim_advance(s);
        const bot_t *b = &g_bots[bi];
        const corpse_t *cp = NULL;
        if (b->alive || !b->active) {
          cp = harness_corpse_of(bi);   // one staged body stays the followed one
          if (!cp) continue;
        }
        scene_reset();
        g_nprobe = 0;
        g_probe_on = 1;
        // Camera ON the body: the invariants have to hold on the mesh the player
        // sees when he walks up to the corpse, not on whatever tier the harness
        // camera happened to select.
        g_cam_eye = cp ? cp->pos : b->pos;
        const rag_t *r;
        if (cp) {
          harness_draw_corpse(cp);
          r = &cp->rag;
        } else {
          scene_draw_bot(b, 1.0f);
          r = &b->rag;
        }
        probe(NULL);
        g_probe_on = 0;
        // Under `figv`, the solved joints themselves. `span`/`hgt` say a corpse
        // is folded; only the joint table says WHERE it is folded.
        if (g_fchk_v) {
          static const char *JN[POSE_NPOS] = {
            "hipL","hipR","kneeL","kneeR","ankL","ankR","toeL","toeR",
            "shoL","shoR","elbL","elbR","wriL","wriR",
            "pelvis","chest","neck","head","gun"};
          const pose_t *QP = cp ? &cp->anim.pose : &b->anim.pose;
          pose_t Q = *QP;
          death_t dq = rag_death(r, cp ? cp->slack : b->topple,
                                 cp ? cp->slack : b->topple, 1.0f);
          v3 qp = cp ? cp->pos : b->pos;
          fig_death_pose(&Q, &dq, qp, cp ? cp->yaw : b->yaw,
                         cp ? cp->wpn : WPN_AR);
          const v3 *qj = (const v3 *)&Q;
          printf("  joints");
          for (int k = 0; k < POSE_NPOS; k++)
            printf(" %s=%.2f", JN[k], (double)qj[k].y);
          printf("\n");
        }
        ragx_t x = ragx_scan(0);
        if (r->grounded) {
          gticks++;
          if (x.clr > wfloat) wfloat = x.clr;
          if (x.bclr > wbfloat) wbfloat = x.bclr;
        }
        // `hang` only means anything on a body that has STOPPED. A body still
        // toppling is standing on its boots with its chest legitimately a metre
        // in the air, and counting those ticks makes every scenario read 0.9.
        if (r->grounded && r->spin == 0.0f && r->vel.x == 0.0f &&
            r->vel.z == 0.0f && x.hang > whang) whang = x.hang;
        if (x.clr < wsink) wsink = x.clr;
        if (x.pen > wpen) wpen = x.pen;
        if (x.nin) nin_ticks++;
        v3 bp = cp ? cp->pos : b->pos;
        printf("ragx t=%ld p=(%.2f %.2f %.2f) tilt=%.3f clr=%+.3f<%s> "
               "body=%+.3f<%s> pen=%.3f<%s> in=%d hang=%.3f span=%.2f hgt=%.2f verts=%d "
               "gnd=%d %s\n",
               (long)s->tick, (double)bp.x, (double)bp.y, (double)bp.z,
               (double)r->tilt, (double)x.clr, ragx_part(x.fclr), (double)x.bclr,
               ragx_part(x.fbclr), (double)x.pen, ragx_part(x.fpen), x.nin,
               (double)x.hang, (double)x.span, (double)x.hgt, x.nv,
               r->grounded, cp ? "corpse" : "entity");
      }
      printf("ragx ticks=%ld grounded=%ld float=%.3f bodyfloat=%.3f hang=%.3f "
             "sink=%.3f pen=%.3f in_ticks=%ld\n", n, gticks, (double)wfloat,
             (double)wbfloat, (double)whang, (double)wsink, (double)wpen,
             nin_ticks);
    } else if (!strcmp(t, "puppet")) {
      char *sub = next_tok();
      bot_t *b = &g_bots[0];
      if (!strcmp(sub, "on")) {
        b->puppet = 1;
        b->pup_speed = 5.0f;
        b->pup_face = b->yaw;
        b->pup_mx = b->pup_mz = 0;
        b->pup_crouch = 0;
        b->pup_ready = 1;
        b->pup_lean_t = b->pup_lean = 0;
      } else if (!strcmp(sub, "off")) {
        b->puppet = 0;
      } else if (!strcmp(sub, "pitch")) {   // `puppet pitch DEG`: + looks up
        b->pitch = f_clamp(next_f() * DEG2RAD, -PITCH_MAX, PITCH_MAX);
      } else if (!strcmp(sub, "cook")) {
        // `puppet cook C`: the grenade in hand, C ticks since the press (the pin
        // pull runs to NADE_PIN_T, past it the fuse is burning). 0 = the hold.
        b->wp.cur = WPN_GR;
        b->wp.switch_t = 0;
        b->wp.gr_t = 0;
        b->wp.gr_cook = (int)next_n();
      } else if (!strcmp(sub, "nade")) {
        // `puppet nade T [lob]`: hold the grenade throw at tick T (1..NADE_THROW_T)
        // so the third-person throw can be photographed frame by frame; 0 is the
        // plain hold. botfreeze keeps the clock where it is put.
        b->wp.cur = WPN_GR;
        b->wp.switch_t = 0;
        b->wp.gr_cook = 0;
        b->wp.gr_fuse = NADE_FUSE - NADE_PIN_T;
        b->wp.gr_t = (int)next_n();
        char *lob = take_tok();
        b->wp.gr_lob = lob && !strcmp(lob, "lob");
        if (lob && !b->wp.gr_lob) g_tok_pushback = lob;   // not ours: the next command
      } else if (!strcmp(sub, "move")) {
        b->pup_mx = next_f();
        b->pup_mz = next_f();
      } else if (!strcmp(sub, "speed")) {
        b->pup_speed = next_f();
      } else if (!strcmp(sub, "face")) {
        b->pup_face = next_f() * DEG2RAD;
      } else if (!strcmp(sub, "warp")) {  // teleport for camera framing
        b->pos.x = next_f();
        b->pos.z = next_f();
        b->pos.y = 0;
        // never warp INTO geometry: step up until the body fits (platforms)
        while (b->pos.y < 3.0f && !aabb_fits(b->pos, PLAYER_RADIUS, HEIGHT_STAND))
          b->pos.y += 0.05f;
        b->prev_pos = b->pos;
        b->vel = (v3){{0, 0, 0}};
        anim_reset(&b->anim, b->pos, b->yaw);
      } else if (!strcmp(sub, "crouch")) {
        b->pup_crouch = (int)next_n();
      } else if (!strcmp(sub, "ready")) {
        b->pup_ready = (int)next_n();
      } else if (!strcmp(sub, "ads")) {
        b->pup_ads = (int)next_n();
      } else if (!strcmp(sub, "lean")) {
        b->pup_lean_t = f_clamp(next_f(), -1.0f, 1.0f);
      } else if (!strcmp(sub, "jump")) {
        b->pup_jump = 1;
      } else if (!strcmp(sub, "slide")) {
        b->pup_slide = 1;
      } else {
        fprintf(stderr, "script: unknown puppet sub-command '%s'\n", sub);
        exit(1);
      }
    } else if (!strcmp(t, "tacstat")) {
      // Census of the tactical layer: how often bots actually slide, crouch,
      // jump and hold a pre-aim. Behaviour that never fires is behaviour that
      // does not exist, and a census is the only way to know.
      long n = next_n();
      long slides = 0, jumps = 0, cr_ticks = 0, sl_ticks = 0, preaim = 0, tot = 0;
      long cov_ticks = 0, hold_ticks = 0, investigate_ticks = 0, search_ticks = 0;
      long visible_ticks = 0, memory_ticks = 0, reacquired = 0, search_anchors = 0;
      long prefires = 0, prefire_ticks = 0, peeks = 0;
      long flanks = 0, flank_ticks = 0, arrivals = 0, stare_max = 0, pin_max = 0;
      long pushes = 0, breaks = 0, alert_ticks = 0;
      int was_dl[MAX_BOTS] = {0};
      long heard_ticks = 0, startles = 0;
      int was_pf[MAX_BOTS] = {0}, was_fl[MAX_BOTS] = {0}, was_hr[MAX_BOTS] = {0};
      int was_arr[MAX_BOTS] = {0};
      for (int b = 0; b < MAX_BOTS; b++) was_arr[b] = g_bots[b].flank_arrived;
      int was_sl[MAX_BOTS] = {0}, was_air[MAX_BOTS] = {0};
      int was_memory[MAX_BOTS] = {0}, was_searching[MAX_BOTS] = {0};
      int was_search_index[MAX_BOTS] = {0};
      v3 was_search_pos[MAX_BOTS];
      for (int b = 0; b < MAX_BOTS; b++) {
        const bot_t *bt = &g_bots[b];
        was_memory[b] = bt->active && bt->alive && bt->tgt >= 0 &&
                        bt->belief != BOT_BELIEF_VISIBLE;
        was_searching[b] = bt->active && bt->alive && bt->tgt >= 0 &&
                           (bt->belief == BOT_BELIEF_INVESTIGATE ||
                            bt->belief == BOT_BELIEF_SEARCH_SWEEP);
        was_search_index[b] = bt->search_index;
        was_search_pos[b] = bt->search_pos;
      }
      for (long i = 0; i < n; i++) {
        sim_advance(s);
        for (int b = 0; b < g_match.nbots; b++) {
          const bot_t *bt = &g_bots[b];
          if (!bt->active || !bt->alive) {
            was_sl[b] = 0;
            was_air[b] = 0;
            was_memory[b] = 0;
            was_searching[b] = 0;
            was_search_index[b] = bt->search_index;
            was_search_pos[b] = bt->search_pos;
            continue;
          }
          tot++;
          if (bt->sliding) sl_ticks++;
          if (bt->crouch) cr_ticks++;
          if (bt->sliding && !was_sl[b]) slides++;
          if (!bt->grounded && was_air[b] == 0 && bt->vel.y > 1.0f) jumps++;
          if (bt->tgt >= 0 && bt->lose_t > 0) preaim++;
          if (bt->prefiring) prefire_ticks++;
          if (bt->prefiring && !was_pf[b]) prefires++;
          was_pf[b] = bt->prefiring;
          if (bt->peek) peeks++;
          if (bt->flank_t > 0) flank_ticks++;
          if (bt->flank_t > 0 && !was_fl[b]) flanks++;
          was_fl[b] = bt->flank_t > 0;
          // ARRIVALS are what separate flanking from wandering with a goal, and
          // they are the only census term that catches a commitment that
          // silently stopped completing — it read a flat 0 while every swing
          // that broke the line was left un-walked. Measured 0.26-0.62 of
          // starts, mean ~0.40 (8 bots, 24000 ticks, five seeds x two tiers):
          // the rest are aborted by the reload/hurt/slide gates, which is the
          // behaviour wanted, so the signal to watch is a COLLAPSE toward 0,
          // not a number below a half.
          arrivals += bt->flank_arrived - was_arr[b];
          was_arr[b] = bt->flank_arrived;
          if (bt->stare > stare_max) stare_max = bt->stare;
          // The deadlock DECISION, split by which way it went — a tier that
          // only ever breaks is a tier that reads as cowardice, and one that
          // only ever pushes stops being a decision at all.
          if (bt->dl_t > 0 && !was_dl[b]) { if (bt->dl_push) pushes++; else breaks++; }
          was_dl[b] = bt->dl_t > 0;
          if (bt->alert_t > 0 && bt->tgt < 0) alert_ticks++;
          if (bt->pin > pin_max) pin_max = bt->pin;
          if (bt->startled) heard_ticks++;
          if (bt->startled && !was_hr[b]) startles++;
          was_hr[b] = bt->startled;
          if (bt->in_cover) cov_ticks++;
          int memory = bt->tgt >= 0 && bt->belief != BOT_BELIEF_VISIBLE;
          int searching = bt->tgt >= 0 &&
                          (bt->belief == BOT_BELIEF_INVESTIGATE ||
                           bt->belief == BOT_BELIEF_SEARCH_SWEEP);
          if (bt->tgt >= 0 && bt->belief == BOT_BELIEF_VISIBLE) visible_ticks++;
          if (bt->belief == BOT_BELIEF_OCCLUDED_HOLD) hold_ticks++;
          if (bt->belief == BOT_BELIEF_INVESTIGATE) investigate_ticks++;
          if (bt->belief == BOT_BELIEF_SEARCH_SWEEP) search_ticks++;
          if (memory) memory_ticks++;
          if (was_memory[b] && bt->tgt >= 0 && bt->belief == BOT_BELIEF_VISIBLE)
            reacquired++;
          if (searching) {
            v3 d = v3_sub(bt->search_pos, was_search_pos[b]);
            float moved2 = d.x * d.x + d.y * d.y + d.z * d.z;
            if (!was_searching[b] || bt->search_index != was_search_index[b] ||
                moved2 > BOT_TARGET_EPS * BOT_TARGET_EPS)
              search_anchors++;
          }
          was_sl[b] = bt->sliding;
          was_air[b] = !bt->grounded;
          was_memory[b] = memory;
          was_searching[b] = searching;
          was_search_index[b] = bt->search_index;
          was_search_pos[b] = bt->search_pos;
        }
      }
      printf("tacstat ticks=%ld botticks=%ld slides=%ld slide_pct=%.1f jumps=%ld "
             "crouch_pct=%.1f preaim_pct=%.1f cover_pct=%.1f hold=%ld "
             "investigate=%ld search=%ld visible=%ld reacquisition=%ld "
             "memory_pct=%.1f investigate_pct=%.1f search_pct=%.1f "
             "reacquired=%ld searchanchors=%ld prefires=%ld prefire_pct=%.1f "
             "peeks=%ld flanks=%ld flank_pct=%.1f flank_arrive=%ld "
             "startles=%ld hear_pct=%.1f stare_max=%ld pin_max=%ld "
             "pushes=%ld breaks=%ld alert_pct=%.1f\n",
             n, tot, slides,
             tot ? 100.0 * (double)sl_ticks / (double)tot : 0.0, jumps,
             tot ? 100.0 * (double)cr_ticks / (double)tot : 0.0,
             tot ? 100.0 * (double)preaim / (double)tot : 0.0,
             tot ? 100.0 * (double)cov_ticks / (double)tot : 0.0,
             hold_ticks, investigate_ticks, search_ticks, visible_ticks,
             reacquired, tot ? 100.0 * (double)memory_ticks / (double)tot : 0.0,
             tot ? 100.0 * (double)investigate_ticks / (double)tot : 0.0,
             tot ? 100.0 * (double)search_ticks / (double)tot : 0.0,
             reacquired, search_anchors, prefires,
             tot ? 100.0 * (double)prefire_ticks / (double)tot : 0.0, peeks,
             flanks, tot ? 100.0 * (double)flank_ticks / (double)tot : 0.0,
             arrivals, startles,
             tot ? 100.0 * (double)heard_ticks / (double)tot : 0.0, stare_max,
             pin_max, pushes, breaks,
             tot ? 100.0 * (double)alert_ticks / (double)tot : 0.0);
    } else if (!strcmp(t, "figv")) {
      g_fchk_v = !g_fchk_v;  // locate every offending triangle, not just count
    } else if (!strcmp(t, "netdebug")) {
      g_net_debug = !g_net_debug;  // per-correction/trim/skip lines, see decl
      printf("netdebug %s\n", g_net_debug ? "on" : "off");
    } else if (!strcmp(t, "figcheck")) {
      long n = next_n();
      // Optional camera DISTANCE, because the invariants have to hold on every
      // profile tier and nothing reached the far ones. `g_fig_k` is chosen from
      // how far the figure is from `g_cam_eye`, and every command that could
      // have checked the coarse tiers pinned it to the finest: figcheck put the
      // camera ON the bot, vmtrig's third-person branch does the same, and
      // vm_build hard-sets FIG_KMAX. So the two-node merged finger mass — the
      // shape that covers everything past 6 m, i.e. most of a 48 m arena — had
      // no gate at all. 0 keeps the old behaviour exactly.
      float fdist = (float)opt_n(0);

      fchk_res_t worst = {0};
      long wt = s->tick, tris = 0;
      int bad = 0, wcross = 0;   // cross has its own worst; see vmcheck's note
      // The whole figure is now body surface: the ground blob disk it used to
      // emit first (an open fan, which had to be skipped) is gone, replaced by a
      // real cast shadow that lives in the shadow map and not in this buffer.
      // Under `figv` the part table is recorded too, so an offender is reported
      // by NAME rather than by triangle index — see fchk_part.
      for (long i = 0; i < (n > 0 ? n : 1); i++) {
        if (n > 0) sim_advance(s);
        scene_reset();
        // Check the mesh the player sees up close: scene_draw_figure picks its
        // profile resolution from the camera distance, and the invariants have to
        // hold on the most detailed variant.
        g_cam_eye = g_bots[0].pos;
        if (fdist > 0.0f) g_cam_eye.z += fdist;
        if (g_fchk_v) { g_nprobe = 0; g_probe_on = 1; }
        scene_draw_figure(&g_bots[0].anim, g_bots[0].pos, g_bots[0].yaw, NULL,
                          1.0f, 0.0f, (v3){{1, 1, 1}}, 0, WPN_AR,
                          gun_anim(&g_bots[0].wp, 1.0f));
        g_probe_on = 0;
        fchk_res_t r = fchk_scene();
        tris = r.tris;
        int sc = 100 * (r.open + r.flip + r.dup + r.degen) + 10 * r.zfight + r.near;
        if (r.cross > wcross) wcross = r.cross;   // its own worst; see vmcheck
        if (sc > bad) { bad = sc; worst = r; wt = s->tick; }
      }
      printf("figcheck ticks=%ld dist=%.0f k=%d tris=%d worst_t=%ld open=%d "
             "flip=%d dup=%d zfight=%d near=%d cross=%d degen=%d\n", n,
             (double)fdist, g_fig_k, (int)tris, wt, worst.open, worst.flip,
             worst.dup, worst.zfight, worst.near, wcross, worst.degen);
    } else if (!strcmp(t, "figins")) {
      // figins 0|1|-1: draw every figure as an operator / an insurgent / as
      // the seat decides (default) — so figcheck can gate both builds.
      long v = next_n();
      g_fig_ins_force = v < 0 ? -1 : v ? 1 : 0;
    } else if (!strcmp(t, "figbury")) {
      figbury_proof();
    } else if (!strcmp(t, "figm")) {
      figm_proof();
    } else if (!strcmp(t, "kine")) {
      // Per-tick kinematics of bot 0's SOLVED pose: what the eye actually sees.
      // `skel` prints the foot controller's plant anchors; this prints where the
      // rendered boot soles and gloves really are, which is the only way to hold
      // a contact claim ("the heel is on the ground at this tick") to a number.
      long n = next_n();
      while (n-- > 0) {
        sim_advance(s);
        const bot_t *b = &g_bots[0];
        const anim_t *A = &b->anim;
        const pose_t *P = &A->pose;
        printf("kine t=%ld pos=(%.3f %.3f %.3f) air=%.2f sl=%.2f hd=%.2f ly=%.4f "
               "pel=(%.3f %.3f %.3f) head=%.4f",
               s->tick, (double)b->pos.x, (double)b->pos.y, (double)b->pos.z,
               (double)A->air_s, (double)A->slide_s, (double)A->hdown,
               (double)A->leg_yaw, (double)P->pelvis.x, (double)P->pelvis.y,
               (double)P->pelvis.z, (double)P->head.y);
        for (int i = 0; i < 2; i++) {
          v3 ank = P->ankle[i], toe = P->toe[i];
          v3 fF = v3_sub(toe, ank);
          fF = v3_dot(fF, fF) > 1e-8f ? v3_norm(fF) : (v3){{0, 0, 1}};
          // Mirror the renderer's boot chain exactly (see scene_draw_figure):
          // av = ankle - 0.028, every ring's centre sits at (rx - H) along the
          // spine so centre - rx is ONE plane per piece, and the lowest of those
          // is the OUTSOLE's at av - SOLE_H. If this drifts from the renderer
          // the proof silently validates a boot nobody can see.
          v3 heel = v3_add(v3_add(ank, (v3){{0, -0.028f, 0}}), v3_scale(fF, -0.064f));
          // The renderer lifts the boot by its own worst dip (see `sole_dip` in
          // scene_draw_figure), so the lowest sole vertex is ON the plane through
          // av at every foot pitch by construction. What is left to measure is
          // the FOOT CONTROLLER: whether the ankle it planted is at the height a
          // sole on the floor implies. That is a stronger contract than the old
          // one and it is the one that can fail.
          // ...which, once the lift is added to av and the ring's own fF.y*T
          // offset is added back, is exactly ankle.y - ANIM_ANKLE_Y. Printing
          // `av - SOLE_H` instead over-reports by the lift itself and made a
          // pitched boot look like it was floating 67 mm.
          float sole = ank.y - b->pos.y - ANIM_ANKLE_Y;
          printf(" %c=(%.4f %.4f %.4f)%c sp=%.2f toe=%.4f heel=%.4f sole=%.4f",
                 i ? 'R' : 'L', (double)ank.x, (double)ank.y, (double)ank.z,
                 A->foot[i].t >= 1.0f ? '*' : '~', (double)A->stance[i],
                 (double)toe.y, (double)heel.y, (double)sole);
        }
        for (int i = 0; i < 2; i++)
          printf(" %cw=(%.4f %.4f %.4f)", i ? 'R' : 'L', (double)P->wri[i].x,
                 (double)P->wri[i].y, (double)P->wri[i].z);
        printf("\n");
      }
    } else if (!strcmp(t, "skel")) {
      long n = next_n();
      while (n-- > 0) {
        sim_advance(s);
        const bot_t *b = &g_bots[0];
        const anim_t *A = &b->anim;
        printf("skel t=%ld spd=%.2f phase=%.2f air=%.2f crouch=%.2f slide=%.2f "
               "lean=%.2f yaw=%.2f leg=%.2f L=(%.3f %.3f %.3f)%s R=(%.3f %.3f %.3f)%s "
               "pelvis=(%.3f %.3f %.3f)\n",
               s->tick, (double)A->spd, (double)A->phase, (double)A->air_s,
               (double)A->crouch_s, (double)A->slide_s, (double)A->lean_s,
               (double)b->yaw, (double)A->leg_yaw,
               (double)A->foot[0].pos.x, (double)A->foot[0].pos.y,
               (double)A->foot[0].pos.z, A->foot[0].t >= 1.0f ? "*" : "~",
               (double)A->foot[1].pos.x, (double)A->foot[1].pos.y,
               (double)A->foot[1].pos.z, A->foot[1].t >= 1.0f ? "*" : "~",
               (double)A->pose.pelvis.x, (double)A->pose.pelvis.y,
               (double)A->pose.pelvis.z);
      }
    } else if (!strcmp(t, "botfreeze")) {
      char *arg = take_tok();
      if (!arg) g_bots_frozen = !g_bots_frozen;
      else if (!strcmp(arg, "on")) g_bots_frozen = 1;
      else if (!strcmp(arg, "off")) g_bots_frozen = 0;
      else {
        g_bots_frozen = !g_bots_frozen;
        g_tok_pushback = arg;
      }
    } else if (!strcmp(t, "hud")) {
      g_hud_visible = next_on_off("hud");
    } else if (!strcmp(t, "dev")) {
      // DEV remains a two-level gate: the runtime config exposes the Root
      // action, while DEV_MENU stays the compile-time hard off switch.
      g_cfg.devmode = next_on_off("dev");
    } else if (!strcmp(t, "showfps")) {
      g_cfg.show_fps = next_on_off("showfps");
    } else if (!strcmp(t, "armory")) {
      // armory rifle|pistol|knife|theme|gear|head|vest|pack N — the ARMORY
      // page's cycles, by index.
      const char *what = next_tok();
      long n = next_n();
      int *dst = !strcmp(what, "rifle") ? &g_cfg.rifle_var
               : !strcmp(what, "pistol") ? &g_cfg.pistol_var
               : !strcmp(what, "knife") ? &g_cfg.knife_model
               : !strcmp(what, "theme") ? &g_cfg.wpn_theme
               : !strcmp(what, "gear")  ? &g_cfg.gear_theme
               : !strcmp(what, "head")  ? &g_cfg.head_gear
               : !strcmp(what, "vest")  ? &g_cfg.vest_gear
               : !strcmp(what, "pack")  ? &g_cfg.pack_gear : NULL;
      int cnt = dst == &g_cfg.rifle_var || dst == &g_cfg.pistol_var ? WPN_VARS
              : dst == &g_cfg.knife_model ? KNIFE_MODEL_COUNT
              : dst == &g_cfg.wpn_theme   ? WPN_THEME_COUNT
              : dst == &g_cfg.head_gear   ? HEAD_GEAR_COUNT
              : dst == &g_cfg.vest_gear   ? VEST_GEAR_COUNT
              : dst == &g_cfg.pack_gear   ? PACK_GEAR_COUNT : GEAR_THEME_COUNT;
      if (!dst || n < 0 || n >= cnt) {
        fprintf(stderr, "script: armory rifle|pistol 0-%d | knife 0-%d | theme 0-%d | gear 0-%d"
                " | head 0-%d | vest 0-%d | pack 0-%d\n",
                WPN_VARS - 1, KNIFE_MODEL_COUNT - 1, WPN_THEME_COUNT - 1, GEAR_THEME_COUNT - 1,
                HEAD_GEAR_COUNT - 1, VEST_GEAR_COUNT - 1, PACK_GEAR_COUNT - 1);
        exit(1);
      }
      *dst = (int)n;
    } else if (!strcmp(t, "bots")) {
      long n = next_n();
      g_cfg.sp_bots = parse_int_def(n, &SP_BOTS_DEF);
    } else if (!strcmp(t, "skill")) {
      // The trailing fallback used to be a bare `: 1`, so `skill hrd` silently
      // measured the NORMAL tier — on the one command the documented difficulty
      // ladder is measured with. Every other enum-valued argument here errors.
      const char *n = next_tok();
      int d = !strcmp(n, "easy") ? 0 : !strcmp(n, "normal") ? 1
            : !strcmp(n, "hard") ? 2 : -1;
      if (d < 0) {
        fprintf(stderr, "script: unknown skill '%s' (easy|normal|hard)\n", n);
        exit(1);
      }
      g_cfg.sp_diff = d;
    } else if (!strcmp(t, "fraglimit")) {
      long n = next_n();
      g_cfg.sp_frag = parse_int_def(n, &SP_FRAG_DEF);
    } else if (!strcmp(t, "usemap")) {
      // The MAP setting, like the menu's: lands on the next `restart`.
      const char *n = next_tok();
      int m = map_parse(n);
      if (m < 0) {
        fprintf(stderr, "script: unknown map '%s' (arena|foundry|bazaar|outpost|citadel|harbor|refinery|compound|rotate)\n", n);
        exit(1);
      }
      g_cfg.sp_map = m;
    } else if (!strcmp(t, "rounds")) {
      long n = next_n();
      g_cfg.sp_rounds = parse_int_def(n, &SP_ROUNDS_DEF);
    } else if (!strcmp(t, "mode")) {
      // Sets the MATCH tab's choice; like the menu it lands on the next
      // `restart`, which is what a script follows it with.
      const char *n = next_tok();
      int m = gm_parse(n);
      if (m < 0) {
        fprintf(stderr, "script: unknown mode '%s' (dm|gun|sniper|blades|lms|rescue|tdm)\n", n);
        exit(1);
      }
      g_cfg.sp_mode = m;
    } else if (!strcmp(t, "teamsize")) {        // TEAM DEATHMATCH settings, like the
      g_cfg.sp_team_size = (int)next_n();       // MATCH page: next `restart`/tick
    } else if (!strcmp(t, "teamfill")) {
      g_cfg.sp_team_fill = next_n() ? 1 : 0;
    } else if (!strcmp(t, "teamff")) {
      g_cfg.sp_team_ff = next_n() ? 1 : 0;
    } else if (!strcmp(t, "squad")) {           // HOSTAGE RESCUE squad size: next `restart`
      long n = next_n();
      g_cfg.sp_squad = n < 1 ? 1 : n > RESC_SQUAD_MAX ? RESC_SQUAD_MAX : (int)n;
    } else if (!strcmp(t, "team")) {            // the local player's side (0/1)
      g_cfg.sp_team = next_n() ? 1 : 0;
    } else if (!strcmp(t, "teams")) {
      // Each side: its members (h = human), alive count, score; then whether
      // any two teammates have ever damaged each other (ffhits must be 0).
      for (int tm = 0; tm < 2; tm++) {
        printf("team %s score=%d members=", TEAM_NAMES[tm], g_team_score[tm]);
        int n2 = 0, al = 0, zs = 0;
        ENT_FOREACH(e) if (g_team[e] == tm) {
          printf("%s%s%s", n2 ? "," : "", ent_name(e), human_of_ent(e) >= 0 ? "(h)" : "");
          n2++; al += ent_alive(e);
          zs += ent_pos(e).z < 0.0f;
        }
        printf(" n=%d alive=%d on_minus_z=%d\n", n2, al, zs);
      }
      int ksum = 0;
      ENT_FOREACH(e) ksum += g_match.kills[e];
      int locks = 0;
      for (int i = 0; i < g_match.nbots; i++)
        if (g_bots[i].active && g_bots[i].tgt >= 0 && team_mates(i + 1, g_bots[i].tgt)) locks++;
      printf("teams kills=%d (side frags %d) mate_locks=%d ff=%d\n", ksum,
             g_team_score[0] + g_team_score[1], locks, team_ff());
      printf("teams on=%d size=%d fill=%d nbots=%d over=%d winner=%s\n", teams_on(),
             team_size(), g_cfg.sp_team_fill, g_match.nbots, g_match.over,
             g_match.over ? ent_name(g_match.winner) : "none");
    } else if (!strcmp(t, "rescue")) {
      // HOSTAGE RESCUE state: the mission line, every guard, every hostage.
      // `rescue show` (the mission line) | `rescue all` (+ guards, hostages) |
      // `rescue free N` (cut hostage N loose) | `rescue alarm`.
      const char *a = next_tok();
      if (!strcmp(a, "free")) {
        long k = next_n();
        if (k >= 0 && k < RESC_HOSTAGES && g_resc.h[k].state == HS_CAPTIVE) {
          g_resc.h[k].state = HS_FOLLOW;
          g_resc.h[k].leader = g_local_ent;
          g_resc.freed++;
        }
        continue;
      }
      if (!strcmp(a, "alarm")) { rescue_alarm(RW_SPOTTED); continue; }
      printf("rescue t=%ld on=%d alarm=%d exec=%d freed=%d extracted=%d result=%d why=%d "
             "kills=%d td=%d heads=%d spotted=%d bodies=%d lit=%d near=%d clock=%d\n",
             s->tick, rescue_on(), g_resc.alarm, g_resc.exec_t, g_resc.freed, g_resc.extracted,
             g_resc.result, g_resc.why, g_resc.kills, g_resc.takedowns, g_resc.heads,
             g_resc.spotted_n, g_resc.bodies, g_resc.lit, g_resc.reach_i, g_resc.clock);
      printf("rescue squad=%d guards=%d nbots=%d humans=%d\n", g_resc.squad, g_resc.nguards,
             g_match.nbots, resc_humans());
      if (!strcmp(a, "all")) {
        for (int i = 0; i < g_match.nbots; i++) {
          const bot_t *b = &g_bots[i];
          int gd = i < g_resc.nguards;
          printf("%s %d kind=%d alive=%d pos=(%.1f %.1f %.1f) yaw=%.2f det=%.2f who=%d tgt=%d alert=%d crouch=%d wpn=%s\n",
                 gd ? "guard" : "squadbot", i, gd ? rescue_post(i)->kind : -1, b->alive,
                 (double)b->pos.x, (double)b->pos.y, (double)b->pos.z, (double)b->yaw,
                 gd ? (double)g_resc.detect[i] : 0.0, gd ? g_resc.det_who[i] : -1, b->tgt,
                 b->alert_t, b->crouch, WPN_DEF[b->wp.cur].name);
        }
        for (int i = 0; i < RESC_HOSTAGES; i++)
          printf("hostage %d state=%d leader=%d pos=(%.1f %.1f %.1f) free=%.2f\n", i, g_resc.h[i].state,
                 g_resc.h[i].leader, (double)g_resc.h[i].pos.x, (double)g_resc.h[i].pos.y,
                 (double)g_resc.h[i].pos.z, (double)g_resc.h[i].free_t);
      }
    } else if (!strcmp(t, "rescuecheck")) {
      // Every post and waypoint stands on floor a body fits on, and the grid
      // routes reach every hostage, the LZ and each next waypoint.
      nav_ready();
      int bad = 0;
      v3 ins = {{RESC_INSERT_X, 0.0f, RESC_INSERT_Z}};
      for (int i = 0; i < RESC_POSTS; i++) {
        const guard_post_t *gp = &GUARD_POSTS[i];
        v3 c = {{gp->x, gp->y + 0.01f, gp->z}};
        if (!aabb_fits(c, PLAYER_RADIUS, HEIGHT_STAND)) { printf("rescuecheck post %d blocked\n", i); bad++; }
      }
      for (size_t r = 0; r < sizeof GUARD_ROUTES / sizeof GUARD_ROUTES[0]; r++)
        for (int k = 0; k < GUARD_ROUTES[r].n; k++) {
          v3 a = {{GUARD_ROUTES[r].p[k][0], 0.01f, GUARD_ROUTES[r].p[k][1]}};
          int k2 = (k + 1) % GUARD_ROUTES[r].n;
          v3 b2 = {{GUARD_ROUTES[r].p[k2][0], 0.0f, GUARD_ROUTES[r].p[k2][1]}};
          if (!aabb_fits(a, PLAYER_RADIUS, HEIGHT_STAND)) { printf("rescuecheck route %d wp %d blocked\n", (int)r, k); bad++; continue; }
          int g2 = nav_snap(b2), s2 = nav_snap(a);
          if (g2 < 0 || s2 < 0 || nav_field_for(g2)[s2] == 0xFFFF) { printf("rescuecheck route %d leg %d unrouted\n", (int)r, k); bad++; }
        }
      v3 goals[RESC_HOSTAGES + 1];
      for (int i = 0; i < RESC_HOSTAGES; i++) goals[i] = (v3){{RESC_HOLD[i][0], RESC_HOLD[i][2], RESC_HOLD[i][1]}};
      goals[RESC_HOSTAGES] = (v3){{22.0f, 0.0f, -41.5f}};
      int si = nav_snap(ins);
      for (int i = 0; i <= RESC_HOSTAGES; i++) {
        int gi = nav_snap(goals[i]);
        const uint16_t *f = gi >= 0 ? nav_field_for(gi) : NULL;
        // The roof is climbed by hand (a 0.4 m stair is two steps a grid cell,
        // which the routes do not climb); what must hold there is the way DOWN.
        int ok = goals[i].y > 1.0f || (f && si >= 0 && f[si] != 0xFFFF);
        if (i < RESC_HOSTAGES && goals[i].y < 1.0f) {   // ...and back out to the LZ
          int lz = nav_snap(goals[RESC_HOSTAGES]);
          const uint16_t *fl = lz >= 0 ? nav_field_for(lz) : NULL;
          ok = ok && fl && fl[gi] != 0xFFFF;
        }
        printf("rescuecheck goal %d %s steps=%d\n", i, ok ? "ok" : "UNREACHABLE", f && si >= 0 ? f[si] : -1);
        if (!ok) bad++;
      }
      printf("rescuecheck bad=%d\n", bad);
    } else if (!strcmp(t, "restart")) {
      g_match.restart_req = 1;
    } else if (!strcmp(t, "match")) {
      // humans= is THE cheapest observable of the whole entity model (P1-64):
      // the only output that would show a missing human at all — the byte
      // gate stays green while half a roster is invisible.
      int nh = 0;
      for (int h = 0; h < MAX_HUMANS; h++)
        if (g_humans_on[h]) nh++;
      printf("match t=%ld over=%d winner=%s bots=%d humans=%d frag=%d diff=%d "
             "php=%d palive=%d mode=%s map=%s round=%d round_t=%d\n",
             s->tick, g_match.over, g_match.over ? ent_name(g_match.winner) : "none",
             g_match.nbots, nh, g_cfg.sp_frag, g_cfg.sp_diff, s->p->hp, s->p->alive,
             GM_KEYS[game_mode()], MAP_DEFS[map_id_valid(g_map_id)].key,
             g_match.round_no, g_match.round_t);
      ENT_FOREACH(e) {
        const weapon_t *ew = ent_weapon(e);
        printf("score %s k=%d d=%d shots=%d hits=%d acc=%d%% m=%d wpn=%s alive=%d\n",
               ent_name(e),
               g_match.kills[e], g_match.deaths[e], g_match.shots[e], g_match.hits[e],
               g_match.shots[e] ? g_match.hits[e] * 100 / g_match.shots[e] : 0,
               g_match.mscore[e], ew ? WPN_DEF[ew->cur].prefix : "-", ent_alive(e));
      }
    } else if (!strcmp(t, "sfxlog")) {
      // Every audio decision the game makes, for N ticks, as text. Synthesis
      // can be judged from a WAV; ROUTING cannot — whether a shot 30 m away
      // crossfades to the far bake, whether it is panned to the correct side,
      // whether the hit tick is actually offset out from under the gunshot,
      // and whether a firefight ever exceeds the voice budget are all decisions
      // with no waveform to inspect. This is the only test they have.
      long n = next_n();
      g_sfx_log_n = 0;
      g_sfx_log_drops = 0;
      g_sfx_log_on = g_audio_route = 1;
      for (long i = 0; i < n; i++) {
        sim_advance(s);
        // Routing without a mixer: nothing consumes the trigger ring, so the
        // command plays the consumer itself (mixdown's own idiom) — otherwise
        // the ring reads permanently flooded after 64 pushes and every later
        // trigger of the run counts as a bogus flood.
        atomic_store(&g_trig_r, atomic_load(&g_trig_w));
      }
      g_sfx_log_on = g_audio_route = 0;
      float peak_t = -1.0f;
      int overlap = 0, worst = 0;
      for (int i = 0; i < g_sfx_log_n; i++) {
        const sfx_log_t *l = &g_sfx_log[i];
        printf("sfxlog t=%ld %-9s gain=%.2f rate=%.2f pan=%+.2f delay=%dms\n",
               l->tick, SFX_NAME[l->id], (double)l->gain, (double)l->rate,
               (double)l->pan, (int)(l->delay * 1000.0f));
        // How many sounds are alive at this trigger's start: the voice budget
        // is 24 and a 20-bot firefight is the case that finds out.
        float t0 = (float)l->tick / TICK_HZ + l->delay;
        overlap = 0;
        for (int j = 0; j < g_sfx_log_n; j++) {
          float tj = (float)g_sfx_log[j].tick / TICK_HZ + g_sfx_log[j].delay;
          float dur = (float)g_sfx_len[g_sfx_log[j].id] / SFX_HZ;
          if (tj <= t0 && tj + dur > t0) overlap++;
        }
        if (overlap > worst) { worst = overlap; peak_t = t0; }
      }
      printf("sfxlog total=%d ticks=%ld max_concurrent=%d at t=%.2fs "
             "(voices=%d)", g_sfx_log_n, n, worst, (double)peak_t, MAX_VOICES);
      // The one state this command exists for (a firefight past the budget) is
      // also the one that overflows the log — say so instead of presenting a
      // truncated prefix as the whole run.
      if (g_sfx_log_drops)
        printf(" SATURATED log_drops=%ld", g_sfx_log_drops);
      putchar('\n');
    } else if (!strcmp(t, "sfx")) {
      const char *dir = next_tok();
      for (int i = 0; i < SFX_COUNT; i++) {
        if (!g_sfx_buf[i]) continue;   // a sampled-only id the pack does not carry
        char path[560];
        snprintf(path, sizeof path, "%s/%s.wav", dir, SFX_NAME[i]);
        write_wav(path, g_sfx_buf[i], g_sfx_len[i]);
        an_report(SFX_NAME[i], g_sfx_buf[i], g_sfx_len[i], 0.30f);
      }
    } else if (!strcmp(t, "mixdown")) {
      // The bus, rendered. Every other audio test looks at ONE buffer in
      // isolation; this is the only one that sees what the player hears — the
      // scheduled delays actually elapsing, voices stealing slots from each
      // other, and the master limiter ducking everything because one sound was
      // made "punchier". A confirmation that measures +8 dB over its own shot
      // as a pair of WAVs can still arrive 2 dB DOWN once the limiter has been
      // driven by both of them, and nothing before this could tell.
      const char *path = next_tok();
      long n = next_n();
      if (n < 1) n = 1;
      const int fpt = SFX_HZ / TICK_HZ;            // exactly 400 frames a tick
      const long tail = 3 * TICK_HZ;               // let the last decays finish
      long total = n + tail;
      float *mix = calloc((size_t)total * (size_t)fpt * SFX_CH, sizeof *mix);
      if (!mix) fatal("out of memory");
      g_dev_rate = 1.0f;            // the harness "device" IS 48 kHz
      g_lim_g = g_lim_min = g_duck_min = 1.0f;
      g_duck_env = 0.0f;
      g_voice_peak = 0;
      g_trig_dropped = 0;
      g_trig_capped = 0;
      g_trig_flooded = 0;
      for (int i = 0; i < MAX_VOICES; i++) g_voices[i].active = 0;
      atomic_store(&g_trig_r, atomic_load(&g_trig_w));   // drop stale triggers
      g_audio_route = 1;
      for (long i = 0; i < total; i++) {
        if (i < n) sim_advance(s);
        audio_mix(mix + (size_t)i * (size_t)fpt * SFX_CH, (uint32_t)fpt, SFX_CH);
      }
      g_audio_route = 0;
      int frames = (int)(total * fpt);
      write_wav(path, mix, frames);
      long clip = 0;
      for (long k = 0; k < (long)frames * SFX_CH; k++)
        if (mix[k] >= 0.999f || mix[k] <= -0.999f) clip++;
      an_report("bus", mix, frames, 0.0f);
      printf("mixdown %s ticks=%ld voices_peak=%d/%d dropped=%ld capped=%ld "
             "limiter=%.2f (%+.1fdB) duck=%.2f (%+.1fdB) clipped=%ld\n", path, n,
             g_voice_peak, MAX_VOICES, g_trig_dropped, g_trig_capped,
             (double)g_lim_min,
             20.0 * log10((double)g_lim_min + 1e-9), (double)g_duck_min,
             20.0 * log10((double)g_duck_min + 1e-9), clip);
      // Printed only when it happened, so every healthy baseline stays
      // byte-identical — the same idiom as `bang`'s pen= field.
      if (g_trig_flooded)
        printf("mixdown WARNING flooded=%ld (trigger ring refused)\n",
               g_trig_flooded);
      free(mix);
    } else if (!strcmp(t, "mapcheck")) {
      // The world's version of figcheck, and nothing covered it before: two solids
      // whose SAME-facing faces land on one plane z-fight at every distance, and
      // on 5 m wall faces that is the most visible flicker in the game. Opposite-
      // facing coplanar faces (a crate standing on a platform) are fine — one of
      // the two is always back-facing — so only min/min and max/max pairs count.
      // Runs over N freshly generated arenas, since this is a property of the
      // GENERATOR, not of one seed.
      long n = next_n();
      if (n < 1) n = 1;
      long worst = 0, total = 0;
      // The check is a property of the GENERATOR, so it walks the seed chain
      // (map_seed_next, the same chain `restart` advances) and must put it
      // back: leaving the seed N steps on desynchronises every later
      // `restart`, and leaving the last probe map loaded without flagging the
      // renderer means every later `shot` photographs an arena the sim is no
      // longer colliding against.
      uint32_t seed0 = g_map_seed;
      uint32_t bad_seed = g_map_seed;
      for (long m = 0; m < n; m++) {
        long hits = 0;
        for (int i = 0; i < g_num_solids; i++)
          for (int j = i + 1; j < g_num_solids; j++) {
            const solid_t *A = &g_solids[i], *B = &g_solids[j];
            const float *amn = &A->min.x, *amx = &A->max.x;
            const float *bmn = &B->min.x, *bmx = &B->max.x;
            for (int a = 0; a < 3; a++) {
              int u = (a + 1) % 3, v = (a + 2) % 3;
              // overlap in the two axes the faces span, with real area
              if (fminf(amx[u], bmx[u]) - fmaxf(amn[u], bmn[u]) <= 0.001f) continue;
              if (fminf(amx[v], bmx[v]) - fmaxf(amn[v], bmn[v]) <= 0.001f) continue;
              // A -y face at ground level is buried in the floor and can never
              // be seen, so two boxes standing on the ground always share that
              // plane and it means nothing. Everything else counts.
              int buried = (a == 1 && amn[a] < 0.001f && bmn[a] < 0.001f);
              int h0 = !buried && fabsf(amn[a] - bmn[a]) < 0.001f;
              int h1 = fabsf(amx[a] - bmx[a]) < 0.001f;
              hits += h0 + h1;
              // MAPCHECK_V=1 names the first pairs, for whoever is fixing them.
              static int shown;
              if ((h0 || h1) && getenv("MAPCHECK_V") && shown < 60) {
                shown++;
                printf("  pair %d %d axis=%c %s (%.2f %.2f %.2f)-(%.2f %.2f %.2f) | (%.2f %.2f %.2f)-(%.2f %.2f %.2f)\n",
                       i, j, "xyz"[a], h0 ? "min" : "max",
                       (double)amn[0], (double)amn[1], (double)amn[2], (double)amx[0], (double)amx[1], (double)amx[2],
                       (double)bmn[0], (double)bmn[1], (double)bmn[2], (double)bmx[0], (double)bmx[1], (double)bmx[2]);
              }
            }
          }
        total += hits;
        if (hits > worst) { worst = hits; bad_seed = g_map_seed; }
        g_map_seed = map_seed_next(g_map_seed);
        map_generate(g_map_seed);
      }
      g_map_seed = seed0;
      map_generate(seed0);
      g_world_dirty = 1;
      printf("mapcheck maps=%ld coplanar_total=%ld worst=%ld worst_seed=%u\n",
             n, total, worst, bad_seed);
    } else if (!strcmp(t, "ballistics")) {
      // Lands on the next `restart`, like the menu toggle.
      g_cfg.ballistics = next_on_off("ballistics");
    } else if (!strcmp(t, "exposure")) {
      // The adaptation state after the last rendered frame: the applied offset
      // in stops and the metered mean luminance it was chasing.
      float px[4] = {0};
      if (R.ad_fbo[R.ad_i]) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, R.ad_fbo[R.ad_i]);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, px);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, R.present_fbo);
      }
      printf("exposure stops=%.3f mean_lum=%.4f\n", (double)px[0], (double)px[1]);
    } else if (!strcmp(t, "postdbg")) {
      // 0 off, 1 SSAO, 2 contact shadow, 3 edge catch-light, 4 volumetric, 5 bloom
      g_post_dbg = (int)next_n();
    } else if (!strcmp(t, "budget")) {
      // A dropped triangle looks exactly like geometry that was never authored,
      // so the peak has to be printable rather than inferred from a screenshot.
      // `ev_peak` is the other silent ceiling in the frame: MAX_EVENTS is a
      // fixed ring and a dropped EV_HIT is a hitmarker that never appeared.
      // ui_peak/ui_drops: the ONE ceiling that had no proof (P0-7) — an
      // overflowing HUD silently stops drawing scoreboard rows.
      printf("budget scene_verts=%d peak=%d max=%d drops=%ld world_verts=%d "
             "ev_peak=%d ev_max=%d ev_drops=%ld part_peak=%d/%d res=%d/%d "
             "world_decor=%d/%d world_drops=%ld ui_peak=%d/%d ui_drops=%ld\n",
             g_scene_floats / VERT_FLOATS, g_scene_peak / VERT_FLOATS,
             SCENE_MAX_VERTS, g_scene_drops, R.world_verts,
             g_ev_peak, MAX_EVENTS, g_ev_drops, g_part_peak, MAX_PARTICLES,
             g_part_res_peak, PARTS_SKID_LO,
             R.world_decor_verts, DECOR_MAX_VERTS, g_mesh_drops,
             g_ui_peak, UI_MAX_VERTS, g_ui_drops);
    } else if (!strcmp(t, "map")) {
      // The theme decides the whole surface treatment now (ripples, glitter,
      // standing water) and it was previously unprintable — you could only find
      // out which one a seed rolled by photographing it.
      { v3 sp = surf_params();
        static const char *SN[] = {"grit", "wet", "sand"};
        printf("map theme=%s wind=%.3f ripple=%.2f glint=%.2f water=%.2f\n",
               SN[g_theme.surf], (double)g_wind_ang,
               (double)sp.x, (double)sp.y, (double)sp.z); }
      for (int i = 0; i < g_num_solids; i++)
        printf("solid %d min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)\n", i,
               (double)g_solids[i].min.x, (double)g_solids[i].min.y, (double)g_solids[i].min.z,
               (double)g_solids[i].max.x, (double)g_solids[i].max.y, (double)g_solids[i].max.z);
    } else if (!strcmp(t, "trace")) {
      long n = next_n();
      while (n-- > 0) { sim_advance(s); sim_print(s); }
    } else if (!strcmp(t, "shot")) {
      const char *path = next_tok();
      render_frame(s->p, 1.0f, 120, &s->cam);
      write_png(path, R.width, R.height);
    } else if (!strcmp(t, "capture")) {
      const char *rgb_path = next_tok();
      const char *wav_path = next_tok();
      long fps = next_n();
      long frames = next_n();
      capture_run(s, rgb_path, wav_path,
                  fps == 60 || fps == 120 ? (int)fps : 0, frames);
    } else if (!strcmp(t, "cam")) {
      char *a = next_tok();
      if (!strcmp(a, "off")) {
        s->cam.active = s->cam.bot = s->cam.orbit = 0;
        continue;
      }
      s->cam.active = 1;
      s->cam.bot = s->cam.orbit = 0;
      s->cam.eye.x = parse_f(a);
      s->cam.eye.y = next_f();
      s->cam.eye.z = next_f();
      s->cam.yaw = next_f();
      s->cam.pitch = next_f();
    } else if (!strcmp(t, "vmhand")) {
      // THE FOUR NUMBERS THAT DECIDE WHETHER A WRIST LOOKS KINKED. A render
      // cannot answer how far a wrist bends — foreshortening hides it, and
      // three rounds of this were judged from screenshots and kept coming back.
      // `bend` is the angle between the palm's axis and the forearm's (a firing
      // grip runs 20-30 degrees; 0 is a splint, 45+ is the original kink),
      // `bore` is how the forearm meets the weapon's axis (a real support arm
      // sits 40-55 degrees off it, 90 is the perpendicular clamp), `palm` is
      // the wrist-to-knuckle chord against ANSUR's 116 mm p50, and `off` is how
      // far the wrist stands off the grip SURFACE.
      // THE RELOAD IS COVERED TOO, and it was the hole in this proof. The
      // complaint that started three rounds of wrist work was "the wrist kinks
      // awkwardly while holding the weapon AND DURING THE RELOAD", and this
      // command only ever measured the two static holds. A reload moves both
      // grip cylinders — the support hand onto a magazine, the trigger hand onto
      // a bolt knob — so the four numbers are different numbers there, and they
      // were never being read. Stations at 15 / 35 / 55 / 75 / 90 percent cover
      // the fetch, the hold, the seat and the bolt cycle.
      player_t tmp = *s->p;
      v3 eye = {{0, 0, 0}}, rgt = {{1, 0, 0}}, upv = {{0, 1, 0}}, fwd = {{0, 0, -1}};
      static const float RSTATION[5] = {0.15f, 0.35f, 0.55f, 0.75f, 0.90f};
      for (int wi = 0; wi < WPN_COUNT; wi++) {
        tmp.wp.cur = wi;
        for (int a = 0; a <= 6; a++) {
          char tag[12];
          tmp.wp.ads_lin = a == 1 ? 1.0f : 0.0f;
          tmp.wp.ads_s = tmp.wp.prev_ads_s = tmp.wp.ads_lin;
          tmp.wp.switch_t = 0; tmp.wp.prev_switch = 0.0f;
          tmp.vm.kick = tmp.vm.prev_kick = 0.0f;
          if (a < 2) {
            tmp.wp.reload_t = 0;
            snprintf(tag, sizeof tag, "%s", a ? "ads" : "hip");
          } else {
            float pr = RSTATION[a - 2];
            tmp.wp.reload_t = (int)((1.0f - pr) * WPN_DEF[wi].reload_ticks + 0.5f);
            snprintf(tag, sizeof tag, "rl%02d", (int)(pr * 100.0f));
          }
          vm_build(&tmp, 1.0f, eye, rgt, upv, fwd);
          for (int h = 0; h < 2; h++)
            printf("vmhand %s %-4s %-7s bend=%5.1f deg  bore=%5.1f deg  "
                   "palm=%5.1f mm  off=%5.1f mm\n", WPN_DEF[wi].prefix,
                   tag, h ? "support" : "trigger",
                   (double)g_vm_hdbg[h].bend_deg, (double)g_vm_hdbg[h].bore_deg,
                   (double)g_vm_hdbg[h].palm_mm, (double)g_vm_hdbg[h].off_mm);
        }
      }
    } else if (!strcmp(t, "vmorbit")) {
      // Turntable on the VIEWMODEL's hands: the pose is the real one (springs,
      // ADS blend, reload dip), only the camera moves. Angles in DEGREES,
      // distance in metres; yaw 0 is the player's own view, 180 looks back at
      // the shooter from in front of the muzzle.
      char *a = next_tok();
      if (!strcmp(a, "off")) { g_vmorb.on = 0; continue; }
      g_vmorb.on = 1;
      g_vmorb.yaw = parse_f(a) * DEG2RAD;
      g_vmorb.pit = next_f() * DEG2RAD;
      g_vmorb.dist = next_f();
      // Indexes g_vm_hand_p[2] directly, so a negative value read out of bounds
      // (ASan: global-buffer-overflow) and anything above 2 quietly meant "both".
      g_vmorb.fov = 30.0f;        // the harness turntable keeps its wide lens
      g_vmorb.piv_on = 0;
      g_vmorb.rise = 0.0f;
      g_vmorb.comp = 0.0f;        // ...and its centred subject
      int wsel = (int)next_n();   // 0 trigger hand, 1 support, 2 both, 3 trigger
      if (wsel < 0 || wsel > 3) {
        fprintf(stderr, "script: vmorbit WHICH must be 0..3\n");
        exit(1);
      }
      g_vmorb.which = wsel;
    } else if (!strcmp(t, "camaim")) {
      s->cam.blook = next_f();  // cambot look height (default .9)
    } else if (!strcmp(t, "cambot")) {
      char *a = next_tok();
      if (!strcmp(a, "off")) { s->cam.bot = 0; continue; }
      s->cam.bot = 1;
      s->cam.active = s->cam.orbit = 0;
      s->cam.boff.x = parse_f(a);
      s->cam.boff.y = next_f();
      s->cam.boff.z = next_f();
    } else if (!strcmp(t, "camorbit")) {
      char *a = next_tok();
      if (!strcmp(a, "off")) { s->cam.orbit = 0; continue; }
      s->cam.orbit = 1;
      s->cam.active = s->cam.bot = 0;
      s->cam.orbit_radius = parse_f(a);
      s->cam.orbit_height = next_f();
      s->cam.orbit_rate = next_f();
      s->cam.orbit_look = next_f();
      s->cam.orbit_phase = 0.0f;
    } else {
      fprintf(stderr, "script: unknown command '%s'\n", t);
      exit(1);
    }
  }
}

