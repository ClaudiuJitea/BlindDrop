// code/sim/ballistics.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ===========================================================================
// PHYSICAL BALLISTICS (the `ballistics` option, OFF by default). With it on,
// a rifle or pistol round is a body in flight rather than a ray: it leaves at
// its muzzle velocity, loses speed to quadratic drag, falls under gravity, and
// is swept one tick at a time against the same world trace and the same
// entity boxes the hitscan path uses. A round that meets steel at a grazing
// angle ricochets once, losing most of its energy. Deterministic (no rng) and
// authoritative only: an online client keeps the hitscan path, so the option
// changes nothing about the netcode contract when it is off, and nothing about
// a server's when the server has not enabled it (`--ballistics`).
// ===========================================================================
static int g_ballistics;
static int g_ballistics_srv;   // --ballistics: a server's own switch
typedef struct { v3 pos, vel, o; int ent, wpn, life, rico, live; } proj_t;
#define MAX_PROJ 96
static proj_t g_proj_all[SRV_LOBBIES][MAX_PROJ];
#define g_proj (g_proj_all[g_lobby])
// Muzzle velocity (m/s) and drag (1/m, in dv/dt = -k|v|v) per weapon.
static float proj_v0(int wpn) { return wpn == WPN_SR ? 850.0f : wpn == WPN_PT ? 360.0f : 910.0f; }
static float proj_k(int wpn)  { return wpn == WPN_SR ? 0.00028f : wpn == WPN_PT ? 0.0016f : 0.00062f; }
static void proj_spawn(int ent, int wpn, v3 o, v3 dir) {
  for (int i = 0; i < MAX_PROJ; i++)
    if (!g_proj[i].live) {
      g_proj[i] = (proj_t){.pos = o, .vel = v3_scale(dir, proj_v0(wpn)), .o = o,
                           .ent = ent, .wpn = wpn, .life = TICK_HZ * 2, .live = 1};
      return;
    }
}
static void proj_clear(void) { memset(g_proj, 0, sizeof g_proj); }
static void proj_tick(void) {
  for (int i = 0; i < MAX_PROJ; i++) {
    proj_t *q = &g_proj[i];
    if (!q->live) continue;
    if (--q->life <= 0) { q->live = 0; continue; }
    float sp = v3_len(q->vel);
    // Semi-implicit: drag and gravity first, then the sweep over the new
    // velocity — the segment drawn is the segment flown.
    q->vel = v3_scale(q->vel, 1.0f / (1.0f + proj_k(q->wpn) * sp * TICK_DT));
    q->vel.y -= 9.81f * TICK_DT;
    sp = v3_len(q->vel);
    float seg = sp * TICK_DT;
    v3 d = v3_scale(q->vel, 1.0f / sp);
    v3 wn = {{0, 1, 0}};
    int ws = -1;
    float wall = trace_world(q->pos, d, &wn, &ws);
    float hd = fminf(wall, seg);
    int part = 0;
    int hit = ent_trace(q->ent, q->pos, d, &hd, &part);
    if (hit >= 0) {
      v3 at = v3_add(q->pos, v3_scale(d, hd));
      const wpn_def_t *def = &WPN_DEF[q->wpn];
      // Energy on arrival, not at the muzzle: a pistol round at 60 m has shed
      // a third of its speed and lands as the lighter hit it is.
      float ek = sp / proj_v0(q->wpn);
      int dmg = (int)((float)(def->dmg * (part == 3 ? def->head_x : 1)) * fminf(1.0f, 0.35f + 0.65f * ek * ek) + 0.5f);
      ent_damage(hit, dmg, part, q->ent, q->o, at);
      q->live = 0;
      continue;
    }
    if (wall <= seg) {
      v3 at = v3_add(q->pos, v3_scale(d, wall));
      int metal = ws >= 0 && ws < g_num_solids && smat_metal(g_solid_mat[ws]);
      float cosi = -v3_dot(d, wn);
      if (metal && cosi < 0.26f && q->rico == 0) {
        // RICOCHET: the round skips off the plate, keeping ~45% of its speed.
        ev_push((event_t){.type = EV_IMPACT, .a = at, .b = wn, .i = ws, .k = 1,
                          .f = v3_len(v3_sub(at, q->o)), .src = q->ent});
        v3 r = v3_sub(d, v3_scale(wn, 2.0f * v3_dot(d, wn)));
        q->vel = v3_scale(r, sp * 0.45f);
        q->pos = v3_add(at, v3_scale(wn, 0.002f));
        q->rico = 1;
        continue;
      }
      ev_push((event_t){.type = EV_IMPACT, .a = at, .b = wn, .i = ws,
                        .f = v3_len(v3_sub(at, q->o)), .src = q->ent});
      q->live = 0;
      continue;
    }
    q->pos = v3_add(q->pos, v3_scale(d, seg));
    if (q->pos.y < -1.0f) q->live = 0;
  }
}

