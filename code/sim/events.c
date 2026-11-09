// code/sim/events.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Events — the sim's one-way channel to presentation. player_tick/bot_tick
// push; consumers pull: the harness prints a line per event at push time (the
// scripted-test contract), VFX spawn at push time (pure data), the shared
// `audio_frame_drain` empties the buffer once per frame for audio on every
// platform (the dedicated server drains it per TICK instead), then clears it.
// Events must survive all ticks of one frame, hence the frame-scope buffer.
// ---------------------------------------------------------------------------

// Deathmatch roster: short, all-caps (font glyph set), <=7 chars for tight
// scoreboard/killfeed columns.
static const char *BOT_NAMES[MAX_BOTS] = {
  "VIPER", "ROOK",  "HAVOC", "NOVA",  "ONYX",  "SABLE", "PIVOT", "FLUX",
  "RAZOR", "GHOST", "BOLT",  "CRUX",  "DRIFT", "EMBER", "FANG",  "IRIS",
  "JOLT",  "KILO",  "LYNX",  "MACE",
};

// Three-range dispatch (P1-8). The old `(e - 1) % MAX_BOTS` modulo LOOKED like
// safety and was a lie: it turned every high human id into a WRONG bot name in
// the killfeed, the banners and every proof line — no command flags a wrong
// name. Negative e (server's g_local_ent, `bot`'s "no target") is explicit.
static const char *ent_name(int e) {
  if (e < 0) return "none";
  if (e >= MAX_ENTS) return e - MAX_ENTS < 2 ? TEAM_NAMES[e - MAX_ENTS] : "none";  // a team won
  if (e == 0) return g_cfg.name;
  if (e <= MAX_BOTS) return BOT_NAMES[e - 1];
  int h = e - MAX_BOTS;
  return h < MAX_HUMANS && g_human_name[h][0] ? g_human_name[h] : "REMOTE";
}

typedef enum {
  EV_FIRE, EV_HIT, EV_HIT_HEAD, EV_KILL, EV_IMPACT, EV_DRYFIRE, EV_RELOAD,
  EV_RELOAD_CANCEL,  // a swap abandoned a running reload; wpn = the weapon
  EV_RESPAWN, EV_END, EV_STEP, EV_LAND,
  EV_MELEE, EV_BLOCK, EV_NADE, EV_BLAST
} ev_type_t;

// AI hearing is simulation state, deliberately separate from g_events: events
// are drained by presentation and can never be an authoritative AI input.
typedef enum { BOT_NOISE_SHOT, BOT_NOISE_STEP, BOT_NOISE_COUNT } bot_noise_kind_t;
typedef struct {
  v3 pos;
  long tick;
  float radius;
  int src;
} bot_noise_t;

typedef struct bot_t bot_t;
static void bot_noise_emit(int src, bot_noise_kind_t kind, v3 pos, float radius);
static float bot_step_noise_radius(float speed, float crouch);
static void bot_hear_update(bot_t *b, int bi);
static float bot_eye(const bot_t *b);
static int los_clear(v3 o, v3 tp);
// Forward, not moved: bot_focus_invalidate is the chokepoint for "the target
// identity is changing" and has to release a committed flank, but the flank leg
// needs the cover/search helpers that are defined between the two.
static void bot_flank_release(bot_t *b);

// EV_FIRE:   a = impact/end point, b = shot dir, f = hit distance, i = shot
//            index, k = 0 none / 1 solid / 2 body / 3 head,
//            g = ads blend (the tracer's muzzle offset collapses onto the
//            view axis while aiming).
// EV_HIT*:   a = impact point, b = shooter origin, i = damage, k = hp left.
// EV_IMPACT: a = impact point, b = surface normal, i = solid index (-1 = the
//            arena floor plane; the face the mark has to be clipped to),
//            f = the range the round travelled (the burst's angular size).
// EV_KILL:   k = headshot flag.  EV_END: k = winner entity.
// EV_STEP:   a = the foot that just landed, f = speed, g = crouch blend.
// EV_LAND:   a = where the body touched down, f = the IMPACT SPEED (m/s down).
// EV_MELEE:  a = the blade's origin. k = 0 a swing starting, 1 steel on a wall.
// EV_BLOCK:  a = contact point, src = attacker, dst = the guard, k = 1 a parry.
// EV_NADE:   k = 0 thrown: a = position, b = velocity, i = fuse ticks left.
//            k = 1 a bounce: a = where, f = impact speed.
//            k = 2 the spoon flew: a = hand, b = its velocity. k = 3 taken in hand.
// EV_BLAST:  a = the detonation, src = thrower, g = 1 on open ground.
// src/dst:   attacker/victim entity id (0 = player, 1.. = bots).
// EV_FIRE also carries `pen`: metres of MATERIAL the round crossed on the way,
// 0 for an ordinary shot. It is the only field that distinguishes a wallbang
// from a clean hit, and the harness's `bang` line prints it.
typedef struct {
  ev_type_t type; v3 a, b; float f, g, pen; int i, k, wpn, src, dst;
} event_t;
static int hit_on_plate(const event_t *e);   // presentation: a chest-band hit

