// code/sim/weapon.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Weapon — full-auto hitscan AR. Recoil is a pitch/yaw offset that climbs
// while the trigger is held (deterministic pattern + small jitter) and is
// applied to BOTH the view and the shot direction, so pulling down really
// compensates; it recovers exponentially once the trigger is released.
// Bloom is the random cone on top: grows per shot, decays between bursts.
// ---------------------------------------------------------------------------

// THE keystone call (P1-2): real entity id, per-human RNG stream, per-human
// anim — while any of the three is a literal/singleton, every P0-6 reader is
// CONSISTENTLY wrong and one human looks correct.
static void weapon_fire(player_t *p, int ent) {
  int h = human_of_ent(ent);
  v3 o = player_eye_pos(p);
  gun_shoot(ent, &p->wp, o, p->yaw, p->pitch, p->vel, p->grounded,
            &g_rng_gun[h], &g_player_anim[h]);
}

// WHERE A THROW STARTS AND HOW FAST, one copy: the throw and the arc preview both
// read this, so the line on screen is the line the grenade takes.
static void nade_launch(const player_t *p, int lob, v3 *at, v3 *vel) {
  float pa = p->pitch + (lob ? -0.10f : 0.13f), ya = p->yaw;
  v3 dir = {{cosf(pa) * sinf(ya), sinf(pa), -cosf(pa) * cosf(ya)}};
  v3 rt = {{cosf(ya), 0, sinf(ya)}};
  v3 eye = player_eye_pos(p);
  *at = v3_add(eye, v3_add(v3_scale(rt, 0.14f), v3_add(v3_scale(dir, 0.42f),
                                                       (v3){{0, -0.08f, 0}})));
  // Up against a wall the hand is past it: let go at the eye instead.
  v3 wn;
  int ws;
  if (trace_world(eye, dir, &wn, &ws) < 0.6f) *at = eye;
  *vel = v3_add(v3_scale(p->vel, 0.6f), v3_scale(dir, lob ? NADE_LOB : NADE_SPEED));
}

// The grenade in the hand. Press pulls the pin and starts the fuse, release winds
// through the throw, and the hand opens at NADE_RELEASE_T. Prediction runs all of
// this on a client except the spawn itself (nade_throw returns there).
static void grenade_tick(player_t *p, int ent, weapon_t *w, const int held[IN_COUNT]) {
  if (w->switch_t > 0) return;
  if (w->gr_t > 0) {
    if (++w->gr_t == NADE_RELEASE_T) {
      v3 at, vel;
      nade_launch(p, w->gr_lob, &at, &vel);
      nade_throw(at, vel, w->gr_fuse, ent);
      w->ammo[WPN_GR]--;
    }
    if (w->gr_t > NADE_THROW_T) {
      w->gr_t = 0;
      mode_nade_refill(ent, w);                  // the bottomless GUN GAME rung
      if (w->ammo[WPN_GR] <= 0) {                // out: back to the gun you had
        w->cur = w->gr_prev == WPN_GR ? WPN_AR : w->gr_prev;
        if (!mode_allowed(ent, w->cur)) w->cur = mode_primary(ent);
        w->switch_t = GUN_SWITCH_TICKS;
      }
    }
    return;
  }
  if (w->ammo[WPN_GR] <= 0 || !p->alive || g_match.over) { w->gr_cook = 0; return; }
  if (w->gr_cook == 0) {
    if (held[IN_FIRE] && !p->prev_held[IN_FIRE]) {
      w->gr_cook = 1;
      ev_push((event_t){.type = EV_RELOAD, .src = ent, .a = p->pos, .wpn = WPN_GR});
    }
    return;
  }
  w->gr_cook++;
  // The pin clears and the spoon goes: the fuse is committed and everyone close
  // hears it. It flies off the hand on its own little arc.
  if (w->gr_cook == NADE_PIN_T) {
    v3 rt = {{cosf(p->yaw), 0, sinf(p->yaw)}};
    v3 sv = v3_add(p->vel, v3_add(v3_scale(rt, 2.2f), (v3){{0, 3.4f, 0}}));
    v3 sp = v3_add(player_eye_pos(p), v3_add(v3_scale(rt, 0.26f), (v3){{0, -0.10f, 0}}));
    ev_push((event_t){.type = EV_NADE, .k = 2, .a = sp, .b = sv, .src = ent, .dst = -1});
  }
  if (w->gr_cook >= NADE_FUSE) {                 // held too long
    w->gr_cook = 0;
    w->ammo[WPN_GR]--;
    if (!g_predicting) nade_blast(player_eye_pos(p), ent);
    return;
  }
  // The pin has to finish coming out before the hand can let go, so a tap still
  // makes a full, honest throw.
  if (!held[IN_FIRE] && w->gr_cook >= NADE_PIN_T) {
    w->gr_fuse = NADE_FUSE - w->gr_cook;
    w->gr_lob = held[IN_ADS];
    w->gr_cook = 0;
    w->gr_t = 1;
  }
}

