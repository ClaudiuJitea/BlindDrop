// code/net/conn.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Live connection layer — SHARED. The server accepts clients,
// applies their inputs and streams snapshots; the client runs the connection
// automaton, predicts its own motion and interpolates the others. Only the
// socket calls (net_udp_*) are per-platform; this whole file is testable on
// 127.0.0.1 with `--server` in one process and `netclient` in another.
// ---------------------------------------------------------------------------

// --- server side ---
typedef struct {
  int      used;
  uint32_t ip; uint16_t port;
  int      human;                 // human slot, 1..MAX_HUMANS-1 (a conn only
                                  // exists fully joined — net_srv_admit
                                  // assigns the slot before the conn)
  uint32_t token;                 // session token, issued in ACCEPT and echoed
                                  // on every INPUT/LEAVE: an unauthenticated
                                  // LEAVE was a 3-byte spoofable kick
  uint32_t last_recv_ms;          // idle-timeout clock
  uint32_t last_input_tick;       // tag the snapshot with the last input applied
  uint32_t ev_high;               // highest event id this client has acked
  // De-jitter input queue. The client predicts one move per input tick;
  // the server must CONSUME one input per world tick or the two simulations
  // count moves differently under any jitter ("apply the newest per packet"
  // collapsed a press+release drained in one poll into nothing — a lost fire
  // tap, a missed jump). Slot = tick % NET_IN_MAX, validated by the stored
  // tick, so a stale entry reads as a gap, never as a wrong input.
  net_input_t inq[NET_IN_MAX];
  uint8_t     inq_set[NET_IN_MAX];
  uint32_t    in_next;            // next input tick to consume
  uint32_t    in_high;            // highest input tick buffered (backlog gauge)
  int         in_started;         // first input adopted the client's tick clock
  uint32_t    btn_carry;          // buttons of backlog-dropped ticks, OR-ed
                                  // into the next played one so a press EDGE
                                  // inside a dropped tick still fires
  int         rtt_ms;             // smoothed g_tick - input.view at consume
                                  // time: the whole loop the player
                                  // feels, and the ping the scoreboard shows.
                                  // 0 = not measured yet
} net_conn_t;
static net_conn_t g_conns_all[SRV_LOBBIES][MAX_HUMANS];
#define g_conns (g_conns_all[g_lobby])
static int g_srv_fd = -1;
static int g_srv_in_budget_all[SRV_LOBBIES][MAX_HUMANS];  // input ticks/s per slot
#define g_srv_in_budget (g_srv_in_budget_all[g_lobby])
static int g_srv_catchup;                // the pacing loop is BEHIND its clock:
                                         // the queued inputs are about to be
                                         // replayed one per catch-up tick, so
                                         // the backlog drop must not eat them

// Insert one decoded input record into a connection's queue. Returns 1
// when the record became a FRESH buffered tick; redundancy duplicates and
// already-consumed ticks are free, an over-budget or unrecoverably skipped
// tick counts into g_srv_dropped_inputs. This is the WHOLE admission policy —
// `netstall` drives exactly this function plus the consume step.
static int net_srv_queue_input(net_conn_t *c, int h, const net_input_t *in) {
  if (!c->in_started) {                   // adopt the client's tick clock
    c->in_started = 1;
    c->in_next = in->tick;
    c->in_high = in->tick - 1;
  }
  int32_t d = (int32_t)(in->tick - c->in_next);
  if (d < 0) return 0;                    // already consumed: redundancy dup
  if (d >= NET_IN_MAX) {                  // far ahead (stall/loss burst):
    uint32_t skip = in->tick - (NET_IN_MAX - 1) - c->in_next;
    // The COUNTED skip is clamped: `skip` is attacker-controlled through the
    // record's tick field, and one crafted jump made the digest's dropped_in
    // garbage (with a second jump overflowing the int — UB). The queue's own
    // in_next still moves the full distance; only the statistic saturates.
    g_srv_dropped_inputs += skip > NET_IN_MAX ? NET_IN_MAX : (unsigned)skip;
    c->in_next += skip;                   // stale slots fail the tick check
    if (g_net_debug)
      printf("netdbg skip human=%d ticks=%u to=%u\n", h, skip, c->in_next);
  }
  int slot = (int)(in->tick % NET_IN_MAX);
  if (c->inq_set[slot] && c->inq[slot].tick == in->tick) return 0;      // dup
  if (g_srv_in_budget[h] >= TICK_HZ * NET_IN_RATE_NUM / NET_IN_RATE_DEN) {
    g_srv_dropped_inputs++;               // rate budget spent this second
    return 0;
  }
  c->inq[slot] = *in;
  c->inq_set[slot] = 1;
  if ((int32_t)(in->tick - c->in_high) > 0) c->in_high = in->tick;
  g_srv_in_budget[h]++;
  return 1;
}

static int net_conn_find(uint32_t ip, uint16_t port) {
  for (int i = 0; i < MAX_HUMANS; i++)
    if (g_conns[i].used && g_conns[i].ip == ip && g_conns[i].port == port) return i;
  return -1;
}

// Buffered-not-yet-played input ticks for one conn — the snapshot's
// qdepth byte and the digest's qd column are the SAME number through this one
// helper. 0 until the client's clock is adopted: before in_started both
// counters are zero and the raw expression read a phantom depth of 1.
static int32_t net_conn_qdepth(const net_conn_t *c) {
  if (!c->in_started) return 0;
  int32_t qd = (int32_t)(c->in_high - c->in_next) + 1;
  return qd < 0 ? 0 : qd;
}

// Not crypto — off-path spoof protection. An on-path attacker can already do
// worse than kick a player.
static uint32_t net_srv_new_token(uint32_t ip, uint16_t port) {
  static uint32_t ctr;
  uint32_t t = (net_now_ms() * 2654435761u) ^ (ip * 0x9e3779b9u) ^
               ((uint32_t)port << 16) ^ ++ctr;
  return t ? t : 1;
}

// The bot target is a pure function of the humans present: bots pad the
// arena up to cap_public, at most cap_bots of them. A joining human therefore
// displaces a bot without anyone deciding to — the target simply shrinks.
// match_tick reads sp_bots each tick, so this is all it takes; the surplus bot
// retires at the next fill pass, i.e. at its next respawn.
static void net_srv_rebalance(void) {
  int humans = 0;
  for (int h = 0; h < MAX_HUMANS; h++) if (g_humans_on[h]) humans++;
  g_cfg.sp_bots = net_bot_target(humans, g_srv.cap_public, g_srv.cap_bots);
}

// Everything that makes a human slot THIS player's: score row, display name,
// player struct, input residue, gun-RNG stream, spawn. ONE copy — a fresh
// admit and a re-JOIN on a live conn (a new session from the same address)
// are the same identity reset; the re-JOIN branch used to re-arm only the
// transport, so the new session inherited the old one's kills and name.
static void net_srv_slot_reset(int h, const char *name) {
  int e = ent_of_human(h);
  g_match.kills[e] = g_match.deaths[e] = g_match.shots[e] = g_match.hits[e] = 0;
  int n = 0;
  for (const char *q = name; *q && n < (int)sizeof g_human_name[h] - 1; q++) {
    char g = name_glyph(*q);
    if (g) g_human_name[h][n++] = g;
  }
  g_human_name[h][n] = '\0';
  if (!n) snprintf(g_human_name[h], sizeof g_human_name[h], "PLAYER%d", h);
  memset(&g_players[h], 0, sizeof g_players[h]);
  // CLEAR THE SLOT'S INPUT RESIDUE. g_human_held/wish/move belong to the
  // PREVIOUS occupant of this slot until the new client's first input is
  // consumed — with FIRE still latched, the fresh spawn shot one silent round
  // (its score row opened shots=1) and could walk on stale movement for the
  // first RTT. The move gate closes until the queue actually feeds a tick.
  memset(g_human_held[h], 0, sizeof g_human_held[h]);
  g_human_loadout[h] = 0;
  g_human_wish[h][0] = g_human_wish[h][1] = 0.0f;
  g_human_move[h] = 0;
  g_human_view[h] = -1;
  g_rng_gun[h] = rng_gun_seed(g_map_seed, h);   // fresh joiner, fresh stream —
                                                // mirrored client-side at sync
  player_spawn(&g_players[h]);
  player_respawn(&g_players[h], e);
}

// Assign a free human slot to a newly joined client, honouring the fill caps
//. Returns the human slot, or -1 if the instance is full.
static int net_srv_admit(uint32_t ip, uint16_t port, const char *name) {
  int humans = 0;
  for (int h = 0; h < MAX_HUMANS; h++) if (g_humans_on[h]) humans++;
  // ONE HOST MAY NOT BE THE WHOLE ARENA. Nothing above bounds joins per source
  // address, so a single machine could take every human slot with one packet
  // each and lock a public instance out — the cookie proves the address is
  // reachable, not that it is a different person. NAT legitimately puts a few
  // players behind one address, which is what the allowance is for.
  int same = 0;
  for (int i = 0; i < MAX_HUMANS; i++)
    if (g_conns[i].used && g_conns[i].ip == ip) same++;
  if (same >= NET_JOIN_PER_HOST) return -1;
  if (!net_join_ok(humans, g_srv.cap_public))
    return -1;
  int h = -1;
  for (int i = 1; i < MAX_HUMANS; i++) if (!g_humans_on[i]) { h = i; break; }
  if (h < 0) return -1;
  g_humans_on[h] = 1;
  // A side before the spawn (spawn_spot deploys on it): the emptier one. The
  // player can change it from the TEAM page that opens on join.
  if (teams_on()) { g_team[ent_of_human(h)] = -1; g_team[ent_of_human(h)] = (signed char)team_auto(); }
  net_srv_rebalance();               // fighter count stays constant
  net_srv_slot_reset(h, name);
  int conn = -1;
  for (int i = 0; i < MAX_HUMANS; i++) if (!g_conns[i].used) { conn = i; break; }
  if (conn < 0) { g_humans_on[h] = 0; return -1; }
  g_conns[conn] = (net_conn_t){.used = 1, .ip = ip, .port = port, .human = h,
                               .token = net_srv_new_token(ip, port),
                               .last_recv_ms = net_now_ms(),
                               // Start the event ack AT the present: events
                               // pushed before this client existed are not its
                               // history — without this a join within the
                               // resend window replayed the last ~300 ms of
                               // someone else's gunfire into the fresh client.
                               .ev_high = g_srv_ev_next - 1};
  return conn;
}

