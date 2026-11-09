// code/ui/rescue_hud.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// HOSTAGE RESCUE on the HUD: the objective card, the detection arcs round the
// crosshair, ? and ! over the guards you can see, objective markers, the
// RELOAD prompt beside a captive, the banner line, and the debrief.
// ---------------------------------------------------------------------------
static int rescue_proj(v3 w, float *sx, float *sy) {
  const float *m = R.cur_vp.m;
  float cx = m[0] * w.x + m[4] * w.y + m[8] * w.z + m[12];
  float cy = m[1] * w.x + m[5] * w.y + m[9] * w.z + m[13];
  float cw = m[3] * w.x + m[7] * w.y + m[11] * w.z + m[15];
  if (cw < 0.1f) return 0;
  *sx = (cx / cw * 0.5f + 0.5f) * g_ui_vw;
  *sy = (0.5f - cy / cw * 0.5f) * g_ui_vh;
  return *sx > -40.0f && *sx < g_ui_vw + 40.0f && *sy > -40.0f && *sy < g_ui_vh + 40.0f;
}
static void rescue_diamond(float x, float y, float r, float cr, float cg, float cb, float a) {
  ui_line(x, y - r, x + r, y, 1.6f, cr, cg, cb, a);
  ui_line(x + r, y, x, y + r, 1.6f, cr, cg, cb, a);
  ui_line(x, y + r, x - r, y, 1.6f, cr, cg, cb, a);
  ui_line(x - r, y, x, y - r, 1.6f, cr, cg, cb, a);
}
static void rescue_clock(char *b, size_t n, int ticks) {
  int s = (ticks + TICK_HZ - 1) / TICK_HZ;
  snprintf(b, n, "%d:%02d", s / 60, s % 60);
}
static const char *rescue_rating(void) {
  if (g_resc.result != RR_SUCCESS) return "MISSION FAILED";
  if (!g_resc.alarm && !g_resc.ever_seen && g_resc.kills == 0) return "GHOST";
  if (!g_resc.alarm && !g_resc.ever_seen) return "SHADOW";
  if (!g_resc.alarm) return "SILENT";
  return "LOUD";
}

