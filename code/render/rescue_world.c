// code/render/rescue_world.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// HOSTAGE RESCUE in the world: the hostages and the helicopter's rotor, which
// spins up as the evacuation starts.
//
// THE HOSTAGES ARE BUILT LIKE EVERY OTHER MAN IN THE GAME. They used to be a
// figure of their own — eleven soft ellipsoids — standing among operators and
// insurgents swept from the full figure builder: a toy beside the people
// sent to rescue it. Now each one is a pose_t on the shared skeleton's
// proportions (the operator's hip, chest, neck, head and shoulder stations,
// the same thigh/shin and upper/fore arm lengths, the knees and elbows solved
// by the same two-bone IK) handed to scene_draw_figure with the HOSTAGE kit
// (g_fig_hos): civilian shirt and trousers, bare hands and face, the bomb vest,
// and the sack over the head while captive. The pose is built here because no
// sim skeleton kneels: on his knees, sat up, hands cable-tied behind his back
// and head bowed while captive; standing and walking, hunched, once freed.
typedef struct { v3 o, R, Y, F; float topple; } hframe_t;
static v3 hfp(const hframe_t *f, float x, float y, float z) {
  if (f->topple > 0.0f) {   // the executed: fallen on their side
    float c = cosf(f->topple), s = sinf(f->topple);
    float x2 = x * c - y * s, y2 = x * s + y * c;
    x = x2; y = fmaxf(y2 + 0.16f, 0.06f);
  }
  return v3_add(f->o, v3_add(v3_scale(f->R, x), v3_add(v3_scale(f->Y, y), v3_scale(f->F, z))));
}
static v3 hfl(const hframe_t *f, v3 p) { return hfp(f, p.x, p.y, p.z); }
static v3 hfd(const hframe_t *f, v3 at, v3 d) {   // a direction at a point, through the topple
  v3 a = hfl(f, at), b = hfl(f, v3_add(at, v3_scale(d, 0.1f)));
  return v3_norm(v3_sub(b, a));
}
static v3 hv3(float x, float y, float z) { return (v3){{x, y, z}}; }
static void hostage_draw(const hostage_t *h, int i, float alpha) {
  v3 pos = v3_lerp(h->prev, h->pos, alpha);
  float yaw = h->prev_yaw + wrap_pi(h->yaw - h->prev_yaw) * alpha;
  float pose = h->prev_pose + (h->pose - h->prev_pose) * alpha;
  float m = h->prev_move + (h->move - h->prev_move) * alpha;
  float g = h->prev_gait + (h->gait - h->prev_gait) * alpha;
  float st = smooth01(pose);
  int captive = h->state == HS_CAPTIVE, dead = h->state == HS_DEAD;
  hframe_t f = {pos, {{cosf(yaw), 0, sinf(yaw)}}, {{0, 1, 0}}, {{sinf(yaw), 0, -cosf(yaw)}},
                dead ? 1.45f : 0.0f};
  float t = (float)g_tick * TICK_DT;
  float breath = 0.006f * sinf(t * 1.7f + (float)i);
  // ---- the trunk, in the figure's own frame (x right, y up, z forward) ----
  // Kneeling upright (hips 0.50 over the knees: thighs near vertical, the
  // weight forward of the heels), walking at the operator's 0.87 less a stoop.
  // THREE PEOPLE: the older man kneels sat back on his heels and stoops; the
  // young man kneels upright; the woman is slighter (narrower shoulders, a
  // finer arm) and kneels between the two.
  static const float KNEEL_HIP[3] = {0.44f, 0.50f, 0.47f}, STOOP[3] = {0.07f, 0.0f, 0.02f},
                     SHO_W[3] = {0.98f, 1.00f, 0.86f}, ARM[3] = {0.88f, 0.86f, 0.78f};
  int pv = i % 3;
  float hipy = KNEEL_HIP[pv] + st * (0.872f - 0.035f * m - KNEEL_HIP[pv]);
  float lean = (1.0f - st) * (0.10f + STOOP[pv] * 1.5f) + st * (0.07f + STOOP[pv] + 0.16f * m);
  v3 sp = hv3(0.0f, cosf(lean), sinf(lean));                     // the spine
  v3 pel = hv3(0.0f, hipy + 0.070f, -0.010f);
  v3 chest = v3_add(pel, v3_scale(sp, 0.400f + breath));
  v3 neck = v3_add(chest, v3_scale(sp, 0.100f));
  // the head bowed while captive (a man on his knees under a sack looks at the
  // floor), lifting as he is freed
  float bow = captive ? 0.42f + 0.10f * (float)(pv == 0) - 0.12f * (float)(pv == 1) + 0.05f * sinf(t * 0.35f + (float)i * 2.1f)
                      : 0.10f + 0.20f * m;
  v3 hfw = hv3(0.0f, -sinf(bow), cosf(bow));
  v3 head = v3_add(neck, v3_add(v3_scale(sp, 0.150f), v3_scale(hfw, 0.020f)));
  v3 hip[2], ank[2], toe[2], sho[2], wri[2], pole_k[2], pole_e[2];
  for (int s = 0; s < 2; s++) {
    float sd = s ? 1.0f : -1.0f;
    hip[s] = v3_add(pel, hv3(sd * 0.095f, -0.070f, 0.010f));
    // ---- legs: knees on the floor, shins flat behind; or a walk ----
    // kneeling ankle: sat back on the heels, the shins run shorter behind
    v3 ak = hv3(sd * 0.115f, 0.078f, -0.285f + (0.50f - KNEEL_HIP[pv]) * 1.2f);
    v3 tk = v3_add(ak, hv3(0.0f, -0.030f, -0.172f));             // instep down, toe back
    float ph = g + (s ? 0.0f : F_PI);
    float a = 0.50f * m * sinf(ph);                               // thigh swing
    float kb = 0.18f + 0.80f * m * fmaxf(0.0f, sinf(ph + 1.3f));  // knee bend
    v3 kw = v3_add(hip[s], hv3(sd * 0.020f, -ANIM_THIGH * cosf(a), ANIM_THIGH * sinf(a)));
    float a2 = a - kb;
    v3 aw = v3_add(kw, hv3(sd * 0.010f, -ANIM_SHIN * cosf(a2), ANIM_SHIN * sinf(a2)));
    if (aw.y < ANIM_ANKLE_Y) aw.y = ANIM_ANKLE_Y;
    v3 tw = v3_add(aw, hv3(sd * 0.020f, 0.0f, 0.172f));
    if (tw.y < ANIM_ANKLE_Y) tw.y = ANIM_ANKLE_Y;
    ank[s] = v3_lerp(ak, aw, st);
    toe[s] = v3_lerp(tk, tw, st);
    pole_k[s] = hv3(sd * 0.10f, 0.0f, 1.0f);
    // ---- arms: wrists crossed and tied at the small of the back; or hanging ----
    sho[s] = v3_add(chest, v3_add(v3_scale(sp, 0.050f), hv3(sd * ANIM_SHO_HALF * SHO_W[pv], 0.0f,
                    captive ? -0.030f : 0.0f)));
    v3 wk = v3_add(pel, v3_add(v3_scale(sp, 0.130f), hv3(sd * 0.030f, 0.0f, -0.205f)));
    float sw = 0.40f * m * sinf(ph + F_PI);
    v3 ww = v3_add(sho[s], hv3(sd * 0.040f, -0.540f * cosf(sw), 0.060f + 0.540f * sinf(sw) + 0.06f * m));
    float fr = captive ? 0.0f : st;   // cut loose: the hands come round as he stands
    wri[s] = v3_lerp(wk, ww, fr);
    pole_e[s] = v3_norm(v3_lerp(hv3(sd * 0.75f, -0.15f, -0.65f), hv3(sd * 0.35f, -0.20f, -0.90f), fr));
  }
  // ---- into the world ----
  pose_t P;
  memset(&P, 0, sizeof P);
  for (int s = 0; s < 2; s++) {
    P.hip[s] = hfl(&f, hip[s]);
    P.ankle[s] = hfl(&f, ank[s]);
    P.toe[s] = hfl(&f, toe[s]);
    P.knee[s] = ik_solve(P.hip[s], P.ankle[s], ANIM_THIGH, ANIM_SHIN, hfd(&f, hip[s], pole_k[s]));
    P.sho[s] = hfl(&f, sho[s]);
    P.wri[s] = hfl(&f, wri[s]);
    P.elb[s] = ik_solve(P.sho[s], P.wri[s], ANIM_UARM, ANIM_FARM, hfd(&f, sho[s], pole_e[s]));
    // a kneeler's feet lie instep-down: the boot's top faces the floor
    g_fig_bup[s] = st < 0.5f ? hfd(&f, ank[s], hv3(0.0f, -1.0f, 0.0f)) : (v3){{0, 0, 0}};
  }
  P.pelvis = hfl(&f, pel);
  P.chest = hfl(&f, chest);
  P.neck = hfl(&f, neck);
  P.head = hfl(&f, head);
  P.right = hfd(&f, chest, hv3(1, 0, 0));
  P.head_f = hfd(&f, head, hfw);
  P.gun_o = P.chest;   // no weapon; pose_gun_frame still wants a frame
  P.gun_f = hfd(&f, chest, hv3(0, 0, 1));
  P.gun_u = hfd(&f, chest, hv3(0, 1, 0));
  static anim_t A;
  memset(&A, 0, sizeof A);
  A.pose = P;
  A.prev_pose = P;
  gunanim_t gs;
  memset(&gs, 0, sizeof gs);
  g_fig_hos = i + 1;
  g_fig_hos_ars = ARM[pv];
  g_fig_hos_hood = captive;
  g_fig_hos_led = dead ? 0 : captive ? 1 : 2;   // freed: the squad cut the trigger lead
  g_fig_ent = -1;
  g_fig_team = -1;
  g_fig_ins = 0;
  g_fig_var = 0;
  g_fig_gear = -1;
  scene_draw_figure(&A, pos, yaw, NULL, 0.0f, 0.0f, (v3){{1, 1, 1}}, 0, WPN_AR, gs);
  g_fig_hos = 0;
  g_fig_hos_hood = 0;
  g_fig_hos_led = 0;
  g_fig_bup[0] = g_fig_bup[1] = (v3){{0, 0, 0}};
  if (captive) {   // the cable tie round both wrists
    v3 c = v3_lerp(P.wri[0], P.wri[1], 0.5f);
    v3 ax = v3_norm(v3_sub(P.wri[1], P.wri[0]));
    v3 up = hfd(&f, chest, sp);
    v3 bk = v3_norm(v3_cross(ax, up));
    mat_set(0.6f, 0.0f);
    scene_box(c, v3_scale(ax, 0.050f), v3_scale(up, 0.012f), v3_scale(bk, 0.034f),
              rgb(0.05f, 0.05f, 0.05f));
  }
}