static void gun_shoot(int ent, weapon_t *w, v3 o, float yaw,
                      float pitch, v3 vel, int grounded, uint64_t *rng,
                      anim_t *anim) {
  const wpn_def_t *def = wdef(w, w->cur);
  float fb[WP_COUNT];
  const float *wp = gun_feel(w, w->cur, fb);
  w->ammo[w->cur]--;
  w->fresh = 0;
  g_match.shots[ent]++;
  // HOSTAGE RESCUE: the squad's rifle and pistol are suppressed, and a human
  // of the squad firing frees the squad bots to fire with him.
  bot_noise_emit(ent, BOT_NOISE_SHOT, o,
                 rescue_on() && resc_is_squad(ent) ? (w->cur == WPN_SR ? 15.0f : 11.0f) : 34.0f);
  if (rescue_on() && human_of_ent(ent) >= 0) g_resc.fire_free_t = 4 * TICK_HZ;
  w->cool[w->cur] = (int)((float)TICK_HZ * 60.0f / wp[WP_RPM] + 0.5f);
  if (w->cool[w->cur] < 1) w->cool[w->cur] = 1;

  // Spread is sampled BEFORE this shot's bloom is added: the first shot of a
  // burst fires at base accuracy, the penalty always trails by one shot.
  float spread = gun_spread(w, vel, grounded);
  w->bloom += wp[WP_BLOOM];
  if (w->bloom > GUN_BLOOM_CAP) w->bloom = GUN_BLOOM_CAP;

  float ya = yaw + w->rec_y * DEG2RAD;
  // The view punch counts too. The camera is pitched by rec_p + punch, so during
  // a spray a shot that ignored the punch left 0.1-0.17 deg BELOW the crosshair:
  // 6 cm at 20 m, every round after the first. This is the crosshair's own line.
  float pa = f_clamp(pitch + (w->rec_p + w->punch) * DEG2RAD, -PITCH_MAX, PITCH_MAX);
  v3 dir = {{cosf(pa) * sinf(ya), sinf(pa), -cosf(pa) * cosf(ya)}};
  float r = spread * DEG2RAD * sqrtf(rng_rangef(rng, 0.0f, 1.0f));
  float phi = rng_rangef(rng, 0.0f, F_TAU);
  v3 right = v3_norm(v3_cross(dir, (v3){{0, 1, 0}}));
  v3 upv = v3_cross(right, dir);
  dir = v3_norm(v3_add(dir, v3_add(v3_scale(right, r * cosf(phi)),
                                   v3_scale(upv, r * sinf(phi)))));

  // The kick lands AFTER the shot left the barrel.
  w->rec_p += wp[WP_KICK_UP] * rng_rangef(rng, 0.9f, 1.1f);
  w->rec_y += wp[WP_KICK_SIDE] *
              (GUN_PATTERN[w->idx & 31] + rng_rangef(rng, -0.2f, 0.2f));
  w->rec_p = f_clamp(w->rec_p, -GUN_REC_CAP, GUN_REC_CAP);
  w->rec_y = f_clamp(w->rec_y, -GUN_REC_CAP, GUN_REC_CAP);
  w->punch_v += GUN_PUNCH_SHOT * wp[WP_PUNCH] * GUN_PUNCH_V;
  // The roll's SIGN comes from the pattern step this shot just took, so the
  // view rolls into the direction the muzzle is being pulled. Zero steps (the
  // pattern legitimately has them) fall back to the shot index's parity, or the
  // roll would silently vanish on those rounds.
  {
    float s = GUN_PATTERN[w->idx & 31];
    w->punch_roll_v = (s > 0.001f ? 1.0f : s < -0.001f ? -1.0f
                                   : (w->idx & 1) ? 1.0f : -1.0f) *
                      GUN_PUNCH_SHOT * wp[WP_PUNCH] * GUN_PUNCH_V * GUN_ROLL_K
                      // automatic fire rocked the picture side to side at the
                      // fire rate; the single-shot sniper keeps its full roll
                      * (w->cur == WPN_SR ? 1.0f : 0.4f);
  }
  // SPLIT, not converted (the one line that fused both meanings): shots++ is
  // INDEX — every human's lifetime counter, the trace contract — while the
  // viewmodel kick is LOKAL, per-client. As one LOKAL block the client
  // prediction loses the gun kick; as one INDEX block foreign fire kicks
  // human 0's viewmodel.
  int sh = human_of_ent(ent);
  if (sh >= 0) g_players[sh].shots++;
  if (ent == g_local_ent && sh >= 0) {
    g_players[sh].vm.kick_v += w->cur == WPN_SR ? 0.8f + 0.5f * wp[WP_PUNCH]  // gun slams back
                                                : 0.42f + 0.26f * wp[WP_PUNCH];
    // (The AR and pistol had 0.8 + 0.5 p before 0.7: at the AR's 12.5 rounds a second the gun jumped
    // a full 8 mm and 4 deg every round and read as shaking, not recoiling.)
    // ...and TWISTS: the bore is above the grip, so the recoil impulse is a
    // torque as well as a push. Its sign and size come from the shot index
    // (a hash, no rng draw: the gun stream stays exactly what it was).
    uint32_t th = (uint32_t)w->idx * 2654435761u ^ 0x9E37u;
    float tw = ((float)(th >> 8 & 0xFFFF) / 65535.0f - 0.35f);
    g_players[sh].vm.twist_v += tw * (w->cur == WPN_SR ? 1.0f + 0.7f * wp[WP_PUNCH]
                                                      : 0.45f + 0.3f * wp[WP_PUNCH])
                                 * (1.0f - 0.6f * w->ads_s);
  }
  if (anim) {
    anim->fire_k = 1.0f;                          // third-person body: gun kick
    // ...and the weapon is ON the line when the round leaves. A figure caught at
    // the low ready used to fire from a barrel pointing 34 degrees off the shot,
    // with the tracer leaving it sideways. The same tick's pose solve sees this.
    anim->ready_s = 1.0f;
  }

  // THE ROUND'S FLIGHT, one segment per piece of material it crosses. A shot
  // that meets nothing runs this loop exactly once and does exactly what it did
  // before: one trace_world, one ent_trace. Everything below only happens on a
  // shot that actually hits a wall AND has budget left, which is the whole cost
  // argument — a penetrating shot is a handful more AABB scans, and shots are
  // rare next to the LOS traces the AI already runs every tick.
  if (g_ballistics && !g_online && !g_predicting &&
      (w->cur == WPN_AR || w->cur == WPN_SR || w->cur == WPN_PT)) {
    // A body in flight (see proj_tick). The tracer still leaves now, down the
    // line the round started on; what it hits arrives as an EV_IMPACT later.
    proj_spawn(ent, w->cur, o, dir);
    ev_push((event_t){.type = EV_FIRE, .a = v3_add(o, v3_scale(dir, GUN_RANGE)), .b = dir,
                      .f = GUN_RANGE, .g = w->ads_s, .i = w->idx, .k = 0,
                      .wpn = w->cur, .src = ent, .dst = -1});
    w->idx++;
    return;
  }
  v3 nrm = {{0, 1, 0}};                 // the face the round finally stopped on
  v3 so = o;                            // this segment's origin
  float base = 0.0f, used = 0.0f;       // travelled so far / spent in material
  float dist = 0.0f;
  int part = 0, hit = -1, wsol = -1, crossings = 0;
  for (;;) {
    v3 wn = {{0, 1, 0}};
    int ws = -1;
    float wall = trace_world(so, dir, &wn, &ws);
    float d = wall;
    hit = ent_trace(ent, so, dir, &d, &part);       // nearer than the wall
    if (hit >= 0) { dist = base + d; break; }
    if (wall >= GUN_RANGE) { dist = base + wall; nrm = wn; wsol = ws; break; }
    v3 entry = v3_add(so, v3_scale(dir, wall));
    float left = PEN_PATH - used;
    // The arena floor plane is not a solid and has no far side, so `ws < 0`
    // stops the round by construction — a round may not tunnel through the
    // ground the way it may through a crate.
    // The `pen` flag is tested HERE as well as inside pen_span, and that is not
    // belt and braces: pen_span only consults the flag for solids that contain
    // its probe point, and the probe starts PEN_EPS past the entry face — so a
    // ray whose chord through the box is shorter than 1.5 mm finds nothing,
    // returns "crossed, zero material", and the round sails through the arena
    // SHELL at its corners. Measured before this line: aiming 0.4 mm apart at
    // the shell's top edge gave `hit=solid d=4.30` and then `hit=none d=-1.00`
    // with the round 210 m outside the world.
    float span = (ws >= 0 && g_solids[ws].pen && crossings < PEN_MAX &&
                  left > 0.0f) ? pen_span(entry, dir) : -1.0f;
    // A round that leaves the far side UNDERGROUND has not gone through
    // anything, it has buried itself: the arena floor is a plane rather than a
    // solid, so nothing below y = 0 would ever stop it and it would fly under
    // the map to GUN_RANGE. Reachable on any box standing on the ground — the
    // 0.55 m centre platform is the obvious one — shot at from above.
    if (span >= 0.0f && v3_add(entry, v3_scale(dir, span)).y <= PEN_EPS)
      span = -1.0f;
    if (span < 0.0f || span > left) {               // it stops in the material
      dist = base + wall; nrm = wn; wsol = ws;
      break;
    }
    // THROUGH. The entry hole is the one the shooter sees and is the only extra
    // event this costs; the exit is on the far face, inside cover, and paying a
    // second event plus a backward trace for it every crossing is not worth a
    // decal nobody is looking at.
    // `k = 1` marks a THROUGH hole. It still throws its sparks and stamps its
    // decal — the shooter is looking straight at it — but the audio layer skips
    // it: a penetrating round would otherwise spend two impact voices where a
    // stopped one spends one, and measured over five seeds at 12 bots that took
    // the pool from dropped=0 to dropped=2 on the reference configuration
    // MAX_VOICES is sized against. The entry crack is the quieter half
    // of the pair and is masked by the shot and the hit confirmation anyway.
    // `f` is the RANGE the round travelled, which the VFX layer needs for the
    // angular size of the burst — the same quantity EV_FIRE already carries and
    // the kill burst already takes from its own wound latch. Reading it without
    // setting it here is exactly how the first version of the angular scale did
    // nothing at all: `f` defaulted to 0, the fallback clamped every impact to
    // the reference distance, and a contact hit rendered at full size.
    ev_push((event_t){.type = EV_IMPACT, .a = entry, .b = wn, .i = ws, .k = 1,
                      .f = v3_len(v3_sub(entry, o)), .src = ent});
    used += span;
    base += wall + span;
    so = v3_add(entry, v3_scale(dir, span));
    // ...AND THE SIDE THE ENEMY IS STANDING ON. Measured through a 0.60 m cover
    // wall with the camera moved to the far side, the ENTRY face changed 109
    // pixels and the EXIT face exactly ZERO — so the one mechanic this file
    // calls "mostly a PLAYER power" left no read at all where it matters, for
    // the shooter watching in third person and for the man it is being shot
    // through. The exit point is already computed one line up as `so`; the only
    // missing datum is its NORMAL, and that is a six-way min over the box the
    // round is leaving — no second trace. `k = 1` keeps it silent, exactly as
    // the entry hole is, so this costs zero voices and one event slot out of
    // 128 against a measured ev_peak of 8-9.
    if (ws >= 0 && ws < g_num_solids) {
      // pen_span WELDS abutting solids, so the exit face can belong to a LATER
      // box in the run than the one the round entered — measure the normal and
      // clip the decal against the box the round actually LEAVES: the one
      // containing a probe point just short of the exit. Falls back to the
      // entry box for the single-box crossing, which is the common case.
      int xs = ws;
      v3 pp = v3_sub(so, v3_scale(dir, PEN_EPS));
      BP_BOX(si, pp.x, pp.z, pp.x, pp.z) {
        const solid_t *sb = &g_solids[si];
        if (pp.x >= sb->min.x && pp.x <= sb->max.x && pp.y >= sb->min.y &&
            pp.y <= sb->max.y && pp.z >= sb->min.z && pp.z <= sb->max.z) {
          xs = si;
          break;
        }
      }
      v3 xn = {{0, 0, 0}};
      float bestd = 1e9f;
      for (int ax = 0; ax < 3; ax++) {
        float dlo = fabsf(so.e[ax] - g_solids[xs].min.e[ax]);
        float dhi = fabsf(so.e[ax] - g_solids[xs].max.e[ax]);
        if (dlo < bestd) { bestd = dlo; xn = (v3){{0, 0, 0}}; xn.e[ax] = -1.0f; }
        if (dhi < bestd) { bestd = dhi; xn = (v3){{0, 0, 0}}; xn.e[ax] =  1.0f; }
      }
      ev_push((event_t){.type = EV_IMPACT, .a = so, .b = xn, .i = xs, .k = 1,
                        .f = v3_len(v3_sub(so, o)), .src = ent});
    }
    crossings++;
  }
  int kind = hit >= 0 ? part : dist < GUN_RANGE ? 1 : 0;
  v3 at = v3_add(o, v3_scale(dir, dist));
  ev_push((event_t){.type = EV_FIRE, .a = at, .b = dir, .f = dist,
                    .g = w->ads_s, .pen = used, .i = w->idx, .k = kind,
                    .wpn = w->cur, .src = ent, .dst = hit});
  if (hit >= 0 && !g_predicting) {   // damage is authoritative — server only
    // A ROUND THAT ARRIVES ARRIVES WHOLE. `used` is not read here at all any
    // more — the wall was already paid for by the round either fitting inside
    // PEN_PATH or being left in the material, and there is nothing between
    // those two states. The event still carries `used` because scripts assert
    // on it and it is the only thing that tells a wallbang from a clean hit.
    ent_damage(hit, def->dmg * (part == 3 ? def->head_x : 1), part, ent, o, at);
  } else if (kind == 1) {
    // `i` carries the solid the round entered — EV_IMPACT leaves it unused, so
    // the mark's clip window costs nothing in the event ring.
    ev_push((event_t){.type = EV_IMPACT, .a = at, .b = nrm, .i = wsol,
                      .f = dist, .src = ent});
  }
  w->idx++;
}

