// code/sim/rescue_guard.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// HOSTAGE RESCUE, the guard's half: perception and the post/route walk. Both
// run inside bot_tick and only when rescue_on().
// ---------------------------------------------------------------------------
static void rescue_alarm(int why);
static void rescue_spawn_guard(bot_t *b, int bi) {
  const guard_post_t *gp = rescue_post(bi);
  b->pos = b->prev_pos = b->target = (v3){{gp->x, gp->y, gp->z}};
  b->yaw = b->prev_yaw = b->pup_face = gp->yaw;
  if (gp->kind == GK_SNIPER) b->wp.cur = WPN_SR;
  g_resc.detect[bi] = g_resc.prev_detect[bi] = 0.0f;
  g_resc.radio_t[bi] = 0;
  g_resc.wp_i[bi] = gp->kind == GK_PATROL ? 1 % GUARD_ROUTES[gp->route].n : 0;
  g_resc.wait_t[bi] = 0;
  g_resc.body_found[bi] = 0;
  g_resc.heard_tick[bi] = 0;
  anim_reset(&b->anim, b->pos, b->yaw);
}

// DETECTION. A guard that can see the player fills a 0..1 meter; at 1 it may
// target him (bot_tick's scan reads rescue_may_target) and starts the radio
// clock, and a spotter that stays on the radio long enough raises the alarm.
// Traced every third tick per guard (staggered), which is what keeps eighteen
// guards' extra line-of-sight rays inside the budget.
// One squad member as a guard sees him: where, how fast, how low.
typedef struct { v3 pos, vel, aim, head; int crouched, grounded; } resc_body_t;
static int resc_body(int e, resc_body_t *o) {
  if (!resc_is_squad(e) || !ent_alive(e)) return 0;
  int h = human_of_ent(e);
  if (h >= 0) {
    const player_t *p = &g_players[h];
    o->pos = p->pos; o->vel = p->vel;
    o->crouched = p->crouched; o->grounded = p->grounded;
    o->head = (v3){{p->pos.x, p->pos.y + p->eye, p->pos.z}};
  } else {
    const bot_t *sb = &g_bots[e - 1];
    if (!sb->active) return 0;
    o->pos = sb->pos; o->vel = sb->vel;
    o->crouched = sb->crouch; o->grounded = sb->grounded;
    o->head = (v3){{sb->pos.x, sb->pos.y + bot_eye(sb), sb->pos.z}};
  }
  o->aim = ent_aimpoint(e);
  return 1;
}

static void rescue_guard_tick(bot_t *b, int bi) {
  if (!rescue_on() || bi >= g_resc.nguards) return;   // squad bots perceive as bots
  const resc_diff_t *rd = resc_diff();
  const guard_post_t *gp = rescue_post(bi);
  float *det = &g_resc.detect[bi];
  g_resc.prev_detect[bi] = *det;
  int engaged = b->tgt >= 0 && resc_is_squad(b->tgt) && b->belief == BOT_BELIEF_VISIBLE &&
                ent_alive(b->tgt);
  // A gunshot heard is a reason to look (the hearing itself still steers the
  // turn and the walk over, through bot_tick's startle branch).
  if (b->hear_tick && b->hear_kind == BOT_NOISE_SHOT && b->hear_tick != g_resc.heard_tick[bi]) {
    g_resc.heard_tick[bi] = b->hear_tick;
    if (*det < 0.55f) *det = 0.55f;
    g_resc.glimpse[bi] = b->hear_pos;
  }
  if ((g_tick + bi) % 3 == 0) {
    const float dt3 = 3.0f * TICK_DT;
    v3 eo = {{b->pos.x, b->pos.y + bot_eye(b), b->pos.z}};
    float R = gp->kind == GK_SNIPER ? rd->snipe : gp->kind == GK_WARDEN ? rd->range * 0.8f : rd->range;
    if (g_resc.alarm) R *= 1.25f;
    // Every member of the squad in view adds to ONE meter; the one filling it
    // fastest is the one he is looking at (det_who), and where he goes to look.
    int seen = 0, who = -1;
    float rate_sum = 0.0f, rate_best = 0.0f;
    v3 who_pos = {{0, 0, 0}};
    ENT_FOREACH(e) {
      resc_body_t sb;
      if (!resc_body(e, &sb)) continue;
      v3 tp = sb.aim;
      float dx = tp.x - eo.x, dz = tp.z - eo.z, hl = sqrtf(dx * dx + dz * dz);
      float d = v3_len(v3_sub(tp, eo));
      if (d >= R) continue;
      float align = hl > 0.01f ? (dx * sinf(b->yaw) - dz * cosf(b->yaw)) / hl : 1.0f;
      // The cone is ~145 deg wide, opening out to the flanks within arm's reach;
      // behind his back a guard sees nothing (he hears — bot_hear_update).
      float cone = d < 2.5f ? -0.10f : 0.30f;
      if (align <= cone || !(los_clear(eo, tp) || los_clear(eo, sb.head))) continue;
      float prox = 1.0f - d / R;
      float periph = 0.30f + 0.70f * f_clamp((align - cone) / (0.92f - cone), 0.0f, 1.0f);
      float hs = sqrtf(sb.vel.x * sb.vel.x + sb.vel.z * sb.vel.z) / g_sim_mv[MV_RUN];
      float motion = 0.50f + 0.65f * f_clamp(hs, 0.0f, 1.3f) + (sb.grounded ? 0.0f : 0.3f);
      float stance = sb.crouched ? 0.45f : 1.0f;
      float light = g_resc.lit_e[e] ? 1.7f : 1.0f;
      float rate = rd->rate * (0.22f + 2.6f * prox * prox) * periph * motion * stance * light;
      if (d < 4.0f) rate *= 3.0f;
      if (g_resc.alarm) rate *= 2.2f;
      if (gp->kind == GK_SNIPER) rate *= 0.8f;
      rate_sum += rate;
      if (rate > rate_best) { rate_best = rate; who = e; who_pos = sb.pos; }
      seen = 1;
    }
    if (seen) {
      *det += rate_sum * dt3;
      g_resc.det_who[bi] = who;
      g_resc.glimpse[bi] = who_pos;
      if (*det >= 1.0f) {
        g_resc.lkp = who_pos;
        g_resc.lkp_tick = g_tick;
      }
    }
    if (!seen && !engaged) {
      float decay = g_resc.alarm ? 0.08f : 0.16f;
      if (*det >= 1.0f) {   // lost him: go and look where he was
        b->alert_p = g_resc.glimpse[bi];
        b->alert_t = 8 * TICK_HZ;
        *det = 0.95f;
      }
      *det -= decay * dt3;
      if (*det < 0.0f) *det = 0.0f;
      if (*det <= 0.0f) g_resc.det_who[bi] = -1;
      if (g_resc.prev_detect[bi] > 0.5f && *det <= 0.5f && b->tgt < 0) {
        b->alert_p = g_resc.glimpse[bi];   // "something was there": check it
        b->alert_t = 6 * TICK_HZ;
      }
    }
    if (*det > 1.0f) *det = 1.0f;
    if (*det >= 1.0f && g_resc.prev_detect[bi] < 1.0f) {
      g_resc.spotted_n++;
      g_resc.ever_seen = 1;
    }
    // BODIES. A guard who walks up on a fallen comrade radios it in.
    if (--g_resc.body_cd[bi] <= 0) {
      g_resc.body_cd[bi] = 5;   // x3 ticks
      for (int j = 0; j < g_resc.nguards && j < g_match.nbots; j++) {
        const bot_t *o = &g_bots[j];
        if (j == bi || !o->active || o->alive || g_resc.body_found[j]) continue;
        v3 cp = {{o->pos.x, o->pos.y + 0.35f, o->pos.z}};
        float cx = cp.x - eo.x, cz = cp.z - eo.z, cl = sqrtf(cx * cx + cz * cz);
        if (cl > 17.0f) continue;
        float ca = cl > 0.01f ? (cx * sinf(b->yaw) - cz * cosf(b->yaw)) / cl : 1.0f;
        if (ca < 0.35f || !los_clear(eo, cp)) continue;
        g_resc.body_found[j] = 1;
        g_resc.bodies++;
        g_resc.radio_why[bi] = RW_BODY;
        if (g_resc.radio_t[bi] < 1) g_resc.radio_t[bi] = 1;
        b->alert_p = o->pos;
        b->alert_t = 10 * TICK_HZ;
        if (*det < 0.6f) *det = 0.6f;
        g_resc.glimpse[bi] = o->pos;
        if (!g_resc.alarm) rescue_msg(1, "BODY DISCOVERED", "KILL THE GUARD BEFORE HE RADIOS IT IN");
        break;
      }
    }
  }
  // THE RADIO. Seeing the player (engaged) or a body keeps the call going; a
  // guard killed before it finishes never gets it out.
  if (!g_resc.alarm) {
    if (engaged) {
      g_resc.radio_why[bi] = RW_SPOTTED;
      g_resc.radio_t[bi]++;
    } else if (g_resc.radio_why[bi] == RW_BODY && g_resc.radio_t[bi] > 0) {
      g_resc.radio_t[bi]++;
    } else if (g_resc.radio_t[bi] > 0) {
      g_resc.radio_t[bi] -= 2;
      if (g_resc.radio_t[bi] < 0) g_resc.radio_t[bi] = 0;
    }
    float need = rd->radio_s * (g_resc.radio_why[bi] == RW_BODY ? 1.6f : 1.0f);
    if (g_resc.radio_t[bi] > (int)(need * TICK_HZ))
      rescue_alarm(g_resc.radio_why[bi]);
  }
}