static int rescue_reload_block(const player_t *p);
static void weapon_tick(player_t *p, int ent, const int held[IN_COUNT]) {
  weapon_t *w = &p->wp;
  const wpn_def_t *def = wdef(w, w->cur);
  w->prev_rec_p = w->rec_p;
  w->prev_rec_y = w->rec_y;
  w->prev_punch = w->punch;
  w->prev_punch_roll = w->punch_roll;
  w->prev_ads_s = w->ads_s;
  w->prev_switch = (float)w->switch_t / GUN_SWITCH_TICKS;

  gun_cool(w);
  // A loadout picked after the spawn is taken at once while the weapons are
  // still untouched — a joiner's first input arrives after its first spawn,
  // and a menu change should not have to wait for a death.
  if (w->fresh) {
    int lo = human_loadout_src(human_of_ent(ent));
    if (lo >= 0 && lo != loadout_pack(w->var[WPN_AR], w->var[WPN_PT])) {
      loadout_take(w, lo);
      def = wdef(w, w->cur);
    }
  }

  // Swap: instant identity change, then the raise animation gates everything.
  // A running reload is abandoned (the mag stays as it was). Three keys reach
  // this: the toggle, and the two direct slots (1 = AR, 2 = SNIPER). A slot
  // press for the weapon already in the hands is a no-op — re-raising the gun
  // you are holding because you double-tapped its own key is the one thing a
  // direct slot key must never do.
  int want = -1;
  if (held[IN_SWAP] && !p->prev_held[IN_SWAP]) want = (w->cur + 1) % WPN_COUNT;
  if (held[IN_SLOT1] && !p->prev_held[IN_SLOT1]) want = WPN_AR;
  if (held[IN_SLOT2] && !p->prev_held[IN_SLOT2]) want = WPN_SR;
  if (held[IN_SLOT3] && !p->prev_held[IN_SLOT3]) want = WPN_PT;
  if (held[IN_SLOT4] && !p->prev_held[IN_SLOT4]) want = WPN_KN;
  if (held[IN_SLOT5] && !p->prev_held[IN_SLOT5]) want = WPN_GR;
  mode_nade_refill(ent, w);
  // THE MODE'S WEAPON GATE. The cycle walks on to the next weapon the mode
  // allows (with grenades in stock); a slot key for a forbidden weapon does
  // nothing; and a weapon in the hands that the mode no longer allows — a GUN
  // GAME promotion, a respawn preference — is swapped out by the same path.
  int cycling = held[IN_SWAP] && !p->prev_held[IN_SWAP];
  if (cycling) {
    want = -1;
    for (int k = 1; k <= WPN_COUNT; k++) {
      int c = (w->cur + k) % WPN_COUNT;
      if (!mode_allowed(ent, c) || (c == WPN_GR && w->ammo[WPN_GR] <= 0)) continue;
      want = c;
      break;
    }
  } else if (want >= 0 && !mode_allowed(ent, want)) {
    want = -1;
  }
  if (want < 0 && !mode_allowed(ent, w->cur)) want = mode_primary(ent);
  // No grenades left: the cycle steps past the slot and the key does nothing.
  if (want == WPN_GR && w->ammo[WPN_GR] <= 0)
    want = cycling && mode_allowed(ent, WPN_AR) ? WPN_AR : -1;
  // A live grenade stays in the hand: the pin does not go back in.
  if (w->gr_cook > 0 || w->gr_t > 0) want = -1;
  // Dying with the pin out drops it where you fall, on what is left of the fuse.
  if (!p->alive && w->gr_cook > 0) {
    nade_throw(v3_add(p->pos, (v3){{0, 0.25f, 0}}), p->vel,
               NADE_FUSE - w->gr_cook, ent);
    w->gr_cook = 0;
  }
  if (want >= 0 && want != w->cur && w->switch_t == 0) {
    if (want == WPN_GR) {
      w->gr_prev = w->cur;
      ev_push((event_t){.type = EV_NADE, .k = 3, .a = p->pos, .src = ent, .dst = -1});
    }
    // The abandoned reload's SOUND stops with it — otherwise the buffer plays
    // to its end and announces A LIVE WEAPON (the bolt cue) for a magazine
    // that never went in.
    if (w->reload_t > 0)
      // .src: a designated-initializer push with the field left out is
      // SILENTLY entity 0 (zero-init) — every identity field is explicit now.
      ev_push((event_t){.type = EV_RELOAD_CANCEL, .src = ent, .a = p->pos,
                        .wpn = w->cur});
    w->cur = want;
    def = wdef(w, w->cur);
    w->switch_t = GUN_SWITCH_TICKS;
    w->reload_t = 0;
    w->melee_t = 0;
    w->dry_done = 0;         // a fresh weapon gets its own one click
    // cool[] is deliberately NOT touched: the outgoing weapon's cycle keeps
    // running (a quick-switch must not reset the SR's bolt) and the incoming
    // weapon's own cooldown still applies from before it was holstered.
    w->idx = 0;
  }
  int busy = w->reload_t > 0 || w->switch_t > 0;

  // With the knife out the aim button is the guard. It cannot be held through a
  // swing or a stagger, and guard_t is what makes a timed raise a parry.
  int guarding = held[IN_ADS] && !busy && w->cur != WPN_GR &&
                 !(w->cur == WPN_KN && (w->melee_t > 0 || w->stagger_t > 0));
  gun_ads(w, guarding);
  w->guard_t = guarding ? w->guard_t + 1 : 0;

  if (w->cur == WPN_KN) {
    melee_tick(ent, w, player_eye_pos(p), p->yaw, p->pitch,
               &g_player_anim[human_of_ent(ent)],
               held[IN_FIRE] && p->alive && !g_match.over && !guarding);
    gun_recover(w, held[IN_FIRE]);
    return;
  }
  if (w->cur == WPN_GR) {
    grenade_tick(p, ent, w, held);
    gun_recover(w, 0);
    return;
  }

  gun_reload_tick(w);
  // AUTO RELOAD: an empty magazine starts its own reload as soon as the last shot
  // has cycled. Same path as the key, so it predicts, sounds and animates alike,
  // and a swap onto an empty weapon reloads once the raise finishes.
  int auto_rl = w->ammo[w->cur] == 0 && w->cool[w->cur] == 0 && p->alive;  // the grenade returned above
  if (((held[IN_RELOAD] && !p->prev_held[IN_RELOAD] && !rescue_reload_block(p)) || auto_rl) && !busy &&
      w->ammo[w->cur] < def->mag) {
    w->reload_tac = w->ammo[w->cur] > 0;
    w->reload_t = w->reload_tac ? def->reload_tac_ticks : def->reload_ticks;
    w->fresh = 0;
    w->dry_done = 0;         // ...and so does a fresh magazine
    // .src AND .a, mirroring the bot push: with .src set and .a zero the
    // audio router takes the audio_place branch and pans a remote reload to
    // the ARENA ORIGIN (screen centre) — no proof watches reload panning.
    ev_push((event_t){.type = EV_RELOAD, .src = ent, .a = p->pos,
                      .wpn = w->cur, .k = w->reload_tac});
  }

  // Full-auto weapons fire while held; semi/bolt only on the press edge.
  // No shots from the dead or after the match ended.
  int trigger = held[IN_FIRE] && p->alive && !g_match.over &&
                (def->full_auto || !p->prev_held[IN_FIRE]);
  // THE DRY CLICK IS A LATCH, NOT AN EDGE. `trigger` is already false on every
  // held tick of a semi/bolt weapon, so the latch alone gives exactly one click
  // per press there and exactly one for a full-auto weapon that runs dry under
  // a held trigger — the case the old press-edge test made unreachable, and the
  // commonest way anybody empties a magazine.
  if (!held[IN_FIRE]) w->dry_done = 0;
  if (trigger && w->cool[w->cur] == 0 && w->reload_t == 0 && w->switch_t == 0 &&
      w->stagger_t == 0) {
    if (w->ammo[w->cur] > 0) weapon_fire(p, ent);
    else if (!w->dry_done) {
      ev_push((event_t){.type = EV_DRYFIRE, .src = ent, .a = p->pos});
      w->dry_done = 1;
    }
  }

  gun_recover(w, held[IN_FIRE]);
}

