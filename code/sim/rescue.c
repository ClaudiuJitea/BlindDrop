// code/sim/rescue.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ===========================================================================
// HOSTAGE RESCUE — "OPERATION NIGHTFALL". A single-player mission on COMPOUND:
// three hostages are held inside a guarded stronghold; free them (hold the
// RELOAD key beside one) and walk them out to the helicopter on the LZ north
// of the walls. The guards are the ordinary bots with four changes, all gated
// on rescue_on(): they never fight each other, they keep POSTS and walk
// ROUTES instead of wandering, a guard only fights you once its DETECTION
// meter has filled (distance, stance, movement, lamp light and the edge of its
// view all set the rate), and one who has you in sight long enough — or who
// finds a body — radios the ALARM. The alarm starts the hostages' execution
// clock and sends every hunter to your last known position.
//
// CO-OP. The mission is played by a SQUAD of one to three: the humans first,
// and bots take the places nobody holds (resc_squad). Bot slots [0, nguards)
// are the guards and [nguards, nbots) the squad's bots, so a joining human
// retires a squad bot (bots leave from the top) and never a guard. More squad
// means more guards (resc_guard_count). The sim runs offline and on a server;
// a client runs none of it and draws what NP_RESCUE tells it (rescue_view).
// ===========================================================================
#define RESC_HOSTAGES 3
enum { HS_CAPTIVE, HS_FOLLOW, HS_EXTRACTED, HS_DEAD };
enum { GK_SENTRY, GK_PATROL, GK_SNIPER, GK_WARDEN };
enum { RW_NONE, RW_SPOTTED, RW_BODY, RW_SHOT };
enum { RR_RUNNING, RR_SUCCESS, RR_FAIL };
enum { RF_NONE, RF_KILLED, RF_EXECUTED };
typedef struct { float x, z, y, yaw; int kind, route; } guard_post_t;
typedef struct { int n; float p[6][2]; } guard_route_t;
typedef struct {
  v3 pos, prev, vel;
  float yaw, prev_yaw;
  int state, grounded, stuck;
  float free_t;            // 0..1 restraints cut
  float gait, prev_gait;   // walk cycle phase (rad)
  float move, prev_move;   // 0 standing .. 1 running, smoothed
  float pose, prev_pose;   // 0 kneeling .. 1 standing (eased)
  v3 wp;
  int wp_t;
  int leader;              // the squad entity this hostage follows (-1)
  v3 net_pos;              // client: the last replicated position
  float net_yaw;           // ...and heading
} hostage_t;
typedef struct {
  int on;
  hostage_t h[RESC_HOSTAGES];
  int alarm, alarm_tick, alarm_why, exec_t, exec_max;
  int clock, result, why, end_tick;
  int freed, extracted;
  int kills, takedowns, heads, spotted_n, bodies;
  int reach_i, cutting;    // the captive within the LOCAL player's reach / being freed (-1)
  int use_held[MAX_HUMANS];  // each human's RELOAD, latched by player_tick
  int msg_t, msg_kind, msg_seq;
  char msg[48], msg2[48];
  int ever_seen;           // any guard ever reached full detection
  int lit;                 // the LOCAL player stands in lamp light this tick
  uint8_t lit_e[MAX_ENTS]; // ...and every squad member (sim side)
  int nguards, squad;      // latched at the mission's start
  int fire_free_t;         // ticks the squad bots may open fire (a human shot)
  int cl_alarm_age;        // client: ticks since it saw the alarm go up
  v3 lkp;                  // last position any guard saw the squad at
  long lkp_tick;
  float detect[MAX_BOTS], prev_detect[MAX_BOTS];
  int det_who[MAX_BOTS];   // the squad entity each guard's meter is on (-1)
  v3 glimpse[MAX_BOTS];
  int radio_t[MAX_BOTS], radio_why[MAX_BOTS];
  int wp_i[MAX_BOTS], wait_t[MAX_BOTS], body_cd[MAX_BOTS];
  float look_yaw[MAX_BOTS];
  long heard_tick[MAX_BOTS];
  uint8_t body_found[MAX_BOTS];
  int siren_t, sfx_pending;
  float rotor, prev_rotor, rotor_w;
} rescue_t;
// Per lobby, like every other piece of match state a server ticks per arena.
static rescue_t g_resc_all[SRV_LOBBIES];
#define g_resc (g_resc_all[g_lobby])

// The hostages: where each is held (x, z, floor y) and which way they kneel.
static const float RESC_HOLD[RESC_HOSTAGES][4] = {
  {-27.0f, -25.0f, 0.0f, -1.5708f},   // the pen, facing its back wall
  {33.0f, -6.0f, 0.0f, 1.5708f},      // the motor pool, between the jeeps
  {3.0f, -21.5f, 6.4f, 0.0f},         // the HQ roof
};
static const char *const RESC_HOLD_NAME[RESC_HOSTAGES] = {"THE PEN", "MOTOR POOL", "HQ ROOF"};
// The LZ (the pad north of the walls) and where the freed sit to wait.
#define RESC_LZ_X0 13.0f
#define RESC_LZ_X1 31.0f
#define RESC_LZ_Z0 -51.0f
#define RESC_LZ_Z1 -39.0f
static const float RESC_SEAT[RESC_HOSTAGES][2] = {{19.6f, -42.4f}, {21.4f, -42.3f}, {23.2f, -42.4f}};
#define RESC_INSERT_X -45.0f
#define RESC_INSERT_Z 50.0f

static const guard_route_t GUARD_ROUTES[] = {
  {4, {{-30.0f, 24.5f}, {12.0f, 24.5f}, {12.0f, 17.5f}, {-12.0f, 17.5f}}},       // 0 south yard
  {4, {{-37.0f, -27.0f}, {-37.0f, -3.0f}, {-24.0f, -4.0f}, {-24.0f, -16.0f}}},   // 1 west lane
  {4, {{16.0f, 3.0f}, {36.5f, 5.0f}, {36.5f, 24.0f}, {14.0f, 28.0f}}},           // 2 east lane
  {3, {{-18.0f, -32.0f}, {30.0f, -32.8f}, {14.0f, -26.0f}}},                     // 3 north lane
  {4, {{-14.0f, -12.0f}, {12.0f, -12.0f}, {12.0f, -28.0f}, {-14.0f, -28.0f}}},   // 4 round the HQ
  {2, {{-20.0f, 35.0f}, {30.0f, 35.0f}}},                                         // 5 outside south
};
// In the order the difficulty adds them: EASY fields the first 10, NORMAL 14,
// HARD all 18.
static const guard_post_t GUARD_POSTS[] = {
  {-20.5f, -24.0f, 0.0f, 1.5708f, GK_WARDEN, 0},     // the pen's door
  {23.0f, -4.0f, 0.0f, -1.5708f, GK_WARDEN, 0},      // the motor pool
  {-2.0f, -17.0f, 6.4f, 3.1416f, GK_WARDEN, 0},      // the HQ roof
  {35.5f, -31.5f, 6.0f, 0.7854f, GK_SNIPER, 0},      // NE tower, over the LZ road
  {-30.0f, 24.5f, 0.0f, 1.5708f, GK_PATROL, 0},      // south yard
  {-6.5f, 33.6f, 0.0f, 3.1416f, GK_SENTRY, 0},       // south gate, outside
  {-37.0f, -27.0f, 0.0f, 3.1416f, GK_PATROL, 1},     // west lane
  {-35.5f, 27.5f, 6.0f, -2.3562f, GK_SNIPER, 0},     // SW tower, over the wadi
  {16.0f, 3.0f, 0.0f, 1.5708f, GK_PATROL, 2},        // east lane
  {15.0f, -32.6f, 0.0f, 0.0f, GK_SENTRY, 0},         // north gate
  {-14.0f, -12.0f, 0.0f, 1.5708f, GK_PATROL, 4},     // round the HQ
  {6.5f, 33.6f, 0.0f, 3.1416f, GK_SENTRY, 0},        // south gate, outside
  {-35.5f, -31.5f, 6.0f, -0.7854f, GK_SNIPER, 0},    // NW tower
  {-20.0f, 35.0f, 0.0f, 1.5708f, GK_PATROL, 5},      // outside the south wall
  {27.0f, 16.8f, 0.0f, -1.5708f, GK_SENTRY, 0},      // the fuel depot
  {35.5f, 27.5f, 6.0f, 2.3562f, GK_SNIPER, 0},       // SE tower
  {-18.0f, -32.0f, 0.0f, 1.5708f, GK_PATROL, 3},     // north lane
  {-35.0f, -4.0f, 0.0f, -1.5708f, GK_SENTRY, 0},     // the west breach
};
#define RESC_POSTS ((int)(sizeof GUARD_POSTS / sizeof GUARD_POSTS[0]))

// What the difficulty changes: how many guards, how far and how fast they
// see, how long a spotter needs on the radio, and the execution clock.
typedef struct { int guards; float range, snipe, rate, radio_s, exec_s; } resc_diff_t;
static const resc_diff_t RESC_DIFF[3] = {
  {10, 30.0f, 52.0f, 0.75f, 1.7f, 100.0f},
  {14, 36.0f, 60.0f, 1.00f, 1.25f, 75.0f},
  {18, 42.0f, 68.0f, 1.30f, 0.85f, 55.0f},
};
static const resc_diff_t *resc_diff(void) {
  int d = g_cfg.sp_diff;
  return &RESC_DIFF[d < 0 ? 0 : d > 2 ? 2 : d];
}
// rescue_on: THIS process runs the mission (offline, or a server's arena).
// rescue_view: a mission is being shown (also a client, from NP_RESCUE).
static int rescue_on(void) { return g_resc.on && !g_home && (g_server || !g_online); }
static int rescue_view(void) { return g_resc.on && !g_home; }
// The squad's size, 1..RESC_SQUAD_MAX: the humans plus the bots in the rest.
static int resc_squad_cfg(void) {
  int n = g_cfg.sp_squad;
  return n < 1 ? 1 : n > RESC_SQUAD_MAX ? RESC_SQUAD_MAX : n;
}
// The guards: the difficulty's number, and two more for every extra member of
// the squad (a three-man team on EASY meets NORMAL's fourteen), capped by the
// posts there are and by the bot slots the squad's own bots still need.
static int resc_guard_count(int diff, int squad) {
  int n = RESC_DIFF[diff < 0 ? 0 : diff > 2 ? 2 : diff].guards + 2 * (squad - 1);
  if (n > RESC_POSTS) n = RESC_POSTS;
  if (n > MAX_BOTS - (RESC_SQUAD_MAX - 1)) n = MAX_BOTS - (RESC_SQUAD_MAX - 1);
  return n;
}
static int resc_humans(void) {
  int n = 0;
  for (int h = 0; h < MAX_HUMANS; h++) n += g_humans_on[h] ? 1 : 0;
  return n;
}
static int rescue_bot_count(void) {
  if (!rescue_on()) return -1;
  int hu = resc_humans();
  int sb = hu ? g_resc.squad - hu : 0;   // an empty server fields no squad bots
  return g_resc.nguards + (sb > 0 ? sb : 0);
}
// Which side an entity is on: 0 the squad (humans and the squad's bots),
// 1 the guards, -1 nobody. The guard range is the low bot slots.
static int resc_is_guard(int e) { return e >= 1 && e <= MAX_BOTS && e - 1 < g_resc.nguards; }
static int resc_is_squad(int e) {
  return human_of_ent(e) >= 0 || (e >= 1 && e <= MAX_BOTS && e - 1 >= g_resc.nguards);
}
static int resc_side(int e) {
  if (e < 0 || e >= MAX_ENTS) return -1;
  return resc_is_guard(e) ? 1 : 0;
}
// The kit colour a body is drawn in: its side in a team game; in HOSTAGE
// RESCUE the squad wears BRAVO's blue and the guards their own gear, so a
// squad bot is never mistaken for a guard. -1 = the ordinary kit.
static int fig_team_of(int e) {
  if (teams_on()) return team_of(e);
  if (rescue_view() && e >= 0 && e < MAX_ENTS && resc_is_squad(e)) return 1;
  return -1;
}
// Is entity `e` drawn as an INSURGENT (the head wrap and chest rig) rather
// than an operator? Whoever is against the local player: the guards in a
// mission, the other side in a team game, everyone else in a free-for-all.
// The local player and his side are always operators. Presentation only: each
// client decides it from its own seat, the sim never reads it.
static int fig_ins_of(int e) {
  if (e < 0 || e >= MAX_ENTS) return 0;
  if (g_home) return e & 1;                 // the backdrop shows both forces
  if (e == g_local_ent) return 0;
  if (rescue_view()) return resc_is_guard(e);
  if (teams_on()) return g_local_ent >= 0 ? team_of(e) != team_of(g_local_ent) : team_of(e) == 0;
  return 1;
}
static const guard_post_t *rescue_post(int bi) {
  return &GUARD_POSTS[bi >= 0 && bi < RESC_POSTS ? bi : 0];
}
#define RESC_INTRO_TICKS (9 * TICK_HZ / 2)   // 4.5 s: the briefing, then the job
static void rescue_msg(int kind, const char *a, const char *b) {
  snprintf(g_resc.msg, sizeof g_resc.msg, "%s", a);
  snprintf(g_resc.msg2, sizeof g_resc.msg2, "%s", b ? b : "");
  g_resc.msg_kind = kind;
  g_resc.msg_t = 4 * TICK_HZ;
  g_resc.msg_seq++;          // a client takes the new banner on this edge
}
// May bot `bi` choose entity `e` as a target? A GUARD: never another guard,
// and a member of the squad only once it has detected someone (or he shot it,
// or it already has him). A SQUAD BOT holds its fire: it engages only a guard
// who is onto the squad, once the alarm is up, or for a few seconds after a
// human of the squad has fired (the leader opened up, everyone does).
static int rescue_may_target(const bot_t *b, int bi, int e) {
  if (bi < g_resc.nguards) {
    if (!resc_is_squad(e)) return 0;
    return g_resc.detect[bi] >= 1.0f || b->tgt == e || b->threat_src == e;
  }
  if (!resc_is_guard(e)) return 0;
  const bot_t *g = &g_bots[e - 1];
  return g_resc.alarm || g_resc.fire_free_t > 0 || b->tgt == e || b->threat_src == e ||
         g_resc.detect[e - 1] >= 0.5f || (g->tgt >= 0 && resc_is_squad(g->tgt));
}