// THE POST AND THE ROUTE, in place of bot_tick's random wander (it runs only
// where that would have: no target, no fresh noise, no spot to clear).
// Returns 1 when it set the intent.
static int rescue_patrol(bot_t *b, int bi, float *yaw, float *pitch, float *wx,
                         float *wz, float *pace) {
  const guard_post_t *gp = rescue_post(bi);
  float det = g_resc.detect[bi];
  b->crouch = 0;
  b->crouch_t = 0;
  *pitch = 0.0f;
  if (det > 0.22f) {
    // SUSPICIOUS: turn to what it glimpsed; past halfway, walk over to look.
    v3 gl = g_resc.glimpse[bi];
    float dx = gl.x - b->pos.x, dz = gl.z - b->pos.z, l = sqrtf(dx * dx + dz * dz);
    if (l > 0.01f) *yaw = atan2f(dx, -dz);
    if (det > 0.6f && l > 2.0f && fabsf(wrap_pi(*yaw - b->yaw)) < 0.8f) {
      v3 rw = bot_route(b, gl);
      float rx = rw.x - b->pos.x, rz = rw.z - b->pos.z, rl = sqrtf(rx * rx + rz * rz);
      if (rl > 0.01f) { *wx = rx / rl; *wz = rz / rl; *yaw = atan2f(rx, -rz); }
      *pace = 0.45f;
    }
    return 1;
  }
  if (gp->kind != GK_PATROL) {
    // HOLD THE POST: back to it if anything moved the guard, then sweep.
    float dx = gp->x - b->pos.x, dz = gp->z - b->pos.z, l = sqrtf(dx * dx + dz * dz);
    if (l > 0.9f) {
      v3 rw = bot_route(b, (v3){{gp->x, gp->y, gp->z}});
      float rx = rw.x - b->pos.x, rz = rw.z - b->pos.z, rl = sqrtf(rx * rx + rz * rz);
      if (rl > 0.01f) *yaw = atan2f(rx, -rz);
      if (rl > 0.01f && fabsf(wrap_pi(*yaw - b->yaw)) < 1.0f) { *wx = rx / rl; *wz = rz / rl; }
      *pace = 0.5f;
      return 1;
    }
    float amp = gp->kind == GK_SNIPER ? 1.15f : gp->kind == GK_WARDEN ? 0.65f : 0.95f;
    float per = gp->kind == GK_SNIPER ? 13.0f : 9.0f;
    float ph = (float)g_tick * TICK_DT * (2.0f * F_PI / per) + (float)bi * 1.7f;
    // A sweep that HOLDS at each end, the way a person scans: a softened sine.
    float sw = sinf(ph);
    sw = sw * (1.35f - 0.35f * sw * sw);
    *yaw = gp->yaw + amp * sw;
    if (gp->kind == GK_SNIPER) *pitch = -0.12f;
    return 1;
  }
  // PATROL: walk the route; at each point stop and look about.
  const guard_route_t *rt = &GUARD_ROUTES[gp->route];
  int *wi = &g_resc.wp_i[bi];
  if (*wi < 0 || *wi >= rt->n) *wi = 0;
  if (g_resc.wait_t[bi] > 0) {
    int w = --g_resc.wait_t[bi];
    *yaw = g_resc.look_yaw[bi] + 0.85f * sinf((float)w * TICK_DT * 1.4f);
    return 1;
  }
  v3 tgt = {{rt->p[*wi][0], 0.0f, rt->p[*wi][1]}};
  float dx = tgt.x - b->pos.x, dz = tgt.z - b->pos.z, l = sqrtf(dx * dx + dz * dz);
  if (l < 1.0f || b->stuck > 150) {
    *wi = (*wi + 1) % rt->n;
    g_resc.wait_t[bi] = (int)rng_rangef(&g_rng_bot, 2.0f, 4.0f) * TICK_HZ;
    g_resc.look_yaw[bi] = b->yaw;
    b->stuck = 0;
    return 1;
  }
  v3 rw = bot_route(b, tgt);
  float rx = rw.x - b->pos.x, rz = rw.z - b->pos.z, rl = sqrtf(rx * rx + rz * rz);
  if (rl > 0.01f) *yaw = atan2f(rx, -rz);
  if (rl > 0.01f && fabsf(wrap_pi(*yaw - b->yaw)) < 1.0f) { *wx = rx / rl; *wz = rz / rl; }
  *pace = g_resc.alarm ? 0.6f : 0.34f;
  return 1;
}

// A SQUAD BOT with nobody to fight. With a human in the squad alive it is HIS
// man: it keeps a few metres behind him, crouches when he crouches, watches
// the way he faces (a little to one side) and frees a captive the two of them
// are standing near. With no human left standing it runs the mission itself:
// the nearest captive, then the LZ with whoever follows it. Returns 1 when it
// set the intent.
#define RESC_LZ_CX 22.0f
#define RESC_LZ_CZ -45.0f
static int rescue_squad_intent(bot_t *b, int bi, float *yaw, float *pitch, float *wx,
                               float *wz, float *pace) {
  int me = bi + 1, slot = bi - g_resc.nguards;
  *pitch = 0.0f;
  b->crouch_t = 0;
  const player_t *lp = NULL;
  float ld = 1e9f;
  for (int h = 0; h < MAX_HUMANS; h++) {
    if (!g_humans_on[h] || !g_players[h].alive) continue;
    float d = v3_len(v3_sub(g_players[h].pos, b->pos));
    if (d < ld) { ld = d; lp = &g_players[h]; }
  }
  int followers = 0, ci = -1;
  float cd = 1e9f;
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    const hostage_t *h = &g_resc.h[i];
    if (h->state == HS_FOLLOW && h->leader == me) followers++;
    if (h->state != HS_CAPTIVE) continue;
    float d = v3_len(v3_sub(h->pos, b->pos));
    if (d < cd) { cd = d; ci = i; }
  }
  v3 goal;
  float stop;
  int low = 0;
  if (followers) {
    goal = (v3){{RESC_LZ_CX, 0.0f, RESC_LZ_CZ}};
    stop = 1.5f;
  } else if (lp) {
    int help = ci >= 0 && cd < 9.0f && v3_len(v3_sub(lp->pos, g_resc.h[ci].pos)) < 14.0f;
    if (help) { goal = g_resc.h[ci].pos; stop = 1.1f; }
    else      { goal = lp->pos; stop = 2.6f + 1.4f * (float)slot; }
    low = lp->crouched && !g_resc.alarm;
  } else if (ci >= 0) {
    goal = g_resc.h[ci].pos;
    stop = 1.1f;
    low = !g_resc.alarm;               // alone, it goes in low and slow
  } else {
    goal = (v3){{RESC_LZ_CX, 0.0f, RESC_LZ_CZ}};
    stop = 1.5f;
  }
  b->crouch = low;
  float dx = goal.x - b->pos.x, dz = goal.z - b->pos.z, d = sqrtf(dx * dx + dz * dz);
  if (d < stop && fabsf(goal.y - b->pos.y) < 1.5f) {
    // In place: watch the leader's sector, one bot to each side of it.
    if (lp && !followers) *yaw = lp->yaw + ((slot & 1) ? -0.9f : 0.9f);
    else if (d > 0.3f)    *yaw = atan2f(dx, -dz);
    else                  *yaw = b->yaw;
    *wx = *wz = 0.0f;
    *pace = 0.0f;
    return 1;
  }
  v3 rw = bot_route(b, goal);
  float rx = rw.x - b->pos.x, rz = rw.z - b->pos.z, rl = sqrtf(rx * rx + rz * rz);
  if (rl > 0.01f) *yaw = atan2f(rx, -rz);
  if (rl > 0.01f && fabsf(wrap_pi(*yaw - b->yaw)) < 1.0f) { *wx = rx / rl; *wz = rz / rl; }
  // Quiet unless it has to catch up, the alarm is up, or it is crouched (the
  // crouch speed is already the slow one).
  *pace = low ? 1.0f : g_resc.alarm || ld > 14.0f || (!lp && d > 25.0f) ? 0.95f : 0.55f;
  return 1;
}

