// code/render/fx_pass.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// The player's first-person tracer re-anchor: render_frame FOV-remaps the
// viewmodel's published muzzle into world-projection space before fx_build
// runs, so the streak's start lands on the same screen pixel as the drawn
// barrel tip. Live only when the viewmodel pass will actually run this frame.
static v3  g_fx_muzzle;
static float g_fx_alpha;   // this frame's tick fraction, for a bolt's flight
static int g_fx_muz_live;
static int g_fx_vm_flash;  // this frame's viewmodel pass will draw the
                           // player's flash star — the world pass must not
                           // draw a second one at the third-person muzzle

// Additive light pass: tracer streaks (hot core cooling downrange under a
// wide halo, the tail receding toward the impact so a burst reads as rounds
// LEAVING rather than a laser held on the wall), muzzle-flash stars for every
// shooter the world pass can see, and impact sparks (motion-stretched embers
// cooling from white-hot to orange).
// THE TWO-LERP COOLING LAW, ONE COPY. Green falls first and red last, because
// a straight white->red lerp passes through pink and nothing hot is ever pink.
// The four emitters (kill burst, body spark, skid ember, stone chip) differ only
// in the three colours and in where the knee sits — never in the law, which is
// why the four hand-written copies each carried a comment claiming it was shared.
static v3 fx_cool(float a1, float knee, v3 hot, v3 mid, v3 cold) {
  return a1 < knee ? v3_lerp(hot, mid, a1 / knee)
                   : v3_lerp(mid, cold, (a1 - knee) / (1.0f - knee));
}