// Bounded, simulation-owned writer. tick==0 remains the empty-store marker.
static void bot_noise_emit(int src, bot_noise_kind_t kind, v3 pos, float radius) {
  if (src < 0 || src >= MAX_ENTS || kind < 0 || kind >= BOT_NOISE_COUNT ||
      !(radius > 0.0f) || f_is_nonfinite(radius)) return;
  g_match.noise[kind][src] = (bot_noise_t){pos, g_tick, radius, src};
}

static int bot_hear_life(bot_noise_kind_t kind) {
  return kind == BOT_NOISE_SHOT ? BOT_HEAR_SHOT_TICKS : BOT_HEAR_STEP_TICKS;
}

// Perception reads only a completed world tick. That makes a stimulus equally
// available to every bot on the following tick, independent of entity order.
static void bot_hear_update(bot_t *b, int bi) {
  if (b->hear_tick && g_tick > b->hear_until) {
    b->hear_tick = 0;
    b->hear_until = 0;
    b->hear_score = 0.0f;
  }

  v3 ear = {{b->pos.x, b->pos.y + bot_eye(b), b->pos.z}};
  const bot_noise_t *best = NULL;
  bot_noise_kind_t best_kind = BOT_NOISE_SHOT;
  float best_score = 0.0f;
  int best_blocked = 0;
  for (int kind = 0; kind < BOT_NOISE_COUNT; kind++) {
    for (int src = 0; src < MAX_ENTS; src++) {
      const bot_noise_t *n = &g_match.noise[kind][src];
      if (n->tick == 0 || n->tick >= g_tick || n->src == bi + 1) continue;
      if (team_mates(bi + 1, n->src)) continue;     // a teammate's boots and shots
      if (g_tick - n->tick > bot_hear_life((bot_noise_kind_t)kind)) continue;
      float dist = v3_len(v3_sub(n->pos, b->pos));
      // RELATIVE loudness — how much of its own reach the sound has left, not
      // how many metres. The absolute margin made a gunshot's 34 m reach
      // dominate a footstep's 11 m ABSOLUTELY: a shot at 5 m scored 29, which
      // no footstep can ever beat, so for the 1.5 s life of every shot inside
      // ~28 m a bot was deaf to boots in the same room. That is a firefight,
      // i.e. exactly when someone is walking up behind you. Normalised, a step
      // at 5 m (0.55) beats a shot at 30 m (0.12) and loses to one at 10 m
      // (0.71) — near beats far within a kind, and a shot still outranks a
      // step at comparable closeness, which is the ordering that was wanted.
      float score = (n->radius - dist) / n->radius;
      if (score <= 0.0f) continue;
      // Exactly result-preserving, and it is what pays for the ray the startle
      // branch now spends: BOT_HEAR_WALL is a constant below 1, so attenuation
      // can only LOWER a score. A candidate that cannot beat the incumbent even
      // unattenuated cannot beat it attenuated either, so its trace is wasted —
      // and this loop is MAX_ENTS x 2 kinds per bot per tick against a LOS
      // budget already at ~23% of sim self-time at 20 bots.
      if (score <= best_score) continue;
      // The blocked verdict is KEPT, not re-derived: the localisation error
      // below doubles through a wall and re-tracing the winner for that was one
      // more ray per bot per tick (measured 0.74/bot-tick at 20 bots) to
      // recompute something this loop had already decided.
      int blocked = !los_clear(ear, n->pos);
      if (blocked) score *= BOT_HEAR_WALL;
      if (score > best_score) {
        best = n;
        best_kind = (bot_noise_kind_t)kind;
        best_score = score;
        best_blocked = blocked;
      }
    }
  }
  // The 1.15x hysteresis answers "which SOURCE do I believe" and must not also
  // answer "where is the source I already believe". Source-agnostic, it froze
  // the memory on the first footfall of a pass-by: a runner going past has a
  // FALLING score, so every later step was rejected and the bot spent the
  // whole 0.6 s life walking at a point the runner had left metres ago — the
  // "reacts late, to where I was" complaint, in one comparison.
  int fresh = !b->hear_tick || g_tick > b->hear_until;
  if (best && (fresh || best->src == b->hear_src ||
               best_score > b->hear_score * 1.15f)) {
    // The error DIRECTION is drawn once per contact and then held for as long
    // as the same source keeps refreshing it. Re-rolling it per footfall was
    // measurably worse than no error at all: at ~4.5 plants a second the bot's
    // facing swayed +-9 deg several times a second while it was turning, which
    // reads as indecision rather than as uncertainty. A person mislocates a
    // sound in ONE direction and keeps making the same mistake.
    if (fresh || best->src != b->hear_src)
      b->hear_err_a = rng_rangef(&g_rng_bot, -F_PI, F_PI);
    float err = BOT_HEAR_ERR * v3_len(v3_sub(best->pos, b->pos));
    if (best_blocked) err *= 2.0f;
    // Clamp AFTER the doubling, or BOT_HEAR_ERR_MAX is not the ceiling it says
    // it is: clamping first let a blocked contact past 16 m reach 5.2 m, which
    // no fixture could see because the wall case is staged at 10 m.
    err = f_clamp(err, 0.0f, BOT_HEAR_ERR_MAX);
    b->hear_pos = best->pos;
    b->hear_pos.x += sinf(b->hear_err_a) * err;
    b->hear_pos.z += cosf(b->hear_err_a) * err;
    b->hear_pos.y = 0.0f;   // a bearing along the floor; height is not heard
    b->hear_src = best->src;
    b->hear_tick = best->tick;
    b->hear_until = best->tick + bot_hear_life(best_kind);
    b->hear_kind = best_kind;
    b->hear_score = best_score;
  }
}

// Server observability: ONE function feeds both the server's 10 s
// stdout digest (the wrapper redirects stdout into game.log, and
// `make server-logs` renders it — that file is the whole monitoring story)
// and the `netstat` proof command — the one-copy rule
// applied to observability, and the reason a hanging instance is diagnosable
// at all. Grows fields as the phases add them (clients, worst RTT, dropped
// inputs come with the transport).
static long g_srv_started;  // unix time at server start, 0 = not a server
static long g_srv_behind;     // worst tick backlog the pacing loop ever saw
static int net_srv_client_count(void);  // defined with the connection layer below
static void net_srv_lat(int *rtt_max, int *qd_max);  // ditto (reads g_conns)
// UNSIGNED, because these are server-LIFETIME totals with no reset: a client
// holding a valid token can add up to NET_IN_MAX per record, which reaches
// INT_MAX in about a quarter of an hour of sustained traffic. Wraparound is
// defined for unsigned; signed overflow is UB, and these only feed a diagnostic.
static unsigned g_srv_dropped_inputs;   // input ticks the budget refused
static unsigned g_srv_trim_drops;       // moves discarded by the backlog trim —
                                        // THE rubber-band path; it read as
                                        // dropped_in=0 while the live server
                                        // trimmed a correction per hitch
// Optional net diagnostics (OFF by default): `netdebug` in the harness,
// `--net-debug` on a server. One line per correction / backlog trim / far-
// ahead skip — the questions a live rubber-banding report needs answered,
// priced so the default game.log stays at one digest per 10 s (decision 9's
// bounded-volume argument rests on that number).
static int g_net_debug;
[[maybe_unused]] static void net_digest(char *out, size_t n) {
  int nh = 0;
  for (int h = 0; h < MAX_HUMANS; h++)
    if (g_humans_on[h]) nh++;
  long up = g_srv_started ? (long)time(NULL) - g_srv_started : 0;
  // The world hash rides the digest so a client<->server arena divergence
  // is diagnosable from game.log with grep and nothing else. clients and
  // dropped-inputs are the numbers an operator asks for; rtt/qd are the worst
  // smoothed ping and input-queue depth across the conns — the two numbers
  // that say "the connection feel is fine" or name the client it is not.
  int rtt, qd;
  net_srv_lat(&rtt, &qd);
  snprintf(out, n,
           "digest up=%ld tick=%ld clients=%d humans=%d bots=%d over=%d "
           "behind=%ld dropped_in=%u trim=%u ev_drops=%ld rtt=%d qd=%d world=%08x",
           up, g_tick, net_srv_client_count(), nh, g_match.nbots, g_match.over,
           g_srv_behind, g_srv_dropped_inputs, g_srv_trim_drops, g_ev_drops,
           rtt, qd, net_world_hash());
}

// Server log line: unix-ts prefix, one flushed stdout line. Only --server mode
// calls this; the harness and play mode never do.
static void slog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void slog(const char *fmt, ...) {
  printf("%ld ", (long)time(NULL));
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  putchar('\n');
  fflush(stdout);
}

// Position of ANY entity — the one copy (A.2): bot_tick resolved this inline
// twice byte-identically, ent_damage's last_seen a third time, and each was a
// g_bots[e - 1] OOB for a human id above MAX_BOTS.
static v3 ent_pos(int e) {
  int h = human_of_ent(e);
  return h >= 0 ? g_players[h].pos : g_bots[e - 1].pos;
}

// A body hit in the chest band of the victim's frame — the plate carrier. A
// presentation call only (the round does the same damage either way).
static int hit_on_plate(const event_t *e) {
  if (e->type != EV_HIT || e->dst < 0 || e->dst >= MAX_ENTS) return 0;
  float h = e->a.y - ent_pos(e->dst).y;
  return h > 1.02f && h < 1.48f;
}

// Fill / cap decisions. THREE numbers, each readable on its own:
// cap_bots is how many bots the arena may hold, cap_public how large it may
// grow through the public door (humans AND bots together). The arena is NOT a
// constant fighter count —
// it grows with the humans, and the bot target is a pure function of them.
//
// "Bots yield first" is deliberately NOT a branch: it falls out of the bot
// target shrinking as humans arrive. The predecessor spent a three-valued enum
// (reject / displace-bot / grow) on saying the same thing, and needed the
// caller to keep bots and humans consistent to say it correctly.
//
// The never-disguised-bots rule is untouched by both — the surplus bot
// retires at its NEXT respawn.
static int net_bot_target(int humans, int cap_public, int cap_bots) {
  int room = cap_public - humans;
  if (room > cap_bots) room = cap_bots;
  return room < 0 ? 0 : room;
}
static int net_join_ok(int humans, int cap_public) {
  return humans < cap_public;
}


// One entity's live physics/state packed into a wire record. Presentation
// (anim_t/vm_t/pose_t) is deliberately absent — the client re-solves it.
// Spawn generation per entity, bumped on every (re)spawn. The snapshot carries
// it so a client can tell a TELEPORT (respawn: reset the anim solver, do not
// interpolate across the map, adopt the server's spawn angles for its OWN
// entity) from ordinary motion. It stayed 0 for a while — which silently made
// the client's whole respawn-edge machinery dead code.
static uint8_t g_spawn_seq_all[SRV_LOBBIES][MAX_ENTS];
#define g_spawn_seq (g_spawn_seq_all[g_lobby])

static void net_ent_from_live(net_ent_t *e, int ent) {
  int h = human_of_ent(ent);
  v3 pos, vel; float yaw, pitch, lean; int alive, grounded, crouch, slide, hp, wpn;
  if (h >= 0) {
    const player_t *p = &g_players[h];
    pos = p->pos; vel = p->vel; yaw = p->yaw; pitch = p->pitch; lean = p->lean;
    alive = p->alive; grounded = p->grounded; crouch = p->crouched; slide = p->sliding;
    hp = p->hp; wpn = p->wp.cur;
  } else {
    const bot_t *b = &g_bots[ent - 1];
    pos = b->pos; vel = b->vel; yaw = b->yaw; pitch = b->pitch; lean = 0.0f;
    alive = b->alive; grounded = b->grounded; crouch = b->crouch; slide = b->sliding;
    hp = b->hp; wpn = b->wp.cur;
  }
  e->id = (uint8_t)ent;
  e->flags = (alive ? 1 : 0) | (grounded ? 2 : 0) | (crouch ? 4 : 0) | (slide ? 8 : 0);
  e->hp = (uint8_t)f_clamp((float)hp, 0.0f, 255.0f);
  e->weapon = (uint8_t)(wpn & 7);
  {
    const weapon_t *lw = h >= 0 ? &g_players[h].wp : &g_bots[ent - 1].wp;
    e->loadout = (uint8_t)loadout_pack(lw->var[WPN_AR], lw->var[WPN_PT]);
  }
  e->spawn_seq = g_spawn_seq[ent];
  {
    const weapon_t *wp = h >= 0 ? &g_players[h].wp : &g_bots[ent - 1].wp;
    int mag = wp->ammo[wpn >= 0 && wpn < WPN_COUNT ? wpn : 0];
    e->mag = (uint8_t)(mag < 0 ? 0 : mag > 255 ? 255 : mag);
    e->melee = (uint8_t)(wp->melee_v << 6 | ((wp->melee_t + 1) / 2 & 63));
    if (wpn == WPN_GR)
      e->melee = wp->gr_t > 0 ? (uint8_t)(1 << 6 | ((wp->gr_t + 1) / 2 & 63))
                              : (uint8_t)(wp->gr_cook > 0 ? 2 << 6 : 0);
    if (wpn == WPN_KN && wp->ads_s > 0.5f) e->flags |= 16;
  }
  e->x = net_q_pos(pos.x); e->y = net_q_pos(pos.y); e->z = net_q_pos(pos.z);
  e->vx = net_q_vel(vel.x); e->vy = net_q_vel(vel.y); e->vz = net_q_vel(vel.z);
  e->yaw = net_q_yaw(yaw); e->pitch = net_q_pitch(pitch);
  e->lean = net_q_wish(lean);
}