static void bot_tick(int bi) {
  bot_t *b = &g_bots[bi];
  const bot_skill_t *sk = &BOT_SKILL[g_cfg.sp_diff];
  b->prev_pos = b->pos;
  b->prev_yaw = b->yaw;
  b->prev_topple = b->topple;
  b->startled = 0;   // census flag: honest on the dead/puppet/frozen returns too
  if (b->flash > 0) b->flash--;

  if (!b->alive) {
    rag_advance(&b->rag, &b->pos, b->prev_pos, &b->topple, &b->anim);
    b->grounded = b->rag.grounded;
    if (!g_match.over && mode_respawns() && --b->respawn_t <= 0) {
      // The BODY stays behind. Respawn timing is match pacing and is untouched;
      // what changes is that the corpse is no longer the same object as the
      // entity, so it can lie there and come apart on its own clock.
      // The whole ragdoll goes with it, not just the velocity. Measured, 8% of
      // handovers are still travelling over 2 m/s and 1.4% are airborne — and a
      // body that stops tumbling, stops falling and stops bouncing at that
      // instant is the handover made visible on exactly those.
      corpse_add(&b->anim, b->pos, b->prev_pos, b->rag, b->yaw,
                 b->topple, b->prev_topple,
                 gun_anim(&b->wp, 1.0f), b->wp.cur, 0, bi + 1);
      bot_spawn(bi);
      ev_push((event_t){.type = EV_RESPAWN, .dst = bi + 1});
    }
    return;
  }
  if (b->puppet) {
    // ADS is driven HERE and not down at the AI's own gun_ads: the puppet tick
    // returns on the next line, so an override placed there is unreachable.
    // The anim testbed exists so the pose being tuned can be reached
    // deterministically, and ADS is the pose that decides where a buttstock
    // ends up — waiting for a live bot to find a distant target is not a
    // measurement, it is a coincidence.
    gun_ads(&b->wp, b->pup_ads);
    bot_puppet_tick(b, bi);
    return;
  }
  // The BOOT SCREEN used to freeze them too, on the reasoning that a ticking
  // world with live bots is a main menu in which the player is hunted down and
  // killed while reading the tiles. That reasoning died with `ent_damage`'s own
  // home gate: nothing can lose hit points while the screen is up, so nothing
  // can die, and what is left is six soldiers fighting an endless firefight
  // behind the camera. That is the backdrop the screen wanted all along — and
  // it is also where the rounds that crack past the player during the slide
  // come from, which is not something a director can fake.
  if (g_bots_frozen || g_match.over || home_calm()) return;

  gun_cool(&b->wp);
  gun_reload_tick(&b->wp);
  if (b->pause_t > 0) b->pause_t--;
  if (b->react_t > 0) b->react_t--;
  if (b->hurt_t > 0) b->hurt_t--;
  if (b->threat_t > 0 && --b->threat_t == 0) {
    b->threat_src = -1;
    b->threat_score = 0;
  }
  if (b->slide_cd > 0) b->slide_cd--;
  if (b->jump_cd > 0) b->jump_cd--;
  bot_flank_tick(b);
  bot_deadlock_tick(b);
  if (b->alert_t > 0) b->alert_t--;
  if (b->prefire_t > 0) b->prefire_t--;
  if (b->prefire_cd > 0) b->prefire_cd--;
  b->prefiring = 0;
  b->peek = 0;
  if (b->cov_retry > 0) b->cov_retry--;
  if (b->swap_cd > 0) b->swap_cd--;
  bot_hear_update(b, bi);
  rescue_guard_tick(b, bi);
  float flk = expf(-6.0f * TICK_DT);
  b->fl_y *= flk;
  b->fl_p *= flk;

  // A TEAMMATE IS NEVER HELD. The scan below refuses teammates, but a lock
  // taken while he was an enemy (he switched sides, or joined onto ours) used
  // to be kept by the hold logic — the bot went on shooting its own side.
  if (b->tgt >= 0 && team_mates(bi + 1, b->tgt)) {
    b->tgt = -1;
    b->lose_t = 0;
    bot_belief_clear(b);
  }
  // Perception: keep the current target while it stays visible, remember it
  // briefly when line of sight breaks, rescan on a per-bot stagger.
  float tdist = 1e9f;
  int see = b->tgt >= 0 && ent_alive(b->tgt) &&
            bot_in_arc(b, b->tgt, BOT_KEEP_COS) &&
            bot_can_see(b, b->tgt, &tdist);
  if (see) {
    int reacquired = b->tgt >= 0 && b->belief != BOT_BELIEF_VISIBLE;
    int hidden = b->lose_t;          // observe zeroes it; the RE-ARM needs it
    bot_belief_observe(b, b->tgt);
    b->belief = BOT_BELIEF_VISIBLE;
    if (reacquired) {
      b->reacquire_count++;
      // A RE-PEEK IS NOT A FREE SHOT. react_t and the settling aim error were
      // only ever armed on a target SWITCH or a damage reveal, so both survived
      // an occlusion intact: first contact of a life cost 37 ticks to the first
      // round, re-exposing the same angle 60 ticks later cost two. Peeking one
      // angle twice inside the memory window was a death sentence, and it is
      // the exact shape of the "the gun was already on me" complaint. Half the
      // fresh delay, half the fresh error: re-finding someone you were already
      // fighting IS faster than finding them cold, just not free.
      // RAMPED with how long it was actually gone. A flat re-arm was charged
      // for a ONE-TICK loss too, and at a corner the line of sight flickers:
      // staged at a peeking player, 38-53% of every tick the bot could see him
      // was spent back under a fresh reaction delay, so it kept looking at him
      // and not shooting. That is the "it has sight on me, flickers, doesn't
      // shoot" report, and it was this line. Full price at 45 ticks (0.4 s)
      // hidden — a real re-peek — and nothing for the flicker of an edge.
      float re = f_clamp((float)hidden / 45.0f, 0.0f, 1.0f);
      int t = (int)(sk->react_ms * 0.5f * re * (float)TICK_HZ / 1000.0f);
      if (b->react_t < t) b->react_t = t;
      if (b->err < sk->err0 * 0.5f * re) b->err = sk->err0 * 0.5f * re;
    }
  } else if (b->tgt >= 0) {
    if (!ent_alive(b->tgt)) {
      b->tgt = -1;
      b->lose_t = 0;
      bot_belief_clear(b);
    } else {
      if (b->belief == BOT_BELIEF_VISIBLE) {
        b->belief = BOT_BELIEF_OCCLUDED_HOLD;
        b->search_t = sk->hold_ticks;
        b->search_index = 0;
        b->search_visited = 0;
      }
      b->lose_t++;
      bot_belief_predict(b);
      if (b->belief == BOT_BELIEF_OCCLUDED_HOLD) {
        if (b->search_t > 0) b->search_t--;
        if (b->search_t <= 0) {
          b->belief = BOT_BELIEF_INVESTIGATE;
          bot_search_plan_next(b, sk);
        }
      }
      if (b->lose_t >= sk->memory_ticks) {
        b->tgt = -1;
        b->lose_t = 0;
        bot_belief_clear(b);
      }
    }
  }
  if (--b->scan_t <= 0) {
    b->scan_t = 10;
    float fx = sinf(b->yaw), fz = -cosf(b->yaw);
    v3 eo = {{b->pos.x, b->pos.y + bot_eye(b), b->pos.z}};
    bot_target_choice_t best = {.ent = -1, .score = -1.0e30f, .dist = 1.0e9f};
    ENT_FOREACH(e) {   // P1-54: without the human range, humans are invisible to the whole AI
      if (e == bi + 1 || !ent_alive(e)) continue;
      if (team_mates(bi + 1, e)) continue;          // a teammate is not a target
      if (rescue_on() && !rescue_may_target(b, bi, e)) continue;
      // Cheap rejects FIRST: most candidates fail the cone/heard gate, and the
      // LOS trace is the expensive test. The range gate and the distance are
      // bot_can_see's own arithmetic, so the reorder cannot change a verdict.
      v3 tp = ent_aimpoint(e);
      float d = v3_len(v3_sub(tp, eo));
      if (d < 0.01f || d > GUN_RANGE) continue;
      float dx = tp.x - b->pos.x, dz = tp.z - b->pos.z;
      float hl = sqrtf(dx * dx + dz * dz);
      if (hl <= 0.01f) continue;
      float align = (dx * fx + dz * fz) / hl;
      if (e != b->tgt && align <= BOT_FOV_COS) continue;
      // The current target's LOS was already traced by the perception step
      // this tick, and bot_can_see is pure — reuse that verdict.
      if (e == b->tgt) { if (!see) continue; }
      else if (!los_clear(eo, tp)) continue;
      bot_target_choice_t cand = {
        .ent = e,
        .score = bot_target_score(b, e, d, align),
        .dist = d,
      };
      if (bot_target_better(cand, best)) best = cand;
    }
    // Visible candidates use one deterministic score with an explicit hold
    // bonus and epsilon ties. A hidden remembered target is different: the bot
    // keeps that focus unless the new visible entity is the recorded attacker
    // or is close/centered enough to clear the stronger immediate-threat gate.
    int hidden_current = b->tgt >= 0 && !see && ent_alive(b->tgt);
    int can_switch = best.ent >= 0 && best.ent != b->tgt &&
                     (!hidden_current || bot_target_breaks_hidden(b, best));
    if (can_switch) {
      bot_focus_invalidate(b);
      b->tgt = best.ent;
      b->lose_t = 0;
      b->err = sk->err0;
      b->react_t = (int)(sk->react_ms * rng_rangef(&g_rng_bot, 0.85f, 1.25f) *
                         (float)TICK_HZ / 1000.0f);
      see = bot_in_arc(b, b->tgt, BOT_KEEP_COS) &&
            bot_can_see(b, b->tgt, &tdist);
      if (see) bot_belief_observe(b, b->tgt);
    }
  }

  int engaging = b->tgt >= 0 && see;
  // STARTLE. A fresh noise is NEWER information than a stale memory, and that
  // one sentence is the whole rule: a bot acts on what it just heard whenever
  // it is not currently looking at somebody. The old gate was `tgt < 0`, i.e.
  // hearing only worked while patrolling — measured at 2.4-3.1% of bot-ticks
  // in a live match, so footsteps were dead code exactly when a player is
  // sneaking up on a bot busy with someone else. Comparing against
  // last_seen_tick keeps a SEEN target authoritative (you do not turn your
  // back on the man shooting at you) while a REMEMBERED one yields to a boot
  // landing behind you, which is the case the complaint was actually about.
  // It stays a look, never a shot: no target assignment, no aimpoint, and the
  // trigger below is gated on `engaging || prefiring`, neither of which this is.
  int startled = b->hear_tick && !engaging && b->hear_tick > b->last_seen_tick;
  b->startled = startled;   // census only (tacstat) — nothing branches on it
  float horizontal_speed = sqrtf(b->vel.x * b->vel.x + b->vel.z * b->vel.z);
  int other_weapon = b->wp.cur == WPN_AR ? WPN_SR : WPN_AR;
  int emergency_swap = engaging && b->wp.ammo[b->wp.cur] <= 0 &&
                       (b->wp.ammo[other_weapon] > 0 || b->wp.ammo[WPN_PT] > 0);
  int desired = bot_desired_weapon(b, engaging, tdist, horizontal_speed);
  desired = mode_bot_weapon(b, bi, desired, engaging, tdist);
  mode_nade_refill(bi + 1, &b->wp);
  // A weapon the mode no longer allows (a GUN GAME promotion) goes at once.
  if (!mode_allowed(bi + 1, b->wp.cur) && b->wp.gr_t == 0) emergency_swap = 1;
  if (b->wp.switch_t == 0 && (b->swap_cd == 0 || emergency_swap) &&
      bot_request_weapon(b, bi, desired))
    b->swap_cd = BOT_SWAP_COOLDOWN_TICKS;

  // An available alternate weapon wins before the automatic empty-mag reload.
  // When neither weapon can fire, retain the existing reload/cover behavior.
  // (Grenades never reload: their stock is the spawn's, or the mode's refill.)
  if (b->wp.ammo[b->wp.cur] <= 0 && b->wp.reload_t == 0 && b->wp.switch_t == 0 &&
      b->wp.cur != WPN_GR) {
    b->wp.reload_tac = 0;    // a bot only ever reloads when the mag is dry
    b->wp.reload_t = WPN_DEF[b->wp.cur].reload_ticks;
    ev_push((event_t){.type = EV_RELOAD, .a = b->pos, .wpn = b->wp.cur,
                      .src = bi + 1});
  }

  // Decide where to look and where to move.
  float want_yaw = b->yaw, want_pitch = 0;
  // Intent, not physics: what a human would be holding on the keyboard this
  // tick. `pace` is the one scale on it — a bot that has not seen anybody walks
  // its patrol, and everything from the moment it engages runs at the player's
  // full RUN SPEED.
  float wx = 0, wz = 0, pace = BOT_PATROL_PACE;
  int want_slide = 0, want_jump = 0;
  if (engaging) {
    // The error magnitude settles while tracking; its direction drifts a few
    // times a second and the offset eases toward the goal — imperfect like a
    // human, never jittery like a broken servo.
    b->err += (sk->err1 - b->err) * mv_alpha(sk->settle_ms);
    if (--b->drift_t <= 0) {
      b->drift_t = (int)rng_rangef(&g_rng_bot, 24, 42);
      b->goal_y = rng_rangef(&g_rng_bot, -b->err, b->err);
      b->goal_p = rng_rangef(&g_rng_bot, -b->err, b->err) * 0.6f;
    }
    float ek = ease_k(5.0f);
    b->off_y += (b->goal_y - b->off_y) * ek;
    b->off_p += (b->goal_p - b->off_p) * ek;

    v3 o = {{b->pos.x, b->pos.y + bot_eye(b), b->pos.z}};
    v3 tp = ent_aimpoint(b->tgt);
    // Aim at where the target IS. These weapons are hitscan, so there is nothing
    // to lead: a forward offset is a systematic miss, and once bots moved at the
    // player's speed instead of a third of it, the 0.13 s "human anticipation"
    // lead threw the crosshair 1.5 m off and cost the hard tier a third of its
    // hit rate (37.1% -> 24.6% measured). What that lead was really reaching for
    // — a weak player's shots trailing a strafing target — falls out of the
    // turn-rate cap below all by itself, with the right sign.
    float dx = tp.x - o.x, dy = tp.y - o.y, dz = tp.z - o.z;
    float hl = sqrtf(dx * dx + dz * dz);
    // Error grows with range: the same angular sloppiness costs more metres far
    // away, and a bot that is pinpoint at 30 m does not read as a person.
    float rng_k = 1.0f + f_clamp(tdist * 0.020f, 0.0f, 0.55f);
    // Fight the recoil, as far as this skill can. Bots shoot through the same
    // climb the player does now, so without this the hard tier would spray its
    // long bursts into the sky; with it, recoil control is what separates a good
    // shooter from a bad one — which is how it works for people.
    float cy = b->wp.rec_y * sk->rec_comp * DEG2RAD;
    float cp = b->wp.rec_p * sk->rec_comp * DEG2RAD;
    want_yaw = atan2f(dx, -dz) + (b->off_y * rng_k + b->fl_y) * DEG2RAD - cy;
    want_pitch = f_clamp(atan2f(dy, hl) + (b->off_p * rng_k + b->fl_p) * DEG2RAD - cp,
                         -1.2f, 1.2f);

    // Combat footwork: strafe with occasional direction changes (sometimes
    // planting), drift toward/away to hold a 7..16 m engagement range.
    if (--b->strafe_t <= 0) {
      b->strafe_t = (int)rng_rangef(&g_rng_bot, 70, 180);
      uint32_t rr = rng_next(&g_rng_bot) % 10;
      b->strafe_dir = rr < 4 ? 1 : rr < 8 ? -1 : 0;
    }
    // TAKE COVER when there is a reason to be out of the line: an empty mag, or
    // hurt and low on health. A bot that keeps trading in the open while it is
    // reloading is the least human thing it can do. Breaking the line also hands
    // it to the pre-aim/peek branch below, so the whole loop reads the way a
    // person plays it: get hit, break off behind something, reload, hold the
    // angle, peek back out.
    // The deadlock decision outranks both of the legs below: when a bot is
    // welded to a corner, the destination one of THEM picked is usually what is
    // wrong, so overriding it is the whole point.
    int deadlocked = bot_deadlock_steer(b, tdist, &wx, &wz, &pace);
    if (deadlocked) {
      b->crouch = 0;
      b->in_cover = 0;   // the cover leg did not run this tick; do not go stale
    }
    if (!deadlocked) bot_cover_steer(b, NULL, &wx, &wz, &pace);
    // Stand up while walking to cover — kept at the CALL SITE because the
    // belief branch's copy of this leg never did it. That difference is
    // preserved rather than unified: it is one behaviour change, it belongs in
    // a measured tacstat A/B (cover_pct, crouch_pct), not in a refactor.
    if (b->in_cover) b->crouch = 0;
    // FLANK, after cover and before range-keeping: needing to be out of the
    // line beats wanting a better angle, and a bot on the swing is not also
    // holding a range band. Standing up is not optional here — see the leg.
    int flanking = !deadlocked && !b->in_cover &&
                   bot_flank_steer(b, sk, tdist, &wx, &wz, &pace);
    if (flanking) b->crouch = 0;
    if (!deadlocked && !b->in_cover && !flanking && hl > 0.01f) {
      float tx = dx / hl, tz = dz / hl;
      // Range keeping needs a DEAD BAND now that bots move at the player's
      // 12 m/s: the whole 8..16 m window is crossed in well under a second, so a
      // hard in/out flip at the edges makes a bot jitter on the spot. Ease the
      // advance in proportion to how far outside the band it actually is.
      float adv = 0.0f, side = 0.9f;
      if (b->wp.cur == WPN_KN) {
        // A BLADE HAS NO RANGE BAND: it closes. The strafe is kept, weakened,
        // as the zig-zag that makes a charging man hard to track.
        adv = tdist > 1.2f ? 1.0f : 0.0f;
        side = tdist > 4.0f ? 0.45f : 0.2f;
      } else if (tdist > 16.0f) adv = f_clamp((tdist - 16.0f) / 7.0f, 0.0f, 1.0f) * 0.9f;
      else if (tdist < 8.0f) adv = -f_clamp((8.0f - tdist) / 4.0f, 0.0f, 1.0f) * 0.9f;
      wx = -tz * (float)b->strafe_dir * side + tx * adv;
      wz =  tx * (float)b->strafe_dir * side + tz * adv;
      // Tactical slide: close the gap fast, or break off sideways when being
      // shot at. Gated by a cooldown and by skill so EASY bots rarely do it.
      // Rate matters as much as the behaviour: a slide is a once-every-several-
      // seconds commitment, not a habit. A census showed 5%/tick putting bots in
      // a slide a THIRD of the time, which reads as a bug, not as tactics.
      if (!b->sliding && b->grounded && b->slide_cd == 0 &&
          rng_rangef(&g_rng_bot, 0.0f, 1.0f) < sk->tac * 0.006f) {
        int knife = b->wp.cur == WPN_KN;
        int dodge = !knife && b->hurt_t > 0 && tdist < 13.0f;
        if (dodge || tdist > (knife ? 5.0f : 14.0f)) {
          // Steer into the slide and ask for it; the shared entry gate decides
          // whether the bot is actually moving fast enough to have one.
          wx = dodge ? -tz * (float)(b->strafe_dir ? b->strafe_dir : 1) : tx;
          wz = dodge ?  tx * (float)(b->strafe_dir ? b->strafe_dir : 1) : tz;
          want_slide = 1;
        }
      }
    }
    // Hold low at range: crouching steadies the aim and genuinely shrinks the
    // hitbox (bot_trace_one follows the stance), so it is a real trade and not
    // a pose. Never while closing, never while suppressed.
    if (!b->sliding) {
      int want_cr = tdist > 13.0f && b->strafe_dir == 0 && b->hurt_t == 0 &&
                    b->wp.cur != WPN_KN &&
                    b->wp.reload_t == 0 && b->flank_t == 0 && b->dl_t == 0;
      if (b->crouch_t < 0) b->crouch_t++;  // standing again, briefly
      if (b->crouch) {
        // A hold is a BURST, not a posture. Exiting only when the SITUATION
        // changes is not enough: in a duel across a long angle nothing changes,
        // so the bot just stays down — a lone bot measured 80% of its life
        // crouched, which reads as a bot that sits rather than one that holds.
        if (--b->crouch_t <= 0 || !want_cr) {
          b->crouch = 0;
          b->crouch_t = -(int)rng_rangef(&g_rng_bot, 240.0f, 600.0f);
        }
      } else if (want_cr && b->crouch_t == 0 &&
                 rng_rangef(&g_rng_bot, 0.0f, 1.0f) < 0.06f) {
        b->crouch = 1;
        b->crouch_t = (int)rng_rangef(&g_rng_bot, 130.0f, 320.0f);
      }
    }

    // Dodge jump: breaks a tracking crosshair and is the other half of "pops out
    // unexpectedly". Only while being shot at, and rate-limited hard.
    if (!b->sliding && b->grounded && b->jump_cd == 0 && b->hurt_t > 0 &&
        tdist > 5.0f && tdist < 20.0f &&
        rng_rangef(&g_rng_bot, 0.0f, 1.0f) < sk->tac * 0.012f) {
      want_jump = 1;
      b->jump_cd = (int)rng_rangef(&g_rng_bot, 420, 900);
      b->crouch = 0;
    }
    pace = 1.0f;
  } else if (startled) {
    // An audible position is not a target: no aimpoint, no target assignment,
    // no combat trigger. Turn first, then walk the patrol pace to investigate.
    // This branch sits ABOVE the belief branch so a live noise outranks a stale
    // memory; b->tgt is untouched, so the moment the bot sees whoever it is
    // still after, `engaging` takes the tick back.
    float dx = b->hear_pos.x - b->pos.x, dz = b->hear_pos.z - b->pos.z;
    float hl = sqrtf(dx * dx + dz * dz);
    // The STORE flattens y (a bearing along the floor is what is heard), so the
    // consumer has to put a person back on it — the same chest height
    // bot_belief_aimpoint uses. Reading the stored 0 directly aimed the head at
    // the ground: a gunshot is emitted at the MUZZLE, so the pitch went to -19
    // deg at 8 m and -39 deg at 2 m, in the one branch whose whole job is to
    // look like it noticed you.
    float dy = (b->hear_pos.y + EYE_STAND - ENT_CHEST_OFF) -
               (b->pos.y + bot_eye(b));
    if (hl > 0.01f) {
      want_yaw = atan2f(dx, -dz);
      want_pitch = f_clamp(atan2f(dy, hl), -1.2f, 1.2f);
      if (hl > 1.8f && fabsf(wrap_pi(want_yaw - b->yaw)) <= 1.0471976f) {
        // Look at the sound, WALK the route to it (the sound itself off a
        // routed map).
        v3 rw = bot_route(b, b->hear_pos);
        float rx = rw.x - b->pos.x, rz = rw.z - b->pos.z, rl = sqrtf(rx * rx + rz * rz);
        if (rl > 0.01f) { wx = rx / rl; wz = rz / rl; }
        pace = BOT_PATROL_PACE;
      }
    }
    b->crouch = 0;
  } else if (b->tgt >= 0) {
    // Lost sight: aim and movement use only the stored belief/search memory.
    v3 tp = bot_belief_aimpoint(b);
    v3 eo = {{b->pos.x, b->pos.y + bot_eye(b), b->pos.z}};
    float adx = tp.x - b->pos.x, adz = tp.z - b->pos.z;
    float ahl = sqrtf(adx * adx + adz * adz);
    if (ahl > 0.01f) {
      float aimy = tp.y - eo.y;
      want_yaw = atan2f(adx, -adz) + b->fl_y * DEG2RAD;
      want_pitch = f_clamp(atan2f(aimy, ahl), -1.2f, 1.2f);
    }
    b->crouch = 0;

    // PRE-FIRE: a human who is CONFIDENT the corner is about to break holds
    // the trigger through it — rounds land on the angle as it opens, before
    // visual confirmation. Confidence is a TIGHT belief (the dead-reckoning
    // radius still small), fresh memory and short range; the shot itself goes
    // through the normal trigger discipline below (settle, bursts, pauses),
    // aimed at the same belief point the pre-aim already holds. A tactic, so
    // its RATE rides sk->tac (never a gun-handling knob), and the burst
    // window is committed once so it reads as intent rather than as flicker.
    if ((b->belief == BOT_BELIEF_OCCLUDED_HOLD ||
         b->belief == BOT_BELIEF_INVESTIGATE) &&
        b->belief_radius < 1.3f && ahl > 2.0f && ahl < 20.0f &&
        (int)(g_tick - b->last_seen_tick) < 84) {
      if (b->prefire_t > 0) b->prefiring = 1;
      else if (b->prefire_cd == 0 &&
               rng_rangef(&g_rng_bot, 0.0f, 1.0f) < sk->tac * 0.010f) {
        b->prefire_t = (int)rng_rangef(&g_rng_bot, 24.0f, 54.0f);
        b->prefire_cd = (int)rng_rangef(&g_rng_bot, 300.0f, 700.0f);
        b->prefiring = 1;
      }
    }

    // Same cover leg, probed against the BELIEVED position instead of a seen
    // one. The return value, not b->in_cover: a bot that just arrived has
    // already handled its tick and must not fall through into the search walk.
    if (bot_cover_steer(b, &tp, &wx, &wz, &pace)) {
      // steering (or arrival) fully handled by the shared leg
    } else if (b->belief == BOT_BELIEF_OCCLUDED_HOLD &&
               bot_flank_walk(b, &wx, &wz, &pace)) {
      // FINISH THE SWING. Only while the bot still believes in the target it
      // planned around (OCCLUDED_HOLD, not INVESTIGATE/SEARCH — once it has
      // started sweeping for a lost man the old anchor means nothing), and it
      // is the one thing worth doing with a hold, which otherwise stands still.
      b->crouch = 0;
    } else if (b->belief == BOT_BELIEF_OCCLUDED_HOLD) {
      wx = wz = 0.0f;
    } else {
      float sdx = b->search_pos.x - b->pos.x;
      float sdz = b->search_pos.z - b->pos.z;
      float sl = sqrtf(sdx * sdx + sdz * sdz);
      // `sl` stays the distance to the search point itself (arrival is about
      // getting THERE); the walk direction below follows the route to it.
      v3 rw = bot_route(b, b->search_pos);
      if (map_routes()) {
        float rx = rw.x - b->pos.x, rz = rw.z - b->pos.z, rl = sqrtf(rx * rx + rz * rz);
        if (rl > 0.01f) { sdx = rx / rl * sl; sdz = rz / rl * sl; }
      }
      if (sl > 0.01f) want_yaw = atan2f(sdx, -sdz);
      if (b->belief == BOT_BELIEF_INVESTIGATE) {
        if (b->search_t > 0) b->search_t--;
        if (sl < 0.9f || b->search_t <= 0) {
          b->belief = BOT_BELIEF_SEARCH_SWEEP;
          bot_search_plan_next(b, sk);
        } else {
          wx = sdx / sl;
          wz = sdz / sl;
          // A human RUNS the approach and only slows for the last angle — the
          // constant 0.75 shuffle read as a bot creeping everywhere. Running
          // is also what makes the two peek moves below reachable at all: a
          // slide has to be EARNED at speed (the shared entry gate).
          pace = sl > 8.0f ? 1.0f : 0.75f;
          float spd2 = sqrtf(b->vel.x * b->vel.x + b->vel.z * b->vel.z);
          // SLIDE-PEEK / traversal slide: break into the believed angle low
          // and fast, or eat distance on a long approach. Intent only fires
          // when the shared gate would take it (speed check here), so a
          // request is never silently eaten.
          if (!b->sliding && b->grounded && b->slide_cd == 0 && sl > 3.0f &&
              spd2 > g_sim_mv[MV_RUN] * SLIDE_START_SPEED &&
              rng_rangef(&g_rng_bot, 0.0f, 1.0f) < sk->tac * 0.006f) {
            want_slide = 1;
            b->peek = sl < 12.0f;
          }
          // CORNER-JUMP: closing on a tight belief, occasionally break the
          // sightline vertically as the angle opens — the human peek-jump.
          // Rate rides sk->tac; the cooldown keeps it an event, not a habit.
          if (!b->sliding && b->grounded && b->jump_cd == 0 && sl > 2.5f &&
              sl < 13.0f && b->belief_radius < 3.2f &&
              rng_rangef(&g_rng_bot, 0.0f, 1.0f) < sk->tac * 0.022f) {
            want_jump = 1;
            b->peek = 1;
            b->jump_cd = (int)rng_rangef(&g_rng_bot, 360.0f, 780.0f);
          }
        }
      } else if (b->belief == BOT_BELIEF_SEARCH_SWEEP) {
        if (sl < 0.9f) {
          wx = wz = 0.0f;
          if (b->search_t > 0) b->search_t--;
          if (b->search_t <= 0) bot_search_plan_next(b, sk);
        } else {
          wx = sdx / sl;
          wz = sdz / sl;
          pace = 0.75f;
        }
      }
    }
  }
  if (b->tgt < 0 && !startled && b->alert_t > 0) {
    // CLEARING THE SPOT. Not a target and not a noise — a place the bot has
    // unfinished business with. It walks over rifle up, faster than a patrol
    // and slower than a run, and the moment it has BEEN there the business is
    // finished and the alert ends; the timer is only the backstop for a spot it
    // cannot reach. Acquiring anyone on the way pre-empts this branch, and a
    // fresh noise pre-empts it through `startled`, so it never traps the bot.
    b->crouch = 0;
    float adx = b->alert_p.x - b->pos.x, adz = b->alert_p.z - b->pos.z;
    float al = sqrtf(adx * adx + adz * adz);
    if (al < BOT_ALERT_ARRIVE) {
      b->alert_t = 0;                    // checked it; stand down
    } else {
      v3 rw = bot_route(b, b->alert_p);
      float rx = rw.x - b->pos.x, rz = rw.z - b->pos.z, rl = sqrtf(rx * rx + rz * rz);
      if (rl > 0.01f) { adx = rx; adz = rz; al = rl; }
      want_yaw = atan2f(adx, -adz);
      wx = adx / al;
      wz = adz / al;
      pace = BOT_ALERT_PACE;
    }
  }
  if (b->tgt < 0 && !startled && b->alert_t == 0 &&
      !(rescue_on() && (bi < g_resc.nguards
                          ? rescue_patrol(b, bi, &want_yaw, &want_pitch, &wx, &wz, &pace)
                          : rescue_squad_intent(b, bi, &want_yaw, &want_pitch, &wx, &wz, &pace)))) {
    // Patrol: wander between random reachable waypoints.
    // STAND UP FIRST. Every other branch clears the crouch and this one did
    // not, so a bot that killed its target from a crouched hold stayed crouched
    // for the rest of the match — and self-reinforcingly, because patrol at
    // crouch speed is 1.4 m/s against 6.6, which is too slow to cover the arena
    // and reach any branch that would have cleared it. Measured at seed 1337,
    // one bot, hard: crouch_pct 100.0 over the last 3600 ticks, still crouched,
    // still targetless. It hits 1v1 hardest, i.e. exactly the configuration the
    // AI gets judged in.
    // crouch_t goes with it. Only the crouch leg's own exit resets that timer,
    // so an external clear that leaves it positive latches the bot out of ever
    // crouching again (the `crouch_t == 0` arm of the entry test) — a
    // pre-existing trap the belief and cover clears still carry, and one this
    // site must not add a fourth instance of. A patrolling bot is simply
    // standing and free to crouch when it next has a reason.
    b->crouch = 0;
    b->crouch_t = 0;
    float dx = b->target.x - b->pos.x, dz = b->target.z - b->pos.z;
    float dist = sqrtf(dx * dx + dz * dz);
    if (dist < 0.8f || b->stuck > 60) {
      for (int i = 0; i < 16; i++) {
        v3 c = {{rng_rangef(&g_rng_bot, -ARENA_HALF + 2, ARENA_HALF - 2), 0,
                 rng_rangef(&g_rng_bot, -ARENA_HALF + 2, ARENA_HALF - 2)}};
        float tx = c.x - b->pos.x, tz = c.z - b->pos.z;
        if (tx * tx + tz * tz < 64.0f) continue;  // wander legs are >=8 m
        if (!aabb_fits(c, PLAYER_RADIUS, HEIGHT_STAND)) continue;
        b->target = c;
        break;
      }
      b->stuck = 0;
      dx = b->target.x - b->pos.x;
      dz = b->target.z - b->pos.z;
    }
    if (map_routes()) {           // face (and so walk) the route, not the goal
      v3 rw = bot_route(b, b->target);
      dx = rw.x - b->pos.x;
      dz = rw.z - b->pos.z;
    }
    want_yaw = atan2f(dx, -dz);
    float fdiff = wrap_pi(want_yaw - b->yaw);
    if (fdiff < 1.2f && fdiff > -1.2f) {  // walk once roughly facing the goal
      wx = sinf(b->yaw);
      wz = -cosf(b->yaw);
    }
  }

  // Turn: proportional approach capped at the skill's rate — decisive on big
  // angles, gentle over the last degrees. Pitch tracks at a softer rate.
  // ...except when the goal came from the EAR, which gets the startle rate.
  //
  // The gain stays 8. Raising it was tried and REVERTED: it does fix the stare
  // (measured 351-486 ticks down to 209-341) but only by making bots track like
  // good players, which took hard from 178 kills per 7200 ticks to 274 — the
  // trailing lag is this AI's designed weak-shooter handicap, not a defect, and
  // the fix for "it never shoots" belongs in the trigger, not here.
  float diff = wrap_pi(want_yaw - b->yaw);
  float mt = (startled ? BOT_ALERT_TURN : sk->turn) * TICK_DT;
  b->yaw += f_clamp(diff * 8.0f * TICK_DT, -mt, mt);
  float pdiff = want_pitch - b->pitch;
  float mp = sk->turn * 0.7f * TICK_DT;   // pitch stays on the tracking rate
  b->pitch += f_clamp(pdiff * 8.0f * TICK_DT, -mp, mp);

  // WALL-FOLLOW. There is no pathfinding in this game and there should not be,
  // so every leg above picks a destination and walks the straight line to it —
  // and a straight line into a corner is a bot standing on the spot until
  // whatever chose that destination gives up. The cheapest honest substitute
  // for a path is to stop pushing on the wall and slide ALONG it, which is what
  // a person does when the doorway is not where they thought it was.
  //
  // It lives HERE, on the finished wish vector, so it covers every leg at once
  // — patrol, the noise investigation, cover, the flank and combat footwork all
  // walk straight lines and all could press. The measured worst cases were NOT
  // in combat: 228 and 162 ticks with engaging=0, i.e. a bot walking to a
  // patrol waypoint or to a heard noise. The side comes from strafe_dir so it
  // is deterministic and does not dither, and the whole thing is off again the
  // moment the bot actually moves (b->stuck resets on any real speed).
  // ESCALATING, because one perpendicular is not enough: in a 90-degree corner
  // the sideways direction is blocked too, and a bot that only ever tries the
  // same one stays welded to the corner (measured: a fixed 90-degree slide left
  // the worst case at 228 ticks, unchanged). So the wish is turned a further
  // quarter every 30 ticks of not moving — 90, then 180 (back out the way it
  // came, which always works), then 270. Nothing accumulates: the cycle is
  // derived from the pin counter, which any real movement clears.
  // Gated on b->pin, NOT b->stuck: three separate handlers reset stuck at 30 or
  // 60 (the search replan, the combat escape, the patrol waypoint re-pick), so
  // it almost never gets above the threshold and a wall-follow hanging off it
  // silently never ran — the worst case stayed at 228 ticks, unchanged, which
  // is how this was caught. pin is the census counter and only real movement
  // clears it, which is exactly the condition this wants.
  // ...and the cheaper half: DO NOT WALK INTO IT IN THE FIRST PLACE. Everything
  // above is recovery — it needs `pin` to reach 30, i.e. a quarter of a second
  // of a bot leaning on a wall with its legs running, and that quarter second is
  // most of what a spectator ever sees of it. Worse, the collision box is
  // narrower than the figure standing in it: PLAYER_RADIUS is 0.35 and `figm`
  // measures the low-ready carry at 769 mm across, so a bot pressed flush has
  // ~35 mm of strong-side elbow INSIDE the masonry. Widening the box is not the
  // fix (it is the sim's, the net's and the trace's radius, and a fatter bot is
  // a different game); not arriving flush is.
  //
  // A PROJECTION, NOT A FORCE, which is what makes it safe to put on the
  // finished wish vector: only the component pointing INTO a near face is
  // removed, so a bot heading for cover still arrives at it and simply glides
  // the last few centimetres along the face instead of grinding into it. A
  // force would fight the cover legs and dither; a projection cannot, because
  // once the inward component is gone there is nothing left for it to act on.
  // The small outward term is separate and only lives inside the elbow band.
  {
    float bh_ = b->crouch ? HEIGHT_CROUCH : HEIGHT_STAND;
    // HEAD-ON IS THE PROJECTION'S BLIND SPOT. Walking square into a face, the
    // inward component IS the whole wish, so removing it leaves nothing: the bot
    // stands still, and the pin escape below never runs because it needs a wish
    // to rotate. Measured in LAST MAN STANDING, two survivors pinging each other
    // through one wall stood 1.9 m apart for minutes. So the face that took the
    // most is remembered, and a wish it cancelled is turned along that face.
    float w0 = sqrtf(wx * wx + wz * wz), big_into = 0.0f, fnx = 0.0f, fnz = 0.0f;
    BP_BOX(i, b->pos.x - PLAYER_RADIUS - 2.0f, b->pos.z - PLAYER_RADIUS - 2.0f,
           b->pos.x + PLAYER_RADIUS + 2.0f, b->pos.z + PLAYER_RADIUS + 2.0f) {
      const solid_t *sd = &g_solids[i];
      if (!sd->pen) continue;                       // the shell clamps elsewhere
      // Only solids this body actually shares height with — a bot standing ON a
      // platform is inside it in plan view and must not be shoved off its own
      // footing. 0.2 m of lift is the step the locomotion already swallows.
      // ...and on a stepped map, nothing the locomotion will step UP onto: a
      // stair's face is not a wall, and steering off it (measured) left two
      // bots routed up a stair standing in front of the first step for good.
      if (sd->max.y <= b->pos.y + fmaxf(0.20f, STEP_H) || sd->min.y >= b->pos.y + bh_)
        continue;
      // Signed distance in XZ to the face, through the box grown by the
      // collision radius — the same expansion move_axis resolves against.
      float ex = fmaxf(sd->min.x - PLAYER_RADIUS - b->pos.x,
                       b->pos.x - (sd->max.x + PLAYER_RADIUS));
      float ez = fmaxf(sd->min.z - PLAYER_RADIUS - b->pos.z,
                       b->pos.z - (sd->max.z + PLAYER_RADIUS));
      // PAST BOTH FACES IS A CORNER, NOT A FACE. Outside the box's span on
      // both axes, the nearest thing is the edge where the faces meet, so the
      // distance is the diagonal and the normal points away from that edge.
      // Picking the nearer face's axis there (as this once did) blocked
      // motion ALONG the other face — a bot just past the end of a crate
      // could not walk past the end of the crate, measured as a bot frozen
      // there for a whole LAST MAN STANDING round.
      float d, nx = 0.0f, nz = 0.0f;
      if (ex > 0.0f && ez > 0.0f) {
        d = sqrtf(ex * ex + ez * ez);
        if (d >= BOT_WALL_BAND) continue;
        nx = (b->pos.x > sd->max.x ? 1.0f : -1.0f) * ex / d;
        nz = (b->pos.z > sd->max.z ? 1.0f : -1.0f) * ez / d;
      } else {
        d = fmaxf(ex, ez);
        if (d >= BOT_WALL_BAND) continue;
        // Beside a face: the outward normal is that face's own axis (the one
        // the bot is least buried along). These ARE the world's AABBs.
        if (ex >= ez) nx = b->pos.x > sd->max.x ? 1.0f : -1.0f;
        else          nz = b->pos.z > sd->max.z ? 1.0f : -1.0f;
      }
      float into = wx * nx + wz * nz;
      if (into < big_into) { big_into = into; fnx = nx; fnz = nz; }
      if (into < 0.0f) { wx -= nx * into; wz -= nz * into; }  // slide, don't press
      // The elbow band: the last 50 mm, where the figure is wider than the box
      // it collides with. A nudge, not a shove — it has to be too weak to pull
      // a bot off a corner it chose.
      if (d < BOT_WALL_ELBOW) {
        float k = (BOT_WALL_ELBOW - d) * (1.0f / BOT_WALL_ELBOW) * 0.35f;
        wx += nx * k; wz += nz * k;
      }
    }
    // ...and a wish the projection only MOSTLY cancelled is a slide that
    // creeps: measured at a wall's end, 15% of the wish left a bot standing
    // on the corner for the rest of a round. So a wish cut below half keeps
    // the direction it has left at the speed it came in with, which is how a
    // player slides along a wall; one cut to nothing picks a side by strafe_dir.
    float wr = sqrtf(wx * wx + wz * wz);
    if (w0 > 0.3f && wr < 0.5f * w0 && big_into < 0.0f) {
      if (wr > 0.05f * w0) {
        wx *= w0 / wr;
        wz *= w0 / wr;
      } else {
        float s = b->strafe_dir >= 0 ? 1.0f : -1.0f;
        wx = -fnz * s * w0;
        wz =  fnx * s * w0;
      }
    }
  }

  if (b->pin > 30 && wx * wx + wz * wz > 0.01f) {
    float s = b->strafe_dir >= 0 ? 1.0f : -1.0f;
    float ox = wx, oz = wz;
    switch (((b->pin - 1) / 30) % 3) {
      case 0: wx = -oz * s; wz = ox * s; break;   // slide along it
      case 1: wx = -ox;     wz = -oz;    break;   // back out
      default: wx = oz * s; wz = -ox * s; break;  // slide the other way
    }
  }

  // Move on the PLAYER's locomotion — same helpers, same numbers, same order.
  // The intent above (wx/wz, want_slide, want_jump, crouch) is all that differs
  // from a human holding keys: top speed, spin-up, slide boost/decay/cooldown,
  // jump height and air control are the player's, straight out of g_cfg.mv.
  float wlen = sqrtf(wx * wx + wz * wz);
  if (wlen > 1.0f) { wx /= wlen; wz /= wlen; wlen = 1.0f; }
  float sp2d = sqrtf(b->vel.x * b->vel.x + b->vel.z * b->vel.z);
  // slide_cd is decremented ONCE per tick, up in the tactical-cooldown block
  // beside pause_t/react_t/jump_cd — the same shape player_tick uses. It used to
  // be decremented here as well, so SLIDE_COOLDOWN_TICKS elapsed at 2x for a bot
  // and 1x for the player: a locomotion-parity break that `parity` cannot see,
  // because that compares NUMBERS returned by shared code, not per-tick
  // bookkeeping. Decrementing at the top is also what makes the tactic gate
  // (want_slide, above) and the entry gate (below) read the same value in a tick.
  // A slide has to be EARNED at speed, exactly like the player's: the same
  // fraction-of-run-speed entry gate, the same boost multiplier, the same
  // cooldown. The old code just assigned a velocity, which is a teleport a human
  // has no access to.
  if (!b->sliding && b->grounded && want_slide && b->slide_cd == 0 &&
      sp2d > g_sim_mv[MV_RUN] * SLIDE_START_SPEED) {
    sp2d = mv_slide_launch(&b->vel, sp2d);
    b->sliding = 1;
    b->crouch = 0;
    b->slide_cd = SLIDE_COOLDOWN_TICKS;
  }
  // Bots hold the crouch through a slide (the literal 1 — every path that set
  // sliding also held it), so it ends the way a player's does: on leaving the
  // ground or bleeding down to crouch-walk speed.
  float ms = (b->crouch ? g_sim_mv[MV_CROUCH_SP] : g_sim_mv[MV_RUN]) * pace;
  ms *= mv_ads_walk_scale(b->wp.ads_s);   // aimed walk: the player's tactical slow
  sp2d = mv_locomote(b, wx, wz, wlen, sp2d, ms);
  // The jump reads grounded AFTER the locomotion step, which cannot change it.
  if (b->grounded && want_jump) {
    b->vel.y = mv_jump_speed();
    b->grounded = 0;
    b->sliding = 0;
  }
  // The hitbox eye follows the stance on the player's own ease (see bot_eye).
  float bot_eye_tgt = b->crouch ? EYE_CROUCH : EYE_STAND;
  b->eye += (bot_eye_tgt - b->eye) * f_clamp(EYE_LERP_RATE * TICK_DT, 0, 1);
  float bh = b->crouch ? HEIGHT_CROUCH : HEIGHT_STAND;
  b->vel.y -= GRAVITY * TICK_DT;
  b->grounded = 0;
  ent_move(&b->pos, &b->vel, &b->grounded, bh);
  // Standing up needs headroom, or a bot could uncrouch inside low cover. This
  // runs BEFORE the arena clamp on purpose — see arena_clamp's comment.
  if (b->crouch == 0 && !aabb_fits(b->pos, PLAYER_RADIUS, HEIGHT_STAND)) b->crouch = 1;
  arena_clamp(&b->pos);

  float hs = sqrtf(b->vel.x * b->vel.x + b->vel.z * b->vel.z);
  if (wx * wx + wz * wz > 0.01f && hs < 0.3f) b->stuck++;
  else b->stuck = 0;
  // THE PIN, censused separately from `stuck` because the escape below RESETS
  // stuck — so stuck can never show how long a bot actually spent leaning on a
  // wall, which is the half of "stuck at the corner in a loop" that the escape
  // exists to bound. Only real movement clears this one.
  if (wx * wx + wz * wz > 0.01f && hs < 0.3f) b->pin++;
  else b->pin = 0;
  if (!engaging && b->tgt >= 0 && b->belief != BOT_BELIEF_VISIBLE &&
      b->stuck > 30) {
    bot_search_plan_next(b, sk);
    b->belief = BOT_BELIEF_SEARCH_SWEEP;
    b->stuck = 0;
  } else if (engaging && b->stuck > 30) {
    // PRESSED INTO GEOMETRY. Flipping the strafe was the only escape here, and
    // it is an escape for the STRAFE leg alone: a bot walking to a cover spot
    // or a flank anchor takes its wish vector from that leg and ignores
    // strafe_dir entirely, so it leaned on the wall until the commitment
    // expired — measured runs of 121 consecutive ticks wanting to move and not
    // moving, standing at an edge with the target in view. That is the "stuck
    // at the corner in a loop" report, and it is also why it does not shoot:
    // pinned against a wall while the target strafes past is the highest
    // angular rate the aim follower ever sees.
    //
    // There is no pathfinding here and there should not be — the honest
    // conclusion from "I have wanted to move for a quarter second and have not"
    // is that THIS destination is not reachable from here, whoever chose it.
    // Give it up, and make sure the next probe cannot hand back the same point.
    if (b->flank_t) bot_flank_release(b);
    b->cov_valid = 0;
    b->cov_retry = 10;
    b->cov_phase++;
    b->strafe_dir = -b->strafe_dir;
    b->stuck = 0;
  }

  // Trigger discipline: shots leave only once the (imperfect) aim has settled
  // onto the target, after the reaction delay, in bursts with pauses. A
  // PRE-FIRE window (armed above, aimed at the belief point) goes through the
  // same discipline — the only difference is what the settle is measured
  // against, which want_yaw/want_pitch already carry in that state.
  // The settle gate is an allowed MISS DISTANCE, not a fixed angle: 1.6 m from
  // the target's centre at whatever range the fight is at. The old flat 0.05 rad
  // was a distance too (0.6 m at 12 m, 7.5 cm at 1.5 m) — it just happened to be
  // the wrong one, because it was not reachable against a MOVING target. The aim
  // follower is a first-order lag (diff * 8 * dt), so its steady-state error
  // against a target sweeping w rad/s is w/8: at 9 m against a 12 m/s strafe the
  // crosshair trails by 1.5 m, permanently, and a 0.6 m gate can never open. The
  // bot then tracks you for SECONDS without firing — measured 368-438
  // consecutive ticks (3.1-3.7 s) of engaging, weapon ready, no shot, which is
  // exactly the "it stands at the corner staring at me and does not shoot"
  // report.
  //
  // It opens ONLY when the bot has been unable to take the shot for a while.
  // Widening it permanently also fixes the stare, and was measured and
  // rejected: it roughly doubles the volume of fire, drops bot accuracy from
  // 30% to 18%, and saturates the voice pool at the 20-bot ceiling
  // (mixdown voices_peak 92/96, dropped=16 against a hard gate of 0). The
  // ordinary case is not broken and must not be re-tuned to fix the broken one.
  //
  // b->stare is the count of consecutive engaging-and-ready ticks with no shot,
  // so half a second of it means the settle will not arrive on its own. Then a
  // person stops waiting for the perfect line and takes the shot at where the
  // target roughly is — 1.6 m is about one trailing shot behind a running man,
  // and the round lands BEHIND them, which is the weak-shooter feel the no-lead
  // comment in the aiming code is describing. Firing resets the counter, so
  // this is one trailing shot every half second while the tracking is hopeless,
  // not a stream.
  //
  // The 0.05 rad floor is the old number, so a distant duel keeps exactly the
  // discipline it had, and so does the belief-based pre-fire — tdist is 1e9
  // there, which is what makes blind fire keep the tight gate by construction.
  float aim_gate = b->stare > 60 ? f_clamp(1.60f / fmaxf(tdist, 1.0f), 0.05f, 0.40f)
                                 : 0.05f;
  int trigger = (engaging || b->prefiring) && b->react_t == 0 &&
                b->pause_t == 0 && !b->sliding && b->wp.switch_t == 0 &&
                diff > -aim_gate && diff < aim_gate &&
                pdiff > -aim_gate * 1.2f && pdiff < aim_gate * 1.2f;
  // THE STARE, counted because it is the one failure a player reports in words
  // and no other number here can see: the bot can shoot, has someone to shoot
  // at, and the trigger never comes. tacstat reports the longest run.
  //
  // It LEAKS rather than resetting, because the situation it has to survive is
  // exactly the one where sight flickers: at a corner `engaging` drops for a
  // tick or two at a time, and a hard reset meant the counter never reached the
  // threshold that opens the gate below — the fix for the stare could not fire
  // in the one place the stare actually happens. Firing clears it outright.
  if (trigger)
    b->stare = 0;
  else if (engaging && b->wp.reload_t == 0 && b->wp.switch_t == 0 &&
           b->wp.ammo[b->wp.cur] > 0)
    b->stare++;
  else if (b->stare > 0)
    b->stare--;
  if (b->wp.cur == WPN_GR) {
    // The throw runs the player's own clock (gr_t through NADE_RELEASE_T and
    // NADE_THROW_T), so the arm animates the same; the solution is taken at
    // the release, against where the target is THEN.
    weapon_t *w = &b->wp;
    if (w->gr_t > 0) {
      if (++w->gr_t == NADE_RELEASE_T) {
        v3 at, vel;
        int fuse;
        v3 tp = b->tgt >= 0 ? ent_pos(b->tgt) : b->last_seen;
        if (bot_nade_solve(b, tp, sk->err1 * 2.0f + 1.5f, &at, &vel, &fuse)) {
          nade_throw(at, vel, fuse, bi + 1);
          w->ammo[WPN_GR]--;
        }
      }
      if (w->gr_t > NADE_THROW_T) {
        w->gr_t = 0;
        w->cool[WPN_GR] = TICK_HZ * 3 / 2;     // a breath between throws
        mode_nade_refill(bi + 1, w);
      }
    } else if (engaging && b->react_t == 0 && b->wp.switch_t == 0 &&
               w->ammo[WPN_GR] > 0 && w->cool[WPN_GR] == 0 &&
               diff > -0.15f && diff < 0.15f &&
               tdist >= BOT_NADE_MIN && tdist <= BOT_NADE_MAX) {
      w->gr_t = 1;
      ev_push((event_t){.type = EV_RELOAD, .src = bi + 1, .a = b->pos, .wpn = WPN_GR});
    }
  } else if (b->wp.cur == WPN_KN) {
    // The AI's only knife decision is the range: everything else is the gate
    // the guns already use, so a bot still has to be facing you to cut you.
    v3 bo = {{b->pos.x, b->pos.y + bot_eye(b), b->pos.z}};
    melee_tick(bi + 1, &b->wp, bo, b->yaw, b->pitch, &b->anim,
               trigger && tdist < MELEE[b->wp.melee_n % 3].reach);
  } else if (trigger && b->wp.cool[b->wp.cur] == 0 && b->wp.reload_t == 0 &&
      b->wp.ammo[b->wp.cur] > 0) {
    bot_fire(bi);
    if (--b->burst <= 0) {
      b->burst = (int)rng_rangef(&g_rng_bot, (float)sk->burst_lo,
                                 (float)sk->burst_hi + 0.99f);
      b->pause_t = (int)(rng_rangef(&g_rng_bot, sk->pause_lo, sk->pause_hi) *
                         (float)TICK_HZ / 1000.0f);
    }
  }

  // ADS on the same ramp and the same ADS TIME as the player's: a bot shoulders
  // the weapon for a distant target, which buys it the tuned base spread and
  // costs it the same aimed-walk slow. Not shouldering it would have been a
  // quiet asymmetry in the player's favour.
  int want_ads = engaging && b->wp.reload_t == 0 && b->wp.switch_t == 0 &&
                 !b->sliding && tdist > 9.0f;
  // ...and the PUPPET can ask for it directly. The anim testbed exists so the
  // pose being tuned can be reached deterministically, and ADS is the pose that
  // decides where a buttstock ends up — measuring it by waiting for a live bot
  // to find a distant target is not a measurement, it is a coincidence.
  // With the knife out, ADS is the guard. A bot reads an incoming swing's windup
  // and catches every other one: often enough to matter, not a wall.
  if (b->wp.cur == WPN_KN) {
    const weapon_t *tw = b->tgt >= 0 ? ent_weapon(b->tgt) : NULL;
    want_ads = tw && tw->cur == WPN_KN && tw->melee_t > 0 && b->wp.melee_t == 0 &&
               tw->melee_t <= MELEE[tw->melee_v].strike1 && (tw->melee_n & 1);
    b->wp.guard_t = want_ads && b->wp.ads_lin > 0.0f ? b->wp.guard_t + 1 : 0;
  }
  gun_ads(&b->wp, want_ads);
  // Recoil recovers the same way it does for the player: only once the trigger
  // is off, so a bot has to fight its own climb through a burst.
  gun_recover(&b->wp, trigger);
  // Bots do not lean yet: the pose supports it (the puppet drives it) but no AI
  // branch asks for it, so this is 0 by construction rather than by omission.
  anim_tick(&b->anim, b->pos, b->vel, b->yaw, b->pitch, b->grounded,
            b->crouch, 0.0f, b->sliding, b->wp.ads_s, 0.0f,
            b->tgt >= 0 || startled || b->alert_t > 0, bi + 1);
  // (`startled` is in there because a bot that turns to a sound with the rifle
  // still slung is the single most "it did not really notice me" frame in the
  // game — the carry state is the only tell the player gets that hearing
  // happened at all, and the raise is ~110 ms, so it lands with the turn.)
  // (The 0.0f is the baked TUCK, and it is 0 by construction rather than by
  // omission: a bot's crouch only picks a shorter collider — `bh` above — and
  // never moves b->pos.y, so nothing is folded into its origin.)
}