static void net_srv_drop(int conn) {
  if (!g_conns[conn].used) return;
  int h = g_conns[conn].human;
  if (h > 0) {                         // free the slot and clear the score row
    g_humans_on[h] = 0;
    g_human_view[h] = -1;              // no conn, no lag-comp anchor
    int e = ent_of_human(h);
    g_match.kills[e] = g_match.deaths[e] = g_match.shots[e] = g_match.hits[e] = 0;
    net_srv_rebalance();               // a bot fills the freed slot
  }
  g_conns[conn] = (net_conn_t){0};
}

// Handle one received packet on the server socket. `epoch` is unix_time/30.
static void net_srv_packet(const unsigned char *b, int n, uint32_t ip, uint16_t port,
                           uint32_t epoch) {
  if (n < 3) return;
  np_rd r = {b, (size_t)n, 0, 0};
  if (np_r16(&r) != NET_PROTO) {            // wrong protocol: REJECT — but only
    // to a packet at least as big as the reply: a real old client's
    // CONNECT is NET_CONNECT_MIN-padded, while a 3-byte probe with a forged
    // source must not buy a 4-byte reflection.
    if (n < 4) return;
    unsigned char rej[4]; np_wr w = {rej, sizeof rej, 0, 0};
    np_w16(&w, NET_PROTO); np_w8(&w, NP_REJECT); np_w8(&w, NR_PROTO);
    net_udp_send(g_srv_fd, ip, port, rej, (int)w.at);
    return;
  }
  int type = np_r8(&r);
  if (type == NP_CONNECT) {
    // THE REPLY MUST BE SMALLER THAN THE REQUEST, and that only holds if the
    // request has a floor. A bare CONNECT is 3 bytes (proto + type, empty
    // name) against a 7-byte COOKIE — a 2.3x amplifier pointed at whatever
    // source address the attacker forged. The client pads to
    // NET_CONNECT_MIN; anything shorter is not a client of ours and is
    // dropped in silence, which costs a spoofer the whole reflection.
    if (n < NET_CONNECT_MIN) return;
    unsigned char out[8]; np_wr w = {out, sizeof out, 0, 0};
    np_w16(&w, NET_PROTO); np_w8(&w, NP_COOKIE);
    np_w32(&w, net_cookie(ip, port, epoch));
    net_udp_send(g_srv_fd, ip, port, out, (int)w.at);
  } else if (type == NP_JOIN) {
    uint32_t cookie = np_r32(&r);
    if (!net_cookie_valid(ip, port, cookie, epoch)) return;   // no valid cookie, no world
    char name[12] = {0};
    for (int i = 0; i < 11; i++) { char c = (char)np_r8(&r); if (!c) break; name[i] = c; }
    int conn = net_conn_find(ip, port);
    if (conn >= 0) {
      // A JOIN on a LIVE conn is a client announcing a fresh session from the
      // same address (process restart behind a reused NAT mapping). Its tick
      // counter restarted lower, so the old in_next made every input read as
      // an already-consumed dup — the player froze while his packets kept
      // refreshing the idle timer. Re-arm the queue and the event ack; the
      // next input re-adopts the client's clock. Cookie-gated, so only the
      // address owner can trigger it.
      net_conn_t *c = &g_conns[conn];
      c->in_started = 0;
      memset(c->inq_set, 0, sizeof c->inq_set);
      c->btn_carry = 0;
      c->rtt_ms = 0;
      c->ev_high = g_srv_ev_next - 1;
      // A fresh session is a fresh IDENTITY too: same reset a new admit gets
      // (score row, name, spawn, gun-RNG) and a fresh token — the old
      // session's kills carried over to whoever re-joined the slot.
      c->token = net_srv_new_token(ip, port);
      net_srv_slot_reset(c->human, name);
    }
    if (conn < 0) conn = net_srv_admit(ip, port, name);
    if (conn < 0) {                         // instance full
      unsigned char rej[4]; np_wr w = {rej, sizeof rej, 0, 0};
      np_w16(&w, NET_PROTO); np_w8(&w, NP_REJECT); np_w8(&w, NR_FULL);
      net_udp_send(g_srv_fd, ip, port, rej, (int)w.at);
      return;
    }
    g_conns[conn].last_recv_ms = net_now_ms();
    // ACCEPT carries the match's SIM TUNING(light): the client must
    // predict with the SERVER's movement and weapon tables, not its own
    // preferences — a differing mv_run is a permanent standing rubber-band.
    // Floats travel as raw IEEE bits (np_wf/np_rf): quantizing a tunable the
    // two sims multiply with every tick would rebuild the very divergence
    // this exists to remove.
    // SIZED FOR THE WHOLE TABLE: 19 bytes of header, MV_COUNT + WPN_COUNT x
    // WP_COUNT floats. It was 168 from the two-weapon days, so with five
    // weapons the writer ran out partway through the pistol's row and every
    // client predicted the pistol, knife and grenade on each tunable's LOW
    // bound (np_rf reads 0 past the end).
    unsigned char out[19 + 4 * (MV_COUNT + WPN_COUNT * WP_COUNT)];
    np_wr w = {out, sizeof out, 0, 0};
    np_w16(&w, NET_PROTO); np_w8(&w, NP_ACCEPT);
    np_w8(&w, (uint8_t)ent_of_human(g_conns[conn].human));
    np_w8(&w, (uint8_t)g_lobby);          // WHICH arena, and
    np_w8(&w, (uint8_t)g_srv.lobbies);    // how many this process runs
    np_w32(&w, g_map_seed);
    np_w8(&w, (uint8_t)g_map_id);
    np_w32(&w, net_world_hash());
    np_w32(&w, g_conns[conn].token);
    for (int i = 0; i < MV_COUNT; i++) np_wf(&w, g_cfg.mv[i]);
    for (int wp = 0; wp < WPN_COUNT; wp++)
      for (int i = 0; i < WP_COUNT; i++) np_wf(&w, g_cfg.wp[wp][i]);
    if (w.bad) slog("ACCEPT overflow at %d of %d bytes", (int)w.at, (int)sizeof out);
    net_udp_send(g_srv_fd, ip, port, out, (int)w.at);
  } else if (type == NP_TEAM) {
    int conn = net_conn_find(ip, port);     // token-gated, like LEAVE
    if (conn < 0 || np_r32(&r) != g_conns[conn].token) return;
    int t = np_r8(&r);
    if (r.bad || t > 1 || !teams_on()) return;
    int h = g_conns[conn].human, e = ent_of_human(h);
    if (g_team[e] == t || team_humans(t) >= team_size()) return;   // same, or full
    g_team[e] = (signed char)t;
    // Changing sides is a redeploy on the new side's ground, not a death.
    player_respawn(&g_players[h], e);
    g_conns[conn].last_recv_ms = net_now_ms();
  } else if (type == NP_LEAVE) {
    int conn = net_conn_find(ip, port);     // token-gated: a bare LEAVE
    if (conn >= 0 && np_r32(&r) == g_conns[conn].token)
      net_srv_drop(conn);                   // same teardown as the idle timeout
  } else if (type == NP_INPUT) {
    int conn = net_conn_find(ip, port);
    if (conn < 0) return;                   // unknown source: ignore
    net_conn_t *c = &g_conns[conn];
    if (np_r32(&r) != c->token) return;     // spoofed source: no input, no
                                            // idle-timer refresh
    c->last_recv_ms = net_now_ms();
    int h = c->human;
    // Event ack: the highest event id the client has applied. Compared
    // as a SIGNED difference so a reordered datagram carrying an older ack
    // cannot walk the high-water mark backwards and resend a kill sound.
    uint32_t ack = np_r32(&r);
    if ((int32_t)(ack - c->ev_high) > 0) c->ev_high = ack;
    int cnt = np_r8(&r);
    if (cnt > NET_IN_MAX) cnt = NET_IN_MAX;
    // BUFFER, don't apply — net_srv_consume_inputs plays one per world tick.
    // The packet carries the last NET_IN_REDUN ticks (redundancy against
    // loss), so most records are duplicates of already-buffered or already-
    // consumed ticks; only FRESH ticks touch the queue or the rate
    // budget — counting the redundancy would eat the whole 1.25x allowance.
    for (int i = 0; i < cnt; i++) {
      net_input_t in;
      if (!net_input_decode(&r, &in)) break;
      net_srv_queue_input(c, h, &in);
    }
  }
}

// Consume at most ONE buffered input tick per world tick for every connection.
// ONE INPUT IS ONE MOVE — g_human_move[h] tells player_tick whether to run
// this human's move tick at all. An empty queue runs NO move (the client
// predicted exactly one move per input; a server that moved the player on
// gap ticks too counted moves the client never predicted — measured 372 mm
// of pure fill-rule drift on LOOPBACK). The authoritative position is thus a
// function of the input SEQUENCE, not of packet timing, which is what makes
// prediction bit-exact. A backlog deeper than 4 (client burst after a stall)
// drains by DROPPING oldest ticks — their buttons carry into the next played
// tick so a press edge survives; the client sees one small correction.
static void net_srv_consume_inputs(void) {
  for (int i = 0; i < MAX_HUMANS; i++) {
    net_conn_t *c = &g_conns[i];
    if (!c->used || !c->in_started) continue;
    int h = c->human;
    // Backlog deeper than NET_IN_BACKLOG is a CLIENT running away (clock
    // skew the nudge cannot hold, a runaway sender): trim oldest so input
    // latency stays bounded — but never during the server's own post-stall
    // catch-up, where the same depth is inputs waiting for the ticks about
    // to be replayed. Ordinary jitter and frame bursts stay UNDER this
    // threshold and drain via the client's clock nudge instead —
    // every trimmed tick is a move the client already predicted, i.e. a
    // guaranteed correction.
    int trimmed = 0;
    while (!g_srv_catchup && (int32_t)(c->in_high - c->in_next) > NET_IN_BACKLOG) {
      int slot = (int)(c->in_next % NET_IN_MAX);
      if (c->inq_set[slot] && c->inq[slot].tick == c->in_next) {
        c->btn_carry |= c->inq[slot].buttons;
        c->inq_set[slot] = 0;
      }
      c->in_next++;
      trimmed++;
    }
    g_srv_trim_drops += trimmed;
    if (trimmed && g_net_debug)
      printf("netdbg trim conn=%d ticks=%d high=%u next=%u\n",
             i, trimmed, c->in_high, c->in_next);
    int slot = (int)(c->in_next % NET_IN_MAX);
    if (!c->inq_set[slot] || c->inq[slot].tick != c->in_next) {
      g_human_move[h] = 0;                  // gap: no input, no move
      continue;
    }
    net_input_t in = c->inq[slot];
    c->inq_set[slot] = 0;
    in.buttons |= c->btn_carry;
    c->btn_carry = 0;
    c->last_input_tick = c->in_next;
    c->in_next++;
    // The input's view tick: lag-comp anchor for this move's shots,
    // and g_tick - view IS the round trip the player feels — smoothed into
    // the conn's ping. Both reject an unsynced (0) or insane value.
    long view = (long)in.view;
    long lag = g_tick - view;
    g_human_view[h] = (in.view && lag >= 0 && lag <= NET_LAGCOMP_MAX) ? view : -1;
    if (in.view && lag >= 0 && lag < 4 * TICK_HZ) {
      int ms = (int)(lag * 1000 / TICK_HZ);
      c->rtt_ms = c->rtt_ms ? (c->rtt_ms * 7 + ms) / 8 : ms;
    }
    for (int b = 0; b < IN_COUNT; b++)
      g_human_held[h][b] = (in.buttons >> b) & 1;
    g_human_loadout[h] = in.loadout;
    g_human_wish[h][0] = net_dq_wish(in.wish_x);
    g_human_wish[h][1] = net_dq_wish(in.wish_y);
    float yaw = net_dq_yaw(in.yaw), pitch = net_dq_pitch(in.pitch);
    net_validate_angles(&yaw, &pitch, g_players[h].yaw);   // the yaw chain
    g_players[h].yaw = yaw;
    g_players[h].pitch = pitch;
    g_human_move[h] = 1;
  }
}