// SNAPSHOT: header + one record per live entity. Returns bytes written,
// or 0 if the buffer was too small (a real snapshot at 8 fighters is ~230 B).
// The server's outbound event log: every event the sim pushed, stamped
// with a monotone id and the tick it happened on. Entries stay until every
// client has acked them or they age past the resend window, so a lost snapshot
// costs a repeat, not a missing kill sound.
// 256: at the measured ev_peak of 8-10 per tick (20 bots hard) 128 was only
// ~13 ticks of history — INSIDE the 36-tick resend window, so a busy server
// overwrote a record before a lost snapshot could resend it.
#define NET_EVLOG 256
typedef struct { uint32_t id, tick; event_t e; } srv_ev_t;
static srv_ev_t g_srv_evlog_all[SRV_LOBBIES][NET_EVLOG];
#define g_srv_evlog (g_srv_evlog_all[g_lobby])
static int      g_srv_evlog_head_all[SRV_LOBBIES], g_srv_evlog_n_all[SRV_LOBBIES];
#define g_srv_evlog_head (g_srv_evlog_head_all[g_lobby])
#define g_srv_evlog_n    (g_srv_evlog_n_all[g_lobby])
static uint32_t g_srv_ev_next_all[SRV_LOBBIES] = {[0 ... SRV_LOBBIES - 1] = 1};
#define g_srv_ev_next (g_srv_ev_next_all[g_lobby])   // 0 = acked nothing

// Called once per server tick, BEFORE the sim's event list is drained.
static void net_ev_capture(void) {
  for (int i = 0; i < g_num_events; i++) {
    ev_type_t t = g_events[i].type;
    if (t == EV_STEP || t == EV_LAND) continue;   // each client makes its own
    g_srv_evlog[g_srv_evlog_head] =
        (srv_ev_t){g_srv_ev_next++, (uint32_t)g_tick, g_events[i]};
    g_srv_evlog_head = (g_srv_evlog_head + 1) % NET_EVLOG;
    if (g_srv_evlog_n < NET_EVLOG) g_srv_evlog_n++;
  }
}

static size_t net_snapshot_build(unsigned char *buf, size_t cap, uint32_t last_input_tick,
                                 uint32_t ev_ack, uint8_t qdepth) {
  np_wr w = {buf, cap, 0, 0};
  np_w16(&w, NET_PROTO);
  np_w8(&w, NP_SNAPSHOT);
  np_w32(&w, (uint32_t)g_tick);
  np_w32(&w, last_input_tick);
  np_w32(&w, net_world_hash());
  // THE ARENA CAN CHANGE UNDER A LIVE CLIENT. A server regenerates the map on
  // every rematch, and the seed was handed out exactly once, in ACCEPT — so
  // after the first match end the server simulated one world and streamed it
  // to clients still colliding against another. That does not present as a
  // version error, it presents as rubber-banding through walls. 4 bytes at
  // 60 Hz is 240 B/s per client; the alternative is a re-handshake.
  np_w32(&w, g_map_seed);
  np_w8(&w, (uint8_t)g_map_id);    // ...and which map that seed builds
  // This client's input-queue depth at broadcast time: the client's
  // clock nudge holds it at NET_QDEPTH_TARGET instead of letting drift and
  // jitter walk it into the backlog trim (a trim discards moves the client
  // already predicted — every one is a visible correction).
  np_w8(&w, qdepth);
  size_t cnt_at = w.at;
  np_w8(&w, 0);                            // count placeholder
  int cnt = 0;
  ENT_FOREACH(e) {
    net_ent_t ne;
    net_ent_from_live(&ne, e);
    net_ent_encode(&w, &ne);
    cnt++;
  }
  // Events above this client's ack, oldest first. Capped per snapshot so
  // one busy tick cannot blow the datagram past the MTU — the remainder simply
  // rides the next one, which is the same resend path a lost packet takes.
  // SELECT NEWEST-FIRST, EMIT OLDEST-FIRST. Selecting oldest-first looks like
  // the obvious reading of "resend everything above the ack", and it head-of-
  // line blocks: the cap fills with unacked old records every snapshot, and
  // anything newer waits for an ack that is itself one RTT away. Measured on
  // localhost with two bots, a client firing 30 rounds heard 21 of them; the
  // nine it lost were the newest, i.e. exactly the shots it had just taken.
  // The client dedups on id, so what matters inside one datagram is only that
  // ids ASCEND — the event channel is "grouped by tick in push order" — which
// the second
  // loop restores.
  size_t ev_at = w.at;
  np_w8(&w, 0);
  int pick[NET_EV_SNAP_MAX], nev = 0;
  // Both cut-offs are monotone along the newest-first scan (ids descend, age
  // ascends), so the first acked or aged record ends the walk — the loop used
  // to `continue` and rescan all 256 entries per conn per snapshot to pick 0-2.
  for (int k = g_srv_evlog_n - 1; k >= 0 && nev < NET_EV_SNAP_MAX; k--) {
    int idx = (g_srv_evlog_head - g_srv_evlog_n + k + 2 * NET_EVLOG) % NET_EVLOG;
    const srv_ev_t *s = &g_srv_evlog[idx];
    if ((int32_t)(s->id - ev_ack) <= 0) break;           // already acknowledged
    uint32_t age = (uint32_t)g_tick - s->tick;
    if (age > NET_EV_RESEND_TCK) break;                  // outside the window
    pick[nev++] = idx;
  }
  for (int k = nev - 1; k >= 0; k--) {                   // ascending ids again
    const srv_ev_t *s = &g_srv_evlog[pick[k]];
    net_ev_t ne;
    net_ev_from_live(&ne, &s->e, s->id);
    net_ev_encode(&w, &ne);
  }
  if (w.bad) return 0;
  buf[cnt_at] = (uint8_t)cnt;
  buf[ev_at]  = (uint8_t)nev;
  return w.at;
}

// Decode a snapshot into a caller array (the client's interpolation source).
// Every field is bounds-checked in net_ent_decode; a bad count or a short
// buffer yields fewer records, never a wild read. Returns entity count, or -1
// on a malformed header (wrong proto / type).
static int net_snapshot_read(const unsigned char *buf, size_t len, uint32_t *server_tick,
                             uint32_t *last_input_tick, uint32_t *world,
                             uint32_t *seed, uint8_t *mapid, uint8_t *qdepth,
                             net_ent_t *out, int out_cap, int *complete,
                             net_ev_t *evs, int ev_cap, int *ev_cnt) {
  np_rd r = {buf, len, 0, 0};
  if (np_r16(&r) != NET_PROTO) return -1;
  if (np_r8(&r) != NP_SNAPSHOT) return -1;
  uint32_t st = np_r32(&r), li = np_r32(&r), wh = np_r32(&r), sd = np_r32(&r);
  uint8_t mid = np_r8(&r);
  uint8_t qd = np_r8(&r);
  int cnt = np_r8(&r);
  if (r.bad) return -1;
  if (mid >= MAP_COUNT) return -1;         // hostile map id: refuse the packet
  if (mapid) *mapid = mid;
  if (qdepth) *qdepth = qd;
  if (cnt > out_cap) cnt = out_cap;        // never write past the caller array
  int got = 0;
  for (int i = 0; i < cnt; i++) {
    net_ent_t ne;
    if (!net_ent_decode(&r, &ne)) break;   // truncated: stop, keep what parsed
    out[got++] = ne;
  }
  // A PARTIAL LIST IS NOT A SHORTER ROSTER. Records go out in ENT_FOREACH
  // order, so humans are LAST — a single unparseable record therefore looks
  // exactly like "everyone after this left", and a caller that reads absence
  // as departure would retire them, up to and including the local player.
  if (complete) *complete = (got == cnt);
  // The event section. A truncated one costs the events it cut, never a
  // wild read, and never the entity list above it — which is why the roster's
  // completeness flag is decided BEFORE this runs.
  if (ev_cnt) *ev_cnt = 0;
  if (evs && ev_cap > 0) {
    int ec = np_r8(&r);
    if (!r.bad) {
      if (ec > ev_cap) ec = ev_cap;
      int eg = 0;
      for (int i = 0; i < ec; i++) {
        net_ev_t ne;
        if (!net_ev_decode(&r, &ne)) break;
        evs[eg++] = ne;
      }
      if (ev_cnt) *ev_cnt = eg;
    }
  }
  if (server_tick) *server_tick = st;
  if (last_input_tick) *last_input_tick = li;
  if (world) *world = wh;
  if (seed) *seed = sd;
  return got;
}

// SCORES(sibling): the match's score rows as replicated STATE, 4 Hz.
// kills/deaths/shots/hits only ever mutate on the server (ent_damage,
// match_on_kill, gun_shoot), so a connected client's scoreboard read zeros
// forever — this packet is the one source of score truth online. Identical
// for every client, so the caller builds it once and sends it per conn.
// ping is measured per connection (g_tick - input view tick, smoothed) and
// rides here because it is scoreboard furniture, not hot-path state.
typedef struct { uint8_t id; uint16_t kills, deaths, shots, hits; uint8_t ping, mscore, team; } net_score_t;
// The mode block: the rules a client predicts under, and LAST MAN STANDING's
// between-rounds state for its HUD. round_win 0xFF = nobody.
typedef struct { uint8_t mode, round_secs, round_win, rounds, round_no;
                 uint8_t team_size, team_fill; uint16_t tscore[2]; } net_mode_t;
static size_t net_scores_build(unsigned char *buf, size_t cap,
                               const uint8_t ping[MAX_ENTS]) {
  np_wr w = {buf, cap, 0, 0};
  np_w16(&w, NET_PROTO);
  np_w8(&w, NP_SCORES);
  np_w8(&w, (uint8_t)(g_match.over ? 1 : 0));
  np_w8(&w, (uint8_t)(g_match.winner < 0 ? 0xFF : g_match.winner));
  np_w16(&w, (uint16_t)g_cfg.sp_frag);       // "FIRST TO N" is match property
  np_w8(&w, (uint8_t)game_mode());
  np_w8(&w, (uint8_t)((g_match.round_t + TICK_HZ - 1) / TICK_HZ));
  np_w8(&w, (uint8_t)(g_match.round_win < 0 ? 0xFF : g_match.round_win));
  np_w8(&w, (uint8_t)g_cfg.sp_rounds);
  np_w8(&w, (uint8_t)(g_match.round_no & 0xFF));
  np_w8(&w, (uint8_t)g_cfg.sp_team_size);     // the team block
  np_w8(&w, (uint8_t)((g_cfg.sp_team_fill ? 1 : 0) | (g_cfg.sp_team_ff ? 2 : 0)));
  np_w16(&w, (uint16_t)g_team_score[0]);
  np_w16(&w, (uint16_t)g_team_score[1]);
  size_t cnt_at = w.at;
  np_w8(&w, 0);
  int cnt = 0;
  ENT_FOREACH(e) {
    np_w8(&w, (uint8_t)e);
    np_w16(&w, (uint16_t)g_match.kills[e]);
    np_w16(&w, (uint16_t)g_match.deaths[e]);
    np_w16(&w, (uint16_t)g_match.shots[e]);
    np_w16(&w, (uint16_t)g_match.hits[e]);
    np_w8(&w, ping ? ping[e] : 0);
    np_w8(&w, (uint8_t)g_match.mscore[e]);
    np_w8(&w, (uint8_t)(g_team[e] ? 1 : 0));
    cnt++;
  }
  // Name section: every present HUMAN's display name (bots derive theirs from
  // BOT_NAMES locally). Without this a client rendered every human — itself
  // included — as ent_name's REMOTE fallback, on the board, the killfeed and
  // the winner banner. ~100 B at 4 Hz for the full roster.
  size_t ncnt_at = w.at;
  np_w8(&w, 0);
  int ncnt = 0;
  for (int h = 0; h < MAX_HUMANS; h++) {
    if (!g_humans_on[h]) continue;
    np_w8(&w, (uint8_t)ent_of_human(h));
    // Fixed 11 bytes, zero-filled past the NUL: g_human_name keeps the
    // previous occupant's tail bytes after a shorter name overwrites it, and
    // emitting the raw array leaked them to every client.
    int done = 0;
    for (int k = 0; k < 11; k++) {
      uint8_t ch = done ? 0 : (uint8_t)g_human_name[h][k];
      if (!ch) done = 1;
      np_w8(&w, ch);
    }
    ncnt++;
  }
  if (w.bad) return 0;
  buf[cnt_at] = (uint8_t)cnt;
  buf[ncnt_at] = (uint8_t)ncnt;
  return w.at;
}
// Hostile-input rules as everywhere: bounds-checked cursor, id rejected as an
// index, a truncated row list keeps what parsed. Returns rows, -1 on bad header.
typedef struct { uint8_t id; char name[12]; } net_name_t;
static int net_scores_read(const unsigned char *buf, size_t len,
                           uint8_t *over, uint8_t *winner,
                           uint16_t *frag, net_mode_t *mode,
                           net_score_t *out, int out_cap,
                           net_name_t *nout, int nout_cap,
                           int *nout_cnt) {
  np_rd r = {buf, len, 0, 0};
  if (np_r16(&r) != NET_PROTO) return -1;
  if (np_r8(&r) != NP_SCORES) return -1;
  uint8_t ov = np_r8(&r), wi = np_r8(&r);
  uint16_t fr = np_r16(&r);
  net_mode_t md;
  md.mode = np_r8(&r); md.round_secs = np_r8(&r); md.round_win = np_r8(&r);
  md.rounds = np_r8(&r); md.round_no = np_r8(&r);
  md.team_size = np_r8(&r); md.team_fill = np_r8(&r) & 3;
  md.tscore[0] = np_r16(&r); md.tscore[1] = np_r16(&r);
  if (md.mode >= GM_COUNT) return -1;        // hostile mode: refuse the packet
  if (md.team_size > TEAM_SIZE_MAX) md.team_size = TEAM_SIZE_MAX;
  if (md.round_win != 0xFF && md.round_win >= MAX_ENTS) return -1;
  int cnt = np_r8(&r);
  if (r.bad) return -1;
  if (cnt > out_cap) cnt = out_cap;
  int got = 0;
  for (int i = 0; i < cnt; i++) {
    net_score_t s;
    s.id = np_r8(&r);
    s.kills = np_r16(&r); s.deaths = np_r16(&r);
    s.shots = np_r16(&r); s.hits = np_r16(&r);
    s.ping = np_r8(&r);
    s.mscore = np_r8(&r);
    s.team = np_r8(&r) ? 1 : 0;
    if (r.bad) break;
    if (s.id >= MAX_ENTS) return -1;         // hostile id: refuse the packet
    out[got++] = s;
  }
  if (nout_cnt) *nout_cnt = 0;
  if (nout && nout_cap > 0) {
    int nc = np_r8(&r);
    int ng = 0;
    for (int i = 0; i < nc && ng < nout_cap; i++) {
      net_name_t nm = {0, {0}};
      nm.id = np_r8(&r);
      for (int k = 0; k < 11; k++) {
        char c = (char)np_r8(&r);
        nm.name[k] = c;                      // force-terminated by the {0} init
      }
      if (r.bad) break;
      if (nm.id >= MAX_ENTS) return -1;      // hostile id: refuse the packet
      nout[ng++] = nm;
    }
    if (nout_cnt) *nout_cnt = ng;
  }
  if (over) *over = ov;
  if (winner) *winner = wi;
  if (frag) *frag = fr;
  if (mode) *mode = md;
  return got;
}