// The map seed chain, in ONE place: match_restart advances it per restart and
// the harness's mapcheck walks (and restores) the same chain, so a mapcheck
// `worst_seed` stays reproducible via `restart`.
static uint32_t map_seed_next(uint32_t s) { return s * 747796405u + 2891336453u; }

// THE SCOREBOARD'S OWN ROWS, cleared as one unit. BOTH edges of a networked
// session need exactly this: the join (the pre-join local arena scored into
// rows whose ids collide with server entities, and a stale over=1 pinned the
// VICTORY banner and blocked predicted fire) and the leave (nothing of the
// server's board may survive into the local match). Two copies is how one of
// the two ends up missing the field the other clears.
//
// The four ENTITY-ID-INDEXED counter arrays are their own unit: the client's
// NP_SCORES adoption needs exactly those and must NOT touch over/winner, and a
// fifth counter has to land in both places or it does not land at all.
static void match_counters_clear(void) {
  memset(g_match.kills, 0, sizeof g_match.kills);
  memset(g_match.deaths, 0, sizeof g_match.deaths);
  memset(g_match.shots, 0, sizeof g_match.shots);
  memset(g_match.hits, 0, sizeof g_match.hits);
  memset(g_match.mscore, 0, sizeof g_match.mscore);
}
static void match_scores_clear(void) {
  match_counters_clear();
  g_match.over = 0; g_match.winner = 0; g_match.last_killer = 0;
}

