// code/sim/drops.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// DROPPED GUNS. A rifle, a sniper or a pistol can leave a hand and lie on the
// floor for anyone to take: thrown with DROP / PICK UP (IN_USE), let go by
// whoever dies holding it, or put down in exchange for the one you take. A gun
// is taken into ITS OWN SLOT — an AWP for your sniper, an AK-47 for your rifle —
// so the set of things you carry never changes, only which of each it is.
//
//   * IN_USE with a gun under the crosshair and in reach (drop_target) takes
//     it; the one you had in that slot goes down where it lay. With nothing
//     under the crosshair, IN_USE throws the gun in your hands.
//   * Walking over a gun whose slot you have emptied takes it without a press
//     (the thrower's own gun waits DROP_LOCK first, or it would be caught on
//     the bounce).
//   * A gun lies DROP_LIE, then dissolves the way a body does (DROP_DISS).
//
// The authority (offline, or the server) decides every drop and every take and
// announces them as EV_WDROP / EV_WTAKE; a client runs its own copy of the
// flight on the same physics for the eye, exactly as it does a grenade's, and
// learns which slots a shooter has emptied from the entity record (weapon_t.gone
// rides above the loadout). Nothing here is predicted: a drop is not a shot,
// and one round trip before the gun leaves the hand is not felt.
// ---------------------------------------------------------------------------
#define MAX_DROPS   16
#define DROP_LIE    (30 * TICK_HZ)   // how long a gun lies before it goes
#define DROP_DISS   (TICK_HZ / 2)    // ...and how long it takes to come apart
#define DROP_R      0.045f           // collision radius: what a gun on its side is thick
#define DROP_REACH  2.6f             // eye to gun, for a take under the crosshair: ~2 m
                                     // along the floor from a standing eye, past where
                                     // a gun you threw yourself lands
#define DROP_AIM    0.42f            // m the gun may sit off the aim ray and still be "that one"
#define DROP_WALK   0.80f            // feet to gun: walking over it takes it into an empty slot
#define DROP_LOCK   (TICK_HZ * 3 / 2)// the thrower's own gun is not walked back up for 1.5 s
#define DROP_THROW  4.4f             // m/s off the hand along the view
#define DROP_AMMO_MAX 2047           // EV_W*'s i is ammo * 16 + index in an i16

typedef struct {
  v3    pos, prev_pos, vel;
  float yaw;                 // the muzzle's heading on the floor
  float roll, prev_roll, roll_v;   // tumble about the bore; settles onto a side
  int   slot, var, ammo, owner;
  int   life;                // ticks left, DROP_DISS of them dissolving; 0 = free
  int   age, rest, floor;
  uint32_t seed;             // its own dissolve pattern
} drop_t;
static drop_t g_drops_all[SRV_LOBBIES][MAX_DROPS];
#define g_drops (g_drops_all[g_lobby])

static int drop_takeable(const drop_t *d) { return d->life > DROP_DISS; }
static void drops_reset(void) { memset(g_drops, 0, sizeof g_drops); }

// Index i takes a gun leaving a hand at `at` with `vel`. The authority picks i
// (drop_slot_new); a client is told it, so both sides name the same gun.
static void drop_add(int i, v3 at, v3 vel, int slot, int var, int ammo, int owner) {
  if (i < 0 || i >= MAX_DROPS || slot < 0 || slot >= WPN_COUNT || !(DROP_SLOTS >> slot & 1)) return;
  if (var < 0 || var >= wpn_var_n(slot)) var = 0;
  float hz = hypotf(vel.x, vel.z);
  g_drops[i] = (drop_t){.pos = at, .prev_pos = at, .vel = vel,
                        .yaw = hz > 0.05f ? atan2f(vel.x, -vel.z) : 0.0f,
                        .roll_v = (i & 1) ? 0.105f : -0.095f,
                        .slot = slot, .var = var, .ammo = ammo, .owner = owner,
                        .life = DROP_LIE + DROP_DISS,
                        .seed = 0x9E3779B9u * (uint32_t)(i + 1) ^ (uint32_t)lrintf(at.x * 977.0f + at.z * 131.0f)};
}
// A free index, else the gun closest to going anyway.
static int drop_slot_new(void) {
  int best = 0, low = 1 << 30;
  for (int i = 0; i < MAX_DROPS; i++) {
    if (g_drops[i].life <= 0) return i;
    if (g_drops[i].life < low) { low = g_drops[i].life; best = i; }
  }
  return best;
}

