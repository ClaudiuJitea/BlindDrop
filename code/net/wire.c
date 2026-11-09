// code/net/wire.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// Netcode wire layer — SHARED CORE, above the platform split,
// exactly the idiom audio uses: encode/decode, validation, the connection
// automaton and the FNV world hash all live here where the harness can prove
// them; only socket()/sendto()/recvfrom() go below the split, twice. Every
// decoder here treats its input as HOSTILE: a public UDP port receives
// no well-formed output. Fixed sizes, exact length checks, every count and
// index clamped, every string force-terminated.
// ---------------------------------------------------------------------------

// PROTO is byte 0-1 of EVERY packet, hand-bumped on any wire change.
// An old client must fail LOUDLY (REJECT), never misinterpret a changed schema.
// 2: CONNECT and QUERY carry a length floor, the snapshot header carries the
// map seed, NP_LEAVE exists, and the entity record's weapon field is rejected
// rather than masked. A version-1 peer trips the PROTO check on byte 0 and is
// told so, which is the whole reason this number is hand-maintained.
// 3: the snapshot carries the EVENT SECTION after the entity list, and
// the input packet's event-ack field is finally read. A version-2 client would
// parse the entity list correctly and then read the event count as the start of
// nothing — exactly the silent misinterpretation this number exists to prevent.
// 5: the input record carries the VIEW TICK (lag comp + ping in one field), the
// snapshot header carries this client's input-queue depth (clock nudge), and
// NP_SCORES replicates the match's score rows — the scoreboard was all zeros
// online because g_match only ever mutates on the server.
// 8: map_generate produces a DIFFERENT ARENA for the same seed — the tactical
// walls now break into several collidable pieces (stepped tops, broken ends,
// see-through breaches) and draw from the map stream to decide it. The packet
// format is untouched, so a 7 client would parse every field correctly and then
// fail the world hash on the first snapshot instead — a clean refusal, but at
// the wrong boundary and with the wrong message. The version is what the
// handshake is for.
// 13: Harbor gains LADDERS (new solids, split parapets) and player_move_tick
// climbs them; a 12 client would mispredict every climb and fail the world hash.
// 14: HOSTAGE RESCUE goes online (NP_RESCUE, the squad/guard sides in
// team_mates); a 13 client would neither draw nor predict the mission.
// 15: the small maps re-dressed (FOUNDRY, BAZAAR, OUTPOST rebuilt, piers on
// every shell): a 14 client would build a different world and fail the hash.
// 16: LOADOUT VARIANTS. The entity record's weapon byte carries the shooter's
// rifle/pistol variant in its top bits and the input's buttons word carries the
// menu's choice in its top nibble — a 15 peer would reject both.
#define NET_PROTO 16   // 12: TEAM DEATHMATCH (NP_TEAM, NP_SCORES team block + team column)
// Floors on UNVERIFIED requests, so the answer to one is always smaller than
// the request was and neither responder can be pointed at a forged
// source address. 16 against the 7-byte COOKIE; 736 against the INSTANCES
// reply, whose own cap is 40 records x 18 bytes + 4 = 724.
#define NET_CONNECT_MIN 16
// Human slots one source address may hold (net_srv_admit). A cookie proves an
// address is reachable, never that it is a different person; NAT legitimately
// puts a few players behind one address, a slot-squatter takes all of them.
#define NET_JOIN_PER_HOST 4

typedef enum {
  NP_CONNECT = 1,   // client->server, unverified: PROTO + name, zero-padded to
                    // NET_CONNECT_MIN so the COOKIE answer is strictly SMALLER
                    // than the request and the port cannot be reflected
  NP_COOKIE,        // server->client: 8-byte stateless cookie
  NP_JOIN,          // client->server: CONNECT again WITH the cookie
  NP_ACCEPT,        // server->client: your id, map seed, solids hash
  NP_REJECT,        // server->client: reason (proto mismatch, full)
  NP_INPUT,         // client->server: input_tick(s) + event ack
  NP_SNAPSHOT,      // server->client: world state
  NP_LEAVE,         // client->server: I am going. Frees the slot NOW instead
                    // of at the 10 s idle timeout — without it a quit-and-
                    // rejoin arrives from a fresh ephemeral port, finds its
                    // old conn still holding a human slot, and can be refused
                    // an arena that is half empty.
  NP_SCORES,        // server->client, 4 Hz: over/winner/fraglimit + one score
                    // row per entity (kills/deaths/shots/hits/ping). Score is
                    // STATE, not an event stream — deriving it from EV_KILL
                    // breaks on any loss burst past the resend window and for
                    // every late joiner, so it rides its own low-rate packet
                    // (the hot snapshot stays inside the MTU at full roster).
  NP_TEAM,          // client->server: token + side (0/1). The server refuses a
                    // full side in silence; the next NP_SCORES says which side
                    // the client is really on.
  NP_RESCUE,        // server->client, 20 Hz while a HOSTAGE RESCUE runs: the
                    // mission's state (alarm, clocks, hostages, every guard's
                    // detection meter, the banner). STATE like NP_SCORES: a
                    // lost packet is repaired by the next one.
} np_type_t;