// The viewmodel's life comes from six springs: look sway (rotation lags the
// mouse), move inertia (position lags acceleration — stops and direction
// changes drag the gun), walk bob (figure-8 from a distance phase), the fire
// kick (impulse into a stiff return spring), slide/strafe roll, and the
// landing dip (shared with the camera). SWAY/BOB scales the ambient ones.
static float sp_h_(const player_t *p) { return sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z); }
// One step of a damped spring toward `target`: natural frequency `hz`,
// damping ratio `zeta` (1 = critical, < 1 overshoots). Semi-implicit Euler at
// 120 Hz is stable far past any frequency used here.
static void vm_spring(float *x, float *xv, float target, float hz, float zeta) {
  float w = F_TAU * hz;
  *xv += (w * w * (target - *x) - 2.0f * zeta * w * *xv) * TICK_DT;
  *x += *xv * TICK_DT;
}
static void vm_tick(player_t *p) {
  vm_t *v = &p->vm;
  float sc = g_cfg.wp[p->wp.cur][WP_SWAY];
  v->prev_sway_p = v->sway_p; v->prev_sway_y = v->sway_y;
  v->prev_lag_x = v->lag_x; v->prev_lag_y = v->lag_y; v->prev_lag_z = v->lag_z;
  v->prev_bob_x = v->bob_x; v->prev_bob_y = v->bob_y;
  v->prev_kick = v->kick;
  v->prev_roll = v->roll;
  v->prev_lean_lag = v->lean_lag;

  // Look sway: the gun trails the view's angular velocity.
  float dyaw = p->yaw - v->last_yaw, dpit = p->pitch - v->last_pitch;
  v->last_yaw = p->yaw; v->last_pitch = p->pitch;
  float damp = 1.0f - 0.88f * p->wp.ads_s;  // ADS pins the sight
  v->prev_twist = v->twist;
  v->prev_wall = v->wall;
  v->prev_cam_roll = v->cam_roll;
  // The idle FIGURE-8: a shooter's chest rises and falls ~15 times a minute,
  // and a held weapon traces a lissajous over it (side at f, lift at 2f).
  // Present at rest and in ADS, where it is what makes a scope feel held.
  v->breath += F_TAU * 0.26f * TICK_DT;
  if (v->breath > F_TAU * 8.0f) v->breath -= F_TAU * 8.0f;
  float br = sc * (1.0f - 0.55f * p->wp.ads_s);
  float br_p = sinf(v->breath * 2.0f) * 0.0024f * br;
  float br_y = sinf(v->breath) * 0.0032f * br;
  vm_spring(&v->sway_y, &v->sway_yv,
            f_clamp(-dyaw / TICK_DT * 0.014f, -0.05f, 0.05f) * sc * damp + br_y, 3.1f, 0.52f);
  vm_spring(&v->sway_p, &v->sway_pv,
            f_clamp(-dpit / TICK_DT * 0.011f, -0.04f, 0.04f) * sc * damp + br_p, 3.4f, 0.55f);

  // Move inertia: world acceleration projected into gun space.
  v3 dv = v3_add(p->vel, v3_scale(v->last_vel, -1.0f));
  v->last_vel = p->vel;
  float fx = sinf(p->yaw), fz = -cosf(p->yaw);
  float ax = (dv.x * fz * -1.0f + dv.z * fx) / TICK_DT;        // right accel
  float az = (dv.x * fx + dv.z * fz) / TICK_DT;                // forward accel
  vm_spring(&v->lag_x, &v->lag_xv, f_clamp(-ax * 0.0009f, -0.03f, 0.03f) * sc * damp, 2.3f, 0.45f);
  vm_spring(&v->lag_z, &v->lag_zv, f_clamp(-az * 0.0007f, -0.035f, 0.035f) * sc * damp, 2.6f, 0.50f);
  vm_spring(&v->lag_y, &v->lag_yv, f_clamp(-p->vel.y * 0.006f, -0.035f, 0.035f) * sc * damp, 2.4f, 0.42f);
  // LANDING SHOCK: the arms take the impact, so the weapon compresses down
  // into it and springs back, by how hard the feet hit.
  if (p->grounded && !v->was_grounded) {
    float impact = f_clamp(-(v->last_vel.y - dv.y), 0.0f, 14.0f);  // pre-landing fall speed
    v->lag_yv -= impact * 0.020f * sc * (1.0f - 0.6f * p->wp.ads_s);
    v->roll_v += impact * 0.010f * sc;
  }
  v->was_grounded = p->grounded;
  // WALL AVOIDANCE: a rifle is ~0.8 m long, and pushed into a wall it does not
  // clip — the shooter draws it back to the chest and brings the muzzle up.
  // One trace along the view per tick; the spring makes the move a motion.
  {
    v3 eye = player_eye_pos(p);
    v3 fwd = {{cosf(p->pitch) * sinf(p->yaw), sinf(p->pitch), -cosf(p->pitch) * cosf(p->yaw)}};
    v3 wn;
    int ws;
    float wd = trace_world(eye, fwd, &wn, &ws);
    float reach = p->wp.cur == WPN_SR ? 1.05f : p->wp.cur == WPN_PT ? 0.55f : 0.85f;
    float tw = p->wp.cur == WPN_KN || p->wp.cur == WPN_GR ? 0.0f
             : f_clamp((reach - wd) / (reach * 0.55f), 0.0f, 1.0f);
    vm_spring(&v->wall, &v->wall_v, tw, 2.2f, 0.9f);
    v->wall = f_clamp(v->wall, 0.0f, 1.0f);
  }
  // Torque twist from the last shot (impulse in weapon_fire), ringing out.
  vm_spring(&v->twist, &v->twist_v, 0.0f, 4.2f, 0.55f);  // settles, no ringing

  // Walk bob: the phase advances at a STRIDE CADENCE (~1.6..2.3 Hz), not
  // with distance — at 12 m/s a distance phase would wag the gun at ~10 Hz,
  // pure jitter. Speed feeds the amplitude instead: faster = deeper sway,
  // never faster wagging. Airborne/sliding the whole thing eases out.
  float hs = sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z);
  float run_t = f_clamp(hs / g_cfg.mv[MV_RUN], 0, 1);
  float amp_t = p->grounded && !p->sliding && hs > 0.4f ? run_t : 0;
  v->bob_amp += (amp_t - v->bob_amp) * f_clamp(6.0f * TICK_DT, 0, 1);
  if (v->bob_amp > 0.01f)
    v->bob_phase += F_TAU * (1.6f + 0.7f * run_t) * TICK_DT;
  float bs = v->bob_amp * sc * damp;
  v->bob_x = sinf(v->bob_phase) * 0.014f * bs + sinf(v->breath) * 0.0016f * br;
  v->bob_y = -(0.5f - 0.5f * cosf(v->bob_phase * 2.0f)) * 0.011f * bs
           + sinf(v->breath * 2.0f) * 0.0010f * br;

  // Fire kick: stiff underdamped return.
  v->kick_v += (-1400.0f * v->kick - 55.0f * v->kick_v) * TICK_DT;
  v->kick += v->kick_v * TICK_DT;

  // Lean lag. The camera's roll and offset are rigid — the gun is not: it hangs
  // off two arms, so it arrives at the new lean a beat late and settles. The
  // whole tell is the DIFFERENCE between this and the player's lean, which is
  // zero except while the lean is actually moving.
  vm_spring(&v->lean_lag, &v->lean_v, p->lean, 1.9f, 0.62f);

  // Roll: slide tilt plus a strafe hint.
  float roll_t = (p->sliding ? 0.09f : 0.0f);
  float side = p->vel.x * fz * -1.0f + p->vel.z * fx;
  roll_t += f_clamp(-side / g_cfg.mv[MV_RUN], -1, 1) * 0.018f * damp;
  vm_spring(&v->roll, &v->roll_v, roll_t * sc, 1.8f, 0.60f);
  // THE CAMERA BANKS INTO A SLIDE: a few degrees, toward the side the body is
  // drifting (a straight slide banks to the weapon side). A roll about the
  // view axis never moves the crosshair, so it is free against point of aim.
  float bank = 0.0f;
  if (p->sliding) {
    float lat = f_clamp(side / fmaxf(sp_h_(p), 1.0f), -1.0f, 1.0f);
    bank = (fabsf(lat) > 0.15f ? -lat : -0.6f) * 0.052f;
  }
  vm_spring(&v->cam_roll, &v->cam_roll_v, bank, 1.6f, 0.75f);
}

