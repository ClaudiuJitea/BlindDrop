// code/sim/modes.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// GAME MODES — the rules, in one place. Every reader below runs on the server,
// offline AND in a client's prediction, from the same replicated inputs
// (g_match.mode and g_match.mscore ride NP_SCORES), so the predicted weapon
// gate and the server's agree; where they briefly do not (a promotion the 4 Hz
// packet has not delivered yet), the snapshot's weapon truth wins as it always
// has.
// ---------------------------------------------------------------------------

// The boot screen is a deathmatch backdrop whatever the MATCH tab says.
static int game_mode(void) {
  if (g_home) return GM_DM;
  int m = g_match.mode;
  return m >= 0 && m < GM_COUNT ? m : GM_DM;
}

// ---- TEAMS (TEAM DEATHMATCH) ------------------------------------------------
// One side per entity, per lobby, OUTSIDE match_t for the same reason the
// human roster is (match_restart memsets it; a side survives a rematch). The
// server/offline sim owns it: humans pick theirs (NP_TEAM online, the TEAM page
// offline), bots are dealt to keep each side at the chosen size. A client
// adopts every entity's side from NP_SCORES. The team score is its own counter
// rather than a sum of kills: bots leave as humans join, and a departing bot
// must not take its side's frags with it.
static signed char g_team_all[SRV_LOBBIES][MAX_ENTS];
#define g_team (g_team_all[g_lobby])
static int g_team_score_all[SRV_LOBBIES][2];
#define g_team_score (g_team_score_all[g_lobby])
static int g_net_team_size;   // client: the server's team size (NP_SCORES)
static int g_net_team_fill;   // client: ...and whether bots fill it
static int teams_on(void) { return game_mode() == GM_TDM; }
static int team_of(int e) { return e >= 0 && e < MAX_ENTS ? g_team[e] : -1; }
// Friend, not foe: two DIFFERENT entities on one side of a team game.
// HOSTAGE RESCUE has two sides as well, the squad and the guards, and they are
// fixed by who an entity is (resc_side), not by a choice.
static int resc_side(int e);
static int resc_is_squad(int e);
static int team_mates(int a, int b) {
  if (a == b || a < 0 || b < 0 || a >= MAX_ENTS || b >= MAX_ENTS) return 0;
  if (game_mode() == GM_RESCUE) return resc_side(a) == resc_side(b);
  return teams_on() && g_team[a] == g_team[b];
}
static int team_size(void) {
  int n = g_online && g_net_team_size ? g_net_team_size : g_cfg.sp_team_size;
  return n < 1 ? 1 : n > TEAM_SIZE_MAX ? TEAM_SIZE_MAX : n;
}
static int team_fill(void) { return g_online ? g_net_team_fill : g_cfg.sp_team_fill; }
static int g_net_team_ff;     // client: the server's friendly-fire rule
static int team_ff(void) { return g_online ? g_net_team_ff : g_cfg.sp_team_ff; }
static int team_humans(int t) {
  int n = 0;
  ENT_FOREACH(e) if (human_of_ent(e) >= 0 && g_team[e] == t) n++;
  return n;
}
// Bots a side needs to reach its size: the humans on it take their places.
static int tdm_bot_want(int t) {
  if (!g_cfg.sp_team_fill) return 0;
  int w = team_size() - team_humans(t);
  return w < 0 ? 0 : w;
}
// The side with more free human places (ALPHA on a tie): an auto-pick.
static int team_auto(void) {
  int fa = team_size() - team_humans(0), fb = team_size() - team_humans(1);
  return fb > fa ? 1 : 0;
}
// Did the local player's side (or the local player) take the match?
static void tdm_deal_bots(int respawn);   // with the bot roster, below
static int rescue_won(void);
static int won_by_me(int winner) {
  if (game_mode() == GM_RESCUE) return rescue_won();   // the squad wins or loses together
  if (winner >= MAX_ENTS) return winner - MAX_ENTS == team_of(g_local_ent);
  return winner == g_local_ent;
}

// mscore in GUN GAME is POINTS (kills on the ladder); the rung is derived.
static int gg_level(int ent) {
  int l = ent >= 0 && ent < MAX_ENTS ? g_match.mscore[ent] / GG_RUNG_KILLS : 0;
  return l < 0 ? 0 : l >= GG_LEVELS ? GG_LEVELS - 1 : l;
}
static int gg_progress(int ent) {   // kills already made on the current rung
  return gg_level(ent) == GG_LEVELS - 1 ? 0 : g_match.mscore[ent] % GG_RUNG_KILLS;
}

// May `ent` hold weapon `wpn` in this mode?
static int mode_allowed(int ent, int wpn) {
  switch (game_mode()) {
    case GM_GUN:    return wpn == GG_LADDER[gg_level(ent)];
    case GM_SNIPER: return wpn == WPN_SR;
    case GM_BLADES: return wpn == WPN_KN || wpn == WPN_GR;
    // The operator carries the suppressed sniper, a sidearm and the knife.
    case GM_RESCUE: return !resc_is_squad(ent) || wpn == WPN_SR || wpn == WPN_PT || wpn == WPN_KN;
    default:        return 1;
  }
}

// The weapon a spawn starts on, and what a disallowed weapon is swapped for.
static int mode_primary(int ent) {
  switch (game_mode()) {
    case GM_GUN:    return GG_LADDER[gg_level(ent)];
    case GM_SNIPER: return WPN_SR;
    case GM_BLADES: return WPN_KN;
    case GM_RESCUE: return resc_is_squad(ent) ? WPN_SR : WPN_AR;
    default:        return WPN_AR;
  }
}

// LAST MAN STANDING's rounds to win: the server's on a client, ours otherwise.
static int g_net_rounds;
static int g_cl_team_asked;   // this connection has already offered the TEAM page
static int g_ui_team_prompt;  // ...and the menu should open on it at the next frame
static int mode_round_goal(void) { return g_online && g_net_rounds ? g_net_rounds : g_cfg.sp_rounds; }

// LAST MAN STANDING: the dead stay down until the round resets everyone.
static int mode_respawns(void) { return game_mode() != GM_LMS && game_mode() != GM_RESCUE; }

// Grenade stock that is not the spawn's fixed two. The GUN GAME grenade rung is
// bottomless (one kill to climb, and running dry would strand you on a weapon
// you may not hold); KNIVES + NADES grows one back every NADE_REGEN_TICKS.
// Runs in the shared weapon paths, so a client's prediction counts the same.
#define NADE_REGEN_TICKS (5 * TICK_HZ)
static void mode_nade_refill(int ent, weapon_t *w) {
  int m = game_mode();
  if (m == GM_GUN && GG_LADDER[gg_level(ent)] == WPN_GR) {
    if (w->ammo[WPN_GR] < 1) w->ammo[WPN_GR] = 1;
    w->gr_regen = 0;
  } else if (m == GM_BLADES && w->ammo[WPN_GR] < WPN_DEF[WPN_GR].mag) {
    if (++w->gr_regen >= NADE_REGEN_TICKS) { w->gr_regen = 0; w->ammo[WPN_GR]++; }
  } else {
    w->gr_regen = 0;
  }
}

// A fresh life's weapon: the mode's primary, with no raise to wait through.
static void mode_loadout(int ent, weapon_t *w) {
  if (!mode_allowed(ent, w->cur)) w->cur = mode_primary(ent);
  mode_nade_refill(ent, w);
}