// Values are pinned: they ride the NP_REJECT payload, and 3/4 (bad cookie /
// bad name) were retired without producers — a bad cookie is a deliberate
// silent drop (replying would be a reflection vector) and names are
// sanitized with a PLAYERn fallback, never rejected.
typedef enum {
  NR_PROTO = 1, NR_FULL = 2,
  NR_WORLD = 5,   // client-side only: the server's arena hash is not ours
} np_reject_t;

// Client input for one tick: the wish vector is post-deadzone/curve
// (the server must never re-apply ITS pad settings), angles are QUANTIZED so
// the client predicts with the exact values the server uses — float-yaw
// prediction against an i16 server angle is a per-tick systematic bias, the
// signature of permanent rubber-banding.
typedef struct {
  uint32_t tick;
  uint32_t view;         // SERVER tick whose world this input was aimed at:
                         // last applied snapshot's tick + the dead-reckoning
                         // ticks since. One field, two consumers: the
                         // server's lag-comp rewind traces the shot against
                         // the world the shooter actually saw, and
                         // g_tick - view at consume time IS the round trip —
                         // the ping the scoreboard shows. 0 = not synced yet.
  uint32_t buttons;      // IN_COUNT outgrew 16 bits with the weapon slots
  uint8_t  loadout;      // the menu's loadout (loadout_pack), applied by the
                         // server on this human's next spawn. On the wire it
                         // is the top nibble of the buttons word (IN_COUNT <= 28)
  int16_t  yaw, pitch;   // yaw: full circle -> i16; pitch: +/-PITCH_MAX -> i16
  int16_t  wish_x, wish_y;
} net_input_t;

// A cursor-based writer/reader with hard bounds — the ONLY way bytes move, so
// no decode path can run past its buffer. `bad` latches on any overflow and
// every read after it returns 0, so a truncated packet yields zeros, never a
// wild read.
typedef struct { unsigned char *p; size_t n, at; int bad; } np_wr;
typedef struct { const unsigned char *p; size_t n, at; int bad; } np_rd;

static void np_w8(np_wr *w, uint8_t v) {
  if (w->at + 1 > w->n) { w->bad = 1; return; }
  w->p[w->at++] = v;
}
static void np_w16(np_wr *w, uint16_t v) { np_w8(w, v & 0xff); np_w8(w, v >> 8); }
static void np_w32(np_wr *w, uint32_t v) { np_w16(w, v & 0xffff); np_w16(w, v >> 16); }
static uint8_t np_r8(np_rd *r) {
  if (r->bad || r->at + 1 > r->n) { r->bad = 1; return 0; }
  return r->p[r->at++];
}
static uint16_t np_r16(np_rd *r) { uint16_t a = np_r8(r); return a | (uint16_t)np_r8(r) << 8; }
static uint32_t np_r32(np_rd *r) { uint32_t a = np_r16(r); return a | (uint32_t)np_r16(r) << 16; }
// Floats as raw IEEE-754 bits — ONLY for the ACCEPT tuning tables, where both
// sims must multiply with the exact same value. Range-clamped on decode; a
// hostile NaN fails the >= comparison and lands on the low bound.
static void np_wf(np_wr *w, float v) { uint32_t b; memcpy(&b, &v, 4); np_w32(w, b); }
static float np_rf(np_rd *r) {
  uint32_t b = np_r32(r); float v; memcpy(&v, &b, 4); return v;
}