// One tick of the lean. The command eases toward the held keys (both keys, or
// neither, is upright), fades out with speed and is cancelled outright by a
// slide — a slide already rolls the body, and a 31 degree lean stacked on top of
// it is a barrel roll. What the player GETS is then whatever the geometry
// allows: the eye's sideways path is cast against the solids and stops
// LEAN_MARGIN short of the first one.
//
// The clearance is a RATIO, not a hard stop on the position. Clamping only the
// offset would leave the view rolled a full 31 degrees with the head still
// behind the wall — which is precisely what a camera clipping into a corner
// looks like — so the roll, the drop and the offset all shrink together.
static void player_lean_tick(player_t *p, const int held[IN_COUNT]) {
  p->prev_lean = p->lean;
  float want = (held[IN_LEAN_R] ? 1.0f : 0.0f) - (held[IN_LEAN_L] ? 1.0f : 0.0f);
  float sp = sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z);
  float run = g_cfg.mv[MV_RUN] > 0.1f ? g_cfg.mv[MV_RUN] : 0.1f;
  want *= 1.0f - f_clamp((sp / run - LEAN_SPD_LO) / (LEAN_SPD_HI - LEAN_SPD_LO),
                         0.0f, 1.0f);
  if (p->sliding || !p->alive) want = 0.0f;
  float rate = fabsf(want) > fabsf(p->lean_in) ? LEAN_IN_RATE : LEAN_OUT_RATE;
  p->lean_in += (want - p->lean_in) * ease_k(rate);

  // TWO probes, and the pair is the whole design. `now` is the hard constraint:
  // the eye may never be nearer than LEAN_MARGIN to a solid, so it is applied as
  // a floor after the easing, not through it. `ahead` is the warning: the same
  // ray from where the eye will be in LEAN_LOOK seconds, which is what lets the
  // ease actually finish before the wall arrives.
  //
  // Neither alone works. Ease on `now` alone and walking into a wall outruns the
  // filter — measured, a crouch-walk into the centre monolith put the eye 0.116 m
  // off the face, INSIDE the 0.12 m near plane, and the wall went see-through for
  // two ticks. Clamp on `now` alone and a corner entering the ray snaps the head
  // 0.12 m sideways in ONE tick (measured: lean 1.000 -> 0.573 at t=220). The
  // look-ahead turns that snap into a 0.15 s ramp and the floor is then almost
  // never the thing that acts.
  float clr_now = 1.0f, clr_ahead = 1.0f;
  if (fabsf(p->lean_in) > 1e-4f) {
    float dist = fabsf(lean_eye_off(p->lean_in));
    float s = p->lean_in > 0 ? 1.0f : -1.0f;
    v3 o = player_eye_base(p);
    v3 d = {{cosf(p->yaw) * s, 0, sinf(p->yaw) * s}};
    v3 ah = {{o.x + p->vel.x * LEAN_LOOK, o.y, o.z + p->vel.z * LEAN_LOOK}};
    float ax = ah.x - o.x, az = ah.z - o.z, al = sqrtf(ax * ax + az * az);
    if (al > LEAN_LOOK_MAX) {          // sprinting must not anticipate a wall
      ah.x = o.x + ax * LEAN_LOOK_MAX / al;   // two rooms early
      ah.z = o.z + az * LEAN_LOOK_MAX / al;
    }
    // BOTH probes cast at TWO heights: the base eye AND the eye at the full
    // lean's own drop (the arc lowers it up to 0.091 m). Cast at base height
    // alone, a solid whose top lies between the two — LOW cover rolls heights
    // 0.95-1.10 m, a crouch-lean eye sits just above — was invisible to both
    // probes, full lean was granted, and the eye ended 0.081 m off the face:
    // inside the 0.12 m near plane, the exact see-through-wall defect the
    // two-probe design exists to prevent.
    float oy2 = o.y - lean_eye_drop(fabsf(p->lean_in));
    float bn = dist, ba = dist;
    BP_BOX(i, fminf(o.x, ah.x) - dist - 1.0f, fminf(o.z, ah.z) - dist - 1.0f,
           fmaxf(o.x, ah.x) + dist + 1.0f, fmaxf(o.z, ah.z) + dist + 1.0f) {
      v3 n;
      for (int hgt = 0; hgt < 2; hgt++) {
        v3 oh = o, ahh = ah;
        if (hgt) { oh.y = oy2; ahh.y = oy2; }
        float t = ray_aabb(oh, d, g_solids[i].min, g_solids[i].max, &n);
        if (t > 0 && t - LEAN_MARGIN < bn) bn = t - LEAN_MARGIN;
        t = ray_aabb(ahh, d, g_solids[i].min, g_solids[i].max, &n);
        if (t > 0 && t - LEAN_MARGIN < ba) ba = t - LEAN_MARGIN;
      }
    }
    clr_now = (bn > 0 ? bn : 0.0f) / dist;
    clr_ahead = (ba > 0 ? ba : 0.0f) / dist;
  }
  float want_clr = fminf(clr_now, clr_ahead);
  p->lean_clr += (want_clr - p->lean_clr) * ease_k(LEAN_CLR_RATE);
  if (p->lean_clr > clr_now) p->lean_clr = clr_now;
  p->lean = p->lean_in * f_clamp(p->lean_clr, 0.0f, 1.0f);
}