static void rescue_reset(void);
static void match_restart(void) {
  int n = g_match.nbots;
  memset(&g_match, 0, sizeof g_match);   // the human roster lives OUTSIDE
  g_match.nbots = n;                     // g_match and survives this on purpose
  g_match.mode = g_cfg.sp_mode;          // the MATCH tab's choice lands HERE
  g_team_score[0] = g_team_score[1] = 0;
  // A team game started from a deathmatch roster: deal the bots that are
  // already standing to their sides before anyone spawns.
  tdm_deal_bots(0);
  g_match.round_win = -1;
  g_match.round_no = 1;
  g_ballistics = g_ballistics_srv || g_cfg.ballistics;   // latches per match
  proj_clear();
  // Fresh procedural arena every match (or the next map of the rotation, or
  // the chosen handmade one); everyone respawns onto it.
  map_pick_apply(1);
  g_map_seed = map_seed_next(g_map_seed);
  map_generate(g_map_seed);
  g_world_dirty = 1;
  rescue_reset();   // before anyone spawns: posts and the insertion
  // EVERY human (P1-60): respawning only one leaves the others on their old
  // positions inside freshly generated geometry.
  for (int h = 0; h < MAX_HUMANS; h++)
    if (g_humans_on[h])
      player_respawn(&g_players[h], ent_of_human(h));
  for (int i = 0; i < n; i++) bot_spawn(i);
  vfx_match_reset();
}