// Angle quantization: yaw wraps a full turn to the i16 range, pitch
// maps +/-PITCH_MAX. The client dequantizes and predicts with THESE, so both
// sides see one value.
static int16_t net_q_yaw(float y)   { return (int16_t)lrintf(wrap_pi(y) * (32767.0f / F_PI)); }
static float   net_dq_yaw(int16_t q){ return (float)q * (F_PI / 32767.0f); }
static int16_t net_q_pitch(float p) { return (int16_t)lrintf(f_clamp(p, -PITCH_MAX, PITCH_MAX) * (32767.0f / PITCH_MAX)); }
static float   net_dq_pitch(int16_t q){ return f_clamp((float)q * (PITCH_MAX / 32767.0f), -PITCH_MAX, PITCH_MAX); }
static int16_t net_q_wish(float w)  { return (int16_t)lrintf(f_clamp(w, -1.0f, 1.0f) * 32767.0f); }
static float   net_dq_wish(int16_t q){ return f_clamp((float)q * (1.0f / 32767.0f), -1.0f, 1.0f); }

// The record carries NO type byte — NP_INPUT is the PACKET header's job
// (net_cl_send_inputs writes it once). It used to be written here too, and
// since the decoder never consumed it, the live server decoded every field
// one byte early: movement bits arrived as zero (the server player never
// walked — every snapshot dragged the predicting client back), W landed on
// the FIRE bit and the real trigger bit landed inside the yaw field. All
// three launch-day symptoms (rubber-banding, scrambled controls, no bullets)
// were this one byte; netpack stayed green only because the proof hand-
// skipped it. Encoder and decoder MUST stay field-for-field mirrors.
static void net_input_encode(np_wr *w, const net_input_t *in) {
  np_w32(w, in->tick);
  np_w32(w, in->view);
  np_w32(w, (in->buttons & 0x0FFFFFFFu) | (uint32_t)(in->loadout & 15) << 28);
  np_w16(w, (uint16_t)in->yaw);
  np_w16(w, (uint16_t)in->pitch);
  np_w16(w, (uint16_t)in->wish_x);
  np_w16(w, (uint16_t)in->wish_y);
}
// Returns 1 on a fully-consumed, valid record. The button mask is clamped to
// the defined actions; the angles cannot be out of range (they came through
// i16), but pitch is re-clamped on use anyway.
static int net_input_decode(np_rd *r, net_input_t *in) {
  in->tick   = np_r32(r);
  in->view   = np_r32(r);
  uint32_t raw = np_r32(r);
  in->buttons = raw & ((1u << IN_COUNT) - 1u);
  in->loadout = (uint8_t)(raw >> 28);
  if ((in->loadout & 3) >= WPN_VARS || (in->loadout >> 2 & 3) >= WPN_VARS) in->loadout = 0;
  in->yaw    = (int16_t)np_r16(r);
  in->pitch  = (int16_t)np_r16(r);
  in->wish_x = (int16_t)np_r16(r);
  in->wish_y = (int16_t)np_r16(r);
  return !r->bad;
}

// Per-entity snapshot record: the anim_tick INPUT list (pure physics
// state — anim_t/pose_t/vm_t are 100% presentation, re-solved by each client
// and NEVER sent) plus the state the HUD and the weapon model need. spawn_seq
// tells the client to anim_reset instead of interpolating across a respawn
// teleport.
typedef struct {
  uint8_t  id, flags;      // flags bit0 alive, bit1 grounded, bit2 crouched,
                           // bit3 sliding, bit4 knife guard up
  uint8_t  hp, weapon;     // weapon: 3 bits used...
  uint8_t  loadout;        // ...and the byte's top bits carry this on the
                           // wire: loadout_pack's nibble (rifle, pistol variant)
  uint8_t  spawn_seq;
  uint8_t  melee;          // swing for remote figures: variant << 6 | ticks / 2
  uint8_t  mag;            // rounds in the CURRENT weapon's magazine. Without
                           // it a lost reload press was invisible forever: the
                           // predicted HUD showed a full mag while the server
                           // dry-fired an empty one — "my gun shoots no
                           // bullets" with no way to reconcile out of it.
  int16_t  x, y, z;        // position, cm precision over +/-320 m
  int16_t  vx, vy, vz;     // velocity, cm/s
  int16_t  yaw, pitch;
  int16_t  lean;           // -1..1
} net_ent_t;

// The swing and guard a remote figure plays. Reconstructed, not simulated: only
// the presentation reads these on a client's copy of someone else.
#define net_melee_apply(wp, e) do {                                            \
    (wp)->melee_v = (e)->melee >> 6 > 2 ? 2 : (e)->melee >> 6;                 \
    (wp)->melee_t = ((e)->melee & 63) * 2;                                     \
    float ga_ = (e)->flags & 16 ? 1.0f : 0.0f;                                 \
    (wp)->ads_lin = (e)->weapon == WPN_KN ? ga_ : (wp)->ads_lin;               \
    if ((e)->weapon == WPN_KN) (wp)->ads_s = (wp)->prev_ads_s = ga_;           \
    if ((e)->weapon == WPN_GR) {                                               \
      (wp)->gr_t = (e)->melee >> 6 == 1 ? ((e)->melee & 63) * 2 : 0;           \
      (wp)->gr_cook = (e)->melee >> 6 == 2 ? NADE_PIN_T : 0;                   \
    }                                                                          \
  } while (0)