// RESCUE(sibling): the mission as replicated STATE, 20 Hz, one build per
// arena. The client runs none of the mission (rescue_on is 0 there) and draws
// from this: the tracker, the meters, the hostages, the banner, the debrief.
// Positions are centimetres in i16 (the map is 104 m), meters are u8.
static void np_wstr(np_wr *w, const char *str, int n) {
  int done = 0;
  for (int k = 0; k < n; k++) {
    uint8_t ch = done ? 0 : (uint8_t)str[k];
    if (!ch) done = 1;
    np_w8(w, ch);
  }
}
static void np_rstr(np_rd *r, char *out, int n) {
  for (int k = 0; k < n; k++) {
    char c = (char)np_r8(r);
    out[k] = c >= 32 && c < 127 ? c : (c == 0 ? 0 : (c == 5 ? c : '?'));
  }
  out[n] = '\0';
}
static uint8_t np_u8c(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
static int16_t np_cm(float v) {
  float c = v * 100.0f;
  return (int16_t)(c > 32767.0f ? 32767 : c < -32767.0f ? -32767 : (int)lrintf(c));
}
static size_t net_rescue_build(unsigned char *buf, size_t cap) {
  np_wr w = {buf, cap, 0, 0};
  np_w16(&w, NET_PROTO);
  np_w8(&w, NP_RESCUE);
  np_w8(&w, (uint8_t)((g_resc.alarm ? 1 : 0) | (g_resc.ever_seen ? 2 : 0)));
  np_w8(&w, (uint8_t)g_resc.result);
  np_w8(&w, (uint8_t)g_resc.why);
  np_w8(&w, (uint8_t)g_resc.alarm_why);
  np_w16(&w, (uint16_t)(g_resc.exec_t < 0 ? 0 : g_resc.exec_t > 65535 ? 65535 : g_resc.exec_t));
  np_w16(&w, (uint16_t)(g_resc.exec_max > 65535 ? 65535 : g_resc.exec_max));
  np_w32(&w, (uint32_t)g_resc.clock);
  np_w32(&w, (uint32_t)g_resc.end_tick);
  np_w8(&w, np_u8c(g_resc.freed));
  np_w8(&w, np_u8c(g_resc.extracted));
  np_w8(&w, np_u8c(g_resc.kills));
  np_w8(&w, np_u8c(g_resc.takedowns));
  np_w8(&w, np_u8c(g_resc.heads));
  np_w8(&w, np_u8c(g_resc.spotted_n));
  np_w8(&w, np_u8c(g_resc.bodies));
  np_w8(&w, np_u8c(g_resc.nguards));
  np_w8(&w, np_u8c(g_resc.squad));
  np_w8(&w, np_u8c((int)(g_resc.rotor_w * 5.0f + 0.5f)));
  np_w8(&w, (uint8_t)(g_resc.msg_seq & 0xFF));
  np_w8(&w, (uint8_t)g_resc.msg_kind);
  np_w16(&w, (uint16_t)(g_resc.msg_t < 0 ? 0 : g_resc.msg_t));
  np_wstr(&w, g_resc.msg, 47);
  np_wstr(&w, g_resc.msg2, 47);
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    const hostage_t *h = &g_resc.h[i];
    np_w8(&w, (uint8_t)h->state);
    np_w8(&w, (uint8_t)(h->leader < 0 || h->leader >= MAX_ENTS ? 0xFF : h->leader));
    np_w16(&w, (uint16_t)np_cm(h->pos.x));
    np_w16(&w, (uint16_t)np_cm(h->pos.y));
    np_w16(&w, (uint16_t)np_cm(h->pos.z));
    np_w16(&w, (uint16_t)net_q_yaw(h->yaw));
    np_w8(&w, np_u8c((int)(h->free_t * 255.0f + 0.5f)));
    np_w8(&w, np_u8c((int)(h->pose * 255.0f + 0.5f)));
  }
  uint8_t lit = 0;
  for (int h = 0; h < MAX_HUMANS; h++)
    if (g_humans_on[h] && g_resc.lit_e[ent_of_human(h)]) lit |= (uint8_t)(1u << h);
  np_w8(&w, lit);
  for (int i = 0; i < g_resc.nguards; i++) {
    np_w8(&w, np_u8c((int)(g_resc.detect[i] * 255.0f + 0.5f)));
    int who = g_resc.det_who[i];
    np_w8(&w, (uint8_t)(who < 0 || who >= MAX_ENTS ? 0xFF : who));
  }
  return w.bad ? 0 : w.at;
}
// Hostile input as everywhere: every enum, id and count range-checked, and a
// packet that fails any of it changes nothing (it is read into a copy).
static int net_rescue_read(const unsigned char *buf, size_t len, rescue_t *o) {
  np_rd r = {buf, len, 0, 0};
  if (np_r16(&r) != NET_PROTO || np_r8(&r) != NP_RESCUE) return 0;
  int fl = np_r8(&r);
  o->alarm = fl & 1;
  o->ever_seen = (fl >> 1) & 1;
  o->result = np_r8(&r);
  o->why = np_r8(&r);
  o->alarm_why = np_r8(&r);
  o->exec_t = np_r16(&r);
  o->exec_max = np_r16(&r);
  o->clock = (int)(np_r32(&r) & 0x7FFFFFFF);
  o->end_tick = (int)(np_r32(&r) & 0x7FFFFFFF);
  o->freed = np_r8(&r);
  o->extracted = np_r8(&r);
  o->kills = np_r8(&r);
  o->takedowns = np_r8(&r);
  o->heads = np_r8(&r);
  o->spotted_n = np_r8(&r);
  o->bodies = np_r8(&r);
  o->nguards = np_r8(&r);
  o->squad = np_r8(&r);
  o->rotor_w = (float)np_r8(&r) / 5.0f;
  int seq = np_r8(&r);
  o->msg_kind = np_r8(&r);
  int mt = np_r16(&r);
  char m1[48], m2[48];
  np_rstr(&r, m1, 47);
  np_rstr(&r, m2, 47);
  if (r.bad || o->result > RR_FAIL || o->why > RF_EXECUTED || o->msg_kind > 3 ||
      o->nguards > MAX_BOTS || o->squad < 1 || o->squad > RESC_SQUAD_MAX ||
      o->freed > RESC_HOSTAGES || o->extracted > RESC_HOSTAGES)
    return 0;
  if ((seq & 0xFF) != (o->msg_seq & 0xFF)) {   // a new banner
    o->msg_seq = seq;
    memcpy(o->msg, m1, sizeof o->msg);
    memcpy(o->msg2, m2, sizeof o->msg2);
    o->msg_t = mt > 8 * TICK_HZ ? 8 * TICK_HZ : mt;
  }
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    hostage_t *h = &o->h[i];
    int st = np_r8(&r), ld = np_r8(&r);
    float x = (float)(int16_t)np_r16(&r) / 100.0f;
    float y = (float)(int16_t)np_r16(&r) / 100.0f;
    float z = (float)(int16_t)np_r16(&r) / 100.0f;
    float yw = net_dq_yaw((int16_t)np_r16(&r));
    float ft = (float)np_r8(&r) / 255.0f, ps = (float)np_r8(&r) / 255.0f;
    if (r.bad || st > HS_DEAD || (ld != 0xFF && ld >= MAX_ENTS)) return 0;
    h->state = st;
    h->leader = ld == 0xFF ? -1 : ld;
    h->net_pos = (v3){{x, y, z}};
    h->net_yaw = yw;
    h->free_t = ft;
    h->pose = ps;
  }
  int lit = np_r8(&r);
  int me = human_of_ent(g_local_ent);
  o->lit = me >= 0 ? (lit >> me) & 1 : 0;
  for (int i = 0; i < MAX_BOTS; i++) { o->detect[i] = 0.0f; o->det_who[i] = -1; }
  for (int i = 0; i < o->nguards; i++) {
    float d = (float)np_r8(&r) / 255.0f;
    int who = np_r8(&r);
    if (r.bad) return 0;
    o->detect[i] = d;
    o->det_who[i] = who == 0xFF || who >= MAX_ENTS ? -1 : who;
  }
  return !r.bad;
}
// A client takes the packet: a copy is decoded and committed whole, the
// presentation that the sim would have cued (chimes, siren, the shots) is
// cued from the edges between the old state and the new.
static void rescue_net_apply(const unsigned char *buf, size_t len) {
  static rescue_t tmp;
  tmp = g_resc;
  if (!g_resc.on) {   // the first packet of a mission: a clean slate
    memset(&tmp, 0, sizeof tmp);
    tmp.msg_seq = -1;
    for (int i = 0; i < RESC_HOSTAGES; i++) tmp.h[i].leader = -1;
  }
  if (!net_rescue_read(buf, len, &tmp)) return;
  int was_on = g_resc.on;
  if (was_on) {
    if (tmp.freed > g_resc.freed) tmp.sfx_pending |= 1;
    if (tmp.extracted > g_resc.extracted) tmp.sfx_pending |= 2;
    if (tmp.alarm && !g_resc.alarm) { tmp.cl_alarm_age = 0; tmp.siren_t = 0; }
    if (tmp.result == RR_FAIL && tmp.why == RF_EXECUTED && g_resc.result != RR_FAIL)
      tmp.sfx_pending |= 8;
    // A NEW mission (the server restarted it): the clock ran backwards.
    if (tmp.clock < g_resc.clock) { tmp.cl_alarm_age = 0; tmp.rotor = 0.0f; }
  }
  tmp.on = 1;
  for (int i = 0; i < RESC_HOSTAGES; i++) {
    hostage_t *h = &tmp.h[i];
    // First sight, a captive (they do not move) or a jump past 3 m: snap.
    if (!was_on || h->state == HS_CAPTIVE || v3_len(v3_sub(h->net_pos, h->pos)) > 3.0f) {
      h->pos = h->prev = h->net_pos;
      h->yaw = h->prev_yaw = h->net_yaw;
    }
  }
  g_resc = tmp;
}

// (Lag-comp history moved below ent_trace_box/bot_eye.)