// Per-tick weapon housekeeping every shooter needs: cooldown, the reload
// countdown, recoil recovery once the trigger is off, and the bloom/punch decay.
static void gun_cool(weapon_t *w) {
  for (int i = 0; i < WPN_COUNT; i++)
    if (w->cool[i] > 0) w->cool[i]--;
  if (w->switch_t > 0) w->switch_t--;
  if (w->stagger_t > 0) w->stagger_t--;
  w->prev_block_k = w->block_k;
  w->block_k *= expf(-11.0f * TICK_DT);
  // THE VIEW PUNCH STEPS HERE, at the top of the tick, for both shooters —
  // gun_cool is the one hook that already runs before gun_shoot in the player
  // path AND in bot_tick. Stepped in gun_recover (after the shot) the impulse
  // was decayed 34% before the frame that was supposed to show it. Impulse
  // goes into the VELOCITY, so the angle rises over ~19 ms, and the spring's
  // slight undershoot is what makes it read as a snap-and-settle rather than
  // as a monotone lift. See GUN_PUNCH_K / GUN_PUNCH_C for the peak arithmetic.
  w->punch_v += (-GUN_PUNCH_K * w->punch - GUN_PUNCH_C * w->punch_v) * TICK_DT;
  w->punch += w->punch_v * TICK_DT;
  w->punch_roll_v += (-GUN_PUNCH_K * w->punch_roll
                      - GUN_PUNCH_C * w->punch_roll_v) * TICK_DT;
  w->punch_roll += w->punch_roll_v * TICK_DT;
}