// Drain the socket and service timeouts — called once per server tick.
// WHICH ARENA DOES THIS SOURCE ADDRESS BELONG TO? The conn table is per-lobby
// (g_conns is a g_lobby macro), so finding a packet's owner is a walk across
// all of them — 16 x 8 = 128 compares per packet, which is nothing next to the
// decode that follows. Returns -1 for an address nobody knows yet.
static int net_srv_lobby_of(uint32_t ip, uint16_t port) {
  int save = g_lobby, found = -1;
  for (int l = 0; l < g_srv.lobbies && found < 0; l++) {
    g_lobby = l;
    for (int i = 0; i < MAX_HUMANS; i++)
      if (g_conns[i].used && g_conns[i].ip == ip && g_conns[i].port == port) {
        found = l; break;
      }
  }
  g_lobby = save;
  return found;
}

// THE PACKING RULE, moved server-side. It used to live in the client's
// directory query (`fullest instance WITH public room`) — one process serving
// every arena can answer it directly, which is what deleted the whole
// discovery layer. Filling the fullest arena first is what makes a new lobby
// open only when the previous one is full, instead of scattering four players
// across four empty arenas. Returns -1 when every arena is full: the caller
// then routes to lobby 0, whose admit fails and answers NR_FULL.
static int net_srv_lobby_pick(void) {
  int save = g_lobby, best = -1, best_h = -1;
  for (int l = 0; l < g_srv.lobbies; l++) {
    g_lobby = l;
    int humans = 0;
    for (int h = 0; h < MAX_HUMANS; h++) if (g_humans_on[h]) humans++;
    if (humans < g_srv.cap_public && humans > best_h) { best_h = humans; best = l; }
  }
  g_lobby = save;
  return best;
}

// LAN DISCOVERY. A client looking for games broadcasts NET_SCAN_LEN bytes
// starting with NET_SCAN_MAGIC; every server that hears it answers with a
// NET_HERE_LEN-byte card (name, mode, map, players, version). Neither packet
// carries NET_PROTO in bytes 0-1 — "BD" is 0x4442, never a protocol number —
// so a scan reaches servers of ANY version and the card says which one it is,
// and the list can show "UPDATE NEEDED" instead of a game that fails to join.
// The answer is smaller than the question (44 < 64) and the question is
// dropped below its floor, so the card can never be used as a reflector.
#define NET_SCAN_LEN 64
#define NET_HERE_LEN 44
static const char NET_SCAN_MAGIC[8] = "BDSCAN1";
static const char NET_HERE_MAGIC[8] = "BDHERE1";
static uint32_t g_srv_uid;   // per-process id, so one server heard on two
                             // interfaces is listed once
static void net_srv_scan_reply(uint32_t ip, uint16_t port) {
  int save = g_lobby, humans = 0;
  for (int l = 0; l < g_srv.lobbies; l++) {
    g_lobby = l;
    for (int h = 0; h < MAX_HUMANS; h++) if (g_humans_on[h]) humans++;
  }
  g_lobby = 0;
  int map = g_map_id;
  g_lobby = save;
  unsigned char out[NET_HERE_LEN] = {0};
  np_wr w = {out, sizeof out, 0, 0};
  for (int i = 0; i < 8; i++) np_w8(&w, (uint8_t)NET_HERE_MAGIC[i]);
  np_w16(&w, NET_PROTO);
  np_w16(&w, (uint16_t)g_srv.port);
  np_w32(&w, g_srv_uid);
  np_w8(&w, (uint8_t)humans);
  np_w8(&w, (uint8_t)(g_srv.cap_public * g_srv.lobbies));
  np_w8(&w, (uint8_t)g_srv.mode);
  np_w8(&w, (uint8_t)map);
  for (int i = 0; i < 24; i++) np_w8(&w, (uint8_t)g_srv.name[i]);
  net_udp_send(g_srv_fd, ip, port, out, NET_HERE_LEN);
}

[[maybe_unused]] static void net_srv_poll(void) {
  if (g_srv_fd < 0) return;
  uint32_t epoch = (uint32_t)(time(NULL) / 30);
  unsigned char b[2048];
  uint32_t ip; uint16_t port;
  int n, guard = 0;
  while ((n = net_udp_recv(g_srv_fd, &ip, &port, b, sizeof b)) > 0 && guard++ < 256) {
    if (n >= NET_SCAN_LEN && !memcmp(b, NET_SCAN_MAGIC, 8)) {   // a LAN scan
      net_srv_scan_reply(ip, port);
      continue;
    }
    // ROUTE FIRST, DECODE SECOND. A known address goes to its own arena; a
    // stranger goes to the one the packing rule chose. Everything below this
    // line is the single-arena server that was here before, unchanged.
    int l = net_srv_lobby_of(ip, port);
    if (l < 0) l = net_srv_lobby_pick();
    g_lobby = l < 0 ? 0 : l;
    net_srv_packet(b, n, ip, port, epoch);
  }
  uint32_t now = net_now_ms();
  for (int l = 0; l < g_srv.lobbies; l++) {
    g_lobby = l;
    for (int i = 0; i < MAX_HUMANS; i++)       // idle timeout: 10 s
      if (g_conns[i].used && now - g_conns[i].last_recv_ms > 10000) net_srv_drop(i);
  }
  g_lobby = 0;
}

// Send a snapshot to every connected client — called after the tick, rate-
// limited to ~60 Hz by the caller.
[[maybe_unused]] static void net_srv_broadcast(void) {
  if (g_srv_fd < 0) return;
  for (int i = 0; i < MAX_HUMANS; i++) {
    if (!g_conns[i].used) continue;
    static unsigned char snap[2048];
    // Queue depth AFTER this tick's consume: buffered-not-yet-played ticks.
    int32_t qd = net_conn_qdepth(&g_conns[i]);
    uint8_t q8 = qd > 255 ? 255 : (uint8_t)qd;
    size_t len = net_snapshot_build(snap, sizeof snap, g_conns[i].last_input_tick,
                                    g_conns[i].ev_high, q8);
    if (len) net_udp_send(g_srv_fd, g_conns[i].ip, g_conns[i].port, snap, (int)len);
  }
}

// Broadcast the score rows (NP_SCORES, 4 Hz from server_main). One build —
// the payload is identical per client except nothing: ping is per-ENTITY.
[[maybe_unused]] static void net_srv_scores_broadcast(void) {
  if (g_srv_fd < 0) return;
  uint8_t ping[MAX_ENTS] = {0};
  for (int i = 0; i < MAX_HUMANS; i++) {
    if (!g_conns[i].used) continue;
    int e = ent_of_human(g_conns[i].human);
    int ms = g_conns[i].rtt_ms;
    ping[e] = ms < 0 ? 0 : ms > 255 ? 255 : (uint8_t)ms;
  }
  static unsigned char buf[512];
  size_t len = net_scores_build(buf, sizeof buf, ping);
  if (!len) return;
  for (int i = 0; i < MAX_HUMANS; i++)
    if (g_conns[i].used)
      net_udp_send(g_srv_fd, g_conns[i].ip, g_conns[i].port, buf, (int)len);
}

// The mission's state to every client in this arena (NP_RESCUE, 20 Hz).
[[maybe_unused]] static void net_srv_rescue_broadcast(void) {
  if (g_srv_fd < 0 || !rescue_on()) return;
  static unsigned char buf[512];
  size_t len = net_rescue_build(buf, sizeof buf);
  if (!len) return;
  for (int i = 0; i < MAX_HUMANS; i++)
    if (g_conns[i].used)
      net_udp_send(g_srv_fd, g_conns[i].ip, g_conns[i].port, buf, (int)len);
}

static int net_srv_client_count(void) {
  int c = 0;
  for (int i = 0; i < MAX_HUMANS; i++) if (g_conns[i].used) c++;
  return c;
}

// Worst smoothed RTT and worst input-queue depth across the live conns — the
// digest's connection-feel line. Zeros with no clients.
static void net_srv_lat(int *rtt_max, int *qd_max) {
  *rtt_max = 0; *qd_max = 0;
  for (int i = 0; i < MAX_HUMANS; i++) {
    if (!g_conns[i].used) continue;
    if (g_conns[i].rtt_ms > *rtt_max) *rtt_max = g_conns[i].rtt_ms;
    int32_t qd = net_conn_qdepth(&g_conns[i]);
    if (qd > *qd_max) *qd_max = qd;
  }
}