// --- The event channel ---------------------------------------------
// Every event carries its sim tick and a monotone id; the client acks the
// highest id it has seen on every input packet; the server resends everything
// above that ack for ~200 ms; the client drops any id <= its high-water mark.
// Without this the server simulated shots nobody could see: a client's OWN shot
// is predicted with ev_push suppressed (g_predicting), so the muzzle flash, the
// report, the tracer, the impact and the hitmarker can only ever arrive as
// server events. There were none — the server dropped its whole event list
// every tick and the snapshot had no room for one.
//
// EV_STEP/EV_LAND are deliberately NOT on the wire: they are pushed by each
// client's own animation solver from its own contact edge, so sending them
// would play every footstep twice.
//
// The payload is the quantized event_t, one fixed record. a/b both go through
// the position quantizer: b is a direction or a normal for some types, and cm
// resolution on a unit vector (0.01) is finer than any of its consumers.
// 20 records x 29 B = 580 B on top of the entity list's ~600 B, which keeps the
// datagram inside a 1400 B MTU at the roster ceiling. 36 ticks is 300 ms of
// resend window — one and a half of the design's 200 ms, so a client on a 150 ms
// link still gets a second chance at every event.
#define NET_EV_SNAP_MAX   20     // records per snapshot; the rest ride the
                                 // resend. 20 covers the measured 8-10 events
                                 // per tick at two ticks per snapshot; the
                                 // full datagram stays under ~1400 B (MTU)
#define NET_EV_RESEND_TCK 36     // ~300 ms at 120 Hz, then it ages out
typedef struct {
  uint32_t id;
  uint8_t  type, src, dst, wpn, k;
  int16_t  i, ax, ay, az, bx, by, bz, f, g, pen;
} net_ev_t;

static int16_t net_q_pos(float m)  { return (int16_t)lrintf(f_clamp(m, -327.0f, 327.0f) * 100.0f); }
static float   net_dq_pos(int16_t q){ return (float)q * 0.01f; }
static int16_t net_q_vel(float v)  { return (int16_t)lrintf(f_clamp(v, -327.0f, 327.0f) * 100.0f); }
static float   net_dq_vel(int16_t q){ return (float)q * 0.01f; }