// The MOVE half of a human's tick: movement, lean, weapon, and — local
// only — the viewmodel. Runs once per human BEFORE match_tick, so bots and
// every other human see this tick's positions. wish_px/wish_py is the
// analog move intent AFTER deadzone, handed in by the caller: the sim half
// itself never reads g_pad (a pad is client input).
// THE LEDGE. A jump pressed into cover becomes one of two moves:
//   VAULT  — waist-high cover (0.50..1.15 m) taken at speed: a LOW, fast arc
//            that only just clears the top and keeps the run's momentum, so
//            sprinting over a crate costs nothing like a full jump's hang time.
//   MANTLE — a chest-high ledge (1.15..1.75 m) the plain jump cannot reach:
//            enough lift to put the feet on top, with a little drive forward.
// Probed straight ahead of the body along the move (or the view when
// standing), and only where the top has standing room above it.
static float ledge_top(v3 at, float lo, float hi) {
  float top = -1.0f;
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *sd = &g_solids[i];
    if (at.x < sd->min.x || at.x > sd->max.x || at.z < sd->min.z || at.z > sd->max.z) continue;
    if (sd->max.y > lo && sd->max.y <= hi && sd->min.y < sd->max.y - 0.2f && sd->max.y > top)
      top = sd->max.y;
  }
  return top;
}
static int ledge_room(v3 at, float y0, float y1) {
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *sd = &g_solids[i];
    if (at.x < sd->min.x - 0.2f || at.x > sd->max.x + 0.2f ||
        at.z < sd->min.z - 0.2f || at.z > sd->max.z + 0.2f) continue;
    if (sd->min.y < y1 && sd->max.y > y0) return 0;
  }
  return 1;
}
static void mv_ledge(player_t *p, float wx, float wz, float wlen, float sp2d) {
  float dx, dz;
  if (wlen > 0.2f) { dx = wx / wlen; dz = wz / wlen; }
  else { dx = sinf(p->yaw); dz = -cosf(p->yaw); }
  v3 at = {{p->pos.x + dx * (PLAYER_RADIUS + 0.38f), p->pos.y, p->pos.z + dz * (PLAYER_RADIUS + 0.38f)}};
  float top = ledge_top(at, p->pos.y + 0.50f, p->pos.y + 1.75f);
  if (top < 0.0f) return;
  float h = top - p->pos.y;
  if (!ledge_room(at, top + 0.02f, top + HEIGHT_CROUCH)) return;
  // ...and the body has to be able to rise there: nothing over the head now.
  v3 here = {{p->pos.x, p->pos.y, p->pos.z}};
  if (!ledge_room(here, p->pos.y + HEIGHT_STAND, top + HEIGHT_STAND)) return;
  float toward = p->vel.x * dx + p->vel.z * dz;
  if (h <= 1.15f && toward > g_sim_mv[MV_RUN] * 0.55f) {
    p->vel.y = sqrtf(2.0f * GRAVITY * (h + 0.10f)) + 0.5f * GRAVITY * TICK_DT;
    float go = fmaxf(sp2d, g_sim_mv[MV_RUN] * 0.95f);
    p->vel.x = dx * go; p->vel.z = dz * go;
  } else if (h > 1.15f) {
    p->vel.y = sqrtf(2.0f * GRAVITY * (h + 0.16f)) + 0.5f * GRAVITY * TICK_DT;
    float go = fmaxf(toward, 2.4f);
    p->vel.x = dx * go; p->vel.z = dz * go;
  }
}

// The ladder a body at `pos` is on, or -1: its centre within reach of the
// rails (in front of the face, inside the rail span) and its feet below the
// top. Stateless, so prediction and rollback need nothing new on the wire.
#define LADDER_REACH  (PLAYER_RADIUS + LADDER_OFF + 0.22f)
#define LADDER_CLIMB  3.2f    // m/s up or down
static int ladder_touch(v3 pos) {
  for (int i = 0; i < g_nladders; i++) {
    const ladder_t *L = &g_ladders[i];
    float dx = pos.x - L->x, dz = pos.z - L->z;
    float d = dx * L->nx + dz * L->nz, a = dx * -L->nz + dz * L->nx;
    if (d > 0.0f && d < LADDER_REACH && fabsf(a) < LADDER_HALF + 0.15f &&
        pos.y < L->top + 0.05f)
      return i;
  }
  return -1;
}