// --- client side ---
typedef enum { NC_OFF, NC_CONNECTING, NC_JOINING, NC_PLAYING, NC_FAILED } nc_state_t;
typedef struct {
  int        fd;
  uint32_t   ip; uint16_t port;
  nc_state_t state;
  uint32_t   cookie;
  int        my_ent;                 // our entity id, from ACCEPT
  uint32_t   connect_start_ms, last_send_ms, last_recv_ms;
  uint32_t   server_tick;            // newest snapshot's server tick
  uint8_t    have_snap;              // server_tick is seeded (first snapshot applied)
  uint32_t   world;                  // server's world hash (arena check)
  // Event high-water mark: the highest event id applied. Sent on every
  // input packet as the ack, and the dedup filter for the server's resends —
  // an event that arrives twice must not play its sound twice.
  uint32_t   ev_high;
  uint32_t   token;                  // session token from ACCEPT, echoed on
                                     // every INPUT/LEAVE (the spoof gate)
  uint8_t    spawn_seq;              // last seen for our own entity (respawn edge)
  int        have_spawn_seq;
  uint8_t    ent_ss[MAX_ENTS];       // last spawn_seq per FOREIGN entity: a
  uint8_t    ent_ss_seen[MAX_ENTS];  // changed one is a respawn teleport —
                                     // reset the anim solver, don't lerp
                                     // a corpse across the map to its spawn
  char       name[12];
  int        lobby, lobbies;         // which arena of how many (ACCEPT)
  int        reject;                 // last REJECT reason, 0 = none
  int        synced;                 // first authoritative fix received?
  float      srv_mv[MV_COUNT];       // the SERVER's sim tuning, from ACCEPT —
  float      srv_wp[WPN_COUNT][WP_COUNT];  // g_sim_mv/g_sim_wp point here online
  uint32_t   seq;                    // our own input counter (prediction anchor)
  uint32_t   sync_seq;               // seq at the first authoritative fix —
                                     // predpos entries before it are the PRE-
                                     // JOIN local match, not predictions
  uint32_t   snap_seq;               // seq when the last snapshot applied: the
                                     // dead-reckoning tick count since, and
                                     // with server_tick the input's VIEW tick
  float      qdepth_ema;             // smoothed server-side input-queue depth
                                     // (snapshot header) — the clock nudge
                                     // holds it at NET_QDEPTH_TARGET
  float      prev_yaw;               // last PREDICTED (validated) yaw: the
                                     // prev the server's validation will
                                     // use for the next record — p->yaw is
                                     // already the new aim by the time the
                                     // tick runs, so it cannot be the anchor
  int        have_prev_yaw;
  v3         predpos[NET_HIST];      // predicted pos AFTER each seq's move
  net_input_t inlog[NET_HIST];       // sent inputs, the redundancy source
} net_client_t;
static net_client_t g_cl;
// Score-row furniture replicated by NP_SCORES (online only): per-entity ping
// for the scoreboard, and the SERVER's frag limit — the local g_cfg.sp_frag is
// this player's own preference and lies about the match he actually joined.
static uint8_t  g_net_ping_all[SRV_LOBBIES][MAX_ENTS];
#define g_net_ping (g_net_ping_all[g_lobby])
static uint16_t g_net_fraglimit;   // (g_net_rounds, its LMS twin, is up by the mode helpers)
static int g_cl_corrections;         // reconciliation count, for the proof
static float g_cl_worst_err_mm;
static int g_cl_ev_applied, g_cl_ev_dupes;   // event channel, for the proof
static void net_cl_send_inputs(void);        // defined below

// Derived predicted state per input tick, the predpos ring's sibling.
// A rollback replay must re-run the SAME prediction it replaces: without this
// record the heal re-ran weapon_tick on top of already-advanced state — every
// heal advanced the weapon clock twice for the rollback window and re-drew
// recoil from the shared gun-RNG stream, shifting it against the server's for
// the rest of the life — and re-validated the yaw chain against the
// NEWEST yaw instead of the chain value at `li`.
typedef struct { weapon_t wp; uint64_t rng; float yaw; } cl_pred_t;
static cl_pred_t g_cl_pred[NET_HIST];

// True when input tick `t` is inside the recorded prediction window — the
// same three conditions the reconcile anchor checks.
static int net_cl_hist_ok(uint32_t t) {
  return g_cl.synced && (int32_t)(t - g_cl.sync_seq) >= 0 &&
         (int32_t)(g_cl.seq - t) >= 0 && g_cl.seq - t < NET_HIST;
}

// Restore the recorded derived state at input `li` before a rollback replay.
static void net_cl_restore_pred(player_t *p, int h, uint32_t li) {
  if (!net_cl_hist_ok(li)) return;
  const cl_pred_t *ps = &g_cl_pred[li % NET_HIST];
  p->wp = ps->wp;
  g_rng_gun[h] = ps->rng;
  p->yaw = ps->yaw;        // the chain value the replay validates against
}

// An adopted position rides the wire's 1 cm grid, and against a wall the
// server slides along at exactly face+radius the grid lands up to 5 mm INSIDE
// the solid. move_axis resolves ANY overlap by snapping to a face of the
// first overlapping axis, so a millimetre of wire rounding became a 2-22 m
// sideways teleport, re-created by every subsequent adopt — the sustained
// correction sawtooth (measured live, 2026-08-18). Push out along the
// smallest displacement instead; anything deeper than one grid step is a
// real desync and stays the move solver's problem.
static void net_cl_depenetrate(player_t *p) {
  float height = player_col_height(p);
  BP_BOX(i, p->pos.x - PLAYER_RADIUS - 1.0f, p->pos.z - PLAYER_RADIUS - 1.0f,
         p->pos.x + PLAYER_RADIUS + 1.0f, p->pos.z + PLAYER_RADIUS + 1.0f) {
    const solid_t *s = &g_solids[i];
    if (!(p->pos.x + PLAYER_RADIUS > s->min.x && p->pos.x - PLAYER_RADIUS < s->max.x &&
          p->pos.y + height        > s->min.y && p->pos.y                 < s->max.y &&
          p->pos.z + PLAYER_RADIUS > s->min.z && p->pos.z - PLAYER_RADIUS < s->max.z))
      continue;
    float push[5] = {
      s->max.x - (p->pos.x - PLAYER_RADIUS),     // out through +x
      (p->pos.x + PLAYER_RADIUS) - s->min.x,     // out through -x
      s->max.z - (p->pos.z - PLAYER_RADIUS),     // out through +z
      (p->pos.z + PLAYER_RADIUS) - s->min.z,     // out through -z
      s->max.y - p->pos.y,                       // feet up onto the top
    };
    int k = 0;
    for (int j = 1; j < 5; j++) if (push[j] < push[k]) k = j;
    if (push[k] > 0.011f) continue;              // > one grid step: not ours
    switch (k) {
      case 0: p->pos.x = s->max.x + PLAYER_RADIUS; break;
      case 1: p->pos.x = s->min.x - PLAYER_RADIUS; break;
      case 2: p->pos.z = s->max.z + PLAYER_RADIUS; break;
      case 3: p->pos.z = s->min.z - PLAYER_RADIUS; break;
      case 4: p->pos.y = s->max.y; break;
    }
  }
}

static void net_cl_send_connect(void) {
  unsigned char b[NET_CONNECT_MIN] = {0}; np_wr w = {b, sizeof b, 0, 0};
  np_w16(&w, NET_PROTO); np_w8(&w, NP_CONNECT);
  for (int i = 0; i < 11 && g_cl.name[i]; i++) np_w8(&w, (uint8_t)g_cl.name[i]);
  // Sent at full length whatever the name weighs: the padding IS the
  // anti-amplification property, so it may not shrink with a short name.
  net_udp_send(g_cl.fd, g_cl.ip, g_cl.port, b, NET_CONNECT_MIN);
  g_cl.last_send_ms = net_now_ms();
}
static void net_cl_send_join(void) {
  unsigned char b[24]; np_wr w = {b, sizeof b, 0, 0};
  np_w16(&w, NET_PROTO); np_w8(&w, NP_JOIN); np_w32(&w, g_cl.cookie);
  for (int i = 0; i < 11 && g_cl.name[i]; i++) np_w8(&w, (uint8_t)g_cl.name[i]);
  net_udp_send(g_cl.fd, g_cl.ip, g_cl.port, b, (int)w.at);
  g_cl.last_send_ms = net_now_ms();
}

// Begin a connection(the caller keeps playing locally meanwhile).
static int net_cl_connect(uint32_t ip, uint16_t port, const char *name) {
  if (g_cl.fd >= 0) net_udp_close(g_cl.fd);   // a FAILED handshake parks its
                                              // fd here; retrying must not
                                              // leak one descriptor per press
  memset(&g_cl, 0, sizeof g_cl);
  g_cl.fd = net_udp_open(0);           // ephemeral local port
  if (g_cl.fd < 0) { g_cl.state = NC_FAILED; return 0; }
  g_cl.ip = ip; g_cl.port = port;
  g_cl.my_ent = -1;
  snprintf(g_cl.name, sizeof g_cl.name, "%s", name && name[0] ? name : "PLAYER");
  g_cl.state = NC_CONNECTING;
  g_cl.connect_start_ms = net_now_ms();
  g_cl_team_asked = 0;
  net_cl_send_connect();
  return 1;
}
// Ask the server for a side (TEAM page). Unacknowledged: NP_SCORES is the
// answer, 4 Hz, and a lost request is simply a second press.
static void net_cl_send_team(int t) {
  if (g_cl.fd < 0 || g_cl.state != NC_PLAYING) return;
  unsigned char b[8]; np_wr w = {b, sizeof b, 0, 0};
  np_w16(&w, NET_PROTO); np_w8(&w, NP_TEAM);
  np_w32(&w, g_cl.token);
  np_w8(&w, (uint8_t)(t ? 1 : 0));
  net_udp_send(g_cl.fd, g_cl.ip, g_cl.port, b, (int)w.at);
}
static void net_cl_disconnect(void) {
  // The sim tuning goes back to the player's own config BEFORE g_cl is wiped
  // — the pointers aim into g_cl.srv_mv/srv_wp while online(light).
  g_sim_mv = g_cfg.mv;
  g_sim_wp = g_cfg.wp;
  // SAY GOODBYE. Best effort and unacknowledged — UDP, and a client that has
  // already lost the link cannot be helped — but in the common case (LEAVE
  // MATCH, or quitting) it hands the slot back at once instead of parking it
  // for the 10 s idle timeout.
  if (g_cl.fd >= 0 && (g_cl.state == NC_PLAYING || g_cl.state == NC_JOINING)) {
    unsigned char b[8]; np_wr w = {b, sizeof b, 0, 0};
    np_w16(&w, NET_PROTO); np_w8(&w, NP_LEAVE);
    np_w32(&w, g_cl.token);                   // session token
    net_udp_send(g_cl.fd, g_cl.ip, g_cl.port, b, (int)w.at);
  }
  if (g_cl.fd >= 0) net_udp_close(g_cl.fd);
  memset(&g_cl, 0, sizeof g_cl);
  g_cl.fd = -1;
  memset(g_net_ping, 0, sizeof g_net_ping);  // scoreboard furniture goes local
  g_net_fraglimit = 0;
  g_net_rounds = 0;
}

