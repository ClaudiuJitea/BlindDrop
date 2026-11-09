// code/sim/rescue_mission.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// HOSTAGE RESCUE, the mission's half: the alarm, the execution clock, the
// hostages (captive, freed and following, extracted) and the win/fail rules.
// ---------------------------------------------------------------------------
static void rescue_reset(void) {
  memset(&g_resc, 0, sizeof g_resc);
  // A client never runs the mission: NP_RESCUE switches its view on.
  g_resc.on = g_match.mode == GM_RESCUE && !g_home && (g_server || !g_online);
  g_resc.reach_i = g_resc.cutting = -1;
  for (int i = 0; i < MAX_BOTS; i++) g_resc.det_who[i] = -1;
  if (!g_resc.on) return;
  g_resc.squad = resc_squad_cfg();
  g_resc.nguards = resc_guard_count(g_cfg.sp_diff, g_resc.squad);
  g_resc.exec_max = (int)(resc_diff()->exec_s * TICK_HZ);
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    hostage_t *h = &g_resc.h[i];
    h->pos = h->prev = h->net_pos = (v3){{RESC_HOLD[i][0], RESC_HOLD[i][2], RESC_HOLD[i][1]}};
    h->yaw = h->prev_yaw = RESC_HOLD[i][3];
    h->state = HS_CAPTIVE;
    h->grounded = 1;
    h->leader = -1;
  }
  rescue_msg(0, "OPERATION NIGHTFALL", "FREE THE 3 HOSTAGES  \x05  GET THEM TO THE LZ");
  g_resc.msg_t = RESC_INTRO_TICKS;
}
// The squad won (the debrief and the career ask; a client reads the replicated
// result).
static int rescue_won(void) { return g_resc.result == RR_SUCCESS; }
// The nearest living member of the squad to `at`, humans before bots (a
// hostage would rather follow a person), or -1.
static int resc_nearest_squad(v3 at, int humans_only) {
  int best = -1;
  float bd = 1e18f;
  for (int pass = 0; pass < 2 && best < 0; pass++) {
    ENT_FOREACH(e) {
      if (!resc_is_squad(e) || !ent_alive(e)) continue;
      if ((human_of_ent(e) >= 0) != (pass == 0)) continue;
      v3 d = v3_sub(ent_pos(e), at);
      float dd = v3_dot(d, d);
      if (dd < bd) { bd = dd; best = e; }
    }
    if (humans_only) break;
  }
  return best;
}

static void rescue_alarm(int why) {
  if (!rescue_on() || g_resc.alarm || g_resc.result != RR_RUNNING) return;
  g_resc.alarm = 1;
  g_resc.alarm_tick = (int)g_tick;
  g_resc.alarm_why = why;
  g_resc.exec_t = g_resc.exec_max;
  g_resc.siren_t = 0;
  int captive = 0;
  for (int i = 0; i < RESC_HOSTAGES; i++) captive += g_resc.h[i].state == HS_CAPTIVE;
  rescue_msg(2, "ALARM RAISED",
             captive ? "THE HOSTAGES WILL BE EXECUTED - MOVE NOW" : "GET TO THE LZ");
  // Every hunter is sent to where the squad was last seen (or is now, if
  // nobody has seen it yet — a body was found, the shooter is close).
  int any = resc_nearest_squad((v3){{0, 0, 0}}, 0);
  v3 at = g_resc.lkp_tick ? g_resc.lkp : any >= 0 ? ent_pos(any) : (v3){{0, 0, 0}};
  for (int i = 0; i < g_resc.nguards && i < g_match.nbots; i++) {
    bot_t *b = &g_bots[i];
    int k = rescue_post(i)->kind;
    if (!b->active || !b->alive || k == GK_WARDEN || k == GK_SNIPER) continue;
    if (b->tgt < 0) { b->alert_p = at; b->alert_t = 20 * TICK_HZ; }
  }
}

// The captive within reach of a body at `at`, or -1.
static int rescue_reach_at(v3 at) {
  int best = -1;
  float bd = 2.2f * 2.2f;
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    const hostage_t *h = &g_resc.h[i];
    if (h->state != HS_CAPTIVE || fabsf(h->pos.y - at.y) > 1.2f) continue;
    float dx = h->pos.x - at.x, dz = h->pos.z - at.z, dd = dx * dx + dz * dz;
    if (dd < bd) { bd = dd; best = i; }
  }
  return best;
}
static int rescue_reach(const player_t *p) { return rescue_reach_at(p->pos); }
// Does the RELOAD key belong to a hostage right now? (weapon_tick asks — on
// the server for every human, and in a client's prediction for itself, which
// sees the same replicated captives.)
static int rescue_reload_block(const player_t *p) {
  return rescue_view() && p->alive && rescue_reach(p) >= 0;
}