// THE ONE MATCH-OVER COUNTDOWN — networked sessions only. TWO CALLERS, and
// the second is not optional: a client never runs this file's world tick at
// all (client_online_tick REPLACES player_tick and returns before it, so
// match_tick is unreachable there), so a countdown written only into
// match_tick would be dead code on exactly the half that has to draw it.
//
// A DEDICATED SERVER OWNS ITS OWN REMATCH. Everywhere else the rematch is a
// fire press from g_local_ent, and a server has no local entity (-1), so
// nothing could ever clear g_match.over there: bots stop ticking, respawns
// stop, the trigger is gated off, and the instance sits frozen on its final
// scoreboard until somebody restarts the process.
//
// A CLIENT COUNTS FOR THE HUD AND NOTHING ELSE. It must never set
// restart_req — and the reason is NOT that the write would desync something.
// Measured, that write is INERT (the gate in player_move_tick carries the
// measurement). The reason is that match_tick is the only thing that services
// restart_req and match_tick never runs online, so setting it there is a
// write with no reader: precisely the kind of line that reads as load-bearing
// to the next person to touch this. The real restart arrives as replicated
// g_match.over going 0 in the next NP_SCORES packet, and this counter is
// reset by that same edge. It SATURATES at the threshold rather than
// wrapping or climbing forever.
//
// Honest about the client's number: NP_SCORES is 4 Hz, so the countdown can
// start up to ~250 ms late, and a client that JOINS during the over window
// opens its display at the full 10 s. It is cosmetic — the restart itself is
// server-authoritative.
//
// OFFLINE (neither flag) this is a no-op and the fire press below is
// untouched, byte for byte.
static void match_over_countdown(void) {
  if (!g_server && !g_online) return;
  if (!g_match.over) { g_over_t = 0; return; }
  if (g_over_t < MATCH_OVER_TICKS) g_over_t++;
  if (g_server && g_over_t >= MATCH_OVER_TICKS) {
    g_over_t = 0;
    g_match.restart_req = 1;
  }
}