// Random free floor spot far from every living combatant (excluding entity
// `self`, or nobody for self < 0), and OUT OF THEIR SIGHT.
// ENT_FOREACH (P1-55): counting one human made spawn placement blind to the
// other seven — a deterministic double spawn-kill on a shared tick.
//
// DISTANCE ALONE WAS NOT SAFETY. The old rule took the first spot 8 m from
// everyone, and 8 m is point-blank for a rifle: with 8 bots on arena and
// foundry (3 seeds each), 4.2% of bot deaths came within 1 s of a respawn and
// 0.7% within half a second — killed where they stood. Scored: 1.5% and 0.
// (`spawncheck`; the 3 s figure, 28.4% -> 21.8%, is mostly the arena's base
// death rate, not placement.)
// Every candidate is now SCORED: its distance to the nearest enemy, capped so
// that "far" stops mattering past SPAWN_FAR, minus a heavy penalty per living
// enemy with a clear line to the spawn's head height. A spot nobody can see
// and that clears SPAWN_FAR ends the search early.
#define SPAWN_TRIES   96
#define SPAWN_FAR     24.0f    // m: past this, distance earns nothing more
#define SPAWN_SEEN_PEN 20.0f   // m of score per enemy with a line to the spot
static v3 spawn_spot(int self) {
  int sh = human_of_ent(self);
  if (rescue_on() && resc_is_squad(self)) {
    // The insertion point: humans on the first places, squad bots after them.
    int k = sh >= 0 ? sh : RESC_SQUAD_MAX + (self - 1 - g_resc.nguards);
    return (v3){{RESC_INSERT_X + 1.5f * (float)(k % 6), 0.0f, RESC_INSERT_Z - 1.5f * (float)(k / 6)}};
  }
  // Fallback point PER HUMAN: one literal for all eight is a deterministic
  // stack of bodies on a full-arena tick. Human 0 keeps the historical 18.
  v3 best = {{0, 0, sh >= 0 ? 18.0f - 2.0f * (float)sh : -12.0f}};
  float best_s = -1e9f;
  for (int i = 0; i < SPAWN_TRIES; i++) {
    v3 c = {{rng_rangef(&g_rng_bot, -ARENA_HALF + 2, ARENA_HALF - 2), 0,
             rng_rangef(&g_rng_bot, -ARENA_HALF + 2, ARENA_HALF - 2)}};
    if (!aabb_fits(c, PLAYER_RADIUS, HEIGHT_STAND)) continue;
    float d = 1e9f;
    int seen = 0;
    v3 head = {{c.x, c.y + 1.5f, c.z}};
    ENT_FOREACH(e) {
      if (e == self) continue;
      if (team_mates(self, e)) continue;   // a teammate is no threat to a spawn
      int h = human_of_ent(e);
      if (h >= 0) { if (!g_players[h].alive) continue; }
      else if (!g_bots[e - 1].alive || !g_bots[e - 1].active) continue;
      v3 ep = ent_pos(e);
      float dx = c.x - ep.x, dz = c.z - ep.z;
      float dd = dx * dx + dz * dz;
      if (dd < d) d = dd;
      v3 eye = {{ep.x, ep.y + 1.5f, ep.z}};
      if (dd > 0.25f && los_clear(eye, head)) seen++;
    }
    float sc = fminf(sqrtf(d), SPAWN_FAR) - SPAWN_SEEN_PEN * (float)seen;
    // TEAM GROUND: ALPHA deploys on the -z half, BRAVO on the +z half, so a
    // side starts together and the fight has a front. Strong enough to decide
    // between safe spots, never enough to choose a seen one.
    if (teams_on() && self >= 0 && self < MAX_ENTS) {
      float side = g_team[self] ? 1.0f : -1.0f;
      sc += 14.0f * f_clamp(side * c.z / ARENA_HALF, -1.0f, 1.0f);
    }
    if (sc > best_s) { best_s = sc; best = c; }
    if (!seen && d >= SPAWN_FAR * SPAWN_FAR && !teams_on()) return c;   // unseen and far: done
  }
  return best;
}

static void rescue_spawn_guard(bot_t *b, int bi);
static void bot_spawn(int bi) {
  bot_t *b = &g_bots[bi];
  v3 spot = spawn_spot(bi + 1);   // entity id, not bot index
  // Script control survives death and match restart: a filmed battery that
  // crosses a respawn must not silently hand the testbed back to the AI. The
  // movement intent resets with the body; only the CONTROL MODE persists.
  int pup = b->puppet;
  float pup_speed = b->pup_speed;
  int arrived = b->flank_arrived;   // a CENSUS total, not per-life state: the
  memset(b, 0, sizeof *b);          // memset zeroing it made tacstat's per-tick
  b->puppet = pup;                  // difference go negative on every respawn,
  b->pup_speed = pup_speed;         // which cancelled the arrivals it had
  b->flank_arrived = arrived;       // already counted and printed a flat zero
  b->pos = b->prev_pos = b->target = spot;
  b->hp = PLAYER_HP;
  b->alive = 1;
  b->active = 1;
  b->grounded = 1;
  b->eye = EYE_STAND;
  b->yaw = b->prev_yaw = rng_rangef(&g_rng_bot, -3.14f, 3.14f);
  b->pup_face = b->yaw;  // a surviving puppet faces its spawn, not yaw 0
  b->belief = BOT_BELIEF_VISIBLE;
  b->tgt = -1;
  b->last_seen_vel = (v3){{0, 0, 0}};
  b->belief_pos = (v3){{0, 0, 0}};
  b->belief_radius = 0.0f;
  b->last_seen_tick = 0;
  b->search_pos = (v3){{0, 0, 0}};
  b->search_index = 0;
  b->search_dir = 1;
  b->search_t = 0;
  b->search_visited = 0;
  b->reacquire_count = 0;
  b->threat_src = -1;
  b->threat_t = 0;
  b->threat_score = 0;
  b->wp.cur = WPN_AR;
  wpn_fill_mags(&b->wp);
  mode_loadout(bi + 1, &b->wp);
  b->burst = 3;
  b->strafe_dir = (rng_next(&g_rng_bot) & 1) ? 1 : -1;
  g_spawn_seq[bi + 1]++;       // snapshot readers see a teleport, not motion
  anim_reset(&b->anim, b->pos, b->yaw);
  if (rescue_on() && bi < g_resc.nguards) rescue_spawn_guard(b, bi);   // a guard starts on its post
  else if (rescue_on()) {                        // a squad bot, on the insertion
    b->yaw = b->prev_yaw = b->pup_face = atan2f(-spot.x, spot.z);
    anim_reset(&b->anim, b->pos, b->yaw);
  }
}

static int bot_investigating(const bot_t *b) {
  return b->hear_tick && g_tick <= b->hear_until;
}

// Player-equivalent swap semantics for an AI request. The bot policy owns the
// rate limit; this helper is deliberately just an immediate request/transition.
static int bot_request_weapon(bot_t *b, int bi, int desired) {
  weapon_t *w = &b->wp;
  if (desired < 0 || desired >= WPN_COUNT || desired == w->cur || w->switch_t > 0)
    return 0;
  if (w->reload_t > 0)
    ev_push((event_t){.type = EV_RELOAD_CANCEL, .src = bi + 1, .a = b->pos,
                      .wpn = w->cur});
  w->cur = desired;
  w->switch_t = GUN_SWITCH_TICKS;
  w->reload_t = w->reload_tac = 0;
  w->melee_t = 0;
  w->dry_done = 0;
  w->idx = 0;
  // ammo[] and cool[] remain per weapon: holstering never refills/re-cocks it.
  return 1;
}

// The two distance bands deliberately differ: 24 m enters sniper preference,
// while only sub-16 m combat returns to AR. That gap stops boundary thrashing.
#define BOT_KNIFE_ENTER_DIST 2.3f
#define BOT_KNIFE_EXIT_DIST  4.0f
static int bot_desired_weapon(const bot_t *b, int engaging, float target_dist,
                              float horizontal_speed) {
  int cur = b->wp.cur;
  // Arm's length is knife range. Two bands, so a target on the line does not
  // flick the blade in and out.
  if (engaging && target_dist < (cur == WPN_KN ? BOT_KNIFE_EXIT_DIST : BOT_KNIFE_ENTER_DIST))
    return WPN_KN;
  // The sidearm and the blade are situational: back to a long gun as soon as
  // one can fire, and to the AR when nothing is loaded, because a bot reloads
  // only what it holds.
  if (cur == WPN_KN || cur == WPN_PT) {
    if (b->wp.ammo[WPN_AR] > 0) return WPN_AR;
    if (b->wp.ammo[WPN_SR] > 0) return WPN_SR;
    return cur == WPN_PT && engaging ? WPN_PT : WPN_AR;
  }
  int other = cur == WPN_AR ? WPN_SR : WPN_AR;
  if (engaging && b->wp.ammo[cur] <= 0 && b->wp.ammo[other] > 0) return other;
  if (engaging && b->wp.ammo[cur] <= 0 && b->wp.ammo[WPN_PT] > 0) return WPN_PT;
  // ...and only when there is nothing to shoot at. bot_investigating() is true
  // whenever a hear memory is live, and a gunshot is audible to 34 m, so in any
  // firefight this test used to run BEFORE the !engaging return and pinned an
  // engaging bot to the AR — the sniper preference was quietly suppressed by an
  // audio memory nothing was steering with.
  int investigating = !engaging && bot_investigating(b);
  if (investigating) {
    if (cur == WPN_SR && b->wp.ammo[WPN_AR] > 0) return WPN_AR;
    return cur;
  }
  if (!engaging) return cur;

  if (cur == WPN_AR) {
    if (b->wp.ammo[WPN_SR] > 0 && target_dist >= BOT_SNIPER_ENTER_DIST &&
        b->grounded && horizontal_speed < BOT_SNIPER_ENTER_SPEED && !b->sliding &&
        b->hurt_t == 0)
      return WPN_SR;
  } else if (b->wp.ammo[WPN_AR] > 0 &&
             (target_dist < BOT_AR_RETURN_DIST ||
              horizontal_speed > BOT_AR_RETURN_SPEED ||
              b->sliding || b->hurt_t > 0)) {
    return WPN_AR;
  }
  return cur;
}

// THE MODE OVERRULES THE PREFERENCE. bot_desired_weapon is the deathmatch
// policy and stays exactly that; a mode that narrows the arsenal re-picks here,
// from the weapons it allows, with the same distance sense the policy has.
#define BOT_NADE_MIN 6.0f    // closer than this a throw is a coin toss with
#define BOT_NADE_MAX 26.0f   // your own blast radius; past this, a guess
static int mode_bot_weapon(const bot_t *b, int bi, int desired, int engaging,
                           float tdist) {
  int ent = bi + 1;
  if (game_mode() == GM_BLADES) {
    int nade_ok = b->wp.ammo[WPN_GR] > 0 && b->wp.cool[WPN_GR] == 0;
    if (engaging && nade_ok && tdist >= BOT_NADE_MIN && tdist <= BOT_NADE_MAX)
      return WPN_GR;
    if (b->wp.cur == WPN_GR && (b->wp.gr_t > 0 || b->wp.gr_cook > 0)) return WPN_GR;
    return WPN_KN;
  }
  return mode_allowed(ent, desired) ? desired : mode_primary(ent);
}

// A BOT'S THROW. No cook and no lob: aim the low arc of a ballistic solution at
// the target's feet, at the overhand speed the player throws with, and set the
// fuse to land-plus-a-bounce. The error is the skill's aim error applied to the
// range, so an EASY bot's grenades land metres off and a HARD one's do not.
// Returns 1 when the solution exists (the arm has the reach).
static int bot_nade_solve(const bot_t *b, v3 tgt, float err_deg, v3 *at, v3 *vel,
                          int *fuse) {
  v3 o = {{b->pos.x, b->pos.y + bot_eye(b) + 0.10f, b->pos.z}};
  float dx = tgt.x - o.x, dz = tgt.z - o.z;
  float dh = sqrtf(dx * dx + dz * dz);
  if (dh < 0.5f) return 0;
  float miss = rng_rangef(&g_rng_bot, -1.0f, 1.0f) * err_deg * DEG2RAD;
  float ya = atan2f(dx, -dz) + miss;
  dh *= 1.0f + rng_rangef(&g_rng_bot, -1.0f, 1.0f) * err_deg * 0.04f;
  float dy = tgt.y - o.y, v = NADE_SPEED, g = NADE_GRAVITY;
  float disc = v * v * v * v - g * (g * dh * dh + 2.0f * dy * v * v);
  if (disc < 0.0f) return 0;
  float th = atanf((v * v - sqrtf(disc)) / (g * dh));
  float hv = v * cosf(th);
  *at = o;
  *vel = (v3){{sinf(ya) * hv, v * sinf(th), -cosf(ya) * hv}};
  int f = (int)((dh / fmaxf(hv, 0.1f) + 0.45f) * (float)TICK_HZ);
  *fuse = f < NADE_FUSE ? f : NADE_FUSE;
  return 1;
}

// ONE hitbox for every entity, keyed on the eye height: head straddling the
// neck, body below it. Two derivations of "the same" box had drifted apart — the
// player's head followed its eye while the bot's used its own constants, so at
// the same stance a crouched bot's head sat 10 cm higher than a crouched
// player's and was a measurably different target.
// *part is 3 for the head, 2 for the body; returns the hit distance or -1.
// `off` is the lateral displacement a lean has moved the upper body by. The
// head takes all of it and the torso box a bit over half — a lean bends the
// spine, so the shoulders travel nearly as far as the head and the boots not at
// all, and a box with its floor at the feet can only average that. It matters
// that this exists at all: a peek that moves the shot origin without moving the
// target is a free kill, and the whole appeal of leaning is that it is a TRADE.
static float ent_trace_box(v3 p, float eye, v3 off, v3 o, v3 d, int *part) {
  v3 n;
  float neck = p.y + eye - 0.12f;
  v3 hmn = {{p.x + off.x - 0.14f, neck, p.z + off.z - 0.14f}};
  v3 hmx = {{p.x + off.x + 0.14f, p.y + eye + 0.18f, p.z + off.z + 0.14f}};
  v3 bo = v3_scale(off, LEAN_BODY_K);
  v3 bmn = {{p.x + bo.x - 0.30f, p.y, p.z + bo.z - 0.22f}};
  v3 bmx = {{p.x + bo.x + 0.30f, neck, p.z + bo.z + 0.22f}};
  // A muzzle INSIDE the box still hits it. ray_aabb deliberately reports no hit
  // for an origin inside a solid (there is no sensible entry face, and the lean
  // probe and the third-person pivot cast both depend on that), but entities do
  // not collide with each other — move_axis tests only g_solids — so two players
  // can fully interpenetrate, and a point-blank shot then passed straight
  // through the target into the wall behind it. Head-before-body precedence is
  // unchanged: standing inside someone's head box is a headshot.
  #define IN_BOX(P, MN, MX) ((P).x >= (MN).x && (P).x <= (MX).x && \
                             (P).y >= (MN).y && (P).y <= (MX).y && \
                             (P).z >= (MN).z && (P).z <= (MX).z)
  float th = IN_BOX(o, hmn, hmx) ? 0.001f : ray_aabb(o, d, hmn, hmx, &n);
  float tb = IN_BOX(o, bmn, bmx) ? 0.001f : ray_aabb(o, d, bmn, bmx, &n);
  #undef IN_BOX
  if (th > 0 && (tb <= 0 || th <= tb)) { *part = 3; return th; }
  if (tb > 0) { *part = 2; return tb; }
  return -1.0f;
}