// ...and WHOSE BODY THAT IS. A dead bot's position has one owner: from the
// death edge it saw itself, this client's own ragdoll integrates it and the
// snapshot stops writing it (see net_cl_apply_ent). An edge is not enough to
// decide that — a client that JOINED onto an already-dead bot never saw one,
// and reading the absent edge as ownership froze that body at the world
// origin. So ownership is stored, taken on the launch and handed back on the
// respawn, and the launch fires on the STATE (dead and unowned) rather than on
// the transition into it.
static unsigned char g_cl_body[MAX_ENTS];

// Write one entity record from a snapshot into the local world mirror. The
// LOCAL player's authoritative block is handled separately (reconciliation);
// here we place the OTHERS. anim/vm/pose are re-solved locally, never sent.
static void net_cl_apply_ent(const net_ent_t *e) {
  int h = human_of_ent(e->id);
  v3 pos = {{net_dq_pos(e->x), net_dq_pos(e->y), net_dq_pos(e->z)}};
  v3 vel = {{net_dq_vel(e->vx), net_dq_vel(e->vy), net_dq_vel(e->vz)}};
  // A spawn_seq edge is a TELEPORT: kill the render lerp (prev_pos = pos) and
  // re-solve the pose, exactly what the sim does on its own respawns.
  int tele = g_cl.ent_ss_seen[e->id] && g_cl.ent_ss[e->id] != e->spawn_seq;
  g_cl.ent_ss[e->id] = e->spawn_seq;
  g_cl.ent_ss_seen[e->id] = 1;
  if (h >= 0) {
    player_t *p = &g_players[h];
    p->prev_pos = tele ? pos : p->pos; p->pos = pos; p->vel = vel;
    p->yaw = net_dq_yaw(e->yaw); p->pitch = net_dq_pitch(e->pitch);
    p->lean = net_dq_wish(e->lean);
    p->alive = e->flags & 1; p->grounded = (e->flags >> 1) & 1;
    p->crouched = (e->flags >> 2) & 1; p->sliding = (e->flags >> 3) & 1;
    p->hp = e->hp; p->wp.cur = e->weapon;
    loadout_apply(&p->wp, e->loadout);
    net_melee_apply(&p->wp, e);
    if (!g_humans_on[h]) { g_humans_on[h] = 1; anim_reset(&g_player_anim[h], pos, p->yaw); }
    else if (tele) anim_reset(&g_player_anim[h], pos, p->yaw);
  } else {
    bot_t *b = &g_bots[e->id - 1];
    float byaw = net_dq_yaw(e->yaw);
    // A CORPSE IS PRESENTATION AND IT HAS EXACTLY ONE OWNER. The server
    // ragdolls its dead bots too (bot_tick's dead branch) and ENT_FOREACH
    // sends them, so once the client launched its own ragdoll BOTH were
    // writing this one position: the snapshot yanked the body onto the
    // server's solution, the next tick's rag_step pushed it back onto the
    // client's, and prev_pos was left holding whichever the other one wrote.
    // Two integrators, same launch, different arrival — measured 2.3 m apart —
    // and the render lerped across the gap every frame. The client owns the
    // body from the death edge on, exactly as it owns the human corpse pool;
    // the respawn arrives as alive=1 and takes the position back.
    int keep = g_cl_body[e->id] && !b->alive && !(e->flags & 1);
    if (!keep) { b->prev_pos = tele ? pos : b->pos; b->pos = pos; b->vel = vel; }
    // THE HEADING IS A PAIR TOO. scene_draw_bot lerps prev_yaw->yaw by the
    // sub-tick alpha exactly as it lerps the position pair, and offline
    // bot_tick advances it every tick. Online nothing did: prev_yaw kept the
    // value the slot was born with for the whole session, so every bot's body
    // swung between that stale heading and its real one on every rendered
    // frame — and on a DEAD one it is fig_death_pose's yaw, i.e. the corpse
    // spun on the floor. Maintained HERE, beside prev_pos, for the same
    // reason and by the same rule.
    b->prev_yaw = tele ? byaw : b->yaw; b->yaw = byaw;
    b->pitch = net_dq_pitch(e->pitch);
    b->alive = e->flags & 1; b->grounded = (e->flags >> 1) & 1;
    b->crouch = (e->flags >> 2) & 1; b->sliding = (e->flags >> 3) & 1;
    b->hp = e->hp; b->wp.cur = e->weapon; b->active = 1;
    loadout_apply(&b->wp, e->loadout);
    net_melee_apply(&b->wp, e);
    if (tele) anim_reset(&b->anim, pos, b->yaw);
    if (e->id > g_match.nbots) g_match.nbots = e->id;   // grow the roster to fit
  }
}

// One tick of dead reckoning for every FOREIGN entity. Snapshots land at
// ~60 Hz but the render lerps prev_pos->pos with the 120 Hz tick alpha, so an
// entity that only moves when a snapshot arrives steps BACKWARD every other
// tick — a saw-tooth on every other player and bot. Advancing the pair by
// straight velocity each tick keeps the lerp cadence consistent; the next
// snapshot arrives within quantization of the extrapolated point and its
// application (prev = old pos, pos = auth) absorbs the residual over one tick.
static void net_cl_extrapolate(void) {
  // BOUNDED dead reckoning: snapshots correct the projection every two
  // ticks; when they STOP (loss burst, server stall) unbounded projection
  // glides every foreign entity through walls on stale velocity until the 5 s
  // timeout. 24 ticks (200 ms) is the Source-class cap: beyond it entities
  // hold position and the next snapshot teleports them honestly.
  if (g_cl.seq - g_cl.snap_seq > 24) return;
  int me = human_of_ent(g_cl.my_ent);
  for (int h = 0; h < MAX_HUMANS; h++) {
    if (!g_humans_on[h] || h == me) continue;
    player_t *p = &g_players[h];
    if (!p->alive) continue;
    p->prev_pos = p->pos;
    p->pos = v3_add(p->pos, v3_scale(p->vel, TICK_DT));
  }
  for (int i = 0; i < g_match.nbots; i++) {
    bot_t *b = &g_bots[i];
    if (!b->active || !b->alive) continue;
    b->prev_pos = b->pos;
    b->prev_yaw = b->yaw;      // no angular velocity on the wire: the heading
                               // holds, but the PAIR still has to step or the
                               // render lerp keeps reaching back a whole tick
    b->pos = v3_add(b->pos, v3_scale(b->vel, TICK_DT));
  }
}

// REPLAY THE PENDING INPUTS: the caller has just adopted the server's
// authoritative state AFTER input `li`; every input in (li, seq) is already
// on the wire and the server WILL play it — after a respawn the server is
// already alive and walking one RTT before this client knows. Re-simulating
// those inputs from the fix rebuilds predpos and the live position EXACTLY,
// so the following snapshots anchor cleanly. Without it, every adoption
// opened on ~9 phantom corrections while the in-flight window drained.
// One copy, two callers: the hard join/respawn/desync adoption and the
// >5 cm rollback heal.
static void net_cl_replay(player_t *p, uint32_t li) {
  net_cl_depenetrate(p);     // the adopted fix may sit a wire-rounding inside
                             // a wall; replaying from there teleports
  for (uint32_t rt = li + 1; (int32_t)(g_cl.seq - rt) > 0; rt++) {
    const net_input_t *rin = &g_cl.inlog[rt % NET_HIST];
    if (rin->tick != rt) break;              // ring recycled: stop cleanly
    int rheld[IN_COUNT];
    for (int b = 0; b < IN_COUNT; b++) rheld[b] = (rin->buttons >> b) & 1;
    float ry = net_dq_yaw(rin->yaw), rp = net_dq_pitch(rin->pitch);
    net_validate_angles(&ry, &rp, p->yaw);   // the chain, as sent
    p->yaw = ry; p->pitch = rp;
    g_predicting = 1;
    player_move_tick(p, g_cl.my_ent, rheld,
                     net_dq_wish(rin->wish_x), net_dq_wish(rin->wish_y));
    g_predicting = 0;
    g_cl.predpos[rt % NET_HIST] = p->pos;
  }
}