// THE AUTHORITY'S DROP: spawned here and announced.
static void drop_spawn(v3 at, v3 vel, int slot, int var, int ammo, int owner) {
  if (g_predicting) return;
  int i = drop_slot_new();
  ammo = ammo < 0 ? 0 : ammo > DROP_AMMO_MAX ? DROP_AMMO_MAX : ammo;
  drop_add(i, at, vel, slot, var, ammo, owner);
  ev_push((event_t){.type = EV_WDROP, .k = 0, .a = at, .b = vel, .wpn = slot,
                    .i = ammo * 16 + i, .g = (float)var / 32.0f, .src = owner, .dst = -1});
}

static void drop_cancel_reload(int ent, weapon_t *w) {
  if (w->reload_t <= 0) return;
  ev_push((event_t){.type = EV_RELOAD_CANCEL, .src = ent, .a = ent_pos(ent), .wpn = w->cur});
  w->reload_t = 0;
}

// Let go of the gun in `slot`: it goes down with what was left in its magazine
// and the slot is empty until something is taken into it.
static void drop_release(int ent, weapon_t *w, int slot, v3 at, v3 vel) {
  if (w->cur == slot) {
    drop_cancel_reload(ent, w);
    w->burst_n = 0;
    w->cock_t = 0;
  }
  drop_spawn(at, vel, slot, w->var[slot], w->ammo[slot], ent);
  w->gone |= 1 << slot;
  w->ammo[slot] = 0;
  w->alt[slot] = 0;
  w->fresh = 0;          // the loadout is no longer the menu's to retake
}

// Put a taken gun into `w`. Shared: the authority's take and a client's copy of
// its own (net_cl_on_take). `hands` raises it at once.
static void drop_apply(weapon_t *w, int slot, int var, int ammo, int hands) {
  if (slot < 0 || slot >= WPN_COUNT || !(DROP_SLOTS >> slot & 1)) return;
  if (var < 0 || var >= wpn_var_n(slot)) var = 0;
  w->var[slot] = var;
  int cap = wdef(w, slot)->mag;
  w->ammo[slot] = ammo < 0 ? 0 : ammo > cap ? cap : ammo;
  w->gone &= ~(1 << slot);
  w->alt[slot] = 0;
  w->cool[slot] = 0;
  w->fresh = 0;
  if (hands) {            // a gun new to the hands comes up, even into the slot they held
    w->cur = slot;
    w->switch_t = GUN_SWITCH_TICKS;
    w->reload_t = 0;
    w->melee_t = 0;
    w->dry_done = 0;
    w->idx = 0;
  }
}

// THE AUTHORITY'S TAKE of drop i by `ent`. If that slot is full, the gun in it
// goes down where this one lay — the take is announced FIRST, so a client that
// sees the swap reuse index i frees the old gun before it builds the new one.
static void drop_take(int ent, weapon_t *w, int i, int hands, float yaw) {
  drop_t d = g_drops[i];
  g_drops[i].life = 0;
  if (hands && (w->gr_cook > 0 || w->gr_t > 0)) hands = 0;   // a live grenade stays in hand
  if (hands) drop_cancel_reload(ent, w);
  ev_push((event_t){.type = EV_WTAKE, .a = d.pos, .wpn = d.slot, .i = d.ammo * 16 + i,
                    .g = (float)d.var / 32.0f, .f = hands ? 1.0f : 0.0f, .src = ent, .dst = -1});
  if (!wpn_gone(w, d.slot)) {
    v3 hop = {{sinf(yaw) * 0.6f, 1.6f, -cosf(yaw) * 0.6f}};
    drop_release(ent, w, d.slot, v3_add(d.pos, (v3){{0, 0.10f, 0}}), hop);
  }
  drop_apply(w, d.slot, d.var, d.ammo, hands);
}

// The gun under `ent`'s crosshair and in reach, or -1. Pure: the authority asks
// it on the press, a client every frame for the HUD's prompt.
static int drop_target(int ent, v3 eye, float yaw, float pitch) {
  v3 dir = {{cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw)}};
  int best = -1;
  float bs = 1e9f;
  for (int i = 0; i < MAX_DROPS; i++) {
    const drop_t *d = &g_drops[i];
    if (!drop_takeable(d) || !mode_rule_allows(ent, d->slot)) continue;
    v3 to = v3_sub(d->pos, eye);
    float dist = v3_len(to), t = v3_dot(to, dir);
    if (dist > DROP_REACH || t < 0.05f) continue;
    float off = v3_len(v3_sub(to, v3_scale(dir, t)));
    // The allowance widens close in, where a gun at your feet is far off the
    // ray in metres for a few degrees of look.
    if (off > DROP_AIM + 0.12f * (DROP_REACH - dist)) continue;
    float sc = off + 0.15f * dist;
    if (sc >= bs || !los_clear(eye, v3_add(d->pos, (v3){{0, 0.06f, 0}}))) continue;
    bs = sc;
    best = i;
  }
  return best;
}