static void net_ent_encode(np_wr *w, const net_ent_t *e) {
  np_w8(w, e->id); np_w8(w, e->flags);
  np_w8(w, e->hp); np_w8(w, (uint8_t)((e->weapon & 7) | (e->loadout & 15) << 3));
  np_w8(w, e->spawn_seq);
  np_w8(w, e->melee);
  np_w8(w, e->mag);
  np_w16(w, (uint16_t)e->x); np_w16(w, (uint16_t)e->y); np_w16(w, (uint16_t)e->z);
  np_w16(w, (uint16_t)e->vx); np_w16(w, (uint16_t)e->vy); np_w16(w, (uint16_t)e->vz);
  np_w16(w, (uint16_t)e->yaw); np_w16(w, (uint16_t)e->pitch);
  np_w16(w, (uint16_t)e->lean);
}
static void net_ev_encode(np_wr *w, const net_ev_t *e) {
  np_w32(w, e->id);
  np_w8(w, e->type); np_w8(w, e->src); np_w8(w, e->dst);
  np_w8(w, e->wpn);  np_w8(w, e->k);
  np_w16(w, (uint16_t)e->i);
  np_w16(w, (uint16_t)e->ax); np_w16(w, (uint16_t)e->ay); np_w16(w, (uint16_t)e->az);
  np_w16(w, (uint16_t)e->bx); np_w16(w, (uint16_t)e->by); np_w16(w, (uint16_t)e->bz);
  np_w16(w, (uint16_t)e->f);  np_w16(w, (uint16_t)e->g);  np_w16(w, (uint16_t)e->pen);
}
// Every field is read through the bounds-checked cursor; `type` is clamped to
// the defined range on USE (net_ev_to_live), never trusted as an array index
// here — a hostile server is still a decoder input like any other.
static int net_ev_decode(np_rd *r, net_ev_t *e) {
  e->id = np_r32(r);
  e->type = np_r8(r); e->src = np_r8(r); e->dst = np_r8(r);
  e->wpn = np_r8(r);  e->k = np_r8(r);
  e->i = (int16_t)np_r16(r);
  e->ax = (int16_t)np_r16(r); e->ay = (int16_t)np_r16(r); e->az = (int16_t)np_r16(r);
  e->bx = (int16_t)np_r16(r); e->by = (int16_t)np_r16(r); e->bz = (int16_t)np_r16(r);
  e->f = (int16_t)np_r16(r);  e->g = (int16_t)np_r16(r);  e->pen = (int16_t)np_r16(r);
  return !r->bad;
}
static void net_ev_from_live(net_ev_t *n, const event_t *e, uint32_t id) {
  n->id = id;
  n->type = (uint8_t)e->type;
  n->src = (uint8_t)(e->src < 0 ? 0xFF : e->src);
  n->dst = (uint8_t)(e->dst < 0 ? 0xFF : e->dst);
  n->wpn = (uint8_t)e->wpn; n->k = (uint8_t)e->k;
  n->i = (int16_t)(e->i < -32768 ? -32768 : e->i > 32767 ? 32767 : e->i);
  n->ax = net_q_pos(e->a.x); n->ay = net_q_pos(e->a.y); n->az = net_q_pos(e->a.z);
  n->bx = net_q_pos(e->b.x); n->by = net_q_pos(e->b.y); n->bz = net_q_pos(e->b.z);
  n->f = net_q_pos(e->f); n->g = net_q_wish(e->g); n->pen = net_q_pos(e->pen);
}
static void net_ev_to_live(event_t *e, const net_ev_t *n) {
  *e = (event_t){0};
  e->type = (ev_type_t)(n->type > EV_BLAST ? EV_IMPACT : n->type);
  // src/dst/wpn are ARRAY INDICES in every consumer (g_player_anim via
  // human_of_ent, WPN_DEF, g_bots) — a hostile server's 254 walked straight
  // into g_player_anim[234]. Out-of-range ids neutralize to -1 (no owner),
  // an out-of-range weapon to 0, mirroring net_ent_decode's rejection rule.
  e->src = n->src >= MAX_ENTS ? -1 : n->src;
  e->dst = n->dst >= MAX_ENTS ? -1 : n->dst;
  e->wpn = n->wpn >= WPN_COUNT ? 0 : n->wpn;
  e->k = n->k; e->i = n->i;
  e->a = (v3){{net_dq_pos(n->ax), net_dq_pos(n->ay), net_dq_pos(n->az)}};
  e->b = (v3){{net_dq_pos(n->bx), net_dq_pos(n->by), net_dq_pos(n->bz)}};
  e->f = net_dq_pos(n->f); e->g = net_dq_wish(n->g); e->pen = net_dq_pos(n->pen);
}
static int net_ent_decode(np_rd *r, net_ent_t *e) {
  e->id = np_r8(r); e->flags = np_r8(r);
  e->hp = np_r8(r);
  e->weapon = np_r8(r);
  e->loadout = (uint8_t)(e->weapon >> 3 & 15);
  e->weapon &= 7;
  e->spawn_seq = np_r8(r);
  e->melee = np_r8(r);
  e->mag = np_r8(r);
  e->x = (int16_t)np_r16(r); e->y = (int16_t)np_r16(r); e->z = (int16_t)np_r16(r);
  e->vx = (int16_t)np_r16(r); e->vy = (int16_t)np_r16(r); e->vz = (int16_t)np_r16(r);
  e->yaw = (int16_t)np_r16(r); e->pitch = (int16_t)np_r16(r);
  e->lean = (int16_t)np_r16(r);
  // Both fields are used as ARRAY INDICES by the caller (g_players/g_bots and
  // WPN_DEF/g_cfg.wp), so both are rejection criteria rather than masks: the
  // old `& 3` let a hostile server hand out weapon 2 and 3 against a
  // WPN_COUNT of 2, i.e. an out-of-bounds read per snapshot record.
  return !r->bad && e->id < MAX_ENTS && e->weapon < WPN_COUNT &&
         (e->loadout & 3) < WPN_VARS && (e->loadout >> 2 & 3) < WPN_VARS;
}