// Seconds still on that countdown, for the HUD. Rounded UP, so the first
// frame of the over screen reads the full 10 and the last second reads 1.
//
// FLOORED AT 1 — "NEW ROUND IN 0" must never be displayable. On the client
// this counter saturates at the threshold and then WAITS for the restart to
// arrive as replicated over -> 0, which rides NP_SCORES: 4 Hz and unreliable
// UDP, so the wait is ~250 ms typically, ~500 ms across one dropped packet,
// ~750 ms across two — and longer still for a client whose counter started
// late (it can start up to a packet interval behind, and was measured parked
// for 300 ticks). A zero that sits there reads as a hung timer at the exact
// moment the player is staring at it, whereas "1" reads as "any moment now",
// which is the truth. The saturation itself stays: this floors the DISPLAY,
// not the counter.
static int match_over_secs(void) {
  int left = MATCH_OVER_TICKS - g_over_t;
  if (left < 1) left = 1;
  return (left + TICK_HZ - 1) / TICK_HZ;
}

// LAST MAN STANDING. A round is over when at most one combatant is left
// standing — among at least two, so a lone player on an empty server is not
// awarded a round every tick. The survivor scores it; ROUNDS TO WIN of them take
// the match. Between rounds a short intermission holds the result on screen,
// then everyone respawns on the SAME arena (a fresh one per round would be a
// seed change mid-match for every client, for no gain in a short round).
#define LMS_INTERMISSION_TICKS (3 * TICK_HZ)
#define LMS_HUNT_ALIVE 2
#define LMS_HUNT_TICKS (45 * TICK_HZ)
#define LMS_MAX_TICKS  (180 * TICK_HZ)
static void lms_round_reset(void) {
  for (int h = 0; h < MAX_HUMANS; h++)
    if (g_humans_on[h]) player_respawn(&g_players[h], ent_of_human(h));
  for (int i = 0; i < g_match.nbots; i++) {
    bot_t *b = &g_bots[i];
    if (!b->active) continue;
    if (!b->alive)   // the body stays where it fell, as it does on a respawn
      corpse_add(&b->anim, b->pos, b->prev_pos, b->rag, b->yaw, b->topple,
                 b->prev_topple, gun_anim(&b->wp, 1.0f), b->wp.cur, 0, i + 1);
    bot_spawn(i);
  }
  g_match.round_t = 0;
  g_match.round_clock = 0;
  g_match.round_no++;
}