// 128 against a measured ev_peak of ~10 at 21 entities (20 bots hard); the
// MAX_ENTS growth to 28 entities moves that peak proportionally at most —
// re-measured via `budget` (P0-7), still an order of magnitude of headroom.
#define MAX_EVENTS 128
static event_t g_events_all[SRV_LOBBIES][MAX_EVENTS];
#define g_events (g_events_all[g_lobby])
static int     g_num_events_all[SRV_LOBBIES];
#define g_num_events (g_num_events_all[g_lobby])
static long    g_tick_all[SRV_LOBBIES];  // sim tick, advanced in player_tick
#define g_tick (g_tick_all[g_lobby])
static int     g_score_held;  // scoreboard key level state, sim -> HUD

static void vfx_on_event(const event_t *e);  // defined with the renderer
static void vfx_tick(void);                  // ditto — called every sim tick
static void vfx_match_reset(void);           // clears killfeed etc. on restart

// Harness mode. The Linux binary is BOTH the game and the test harness (`--do`
// / `--script` / `--print-config` select the harness, no arguments launch the
// game), so what used to be a compile-time property of the target is now a
// runtime one. Everything the harness prints has to be gated on it: the event
// lines below ARE the test contract, and a player would get them spat into the
// terminal on every shot. Always 1 on the Linux harness path, always 0 in a
// window; the Windows target never compiles the printer at all.
#ifndef _WIN32
static int g_headless;
#endif

// A DROPPED EVENT IS A MISSING SOUND: VFX (hitmarkers included) spawn at push
// time outside the ring, so what a drop actually loses is the frame drain's
// AUDIO for that event. The drop is counted and `budget` prints it. The ring
// was silently lossy and the pressure on it just went up: a
// penetrating shot pushes an extra EV_IMPACT for its entry hole, so a 20-bot
// firefight's worst tick carries more than it used to. Measured rather than
// assumed — see the `budget` line, and read it as a PER-TICK peak: the harness
// clears the ring every tick inside sim_advance while both windowed backends
// drain it once per FRAME, which at a hitch-clamped 30 fps is four ticks. The
// measured per-tick worst is 8-10 of 128, so even four of them together leave
// the ceiling three times clear; `appframe` cannot show the frame figure
// directly because it drives app_frame one tick at a time.
static long g_ev_drops_all[SRV_LOBBIES];
#define g_ev_drops (g_ev_drops_all[g_lobby])
static int  g_ev_peak_all[SRV_LOBBIES];
#define g_ev_peak (g_ev_peak_all[g_lobby])

// While the client PREDICTS/REPLAYS its own motion, events, VFX, audio
// and damage must not fire — they belong to the authoritative server pass.
// One gate, so weapon_tick can be predicted whole rather than forked into a
// parallel cosmetic path. 0 everywhere except inside a client replay.
static int g_predicting;