static int rescue_in_lz(v3 p) {
  return p.x > RESC_LZ_X0 && p.x < RESC_LZ_X1 && p.z > RESC_LZ_Z0 && p.z < RESC_LZ_Z1;
}

// ONE FREED HOSTAGE: follow the player on the bots' own grid routes, keep a
// little apart from the others, and stop at a polite distance. Same collision
// as everyone (ent_move), so walls are walls and stairs are stairs.
static void hostage_follow(hostage_t *h, int i, v3 goal, float stop, int lead) {
  float dx = goal.x - h->pos.x, dz = goal.z - h->pos.z, d = sqrtf(dx * dx + dz * dz);
  float wx = 0.0f, wz = 0.0f, spd = 0.0f;
  if (d > stop) {
    if (--h->wp_t <= 0 || (h->wp.x == 0.0f && h->wp.z == 0.0f)) {
      h->wp_t = TICK_HZ / 5;
      h->wp = goal;
      if (map_routes() && !nav_clear(h->pos, goal)) {
        nav_ready();
        int gc = nav_snap(goal);
        v3 w;
        if (gc >= 0 && nav_next(nav_field_for(gc), h->pos, 8, &w)) h->wp = w;
      }
    }
    float rx = h->wp.x - h->pos.x, rz = h->wp.z - h->pos.z, rl = sqrtf(rx * rx + rz * rz);
    if (rl > 0.01f) { wx = rx / rl; wz = rz / rl; }
    spd = f_clamp((d - stop) * 1.8f + 1.6f, 0.0f, 6.4f);
  }
  // keep apart
  for (int j = 0; j < RESC_HOSTAGES; j++) {
    if (j == i || g_resc.h[j].state == HS_DEAD) continue;
    float ox = h->pos.x - g_resc.h[j].pos.x, oz = h->pos.z - g_resc.h[j].pos.z;
    float ol = sqrtf(ox * ox + oz * oz);
    if (ol < 1.0f && ol > 0.001f && fabsf(h->pos.y - g_resc.h[j].pos.y) < 1.0f) {
      wx += ox / ol * (1.0f - ol) * 1.5f;
      wz += oz / ol * (1.0f - ol) * 1.5f;
      if (spd < 1.2f) spd = 1.2f;
    }
  }
  float wl = sqrtf(wx * wx + wz * wz);
  if (wl > 1.0f) { wx /= wl; wz /= wl; }
  float k = f_clamp(14.0f * TICK_DT, 0.0f, 1.0f);
  h->vel.x += (wx * spd - h->vel.x) * k;
  h->vel.z += (wz * spd - h->vel.z) * k;
  h->vel.y -= GRAVITY * TICK_DT;
  h->grounded = 0;
  // A step up, the way STEP_H lifts everyone else: try the body a step higher
  // when the floor ahead is a stair.
  v3 ahead = {{h->pos.x + wx * 0.45f, h->pos.y, h->pos.z + wz * 0.45f}};
  if (spd > 0.3f && !aabb_fits(ahead, PLAYER_RADIUS, HEIGHT_STAND) &&
      aabb_fits((v3){{ahead.x, h->pos.y + 0.42f, ahead.z}}, PLAYER_RADIUS, HEIGHT_STAND) &&
      h->vel.y <= 0.0f)
    h->pos.y += 0.42f;
  ent_move(&h->pos, &h->vel, &h->grounded, HEIGHT_STAND);
  arena_clamp(&h->pos);
  float hs = sqrtf(h->vel.x * h->vel.x + h->vel.z * h->vel.z);
  if (hs > 0.4f) {
    float ty = atan2f(h->vel.x, -h->vel.z);
    h->yaw += wrap_pi(ty - h->yaw) * f_clamp(10.0f * TICK_DT, 0.0f, 1.0f);
  } else if (d > 0.5f) {
    h->yaw += wrap_pi(atan2f(dx, -dz) - h->yaw) * f_clamp(4.0f * TICK_DT, 0.0f, 1.0f);
  }
  h->move += (f_clamp(hs / 5.0f, 0.0f, 1.0f) - h->move) * f_clamp(8.0f * TICK_DT, 0.0f, 1.0f);
  h->gait += hs * TICK_DT * 2.1f;
  // Lost behind a corner for good: catch up behind the player rather than
  // wedging (a rescue is not a pathfinding test).
  if (spd > 1.0f && hs < 0.25f) h->stuck++; else h->stuck = 0;
  if (h->stuck > 3 * TICK_HZ && d > 4.0f && lead >= 0 && ent_alive(lead)) {
    v3 lpos = ent_pos(lead);
    float lyaw = ent_yaw(lead);
    for (int t = 0; t < 8; t++) {
      float a = lyaw + F_PI + ((float)t - 3.5f) * 0.5f;
      v3 c = {{lpos.x + sinf(a) * 1.8f, lpos.y, lpos.z - cosf(a) * 1.8f}};
      if (aabb_fits(c, PLAYER_RADIUS, HEIGHT_STAND)) {
        h->pos = h->prev = c;
        h->vel = (v3){{0, 0, 0}};
        break;
      }
    }
    h->stuck = 0;
  }
}