static void gun_reload_tick(weapon_t *w) {
  if (w->reload_t > 0 && --w->reload_t == 0) {
    w->ammo[w->cur] = wdef(w, w->cur)->mag;
    w->idx = 0;
  }
}

// ADS ramps linearly over the weapon's own ADS TIME; presentation reads the
// smoothstep. This is the one per-tick weapon step that used to be written out
// per shooter instead of joining the family, with the bot's copy spelling
// g_cfg.wp[cur] out by hand — so ADS TIME was the one handling number a bot
// could drift on without `parity` noticing.
static void gun_ads(weapon_t *w, int want_ads) {
  float fb[WP_COUNT];
  w->ads_lin = f_clamp(w->ads_lin + (want_ads ? 1.0f : -1.0f) *
                       TICK_DT * 1000.0f / gun_feel(w, w->cur, fb)[WP_ADS_MS],
                       0.0f, 1.0f);
  w->ads_s = smooth01(w->ads_lin);
}

static void gun_recover(weapon_t *w, int trigger_held) {
  float fb[WP_COUNT];
  const float *wp = gun_feel(w, w->cur, fb);
  // TRIGGER TRAVEL, and it belongs here for the same reason `mv_walk` is shared:
  // the player and every bot pull the same trigger, and a finger that presses
  // only in first person is a finger that does not exist on the twenty models
  // the player is actually shooting at. It is presentation — nothing reads it
  // back into aim — but it is presentation off a SIM quantity, so it is
  // deterministic and it interpolates like every other pose input.
  // Fast in, slower out: a trigger break is a snap and the reset is a spring.
  // 36/s and 60/s, i.e. 90% in 7 ticks (58 ms) and in 4 ticks (33 ms). The old
  // 220/90 were not rates at all: 220 * (1/120) = 1.833 clamps to 1, so the
  // press was a one-tick STEP and the 11-frame press battery rendered five
  // unique frames — every held frame byte-identical, and at an fps_cap of 30
  // (4 ticks a frame) no frame could ever land mid-travel. Same class as the
  // muzzle flash's `fresh` latch. 58 ms is inside the human range for a
  // deliberate press and puts a frame mid-travel at every cap the game
  // supports; the AR cycles every 10 ticks at 750 rpm, so the ramp fits inside
  // the first shot's interval and the trigger then simply STAYS back for the
  // burst, which is what a held trigger does. The shot still fires on the first
  // tick of the press — gun_shoot is untouched — so what is visible here is
  // over-travel and follow-through, not input latency.
  w->prev_trig_s = w->trig_s;
  {
    // The FULL-AUTO subtlety: a held trigger stays back, and a semi-auto's does
    // too until the shooter lets go — so this follows the button, not the shot.
    // What makes a bolt gun read differently is its 1.2 s cycle, during which
    // the finger is already back off the blade.
    float want = trigger_held && w->reload_t == 0 && w->switch_t == 0 ? 1.0f : 0.0f;
    float rate = want > w->trig_s ? 36.0f : 60.0f;
    w->trig_s += (want - w->trig_s) * fminf(1.0f, rate * TICK_DT);
  }
  // Recovery only once the trigger is off (or the mag ran dry): during a burst
  // the climb is the shooter's to fight, afterwards the view returns.
  if (!trigger_held || w->ammo[w->cur] == 0 || w->switch_t > 0) {
    float a = mv_alpha(wp[WP_RECOVER_MS]);
    w->rec_p -= w->rec_p * a;
    w->rec_y -= w->rec_y * a;
    if (!trigger_held && w->rec_p > -0.05f && w->rec_p < 0.05f) w->idx = 0;
  }
  w->bloom *= expf(-3.0f * TICK_DT / 0.4f);   // bursts cool off in ~400 ms
  // The punch is NOT stepped here any more — it is a spring and it runs in
  // gun_cool, at the top of the tick, so this tick's impulse survives to the
  // render. Stepping it here decayed it 34% before any frame existed.
}