#ifndef _WIN32
// `spawncheck`: per-entity tick of its last respawn, and how many deaths came
// within SPAWNCHECK_QUICK of one. Fed by the harness event printer below.
// The 1 s window is the one that measures PLACEMENT: deaths later than that
// track the arena's ordinary death rate (a 9-body arena's average life is
// ~10 s, so ~30% of all deaths land within 3 s of any reference point).
#define SPAWNCHECK_QUICK 120     // 1 s
#define SPAWNCHECK_SLOW  360     // 3 s, printed for context
static long g_sc_spawn[MAX_ENTS];
static int  g_sc_deaths, g_sc_quick, g_sc_slow;
#endif
static void ev_push(event_t e) {
  if (g_predicting) return;   // a replayed shot must not re-spawn its VFX/audio
  if (g_num_events < MAX_EVENTS) {
    g_events[g_num_events++] = e;
    if (g_num_events > g_ev_peak) g_ev_peak = g_num_events;
  } else {
    g_ev_drops++;
  }
  vfx_on_event(&e);
#ifndef _WIN32
  if (!g_headless) return;
  static const char *KIND[4] = {"none", "solid", "bot", "head"};
  switch (e.type) {
  case EV_FIRE:
    if (e.src == g_local_ent) {  // player shots only: bot fire would flood every trace
      // `pen` is the PATH LENGTH the round spent inside material, and it is the
      // only way a script can tell a wallbang from an ordinary hit — the two are
      // identical in every other field. Printed only when it happened, so every
      // existing trace artefact is byte-identical.
      printf("bang t=%ld n=%d dir=(%.3f %.3f %.3f) hit=%s at=(%.2f %.2f %.2f) d=%.2f",
             g_tick, e.i, (double)e.b.x, (double)e.b.y, (double)e.b.z,
             // k is wire-fed on a netclient (net_ev_to_live copies it raw) —
             // clamp here at the one consumer, not in the decoder where k's
             // meaning is per-type (EV_HIT hp, EV_END winner).
             e.k >= 0 && e.k < 4 ? KIND[e.k] : "?",
             (double)e.a.x, (double)e.a.y, (double)e.a.z,
             (double)(e.k ? e.f : -1.0f));
      if (e.pen > 0.0f) printf(" pen=%.3f", (double)e.pen);
      putchar('\n');
    }
    break;
  case EV_HIT:
  case EV_HIT_HEAD:
    if (e.src == g_local_ent)       // the player's own hits: unchanged contract
      printf("hit t=%ld part=%s d=%d hp=%d\n", g_tick,
             e.type == EV_HIT_HEAD ? "head" : "body", e.i, e.k);
    else if (e.dst == g_local_ent)  // the player taking damage
      printf("phit t=%ld part=%s d=%d hp=%d by=%s\n", g_tick,
             e.type == EV_HIT_HEAD ? "head" : "body", e.i, e.k, ent_name(e.src));
    break;                // bot-vs-bot hits stay silent
  case EV_KILL:
    printf("kill t=%ld by=%s victim=%s hs=%d\n", g_tick,
           ent_name(e.src), ent_name(e.dst), e.k);
    if (e.dst >= 0 && e.dst < MAX_ENTS && e.dst != g_local_ent && g_sc_spawn[e.dst] > 0) {
      g_sc_deaths++;
      if (g_tick - g_sc_spawn[e.dst] < SPAWNCHECK_QUICK) g_sc_quick++;
      if (g_tick - g_sc_spawn[e.dst] < SPAWNCHECK_SLOW) g_sc_slow++;
      g_sc_spawn[e.dst] = 0;
    }
    break;
  // Gated on the pusher's identity, not on push-side discipline: the printer
  // contract is "the PLAYER's dryfire/reload_cancel", and once bots or remote
  // humans push these with .src set, an ungated line lands mid-trace in every
  // gunplay artefact of the repo.
  case EV_DRYFIRE: if (e.src == g_local_ent) printf("dryfire t=%ld\n", g_tick); break;
  // PLAYER RELOADS ONLY, like `bang`. Bots push EV_RELOAD now (an enemy
  // changing a magazine is the genre's strongest information cue), and printing
  // theirs would put unpredictable lines into the middle of every gunplay trace
  // — the same reason bot fire and footsteps are silent here.
  case EV_RELOAD:  if (e.src == g_local_ent) printf("reload t=%ld\n", g_tick); break;
  case EV_RELOAD_CANCEL:
    if (e.src == g_local_ent) printf("reload_cancel t=%ld\n", g_tick);
    break;
  case EV_RESPAWN:
    printf("respawn t=%ld ent=%s\n", g_tick, ent_name(e.dst));
    if (e.dst >= 0 && e.dst < MAX_ENTS) g_sc_spawn[e.dst] = g_tick;
    break;
  case EV_END:     printf("matchover t=%ld winner=%s\n", g_tick, ent_name(e.k)); break;
  case EV_IMPACT:  break;  // visual only
  // Footsteps and landings are SILENT in the trace by design: 21 entities at
  // ~4 steps/s would bury every gunplay line the contract is written against.
  case EV_STEP:
  case EV_LAND:    break;
  case EV_MELEE:   break;
  case EV_NADE:    if (e.k == 2) printf("spoon t=%ld\n", g_tick); else if (!e.k) printf("nade t=%ld src=%s at=(%.2f %.2f %.2f) v=(%.2f %.2f %.2f) fuse=%d\n", g_tick, ent_name(e.src), (double)e.a.x, (double)e.a.y, (double)e.a.z, (double)e.b.x, (double)e.b.y, (double)e.b.z, e.i); break;
  case EV_BLAST:   printf("blast t=%ld src=%s at=(%.2f %.2f %.2f)\n", g_tick, ent_name(e.src), (double)e.a.x, (double)e.a.y, (double)e.a.z); break;
  case EV_BLOCK:   printf("block t=%ld %s -> %s parry=%d\n", g_tick, ent_name(e.src), ent_name(e.dst), e.k); break;
  }
#endif
}