// FNV-1a over the generated solids: map_generate has NO transcendental
// calls, so box count/kind/order are bit-identical across platforms and only
// coordinates could differ by ULPs — the hash detects a divergent arena so the
// client REFUSES rather than playing a different map (EV_IMPACT.i indexes this
// exact list). 4-byte seed + 4-byte hash, fits any packet, no fragmentation.
static uint32_t net_world_hash(void) {
  // Cached per arena: the solids only change on map_generate (seed change),
  // but this used to rescan ~solids x 32 B per CLIENT per 60 Hz snapshot.
  static uint32_t cache, cache_seed = 0xffffffffu;
  static int cache_n = -1, cache_map = -1;
  if (cache_seed == g_map_seed && cache_n == g_num_solids && cache_map == g_map_id)
    return cache;
  uint32_t h = 2166136261u;
  const unsigned char *p = (const unsigned char *)g_solids;
  size_t n = (size_t)g_num_solids * sizeof g_solids[0];
  for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
  h = (h ^ (uint32_t)g_num_solids) * 16777619u;
  cache = h; cache_seed = g_map_seed; cache_n = g_num_solids; cache_map = g_map_id;
  return h;
}

// Angle validation: absolute angles per tick without checks hand the
// client two things server authority is supposed to hold. gun_shoot builds the
// direction as yaw + rec_y and clamp(pitch + rec_p) — recoil is ADDED to the
// client angle, so a modified client that subtracts the expected recoil has a
// laser. Even without anti-cheat this is mandatory: clamp pitch, wrap yaw, and
// cap the per-tick turn rate (a spinbot is otherwise free). NaN can't arrive
// through the i16 wire, but a validated float path stays the one true gate.
#define NET_MAX_TURN_PER_TICK 0.6f   // rad/tick (~4100 deg/s): far above human,
                                     // below a spinbot's instant flip
static void net_validate_angles(float *yaw, float *pitch, float prev_yaw) {
  // Bit tests, not isfinite(): under -ffast-math the library call folds away,
  // so the defence this line exists for was not actually compiled in.
  if (f_is_nonfinite(*yaw) || f_is_nonfinite(*pitch)) {
    *yaw = prev_yaw; *pitch = 0.0f; return;
  }
  *pitch = f_clamp(*pitch, -PITCH_MAX, PITCH_MAX);
  float dy = wrap_pi(*yaw - prev_yaw);
  if      (dy >  NET_MAX_TURN_PER_TICK) *yaw = wrap_pi(prev_yaw + NET_MAX_TURN_PER_TICK);
  else if (dy < -NET_MAX_TURN_PER_TICK) *yaw = wrap_pi(prev_yaw - NET_MAX_TURN_PER_TICK);
  else                                  *yaw = wrap_pi(*yaw);
}

// Stateless cookie: hash(secret, ip, port, time/30). No per-attempt
// state; the reply is SMALLER than the request, so the server is not an
// amplifier. The secret is process-random.
static uint64_t g_net_secret;
static uint32_t net_cookie(uint32_t ip, uint16_t port, uint32_t epoch) {
  uint64_t h = g_net_secret ^ 0xcbf29ce484222325ull;
  uint64_t v = (uint64_t)ip << 16 | port;
  h = (h ^ v) * 0x100000001b3ull;
  h = (h ^ epoch) * 0x100000001b3ull;
  return (uint32_t)(h ^ (h >> 32));
}
static int net_cookie_valid(uint32_t ip, uint16_t port,
                            uint32_t got, uint32_t now_epoch) {
  return got == net_cookie(ip, port, now_epoch) ||
         got == net_cookie(ip, port, now_epoch - 1);   // one epoch of slack
}