static void rescue_end(int result, int why) {
  if (g_resc.result != RR_RUNNING) return;
  g_resc.result = result;
  g_resc.why = why;
  g_resc.end_tick = g_resc.clock;
  // Offline the winner is the player, as it always was; online the SIDE is
  // (MAX_ENTS + side), which NP_SCORES and the banner carry like a team win.
  if (!g_server) match_end(result == RR_SUCCESS ? g_local_ent : 1);
  else           match_end(MAX_ENTS + (result == RR_SUCCESS ? 0 : 1));
}

// The mission's sounds, played from the tick that caused them. Not positional:
// a siren across the compound and a cue in the operator's ear are both global.
static void rescue_sfx_flush(void) {
  int f = g_resc.sfx_pending;
  g_resc.sfx_pending = 0;
  if (!f || g_server) return;
  if (f & 1) { audio_play(SFX_SNIP, 0.85f, 1.0f, 0.0f, 0.0f); audio_play(SFX_CHIME, 0.45f, 1.0f, 0.0f, 0.25f); }
  if (f & 2) audio_play(SFX_CHIME, 0.55f, 1.12f, 0.0f, 0.0f);
  if (f & 4) audio_play(SFX_ALARM, 0.40f, 1.0f, 0.0f, 0.0f);
  if (f & 8)
    for (int k = 0; k < RESC_HOSTAGES; k++)
      audio_play(SFX_FAR_PT, 0.75f, 0.97f + 0.03f * (float)k, -0.4f + 0.4f * (float)k, 0.45f * (float)k);
}