static void player_move_tick(player_t *p, int ent, const int held[IN_COUNT],
                             float wish_px, float wish_py) {
  const float *mv = g_sim_mv;
  if (p->flash > 0) p->flash--;      // the same countdown bot_tick runs
  p->prev_pos = p->pos;
  p->prev_eye = p->eye;
  p->prev_fov_kick = p->fov_kick;
  p->prev_land_dip = p->land_dip;
  int was_grounded = p->grounded;

  // Match over: a fire press queues the rematch (uses the raw input, the
  // dead-state filter below would swallow it). LOCAL: on a server or for a
  // remote human this must never fire — the match end is owned server-side.
  // AND NOT ON A CLIENT EITHER (!g_online). ent == g_local_ent is TRUE for a
  // client's own player (this runs inside net_cl_tick's prediction and
  // net_cl_replay's rollback, both passing g_cl.my_ent), and g_match.over is
  // replicated over NP_SCORES — so a trigger pull on the final scoreboard
  // ARMED restart_req on the client. BE PRECISE ABOUT WHAT THAT DID, because
  // the obvious reading is wrong twice over. It did not restart the client's
  // arena on the spot: only match_tick services restart_req, and match_tick
  // never runs online (client_online_tick REPLACES player_tick). It did not
  // latch into the following local match either: g_online goes 0 in exactly
  // ONE place, leave_match, whose LAST statement is match_restart() — and
  // that memsets g_match, destroying restart_req before any match_tick can
  // ever read it. Measured: armed to 1 immediately before leave_match, read
  // back 0 after, and an instrumented service print never fired. The pre-fix
  // press was INERT.
  //
  // The gate stays regardless, and not out of caution. The HUD no longer
  // tells anyone to press fire online (see render_frame), so the write has
  // lost the instruction that produced it, and nothing should arm a flag the
  // online path cannot service. What this comment is really guarding: do NOT
  // "repair" the dead path by deleting match_restart() from leave_match so
  // the latch becomes real — that call is what clears the ghost roster, and
  // removing it resurrects the bug it was added to fix.
  // Online the rematch is the server's MATCH_OVER_TICKS countdown, which is
  // why the HUD prints a timer there instead of "PRESS FIRE FOR REMATCH".
  if (!g_online && ent == g_local_ent && g_match.over && held[IN_FIRE] &&
      !p->prev_held[IN_FIRE])
    g_match.restart_req = 1;

  // Dead: gameplay input is ignored (the scoreboard still works), a
  // countdown respawns us somewhere away from the fight.
  // prev_held must keep tracking the RAW input through the substitution:
  // recorded from the zeroed array, every button physically held through death
  // read as a brand-new press afterwards — a held trigger fired the SR on the
  // exact respawn tick, and a trigger held since before the match ended
  // queued the rematch with no deliberate press. This is the keyboard's
  // swallow-until-release, the contract pad_eat_held already keeps.
  const int *raw_held = held;
  int dead_held[IN_COUNT] = {0};
  if (!p->alive) {
    if (!g_match.over && mode_respawns() && --p->respawn_t <= 0) {
      player_respawn(p, ent);
    } else {
      dead_held[IN_SCORE] = held[IN_SCORE];
      dead_held[IN_VIEW] = held[IN_VIEW];  // camera toggling stays available
      held = dead_held;
    }
  }
  // Both LOCAL (P0-33): a remote client pressing V must not flip the server's
  // config (config_write would persist it on the next menu close), and the
  // scoreboard key is the local player's HUD state.
  if (ent == g_local_ent) {
    g_score_held = held[IN_SCORE];
    if (held[IN_VIEW] && !p->prev_held[IN_VIEW])
      g_cfg.third_person = !g_cfg.third_person;
  }

  // A jump press is buffered for a few ticks so a press just before landing
  // (or on the exact landing tick) is not silently swallowed.
  if (held[IN_JUMP] && !p->prev_held[IN_JUMP]) p->jump_buf = JUMP_BUFFER_TICKS;
  else if (p->jump_buf > 0) p->jump_buf--;

  if (p->slide_cd > 0) p->slide_cd--;

  // Slide: crouch while moving fast on the ground — on the press edge, or on
  // the landing tick with crouch held. A crouch pressed while airborne queues
  // the slide so the landing transitions into it dynamically. The launch
  // follows the current velocity, so slides work in every direction.
  float sp2d = sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z);
  int crouch_edge = held[IN_CROUCH] && !p->prev_held[IN_CROUCH];
  int landed = p->grounded && !p->prev_grounded;
  if (crouch_edge && !p->grounded) p->slide_queued = 1;
  if (!held[IN_CROUCH]) p->slide_queued = 0;
  // Entry gate on a fraction of run speed — the ground move always reaches
  // full run speed (no friction equilibrium below it), so no slider combo can
  // put the gate out of reach.
  if (!p->sliding && p->grounded && held[IN_CROUCH] && (crouch_edge || landed) &&
      sp2d > mv[MV_RUN] * SLIDE_START_SPEED) {
    p->sliding = 1;
    // The boost fires on a fresh grounded press or a queued airborne press —
    // never when crouch was simply held through a slide-jump, and always
    // cooldown-gated, so hop chains bleed out instead of pumping forever.
    // Boost speed scales with run speed: MV_SLIDE_BOOST is a multiplier.
    if ((crouch_edge || p->slide_queued) && p->slide_cd == 0) {
      sp2d = mv_slide_launch(&p->vel, sp2d);
      p->slide_cd = SLIDE_COOLDOWN_TICKS;
    }
  }
  if (p->grounded) p->slide_queued = 0;  // the landing consumed (or wasted) it
  // A slide ends by releasing crouch, leaving the ground, or bleeding down to
  // crouch-walk speed — then it settles seamlessly into a regular crouch.
  if (p->sliding && mv_slide_should_end(p->grounded, held[IN_CROUCH], sp2d))
    p->sliding = 0;

  // Crouch state. On the ground the box folds from the top (it follows the
  // smoothed eye down, so the camera never pokes above the box) and standing
  // up requires headroom. In the air crouching is a tuck: the legs pull up
  // while the head stays level in world space — the feet plane rises by the
  // stand/crouch difference (crouch-jump clearance), the box top is
  // unchanged, and pos/eye shift in tandem so the camera never dips.
  const float tuck = HEIGHT_STAND - HEIGHT_CROUCH;
  if (held[IN_CROUCH]) {
    if (!p->crouched && !p->grounded) {
      p->pos.y += tuck;
      p->eye -= tuck;
      p->tuck_y = tuck;   // publish it: the figure must not follow this step
    }
    p->crouched = 1;
  } else if (p->crouched) {
    if (p->grounded) {
      if (player_fits(p, HEIGHT_STAND)) p->crouched = 0;
    } else {
      p->pos.y -= tuck;  // legs extend back down, head stays level
      if (p->pos.y >= 0 && player_fits(p, HEIGHT_STAND)) {
        p->eye += tuck;
        p->crouched = 0;
        p->tuck_y = 0.0f;
      } else {
        p->pos.y += tuck;  // no room below (solid or arena floor): stay tucked
      }
    }
  }
  float height = player_col_height(p);
  float max_speed = p->crouched ? mv[MV_CROUCH_SP] : mv[MV_RUN];
  max_speed *= mv_ads_walk_scale(p->wp.ads_s);  // aimed walk: slight tactical slow

  // Wish direction in the yaw plane.
  float fx = sinf(p->yaw), fz = -cosf(p->yaw);
  float wx = 0, wz = 0;
  if (held[IN_FWD])   { wx += fx; wz += fz; }
  if (held[IN_BACK])  { wx -= fx; wz -= fz; }
  if (held[IN_RIGHT]) { wx += -fz; wz += fx; }
  if (held[IN_LEFT])  { wx -= -fz; wz -= fx; }
  // Left stick: analog move intent in the same wish space, handed in by the
  // caller AFTER deadzone (the sim never reads g_pad: pad settings are
  // client preference, and a server applying ITS deadzone to a client's stick
  // is a permanent misprediction). The magnitude is KEPT below 1 (mv_walk's
  // target velocity scales with it, so half deflection walks at half speed).
  if (wish_px != 0.0f || wish_py != 0.0f) {
    wx += fx * wish_py + -fz * wish_px;
    wz += fz * wish_py + fx * wish_px;
  }
  float wlen = sqrtf(wx * wx + wz * wz);
  if (wlen > 1e-5f) {
    float m = wlen > 1.0f ? 1.0f : wlen;
    wx = wx / wlen * m;
    wz = wz / wlen * m;
  }

  // LADDER: facing the rungs, forward climbs and back descends; strafe
  // shuffles along them, jump kicks off. A grounded body only mounts by
  // pushing INTO the face, so walking past a ladder never grabs anyone. At
  // the crown the climb turns into a step over the parapet onto the roof.
  int climbing = 0;
  int lad = p->alive ? ladder_touch(p->pos) : -1;
  if (lad >= 0) {
    const ladder_t *L = &g_ladders[lad];
    float into = -(wx * L->nx + wz * L->nz);          // + toward the wall
    float along = wx * -L->nz + wz * L->nx;
    if (!p->grounded || into > 0.3f) {
      if (p->jump_buf > 0 && !p->grounded) {          // kick off the rungs
        p->vel = (v3){{L->nx * 3.5f, 3.0f, L->nz * 3.5f}};
        p->jump_buf = 0;
      } else {
        climbing = 1;
        p->sliding = 0;
        float vy = into * LADDER_CLIMB;
        if (p->grounded && vy < 0.0f) vy = 0.0f;
        float hold = into > 0.0f ? 1.5f : 0.0f;      // keep the body on the rungs
        if (p->pos.y > L->top - 0.35f && into > 0.0f) {
          vy = fmaxf(vy, 2.0f);                       // clear the crown...
          hold = 3.0f;                                // ...and step over it
        }
        p->vel = (v3){{-L->nx * hold + -L->nz * along * 1.2f, vy,
                       -L->nz * hold + L->nx * along * 1.2f}};
        p->jump_buf = 0;
      }
    }
  }

  if (climbing) {
    // the ladder set the velocity
  } else if (p->grounded) {
    if (p->sliding) sp2d = mv_slide_step(&p->vel, sp2d, wx, wz, wlen);
    else            mv_walk(&p->vel, wx, wz, wlen, max_speed);
    if (p->jump_buf > 0) {
      p->vel.y = mv_jump_speed();
      // CONTEXT: a jump into a ledge is a vault or a mantle (mv_ledge). It is
      // a VELOCITY decision on the press tick and nothing else — no state, no
      // animation lock — so a client predicting it and the server simulating
      // it cannot disagree about anything a snapshot does not carry.
      mv_ledge(p, wx, wz, wlen, sp2d);
      p->grounded = 0;
      p->sliding = 0;  // slide-jump: full horizontal momentum carries over
      p->jump_buf = 0;
    }
  } else {
    mv_air(&p->vel, wx, wz, wlen, sp2d);
  }

  if (!climbing) p->vel.y -= GRAVITY * TICK_DT;
  p->grounded = 0;
  float vy_pre = p->vel.y;  // impact speed: the move zeroes vel.y on contact
  ent_move(&p->pos, &p->vel, &p->grounded, height);
  arena_clamp(&p->pos);
  // Touching down un-bakes the tuck WITHOUT a step in pos.y: on the ground the
  // collider's floor is the boots again (a grounded crouch folds the box from
  // the TOP, following the smoothed eye). This is therefore the one transition
  // where the published tuck changes and the origin does not — which is why
  // anim_t.root_off unwinds this one instead of tracking it. See there.
  if (p->grounded) p->tuck_y = 0.0f;

  // Landing dip: a fresh ground contact compresses the eye in proportion to
  // the impact speed, then springs back. Pure camera translation — the sim
  // and the aim direction never see it.
  p->land_dip -= p->land_dip * f_clamp(LAND_DIP_RECOVER * TICK_DT, 0, 1);
  if (p->grounded && !was_grounded && vy_pre < 0) {
    float d = -vy_pre * LAND_DIP_SCALE;
    if (d > LAND_DIP_MAX) d = LAND_DIP_MAX;
    if (d > p->land_dip) p->land_dip = d;
  }

  // Dead: the camera sinks toward the floor — a cheap, readable death cam.
  float eye_target = !p->alive ? 0.35f : p->crouched ? EYE_CROUCH : EYE_STAND;
  p->eye += (eye_target - p->eye) * f_clamp(EYE_LERP_RATE * TICK_DT, 0, 1);

  // FOV kick: widen while sliding, eased both ways for a smooth speed surge.
  float kick_target = p->sliding ? 1.0f : 0.0f;
  p->fov_kick += (kick_target - p->fov_kick) * f_clamp(FOV_KICK_RATE * TICK_DT, 0, 1);

  // Lean before the weapon: a shot fired this tick leaves from the leaned eye.
  player_lean_tick(p, held);

  weapon_tick(p, ent, held);
  if (ent == g_local_ent) vm_tick(p);   // the viewmodel is the local client's
  p->prev_grounded = was_grounded;      // read only by our own next move tick
  for (int i = 0; i < IN_COUNT; i++) p->prev_held[i] = raw_held[i];
}