// Input bounds: the lag-switch aimbot is a client that stalls, then
// dumps 60 backdated inputs in one frame, each shot rewound to a different
// time against victims who saw nothing move. Enforced at the queue insert
// (net_srv_queue_input): at most NET_IN_MAX buffered ticks per client, at
// most 1.25x real-time fresh ticks per second, consumed one per world tick;
// the fill rule (hold the last state across a gap) keeps client and server
// agreeing on what the missing ticks did. `netstall` proves all of it against
// the LIVE queue machinery.
// 64 ticks (~530 ms) of buffer, NOT a latency target: in steady state the
// backlog drop in net_srv_consume_inputs holds the queue near zero. The
// window exists for a SERVER stall (CPU steal on a VPS, a paused VM): the
// inputs that pile up while the sim stands still are replayed one per catch-
// up tick afterwards — with a 16-tick window a 450 ms stall silently threw
// away ~38 of them and every stall became a hard position snap.
#define NET_IN_MAX      64           // hard cap on buffered input ticks
#define NET_IN_RATE_NUM 5            // <= 1.25x real-time ticks per second...
#define NET_IN_RATE_DEN 4            // ...i.e. 5/4
// Backlog above which consume trims oldest ticks (buttons carried). 12 ticks
// = 100 ms of jitter/frame-burst absorption. The OLD threshold of 4 was the
// live rubber-banding: a 60 Hz client hitching for 50 ms sends a 6-tick input
// burst, 6 > 4 trimmed two REAL moves the client had already predicted — one
// visible correction per hitch, measured 251 corrections in 30 s over WAN.
// Trimming is now the emergency exit only; the CLIENT drains standing depth
// by nudging its tick clock toward NET_QDEPTH_TARGET (snapshot qdepth byte).
#define NET_IN_BACKLOG  12
#define NET_QDEPTH_TARGET 2          // ~1 consumable + 1 tick of jitter margin
// Ceiling on the lag-comp rewind(rule 2): 30 ticks = 250 ms covers any
// playable ping; beyond it favour-the-shooter starts shooting the past.
#define NET_LAGCOMP_MAX 30
// Each input packet carries the last N ticks, oldest first. UDP loss then
// costs nothing until N consecutive packets die (~1 in 10^5 at 5% loss):
// a fire tap or a jump press is an EDGE — unlike held movement it cannot be
// reconstructed from the next packet. Redundant records are dropped by tick
// on the server before the rate budget, so this does not eat the rate cap.
#define NET_IN_REDUN    4
// (Live snapshot build/read moved below the sim structs — they read g_players/g_bots.)


#define PLAYER_HP            100
#define PLAYER_RESPAWN_TICKS 120  // 1 s death screen

// The player's eye WITHOUT the lean — the sim's own reference point.
static v3 player_eye_base(const player_t *p) {
  return (v3){{p->pos.x, p->pos.y + p->eye, p->pos.z}};
}

// Lateral world offset the lean has moved the upper body by, at lean amount l.
// Right-hand side of the aim yaw, so +1 is a right lean.
static v3 lean_offset(float yaw, float l) {
  float k = lean_eye_off(l);
  return (v3){{cosf(yaw) * k, 0, sinf(yaw) * k}};
}

// Where the player actually looks and shoots FROM. Peeking has to move the shot
// origin or the whole mechanic is a camera trick: the point of leaning out is
// that the round leaves from where the eye is, past the corner the body is
// still behind.
static v3 player_eye_pos(const player_t *p) {
  v3 e = player_eye_base(p);
  e = v3_add(e, lean_offset(p->yaw, p->lean));
  e.y -= lean_eye_drop(p->lean);
  return e;
}

static void player_spawn(player_t *p) {
  memset(p, 0, sizeof *p);
  p->pos = SPAWN_POS;
  p->prev_pos = p->pos;
  p->lean_clr = 1.0f;
  p->eye = p->prev_eye = EYE_STAND;
  p->grounded = p->prev_grounded = 1;  // spawn point is on the floor
  p->alive = 1;
  p->hp = PLAYER_HP;
  wpn_fill_mags(&p->wp);
}

// MOUSE LOOK, and the ADS multiplier lives HERE rather than at the three call
// sites — the online fork, the singleplayer path and the harness `look` command
// all have to feel the same, and the harness one is what a proof measures. It
// is applied as a factor on the whole step, so it scales yaw and pitch by the
// same amount: a mouse player's ADS sensitivity is one number, not two.
// ads_sense 1.00 makes this the identity at every point of the ramp.
static float mouse_ads_look_scale(const player_t *p) {
  float ads = p->wp.ads_s;
  if (ads <= 0) return 1.0f;
  return 1.0f + (g_cfg.ads_sense - 1.0f) * ads;
}

static void player_look(player_t *p, float dx, float dy, float sense) {
  float k = sense * mouse_ads_look_scale(p) * SENSE_RAD_PER_COUNT;
  p->yaw += dx * k;
  p->pitch = f_clamp(p->pitch - dy * k, -PITCH_MAX, PITCH_MAX);
}

// Generic AABB body helpers — shared by the player and the bot so both
// collide with the world through the exact same arithmetic.
static int aabb_fits(v3 pos, float radius, float height) {
  v3 mn = {{pos.x - radius, pos.y, pos.z - radius}};
  v3 mx = {{pos.x + radius, pos.y + height, pos.z + radius}};
  BP_BOX(i, mn.x, mn.z, mx.x, mx.z) {
    const solid_t *s = &g_solids[i];
    if (mx.x > s->min.x && mn.x < s->max.x &&
        mx.y > s->min.y && mn.y < s->max.y &&
        mx.z > s->min.z && mn.z < s->max.z) return 0;
  }
  return 1;
}

