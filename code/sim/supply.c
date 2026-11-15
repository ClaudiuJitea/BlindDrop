// code/sim/supply.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// SUPPLY STATIONS (placed by supply_place, world/dressing.c; drawn by
// render/supply_draw). A living fighter — player or bot — who comes within
// reach of a stocked station and is short of what it holds takes it: the
// medical cabinet gives SUPPLY_HEAL health, the ammunition crate restocks every
// grenade the mode allows back to its spawn count. The station is then empty
// for SUPPLY_COOLDOWN. The decision is the authority's alone (match_tick runs
// only offline and on the server); it is announced as EV_PICKUP, which is what
// empties the station on every client and tops up the taker's own predicted
// grenade slot — health arrives in the snapshot like any other damage.
// ---------------------------------------------------------------------------
#define SUPPLY_COOLDOWN (25 * TICK_HZ)
#define SUPPLY_HEAL     50
#define SUPPLY_REACH    1.35f      // from the station's front face to the body's centre

static int *supply_hp(int e) {
  int h = human_of_ent(e);
  return h >= 0 ? &g_players[h].hp : &g_bots[e - 1].hp;
}
// The grenades a station hands out, to their spawn counts: frags into the hand
// or the bag, whichever holds them; smoke and fire only where the mode carries
// them. Returns 1 if anything was added. Shared by the authority and by a
// client topping up its own prediction.
static int supply_give_nades(int e, weapon_t *w) {
  if (!mode_allowed(e, WPN_GR)) return 0;
  int m = game_mode(), frag_only = m == GM_GUN || m == GM_BLADES;
  static const int CAP[NADE_KINDS] = {[NADE_FRAG] = 3, [NADE_SMOKE] = SMOKE_CARRY, [NADE_INC] = INC_CARRY};
  int gave = 0;
  for (int k = 0; k < NADE_KINDS; k++) {
    if (frag_only && k != NADE_FRAG) continue;
    int cap = k == NADE_FRAG ? WPN_DEF[WPN_GR].mag : CAP[k];
    int *slot = k == w->gr_kind ? &w->ammo[WPN_GR] : &w->gr_bag[k];
    if (*slot < cap) { *slot = cap; gave = 1; }
  }
  return gave;
}
static int supply_wants(int e, int kind) {
  if (kind == SUP_HEALTH) return *supply_hp(e) < PLAYER_HP;
  weapon_t *w = ent_weapon(e);
  if (!mode_allowed(e, WPN_GR)) return 0;
  int m = game_mode(), frag_only = m == GM_GUN || m == GM_BLADES;
  for (int k = 0; k < NADE_KINDS; k++) {
    if (frag_only && k != NADE_FRAG) continue;
    int cap = k == NADE_FRAG ? WPN_DEF[WPN_GR].mag : k == NADE_SMOKE ? SMOKE_CARRY : INC_CARRY;
    if (nade_count(w, k) < cap) return 1;
  }
  return 0;
}
// The point a fighter reaches for: the middle of the station's front face.
static v3 supply_front(const supply_t *s) {
  static const float FX[4] = {1, 0, -1, 0}, FZ[4] = {0, 1, 0, -1};
  float d = s->kind == SUP_HEALTH ? 0.30f : 0.36f;
  return (v3){{s->x + FX[s->qt & 3] * d, 0.0f, s->z + FZ[s->qt & 3] * d}};
}
static void supply_tick(void) {
  if (g_home) return;
  for (int i = 0; i < g_nsupply; i++) {
    if (g_tick < g_supply_ready[i]) continue;
    const supply_t *s = &g_supply[i];
    v3 f = supply_front(s);
    ENT_FOREACH(e) {
      if (!ent_alive(e)) continue;
      v3 p = ent_pos(e);
      if (fabsf(p.y - f.y) > 1.3f || hypotf(p.x - f.x, p.z - f.z) > SUPPLY_REACH) continue;
      if (!supply_wants(e, s->kind)) continue;
      if (s->kind == SUP_HEALTH) {
        int *hp = supply_hp(e);
        *hp = *hp + SUPPLY_HEAL > PLAYER_HP ? PLAYER_HP : *hp + SUPPLY_HEAL;
      } else {
        supply_give_nades(e, ent_weapon(e));
      }
      g_supply_ready[i] = g_tick + SUPPLY_COOLDOWN;
      ev_push((event_t){.type = EV_PICKUP, .a = (v3){{s->x, 0.0f, s->z}}, .k = i, .i = s->kind,
                        .src = e, .dst = e});
      break;
    }
  }
}
// A client's half of EV_PICKUP: empty the station, and top up our own slot.
// Offline (and on a listen server) the authority has already done both; doing
// them again here is a no-op.
static long g_supply_msg_t = -100000;   // when the local player last took one
static int  g_supply_msg_kind;
static void supply_on_event(const event_t *e) {
  if (e->k >= 0 && e->k < g_nsupply) {
    long until = g_tick + SUPPLY_COOLDOWN;
    if (g_supply_ready[e->k] < until - TICK_HZ) g_supply_ready[e->k] = until;
  }
  if (e->src >= 0 && e->src == g_local_ent) {
    if (e->i == SUP_NADES) supply_give_nades(e->src, ent_weapon(e->src));
    g_supply_msg_t = g_tick;
    g_supply_msg_kind = e->i;
  }
}