// LIVE GRENADES. One array per arena, stepped by the sim. On a client each one is a
// local copy started by EV_NADE and run on the same physics for the eye; it deals
// nothing, because EV_BLAST and the damage that follows come from the server.
#define MAX_NADES 24
typedef struct { v3 pos, prev_pos, vel, axis; float spin, prev_spin, spin_v; int fuse, src, rest; } nade_t;
static nade_t g_nades_all[SRV_LOBBIES][MAX_NADES];
#define g_nades (g_nades_all[g_lobby])

static void nade_add(v3 at, v3 vel, int fuse, int src) {
  int slot = 0, low = 1 << 30;
  for (int i = 0; i < MAX_NADES; i++) {
    if (g_nades[i].fuse <= 0) { slot = i; low = -1; break; }
    if (g_nades[i].fuse < low) { low = g_nades[i].fuse; slot = i; }
  }
  v3 ax = v3_cross(vel, (v3){{0.3f, 1.0f, 0.2f}});
  float l = v3_len(ax);
  g_nades[slot] = (nade_t){.pos = at, .prev_pos = at, .vel = vel,
                           .axis = l > 1e-3f ? v3_scale(ax, 1.0f / l) : (v3){{1, 0, 0}},
                           .spin_v = 0.32f, .fuse = fuse, .src = src};
}