static void rescue_tick(void) {
  rescue_sfx_flush();
  if (!rescue_on()) return;
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    hostage_t *h = &g_resc.h[i];
    h->prev = h->pos;
    h->prev_yaw = h->yaw;
    h->prev_gait = h->gait;
    h->prev_move = h->move;
    h->prev_pose = h->pose;
  }
  g_resc.prev_rotor = g_resc.rotor;
  if (g_match.over) {
    // The debrief plays over a live LZ: the rotor at flight speed, the
    // rescued walking to their seats.
    if (g_resc.result == RR_SUCCESS) {
      g_resc.rotor_w = fminf(g_resc.rotor_w + TICK_DT * 6.0f, 42.0f);
      g_resc.rotor += g_resc.rotor_w * TICK_DT;
      for (int i = 0; i < RESC_HOSTAGES; i++)
        if (g_resc.h[i].state == HS_EXTRACTED)
          hostage_follow(&g_resc.h[i], i, (v3){{RESC_SEAT[i][0], 0.0f, RESC_SEAT[i][1]}}, 0.15f, -1);
    }
    return;
  }
  // An arena nobody is in has no mission: an empty server holds it at the
  // start (the guards keep their posts) until someone joins, and one the last
  // player left mid-mission is set up fresh for the next.
  if (g_server && !resc_humans()) {
    if (g_resc.clock > 0) g_match.restart_req = 1;
    return;
  }
  g_resc.clock++;
  if (g_resc.msg_t > 0) g_resc.msg_t--;
  if (g_resc.fire_free_t > 0) g_resc.fire_free_t--;
  // Lamp light: a lit lamp within 7 m makes a squad member far easier to see.
  memset(g_resc.lit_e, 0, sizeof g_resc.lit_e);
  int alive = 0;
  ENT_FOREACH(e) {
    if (!resc_is_squad(e) || !ent_alive(e)) continue;
    if (human_of_ent(e) < 0 && !g_bots[e - 1].active) continue;
    alive++;
    v3 pp = ent_pos(e);
    if (pp.y >= 3.0f) continue;
    for (int i = 0; i < g_nprops; i++) {
      if (g_props[i].kind != PROP_LAMP) continue;
      float dx = g_props[i].x - pp.x, dz = g_props[i].z - pp.z;
      if (dx * dx + dz * dz < 49.0f &&
          los_clear((v3){{g_props[i].x, 3.7f, g_props[i].z}}, (v3){{pp.x, pp.y + 1.2f, pp.z}})) {
        g_resc.lit_e[e] = 1;
        break;
      }
    }
  }
  g_resc.lit = g_local_ent >= 0 ? g_resc.lit_e[g_local_ent] : 0;
  // The whole squad down is the end of it; one survivor carries on.
  if (!alive) { rescue_end(RR_FAIL, RF_KILLED); return; }
  // THE RESTRAINTS: a human holds RELOAD beside a captive for 1.5 s; a squad
  // bot with nobody to fight cuts the one it is standing at.
  int cut[RESC_HOSTAGES] = {0}, cutter[RESC_HOSTAGES] = {-1, -1, -1};
  for (int h = 0; h < MAX_HUMANS; h++) {
    if (!g_humans_on[h] || !g_players[h].alive || !g_resc.use_held[h]) continue;
    int k = rescue_reach(&g_players[h]);
    if (k >= 0) { cut[k] = 1; cutter[k] = ent_of_human(h); }
  }
  for (int i = g_resc.nguards; i < g_match.nbots; i++) {
    const bot_t *sb = &g_bots[i];
    if (!sb->active || !sb->alive || sb->tgt >= 0) continue;
    int k = rescue_reach_at(sb->pos);
    if (k >= 0 && !cut[k]) { cut[k] = 1; cutter[k] = i + 1; }
  }
  g_resc.reach_i = g_local_ent >= 0 && ent_alive(g_local_ent) && human_of_ent(g_local_ent) >= 0
                   ? rescue_reach(&g_players[human_of_ent(g_local_ent)]) : -1;
  g_resc.cutting = -1;
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    hostage_t *h = &g_resc.h[i];
    if (h->state != HS_CAPTIVE) continue;
    if (!cut[i]) {
      if (h->free_t > 0.0f) h->free_t = fmaxf(0.0f, h->free_t - TICK_DT);
      continue;
    }
    if (cutter[i] == g_local_ent) g_resc.cutting = i;
    h->free_t += TICK_DT / 1.5f;
    if (h->free_t < 1.0f) continue;
    h->free_t = 1.0f;
    h->state = HS_FOLLOW;
    // A bot hands the freed man on to the nearest human of the squad, who is
    // the one leading the walk out; a human keeps his own.
    int lead = cutter[i];
    if (human_of_ent(lead) < 0) {
      int hu = resc_nearest_squad(h->pos, 1);
      if (hu >= 0 && v3_len(v3_sub(ent_pos(hu), h->pos)) < 40.0f) lead = hu;
    }
    h->leader = lead;
    g_resc.freed++;
    g_resc.sfx_pending |= 1;
    char b[48];
    snprintf(b, sizeof b, "%d OF %d FREED", g_resc.freed, RESC_HOSTAGES);
    rescue_msg(3, "HOSTAGE FREED", g_resc.freed == RESC_HOSTAGES ?
               "ALL FREE  \x05  GET THEM TO THE LZ" : b);
  }
  // The followers: each behind its leader, in the order they were freed. A
  // leader who falls (or a bot that leaves) hands them to the nearest member
  // of the squad still standing.
  int order[MAX_ENTS] = {0};
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    hostage_t *h = &g_resc.h[i];
    if (h->state == HS_FOLLOW || h->state == HS_EXTRACTED)
      h->pose = fminf(1.0f, h->pose + TICK_DT * 1.4f);
    if (h->state == HS_FOLLOW) {
      if (h->leader < 0 || h->leader >= MAX_ENTS || !resc_is_squad(h->leader) ||
          !ent_alive(h->leader) ||
          (human_of_ent(h->leader) < 0 && !g_bots[h->leader - 1].active))
        h->leader = resc_nearest_squad(h->pos, 0);
      if (h->leader < 0) continue;
      int o = order[h->leader]++;
      hostage_follow(h, i, ent_pos(h->leader), 2.0f + 1.1f * (float)o, h->leader);
      if (rescue_in_lz(h->pos)) {
        h->state = HS_EXTRACTED;
        g_resc.extracted++;
        g_resc.sfx_pending |= 2;
      }
    } else if (h->state == HS_EXTRACTED) {
      v3 seat = {{RESC_SEAT[i][0], 0.0f, RESC_SEAT[i][1]}};
      hostage_follow(h, i, seat, 0.15f, -1);
    }
  }
  if (g_resc.extracted > 0 || g_resc.freed == RESC_HOSTAGES)
    g_resc.rotor_w = fminf(g_resc.rotor_w + TICK_DT * 6.0f, g_resc.extracted == RESC_HOSTAGES ? 42.0f : 9.0f);
  g_resc.rotor += g_resc.rotor_w * TICK_DT;
  // THE ALARM: sirens, radio updates, and the clock.
  if (g_resc.alarm) {
    if (g_resc.siren_t-- <= 0 && (int)g_tick - g_resc.alarm_tick < 14 * TICK_HZ) {
      g_resc.siren_t = 2 * TICK_HZ;
      g_resc.sfx_pending |= 4;
    }
    // Every 5 s the radio passes on where the player was last seen.
    if (((int)g_tick - g_resc.alarm_tick) % (5 * TICK_HZ) == 0 && g_resc.lkp_tick) {
      for (int i = 0; i < g_resc.nguards && i < g_match.nbots; i++) {
        bot_t *b = &g_bots[i];
        int k = rescue_post(i)->kind;
        if (!b->active || !b->alive || k == GK_WARDEN || k == GK_SNIPER || b->tgt >= 0) continue;
        b->alert_p = g_resc.lkp;
        b->alert_t = 10 * TICK_HZ;
      }
    }
    int captive = 0;
    for (int i = 0; i < RESC_HOSTAGES; i++) captive += g_resc.h[i].state == HS_CAPTIVE;
    if (captive && --g_resc.exec_t <= 0) {
      for (int i = 0; i < RESC_HOSTAGES; i++)
        if (g_resc.h[i].state == HS_CAPTIVE) g_resc.h[i].state = HS_DEAD;
      g_resc.sfx_pending |= 8;
      rescue_end(RR_FAIL, RF_EXECUTED);
      return;
    }
  }
  if (g_resc.extracted == RESC_HOSTAGES) rescue_end(RR_SUCCESS, RF_NONE);
}