// A helicopter's rotors over the PROP_HELI airframe parked at (hx, hz), nose
// +x: blade angle a, spin 0 (at rest, drooping) .. 1 (at flight speed).
static void heli_rotors(float hx, float hz, float a, float spin) {
  // The helicopter's rotors (prop_heli is the airframe): two broad main
  // blades on the hub, drooping a little at rest and flat at speed, and the
  // two-bladed tail rotor on the fin's left.
  mat_set(0.55f, 0.2f);
  v3 rc = rgb(0.09f, 0.10f, 0.09f), tipc = rgb(0.80f, 0.74f, 0.20f);
  v3 c = {{hx, 3.58f, hz}};
  for (int k = 0; k < 2; k++) {
    float b = a + (float)k * F_PI;
    v3 d = {{cosf(b), 0, sinf(b)}}, q = {{-sinf(b), 0, cosf(b)}};
    float droop = 0.035f * (1.0f - spin);
    for (int s = 0; s < 6; s++) {            // six spans, each lower toward the tip
      float r0 = 0.35f + (float)s * 0.95f, r1 = r0 + 0.95f, rm = (r0 + r1) * 0.5f;
      float chord = 0.29f - 0.012f * (float)s, tw = 0.06f - 0.012f * (float)s;   // taper and twist
      v3 m = v3_add(c, v3_add(v3_scale(d, rm), (v3){{0, -droop * rm, 0}}));
      v3 ex = v3_add(v3_scale(d, 0.475f), (v3){{0, -droop * 0.475f, 0}});
      v3 qa = v3_add(v3_scale(q, chord * 0.62f), (v3){{0, tw * chord, 0}});
      // the main blade body, and its darker, thicker leading edge
      scene_box(v3_sub(m, v3_scale(q, chord * 0.18f)), ex, (v3){{0, 0.016f, 0}}, qa, s == 5 ? tipc : rc);
      scene_box(v3_add(m, v3_scale(q, chord * 0.62f)), ex, (v3){{0, 0.024f, 0}},
                v3_scale(q, chord * 0.22f), s == 5 ? tipc : v3_scale(rc, 0.7f));
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
  v3 tc = {{hx - 10.72f, 3.42f, hz - 0.24f}};
  float tb = a * 3.1f;
  for (int k = 0; k < 2; k++) {
    float t2 = tb + (float)k * F_PI;
    scene_box(v3_add(tc, (v3){{cosf(t2) * 0.62f, sinf(t2) * 0.62f, 0}}),
              (v3){{cosf(t2) * 0.62f, sinf(t2) * 0.62f, 0}}, (v3){{-sinf(t2) * 0.09f, cosf(t2) * 0.09f, 0}},
              (v3){{0, 0, 0.015f}}, rc);
  }
}

static void rescue_scene(float alpha) {
  if (g_map_id == MAP_COMPOUND_ID && g_resc.on) {
    g_fig_k = 12;
    for (int i = 0; i < RESC_HOSTAGES; i++) hostage_draw(&g_resc.h[i], i, alpha);
  }
  // The helicopters' rotors (prop_heli is the airframe): the Compound's LZ
  // bird turns with the mission, every other parked one stands at rest.
  // (0.8.9: the Outpost's helipad parked one with no rotor at all.)
  for (int i = 0; i < g_nprops; i++) {
    const prop_t *pr = &g_props[i];
    if (pr->kind != PROP_HELI) continue;
    int lz = g_map_id == MAP_COMPOUND_ID && fabsf(pr->x - 22.0f) < 0.1f && fabsf(pr->z + 45.0f) < 0.1f;
    float a = lz ? g_resc.prev_rotor + (g_resc.rotor - g_resc.prev_rotor) * alpha : 0.6f + pr->x * 0.37f;
    float spin = lz ? f_clamp(g_resc.rotor_w / 30.0f, 0.0f, 1.0f) : 0.0f;
    heli_rotors(pr->x, pr->z, a, spin);
  }
}

// THE SUPPLY STATIONS, drawn live: the furniture always, what it holds only
// while it is stocked, and the status lamp on the sign — lit (green for
// health, amber for grenades, breathing) when stocked, a dim red while it
// restocks.
static void supply_draw(void) {
  static const v3 FQ[4] = {{{1, 0, 0}}, {{0, 0, 1}}, {{-1, 0, 0}}, {{0, 0, -1}}};
  for (int i = 0; i < g_nsupply; i++) {
    const supply_t *s = &g_supply[i];
    v3 F = FQ[s->qt & 3], L = FQ[(s->qt + 1) & 3], o = {{s->x, 0.0f, s->z}};
    int ready = g_tick >= g_supply_ready[i], med = s->kind == SUP_HEALTH;
    v3 w = {{1, 1, 1}};
    pmesh_draw(med ? PM_SUP_MED : PM_SUP_AMMO, o, F, L, w, w);
    if (ready) pmesh_draw(med ? PM_SUP_MED_I : PM_SUP_AMMO_I, o, F, L, w, w);
    // the lamp, on its housing atop the sign
    float la = med ? -0.215f : -0.20f, lb = med ? 0.0f : -0.68f, ly = med ? 2.245f : 2.165f;
    v3 lp = v3_add(o, v3_add(v3_scale(F, la), v3_add(v3_scale(L, lb), (v3){{0, ly, 0}})));
    float br = ready ? 0.80f + 0.20f * sinf(g_ui_time * 3.0f + (float)i) : 0.30f;
    v3 c = ready ? (med ? (v3){{0.25f * br, 1.6f * br, 0.55f * br}} : (v3){{1.6f * br, 1.05f * br, 0.15f * br}})
                 : (v3){{0.9f * br, 0.06f, 0.04f}};
    int ssm = g_scene_smat;
    g_scene_smat = SMAT_LED;
    scene_box(lp, (v3){{0.035f, 0, 0}}, (v3){{0, 0.025f, 0}}, (v3){{0, 0, 0.035f}}, c);
    g_scene_smat = ssm;
  }
}

static void scene_build(const player_t *p, float alpha, int show_player) {
  (void)p;   // the roster below covers human 0; parameter kept for callers
  scene_reset();
  supply_draw();
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
      mdl_ref_t nm = nade_mdl(n->kind);
      model_draw(nm.v, nm.p, nm.np, 0, v3_sub(at, ofs), gx, gy, gz);
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
      const mdl_ref_t *mg = gun_var_model(q->wpn == WPN_SR ? WPN_SR : WPN_AR, q->var);
      model_draw(mg->v, mg->p, mg->np, 1, o, gx, gy, gz);
    }
  // Every human body (P1-58): the LOCAL one only when the camera is not
  // first-person; the other seven ALWAYS — unseen here they also cast no
  // shadow, because the depth pass draws this same list. Deliberately not
  // routed through scene_draw_bot (bot palette + its old hardwired AR).
  // Count the figures this pass will draw (an upper bound: a bot that turns
  // out not to draw only leaves its share unspent), for the vertex-pressure
  // tier in scene_draw_figure.
  g_fig_todo = 0;
  for (int h = 0; h < MAX_HUMANS; h++)
    g_fig_todo += g_humans_on[h] && g_players[h].alive;
  for (int i = 0; i < g_match.nbots; i++) g_fig_todo += g_bots[i].active;
  for (int c = 0; c < MAX_CORPSES; c++) g_fig_todo += g_corpses[c].used;
  for (int h = 0; h < MAX_HUMANS; h++) {
    if (!g_humans_on[h]) continue;
    const player_t *hp = &g_players[h];
    int local = ent_of_human(h) == g_local_ent;
    if (local && !show_player) continue;
    if (!hp->alive) continue;
    g_fig_team = fig_team_of(ent_of_human(h));
    g_fig_ins = fig_ins_of(ent_of_human(h));
    g_fig_var = ent_kind(ent_of_human(h));
    g_fig_gear = local ? armory_idx(g_cfg.gear_theme, GEAR_THEME_COUNT) : -1;
    g_tp_theme = local ? (armory_theme()->pat > 0.0f ? armory_theme() : NULL) : NULL;
    fig_own_pieces(local);
    g_fig_ent = ent_of_human(h);
    g_fig_nade_kind = hp->wp.gr_kind;
    scene_draw_figure(&g_player_anim[h], v3_lerp(hp->prev_pos, hp->pos, alpha),
                      hp->yaw, NULL, alpha, ent_flash01(hp->flash, hp->flash_head),
                      hp->flash_head ? (v3){{1.0f, 0.22f, 0.14f}} : (v3){{1, 1, 1}},
                      1, hp->wp.cur, gun_anim(&hp->wp, alpha));
    g_fig_team = -1;
    g_fig_ins = 0;
    g_fig_var = 0;
    g_tp_theme = NULL;
    g_fig_gear = -1;
    fig_own_pieces(0);
    g_fig_ent = -1;
    g_fig_nade_kind = 0;
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
    g_fig_var = ent_kind(cp->owner);
    g_fig_team = fig_team_of(cp->owner);
    g_fig_ins = fig_ins_of(cp->owner);
    g_fig_gear = cp->owner == g_local_ent && cp->pal == 1
               ? armory_idx(g_cfg.gear_theme, GEAR_THEME_COUNT) : -1;
    fig_own_pieces(g_fig_gear >= 0);
    g_tp_theme = g_fig_gear >= 0 ? (armory_theme()->pat > 0.0f ? armory_theme() : NULL) : NULL;
    scene_draw_figure(&cp->anim, cpos, cp->yaw, &dth, alpha, 0.0f,
                      (v3){{1, 1, 1}}, cp->pal, cp->wpn, cp->gs);
    g_fig_var = 0;
    g_fig_team = -1;
    g_fig_ins = 0;
    g_tp_theme = NULL;
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