static int lms_alive(int *last) {
  int n = 0;
  ENT_FOREACH(e) if (ent_alive(e)) { n++; if (last) *last = e; }
  return n;
}

static void lms_tick(void) {
  if (game_mode() != GM_LMS || g_match.over) return;
  if (g_match.round_t > 0) {
    if (--g_match.round_t == 0) lms_round_reset();
    return;
  }
  int total = 0;
  ENT_FOREACH(e) { (void)e; total++; }
  int last = -1;
  int alive = lms_alive(&last);
  g_match.round_clock++;
  // THE HUNT. Two survivors on opposite sides of the arena patrol past each
  // other indefinitely — measured, a 6-bot round sat at two alive for minutes.
  // So in the endgame (LMS_HUNT_ALIVE or fewer standing, or a round past
  // LMS_HUNT_TICKS) every two seconds each bot is told where its nearest living
  // rival is, as TARGET MEMORY — the same evidence being shot gives it — not
  // as a noise. A noise is walked at in a straight line, and with no pathing
  // two bots either side of one wall pinned on it for minutes; a remembered
  // target gets the occluded-target tactics instead (search, flank, the
  // deadlock push/break). A round that still will not end is drawn at
  // LMS_MAX_TICKS.
  if (alive > 1 && (alive <= LMS_HUNT_ALIVE || g_match.round_clock > LMS_HUNT_TICKS) &&
      g_match.round_clock % (TICK_HZ / 4) == 0) {
    nav_ready();
    const uint16_t *field = NULL;
    int field_for = -1;
    for (int i = 0; i < g_match.nbots; i++) {
      bot_t *b = &g_bots[i];
      if (!b->active || !b->alive) continue;
      int best = -1;
      float bd = 1e9f;
      ENT_FOREACH(e) {
        if (e == i + 1 || !ent_alive(e) || team_mates(i + 1, e)) continue;
        v3 d = v3_sub(ent_pos(e), b->pos);
        float dd = v3_dot(d, d);
        if (dd < bd) { bd = dd; best = e; }
      }
      if (best < 0) continue;
      // In a fight already: the combat code has it.
      if (b->tgt == best && b->belief == BOT_BELIEF_VISIBLE) continue;
      // The reveal gives it the target (and the reaction delay a surprise
      // costs), once; after that it is only ever told where to walk. Holding
      // the reveal's OCCLUDED_HOLD was measured to deadlock — two survivors
      // each held an angle on the other for minutes.
      if (b->tgt != best) bot_damage_reveal(b, best, 16);
      v3 pp = ent_pos(best);
      if (field_for != best) {
        int goal = nav_snap(pp);
        if (goal < 0) continue;
        field = nav_field_for(goal);
        field_for = best;
      }
      v3 wp;
      if (!nav_next(field, b->pos, 8, &wp)) wp = pp;   // no route: straight
      bot_belief_seed(b, pp, (v3){{0, 0, 0}});
      b->belief = BOT_BELIEF_INVESTIGATE;
      b->search_pos = wp;
      b->search_t = TICK_HZ;
      b->search_visited = 0;
    }
  }
  if (alive > 1 && g_match.round_clock > LMS_MAX_TICKS) {
    g_match.round_win = -1;
    g_match.round_t = LMS_INTERMISSION_TICKS;
    return;
  }
  if (total < 2 || alive > 1) return;
  g_match.round_win = alive == 1 ? last : -1;
  if (alive == 1 && ++g_match.mscore[last] >= g_cfg.sp_rounds) {
    match_end(last);
    return;
  }
  g_match.round_t = LMS_INTERMISSION_TICKS;
}