// A CLIENT's half of the mission, once per client tick: nothing of the rules,
// only what makes the replicated state look alive between packets — the
// hostages walk to where the server last put them (with the walk cycle that
// distance makes), the rotor turns, the banner counts down, the siren and the
// chimes sound, and the RELOAD prompt is worked out from our own position.
static void rescue_client_tick(void) {
  if (!rescue_view() || rescue_on()) return;
  rescue_sfx_flush();
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    hostage_t *h = &g_resc.h[i];
    h->prev = h->pos;
    h->prev_yaw = h->yaw;
    h->prev_gait = h->gait;
    h->prev_move = h->move;
    h->prev_pose = h->pose;
    v3 d = v3_sub(h->net_pos, h->pos);
    h->pos = v3_add(h->pos, v3_scale(d, f_clamp(12.0f * TICK_DT, 0.0f, 1.0f)));
    float hs = sqrtf(d.x * d.x + d.z * d.z) * 12.0f;   // m/s the lerp is walking
    if (hs < 0.15f) hs = 0.0f;
    h->yaw += wrap_pi(h->net_yaw - h->yaw) * f_clamp(10.0f * TICK_DT, 0.0f, 1.0f);
    h->move += (f_clamp(hs / 5.0f, 0.0f, 1.0f) - h->move) * f_clamp(8.0f * TICK_DT, 0.0f, 1.0f);
    h->gait += hs * TICK_DT * 2.1f;
  }
  g_resc.prev_rotor = g_resc.rotor;
  g_resc.rotor += g_resc.rotor_w * TICK_DT;
  if (g_resc.msg_t > 0) g_resc.msg_t--;
  if (g_resc.alarm && g_resc.result == RR_RUNNING && !g_match.over) {
    if (g_resc.cl_alarm_age++ < 14 * TICK_HZ && g_resc.siren_t-- <= 0) {
      g_resc.siren_t = 2 * TICK_HZ;
      g_resc.sfx_pending |= 4;
    }
  }
  int me = human_of_ent(g_local_ent);
  g_resc.reach_i = me >= 0 && g_players[me].alive ? rescue_reach(&g_players[me]) : -1;
  g_resc.cutting = g_resc.reach_i >= 0 && g_resc.h[g_resc.reach_i].free_t > 0.0f ? g_resc.reach_i : -1;
}