// A throw leaving a hand, authoritative: spawned here and announced to clients.
static void nade_throw(v3 at, v3 vel, int fuse, int src) {
  if (g_predicting) return;
  nade_add(at, vel, fuse, src);
  ev_push((event_t){.type = EV_NADE, .a = at, .b = vel, .i = fuse, .src = src, .dst = -1});
}

// THE DETONATION: who it reaches, how hard, and which way it throws them.
static void nade_blast(v3 at, int src) {
  v3 o = v3_add(at, (v3){{0, 0.12f, 0}});
  int open = at.y < 1.2f;      // a charge still bouncing is close enough to raise the floor
  ev_push((event_t){.type = EV_BLAST, .a = at, .g = open ? 1.0f : 0.0f, .src = src, .dst = -1});
  bot_noise_emit(src, BOT_NOISE_SHOT, at, 46.0f);
  ENT_FOREACH(e) {
    if (!ent_alive(e)) continue;
    v3 base = ent_pos(e);
    v3 pts[3] = {{{base.x, base.y + 0.30f, base.z}},    // shins
                 {{base.x, base.y + 1.10f, base.z}},    // chest
                 {{base.x, base.y + 1.62f, base.z}}};   // head
    float d = 1e9f;
    int clear = 0;
    v3 hit = pts[1];
    for (int i = 0; i < 3; i++) {
      float dp = v3_len(v3_sub(pts[i], o));
      if (dp > BLAST_R || !los_clear(o, pts[i])) continue;
      clear++;
      if (dp < d) { d = dp; hit = pts[i]; }
    }
    if (!clear) continue;
    float k = 1.0f - f_clamp((d - BLAST_CORE) / (BLAST_R - BLAST_CORE), 0.0f, 1.0f);
    int dmg = (int)(BLAST_DMG * powf(k, 1.4f) * (float)clear / 3.0f + 0.5f);
    // The shove is the air, not the fragments, and it does not care about hp.
    v3 away = v3_sub(pts[1], at);
    float al = v3_len(away);
    away = al > 1e-3f ? v3_scale(away, 1.0f / al) : (v3){{0, 1, 0}};
    v3 push = v3_scale(away, BLAST_SHOVE * k * k);
    push.y = fmaxf(push.y, 2.6f * k * k);
    int h = human_of_ent(e);
    if (h >= 0) g_players[h].vel = v3_add(g_players[h].vel, push);
    else if (g_bots[e - 1].active) g_bots[e - 1].vel = v3_add(g_bots[e - 1].vel, push);
    g_kill_wpn = WPN_GR;
    if (dmg > 0) ent_damage(e, dmg, 2, src, at, hit);
    g_kill_wpn = -1;
  }
}

