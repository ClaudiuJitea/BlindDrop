// code/render/rescue_world.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// HOSTAGE RESCUE in the world: the hostages (a civilian figure of soft
// ellipsoids — kneeling, hooded and tied while captive; walking, hunched, once
// freed) and the helicopter's rotor, which spins up as the evacuation starts.
// ---------------------------------------------------------------------------
static void hfig_limb(v3 a, v3 b, float r, v3 col, v3 hint) {
  v3 ax = v3_sub(b, a);
  float l = v3_len(ax);
  if (l < 1e-4f) return;
  fig_ball(v3_lerp(a, b, 0.5f), r, l * 0.5f + r * 0.35f, r, ax, hint, 10, col);
}
typedef struct { v3 o, R, Y, F; float topple; } hframe_t;
static v3 hfp(const hframe_t *f, float x, float y, float z) {
  if (f->topple > 0.0f) {   // the executed: fallen on their side
    float c = cosf(f->topple), s = sinf(f->topple);
    float x2 = x * c - y * s, y2 = x * s + y * c;
    x = x2; y = fmaxf(y2 + 0.16f, 0.06f);
  }
  return v3_add(f->o, v3_add(v3_scale(f->R, x), v3_add(v3_scale(f->Y, y), v3_scale(f->F, z))));
}
static void hostage_draw(const hostage_t *h, int i, float alpha) {
  static const v3 SHIRT[RESC_HOSTAGES] = {{{0.80f, 0.78f, 0.72f}}, {{0.36f, 0.48f, 0.64f}},
                                          {{0.60f, 0.52f, 0.36f}}};
  static const v3 PANTS[RESC_HOSTAGES] = {{{0.20f, 0.22f, 0.28f}}, {{0.32f, 0.28f, 0.22f}},
                                          {{0.16f, 0.17f, 0.18f}}};
  v3 skin = rgb(0.60f, 0.43f, 0.32f), hood = rgb(0.47f, 0.39f, 0.27f),
     shoe = rgb(0.10f, 0.09f, 0.08f), hair = rgb(0.09f, 0.07f, 0.06f);
  v3 pos = v3_lerp(h->prev, h->pos, alpha);
  float yaw = h->prev_yaw + wrap_pi(h->yaw - h->prev_yaw) * alpha;
  float pose = h->prev_pose + (h->pose - h->prev_pose) * alpha;
  float m = h->prev_move + (h->move - h->prev_move) * alpha;
  float g = h->prev_gait + (h->gait - h->prev_gait) * alpha;
  float st = smooth01(pose);
  hframe_t f = {pos, {{cosf(yaw), 0, sinf(yaw)}}, {{0, 1, 0}}, {{sinf(yaw), 0, -cosf(yaw)}},
                h->state == HS_DEAD ? 1.45f : 0.0f};
  float breath = 0.006f * sinf((float)g_tick * TICK_DT * 1.7f + (float)i);
  // Kneeling (st 0) and walking (st 1) joints, blended.
  float hy = 0.42f + st * (0.92f - 0.10f * m - 0.42f);
  float lean = 0.07f + st * (0.05f + 0.16f * m);   // slumped kneeling, hunched running
  v3 pel = hfp(&f, 0.0f, hy, -0.02f);
  v3 neck = hfp(&f, 0.0f, hy + 0.61f + breath, 0.0f + lean * 1.2f);
  float bow = h->state == HS_CAPTIVE ? 0.07f : 0.03f;
  v3 head = hfp(&f, 0.0f, hy + 0.73f + breath - bow * 0.4f, lean * 1.3f + bow);
  mat_set(0.85f, 0.0f);
  ao_off();
  // Legs.
  for (int s = -1; s <= 1; s += 2) {
    float ph = g + (s > 0 ? 0.0f : F_PI);
    float a = 0.55f * m * sinf(ph);                       // thigh swing
    float kb = 0.25f + 0.85f * m * fmaxf(0.0f, sinf(ph + 1.3f));   // knee bend
    // walking
    v3 hipw = {{(float)s * 0.10f, hy, -0.02f - 0.10f * (1.0f - st)}};
    v3 knw = {{(float)s * 0.11f, hy - 0.44f * cosf(a), -0.02f + 0.44f * sinf(a)}};
    float a2 = a - kb;
    v3 anw = {{(float)s * 0.11f, knw.y - 0.43f * cosf(a2), knw.z + 0.43f * sinf(a2)}};
    if (anw.y < 0.08f) anw.y = 0.08f;
    // kneeling
    v3 knk = {{(float)s * 0.13f, 0.10f, 0.28f}}, ank = {{(float)s * 0.09f, 0.08f, -0.20f}};
    v3 kn = v3_lerp(knk, knw, st), an = v3_lerp(ank, anw, st), hp = hipw;
    v3 HP = hfp(&f, hp.x, hp.y, hp.z), KN = hfp(&f, kn.x, kn.y, kn.z), AN = hfp(&f, an.x, an.y, an.z);
    hfig_limb(HP, KN, 0.078f, PANTS[i], f.R);
    hfig_limb(KN, AN, 0.058f, PANTS[i], f.R);
    // the foot: flat behind a kneeler (sole up), forward under a walker
    v3 toe = st > 0.5f ? hfp(&f, an.x, fmaxf(an.y - 0.05f, 0.04f), an.z + 0.17f)
                       : hfp(&f, an.x, 0.05f, an.z - 0.16f);
    hfig_limb(AN, toe, 0.048f, shoe, f.Y);
  }
  // Torso, a little wider than deep.
  {
    v3 ax = v3_sub(neck, pel);
    float l = v3_len(ax);
    fig_ball(v3_lerp(pel, neck, 0.52f), 0.175f, l * 0.55f, 0.115f, ax, f.R, 12, SHIRT[i]);
    fig_ball(v3_add(pel, (v3){{0, 0.03f, 0}}), 0.165f, 0.11f, 0.12f, f.Y, f.R, 10, PANTS[i]);
  }
  hfig_limb(neck, head, 0.05f, skin, f.R);
  // Arms: tied behind the back while captive; swinging once free.
  for (int s = -1; s <= 1; s += 2) {
    float ph = g + (s > 0 ? F_PI : 0.0f);
    float b = 0.45f * m * sinf(ph);
    v3 sh = {{(float)s * 0.20f, hy + 0.54f + breath, -0.02f + lean}};
    v3 elw = {{(float)s * 0.22f, sh.y - 0.27f * cosf(b), sh.z + 0.27f * sinf(b)}};
    v3 wrw = {{(float)s * 0.21f, elw.y - 0.24f * cosf(b + 0.5f + 0.4f * m), elw.z + 0.24f * sinf(b + 0.5f + 0.4f * m)}};
    v3 elk = {{(float)s * 0.19f, hy + 0.32f, -0.12f}}, wrk = {{(float)s * 0.045f, hy + 0.17f, -0.17f}};
    v3 el = v3_lerp(elk, elw, st), wr = v3_lerp(wrk, wrw, st);
    v3 SH = hfp(&f, sh.x, sh.y, sh.z), EL = hfp(&f, el.x, el.y, el.z), WR = hfp(&f, wr.x, wr.y, wr.z);
    hfig_limb(SH, EL, 0.052f, SHIRT[i], f.F);
    hfig_limb(EL, WR, 0.043f, st > 0.5f ? skin : SHIRT[i], f.F);
    fig_ball(WR, 0.045f, 0.05f, 0.035f, v3_sub(WR, EL), f.F, 8, skin);
  }
  if (h->state == HS_CAPTIVE) {   // the cable tie at the wrists, the sack over the head
    scene_box(hfp(&f, 0.0f, hy + 0.17f, -0.18f), v3_scale(f.R, 0.075f), v3_scale(f.Y, 0.012f),
              v3_scale(f.F, 0.014f), rgb(0.05f, 0.05f, 0.05f));
    fig_ball(head, 0.112f, 0.13f, 0.118f, f.Y, f.R, 12, hood);
    fig_ball(hfp(&f, 0.0f, hy + 0.62f, lean * 1.2f + 0.02f), 0.085f, 0.03f, 0.08f, f.Y, f.R, 10,
             tint(hood, 0.8f, 0.85f));   // the drawstring gather
  } else {
    fig_ball(head, 0.092f, 0.112f, 0.102f, f.Y, f.R, 12, skin);
    fig_ball(v3_add(head, v3_add(v3_scale(f.Y, 0.035f), v3_scale(f.F, -0.02f))), 0.097f, 0.085f,
             0.10f, f.Y, f.R, 10, hair);
  }
}