// The PRESENT half: anim solve and the corpse handover. MUST run after
// vfx_tick — corpse_t.born exists precisely because a body added after
// vfx_tick in the same world tick would integrate twice on its handover tick.
static void player_present_tick(player_t *p, int ent) {
  int h = human_of_ent(ent);
  // prev_held holds THIS tick's raw input (copied at the end of the move
  // half), and the anim only runs alive, where raw == effective held.
  if (p->alive)
    anim_tick(&g_player_anim[h], p->pos, p->vel, p->yaw, p->pitch, p->grounded,
              p->crouched && !p->sliding, p->tuck_y, p->sliding, p->wp.ads_s,
              p->lean,
              p->prev_held[IN_FIRE] || p->prev_held[IN_ADS] ||
                  p->wp.cool[p->wp.cur] > 0 || p->wp.melee_t > 0 ||
                  p->wp.gr_cook > 0 || p->wp.gr_t > 0,
              ent);
  else {
    g_player_anim[h].prev_pose = g_player_anim[h].pose;
    if (g_player_corpse_q[h]) {
      g_player_corpse_q[h] = 0;
      corpse_add(&g_player_anim[h], p->pos, p->pos, g_player_rag[h], p->yaw,
                 0.0f, 0.0f, gun_anim(&p->wp, 1.0f), p->wp.cur, 1, ent);
    }
  }
}