static void fx_build(v3 eye) {
  scene_reset();
  for (int i = 0; i < MAX_TRACERS; i++)
    if (g_tracers[i].life > 0) {
      tracer_t *tr = &g_tracers[i];
      const tracer_look_t *tl = tracer_look(tr->wpn);
      // ANCHORED ONCE, ON THE FIRST FRAME THAT DRAWS IT, and then left alone. The
      // event fires before the shooter's pose for this tick is solved, so the
      // spawn point is one pose stale and, for a rifle coming up from the low
      // ready, nowhere near the barrel. Your own shot takes the viewmodel's tip;
      // it used to be re-glued to that tip every frame, which swung the whole
      // line around the impact with every degree of recoil.
      if (tr->anchor) {
        tr->anchor = 0;
        if (tr->src == g_local_ent && g_fx_muz_live) tr->a = g_fx_muzzle;
        else if (tr->src >= 0) {
          int mh = human_of_ent(tr->src);
          tr->a = pose_muzzle(mh >= 0 ? &g_player_anim[mh].pose
                                      : &g_bots[tr->src - 1].anim.pose, tr->wpn);
        }
        if (tr->smoke) { tr->smoke = 0; vfx_muzzle_smoke(tr->a, tr->d, tr->wpn,
                                                         tr->src == g_local_ent && g_fx_muz_live); }
        // A muzzle already past the wall the round hit: a contact shot. There is
        // no line to draw, and drawing it would send the bolt backward.
        if (v3_dot(v3_sub(tr->b, tr->a), tr->d) < 0.6f) { tr->life = 0; continue; }
      }
      v3 d = v3_sub(tr->b, tr->a);
      float len = v3_len(d);
      if (len < 0.6f && !tr->rico) continue;
      if (len < 0.05f) continue;
      // THE LINE RUNS FROM THE BARREL AS DRAWN NOW TO THE HIT. Anchored once in
      // the world, its near end sits half a metre from the eye: the gun then
      // recoils under it, and a strafe at 4-5 cm per tick swings that end ~35 px
      // per tick by parallax — a bolt slowed for the eye and held on the barrel
      // for metres turns the shooter's own motion into a line that misses both
      // the gun and the hit. The far end keeps the world (that is where the round
      // went); the correction falls off linearly to it, so the streak stays one
      // straight line from the drawn muzzle to the impact in every frame.
      v3 glue = {{0, 0, 0}};
      if (!tr->rico && tr->src == g_local_ent && g_fx_muz_live)
        glue = v3_sub(g_fx_muzzle, tr->a);
      #define TR_AT(u) v3_add(v3_add(tr->a, v3_scale(d, (u))), v3_scale(glue, 1.0f - (u)))
      // Where the bolt is along the line. It starts 3 m out so the very first
      // frame shows which way it went, then the tail lets go of the barrel. A
      // skip starts AT the face it skipped off, after its parent arrives.
      // THE FLIGHT CLOCK STARTS ON THE FIRST FRAME THAT SHOWS IT. Counted from the
      // tick the shot was simulated, the bolt was already 1-2 ticks old (plus the
      // 3 m lead: 8-10 m out) when it was first drawn, so on anything nearer —
      // a wall across a street, a man at a corner — the round was never seen
      // leaving the gun, only a stub collapsing into the hit by the crosshair.
      // A skip spawned on the same tick shares its parent's offset.
      float clk = (float)(tr->life0 - tr->life) + g_fx_alpha;
      if (!tr->seen) { tr->seen = 1; tr->base = clk; }
      float age = clk - tr->base - (float)tr->delay;
      if (age < 0.0f) continue;
      float lead = tr->rico ? 0.0f : 3.0f;
      float nose = lead + tr->v * age;
      float slen = tr->rico ? fminf(3.0f, len) : fmaxf(tl->len, tl->span * len);
      float wk = tr->rico ? 1.10f : tl->w;
      float bk = tr->rico ? 1.15f : tl->k;
      // THE HEAT WAKE: the line the round burned through the air, left hanging
      // after the bolt has gone — thin and hot first, then wider and fainter
      // as it diffuses, from the muzzle to wherever the nose has got to.
      if (tr->wake > 0) {
        float wl = (float)tr->life / (float)tr->wake;     // 1 -> 0 over the wake
        float wt1 = f_clamp(nose / len, 0.0f, 1.0f);
        float bloom = 1.0f - f_clamp(wl, 0.0f, 1.0f);
        float wi = f_clamp(wl, 0.0f, 1.0f);
        wi = wi * wi * 0.55f;
        if (wi > 0.004f && wt1 > 0.02f) {
          const int WN = 8;
          for (int sgi = 0; sgi < WN; sgi++) {
            float ua = wt1 * (float)sgi / WN, ub = wt1 * (float)(sgi + 1) / WN;
            v3 pa = TR_AT(ua), pb = TR_AT(ub);
            // Starts 1.5 m off the barrel: the shooter's own wake is not a bar
            // across his sight picture.
            float ra = f_clamp((ua * len - 1.5f) * 0.4f, 0.0f, 1.0f);
            float rb = f_clamp((ub * len - 1.5f) * 0.4f, 0.0f, 1.0f);
            // A slow shimmer along it, so it reads as moving air and not a wire.
            float sa = 0.75f + 0.25f * sinf(ua * len * 1.7f + (float)tr->life * 0.21f);
            float sb = 0.75f + 0.25f * sinf(ub * len * 1.7f + (float)tr->life * 0.21f);
            v3 wc = {{0.30f, 0.27f, 0.22f}};
            float w0 = 0.022f + 0.090f * bloom;
            fx_seg(pa, pb, eye, w0 * ra, w0 * rb, v3_scale(wc, wi * ra * sa),
                   v3_scale(wc, wi * rb * sb));
          }
        }
      }
      // The bolt itself is gone once it has run into the hit; the wake (if
      // any) is what the rest of the life is for.
      float t1 = f_clamp(nose / len, 0.0f, 1.0f);
      float t0 = f_clamp((nose - slen) / len, 0.0f, 1.0f);
      if (t1 - t0 < 1e-3f) continue;
      int flight = tr->life - tr->wake;                    // ticks of bolt left
      float f = f_clamp((float)flight / 2.0f, 0.0f, 1.0f);  // arrival dies quick
      if (tr->rico) f *= 1.0f - f_clamp(nose / len, 0.0f, 1.0f) * 0.7f;
      if (f <= 0.0f) continue;
      const int N = 6;
      for (int sgi = 0; sgi < N; sgi++) {
        float ua = t0 + (t1 - t0) * (float)sgi / N;
        float ub = t0 + (t1 - t0) * (float)(sgi + 1) / N;
        v3 pa = TR_AT(ua), pb = TR_AT(ub);
        // Colour runs on absolute distance, as it always has; the new term is the
        // comet: brightest at the nose, gone at the tail.
        float ka = f_clamp(ua * len * (1.0f / 14.0f), 0.0f, 1.0f);
        float kb = f_clamp(ub * len * (1.0f / 14.0f), 0.0f, 1.0f);
        if (tr->rico) ka = kb = 0.45f;                     // a skip has shed some heat
        float fa = f * bk * (0.35f + 0.65f * (float)sgi / N);
        float fb = f * bk * (0.35f + 0.65f * (float)(sgi + 1) / N);
        v3 ca = v3_lerp(tl->hot, tl->cool, ka), cb = v3_lerp(tl->hot, tl->cool, kb);
        v3 ha = v3_lerp(tl->hhot, tl->hcool, ka), hb = v3_lerp(tl->hhot, tl->hcool, kb);
        // Thin at the bore, full width by 2.5 m: at 0.3 m from the eye a wide halo
        // is a wedge across exactly the pixels the player aims with.
        float ra = tr->rico ? 1.0f : f_clamp(ua * len * (1.0f / 2.5f), 0.0f, 1.0f);
        float rb = tr->rico ? 1.0f : f_clamp(ub * len * (1.0f / 2.5f), 0.0f, 1.0f);
        float wha = (0.010f + 0.045f * ra) * (1.0f - 0.5f * ka) * wk;
        float whb = (0.010f + 0.045f * rb) * (1.0f - 0.5f * kb) * wk;
        float wca = (0.0040f + 0.0050f * ra) * (1.0f - 0.45f * ka) * wk;
        float wcb = (0.0040f + 0.0050f * rb) * (1.0f - 0.45f * kb) * wk;
        fx_seg(pa, pb, eye, wha, whb, v3_scale(ha, fa * fa), v3_scale(hb, fb * fb));
        fx_seg(pa, pb, eye, wca, wcb, v3_scale(ca, fa), v3_scale(cb, fb));
      }
      #undef TR_AT
    }
  // THE THROW PREVIEW: the real flight, stepped ahead from the real launch while the
  // pin is out, in dots that fade to where it first lands, with a ring on that spot.
  {
    const player_t *lp = local_player();
    if (lp && lp->alive && lp->wp.cur == WPN_GR && lp->wp.gr_cook > 0 && !g_home) {
      v3 pos, vel;
      int lob = lp->prev_held[IN_ADS];
      nade_launch(lp, lob, &pos, &vel);
      for (int i = 1; i < 240; i++) {
        vel.y -= NADE_GRAVITY * TICK_DT;
        vel = v3_scale(vel, 1.0f - NADE_DRAG);
        float sp = v3_len(vel), step = sp * TICK_DT;
        v3 d = v3_scale(vel, 1.0f / fmaxf(sp, 1e-4f)), wn = {{0, 1, 0}};
        int ws = -1;
        float hit = trace_world(pos, d, &wn, &ws);
        if (hit < step + NADE_R) {
          pos = v3_add(pos, v3_scale(d, hit));
          for (int a = 0; a < 28; a++) {
            float a0 = (float)a / 28.0f * F_TAU, a1 = (float)(a + 1) / 28.0f * F_TAU;
            v3 t1, t2;
            axis_frame(wn, &t1, &t2);
            v3 ring0 = v3_add(pos, v3_add(v3_scale(t1, cosf(a0) * 0.45f), v3_scale(t2, sinf(a0) * 0.45f)));
            v3 ring1 = v3_add(pos, v3_add(v3_scale(t1, cosf(a1) * 0.45f), v3_scale(t2, sinf(a1) * 0.45f)));
            fx_seg(v3_add(ring0, v3_scale(wn, 0.03f)), v3_add(ring1, v3_scale(wn, 0.03f)), eye,
                   0.016f, 0.016f, (v3){{0.9f, 0.34f, 0.08f}}, (v3){{0.9f, 0.34f, 0.08f}});
          }
          break;
        }
        pos = v3_add(pos, v3_scale(vel, TICK_DT));
        if (i >= 8 && i % 7 == 0) {
          float f = 1.0f - 0.55f * (float)i / 240.0f;
          fx_disc(pos, eye, 0.030f, v3_scale((v3){{0.95f, 0.40f, 0.10f}}, f), (v3){{0, 0, 0}}, 8);
        }
      }
    }
  }
  // The fireball: a soft disc facing the eye, white at birth, through orange to
  // a dull red, and swelling as it dies.
  for (int i = 0; i < MAX_FIRE; i++)
    if (g_fire[i].life > 0) {
      const fire_t *q = &g_fire[i];
      float f = (float)q->life / (float)q->life0, a1 = 1.0f - f;
      v3 c = fx_cool(a1, 0.34f, (v3){{2.6f, 2.2f, 1.6f}}, (v3){{2.2f, 0.95f, 0.22f}},
                     (v3){{0.42f, 0.06f, 0.01f}});
      if (q->dull) {   // banked fire, licking: never white, in and out as it dies
        float lick = 0.6f + 0.4f * sinf((float)g_tick * 0.31f + (float)i * 2.1f);
        c = v3_scale(fx_cool(a1, 0.40f, (v3){{0.80f, 0.31f, 0.06f}}, (v3){{0.42f, 0.10f, 0.015f}},
                             (v3){{0.10f, 0.015f, 0.0f}}), lick * f_clamp(a1 * 8.0f, 0.0f, 1.0f));
      }
      fx_disc(q->pos, eye, q->r * (0.55f + 0.75f * sqrtf(a1)), c, (v3){{0, 0, 0}}, 12);
    }
  // Young smoke is lit from under by the fire inside it.
  for (int i = 0; i < MAX_PUFFS; i++)
    if (g_puffs[i].life > 0 && g_puffs[i].r1 >= 1.0f && g_puffs[i].soot > 0.2f) {
      const puff_t *q = &g_puffs[i];
      float age = (float)(q->life0 - q->life) / 170.0f;
      if (age >= 1.0f) continue;
      float k = (1.0f - age) * (1.0f - age) * f_clamp(age * 14.0f, 0.0f, 1.0f);
      float r = q->r0 + (q->r1 - q->r0) * sqrtf((float)(q->life0 - q->life) / (float)q->life0);
      fx_disc(q->pos, eye, r * 0.85f, v3_scale((v3){{0.10f, 0.036f, 0.007f}}, k), (v3){{0, 0, 0}}, 10);
    }
  for (int i = 0; i < MAX_BRANDS; i++)
    if (g_brands[i].life > 0) {
      const brand_t *q = &g_brands[i];
      float f = (float)q->life / (float)q->life0;
      fx_disc(q->pos, eye, 0.09f + 0.10f * f, v3_scale((v3){{2.2f, 0.85f, 0.16f}}, f_clamp(f * 2.0f, 0.0f, 1.0f)),
              (v3){{0, 0, 0}}, 8);
    }
  for (int i = 0; i < MAX_SHOCK; i++)
    if (g_shock[i].life > 0) {
      const shock_t *q = &g_shock[i];
      float a1 = 1.0f - (float)q->life / (float)SHOCK_LIFE, f = 1.0f - a1;
      float rr = BLAST_R * (1.0f - f * f * f);
      // The shell, as seen from here, and its footprint running out over the floor.
      v3 tow = v3_norm(v3_sub(eye, q->pos)), u, v;
      axis_frame(tow, &u, &v);
      fx_annulus(q->pos, u, v, rr, 0.9f + 1.6f * a1, v3_scale((v3){{0.15f, 0.135f, 0.115f}}, f * f), 48);
      if (q->open)
        fx_annulus((v3){{q->pos.x, q->gy + 0.04f, q->pos.z}}, (v3){{1, 0, 0}}, (v3){{0, 0, 1}},
                   rr, 0.7f + 1.5f * a1, v3_scale((v3){{0.42f, 0.31f, 0.19f}}, f * f), 48);
      // The rays: one instant of hard light, long and thin, uneven in length.
      if (q->life > SHOCK_LIFE - 15) {
        float ry = (float)(q->life - (SHOCK_LIFE - 15)) / 15.0f;
        for (int n = 0; n < 18; n++) {
          uint32_t h = fx_hash(q->seed + (uint32_t)n * 977u);
          float az = (float)(h & 1023) / 1023.0f * F_TAU;
          float el = (float)((h >> 10) & 1023) / 1023.0f * 1.45f;
          float ln = (3.0f + 8.5f * (float)((h >> 20) & 1023) / 1023.0f) * (0.45f + 0.55f * sqrtf(1.0f - ry));
          v3 d = {{cosf(az) * cosf(el), sinf(el), sinf(az) * cosf(el)}};
          fx_seg(q->pos, v3_add(q->pos, v3_scale(d, ln)), eye, 0.20f * ry, 0.0f,
                 v3_scale((v3){{2.1f, 1.45f, 0.72f}}, ry * ry), (v3){{0, 0, 0}});
        }
      }
    }
  // The bloom of the flash in the air: on the charge, so a wall still hides it.
  if (g_blast_light > 0.01f)
    fx_disc(g_blast_p, eye, 15.0f, v3_scale((v3){{0.75f, 0.52f, 0.30f}}, g_blast_light),
            (v3){{0, 0, 0}}, 24);
  {
    // AIRBORNE DUST: motes hanging in a 12 m cube that tiles around the eye,
    // each drifting on the wind and bobbing, so the field is fixed in the
    // WORLD and the player walks through it. They are only seen by forward
    // scatter — bright looking toward the sun, nearly gone away from it —
    // which is how dust in a sunbeam actually shows.
    v3 sn = atm_sun();
    const atm_t *am = atm_now();
    float tt = g_ui_time;
    v3 wd = wind_dir();
    for (int i = 0; i < 140; i++) {
      uint32_t hh = fx_hash((uint32_t)i * 0x9E3779B9u + 0x51u);
      float rx = (float)(hh & 1023) / 1023.0f, ry = (float)((hh >> 10) & 1023) / 1023.0f;
      float rz = (float)((hh >> 20) & 1023) / 1023.0f;
      v3 q = {{rx * 12.0f + wd.x * tt * 0.25f, ry * 4.0f + 0.15f * sinf(tt * 0.6f + rx * 9.0f),
               rz * 12.0f + wd.z * tt * 0.25f}};
      q.x = eye.x - 6.0f + fmodf(fmodf(q.x - (eye.x - 6.0f), 12.0f) + 12.0f, 12.0f);
      q.z = eye.z - 6.0f + fmodf(fmodf(q.z - (eye.z - 6.0f), 12.0f) + 12.0f, 12.0f);
      q.y = eye.y - 2.0f + fmodf(fmodf(q.y - (eye.y - 2.0f), 4.0f) + 4.0f, 4.0f);
      if (q.y < 0.05f) continue;
      v3 dv = v3_sub(q, eye);
      float dl = v3_len(dv);
      if (dl < 0.6f || dl > 7.0f) continue;
      float mu = v3_dot(v3_scale(dv, 1.0f / dl), sn);
      float ph = mu > 0.0f ? mu * mu * mu * mu : 0.0f;
      float k = (0.04f + 0.55f * ph) * (1.0f - dl / 7.0f) * (0.5f + 0.5f * (float)((hh >> 30) & 1));
      if (k < 0.01f) continue;
      fx_disc(q, eye, 0.004f + 0.003f * rx,
              v3_scale((v3){{am->sun_col[0], am->sun_col[1], am->sun_col[2]}}, k * 0.10f),
              (v3){{0, 0, 0}}, 6);
    }
  }
  for (int i = 0; i < MAX_DUST; i++)
    if (g_dust[i].life > 0) {
      const dust_t *q = &g_dust[i];
      float a1 = 1.0f - (float)q->life / (float)q->life0;
      float r = q->r0 + (q->r1 - q->r0) * sqrtf(a1);
      // In fast, out slow; and eased out near the lens like every burst.
      float k = f_clamp(a1 * 10.0f, 0.0f, 1.0f) * (1.0f - a1) * (1.0f - a1);
      float nf = 1.0f;
      if (q->wisp) {
        v3 dq = v3_sub(q->pos, eye);
        if (v3_dot(dq, dq) < 0.12f * 0.12f) continue;
      } else if (!fx_near_ease(q->pos, eye, &nf)) continue;
      fx_disc(q->pos, eye, r, v3_scale(q->col, k * nf), (v3){{0, 0, 0}}, 10);
    }
  for (int i = 0; i < MAX_FLASHES; i++) {
    flash_t *fl = &g_flashes[i];
    if (fl->src == g_local_ent && g_fx_vm_flash) continue;  // the vm star owns it
    int fresh = fl->fresh && g_tick - fl->t0 <= 12;
    if (fl->life <= 0 && !fresh) continue;
    if (fl->fresh && fl->src >= 0) {       // same one-pose-stale spawn as the bolt
      int mh = human_of_ent(fl->src);
      fl->pos = pose_muzzle(mh >= 0 ? &g_player_anim[mh].pose
                                    : &g_bots[fl->src - 1].anim.pose, fl->wpn);
    }
    float f = fresh ? 1.0f : (float)fl->life / (float)flash_ticks(fl->wpn);
    fl->fresh = 0;
    v3 t1, t2;
    axis_frame(fl->dir, &t1, &t2);
    uint32_t h = fx_hash(fl->seed ^ 0x9E37u);
    float s = (0.22f + 0.08f * (float)((h >> 8) & 255) / 255.0f) *
              (0.45f + 0.55f * f);
    fx_flash(fl->pos, fl->dir, t1, t2, eye, s, f, fl->seed, fl->wpn);
  }
  for (int i = 0; i < MAX_PARTICLES; i++)
    if (g_parts[i].life > 0) {
      const particle_t *q = &g_parts[i];
      float f = (float)q->life / (float)q->life0;   // 1 at birth -> 0 at death
      if (q->kind == 3) {
        // Contact flare: the glowing POINT where the knuckle plate is being
        // dragged. Camera-facing rather than a decal on the floor — an
        // additive patch lying on this game's deliberately LIGHT floor clips
        // straight to white and reads as a sheet of paper, which is exactly
        // what the first version of it looked like.
        // A three-armed screen-space ASTERISK and not one quad, because the
        // quad was that same failure one size down: measured at 3x zoom it was
        // a 24 px hard-edged rectangle lying on the floor, and on the strike
        // tick — where it is biggest — it was the most obvious thing in the
        // frame. Arms built in the plane facing the eye have no straight
        // silhouette at any camera angle, and the three overlaps at the centre
        // clip to white, which is what a contact too hot to look at looks like.
        // `sz` is the arm half-length and the width is derived from it, so the
        // shape cannot drift from its own size.
        // THE SAME NEAR-CAMERA EASE EVERY OTHER BURST HAS. This branch returned
        // before any distance test, which was harmless while the only emitter
        // was a skid (a contact under the player's own body, at a fixed small
        // size) and became the worst blowout in the file the moment impacts
        // started spawning one: measured at 0.8 m, three overlapping additive
        // arms painted 94.1% of the 120x120 crosshair box with 2749 px clipped
        // to pure white — against the 18-20% this file tuned the kill burst down
        // to and the 40.9% it called a defect. Two constants, shared with kinds
        // 0, 4 and 5, and nothing else about the flare changes.
        float nf3;
        if (!fx_near_ease(q->pos, eye, &nf3)) continue;
        v3 vd = v3_norm(v3_sub(eye, q->pos));
        v3 t1, t2;
        axis_frame(vd, &t1, &t2);
        v3 col = v3_scale((v3){{1.75f, 0.80f, 0.17f}}, f * f * nf3);
        v3 tipc = v3_scale(col, 0.10f);
        float w = q->sz * 0.34f;
        for (int k = 0; k < 3; k++) {
          float ang = 0.5235988f + (float)k * 1.0471976f;   // 30 + k*60 deg
          v3 d = v3_add(v3_scale(t1, cosf(ang) * q->sz),
                        v3_scale(t2, sinf(ang) * q->sz));
          fx_seg(v3_sub(q->pos, d), q->pos, eye, w * 0.06f, w, tipc, col);
          fx_seg(q->pos, v3_add(q->pos, d), eye, w, w * 0.06f, col, tipc);
        }
        continue;
      }
      if (q->kind == 1) {
        // Slide dust: a warm sand mote that grows and thins as it dies —
        // additive, so it reads as a sunlit puff over the arena floor. It
        // keeps its OWN divisor rather than `f`: it lives PARTICLE_TICKS+6
        // ticks and its fade was authored against PARTICLE_TICKS, so it opens
        // above 1 and holds full width for its first six ticks. Deliberately
        // left pixel-identical — measured through a recolour-and-diff on a 2 m
        // close-up, this puff touches 1469 px at a peak delta of 20/255 and is
        // STILL invisible at five times the level, because additive haze over
        // a floor this light simply cannot darken anything. Its level is not a
        // knob worth turning; a boot that has to read wants a different
        // mechanism (an alpha-blended puff, or a dark ground decal), and that
        // is its own defect. The knuckle is where the energy is anyway.
        float fd = (float)q->life / PARTICLE_TICKS;
        float g2 = 1.0f - f_clamp(fd, 0.0f, 1.0f);
        v3 tail = v3_add(q->pos, v3_scale(q->vel, -0.040f));
        v3 col = v3_scale((v3){{0.105f, 0.093f, 0.075f}}, fd * fd * 0.55f);
        fx_ribbon(q->pos, tail, eye, 0.006f + 0.010f * g2, col);
        continue;
      }
      if (q->kind == 5) {
        // KILL BURST. Same near-camera ease as the body spark — see the note
        // at KILLBURST_STREAK for why it is shared rather than widened, and
        // what the blowout actually responded to.
        float nf;
        if (!fx_near_ease(q->pos, eye, &nf)) continue;
        // The same two-lerp cooling law as the skid ember and the body spark —
        // green falls first and red last, because a straight white->red lerp
        // passes through pink and nothing hot is ever pink. What differs is
        // where it STARTS and how long it holds: this one opens near white and
        // stays there for the first 30% of its life against the body spark's
        // 22%, which is the whole "hotter, therefore worse for him" read.
        float a1 = 1.0f - f;
        v3 col = fx_cool(a1, 0.30f, (v3){{4.30f, 3.45f, 1.90f}},
                         (v3){{2.60f, 0.86f, 0.10f}}, (v3){{0.80f, 0.05f, 0.01f}});
        col = v3_scale(col, f_clamp(f * 3.0f, 0.0f, 1.0f) * q->sz * nf);
        v3 tail = v3_add(q->pos, v3_scale(q->vel, -KILLBURST_STREAK));
        // Widths take the range scale with the rest of the geometry — `y0` on
        // this kind, see the spawn. Without it the far burst is long hairlines:
        // 8 mm at 20 m is under half a pixel at this FOV and resolution, so the
        // shape would grow while the thing drawing it disappeared.
        float wc = 0.0080f * q->y0, wh = 0.0230f * q->y0;
        fx_teardrop(q->pos, tail, eye, wc, wh, col, fx_warm_halo(col));
        continue;
      }
      if (q->kind == 4) {
        // Body impact spark. It needs no arm in vfx_tick: "arc hard and never
        // bounce" is exactly what the else branch there already gives kind 0,
        // and a body is not a floor to skip off.
        // A FADE, NOT A STEP, AND IT IS NOT ONLY ABOUT THE PLAYER'S OWN BODY.
        // The hard cull was scoped to the one body the camera can be inside; a
        // TARGET's head at contact range is 0.6-1.5 m away and HITSPARK_HEAD_K
        // makes that the hottest burst in the set — measured, a headshot at
        // 0.73 m painted 18.5% of a 120 x 120 box round the crosshair and 7.5%
        // at 1.35 m. Easing the burst out over the same window it would
        // otherwise blow out in keeps the cue at every range a player actually
        // fights at and takes the blowout with it.
        float nf;
        if (!fx_near_ease(q->pos, eye, &nf)) continue;
        // Two lerps like the skid ember and for the same reason: a straight
        // white->red lerp passes through pink, so green has to fall first and
        // red last. Faster and redder than the stone chip — a fragment a body
        // stopped is not a piece of concrete.
        float a1 = 1.0f - f;
        v3 col = fx_cool(a1, 0.22f, (v3){{3.10f, 2.05f, 0.78f}},
                         (v3){{2.20f, 0.52f, 0.06f}}, (v3){{0.72f, 0.04f, 0.01f}});
        col = v3_scale(col, f_clamp(f * 3.0f, 0.0f, 1.0f) * q->sz * nf);
        // The streak's length is a time constant times the SPEED, so the faster
        // spray also draws longer lines — which is most of why the burst reads
        // as an explosion of fragments rather than a puff of dots.
        // ...and the same ANGULAR range scale kind 5 carries, out of `y0`,
        // which this kind never used (it does not bounce, so it has no floor).
        // Measured before it: a body hit was 462 px at 8 m and 91 px at 20 m,
        // i.e. tracking 1/d exactly, while the kill burst standing beside it
        // held 141 px at the same distance. What the player perceives is an
        // ANGLE; world metres divide by range. It multiplies the WIDTHS and the
        // streak constant only, never `sz`, which is HEAT and drives the colour.
        v3 tail = v3_add(q->pos, v3_scale(q->vel, -0.030f * q->y0));
        float wc = 0.0068f * q->y0, wh = 0.0185f * q->y0;
        fx_teardrop(q->pos, tail, eye, wc, wh, col, fx_warm_halo(col));
        continue;
      }
      if (q->kind == 6) {
        // BRASS. Not a spark: it is a lit OBJECT, so it is drawn at a flat
        // brass colour that does not cool, and what makes it read is the
        // TUMBLE — the sliver's projected length swings between full and nearly
        // nothing as it turns, which is exactly the glint a case throws. The
        // axis is built in the plane facing the eye so the tumble is visible
        // from any camera, the same construction the contact flare uses.
        // It fades over its last quarter so it does not pop out of existence,
        // and it takes the same near-camera ease as everything else in this
        // pass — the player's own brass leaves from beside his head.
        v3 dqb = v3_sub(q->pos, eye);
        float dqbl = v3_dot(dqb, dqb);
        if (dqbl < 0.10f * 0.10f) continue;
        v3 vd = v3_norm(v3_sub(eye, q->pos));
        v3 t1, t2;
        axis_frame(vd, &t1, &t2);
        float ph = q->y0 + (float)(q->life0 - q->life) * TICK_DT * BRASS_SPIN;
        // The second cosine is the FORESHORTENING: a case seen end-on is a dot.
        float ln = BRASS_LEN * q->sz * (0.28f + 0.72f * fabsf(cosf(ph * 0.61f)));
        v3 ax = v3_add(v3_scale(t1, cosf(ph) * ln), v3_scale(t2, sinf(ph) * ln));
        float fo = f_clamp((float)q->life / (0.25f * (float)q->life0), 0.0f, 1.0f);
        v3 col = v3_scale((v3){{1.15f, 0.74f, 0.26f}}, fo);
        fx_seg(v3_sub(q->pos, ax), q->pos, eye, 0.0016f, 0.0030f,
               v3_scale(col, 0.45f), col);
        fx_seg(q->pos, v3_add(q->pos, ax), eye, 0.0030f, 0.0016f,
               col, v3_scale(col, 0.45f));
        continue;
      }
      if (q->kind == 2) {
        // A skid spark is a COOLING FRAGMENT. Two lerps and not one, because a
        // straight white->red lerp passes through pink: green has to fall
        // first and red last, which is what a black body actually does.
        float a1 = 1.0f - f;
        v3 col = fx_cool(a1, 0.15f, (v3){{2.70f, 1.72f, 0.62f}},
                         (v3){{1.95f, 0.56f, 0.05f}}, (v3){{0.78f, 0.06f, 0.01f}});
        // Fade the last third out: an additive streak that simply stops
        // existing pops, and at 120 Hz that pop is one whole frame of a bright
        // line vanishing.
        col = v3_scale(col, f_clamp(f * 3.0f, 0.0f, 1.0f) * (0.55f + 0.45f * q->sz));
        // Velocity-aligned STREAK: the length is a time constant times the
        // speed, so a fragment thrown hard draws a long line and one that has
        // nearly stopped draws a dot. That one term is most of the read.
        v3 tail = v3_add(q->pos, v3_scale(q->vel, -0.032f * q->sz));
        // A hero is LONGER much more than it is fatter: scaling both by `sz`
        // gave it the same 15:1 aspect as the crowd, so it read as a bigger
        // shard rather than as a spark that flew further.
        float wk = 0.55f + 0.45f * q->sz;
        float wc = 0.0021f * wk, wh = 0.0060f * wk;
        // The skid ember's halo is deliberately HOTTER than fx_warm_halo's
        // shared cut: measured against the display-encoded floor at 0.78,
        // 0.075 green holds the sum at G 0.87 / B 0.69 round a core that is
        // still allowed to clip — an ember at arm's length. (The derivation
        // lives at fx_warm_halo; only the coefficients differ here.)
        v3 halo = {{col.x * 0.34f, col.y * 0.075f, col.z * 0.030f}};
        fx_teardrop(q->pos, tail, eye, wc, wh, col, halo);
        continue;
      }
      // KIND 0 — the WORLD impact spark, and the effect the player triggers
      // most often, on every round that misses. It was the ONE lit particle in
      // the file that never got the pass kinds 2, 4 and 5 all had: a single flat
      // 12 mm fx_ribbon per fragment, no halo, no near-camera ease, no range
      // scale, drawn with a colour ramp whose peak per-pixel delta measured
      // 120/255 against the body spark's 241. Measured consequences: 40.9% of
      // the 120x120 crosshair box at 0.55 m — a WORSE near blowout than either
      // of the two the file already fixed (18.5% and 18-20%) — and 34 px at
      // 20 m, against this file's own written standard that 305 px "is not a
      // cue at all". This is kind 4's block with a stone chip's colours: cooler
      // and paler than flesh spall, because it is concrete rather than a man.
      float nf;
      if (!fx_near_ease(q->pos, eye, &nf)) continue;
      float a1 = 1.0f - f;
      v3 col = fx_cool(a1, 0.18f, (v3){{2.60f, 1.90f, 0.90f}},
                       (v3){{1.85f, 0.62f, 0.10f}}, (v3){{0.70f, 0.07f, 0.01f}});
      // `sz` is the struck material's HEAT (see EV_IMPACT): steel 1.15, stone
      // 0.80, timber 0.55. Zero only on a spawn that predates it.
      col = v3_scale(col, f_clamp(f * 3.0f, 0.0f, 1.0f) * nf * (q->sz > 0.0f ? q->sz : 1.0f));
      // `y0` is the ANGULAR range scale — see the spawn. Kind 5 has had it from
      // the start and measures a 1.53 extent ratio across a 2.54 range ratio,
      // while kinds 0 and 4 tracked 1/d exactly, i.e. fixed world size.
      v3 tail = v3_add(q->pos, v3_scale(q->vel, -0.026f * q->y0));
      // Wider than the body spark's 0.0068/0.0185, deliberately: a round into
      // concrete throws a bigger, cooler, dustier chip than a round into
      // webbing, and this is the effect the player sees on every missed shot —
      // it has to READ, and measured at 8 m the old 12 mm bar was 0.96 px wide.
      float wc = 0.0092f * q->y0, wh = 0.0320f * q->y0;
      fx_teardrop(q->pos, tail, eye, wc, wh, col, fx_warm_halo(col));
    }
}