static void rescue_scene(float alpha) {
  if (g_map_id != MAP_COMPOUND_ID) return;
  if (g_resc.on) {
    g_fig_k = 12;
    for (int i = 0; i < RESC_HOSTAGES; i++) hostage_draw(&g_resc.h[i], i, alpha);
  }
  // The helicopter's rotors (prop_heli is the airframe): two broad main
  // blades on the hub, drooping a little at rest and flat at speed, and the
  // two-bladed tail rotor on the fin's left.
  float a = g_resc.prev_rotor + (g_resc.rotor - g_resc.prev_rotor) * alpha;
  float spin = f_clamp(g_resc.rotor_w / 30.0f, 0.0f, 1.0f);
  mat_set(0.55f, 0.2f);
  v3 rc = rgb(0.09f, 0.10f, 0.09f), tipc = rgb(0.80f, 0.74f, 0.20f);
  v3 c = {{22.0f, 3.58f, -45.0f}};
  for (int k = 0; k < 2; k++) {
    float b = a + (float)k * F_PI;
    v3 d = {{cosf(b), 0, sinf(b)}}, q = {{-sinf(b), 0, cosf(b)}};
    float droop = 0.035f * (1.0f - spin);
    for (int s = 0; s < 6; s++) {            // six spans, each lower toward the tip
      float r0 = 0.35f + (float)s * 0.95f, r1 = r0 + 0.95f;
      v3 m = v3_add(c, v3_add(v3_scale(d, (r0 + r1) * 0.5f), (v3){{0, -droop * (r0 + r1) * 0.5f, 0}}));
      v3 ex = v3_add(v3_scale(d, 0.475f), (v3){{0, -droop * 0.475f, 0}});
      scene_box(m, ex, (v3){{0, 0.022f, 0}}, v3_scale(q, 0.27f), s == 5 ? tipc : rc);
    }
    scene_box(v3_add(c, v3_scale(d, 0.22f)), v3_scale(d, 0.22f), (v3){{0, 0.05f, 0}}, v3_scale(q, 0.09f),
              rgb(0.20f, 0.21f, 0.22f));   // the grip
  }
  // The flybar across the hub, ninety degrees round, as the type has.
  {
    float b = a + F_PI * 0.5f;
    v3 d = {{cosf(b), 0, sinf(b)}};
    scene_box(v3_add(c, (v3){{0, 0.10f, 0}}), v3_scale(d, 1.1f), (v3){{0, 0.012f, 0}},
              (v3){{-sinf(b) * 0.02f, 0, cosf(b) * 0.02f}}, rgb(0.12f, 0.12f, 0.12f));
  }
  v3 tc = {{22.0f - 10.72f, 3.42f, -45.0f - 0.24f}};
  float tb = a * 3.1f;
  for (int k = 0; k < 2; k++) {
    float t2 = tb + (float)k * F_PI;
    scene_box(v3_add(tc, (v3){{cosf(t2) * 0.62f, sinf(t2) * 0.62f, 0}}),
              (v3){{cosf(t2) * 0.62f, sinf(t2) * 0.62f, 0}}, (v3){{-sinf(t2) * 0.09f, cosf(t2) * 0.09f, 0}},
              (v3){{0, 0, 0.015f}}, rc);
  }
}