// One tick of every live grenade. Swept by ray, so at 17 m/s nothing tunnels.
// `auth` is 0 on a client's copy: same flight, no bang of its own.
static void nade_tick(int auth) {
  for (int i = 0; i < MAX_NADES; i++) {
    nade_t *n = &g_nades[i];
    if (n->fuse <= 0) continue;
    n->prev_pos = n->pos;
    n->prev_spin = n->spin;
    if (--n->fuse <= 0) {
      if (auth) nade_blast(n->pos, n->src);
      continue;
    }
    if (n->rest) continue;
    n->vel.y -= NADE_GRAVITY * TICK_DT;
    n->vel = v3_scale(n->vel, 1.0f - NADE_DRAG);
    n->spin += n->spin_v;
    float sp = v3_len(n->vel);
    float step = sp * TICK_DT;
    if (step < 1e-5f) continue;
    v3 d = v3_scale(n->vel, 1.0f / sp), wn = {{0, 1, 0}};
    int ws = -1;
    float hit = trace_world(n->pos, d, &wn, &ws);
    if (hit >= step + NADE_R) {
      n->pos = v3_add(n->pos, v3_scale(n->vel, TICK_DT));
    } else {
      // To the surface, then split the velocity on its normal: the part going
      // into the wall reverses and mostly dies, the part along it scrapes.
      n->pos = v3_add(n->pos, v3_scale(d, fmaxf(hit - NADE_R, 0.0f)));
      float vn = v3_dot(n->vel, wn);
      v3 vt = v3_sub(n->vel, v3_scale(wn, vn));
      if (-vn > 1.1f && auth)
        ev_push((event_t){.type = EV_NADE, .k = 1, .a = n->pos, .f = -vn, .src = n->src, .dst = -1});
      n->vel = v3_add(v3_scale(vt, NADE_FRICTION), v3_scale(wn, -vn * NADE_BOUNCE));
      n->spin_v *= 0.6f;
      if (wn.y > 0.7f && fabsf(vn) < 1.3f) {   // on a floor and not bouncing: rolling
        n->vel.y = 0.0f;
        n->vel = v3_scale(n->vel, NADE_ROLL);
        if (v3_len(n->vel) < 0.12f) { n->rest = 1; n->vel = (v3){{0, 0, 0}}; }
      }
    }
    if (n->pos.y < NADE_R) { n->pos.y = NADE_R; if (n->vel.y < 0) n->vel.y = 0; }
  }
}

// THE WEAPON ANY ENTITY HOLDS, or NULL. The block rule needs the VICTIM's guard
// and only has an entity id.
static weapon_t *ent_weapon(int e) {
  int h = human_of_ent(e);
  if (h >= 0) return &g_players[h].wp;
  return e > 0 && e - 1 < MAX_BOTS ? &g_bots[e - 1].wp : NULL;
}
static int ent_wvar(int ent, int wpn) {
  const weapon_t *w = ent >= 0 && ent < MAX_ENTS ? ent_weapon(ent) : NULL;
  return w && wpn >= 0 && wpn < WPN_COUNT ? wvar(w, wpn) : 0;
}

static float ent_yaw(int e) {
  int h = human_of_ent(e);
  return h >= 0 ? g_players[h].yaw : g_bots[e - 1].yaw;
}

// A BLADE ARRIVING AT A MAN. Everything the defence is lives here: a raised guard
// facing the attacker stops the hit, and one raised just in time parries it,
// which throws the ATTACKER out of his chain. Nothing protects a turned back,
// and a guard does nothing against a bullet because bullets never come here.
static void melee_land(int ent, weapon_t *w, int vic, int part, const melee_def_t *m,
                       v3 o, v3 at) {
  w->melee_done = 1;
  if (g_predicting) return;                 // damage is authoritative: server only
  int dmg = m->dmg * (part == 3 ? WPN_DEF[WPN_KN].head_x : 1);
  // THE TAKEDOWN: a blade from a guard who is not fighting you is the end of
  // him, and a quiet one.
  if (rescue_on() && resc_is_squad(ent) && resc_is_guard(vic)) {
    const bot_t *vb = &g_bots[vic - 1];
    if (!(vb->tgt == ent && vb->belief == BOT_BELIEF_VISIBLE) && vb->alive) {
      dmg = vb->hp > dmg ? vb->hp : dmg;   // exactly lethal: he drops where he stood
      g_resc.takedowns++;
    }
  }
  weapon_t *vw = ent_weapon(vic);
  if (vw && vw->cur == WPN_KN && vw->ads_s > 0.5f && vw->melee_t == 0) {
    float vy = ent_yaw(vic);
    v3 to = v3_sub(o, ent_pos(vic));
    to.y = 0.0f;
    float tl = v3_len(to);
    float facing = tl > 1e-4f ? (to.x * sinf(vy) - to.z * cosf(vy)) / tl : 1.0f;
    if (facing >= MELEE_GUARD_ARC) {
      int parry = vw->guard_t <= MELEE_PARRY_TICKS;
      vw->block_k = 1.0f;
      if (parry) {
        w->stagger_t = MELEE_STAGGER_TICKS;
        w->melee_n = 0;                     // the chain is broken
      } else {
        w->block_k = 0.6f;
      }
      ev_push((event_t){.type = EV_BLOCK, .a = at, .b = o, .k = parry,
                        .src = ent, .dst = vic, .wpn = WPN_KN});
      if (!parry) ent_damage(vic, MELEE_CHIP, 2, ent, o, at);
      return;
    }
  }
  ent_damage(vic, dmg, part, ent, o, at);
}