// IN_USE pressed (weapon_tick, the authority only): take what is under the
// crosshair, or throw what is in the hands.
static void drop_use(player_t *p, int ent) {
  if (!mode_drops() || g_predicting || !p->alive || g_match.over) return;
  weapon_t *w = &p->wp;
  if (w->gr_cook > 0 || w->gr_t > 0) return;
  v3 eye = player_eye_pos(p);
  int t = drop_target(ent, eye, p->yaw, p->pitch);
  if (t >= 0) { drop_take(ent, w, t, 1, p->yaw); return; }
  int s = w->cur;
  if (!(DROP_SLOTS >> s & 1) || !mode_allowed(ent, s)) return;
  v3 dir = {{cosf(p->pitch) * sinf(p->yaw), sinf(p->pitch), -cosf(p->pitch) * cosf(p->yaw)}};
  v3 rt = {{cosf(p->yaw), 0, sinf(p->yaw)}};
  v3 at = v3_add(eye, v3_add(v3_scale(rt, 0.12f), v3_add(v3_scale(dir, 0.40f), (v3){{0, -0.14f, 0}})));
  v3 wn;
  int ws;
  if (trace_world(eye, dir, &wn, &ws) < 0.6f) at = eye;   // up against a wall: let go at the eye
  v3 vel = v3_add(v3_scale(p->vel, 0.6f), v3_add(v3_scale(dir, DROP_THROW), (v3){{0, 1.3f, 0}}));
  drop_release(ent, w, s, at, vel);
}

// A DEATH LETS GO OF THE GUN IN THE HANDS — or, holding the knife or a grenade,
// the first gun still carried. Run from match_on_kill (the authority's).
static void drop_on_death(int ent) {
  if (!mode_drops() || g_predicting || ent < 0 || ent >= MAX_ENTS) return;
  weapon_t *w = ent_weapon(ent);
  if (!w) return;
  int s = w->cur;
  if (!(DROP_SLOTS >> s & 1) || !mode_allowed(ent, s)) {
    static const int ORDER[3] = {WPN_AR, WPN_SR, WPN_PT};
    s = -1;
    for (int k = 0; k < 3 && s < 0; k++)
      if (mode_allowed(ent, ORDER[k])) s = ORDER[k];
  }
  if (s < 0) return;
  int h = human_of_ent(ent);
  v3 v = h >= 0 ? g_players[h].vel : g_bots[ent - 1].vel;
  float yaw = ent_yaw(ent);
  v3 at = v3_add(ent_pos(ent), (v3){{0, 1.05f, 0}});
  v3 vel = v3_add(v3_scale(v, 0.5f), (v3){{sinf(yaw) * 1.1f, 1.4f, -cosf(yaw) * 1.1f}});
  drop_release(ent, w, s, at, vel);
}