// A bot's stance picks its eye exactly like the player's does; the box above
// then makes crouching shrink the target for both, by construction.
// The EASED eye, not a step of b->crouch: the player's hitbox eases at
// EYE_LERP_RATE and the rendered figure eases its crouch too, so a stepped bot
// eye teleported the head/torso boxes 0.62 m the instant the AI flipped
// stance — for ~100-250 ms the visible head had no box behind it, and 1-3
// aimed AR rounds whiffed above the already-crouched hitbox. Same constant as
// the player's ease: one rate, entity parity by construction.
static float bot_eye(const bot_t *b) { return b->eye; }

// Lag-compensation history: the server rewinds OTHER entities to
// the moment the shooter's input tick was rendered on his screen. Each tick
// records the exact box arguments ent_trace_box takes (pos, eye, lean-derived
// offset, yaw). Write-only in singleplayer — nothing reads it there, so the
// byte gate is untouched — and the trace only ever runs on the server path.
#define NET_HIST 128                 // ~1 s at 120 Hz, > any sane rewind window
typedef struct { v3 pos; float eye, yaw, lean; unsigned char alive; } hist_t;
static hist_t g_hist_all[SRV_LOBBIES][MAX_ENTS][NET_HIST];
#define g_hist (g_hist_all[g_lobby])
static long   g_hist_tick = -1;      // sim tick of the newest slot

static void net_hist_record(void) {
  long slot = g_tick % NET_HIST;
  for (int e = 0; e < MAX_ENTS; e++) g_hist[e][slot] = (hist_t){0};
  ENT_FOREACH(e) {
    hist_t *h = &g_hist[e][slot];
    int hh = human_of_ent(e);
    if (hh >= 0) {
      const player_t *p = &g_players[hh];
      *h = (hist_t){p->pos, p->eye, p->yaw, p->lean, (unsigned char)p->alive};
    } else {
      const bot_t *b = &g_bots[e - 1];
      *h = (hist_t){b->pos, bot_eye(b), b->yaw, 0.0f, (unsigned char)b->alive};
    }
  }
  g_hist_tick = g_tick;
}

// Reconstruct one entity's box at a fractional past tick (t whole + frac in
// [0,1) toward t+1) and trace o->d against it. HARD-CLAMPED to the history
// depth(rule 2): a client cannot ask to rewind further than the server
// remembers. Interpolates between the two bracketing 120-Hz entries (rule 3:
// +/-4.2 ms is 5 cm at run speed, a fifth of a head box). Returns hit t or -1.
static float net_hist_trace(int e, long t, float frac, v3 o, v3 d, int *part) {
  if (g_hist_tick < 0) return -1.0f;
  long newest = g_hist_tick, oldest = newest - (NET_HIST - 1);
  // A session younger than the ring has no history before tick 0, and without
  // this the window's floor goes NEGATIVE: the clamp below then passes a
  // negative t straight through, and C's % keeps the sign, so the lookup
  // indexes before the array. Only reachable in the first NET_HIST-1 ticks —
  // which is exactly when a client joins.
  if (oldest < 0) oldest = 0;
  if (t < oldest) { t = oldest; frac = 0.0f; }
  if (t >= newest) { t = newest; frac = 0.0f; }
  const hist_t *a = &g_hist[e][t % NET_HIST];
  const hist_t *b = &g_hist[e][(t + 1) % NET_HIST];
  if (!a->alive) return -1.0f;
  v3 pos = v3_lerp(a->pos, b->alive ? b->pos : a->pos, frac);
  float eye = a->eye + (b->eye - a->eye) * frac;
  float yaw = a->yaw, lean = a->lean + (b->lean - a->lean) * frac;
  int hh = human_of_ent(e);
  v3 off = hh >= 0 ? lean_offset(yaw, lean) : (v3){{0, 0, 0}};
  return ent_trace_box(pos, eye, off, o, d, part);
}

// The lag-comp REWIND GATE. While a network human's move tick runs on the
// server, this holds the server tick his input was aimed at; ent_trace
// then sweeps the HISTORICAL boxes instead of the live ones, so the shot lands
// where the shooter saw his target, not where it stands one RTT later. -1
// everywhere else — singleplayer, bots, and the predicting client never rewind.
// The machinery (net_hist_trace) existed and was proven by `netlagcomp`, but
// nothing on the live shot path ever CALLED it: at 40 ms RTT a strafing
// target is ~2 body-widths past its rendered position, which reads as "my
// hits don't count".
static long g_lagcomp_view = -1;

static float bot_trace_one(const bot_t *b, v3 o, v3 d, int *part) {
  return ent_trace_box(b->pos, bot_eye(b), (v3){{0, 0, 0}}, o, d, part);
}

static float player_trace(const player_t *pl, v3 o, v3 d, int *part) {
  if (!pl->alive) return -1.0f;
  return ent_trace_box(pl->pos, pl->eye, lean_offset(pl->yaw, pl->lean), o, d, part);
}

// Nearest entity along the ray that is closer than *dist (pre-filled with the
// wall hit, so cover genuinely blocks shots). Returns the entity id or -1.
// THE headline sweep of P1-4: without the human range, humans are simply
// unshootable — rounds pass through, no marker, no damage, no error. Note
// `exclude`/the returned id are INDEX semantics, never g_local_ent: written
// as LOKAL, no human is hittable on a dedicated server at all.
static int ent_trace(int exclude, v3 o, v3 d, float *dist, int *part) {
  int hit = -1, pt;
  float t;
  ENT_FOREACH(e) {
    if (e == exclude) continue;
    if (g_lagcomp_view >= 0) {           // server, human shooter: rewound boxes
      if ((t = net_hist_trace(e, g_lagcomp_view, 0.0f, o, d, &pt)) > 0 &&
          t < *dist) {
        *dist = t; *part = pt; hit = e;
      }
      continue;
    }
    int h = human_of_ent(e);
    if (h >= 0) {   // player_trace gates on alive itself
      if ((t = player_trace(&g_players[h], o, d, &pt)) > 0 && t < *dist) {
        *dist = t; *part = pt; hit = e;
      }
    } else {
      if (!g_bots[e - 1].alive || !g_bots[e - 1].active) continue;
      if ((t = bot_trace_one(&g_bots[e - 1], o, d, &pt)) > 0 && t < *dist) {
        *dist = t; *part = pt; hit = e;
      }
    }
  }
  return hit;
}

static int ent_alive(int e) {
  int h = human_of_ent(e);
  if (h >= 0) return g_humans_on[h] && g_players[h].alive;
  return g_bots[e - 1].active && g_bots[e - 1].alive;
}

static v3 ent_aimpoint(int e) {  // upper chest
  int h = human_of_ent(e);
  if (h >= 0) {
    // EVERY human's aim point carries the lean offset, not only entity 0's —
    // a leaning human whose aim point stays centred is aim assist pulling
    // wide and bots aiming at where he is not.
    const player_t *pl = &g_players[h];
    v3 o = v3_scale(lean_offset(pl->yaw, pl->lean), LEAN_BODY_K);  // travels with the torso box
    return (v3){{pl->pos.x + o.x, pl->pos.y + pl->eye - ENT_CHEST_OFF, pl->pos.z + o.z}};
  }
  const bot_t *b = &g_bots[e - 1];
  return (v3){{b->pos.x, b->pos.y + bot_eye(b) - ENT_CHEST_OFF, b->pos.z}};
}

static int bot_threat_accept(const bot_t *b, int src, int score) {
  if (src < 0 || src >= MAX_ENTS || !ent_alive(src)) return 0;
  if (b->threat_t <= 0 || b->threat_src < 0 || !ent_alive(b->threat_src))
    return 1;
  if (score != b->threat_score) return score > b->threat_score;
  // threat_t is a bounded evidence age: an equal hit after at least one bot
  // tick is newer, while equal hits in one world tick resolve by entity ID.
  if (b->threat_t < BOT_THREAT_TICKS) return 1;
  return src < b->threat_src;
}

static void bot_focus_invalidate(bot_t *b) {
  // Every commitment that was planned around WHO the target is dies here, and
  // this is the one chokepoint all three "the target identity is changing"
  // sites go through (damage reveal, rescan switch, target lost). Releasing the
  // flank only in bot_belief_clear covered the third and missed the first two:
  // a bot mid-swing around A's last_seen that switched to B kept walking to A's
  // anchor for up to 3.8 s — measured 53-444 stale walk ticks per 24000 at
  // 8 bots hard — which is precisely the "it ran off to nowhere" tell
  // bot_flank_tick's own comment says the design prevents.
  if (b->flank_t) bot_flank_release(b);
  b->cov_valid = 0;
  b->in_cover = 0;
  b->search_pos = (v3){{0, 0, 0}};
  b->search_index = 0;
  b->search_t = 0;
  b->search_visited = 0;
}

// A FRESH SIGHTING, SEEDED IN ONE PLACE. Both paths that can produce one — eyes
// on the target, and taking a hit from somewhere — record the same eight fields,
// including the 0.20 m fresh-sighting radius and the ONE error-angle draw that
// decides which way this bot will be wrong once it loses sight.
// bot_belief_aimpoint is the single READER of that pair; this is the single
// writer. What the callers still own is the belief STATE and the search plan.
static void bot_belief_seed(bot_t *b, v3 pos, v3 vel) {
  b->last_seen = pos;
  b->last_seen_vel = vel;
  b->belief_pos = pos;
  b->belief_err_a = rng_rangef(&g_rng_bot, -F_PI, F_PI);
  b->belief_radius = 0.20f;
  b->last_seen_tick = g_tick;
  b->search_pos = pos;
  b->lose_t = 0;
}

static void bot_damage_remember(bot_t *b, int src) {
  const bot_skill_t *sk = &BOT_SKILL[g_cfg.sp_diff];
  v3 pos = ent_pos(src);
  b->belief = BOT_BELIEF_OCCLUDED_HOLD;
  bot_belief_seed(b, pos, (v3){{0, 0, 0}});
  b->search_index = 0;
  b->search_dir = 1;
  b->search_t = sk->hold_ticks;
  b->search_visited = 0;
}

static void bot_damage_reveal(bot_t *b, int src, int score) {
  const bot_skill_t *sk = &BOT_SKILL[g_cfg.sp_diff];
  // Hit by a teammate (friendly fire on): an accident, not an enemy to turn on.
  if (team_mates((int)(b - g_bots) + 1, src)) return;
  if (b->tgt != src) bot_focus_invalidate(b);
  b->tgt = src;
  bot_damage_remember(b, src);
  b->threat_src = src;
  b->threat_t = BOT_THREAT_TICKS;
  b->threat_score = score;
  b->err = sk->err0;
  b->react_t = (int)(sk->react_ms * 0.6f * (float)TICK_HZ / 1000.0f);
}

// Is the segment o->tp clear of world geometry? The 0.3 m slack — a target is a
// box, not a point, so a wall hugging it does not count as cover — lives here and
// nowhere else. It used to be restated at all three sites that decide whether a
// bot can SEE, can HIDE or can PEEK, which are the three halves of one tactic:
// three copies of the number that says how much of a body has to be behind a wall
// before the bot believes it. Callers own their own minimum-length gate, because
// each has a different one (a duel, a cover step, a peek probe).
#define LOS_SLACK 0.3f

// Any-hit occlusion test: TRUE the moment one solid blocks the segment, no
// nearest-hit bookkeeping. los_clear is a boolean question, and answering it
// with trace_world's full min-scan made LOS tracing 61% of the whole sim at 20
// bots (measured); the first blocking hit answers it identically
// (min-over-hits >= lim  <=>  no hit < lim). The GUN_RANGE guard mirrors
// trace_world's cap so the boolean stays exactly equal for ANY lim — even the
// out-of-arena lengths no current caller produces.
static int los_blocked(v3 o, v3 d, float lim) {
  if (lim > GUN_RANGE) return 1;
  // This loop is where the 20-bot sim spends most of its time (gprof: 11.9 of
  // 12.1 million ray_aabb calls arrive from here, ~23% of sim self-time), so
  // it looks like the obvious place to optimize. A segment-AABB pre-reject was
  // tried and REMOVED: it is exactly equivalent and it measured 417 ms against
  // 409 ms unmodified over ten interleaved 24000-tick runs, i.e. no win and
  // possibly a small loss. With ~30 solids, ray_aabb is already cheap enough
  // that six extra compares per solid cost more than the rejects save. If this
  // is ever revisited, the lever is the SOLID COUNT (a grid or a BVH), not the
  // per-solid test — and measure it interleaved, on an idle machine.
  // (Revisited for the tactical maps' thousands of solids: BP_RAY is that
  // grid. At the arena's count it is the plain loop.)
  BP_RAY(i, o, d, lim) {
    v3 n;
    float t = ray_aabb(o, d, g_solids[i].min, g_solids[i].max, &n);
    if (t > 0 && t < lim) return 1;
  }
  if (d.y < -1e-8f) {  // arena floor plane, exactly trace_world's test
    float t = -o.y / d.y;
    if (t > 0 && t < lim) {
      float hx = o.x + d.x * t, hz = o.z + d.z * t;
      if (hx > -ARENA_HALF && hx < ARENA_HALF && hz > -ARENA_HALF && hz < ARENA_HALF)
        return 1;
    }
  }
  return 0;
}