// One tick of the blade crossing its arc. A fan of rays, so a target a little
// above or below the blade's plane is still hit and a swing is not a pixel hunt.
// Rays stop at solids: a blade does not reach through a crate.
static void melee_sweep(int ent, weapon_t *w, v3 o, float yaw, float pitch) {
  const melee_def_t *m = &MELEE[w->melee_v];
  float u = (float)(w->melee_t - m->strike0) /
            fmaxf((float)(m->strike1 - m->strike0), 1.0f);
  float ya = yaw + m->a0 + (m->a1 - m->a0) * u;
  float pa = pitch + m->p0 + (m->p1 - m->p0) * u;
  static const float FAN[5][2] = {{0, 0}, {0, 0.16f}, {0, -0.16f},
                                  {0.09f, 0}, {-0.09f, 0}};
  int best = -1, bpart = 2;
  float bd = m->reach, wall_d = m->reach;
  v3 bdir = {{0, 0, -1}}, wall_n = {{0, 1, 0}}, wall_dir = bdir;
  int wall_s = -1;
  for (int i = 0; i < 5; i++) {
    float y = ya + FAN[i][0], p = pa + FAN[i][1];
    v3 d = {{cosf(p) * sinf(y), sinf(p), -cosf(p) * cosf(y)}};
    v3 wn = {{0, 1, 0}};
    int ws = -1;
    float wall = trace_world(o, d, &wn, &ws);
    float lim = fminf(wall, bd);
    int part = 2;
    int hit = ent_trace(ent, o, d, &lim, &part);
    if (hit >= 0) { best = hit; bd = lim; bpart = part; bdir = d; }
    if (i == 0 && wall < wall_d) { wall_d = wall; wall_n = wn; wall_s = ws; wall_dir = d; }
  }
  if (best >= 0) {
    melee_land(ent, w, best, bpart, m, o, v3_add(o, v3_scale(bdir, bd)));
    return;
  }
  if (wall_d < m->reach) {                  // steel on a wall: it stops the swing
    w->melee_done = 1;
    v3 at = v3_add(o, v3_scale(wall_dir, wall_d));
    ev_push((event_t){.type = EV_IMPACT, .a = at, .b = wall_n, .i = wall_s, .k = 1,
                      .f = wall_d, .src = ent});
    ev_push((event_t){.type = EV_MELEE, .a = at, .k = 1, .src = ent, .wpn = WPN_KN});
  }
}

// The knife's whole tick, shared by player and bots as gun_shoot is. It runs in
// place of the trigger block of a gun.
static void melee_tick(int ent, weapon_t *w, v3 o, float yaw, float pitch,
                       anim_t *anim, int trigger) {
  if (w->melee_gap > 0 && --w->melee_gap == 0) w->melee_n = 0;
  if (w->melee_t > 0) {
    const melee_def_t *m = &MELEE[w->melee_v];
    w->melee_t++;
    if (!w->melee_done && w->melee_t >= m->strike0 && w->melee_t <= m->strike1)
      melee_sweep(ent, w, o, yaw, pitch);
    if (w->melee_done && w->melee_t == m->strike1 + 1)
      w->punch_v += GUN_PUNCH_SHOT * g_sim_wp[WPN_KN][WP_PUNCH] * GUN_PUNCH_V;
    if (w->melee_t > m->total) {
      w->melee_t = 0;
      w->melee_gap = MELEE_CHAIN_TICKS;
    }
  }
  if (trigger && w->melee_t == 0 && w->switch_t == 0 && w->stagger_t == 0) {
    w->melee_v = w->melee_n % 3;
    w->melee_n = (w->melee_n + 1) % 3;
    w->melee_t = 1;
    w->melee_done = 0;
    w->melee_gap = 0;
    g_match.shots[ent]++;
    int sh = human_of_ent(ent);
    if (sh >= 0) g_players[sh].shots++;
    bot_noise_emit(ent, BOT_NOISE_SHOT, o, rescue_on() && resc_is_squad(ent) ? 2.5f : 9.0f);
    if (anim) anim->fire_k = 1.0f;
    ev_push((event_t){.type = EV_MELEE, .a = o, .src = ent, .wpn = WPN_KN});
  }
}