// One tick of every gun on the floor, the grenade's contact model with a flatter
// bounce. `auth` is 0 on a client's copy: same flight, nobody picks anything up.
static void drop_tick(int auth) {
  for (int i = 0; i < MAX_DROPS; i++) {
    drop_t *d = &g_drops[i];
    if (d->life <= 0) continue;
    d->prev_pos = d->pos;
    d->prev_roll = d->roll;
    d->age++;
    if (--d->life <= 0) { d->life = 0; continue; }
    // Down on the floor the tumble settles onto the nearer side.
    if (d->floor) {
      float side = floorf(d->roll / F_PI) * F_PI + 0.5f * F_PI;
      d->roll += (side - d->roll) * 0.16f;
      d->roll_v *= 0.8f;
    }
    if (d->rest) continue;
    d->vel.y -= NADE_GRAVITY * TICK_DT;
    d->vel = v3_scale(d->vel, 1.0f - NADE_DRAG * 2.0f);
    d->roll += d->roll_v;
    float sp = v3_len(d->vel), step = sp * TICK_DT;
    if (step < 1e-5f) continue;
    v3 dn = v3_scale(d->vel, 1.0f / sp), wn = {{0, 1, 0}};
    int ws = -1;
    float hit = trace_world(d->pos, dn, &wn, &ws);
    if (hit >= step + DROP_R) {
      d->pos = v3_add(d->pos, v3_scale(d->vel, TICK_DT));
    } else {
      d->pos = v3_add(d->pos, v3_scale(dn, fmaxf(hit - DROP_R, 0.0f)));
      float vn = v3_dot(d->vel, wn);
      v3 vt = v3_sub(d->vel, v3_scale(wn, vn));
      if (-vn > 1.4f && auth)
        ev_push((event_t){.type = EV_WDROP, .k = 1, .a = d->pos, .f = -vn, .wpn = d->slot,
                          .src = d->owner, .dst = -1});
      d->vel = v3_add(v3_scale(vt, 0.45f), v3_scale(wn, -vn * 0.22f));
      if (wn.y > 0.7f) {
        d->floor = 1;
        if (fabsf(vn) < 1.2f) {               // on a floor and not bouncing: sliding to a stop
          d->vel.y = 0.0f;
          d->vel = v3_scale(d->vel, 0.90f);
          if (v3_len(d->vel) < 0.10f) { d->rest = 1; d->vel = (v3){{0, 0, 0}}; }
        }
      }
    }
    // The arena floor is a plane, not a solid (see nade_tick).
    if (d->pos.y < DROP_R) {
      d->pos.y = DROP_R;
      d->floor = 1;
      if (d->vel.y < 0.0f) {
        float vn = -d->vel.y;
        if (vn > 1.4f && auth)
          ev_push((event_t){.type = EV_WDROP, .k = 1, .a = d->pos, .f = vn, .wpn = d->slot,
                            .src = d->owner, .dst = -1});
        d->vel.x *= 0.45f;
        d->vel.z *= 0.45f;
        d->vel.y = vn * 0.22f;
        if (vn < 1.2f) d->vel.y = 0.0f;
      }
      if (d->vel.y == 0.0f) {
        d->vel = v3_scale(d->vel, 0.90f);
        if (v3_len(d->vel) < 0.10f) { d->rest = 1; d->vel = (v3){{0, 0, 0}}; }
      }
    }
  }
  if (!auth || !mode_drops() || g_match.over) return;
  // WALKED OVER: a gun whose slot you have emptied comes up by itself. Straight
  // into the hands only when the knife is all they hold.
  for (int i = 0; i < MAX_DROPS; i++) {
    const drop_t *d = &g_drops[i];
    if (!drop_takeable(d)) continue;
    ENT_FOREACH(e) {
      if (human_of_ent(e) < 0 || !ent_alive(e)) continue;
      if (d->owner == e && d->age < DROP_LOCK) continue;
      weapon_t *w = ent_weapon(e);
      if (!w || !wpn_gone(w, d->slot) || !mode_rule_allows(e, d->slot)) continue;
      v3 p = ent_pos(e);
      if (d->pos.y < p.y - 0.4f || d->pos.y > p.y + 1.0f ||
          hypotf(d->pos.x - p.x, d->pos.z - p.z) > DROP_WALK) continue;
      drop_take(e, w, i, w->cur == WPN_KN && w->melee_t == 0, ent_yaw(e));
      break;
    }
  }
}

// A CLIENT'S HALF of EV_WDROP / EV_WTAKE: build or free its copy of the gun, and
// put a gun it took into its own predicted weapon. Offline the authority has
// already done all of it.
static void net_cl_on_take(int slot, int var, int ammo, int hands);   // net/conn.c
static long g_drop_msg_t = -100000;   // when the local player last took one
static int  g_drop_msg_slot, g_drop_msg_var;
static void drop_on_event(const event_t *e) {
  if (e->type != EV_WDROP && e->type != EV_WTAKE) return;
  if (e->type == EV_WDROP && e->k) return;   // a bounce: audio only
  int i = e->i & 15, ammo = e->i >> 4, var = (int)lrintf(e->g * 32.0f);
  if (e->type == EV_WDROP) {
    if (g_online) drop_add(i, e->a, e->b, e->wpn, var, ammo, e->src);
    return;
  }
  if (g_online) g_drops[i].life = 0;
  if (e->src >= 0 && e->src == g_local_ent) {
    if (g_online) net_cl_on_take(e->wpn, var, ammo, e->f > 0.5f);
    g_drop_msg_t = g_tick;
    g_drop_msg_slot = e->wpn;
    g_drop_msg_var = var < 0 || var >= wpn_var_n(e->wpn) ? 0 : var;
  }
}