static void net_cl_snapshot(const unsigned char *b, int n) {
  uint32_t st, li, wh, sd;
  uint8_t qd = 0;
  net_ent_t es[MAX_ENTS];
  net_ev_t  evs[NET_EV_SNAP_MAX];
  int full = 0, nev = 0;
  uint8_t mid = 0;
  int cnt = net_snapshot_read(b, (size_t)n, &st, &li, &wh, &sd, &mid, &qd, es, MAX_ENTS,
                              &full, evs, NET_EV_SNAP_MAX, &nev);
  if (cnt < 0) return;
  // Signed-difference like every other monotone-counter compare in this layer
  // (ev ack, resend window): a plain unsigned '<' inverts at the 32-bit tick
  // wrap (~414 days of server uptime) and then drops every later snapshot.
  // The FIRST snapshot is exempt — it SEEDS the anchor. server_tick is 0 from
  // the connect memset and ACCEPT carries no tick, so comparing the seed
  // against 0 would reject any server past 2^31 ticks (~207 days) of uptime
  // and hang the join forever.
  if (g_cl.have_snap && (int32_t)(st - g_cl.server_tick) < 0)
    return;   // stale/out-of-order: drop
  g_cl.have_snap = 1;
  g_cl.server_tick = st;
  g_cl.snap_seq = g_cl.seq;                  // dead-reckoning ticks count from here
  // Server-side input-queue depth, smoothed: the clock nudge's input.
  g_cl.qdepth_ema += ((float)qd - g_cl.qdepth_ema) * 0.1f;
  g_cl.world = wh;
  // FOLLOW THE SERVER'S ARENA. A rematch regenerates it, so the seed can move
  // mid-session; adopt it and re-upload the world VBO. Then check the hash the
  // same way the join does — if the two disagree after regenerating from the
  // server's own seed, the two builds do not agree on the generator, and the join
  // says refuse rather than play a divergent map.
  if (sd != g_map_seed || mid != g_map_id) {
    g_map_id = mid;
    map_generate(sd); g_map_seed = sd;
    g_world_dirty = 1;
  }
  if (wh != net_world_hash()) {
    g_cl.reject = NR_WORLD;
    g_cl.state = NC_FAILED;
    return;
  }
  // Foreign entities are applied straight from the newest snapshot. Buffering
  // a few for interpolation would need a ring HERE — there used to be
  // one, filled every snapshot and read by nothing, which reads as a feature
  // that exists. Add it back with the reader, not before it.
  for (int i = 0; i < cnt; i++)
    if ((int)es[i].id != g_cl.my_ent) net_cl_apply_ent(&es[i]);
  // A ROSTER SHRINKS TOO. Every snapshot carries the FULL roster (the server
  // walks ENT_FOREACH with no culling), so absence is information: a bot that
  // yielded its slot to a joining human, or a human who left, is simply not in
  // the record list. Applying only what arrives made the roster grow-only —
  // the ghost kept its last position forever, was drawn, counted in the
  // scoreboard's player total and traced against by every local probe.
  // Only on a COMPLETE list: a truncated one is short at the TAIL, which is
  // where the humans are, so a single bad record would read as a mass exit.
  if (full) {
    unsigned char seen[MAX_ENTS] = {0};
    for (int i = 0; i < cnt; i++) seen[es[i].id] = 1;
    int me = human_of_ent(g_cl.my_ent);
    // From slot 0, not 1. On a server slot 0 is empty and is not in the list
    // at all, so a client that skipped it kept its own pre-join entity 0 alive
    // forever: drawn with a shadow, counted in the scoreboard, and a valid
    // aim-assist target. Our OWN slot is exempt — the local block below owns it.
    for (int h = 0; h < MAX_HUMANS; h++)
      if (h != me && !seen[ent_of_human(h)]) g_humans_on[h] = 0;
    int top = 0;
    for (int e = 1; e <= MAX_BOTS; e++) {
      if (seen[e]) top = e;
      else g_bots[e - 1].active = 0;
    }
    g_match.nbots = top;
    // Departure clears the SCORE ROW too (the server does the same in
    // net_srv_drop; NP_SCORES only carries present entities, so a leaver's
    // kills otherwise stay on the board as a frozen phantom lead).
    for (int e = 0; e < MAX_ENTS; e++)
      if (!seen[e] && e != g_cl.my_ent) {
        g_match.kills[e] = g_match.deaths[e] = 0;
        g_match.shots[e] = g_match.hits[e] = 0;
        g_net_ping[e] = 0;
      }
  }
  // The LOCAL player's authoritative block: reconcile. last_input_
  // tick (`li`) is the highest input seq the server has applied — the anchor.
  for (int i = 0; i < cnt; i++) {
    if ((int)es[i].id != g_cl.my_ent) continue;
    int h = human_of_ent(g_cl.my_ent);
    if (h < 0) break;
    player_t *p = &g_players[h];
    v3 auth = {{net_dq_pos(es[i].x), net_dq_pos(es[i].y), net_dq_pos(es[i].z)}};
    v3 avel = {{net_dq_vel(es[i].vx), net_dq_vel(es[i].vy), net_dq_vel(es[i].vz)}};
    uint8_t alive = es[i].flags & 1;
    if (!g_humans_on[h]) g_humans_on[h] = 1;
    // The PREDICTION ERROR, and the CORRECTION, both anchor on predpos[li] —
    // our own recorded prediction for the tick the server just acked. The
    // current position legitimately LEADS the acked state by ~RTT x speed;
    // correcting against it (as this block once did) counted that lead as
    // error and dragged a moving player backwards a quarter of one RTT's
    // travel on every snapshot — zero on localhost, permanent rubber-banding
    // over a real link. Entries below sync_seq are the pre-join local match.
    int synced = g_cl.synced;
    int anchor = synced && alive && p->alive &&
                 (int32_t)(li - g_cl.sync_seq) >= 0 &&
                 (int32_t)(g_cl.seq - li) >= 0 && g_cl.seq - li < NET_HIST;
    // Computed BEFORE the error metric: a spawn_seq edge without a death is a
    // match-restart teleport — a legitimate server-side move, not a
    // misprediction, and counting it made every rematch read as one
    // correction in a metric whose healthy value is 0.
    int respawned = g_cl.have_spawn_seq && es[i].spawn_seq != g_cl.spawn_seq;
    v3 err = {{0, 0, 0}};
    if (anchor) {
      v3 pp = g_cl.predpos[li % NET_HIST];
      err = v3_sub(auth, pp);
      float e_mm = sqrtf(err.x * err.x + err.z * err.z) * 1000.0f;
      if (respawned) e_mm = 0.0f;              // teleport, not error
      if (e_mm > g_cl_worst_err_mm) g_cl_worst_err_mm = e_mm;
      if (e_mm > 50.0f) {                      // > 5 cm at the acked tick
        g_cl_corrections++;
        if (g_net_debug)
          printf("netdbg corr li=%u seq=%u err=%.0fmm auth=(%.2f %.2f) pred=(%.2f %.2f) vel=(%.2f %.2f) grounded=%d\n",
                 li, g_cl.seq, (double)e_mm, (double)auth.x, (double)auth.z,
                 (double)pp.x, (double)pp.z, (double)avel.x, (double)avel.z,
                 (es[i].flags >> 1) & 1);
      }
    }
    // A big gap or a first fix is a SPAWN/desync, not drift: snap hard and
    // re-solve the pose. Otherwise trust the deterministic prediction and only
    // bleed the small residual toward auth, so the view never pops.
    float cdx = auth.x - p->pos.x, cdz = auth.z - p->pos.z;
    float cur_err = anchor ? sqrtf(err.x * err.x + err.z * err.z)
                           : sqrtf(cdx * cdx + cdz * cdz);
    // A RESPAWN is the only thing that may move the player's AIM. Everything
    // else here corrects POSITION. Taking the server's yaw/pitch on every
    // correction meant the view snapped back to where it was pointing one RTT
    // ago on every hitch or dropped packet — invisible on localhost, a violent
    // mouse yank over a real link. The angles are the client's to begin with:
    // it sends them, the server only validates them, so the snapshot
    // echo of our own aim carries no information we do not already have.
    g_cl.spawn_seq = es[i].spawn_seq;          // respawned: computed above,
    g_cl.have_spawn_seq = 1;                   // before the error metric
    if (!synced || cur_err > 2.0f || !alive || !p->alive) {
      if (!synced) {
        // SPAWN THE LOCAL SLOT BEFORE ADOPTING THE SERVER'S STATE. Until this
        // instant the player was entity 0 (the local match); g_players[h] for
        // the slot the server assigned is still the zeroed struct — magazines
        // included. Everything the client predicts out of it therefore started
        // EMPTY: the HUD counter read 0/30, the predicted weapon_tick dry-fired
        // every trigger pull and the viewmodel never cycled, while the server
        // fired a full magazine and reported it. player_spawn is pure (no RNG,
        // no world query), so calling it here costs nothing and desyncs nothing.
        // First authoritative fix: NOW switch the camera/audio to the server
        // instance. Carry the local match's look direction over so the
        // view does not snap — read BEFORE the spawn, which memsets the struct.
        float carry_yaw = g_players[0].yaw, carry_pitch = g_players[0].pitch;
        player_spawn(p);
        p->yaw = carry_yaw; p->pitch = carry_pitch;
        g_rng_gun[h] = rng_gun_seed(g_map_seed, h);   // mirror net_srv_admit:
                                                      // both sides draw the
                                                      // same recoil stream
        // THE LOCAL MATCH'S SCORE ROWS ARE NOT THIS MATCH'S — the pre-join
        // arena ran through the whole handshake scoring into g_match.
        // NP_SCORES refills the truth within 250 ms.
        match_scores_clear();
        // Our own name renders from g_human_name like every human's:
        // the server admits us under it, but nothing mirrored it locally —
        // scoreboard and killfeed called the local player REMOTE.
        snprintf(g_human_name[h], sizeof g_human_name[h], "%s", g_cl.name);
        g_local_ent = g_cl.my_ent;
        g_online = 1;
        rescue_reset();              // the local mission (if any) is not this one;
                                     // NP_RESCUE switches the server's on
        g_cl.sync_seq = g_cl.seq;    // predictions are real from HERE on
      }
      p->pos = p->prev_pos = auth; p->vel = avel;
      if (respawned) { p->yaw = net_dq_yaw(es[i].yaw); p->pitch = net_dq_pitch(es[i].pitch); }
      g_cl.prev_yaw = p->yaw;              // the prev chain re-anchors here,
      g_cl.have_prev_yaw = 1;              // exactly as the server's does
      anim_reset(&g_player_anim[h], auth, p->yaw);
      // FLOOD THE PREDICTION HISTORY WITH THE ADOPTED FIX. The ring still
      // holds pre-teleport positions for the ~RTT of ticks the server has yet
      // to ack; without this every snapshot of that window re-measured the
      // corpse->spawn distance as "error" (worst_err 28 m on LOOPBACK), took
      // the >2 m hard branch again and anim_reset the fresh spawn once per
      // snapshot — a pose pop per respawn and a metric that buried real
      // regressions. Post-adoption ticks overwrite their slots as they run.
      for (int k = 0; k < NET_HIST; k++) g_cl.predpos[k] = auth;
      g_cl.synced = 1;
      // The hard branch adopts the server's own derived state wholesale — a
      // spawn or desync means the prediction stream is broken anyway.
      p->grounded = (es[i].flags >> 1) & 1;
      p->crouched = (es[i].flags >> 2) & 1; p->sliding = (es[i].flags >> 3) & 1;
      // A RESPAWN is a fresh weapon, exactly player_spawn's reset — the
      // hard adopt used to carry the dying weapon's cool/reload_t/recoil
      // into the new life, locking predicted fire until NP re-sync.
      // Otherwise restore the RECORDED derived state at li, so the
      // replay below re-runs the same prediction it replaces.
      if (respawned) {
        memset(&p->wp, 0, sizeof p->wp);
        loadout_apply(&p->wp, es[i].loadout);    // the server's, not the menu's
        wpn_fill_mags(&p->wp);
      } else if (alive) {
        net_cl_restore_pred(p, h, li);
      }
      p->wp.cur = es[i].weapon;
      p->wp.ammo[p->wp.cur] = es[i].mag;
      // REPLAY THE PENDING INPUTS. The adopted fix is the state after
      // input `li`; every input in (li, seq) is already on the wire and the
      // server WILL play it — after a respawn it is already alive and walking
      // one RTT before this client knows. Re-simulating those inputs from the
      // fix rebuilds predpos and the live position EXACTLY, so the following
      // snapshots anchor cleanly. Without it, every join/respawn/hard snap
      // opened on ~9 phantom corrections (measured on loopback) while the
      // in-flight window drained through the gentle bleed.
      if (p->alive && !alive)                // death edge: presentation-only
        p->respawn_t = PLAYER_RESPAWN_TICKS; // countdown for the REDEPLOY bar
      else if (alive)                        // (the sim's own respawn_t is
        p->respawn_t = 0;                    // server state and never arrives)
      p->alive = alive;                      // BEFORE the replay: the move
      p->hp = es[i].hp;                      // tick's dead branch is a no-op
      if (alive) {
        net_cl_replay(p, li);
        g_cl.prev_yaw = p->yaw;                // the prev chain continues from here
      }
    } else if (anchor && sqrtf(err.x * err.x + err.z * err.z) > 0.05f) {
      // ROLLBACK HEAL: a real divergence (> 5 cm at the acked tick).
      // The old 25 %-per-snapshot bleed was a delayed-feedback loop: at WAN
      // RTT the measurement is ~11 ticks stale, and against a collision
      // knife-edge (grinding a corner chasing a target) the bleed re-seeded
      // the divergence every tick — measured 254 corrections over four
      // SECONDS of sustained meter-scale error. Adopting the server's state
      // at `li` outright and replaying the pending inputs heals exactly, in
      // one snapshot, whatever the RTT. Below 5 cm the gentle branch still
      // absorbs quantization drift without ever touching derived state.
      p->prev_pos = p->pos;                  // one honest render lerp
      p->pos = auth; p->vel = avel;
      p->grounded = (es[i].flags >> 1) & 1;
      p->crouched = (es[i].flags >> 2) & 1; p->sliding = (es[i].flags >> 3) & 1;
      net_cl_restore_pred(p, h, li);         // recorded derived state at li…
      p->wp.cur = es[i].weapon;              // …then the server's weapon truth
      p->wp.ammo[p->wp.cur] = es[i].mag;     // replay re-predicts pending shots
      g_cl.predpos[li % NET_HIST] = auth;
      net_cl_replay(p, li);
      g_cl.prev_yaw = p->yaw;
    } else if (anchor) {
      // Move the RESIDUAL only, a quarter per snapshot, and shift the recorded
      // prediction history by the same amount — otherwise the next snapshot
      // measures the same error again and the correction compounds. Velocity,
      // grounded/crouched/sliding and the weapon stay PREDICTED: the snapshot
      // versions are one RTT old, and adopting them re-seeded divergence every
      // snapshot (air physics while actually grounded, a swapped weapon
      // flickering back for one RTT).
      v3 c = v3_scale(err, 0.25f);
      p->pos = v3_add(p->pos, c);
      net_cl_depenetrate(p);    // the bleed target is a quantized position and
                                // may sit a wire-rounding inside a wall
      for (int k = 0; k < NET_HIST; k++)
        g_cl.predpos[k] = v3_add(g_cl.predpos[k], c);
      // Magazine: measure the desync AT THE ACKED TICK — the recorded
      // prediction for li against the server's mag at li — and shift the
      // CURRENT predicted magazine by that error, keeping in-flight shots.
      // Comparing the current counter against the RTT-old server value (as
      // this once did) needed a ±1 dead band that let a real one-round
      // desync (a shot the server refused) stand forever.
      if (net_cl_hist_ok(li) && g_cl_pred[li % NET_HIST].wp.cur == es[i].weapon) {
        int am = g_cl_pred[li % NET_HIST].wp.ammo[es[i].weapon] - (int)es[i].mag;
        if (am) {
          // Clamped BOTH ways: around a respawn the ring can still hold the
          // dying magazine against the fresh one and the shift overshoots
          // (read 31/30 live before the cap).
          int mag = p->wp.ammo[p->wp.cur] - am;
          int cap = wdef(&p->wp, p->wp.cur)->mag;
          p->wp.ammo[p->wp.cur] = mag < 0 ? 0 : mag > cap ? cap : mag;
        }
      } else {
        int am = p->wp.ammo[p->wp.cur] - (int)es[i].mag;   // no record: old rule
        if (am < -1 || am > 1) p->wp.ammo[p->wp.cur] = es[i].mag;
      }
    } else {
      p->pos.x += cdx * 0.25f; p->pos.z += cdz * 0.25f;   // no anchor: old bleed
      net_cl_depenetrate(p);
    }
    p->hp = es[i].hp; p->alive = alive;
    loadout_apply(&p->wp, es[i].loadout);   // never predicted: server truth
  }
  // EVENTS LAST, and only once the entities are current: vfx_on_event anchors
  // tracers and flashes on the shooter, and the killfeed names entities that
  // must already exist. Records arrive in id order (the server walks its log
  // oldest-first), which IS push order across ticks — the order g_vfx_wpn's
  // latch and the kill burst's same-tick rule depend on.
  //
  // Applied through ev_push like any local event, so audio, VFX, hitmarker,
  // killfeed and the harness printer all see one channel and not two. Anything
  // at or below the high-water mark is a resend of something already played.
  for (int k = 0; k < nev; k++) {
    if ((int32_t)(evs[k].id - g_cl.ev_high) <= 0) { g_cl_ev_dupes++; continue; }
    event_t e;
    net_ev_to_live(&e, &evs[k]);
    // The death caption's killer is per-viewer state match_on_kill derives
    // from g_local_ent — which is the SERVER'S -1 when the kill happens, so
    // the client derives it here with the identical rule ("BY <own name>"
    // was ent_name(0) falling back to the player's own config name).
    if (e.type == EV_KILL && e.dst == g_local_ent && e.src >= 0)
      g_match.last_killer = e.src;
    ev_push(e);
    g_cl.ev_high = evs[k].id;
    g_cl_ev_applied++;
  }
}