static int los_clear(v3 o, v3 tp) {
  v3 d = v3_sub(tp, o);
  float len = v3_len(d);
  return !los_blocked(o, v3_scale(d, 1.0f / len), len - LOS_SLACK);
}

// Line of sight from the bot's eye to the target's aim point. The range reject
// stays BEFORE the trace: it is one subtract against a couple of dozen ray casts.
static int bot_can_see(const bot_t *b, int e, float *dist) {
  v3 o = {{b->pos.x, b->pos.y + bot_eye(b), b->pos.z}};
  v3 tp = ent_aimpoint(e);
  float len = v3_len(v3_sub(tp, o));
  if (len < 0.01f || len > GUN_RANGE) return 0;
  if (!los_clear(o, tp)) return 0;
  *dist = len;
  return 1;
}

// Is `e` inside the horizontal arc `cos_lim` opens in front of the bot? Kept
// OUT of bot_can_see on purpose: that one answers "is the line clear", which is
// what the `lean` proof asks it and what a cover/peek probe means by it. This
// one answers "is it in front of my face", and the two gates are combined only
// where perception needs both. RETENTION calls it with BOT_KEEP_COS;
// ACQUISITION keeps its own inline dot product because the scan needs the
// `align` VALUE anyway (it feeds bot_target_score), so routing through here
// would compute the same two transcendentals twice to save nothing.
static int bot_in_arc(const bot_t *b, int e, float cos_lim) {
  v3 tp = ent_aimpoint(e);
  float dx = tp.x - b->pos.x, dz = tp.z - b->pos.z;
  float hl = sqrtf(dx * dx + dz * dz);
  if (hl <= 0.01f) return 1;   // on top of it: the arc has no meaning
  return (dx * sinf(b->yaw) + dz * -cosf(b->yaw)) / hl > cos_lim;
}

static void player_respawn(player_t *p, int ent) {
  int h = human_of_ent(ent);
  // A live-player restart (menu RESTART, harness `restart`) resets the weapon
  // below; the reload SOUND has to stop with it. (Death already cancelled.)
  if (p->wp.reload_t > 0)
    ev_push((event_t){.type = EV_RELOAD_CANCEL, .src = ent, .a = p->pos,
                      .wpn = p->wp.cur});
  int shots = p->shots;  // lifetime counter, part of the trace contract
  int cur = p->wp.cur;   // the weapon SELECTION is a player preference, not
                         // per-life state: dying on the sniper and coming
                         // back holding the AR overrides a choice the player
                         // already made (ammo, recoil and reload state are
                         // still fresh from player_spawn's reset)
  int ph[IN_COUNT];      // press-edge history crosses the spawn too: zeroed,
                         // a button held through death is a fresh press on
                         // the respawn tick (a held trigger fired the SR)
  memcpy(ph, p->prev_held, sizeof ph);
  player_spawn(p);
  p->shots = shots;
  p->wp.cur = cur;
  {
    int lo = human_loadout_src(h);
    if (lo >= 0) loadout_take(&p->wp, lo);
    p->wp.fresh = 1;
  }
  mode_loadout(ent, &p->wp);   // a preference the mode forbids is overruled
  memcpy(p->prev_held, ph, sizeof ph);
  v3 s = spawn_spot(ent);
  p->pos = p->prev_pos = s;
  p->yaw = atan2f(-s.x, s.z);  // face the arena center
  g_spawn_seq[ent]++;          // snapshot readers see a teleport, not motion
  anim_reset(&g_player_anim[h], p->pos, p->yaw);
  ev_push((event_t){.type = EV_RESPAWN, .dst = ent});
}

// WHAT KILLED THEM. A bullet or a blade is whatever the killer is holding, but a
// grenade lands after its thrower may have changed weapons — and GUN GAME's
// rules turn on the difference — so the blast says so itself for the duration of
// its damage loop. -1 = "read the killer's hands".
static int g_kill_wpn = -1;
static weapon_t *ent_weapon(int e);

static void match_end(int winner) {
  g_match.over = 1;
  g_match.winner = winner;
  ev_push((event_t){.type = EV_END, .k = winner});
}

static void rescue_on_kill(int src, int dst, int hs);
static void match_on_kill(int src, int dst, int hs) {
  // Blowing yourself up is a death, not a frag.
  if (src != dst) g_match.kills[src]++;
  g_match.deaths[dst]++;
  if (dst == g_local_ent) g_match.last_killer = src;
  ev_push((event_t){.type = EV_KILL, .src = src, .dst = dst, .k = hs});
  if (g_match.over) return;
  switch (game_mode()) {
    case GM_GUN: {
      int wpn = g_kill_wpn;
      if (wpn < 0) {
        const weapon_t *kw = ent_weapon(src);
        wpn = kw ? kw->cur : -1;
      }
      // A knife kill, or your own grenade, costs the victim a rung: back to
      // the START of the one below (or of the first).
      if (src == dst || wpn == WPN_KN) {
        int vl = gg_level(dst);
        g_match.mscore[dst] = vl > 0 ? (vl - 1) * GG_RUNG_KILLS : 0;
      }
      if (src == dst) break;
      int lvl = gg_level(src);
      if (wpn != GG_LADDER[lvl]) break;   // a stale grenade from the rung below
      if (lvl == GG_LEVELS - 1) { match_end(src); break; }
      g_match.mscore[src]++;
      // A promotion's weapon arrives loaded; the swap itself is the shared
      // gate in weapon_tick / bot_tick, so a live grenade finishes its throw.
      weapon_t *w = ent_weapon(src);
      if (w && gg_level(src) != lvl) wpn_fill_mags(w);
      break;
    }
    case GM_LMS:
      break;                              // rounds decide it (lms_tick)
    case GM_RESCUE:
      rescue_on_kill(src, dst, hs);       // the mission decides it (rescue_tick)
      break;
    case GM_TDM:
      // The SIDE scores. A suicide is nobody's frag; friendly fire is off, so
      // a teammate can never be the killer.
      if (src != dst && src >= 0 && src < MAX_ENTS && g_team[src] != g_team[dst]) {
        int t = g_team[src] ? 1 : 0;
        if (++g_team_score[t] >= sp_frag_limit()) match_end(MAX_ENTS + t);
      }
      break;
    default:
      if (g_match.kills[src] >= sp_frag_limit()) match_end(src);
  }
}

// One damage sink for every entity: flash/hp/score bookkeeping, kill + match
// end detection. `from` is the shooter's origin (drives the damage direction
// indicator and tells a shot-in-the-back bot where to turn); `at` is the world
// point the round actually landed on, which is what decides the death throw and
// where the hit sparks come off. Only `at` is passed, never the shot direction:
// `at == from + dir*dist` by construction at the one call site, so the direction
// is `normalize(at - from)` and a second copy of it in the argument list is a
// second copy of a number this file already has.
// THE LAUNCH, FOR WHOEVER JUST DIED. It reads nothing but the victim's place
// on the ground and the round that killed them, which is exactly why it can be
// shared: the player and a bot are the same kind of thing to the sim, and a
// player watching twenty enemies get flung while their own body blinks out of
// existence is that principle broken in the one view (`third_person`, and it
// works while dead) where it is visible.
static void rag_launch(rag_t *r, v3 pos, v3 vel, int grounded, v3 from, v3 at,
                       int dmg, int hs) {
  // Momentum from the shot: thrown along the BULLET — the real one, from the
  // muzzle to the point it landed on, so a round fired down from a crate
  // drives the body into the floor instead of sideways. Scaled by damage, so
  // an AR shoves and a sniper round throws, and capped, because "wuchtig" is
  // not "launched".
  v3 bd = v3_sub(at, from);
  float bl = v3_len(bd);
  bd = bl > 1e-4f ? v3_scale(bd, 1.0f / bl) : (v3){{0, 0, 1}};
  v3 d = {{bd.x, 0, bd.z}};
  float dl = sqrtf(d.x * d.x + d.z * d.z);
  // cos(shot pitch): how much of the round's momentum is HORIZONTAL at all.
  // It is read BEFORE the degenerate guard below, which exists only to keep
  // the normalisation finite — a shot fired straight down has dl = 0 and must
  // deliver no sideways throw, and the guard's 1.0 would have handed it the
  // full one in an arbitrary direction.
  float hk = dl;
  if (dl < 1e-3f) { d = (v3){{0, 0, 1}}; dl = 1.0f; }
  d.x /= dl;
  d.z /= dl;
  // Fall into open space: probe the shot direction for a body length of room
  // and swing to the freest side if it is up against a wall. A corpse that
  // topples INTO cover reads as a bug however correct its momentum was — and
  // the joint clamp that keeps it out of the geometry would then crush the
  // whole skeleton into a ball at the wall.
  v3 eo = {{pos.x, pos.y + 0.9f, pos.z}};
  v3 nn;
  v3 fd = d;
  float swing = 0.0f;   // how far the probe had to turn the fall, for the axis
  float room = trace_world(eo, d, &nn, NULL);
  if (room < 1.1f) {
    // Almost against the wall: swing the fall to the freest side, otherwise the
    // whole skeleton would end up inside geometry and the joint clamp would
    // crush it into a ball. Above that threshold the body is deliberately let
    // fly INTO the wall — hitting it and crumpling is the point.
    float best = room;
    for (int k = 1; k < 9; k++) {
      float ang = (float)((k + 1) / 2) * 0.62f * ((k & 1) ? 1.0f : -1.0f);
      v3 c = {{d.x * cosf(ang) + d.z * sinf(ang), 0,
               -d.x * sinf(ang) + d.z * cosf(ang)}};
      float t = trace_world(eo, c, &nn, NULL);
      if (t > best) { best = t; fd = c; swing = ang; }
      if (best > 3.3f) break;
    }
    room = best;
  }
  // Only a body with nowhere to go gets held back (`room` here is already the
  // freest direction found above). Scaling the throw down by the distance to
  // the wall was what made the slam unreachable: ground friction bleeds the
  // horizontal speed in about half a second, so a body that starts slow just
  // creeps into the wall instead of hitting it. Reaching the wall FAST is the
  // point; the per-joint clamp is what keeps the skeleton out of the geometry.
  float turned = f_clamp(fd.x * d.x + fd.z * d.z, 0.35f, 1.0f);
  // The cap goes OUTSIDE the headshot factor or it is not a cap: inside it, it
  // binds only above 227 damage and the ceiling in its comment is fiction.
  float imp = fminf((DEATH_IMP_BASE + (float)dmg * DEATH_IMP_K) *
                        (hs ? 1.20f : 1.0f), DEATH_IMP_MAX) *
              turned * f_clamp(room / 1.5f, 0.60f, 1.0f);
  // WHICH WAY A BODY GOES DOWN IS TORQUE, and torque is the one quantity the
  // old code threw away. r is the lever from the centre of mass to the point
  // the round landed on; F is the bullet. Its HORIZONTAL part tips the body
  // (a head hit is a metre of lever and whips it over, a gut hit is a hand's
  // width and it just crumples, a hit BELOW the CoM sweeps the legs and folds
  // it forward instead) and its VERTICAL part spins it about its own long
  // axis — which is the entire difference between a left and a right shoulder,
  // and is why those two used to be one animation to the last pixel.
  v3 lev = v3_sub(at, (v3){{pos.x, pos.y + DEATH_PIV_Y, pos.z}});
  v3 tau = v3_cross(lev, bd);          // per unit force: metres of lever
  float rw = DEATH_ROT_W(imp);         // ...and how hard the round pushed
  // The wall probe may have swung the FALL away from the bullet to find room;
  // the torque axis has to swing with it, or a corpse tips about an axis its
  // own travel is no longer perpendicular to. Same rotation formula as the
  // probe's, so "positive" means the same thing in both places.
  float cs = cosf(swing), sn = sinf(swing);
  v3 ta = {{tau.x * cs + tau.z * sn, 0.0f, -tau.x * sn + tau.z * cs}};
  float tl = sqrtf(ta.x * ta.x + ta.z * ta.z);
  if (tl < 1e-4f) {                     // straight through the pivot:
    ta = (v3){{fd.z, 0, -fd.x}};        // fall away from the shooter on the
    tl = 0.0f;                          // base spin alone
  } else {
    ta.x /= tl; ta.z /= tl;             // v3_rot_axis needs a UNIT axis
  }
  // The sign of the fall lives in the AXIS and never in the spin: the grounded
  // topple keeps ACCELERATING toward the nearest lying attitude, and a negative
  // rate would fight that integrator instead of feeding it.
  // THE BODY HAS TO LEAVE THE GROUND. The old lift ran 1.05-1.32 m/s, which
  // peaks at 9 cm — the corpse never actually broke contact, so the tumble
  // had nowhere to happen and every death was a slide. The launch is what
  // buys the half-second of air the rest of this reads in.
  float impk = imp / DEATH_IMP_MAX;
  *r = (rag_t){0};
  // THE VICTIM'S OWN MOMENTUM IS PART OF THE THROW. Everything above is the
  // bullet; a man running at 8 m/s who is shot does not stop and then get
  // pushed, he keeps going and tumbles. Starting the ragdoll at rest made
  // every death look identical whether the target was sprinting or standing
  // still, which is half the variety in a firefight thrown away for free.
  // DEATH_KEEP is under 1 because the legs stop carrying it the moment they
  // go slack — the trunk keeps roughly half.
  // The victim's OWN momentum is his already and arrives whole; only the
  // BULLET's share is spread, because that is the part that is supposed to look
  // like it happened to him rather than like something he was already doing.
  r->vel = (v3){{vel.x * DEATH_KEEP,
                  DEATH_LIFT + DEATH_LIFT_K * impk +
                      f_clamp(bd.y, -0.5f, 0.5f) * imp * DEATH_PITCH_K +
                      fmaxf(vel.y, 0.0f) * DEATH_KEEP,
                  vel.z * DEATH_KEEP}};
  r->push = (v3){{fd.x * imp * hk, 0.0f, fd.z * imp * hk}};
  r->push_t = DEATH_PUSH_T;
  // A hash of the impact point, mapped to [0, DEATH_REST_VAR) — NON-NEGATIVE
  // by design: allowed to go negative the offset pushes `rest` past the body's
  // own attitude, rag_rest answers with the other side and the corpse rocks
  // between the two forever. Deterministic (the same round always lands the
  // same body the same way) and different for every round. h >> 8 leaves the
  // full 24 bits skid_h's idiom expects: >> 9 left 23, so the quotient only
  // ever reached 0.5 and every corpse rest attitude lived in HALF the
  // documented 0-17 degree band (14 staged kills measured 0.030-0.137 rad,
  // none above 0.15).
  uint32_t h = (uint32_t)(int32_t)(at.x * 977.0f) * 2654435761u ^
               (uint32_t)(int32_t)(at.y * 991.0f) * 40503u ^
               (uint32_t)(int32_t)(at.z * 1009.0f) * 2246822519u;
  r->rest_off = (float)(h >> 8) / 16777216.0f * DEATH_REST_VAR;
  r->axis = ta;
  // Base + lever + LAUNCH. The launch term is what makes a flung body
  // cartwheel rather than sail: it is the one component that does not care
  // where the round landed, because being thrown is itself a rotation.
  r->spin = fminf(DEATH_SPIN_MAX,
                      (DEATH_SPIN_BASE + DEATH_SPIN_K * tl) * rw +
                          DEATH_AIR_SPIN * impk);
  r->yawv = f_clamp(tau.y * DEATH_YAW_K * rw, -DEATH_YAW_MAX, DEATH_YAW_MAX);
  r->grounded = grounded;
}