// Input storage for humans 1..MAX_HUMANS-1 — filled from the network (Phase
// 3+); slot 0 is unused (the local player's input arrives per call).
static int   g_human_held_all[SRV_LOBBIES][MAX_HUMANS][IN_COUNT];
#define g_human_held (g_human_held_all[g_lobby])
static float g_human_wish_all[SRV_LOBBIES][MAX_HUMANS][2];
#define g_human_wish (g_human_wish_all[g_lobby])
// Whether this human's move tick runs THIS world tick. Only the dedicated
// server's input consume ever writes 0 here (one client input = one move —
// everywhere else the initializer keeps the historical always-move.
static int   g_human_move_all[SRV_LOBBIES][MAX_HUMANS] =
    {[0 ... SRV_LOBBIES - 1] = {[0 ... MAX_HUMANS - 1] = 1}};
#define g_human_move (g_human_move_all[g_lobby])
// The view tick of the input consumed this world tick (lag comp);
// -1 = no rewind (offline, or the client is not synced yet). Consume-written.
static long  g_human_view_all[SRV_LOBBIES][MAX_HUMANS] =
    {[0 ... SRV_LOBBIES - 1] = {[0 ... MAX_HUMANS - 1] = -1}};
#define g_human_view (g_human_view_all[g_lobby])

// THE WORLD TICK. The name and signature stay: sim_advance and app_frame
// call this once per tick with the LOCAL player's input. With one human this
// is byte-for-byte the historical order — move half, match_tick, vfx_tick,
// present half. g_tick advances HERE, exactly once, never per human.
static void player_tick(player_t *p, const int held[IN_COUNT]) {
  (void)p;   // p is &g_players[0]; the roster below covers it
  g_tick++;
  for (int h = 0; h < MAX_HUMANS; h++) {
    if (!g_humans_on[h]) continue;
    int ent = ent_of_human(h);
    player_t *hp = &g_players[h];
    float wpx = 0.0f, wpy = 0.0f;
    const int *hh = g_human_held[h];
    if (ent == g_local_ent) {
      hh = held;
      // The pad wish is assembled HERE, outside the sim half — local input
      // only, gated exactly as frame_input_keys gates the keyboard.
      if (g_pad.connected && hp->alive && !g_menu_open) {
        float mx, my;
        if (pad_deadzone(g_pad.lx, g_pad.ly, g_cfg.pad[PAD_DZ_L], &mx, &my) > 0) {
          wpx = mx;
          wpy = my;
        }
      }
    } else {
      if (!g_human_move[h]) continue;   // no input consumed -> no move
      wpx = g_human_wish[h][0];
      wpy = g_human_wish[h][1];
      // Shots inside THIS move trace against the world the shooter saw.
      g_lagcomp_view = g_human_view[h];
    }
    player_move_tick(hp, ent, hh, wpx, wpy);
    g_lagcomp_view = -1;               // bots and the present half stay live
  }
  // A hostage's restraints: every human's RELOAD (the local one through the
  // menu gate, the others from their consumed input).
  for (int h = 0; h < MAX_HUMANS; h++)
    g_resc.use_held[h] = !g_humans_on[h] ? 0
                       : ent_of_human(h) == g_local_ent ? held[IN_RELOAD] && !g_menu_open
                                                        : g_human_held[h][IN_RELOAD];
  match_tick();
  vfx_tick();
  for (int h = 0; h < MAX_HUMANS; h++)
    if (g_humans_on[h])
      player_present_tick(&g_players[h], ent_of_human(h));
  // The outbound event log is filled HERE, in the one world tick every
  // caller shares, and not in the server's own entry point: each caller drains
  // g_events at its own moment — the harness at the end of sim_advance, the
  // server right after its tick — so a capture anywhere else sees an empty list
  // for somebody. On a client nothing ever reads the log; the copy is ~10
  // records against a full sim tick.
  net_ev_capture();
}

// One SERVER tick: the same world tick plus the PER-TICK event drain —
// without the drain the 128-slot ring saturates after ~15 ticks, silently
// (measured ev_peak is 8-10 per tick at 20 bots hard). The server never runs
// app_frame (that renders, drains audio per FRAME, reads g_menu_open and
// scales the accumulator with hit-stop); this body is what `netloop` proves
// equal to the harness's sim_advance.
static void net_srv_consume_inputs(void);   // defined with the conn layer
[[maybe_unused]] static void server_tick_once(const int held[IN_COUNT]) {
  static const int no_held[IN_COUNT] = {0};
  net_srv_consume_inputs();   // one buffered client input per world tick
  player_tick(&g_players[0], held ? held : no_held);
  net_hist_record();
  g_num_events = 0;      // player_tick already captured them into the log
}