// One predicted client tick: store the input, run our OWN move locally (events
// and damage suppressed — those are the server's), and send it stamped with a
// monotone seq the snapshot echoes back for reconciliation.
static void net_cl_tick(const int held[IN_COUNT], float wx, float wy,
                        float yaw, float pitch) {
  if (g_cl.state != NC_PLAYING || g_cl.my_ent < 0) return;
  int h = human_of_ent(g_cl.my_ent);
  if (h < 0) return;
  player_t *p = &g_players[h];
  uint16_t btn = 0;
  for (int k = 0; k < IN_COUNT; k++) if (held[k]) btn |= (uint16_t)(1u << k);
  // PREDICT ON THE VALUES THE SERVER WILL ACTUALLY SIMULATE. The wire
  // quantizes angles and the wish vector; predicting on the raw floats and
  // sending the rounded ones makes client and server run different inputs
  // every single tick, so reconciliation spends its budget correcting a
  // difference the client created. Round-tripping here costs nothing visible
  // (the yaw step is 0.003 deg) and makes the two simulations bit-identical.
  yaw   = net_dq_yaw(net_q_yaw(yaw));
  pitch = net_dq_pitch(net_q_pitch(pitch));
  wx    = net_dq_wish(net_q_wish(wx));
  wy    = net_dq_wish(net_q_wish(wy));
  // MIRROR THE SERVER'S ANGLE VALIDATION before predicting: the record
  // carries the raw quantized angles, the server dequantizes and validates
  // against ITS previous yaw — an identical input sequence makes both prev
  // chains identical, so validating here keeps the two sims bit-exact even
  // through a flick past NET_MAX_TURN_PER_TICK. Predicting the unvalidated
  // yaw rotated the move basis against the server's for exactly that tick.
  {
    float vy = yaw, vp = pitch;
    net_validate_angles(&vy, &vp, g_cl.have_prev_yaw ? g_cl.prev_yaw : yaw);
    p->yaw = vy; p->pitch = vp;
    g_cl.prev_yaw = vy;
    g_cl.have_prev_yaw = 1;
  }
  // The VIEW tick: the server tick whose world this input was aimed
  // at — the last applied snapshot's tick plus one dead-reckoning tick per
  // client tick since (net_cl_extrapolate advances the targets exactly so).
  // The server rewinds this shot's trace there, and g_tick - view measured at
  // consume is the round trip the scoreboard prints. 0 until the first fix.
  uint32_t view = g_cl.synced ? g_cl.server_tick + (g_cl.seq - g_cl.snap_seq) : 0;
  // Log BEFORE predicting: the record is what the packet carries, and the
  // redundancy tail re-sends it for the next NET_IN_REDUN packets.
  g_cl.inlog[g_cl.seq % NET_HIST] =
      (net_input_t){.tick = g_cl.seq, .view = view, .buttons = btn,
                    .loadout = (uint8_t)loadout_pack(g_cfg.rifle_var, g_cfg.pistol_var),
                    .yaw = net_q_yaw(yaw), .pitch = net_q_pitch(pitch),
                    .wish_x = net_q_wish(wx), .wish_y = net_q_wish(wy)};
  // Predict only while ALIVE: the move tick's dead branch counts down
  // respawn_t and self-respawns — but respawn_t is set in ent_damage, which
  // never runs on a client, so the stale counter respawned the predicted
  // player instantly and every snapshot snapped it back to the corpse, an
  // oscillation at snapshot rate for the whole death screen. Dead players
  // don't move; the server's respawn arrives as an alive flip + spawn_seq
  // edge and is adopted whole (position AND spawn angles) above.
  if (g_cl.synced && p->alive) {
    g_predicting = 1;
    player_move_tick(p, g_cl.my_ent, held, wx, wy);
    g_predicting = 0;
  }
  g_cl.predpos[g_cl.seq % NET_HIST] = p->pos;   // record for the error metric
  g_cl_pred[g_cl.seq % NET_HIST] =              // …and the derived state a
      (cl_pred_t){p->wp, g_rng_gun[h], p->yaw}; // rollback replay restores
  net_cl_send_inputs();
  g_cl.seq++;
}