static void rescue_hud(const player_t *p, float ccx, float ccy) {
  if (!rescue_view()) return;
  char b[64];
  float t = (float)g_tick * TICK_DT;
  // THE ALARM VIGNETTE.
  if (g_resc.alarm && !g_match.over) {
    float pu = 0.5f + 0.5f * sinf(t * 6.0f);
    ui_vgrad(0, 0, g_ui_vw, 60, C4(0.55f, 0.04f, 0.02f, 0.30f + 0.15f * pu), C4(0.5f, 0, 0, 0));
    ui_vgrad(0, g_ui_vh - 60, g_ui_vw, 60, C4(0.5f, 0, 0, 0), C4(0.55f, 0.04f, 0.02f, 0.30f + 0.15f * pu));
  }
  // THE OBJECTIVE TRACKER, top left. It was a dark card carrying the mission
  // name (which the banner was saying at the same moment, in letters ten
  // times the size), both objectives at once and a status line — four rows
  // and a box over the corner of the map, all the time. Now: ONE objective,
  // the one that is live, with its progress as pips, and the stealth state as
  // a small status pill under it. No box: the type's own shadow carries it
  // over a bright sky, and the pill brings its own backing. The alarm is the one state that is allowed to
  // shout.
  {
    float x = 14, y = 13;
    int all = g_resc.freed == RESC_HOSTAGES;
    const char *obj = all ? "ESCORT THEM TO THE LZ" : "FREE THE HOSTAGES";
    int prog = all ? g_resc.extracted : g_resc.freed;
    ui_rect(x - 6, y - 1, 1.5f, 13, C_BLAZE, 0.95f);
    ui_text_sh(obj, x, y, 0.82f, TXT_BOLD, 1.25f, C4(C_BONE, 0.97f));
    float px = x + ui_text_width_tr(obj, 0.82f, 1.25f) + 9;
    for (int k = 0; k < RESC_HOSTAGES; k++) {         // progress pips
      int on = k < prog;
      ui_diskn(px + 3.0f + 8.0f * (float)k, y + 4.5f, 2.6f, 10, C4(0, 0, 0, 0.45f));
      ui_diskn(px + 3.0f + 8.0f * (float)k, y + 4.5f, 2.0f, 10,
               on ? C4(0.45f, 0.95f, 0.55f, 1.0f) : C4(C_BONE, 0.28f));
    }
    // A freed hostage is following you while the rest are still captive:
    // that is the one secondary fact worth a line.
    if (!all && g_resc.freed > 0) {
      snprintf(b, sizeof b, "%d FOLLOWING  \x05  LZ MARKED", g_resc.freed - g_resc.extracted);
      ui_text_sh(b, x, y + 13, 0.66f, TXT_REG, 1.2f, C4(C_HAZE, 0.9f));
    }
    // The meters on ME (a guard looking at a teammate is his business, and
    // the squad line below says so when it is serious).
    float maxd = 0.0f;
    int mate_spotted = 0;
    for (int i = 0; i < g_resc.nguards && i < g_match.nbots; i++) {
      if (!g_bots[i].active || !g_bots[i].alive) continue;
      if (g_resc.det_who[i] == g_local_ent || g_resc.det_who[i] < 0) {
        if (g_resc.detect[i] > maxd) maxd = g_resc.detect[i];
      } else if (g_resc.detect[i] >= 1.0f) {
        mate_spotted = 1;
      }
    }
    // A real array, not a pointer to C4(...): a compound literal dies with
    // the if-branch that made it, and the pill read back garbage colours.
    const char *st;
    float sc[4] = {0.45f, 0.9f, 0.6f, 1.0f};
    float stp = 1.0f;
#define SC_SET(r, g, b_) (sc[0] = (r), sc[1] = (g), sc[2] = (b_))
    if (g_resc.alarm) {
      int captive = 0;
      for (int i = 0; i < RESC_HOSTAGES; i++) captive += g_resc.h[i].state == HS_CAPTIVE;
      char c[16];
      rescue_clock(c, sizeof c, g_resc.exec_t);
      if (captive) snprintf(b, sizeof b, "ALARM  EXECUTIONS IN %s", c);
      else snprintf(b, sizeof b, "ALARM  HOSTAGES SAFE");
      st = b; SC_SET(1.000f, 0.294f, 0.227f); stp = 0.65f + 0.35f * sinf(t * 7.0f);
    } else if (maxd >= 1.0f) {
      st = "SPOTTED  KILL HIM BEFORE THE RADIO"; SC_SET(1.000f, 0.294f, 0.227f);
    } else if (mate_spotted) {
      st = "TEAMMATE SPOTTED  TAKE THE GUARD DOWN"; SC_SET(1.000f, 0.294f, 0.227f);
    } else if (maxd > 0.22f) {
      st = "SUSPICIOUS"; SC_SET(1.000f, 0.549f, 0.102f);
    } else if (g_resc.lit) {
      st = "IN THE LIGHT"; SC_SET(0.95f, 0.85f, 0.45f);
    } else {
      st = "UNDETECTED";
    }
#undef SC_SET
    float sy = y + (!all && g_resc.freed > 0 ? 26.0f : 16.0f);
    float sw = ui_text_width_tr(st, 0.66f, 1.4f);
    ui_rrect_c(x - 2, sy - 2, sw + 16, 11, 0, C4(0.01f, 0.015f, 0.025f, 0.55f),
               C4(sc[0], sc[1], sc[2], 0.35f * stp));
    ui_diskn(x + 3.5f, sy + 3.5f, 1.8f, 10, C4(sc[0], sc[1], sc[2], stp));
    ui_text_ex(st, x + 9, sy + 0.5f, 0.66f, TXT_BOLD, 1.4f, C4(sc[0], sc[1], sc[2], 0.95f * stp));
  }
  if (g_match.over) goto debrief;
  // DETECTION ARCS: one per guard building suspicion of ME, on its bearing.
  for (int i = 0; i < g_resc.nguards && i < g_match.nbots; i++) {
    const bot_t *bt = &g_bots[i];
    float d = g_resc.detect[i];
    if (!bt->active || !bt->alive || d < 0.02f) continue;
    if (g_resc.det_who[i] >= 0 && g_resc.det_who[i] != g_local_ent) continue;
    float dx = bt->pos.x - p->pos.x, dz = bt->pos.z - p->pos.z;
    float rel = wrap_pi(atan2f(dx, -dz) - p->yaw);
    float R0 = 58.0f, half = 0.10f + 0.26f * d;
    float cr = 1.0f, cg = d >= 1.0f ? 0.18f : 1.0f - 0.55f * d, cb = d >= 1.0f ? 0.12f : 1.0f - 0.9f * d;
    float al = 0.35f + 0.6f * d;
    if (d >= 1.0f) al *= 0.7f + 0.3f * sinf(t * 14.0f);
    for (int s = 0; s < 6; s++) {
      float a0 = rel - half + 2.0f * half * (float)s / 6.0f, a1 = rel - half + 2.0f * half * (float)(s + 1) / 6.0f;
      ui_line(ccx + sinf(a0) * R0, ccy - cosf(a0) * R0, ccx + sinf(a1) * R0, ccy - cosf(a1) * R0,
              3.2f, 0, 0, 0, 0.35f * al);
      ui_line(ccx + sinf(a0) * R0, ccy - cosf(a0) * R0, ccx + sinf(a1) * R0, ccy - cosf(a1) * R0,
              2.0f, cr, cg, cb, al);
    }
    // ...and the mark over the guard himself, when he is in plain view.
    float sx, sy;
    v3 hp = {{bt->pos.x, bt->pos.y + 2.15f, bt->pos.z}};
    v3 eye = {{p->pos.x, p->pos.y + p->eye, p->pos.z}};
    if (d > 0.22f && rescue_proj(hp, &sx, &sy) && los_clear(eye, hp)) {
      const char *mk = d >= 1.0f ? "!" : "?";
      ui_text_ex(mk, sx - ui_text_width_tr(mk, 1.6f, 1.0f) * 0.5f, sy - 10, 1.6f, TXT_BLACK, 1.0f,
                 d >= 1.0f ? C4(C_THREAT, 1) : C4(1.0f, 0.82f, 0.30f, 0.6f + 0.4f * d));
    }
  }
  // OBJECTIVE MARKERS: the captives, then the LZ once anyone is free.
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    const hostage_t *h = &g_resc.h[i];
    if (h->state != HS_CAPTIVE) continue;
    float sx, sy;
    v3 w = {{h->pos.x, h->pos.y + 1.7f, h->pos.z}};
    float dd = v3_len(v3_sub(h->pos, p->pos));
    if (dd < 3.0f || !rescue_proj(w, &sx, &sy)) continue;
    // QUIET UNTIL LOOKED AT. Three labelled markers at once were a row of
    // text across the middle of the screen. Each is a small diamond and its
    // distance; the NAME appears only on the marker the player is looking
    // toward, and the others sit back at half strength.
    float a = f_clamp((dd - 3.0f) / 4.0f, 0.0f, 1.0f);
    float off = hypotf(sx - ccx, sy - ccy);
    float focus = 1.0f - f_clamp((off - 40.0f) / 50.0f, 0.0f, 1.0f);
    a *= 0.50f + 0.45f * focus;
    rescue_diamond(sx, sy, 4.2f + 1.0f * focus, 1.0f, 0.75f, 0.25f, a);
    if (focus > 0.05f) snprintf(b, sizeof b, "%s  %dM", RESC_HOLD_NAME[i], (int)(dd + 0.5f));
    else               snprintf(b, sizeof b, "%dM", (int)(dd + 0.5f));
    ui_text_sh(b, sx - ui_text_width_tr(b, 0.62f, 1.2f) * 0.5f, sy + 7, 0.62f, TXT_REG, 1.2f,
               C4(1.0f, 0.85f, 0.5f, a));
  }
  if (g_resc.freed > 0) {
    float sx, sy;
    v3 lz = {{22.0f, 4.5f, -45.0f}};
    float dd = v3_len(v3_sub(lz, p->pos));
    if (dd > 6.0f && rescue_proj(lz, &sx, &sy)) {
      rescue_diamond(sx, sy, 7.0f, 0.4f, 1.0f, 0.55f, 0.9f);
      snprintf(b, sizeof b, "LZ %dM", (int)(dd + 0.5f));
      ui_text_ex(b, sx - ui_text_width_tr(b, 0.75f, 1.3f) * 0.5f, sy + 10, 0.75f, TXT_REG, 1.3f,
                 C4(0.5f, 1.0f, 0.6f, 0.9f));
    }
  }
  // THE PROMPT beside a captive.
  if (g_resc.reach_i >= 0 && p->alive) {
    const hostage_t *h = &g_resc.h[g_resc.reach_i];
    char key[16];
    snprintf(key, sizeof key, "%s", g_cfg.bind[IN_RELOAD]);
    for (char *c = key; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
    snprintf(b, sizeof b, "HOLD [%s] CUT RESTRAINTS", key);
    float w = ui_text_width_tr(b, 1.0f, 1.4f);
    ui_rect(ccx - w * 0.5f - 10, ccy + 44, w + 20, 30, 0.02f, 0.03f, 0.04f, 0.5f);
    ui_text_ex(b, ccx - w * 0.5f, ccy + 49, 1.0f, TXT_REG, 1.4f, C4(C_BONE, 1));
    ui_rect(ccx - w * 0.5f, ccy + 65, w, 3, C_BONE, 0.18f);
    ui_rect(ccx - w * 0.5f, ccy + 65, w * h->free_t, 3, C_BLAZE, 1.0f);
  }
  // THE BANNER.
  if (g_resc.msg_t > 0) {
    float a = f_clamp((float)g_resc.msg_t / 40.0f, 0.0f, 1.0f);
    float in = f_clamp((float)(4 * TICK_HZ - g_resc.msg_t) / 12.0f, 0.0f, 1.0f);
    if (g_resc.msg_kind == 0)
      in = f_clamp((float)(RESC_INTRO_TICKS - g_resc.msg_t) / 30.0f, 0.0f, 1.0f);
    a *= in;
    const float *col = g_resc.msg_kind == 2 ? C4(C_THREAT, a) : g_resc.msg_kind == 1 ? C4(C_BLAZE, a)
                     : g_resc.msg_kind == 3 ? C4(0.5f, 1.0f, 0.6f, a) : C4(C_BONE, a);
    // LOWER THIRD, and a size that is a headline rather than a wall. At 22%
    // down the screen in 2.2x type with three lines under it, the briefing sat
    // exactly where the first guard would be. It now lives under the
    // crosshair band, and the stealth tips it used to carry are a game-wide
    // lesson the control hints and the mission itself teach.
    float by = g_ui_vh * 0.62f, s1 = g_resc.msg_kind == 0 ? 1.55f : 1.30f;
    ui_vgrad(0, by - 8, g_ui_vw, 44, C4(0, 0, 0, 0), C4(0, 0, 0, 0.34f * a));
    float w1 = ui_text_width_tr(g_resc.msg, s1, 2.0f);
    ui_text_ex(g_resc.msg, ccx - w1 * 0.5f + 1, by + 1, s1, TXT_BLACK, 2.0f, C4(0, 0, 0, 0.8f * a));
    ui_text_ex(g_resc.msg, ccx - w1 * 0.5f, by, s1, TXT_BLACK, 2.0f, col);
    if (g_resc.msg2[0]) {
      float w2 = ui_text_width_tr(g_resc.msg2, 0.9f, 1.5f);
      ui_text_ex(g_resc.msg2, ccx - w2 * 0.5f, by + (g_resc.msg_kind == 0 ? 22 : 19), 0.9f, TXT_REG, 1.5f,
                 C4(C_BONE, 0.85f * a));
    }
  }
  return;
debrief:
  {
    int ok = g_resc.result == RR_SUCCESS;
    ui_rect(0, 0, g_ui_vw, g_ui_vh, 0.0f, 0.0f, 0.0f, 0.55f);
    float y = g_ui_vh * 0.24f;
    const char *h1 = ok ? "MISSION COMPLETE" : "MISSION FAILED";
    float w1 = ui_text_width_tr(h1, 2.8f, 2.4f);
    ui_text_ex(h1, ccx - w1 * 0.5f + 1, y + 1, 2.8f, TXT_BLACK, 2.4f, C4(0, 0, 0, 0.85f));
    ui_text_ex(h1, ccx - w1 * 0.5f, y, 2.8f, TXT_BLACK, 2.4f,
               ok ? C4(0.5f, 1.0f, 0.6f, 1) : C4(C_THREAT, 1));
    const char *h2 = ok ? "ALL THREE HOSTAGES ARE ON THE HELICOPTER"
                   : g_resc.why == RF_EXECUTED ? "THE HOSTAGES WERE EXECUTED"
                   : g_resc.squad > 1 ? "THE WHOLE SQUAD WAS KILLED IN ACTION"
                   : "YOU WERE KILLED IN ACTION";
    float w2 = ui_text_width_tr(h2, 1.0f, 1.6f);
    ui_text_ex(h2, ccx - w2 * 0.5f, y + 34, 1.0f, TXT_REG, 1.6f, C4(C_BONE, 0.9f));
    ui_rect(ccx - 150, y + 54, 300, 1, C_BONE, 0.3f);
    char tm[16];
    rescue_clock(tm, sizeof tm, g_resc.end_tick);
    struct { const char *k; char v[24]; } rows[6];
    snprintf(rows[0].v, sizeof rows[0].v, "%s", tm);
    snprintf(rows[1].v, sizeof rows[1].v, "%d / %d", g_resc.extracted, RESC_HOSTAGES);
    snprintf(rows[2].v, sizeof rows[2].v, "%d", g_resc.kills);
    snprintf(rows[3].v, sizeof rows[3].v, "%d", g_resc.takedowns);
    snprintf(rows[4].v, sizeof rows[4].v, "%d", g_resc.heads);
    snprintf(rows[5].v, sizeof rows[5].v, "%s", g_resc.alarm ? "RAISED" : "NEVER");
    rows[0].k = "TIME"; rows[1].k = "HOSTAGES SAVED"; rows[2].k = "GUARDS DOWN";
    rows[3].k = "SILENT TAKEDOWNS"; rows[4].k = "HEADSHOTS"; rows[5].k = "ALARM";
    for (int i = 0; i < 6; i++) {
      float ry = y + 64 + 15.0f * (float)i;
      ui_text_ex(rows[i].k, ccx - 140, ry, 0.9f, TXT_REG, 1.4f, C4(C_HAZE, 0.95f));
      ui_text_ex(rows[i].v, ccx + 140 - ui_text_width_tr(rows[i].v, 0.9f, 1.4f), ry, 0.9f, TXT_REG,
                 1.4f, C4(C_BONE, 1));
    }
    if (ok) {
      const char *r = rescue_rating();
      snprintf(b, sizeof b, "RATING  %s", r);
      float w3 = ui_text_width_tr(b, 1.6f, 2.2f);
      ui_text_ex(b, ccx - w3 * 0.5f, y + 164, 1.6f, TXT_BLACK, 2.2f, C4(C_BLAZE, 1));
    }
    const char *h4 = g_online ? "THE NEXT MISSION STARTS SHORTLY"
                   : ok ? "PRESS FIRE TO PLAY AGAIN" : "PRESS FIRE TO RETRY";
    ui_text_ex(h4, ccx - ui_text_width_tr(h4, 0.9f, 1.4f) * 0.5f, y + 192, 0.9f, TXT_REG, 1.4f,
               C4(C_BONE, 0.85f));
  }
}