static int player_fits(const player_t *p, float height) {
  return aabb_fits(p->pos, PLAYER_RADIUS, height);
}

// The collision height of a live body: standing, or folded from the TOP while
// crouched (the box follows the smoothed eye down, never below the crouch
// floor). ONE copy — the move tick and the client's adopt-time depenetration
// must agree on it to the bit.
static float player_col_height(const player_t *p) {
  if (!p->crouched) return HEIGHT_STAND;
  float h = p->eye + (HEIGHT_STAND - EYE_STAND);
  return h < HEIGHT_CROUCH ? HEIGHT_CROUCH : h;
}

static void move_axis(v3 *pos, v3 *vel, int *grounded, int axis,
                      float radius, float height, float dt) {
  pos->e[axis] += vel->e[axis] * dt;
  float ext[3] = {radius, height, radius};
  // The candidate region has a metre of slack: resolving one solid moves the
  // body back along this axis, and the ones it is then tested against must
  // already be in the set.
  BP_BOX(i, pos->x - radius - 1.0f, pos->z - radius - 1.0f,
         pos->x + radius + 1.0f, pos->z + radius + 1.0f) {
    const solid_t *s = &g_solids[i];
    v3 mn = {{pos->x - radius, pos->y, pos->z - radius}};
    v3 mx = {{pos->x + radius, pos->y + height, pos->z + radius}};
    if (!(mx.x > s->min.x && mn.x < s->max.x &&
          mx.y > s->min.y && mn.y < s->max.y &&
          mx.z > s->min.z && mn.z < s->max.z)) continue;
    // Step up a low edge on a horizontal move, if the body fits on top of it.
    // aabb_fits tests the whole set at the raised position, so a step under a
    // low ceiling, or into a wall, still blocks. Off (STEP_H 0) on the legacy
    // maps, so their collision is unchanged to the bit.
    if (axis != 1 && STEP_H > 0.0f) {
      float rise = s->max.y - pos->y;
      if (rise > 0.0f && rise <= STEP_H &&
          aabb_fits((v3){{pos->x, s->max.y + 0.001f, pos->z}}, radius, height)) {
        pos->y = s->max.y + 0.001f;
        continue;
      }
    }
    if (vel->e[axis] > 0) {
      // ext[axis] is the height on y: moving up hits a box bottom with the full
      // body height (contrast the +0 on the downward line — the origin is at
      // the feet).
      pos->e[axis] = s->min.e[axis] - ext[axis];
    } else if (vel->e[axis] < 0) {
      pos->e[axis] = s->max.e[axis] + (axis == 1 ? 0 : ext[axis]);
      if (axis == 1) *grounded = 1;
    }
    vel->e[axis] = 0;
  }
  if (axis == 1 && pos->y <= 0) {  // arena floor
    pos->y = 0;
    if (vel->y < 0) vel->y = 0;
    *grounded = 1;
  }
}

// One tick of the collision sweep for ANY entity. The axis order — x, z, then y
// — is load-bearing: a landing is decided against the already-resolved
// horizontal position, so a body that slid along a wall this tick lands on the
// floor it actually ended over. It was an unnamed convention copied at four
// sites (player, live bot, puppet, corpse) and one of them reordering is not
// something any proof in this file would catch.
static void ent_move(v3 *pos, v3 *vel, int *grounded, float height) {
  move_axis(pos, vel, grounded, 0, PLAYER_RADIUS, height, TICK_DT);
  move_axis(pos, vel, grounded, 2, PLAYER_RADIUS, height, TICK_DT);
  move_axis(pos, vel, grounded, 1, PLAYER_RADIUS, height, TICK_DT);
}

// Keep an entity inside the arena. Deliberately SEPARATE from ent_move: bot_tick
// runs its stand-up headroom check between the sweep and this clamp, and fusing
// the two would evaluate headroom at the clamped position — a silent crouch
// behaviour change at the arena edge.
static void arena_clamp(v3 *pos) {
  pos->x = f_clamp(pos->x, -ARENA_HALF + PLAYER_RADIUS, ARENA_HALF - PLAYER_RADIUS);
  pos->z = f_clamp(pos->z, -ARENA_HALF + PLAYER_RADIUS, ARENA_HALF - PLAYER_RADIUS);
}