// One client input packet: the event ack + the last NET_IN_REDUN input ticks
// (oldest first, the order the server's queue insert expects). All but the
// newest are redundancy — the server drops already-seen ticks by number, so
// a lost packet costs nothing until NET_IN_REDUN die in a row.
static void net_cl_send_inputs(void) {
  if (g_cl.state != NC_PLAYING) return;
  // 8 B header + NET_IN_REDUN records; sized with headroom so a future input
  // field cannot silently latch w.bad and truncate the newest record.
  unsigned char b[128]; np_wr w = {b, sizeof b, 0, 0};
  np_w16(&w, NET_PROTO); np_w8(&w, NP_INPUT);
  np_w32(&w, g_cl.token);                     // session token
  np_w32(&w, g_cl.ev_high);                   // event ack
  uint32_t lo = g_cl.seq >= NET_IN_REDUN - 1 ? g_cl.seq - (NET_IN_REDUN - 1) : 0;
  np_w8(&w, (uint8_t)(g_cl.seq - lo + 1));
  for (uint32_t t = lo; t != g_cl.seq + 1; t++)
    net_input_encode(&w, &g_cl.inlog[t % NET_HIST]);
  net_udp_send(g_cl.fd, g_cl.ip, g_cl.port, b, (int)w.at);
  g_cl.last_send_ms = net_now_ms();
}


// Drain the socket, drive the automaton, honour timeouts. Returns 1 while a
// connection is live or forming, 0 once it has failed (caller falls back local).
static int net_cl_poll(void) {
  if (g_cl.state == NC_OFF || g_cl.fd < 0) return 0;
  uint32_t now = net_now_ms();
  unsigned char b[2048];
  uint32_t ip; uint16_t port;
  int n, guard = 0;
  while ((n = net_udp_recv(g_cl.fd, &ip, &port, b, sizeof b)) > 0 && guard++ < 256) {
    // Source ADDRESS and PORT: the port is half the identity of the socket we
    // are talking to, and without it any other service on the same host (or
    // anything that can guess it) can inject into the session.
    if (ip != g_cl.ip || port != g_cl.port) continue;
    if (n < 3) continue;
    np_rd r = {b, (size_t)n, 0, 0};
    if (np_r16(&r) != NET_PROTO) continue;
    int type = np_r8(&r);
    g_cl.last_recv_ms = now;
    if (type == NP_COOKIE && g_cl.state == NC_CONNECTING) {
      g_cl.cookie = np_r32(&r);
      g_cl.state = NC_JOINING;
      net_cl_send_join();
    } else if (type == NP_ACCEPT && g_cl.state == NC_JOINING) {
      int me = np_r8(&r);
      int lob = np_r8(&r), lobn = np_r8(&r);
      uint32_t seed = np_r32(&r);
      int mapid = np_r8(&r);
      uint32_t world = np_r32(&r);
      uint32_t token = np_r32(&r);
      // The id the server hands out is an INDEX into g_players for the rest of
      // the session (human_of_ent), so it is validated once, here, at the only
      // place it enters the process. Unchecked, my_ent = 255 wrote past the
      // roster on the first predicted tick.
      if (me >= MAX_ENTS || human_of_ent(me) < 0) {
        g_cl.reject = NR_PROTO; g_cl.state = NC_FAILED; return 0;
      }
      if (mapid >= MAP_COUNT) {
        g_cl.reject = NR_PROTO; g_cl.state = NC_FAILED; return 0;
      }
      g_map_id = mapid;
      map_generate(seed); g_map_seed = seed;  // adopt the server's arena
      // A DIVERGENT ARENA IS REFUSED, NOT PLAYED. Both sides generate
      // the map from the seed, so the hashes agree unless the generators do
      // not — and then every prediction, every trace and every spawn is wrong
      // in a way that reads as cheating rather than as a version mismatch.
      if (world != net_world_hash()) {
        g_cl.reject = NR_WORLD; g_cl.state = NC_FAILED; return 0;
      }
      // Cosmetic only — the scoreboard names the arena so a player can tell
      // two friends whether they landed together. Clamped because it is
      // wire data: it only ever reaches a printf, but the rule holds.
      g_cl.lobby   = lob  < SRV_LOBBIES ? lob : 0;
      g_cl.lobbies = lobn >= 1 && lobn <= SRV_LOBBIES ? lobn : 1;
      g_cl.my_ent = me;
      g_cl.world = world;
      g_cl.token = token;
      // Adopt the SERVER's sim tuning(light) and predict with it from
      // the first tick. Range-clamped against the same bounds the settings UI
      // enforces; NaN fails the >= and lands on the low bound. The player's
      // own g_cfg stays untouched — net_cl_disconnect points the sim back.
      for (int i = 0; i < MV_COUNT; i++) {
        float v = np_rf(&r);
        g_cl.srv_mv[i] = v >= MV_DEF[i].lo ? (v <= MV_DEF[i].hi ? v : MV_DEF[i].hi)
                                           : MV_DEF[i].lo;
      }
      for (int wp = 0; wp < WPN_COUNT; wp++)
        for (int i = 0; i < WP_COUNT; i++) {
          float v = np_rf(&r);
          g_cl.srv_wp[wp][i] = v >= WP_DEF[i].lo ? (v <= WP_DEF[i].hi ? v : WP_DEF[i].hi)
                                                 : WP_DEF[i].lo;
        }
      g_sim_mv = g_cl.srv_mv;
      g_sim_wp = g_cl.srv_wp;
      g_world_dirty = 1;                       // renderer re-uploads the VBO
      // g_local_ent / g_online flip only on the FIRST snapshot: keep
      // playing the local match until the server's world actually arrives.
      g_cl.state = NC_PLAYING;
    } else if (type == NP_REJECT && g_cl.state != NC_PLAYING) {
      g_cl.reject = np_r8(&r);
      g_cl.state = NC_FAILED;
      return 0;
    } else if (type == NP_SNAPSHOT && g_cl.state == NC_PLAYING) {
      net_cl_snapshot(b, n);
    } else if (type == NP_RESCUE && g_cl.state == NC_PLAYING && g_cl.synced) {
      if (g_match.mode == GM_RESCUE) rescue_net_apply(b, (size_t)n);
    } else if (type == NP_SCORES && g_cl.state == NC_PLAYING && g_cl.synced) {
      // synced-gated: rows landing before the first snapshot would only be
      // wiped again by the sync memset — and a stale over=1 pinned the
      // VICTORY banner once already (see the !synced branch).
      // Score rows are replicated STATE (4 Hz): adopt them wholesale. The
      // event channel still carries the kill/hit presentation; these are the
      // numbers the scoreboard, the scoreline and the match-over banner read.
      net_score_t rows[MAX_ENTS];
      net_name_t names[MAX_HUMANS];
      uint8_t over = 0, winner = 0;
      uint16_t frag = 0;
      int nn = 0;
      net_mode_t nm = {0};
      int nr = net_scores_read(b, (size_t)n, &over, &winner, &frag, &nm, rows,
                               MAX_ENTS, names, MAX_HUMANS, &nn);
      if (nr >= 0) {
        // WHOLESALE MEANS WHOLESALE (fixed 2026-08-18). The rows and the
        // names used to be applied ADDITIVELY: an entity that dropped out of
        // the packet kept the kills, the ping and the display name it had
        // when it left, so a player who quit stayed on every remaining
        // client's scoreboard until the next match restart. The server
        // already drops him from the packet the moment net_srv_drop runs
        // (ENT_FOREACH skips an off human, and so does the name section) —
        // only the client kept the corpse of the row alive.
        match_counters_clear();
        memset(g_net_ping, 0, sizeof g_net_ping);
        for (int h = 1; h < MAX_HUMANS; h++) g_human_name[h][0] = '\0';
        for (int i = 0; i < nr; i++) {
          int e = rows[i].id;
          g_match.kills[e] = rows[i].kills;
          g_match.deaths[e] = rows[i].deaths;
          g_match.shots[e] = rows[i].shots;
          g_match.hits[e] = rows[i].hits;
          g_net_ping[e] = rows[i].ping;
          g_match.mscore[e] = rows[i].mscore;
          g_team[e] = (signed char)rows[i].team;
        }
        g_match.mode = nm.mode;
        if (nm.mode != GM_RESCUE) g_resc.on = 0;   // no mission drawn in another mode
        g_net_team_size = nm.team_size;
        g_net_team_fill = nm.team_fill & 1;
        g_net_team_ff = (nm.team_fill >> 1) & 1;
        g_team_score[0] = nm.tscore[0];
        g_team_score[1] = nm.tscore[1];
        // FIRST WORD OF A TEAM GAME: the player picks a side. The server has
        // already put us on one (team_auto), so this is a choice, not a gate.
        if (nm.mode == GM_TDM && !g_cl_team_asked) { g_cl_team_asked = 1; g_ui_team_prompt = 1; }
        g_match.round_t = nm.round_secs * TICK_HZ;   // display only on a client
        g_match.round_win = nm.round_win == 0xFF ? -1 : nm.round_win;
        g_match.round_no = nm.round_no;
        g_net_rounds = nm.rounds;
        for (int i = 0; i < nn; i++) {
          int h = human_of_ent(names[i].id);
          if (h >= 0)
            snprintf(g_human_name[h], sizeof g_human_name[h], "%s", names[i].name);
        }
        g_match.over = over ? 1 : 0;
        if (over) g_match.winner = winner == 0xFF ? 0 : winner;
        g_net_fraglimit = frag;
      }
    }
  }
  // timeouts / resends
  if (g_cl.state == NC_CONNECTING || g_cl.state == NC_JOINING) {
    if (now - g_cl.connect_start_ms > 4000) { g_cl.state = NC_FAILED; return 0; }  // connect timeout
    if (now - g_cl.last_send_ms > 250) {      // resend the last handshake step
      if (g_cl.state == NC_CONNECTING) net_cl_send_connect(); else net_cl_send_join();
    }
  } else if (g_cl.state == NC_PLAYING) {
    if (now - g_cl.last_recv_ms > 5000) { g_cl.state = NC_FAILED; return 0; }  // server gone
  }
  return g_cl.state != NC_FAILED;
}