// TAKING A ROUND, for whoever took it. One copy, because the player's own body
// is a third-person figure like any other and "the player does not flinch" is a
// parity gap rather than a design decision — it was one, and it was invisible
// exactly where a player would see it (`view`, and the free camera).
// `at` is the world impact point and `from` the muzzle, which is everything the
// pose needs: the direction the round was travelling, how high up the body it
// landed, and how far off its centre line.
static void anim_hit(anim_t *a, v3 pos, float yaw, v3 from, v3 at, int hs) {
  v3 hd = v3_sub(at, from);
  hd.y = 0.0f;
  if (v3_dot(hd, hd) > 1e-6f) a->hit_d = v3_norm(hd);
  // Saturating rather than summing: a burst SUPPRESSES, it does not fold a man
  // in half. (Measured, four AR body shots ten ticks apart: 0.400 <-> 0.912 per
  // round, i.e. a real impulse each time.)
  a->hit_k = fminf(1.0f, a->hit_k + (hs ? 1.15f : 0.75f));
  // Normalised on the victim's CURRENT height, not HEIGHT_STAND: on a crouched
  // body (crown ~1.2 m) a helmet hit divided by 1.8 read as mid-chest and a
  // chest hit fell below skel_solve's 0.55 waist pivot — the hip-buckle branch
  // fired for a hit that should fold the trunk, i.e. the round landed on the
  // wrong half of the body, on the stance bots hold through every ranged duel.
  float eff_h = HEIGHT_STAND + (HEIGHT_CROUCH - HEIGHT_STAND) * a->crouch_s;
  a->hit_y = f_clamp((at.y - pos.y) / eff_h, 0.0f, 1.0f);
  // Lateral offset in the body's OWN right axis, normalised on a shoulder
  // half-breadth: that is the widest a hitbox admits, so +-1 is a hit on the
  // point of a shoulder and 0 is dead centre line.
  // THE LEG FRAME, not the aim frame. `skel_solve` builds its `R` from
  // `leg_yaw`, which trails the aim by up to ANIM_TWIST_MAX (63 degrees) — so a
  // shoulder hit taken with the hips turned rolled the body the wrong way.
  v3 rt = {{cosf(a->leg_yaw), 0.0f, sinf(a->leg_yaw)}};
  (void)yaw;
  v3 rel = v3_sub(at, pos);
  a->hit_x = f_clamp((rel.x * rt.x + rel.z * rt.z) / 0.26f, -1.0f, 1.0f);
  a->hit_w = 0.0f;
}

static void ent_damage(int ent, int dmg, int part, int src, v3 from, v3 at) {
  // A MENU IS NOT A MATCH — FOR THE PLAYER. The boot screen fires the weapon on
  // purpose and those rounds go wherever the arena happens to be, so nothing
  // may put the player on the floor of his own main menu.
  // THE BOTS ARE A DIFFERENT QUESTION, and blanket immunity was the wrong
  // answer to it: with nobody able to die, no engagement ever RESOLVES. A bot
  // that has found a target presses toward it forever, pins itself on the cover
  // in between and stands there firing at something it can never beat — which
  // from a camera is exactly the "lame, standing-still" backdrop the screen was
  // supposed to replace. Letting them kill each other gives the loop what it
  // actually wanted: deaths, respawns, fresh approaches, movement. The frag
  // limit is lifted to match (sp_frag_limit) so the match cannot end under the
  // menu, and the HUD is off, so no killfeed reports any of it.
  if (g_home && human_of_ent(ent) >= 0) return;
  // FRIENDLY FIRE IS THE HOST'S CHOICE, off by default: then a teammate's
  // round, blade or grenade does nothing and is not a hit on anyone's
  // accuracy either. Your own grenade still hurts you either way.
  if (team_mates(src, ent) && !team_ff()) return;
  if (rescue_on() && resc_is_guard(ent)) {
    if (resc_is_guard(src)) return;               // guards never hurt guards
    g_resc.detect[ent - 1] = 1.0f;                // ...and a hit guard knows
    g_resc.det_who[ent - 1] = src;
    g_resc.glimpse[ent - 1] = ent_pos(src);
  }
  int hs = part == 3;
  g_match.hits[src]++;
  // The HUMAN branch (three-range rule). Without it a human id above MAX_BOTS
  // falls through to the bot branch below and indexes g_bots[ent - 1] — an
  // out-of-bounds WRITE, before any EV_HIT is ever pushed.
  int vh = human_of_ent(ent);
  if (vh >= 0) {
    player_t *pl = &g_players[vh];
    pl->hp -= dmg;
    if (pl->hp < 0) pl->hp = 0;
    // `a` is the IMPACT POINT, which is what the event header has always
    // documented and what the VFX layer needs to put sparks on the wound. It
    // used to be the victim's feet; the only other reader is the hurt sound's
    // pan, which now answers "which side did that come in on" instead of
    // "where are my boots" (a point 1.6 m under the listener, i.e. pan 0).
    ev_push((event_t){.type = hs ? EV_HIT_HEAD : EV_HIT, .a = at, .b = from,
                      .i = dmg, .k = pl->hp, .src = src, .dst = ent});
    // The player's third-person body flinches on the same helper a bot's does.
    // Inside the survival gate for the same reason it is on a bot: a killing
    // round can never be seen to flinch, and the impulse would just sit on the
    // pose the corpse pool is handed.
    pl->flash = BOT_FLASH_TICKS;
    pl->flash_head = hs;
    if (pl->hp > 0) anim_hit(&g_player_anim[vh], pl->pos, pl->yaw, from, at, hs);
    if (pl->hp == 0 && pl->alive) {
      pl->alive = 0;
      pl->respawn_t = PLAYER_RESPAWN_TICKS;
      // Same launch a bot gets, from the same helper and the same round.
      rag_launch(&g_player_rag[vh], pl->pos, pl->vel, pl->grounded, from, at, dmg, hs);
      g_player_corpse_q[vh] = 1;
      // Death abandons a running reload the same way a swap does, and for the
      // same reason: the buffer would announce A LIVE WEAPON over a corpse.
      if (pl->wp.reload_t > 0) {
        ev_push((event_t){.type = EV_RELOAD_CANCEL, .src = ent, .a = pl->pos,
                          .wpn = pl->wp.cur});
        pl->wp.reload_t = 0;
      }
      match_on_kill(src, ent, hs);   // ent, not literal 0: the victim's id
    }
    return;
  }
  bot_t *b = &g_bots[ent - 1];
  b->hp -= dmg;
  if (b->hp < 0) b->hp = 0;
  b->flash = BOT_FLASH_TICKS;
  b->flash_head = hs;
  ev_push((event_t){.type = hs ? EV_HIT_HEAD : EV_HIT, .a = at, .b = from,
                    .i = dmg, .k = b->hp, .src = src, .dst = ent});
  if (b->hp > 0) {
    // The body flinches AWAY from the shooter, so the round's own direction of
    // travel is the pose input — available here for free and nowhere
    // downstream, which is why it is latched rather than derived. Saturating
    // rather than summing: a burst suppresses (measured, four AR body shots ten
    // ticks apart: 0.400 <-> 0.912 per round, a real impulse each time), it does
    // not fold a man in half.
    // INSIDE the survival gate, because a KILLING round can never be seen to
    // flinch and was measured not to: bot_tick's dead branch returns before
    // anim_tick, so the pose is never re-solved after the kill and the impulse
    // just sits on the corpse until the respawn memset. (The sniper one-shots a
    // full-health bot, so on that weapon EVERY hit takes this exit — what sells
    // its kill is the death momentum applied below, not a flinch.) Left outside
    // the gate it is dead state that turns into a real defect the moment anyone
    // makes the dead branch tick the animation.
    anim_hit(&b->anim, b->pos, b->yaw, from, at, hs);
    // Flinch: being shot at throws the aim off and briefly makes the bot want to
    // break the line rather than stand and trade. Suppression is a real tactic
    // against a human and it should be one against a bot.
    b->hurt_t = 150;
    b->err += 3.0f;
    b->fl_y += rng_rangef(&g_rng_bot, -2.6f, 2.6f);
    b->fl_p += rng_rangef(&g_rng_bot, -1.4f, 2.2f);
  }
  if (b->hp > 0) {  // getting shot reveals the shooter
    // The shooter's FEET, not `from` (the shot's eye-height origin): last_seen
    // is a feet position everywhere else, and the pre-aim adds chest height on
    // top of it — storing the eye point made a bot revealed by an unseen
    // shooter pre-aim ~1.6 m over their head until first LOS corrected it.
    int score = dmg * 16 + (hs ? 8 : 0);
    if (bot_threat_accept(b, src, score))
      bot_damage_reveal(b, src, score);  // accepted evidence owns focus + priority
    else if (b->tgt == src)
      bot_damage_remember(b, src);       // refresh location, never weaker evidence
  }
  // ... && alive, like the human branch above: lag comp legitimately delivers
  // rewound rounds to a bot that died this same window, and an unguarded death
  // branch re-launched the ragdoll and credited a SECOND kill per corpse hit.
  if (b->hp == 0 && b->alive) {
    b->alive = 0;
    b->topple = 0;
    b->respawn_t = PLAYER_RESPAWN_TICKS;
    // Tactical state does not survive the body. The dead branch of `bot_tick`
    // returns before any of it is ticked, so a bot killed mid-slide would keep
    // `sliding` set until it respawned and report a 300-tick "slide" that is
    // really a corpse lying still.
    b->sliding = 0;
    b->crouch = 0;
    b->crouch_t = 0;
    rag_launch(&b->rag, b->pos, b->vel, b->grounded, from, at, dmg, hs);
    match_on_kill(src, ent, hs);
  }
}

// Spread for WHOEVER is shooting. Keyed on the shooter's weapon state and
// motion, never on which kind of entity it is: a running bot's shots bloom the
// same way a running player's do, and one config slider moves both.
static float gun_spread(const weapon_t *w, v3 vel, int grounded) {
  float hs = sqrtf(vel.x * vel.x + vel.z * vel.z);
  // ADS spread is the tuned base; the structural hip penalty fades out as
  // the sight comes up. Movement and air penalties on top.
  float fb[WP_COUNT];
  float s = gun_feel(w, w->cur, fb)[WP_SPREAD] + wdef(w, w->cur)->hip_spread * (1.0f - w->ads_s)
          + w->bloom + GUN_MOVE_SPREAD * f_clamp(hs / g_sim_mv[MV_RUN], 0.0f, 1.5f);
  if (!grounded) s *= GUN_AIR_SPREAD;
  return s < 6.0f ? s : 6.0f;
}

static float weapon_spread(const player_t *p) {
  return gun_spread(&p->wp, p->vel, p->grounded);
}

// ONE shot, for whoever pulled the trigger. Ammo, fire rate, spread, bloom,
// recoil climb and pattern, damage, hitscan and events all live here and read
// the weapon table and the LIVE config — so a developer dragging a slider moves
// the player and all 20 bots by the same amount, in the same tick. `rng` is the
// caller's stream (gun for the player, bot for bots) so their spray patterns
// stay independently reproducible.