static void scene_build(const player_t *p, float alpha, int show_player) {
  (void)p;   // the roster below covers human 0; parameter kept for callers
  scene_reset();
  // Live grenades, tumbling. What flies is the body alone: spoon and pin are gone.
  for (int i = 0; i < MAX_NADES; i++)
    if (g_nades[i].fuse > 0) {
      const nade_t *n = &g_nades[i];
      v3 at = v3_lerp(n->prev_pos, n->pos, alpha);
      float a = n->prev_spin + (n->spin - n->prev_spin) * alpha;
      v3 gx = v3_rot_axis((v3){{1, 0, 0}}, n->axis, a), gy = v3_rot_axis((v3){{0, 1, 0}}, n->axis, a);
      v3 gz = v3_cross(gx, gy);
      // The model sits on the grip station; bring its body's centre onto `at`.
      v3 ofs = v3_add(v3_scale(gy, -0.0643f), v3_scale(gz, 0.0263f));
      mat_set(MAT_PARK_R, MAT_PARK_M);
      model_draw(GRENADE_V, GRENADE_P, GRENADE_NP, 0, v3_sub(at, ofs), gx, gy, gz);
    }
  for (int i = 0; i < MAX_SPOONS; i++)
    if (g_spoons[i].life > 0) {
      const spoon_t *q = &g_spoons[i];
      v3 at = v3_lerp(q->prev, q->pos, alpha);
      v3 gx = v3_rot_axis((v3){{1, 0, 0}}, q->axis, q->a), gy = v3_rot_axis((v3){{0, 1, 0}}, q->axis, q->a);
      v3 gz = v3_cross(gx, gy);
      v3 ofs = v3_add(v3_scale(gy, -0.0643f), v3_scale(gz, 0.0263f));
      model_draw(GRENADE_V, GRENADE_P, GRENADE_NP, 2, v3_sub(at, ofs), gx, gy, gz);
    }
  mat_set(0.92f, 0.0f);
  for (int i = 0; i < MAX_CHIPS; i++)
    if (g_chips[i].life > 0) {
      const chip_t *q = &g_chips[i];
      v3 at = v3_lerp(q->prev, q->pos, alpha);
      float fo = f_clamp((float)q->life / 60.0f, 0.0f, 1.0f);   // shrink away at the end
      float r = q->sz * fo;
      if (r < 0.001f) continue;
      v3 gx = v3_rot_axis((v3){{1, 0, 0}}, q->axis, q->a), gy = v3_rot_axis((v3){{0, 1, 0}}, q->axis, q->a);
      v3 gz = v3_cross(gx, gy);
      scene_box(at, v3_scale(gx, r * 1.3f), v3_scale(gy, r * 0.7f), v3_scale(gz, r), q->col);
    }
  for (int i = 0; i < MAX_WMAGS; i++)
    if (g_wmags[i].life > 0 && g_wmags[i].delay <= 0) {
      const wmag_t *q = &g_wmags[i];
      // The player's own, in first person, is the viewmodel's falling magazine
      // for its first 0.35 s — two copies of one object would be on screen.
      int age = 9 * TICK_HZ - q->life;
      if (q->src == g_local_ent && !show_player && age < (int)(0.35f * TICK_HZ)) continue;
      v3 at = v3_lerp(q->prev, q->pos, alpha);
      v3 gx, gy, gz;
      if (q->rest) {        // on its side: the broad face down, along its heading
        gx = (v3){{0, 1, 0}};
        gy = (v3){{cosf(q->head), 0, sinf(q->head)}};
      } else {
        gx = v3_rot_axis((v3){{1, 0, 0}}, q->axis, q->a);
        gy = v3_rot_axis((v3){{0, 1, 0}}, q->axis, q->a);
      }
      gz = v3_cross(gx, gy);
      v3 L = vm_mag_centre_v(q->wpn, q->var);
      v3 o = v3_sub(at, v3_add(v3_scale(gy, L.y), v3_scale(gz, L.z)));
      const mdl_ref_t *mg = gun_var_model(WPN_AR, q->var);
      if (q->wpn == WPN_SR) model_draw(SNIPER_V, SNIPER_P, SNIPER_NP, 1, o, gx, gy, gz);
      else                  model_draw(mg->v, mg->p, mg->np, 1, o, gx, gy, gz);
    }
  // Every human body (P1-58): the LOCAL one only when the camera is not
  // first-person; the other seven ALWAYS — unseen here they also cast no
  // shadow, because the depth pass draws this same list. Deliberately not
  // routed through scene_draw_bot (bot palette + its old hardwired AR).
  for (int h = 0; h < MAX_HUMANS; h++) {
    if (!g_humans_on[h]) continue;
    const player_t *hp = &g_players[h];
    int local = ent_of_human(h) == g_local_ent;
    if (local && !show_player) continue;
    if (!hp->alive) continue;
    g_fig_team = fig_team_of(ent_of_human(h));
    g_fig_ins = fig_ins_of(ent_of_human(h));
    g_fig_var = ent_of_human(h) & 3;
    g_fig_gear = local ? armory_idx(g_cfg.gear_theme, GEAR_THEME_COUNT) : -1;
    fig_own_pieces(local);
    g_fig_ent = ent_of_human(h);
    scene_draw_figure(&g_player_anim[h], v3_lerp(hp->prev_pos, hp->pos, alpha),
                      hp->yaw, NULL, alpha, ent_flash01(hp->flash, hp->flash_head),
                      hp->flash_head ? (v3){{1.0f, 0.22f, 0.14f}} : (v3){{1, 1, 1}},
                      1, hp->wp.cur, gun_anim(&hp->wp, alpha));
    g_fig_team = -1;
    g_fig_ins = 0;
    g_fig_var = 0;
    g_fig_gear = -1;
    fig_own_pieces(0);
    g_fig_ent = -1;
  }
  for (int i = 0; i < g_match.nbots; i++)
    if (g_bots[i].active) scene_draw_bot(&g_bots[i], alpha);
  // The bodies the entities left behind. They are NOT frozen — one handed over
  // mid-flight is still tumbling on the shared integrator — so they interpolate
  // like any other moving figure. The DISSOLVE threshold is batch state in the
  // same idiom as mat_set and ao_set, so the figure builder needs no new
  // argument for it.
  for (int c = 0; c < MAX_CORPSES; c++) {
    const corpse_t *cp = &g_corpses[c];
    if (!cp->used) continue;
    v3 cpos = v3_lerp(cp->prev_pos, cp->pos, alpha);
    death_t dth = rag_death(&cp->rag, cp->prev_slack, cp->slack, alpha);
    // FRONT-LOADED, because the burst is instantaneous and the geometry has to
    // leave WITH it. Linear, the body stood there almost intact for the first
    // third while the spray was already at full brightness — the fragments read
    // as something thrown AT a corpse rather than as the corpse itself going.
    // sqrt puts half the body away in the first quarter of the window; the tail
    // is what the eye reads as the last chunks tumbling out.
    g_fig_diss = cp->life < CORPSE_DISS
               ? sqrtf(1.0f - (float)cp->life / (float)CORPSE_DISS) : 0.0f;
    g_fig_diss_o = cpos;
    g_fig_diss_seed = cp->seed;
    g_fig_kcap = CORPSE_K;
    g_fig_var = cp->owner > 0 ? (cp->owner - 1) & 3 : 0;
    g_fig_team = fig_team_of(cp->owner);
    g_fig_ins = fig_ins_of(cp->owner);
    g_fig_gear = cp->owner == g_local_ent && cp->pal == 1
               ? armory_idx(g_cfg.gear_theme, GEAR_THEME_COUNT) : -1;
    fig_own_pieces(g_fig_gear >= 0);
    scene_draw_figure(&cp->anim, cpos, cp->yaw, &dth, alpha, 0.0f,
                      (v3){{1, 1, 1}}, cp->pal, cp->wpn, cp->gs);
    g_fig_var = 0;
    g_fig_team = -1;
    g_fig_ins = 0;
    g_fig_gear = -1;
    fig_own_pieces(0);
  }
  g_fig_diss = 0.0f;
  g_fig_kcap = 0;
  rescue_scene(alpha);
  for (int i = 0; i < MAX_MARKS; i++)
    if (g_marks[i].used) {
      // Rate 0 makes ao_at return the floor for every vertex, i.e. exactly the
      // host face's own interpolated occlusion, resolved once at spawn.
      ao_set(0.0f, 0.0f, g_marks[i].ao);
      scene_mark(&g_marks[i]);
    }
  ao_off();
}