// The guard side of a kill (match_on_kill asks): the count, and the end of
// a mission whose operator is down.
static void rescue_on_kill(int src, int dst, int hs) {
  if (!rescue_on()) return;
  if (resc_is_squad(src) && resc_is_guard(dst)) {
    g_resc.kills++;
    if (hs) g_resc.heads++;
    g_resc.radio_t[dst - 1] = 0;   // a dead man radios nobody
  }
}

// TEAM DEATHMATCH bot count: both sides topped up to the team size.
static int match_bot_want(void) {
  return teams_on() ? tdm_bot_want(0) + tdm_bot_want(1) : sp_bot_count();
}
// Deal the bots to the sides. Bots retire from the TOP of the range, so when a
// human takes a place the count per side can come out wrong by one; the
// highest-index bot of the over-full side changes sides and redeploys on its
// new side's ground. A fresh bot simply joins whichever side is short.
static void tdm_deal_bots(int respawn) {
  if (!teams_on()) return;
  int wa = tdm_bot_want(0), ca = 0;
  for (int i = 0; i < g_match.nbots; i++) ca += g_team[i + 1] == 0;
  for (int i = g_match.nbots - 1; i >= 0 && ca > wa; i--)
    if (g_team[i + 1] == 0) { g_team[i + 1] = 1; ca--; if (respawn) bot_spawn(i); }
  for (int i = g_match.nbots - 1; i >= 0 && ca < wa; i--)
    if (g_team[i + 1] != 0) { g_team[i + 1] = 0; ca++; if (respawn) bot_spawn(i); }
}
static int tdm_new_bot_team(void) {
  int ca = 0;
  for (int i = 0; i < g_match.nbots; i++) ca += g_team[i + 1] == 0;
  return ca < tdm_bot_want(0) ? 0 : 1;
}

// Runs once per sim tick: live-applies the BOTS slider (fresh bots spawn,
// extras retire), services a queued restart, then ticks every active bot.
static void match_tick(void) {
  // Offline the local player's side is the TEAM page's choice.
  if (!g_server && !g_online && g_humans_on[0]) g_team[0] = (signed char)(g_cfg.sp_team ? 1 : 0);
  int want = match_bot_want();
  while (g_match.nbots < want) {
    int i = g_match.nbots++;
    g_match.kills[i + 1] = g_match.deaths[i + 1] = 0;
    g_match.shots[i + 1] = g_match.hits[i + 1] = 0;
    // The side BEFORE the spawn: spawn_spot places a fighter on his side.
    if (teams_on()) { g_match.nbots--; g_team[i + 1] = (signed char)tdm_new_bot_team(); g_match.nbots++; }
    bot_spawn(i);
  }
  while (g_match.nbots > want) g_bots[--g_match.nbots].active = 0;
  tdm_deal_bots(1);

  // The server's half of the over countdown (see match_over_countdown): this
  // is the caller that owns the restart. Offline it returns immediately.
  match_over_countdown();

  if (g_match.restart_req) {
    g_match.restart_req = 0;
    match_restart();
  }

  for (int i = 0; i < g_match.nbots; i++) bot_tick(i);
  nade_tick(1);
  if (g_ballistics) proj_tick();
  lms_tick();
  rescue_tick();
}

static void match_init(void) {
  memset(&g_match, 0, sizeof g_match);
  g_match.mode = g_cfg.sp_mode;
  g_match.round_win = -1;
  g_match.round_no = 1;
  memset(g_bots, 0, sizeof g_bots);
  rescue_reset();
  g_team_score[0] = g_team_score[1] = 0;
  if (!g_server && g_humans_on[0]) g_team[0] = (signed char)(g_cfg.sp_team ? 1 : 0);
  g_match.nbots = match_bot_want();
  tdm_deal_bots(0);
  for (int i = 0; i < g_match.nbots; i++) bot_spawn(i);
  for (int h = 0; h < MAX_HUMANS; h++)   // P1-62: every human starts with a
    if (g_humans_on[h])                  // solved skeleton, not garbage
      anim_reset(&g_player_anim[h], g_players[h].pos, g_players[h].yaw);
}

