// code/render/frame.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
static void render_frame(const player_t *p, float alpha, int fps,
                         const cam_override_t *ov) {
  atm_upload();
  if (g_world_dirty) { world_upload(); g_world_dirty = 0; }
  msaa_update();
  scene_update();
  R.frame_no++;
  R.vm_drawn = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, scene_target());
  v3 feet = v3_lerp(p->prev_pos, p->pos, alpha);
  float eye_h = p->prev_eye + (p->eye - p->prev_eye) * alpha
              - (p->prev_land_dip + (p->land_dip - p->prev_land_dip) * alpha);
  v3 eye = {{feet.x, feet.y + eye_h, feet.z}};
  // Lean: the eye travels out and sinks, and the view rolls by half the body's
  // bend. Interpolated like every other camera term, and taken off the SIM yaw
  // (not the recoil-offset one) so the rendered eye is the same point the round
  // left from. The roll is suppressed for the shoulder and debug cameras — those
  // watch the body do the leaning instead of doing it themselves.
  float leanv = p->prev_lean + (p->lean - p->prev_lean) * alpha;
  eye = v3_add(eye, lean_offset(p->yaw, leanv));
  eye.y -= lean_eye_drop(leanv);
  // Recoil offset + visual punch land on the rendered view; the sim's
  // yaw/pitch stay the player's own aim so compensation works.
  const weapon_t *w = &p->wp;
  float rec_p = w->prev_rec_p + (w->rec_p - w->prev_rec_p) * alpha
              + w->prev_punch + (w->punch - w->prev_punch) * alpha;
  float rec_y = w->prev_rec_y + (w->rec_y - w->prev_rec_y) * alpha;
  float ads = w->prev_ads_s + (w->ads_s - w->prev_ads_s) * alpha;
  float yaw = p->yaw + rec_y * DEG2RAD;
  float pitch = f_clamp(p->pitch + rec_p * DEG2RAD, -PITCH_MAX, PITCH_MAX);
  // The boot screen owns the camera while it is up. It goes through the SAME
  // override the harness uses rather than beside it, so `cam_free` and every
  // decision that hangs off it — the own-body draw, the suppressed crosshair,
  // the suppressed view roll — follow for free instead of needing a second term.
  cam_override_t home_ov;
  if (home_camera(&home_ov, p, alpha,
                  ov && (ov->active || ov->bot || ov->orbit)))
    ov = &home_ov;
  int cam_free = ov && (ov->active || ov->bot || ov->orbit);
  int third = g_cfg.third_person && !cam_free;   // shoulder cam, body visible
  // Lean rolls the picture; so does a shot. The punch roll is added here and
  // NOWHERE else, because a roll about the view axis leaves the crosshair pixel
  // exactly where it was — it is the only recoil channel that costs nothing
  // against the point-of-aim contract, which is why it is the one axis a shot
  // is allowed to move freely.
  float proll = w->prev_punch_roll + (w->punch_roll - w->prev_punch_roll) * alpha;
  float roll = (third || cam_free) ? 0.0f
             : leanv * LEAN_ANGLE * LEAN_VIEW_K + proll * DEG2RAD
               + p->vm.prev_cam_roll + (p->vm.cam_roll - p->vm.prev_cam_roll) * alpha;
  // The shock front through the camera: a hard jolt that rings out fast.
  if (g_shake > 0.002f && !cam_free) {
    float st = (float)g_tick * TICK_DT;
    float sa = g_shake * g_shake * 0.055f;
    pitch += sinf(st * 67.0f) * sa;
    yaw += sinf(st * 53.0f + 1.3f) * sa;
    roll += sinf(st * 41.0f + 0.4f) * sa * 0.7f;
  }
  float tp_dist = 2.7f;  // final camera pull-back; short = body would clip
  if (third) {
    // Shoulder camera: pivot slightly right of and above the head, pulled
    // back along the view; any wall behind pushes the camera in so it never
    // clips into geometry. The shoulder offset itself is collision-cast too
    // (hugging a wall on the right must not put the pivot INSIDE it — a ray
    // starting inside a solid reports no hit and the camera would tunnel).
    // Aim direction is untouched — shots still leave the player's eye
    // exactly where the sim says.
    v3 fwd = {{cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw)}};
    v3 rgt = v3_norm(v3_cross(fwd, (v3){{0, 1, 0}}));
    v3 side = v3_add(v3_scale(rgt, 0.34f), (v3){{0, 0.10f, 0}});
    float slen = v3_len(side);
    v3 sdir = v3_scale(side, 1.0f / slen);
    for (int i = 0; i < g_num_solids; i++) {
      v3 n;
      float t = ray_aabb(eye, sdir, g_solids[i].min, g_solids[i].max, &n);
      if (t > 0 && t - 0.06f < slen) slen = t - 0.06f;
    }
    v3 pivot = v3_add(eye, v3_scale(sdir, fmaxf(slen, 0.0f)));
    v3 back = v3_scale(fwd, -1.0f);
    for (int i = 0; i < g_num_solids; i++) {
      v3 n;
      float t = ray_aabb(pivot, back, g_solids[i].min, g_solids[i].max, &n);
      if (t > 0 && t - 0.12f < tp_dist) tp_dist = t - 0.12f;
    }
    tp_dist = f_clamp(tp_dist, 0.30f, 2.7f);
    eye = v3_add(pivot, v3_scale(back, tp_dist));
    if (eye.y < 0.10f) eye.y = 0.10f;
  }
  // THE SPECTATOR CHASE CAM (LAST MAN STANDING, while out of the round): over
  // the followed combatant's shoulder along their own yaw, pulled in by any
  // wall behind them exactly as the shoulder cam above is.
  int spec = cam_free ? -1 : spec_target();
  if (spec >= 0) {
    int sh = human_of_ent(spec);
    v3 sp = sh >= 0 ? v3_lerp(g_players[sh].prev_pos, g_players[sh].pos, alpha)
                    : v3_lerp(g_bots[spec - 1].prev_pos, g_bots[spec - 1].pos, alpha);
    yaw = ent_yaw(spec);
    pitch = -0.20f;
    roll = 0.0f;
    v3 fwd = {{cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw)}};
    v3 back = v3_scale(fwd, -1.0f);
    v3 pivot = v3_add(sp, (v3){{0, 1.85f, 0}});
    float sd = 3.4f;
    for (int i = 0; i < g_num_solids; i++) {
      v3 n;
      float t = ray_aabb(pivot, back, g_solids[i].min, g_solids[i].max, &n);
      if (t > 0 && t - 0.15f < sd) sd = t - 0.15f;
    }
    eye = v3_add(pivot, v3_scale(back, f_clamp(sd, 0.4f, 3.4f)));
    if (eye.y < 0.10f) eye.y = 0.10f;
  }
  if (ov && ov->orbit) {
    eye = ov->eye; yaw = ov->yaw; pitch = ov->pitch;
  } else if (ov && ov->bot) {
    v3 bp = v3_lerp(g_bots[0].prev_pos, g_bots[0].pos, alpha);
    eye = v3_add(bp, ov->boff);
    v3 d = v3_add(v3_add(bp, (v3){{0, ov->blook > 0 ? ov->blook : 0.9f, 0}}),
                  v3_scale(eye, -1.0f));
    yaw = atan2f(d.x, -d.z);
    pitch = f_clamp(atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z)), -PITCH_MAX, PITCH_MAX);
  } else if (ov && ov->active) {
    eye = ov->eye; yaw = ov->yaw; pitch = ov->pitch;
  }

  float aspect = (float)R.width / (float)R.height;
  // Slide FOV kick, interpolated like the camera for smooth surges; ADS
  // narrows the whole thing by the ZOOM factor.
  float kick = p->prev_fov_kick + (p->fov_kick - p->prev_fov_kick) * alpha;
  float fov_deg = f_clamp(g_cfg.fov + kick * g_cfg.mv[MV_FOV_KICK], 40.0f, 150.0f);
  fov_deg *= 1.0f - (1.0f - g_cfg.wp[p->wp.cur][WP_ADS_ZOOM] * 0.01f) * ads;
  // FOV IMPULSE on a shot: the kick spring's own displacement, so the frame
  // breathes out by ~1.5% at the peak of each recoil and settles with the gun.
  if (!cam_free && !third) {
    float vk = p->vm.prev_kick + (p->vm.kick - p->vm.prev_kick) * alpha;
    fov_deg *= 1.0f + f_clamp(vk, 0.0f, 0.02f) * 0.9f;
  }
  float fovy = 2.0f * atanf(tanf(fov_deg * (F_PI / 180.0f) * 0.5f) / aspect);
  // ZNEAR is the single biggest lever on depth precision: resolution goes as
  // z^2*(f-n)/(n*f*2^24), so it is set by 1/n. At 0.05 the 24-bit buffer resolves
  // only 1.07 mm at 30 m, which means any pair of surfaces closer than that
  // flickers — and a figure is full of sub-millimetre nesting (a sight frame, a
  // rail tooth, a pad over a limb). 0.12 m is as close as anything can get: the
  // player's collision radius holds walls 0.35 m off, the third-person camera
  // keeps a 0.12 m margin, and the viewmodel is a separate pass with its own
  // near plane. Pulling the far plane in from 200 to 140 (the arena diagonal is
  // 68 m) is a small extra gain.
  mat4 vp = m4_mul(m4_perspective(fovy, aspect, ZNEAR, ZFAR),
                   m4_look(eye, yaw, pitch, roll));
  R.cur_vp = vp;
  R.inv_vp = m4_inverse(vp);
  {  // A CUT is not motion: a respawn, a map change, a harness camera jump.
    // Anything faster than a hard flick (≈40 deg or 1.5 m in one frame) drops
    // the reprojection instead of smearing the first frame of the new view.
    static v3 pe, pf;
    v3 cf = {{cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw)}};
    if (v3_len(v3_sub(eye, pe)) > 1.5f || v3_dot(cf, pf) < 0.77f) R.have_prev = 0;
    pe = eye; pf = cf;
  }
  R.tan_y = tanf(fovy * 0.5f);
  R.tan_x = R.tan_y * aspect;
  R.ads_now = (!cam_free && !third) ? ads : 0.0f;
  {  // The weapon's screen motion over this frame, from its springs' last tick
    // (angles straight, positions over the ~0.3 m the receiver sits at).
    const vm_t *v = &p->vm;
    float tk = f_clamp(g_ui_dt * (float)TICK_HZ, 0.0f, 3.0f);
    float ty = tanf(VM_FOV_DEG * DEG2RAD * 0.5f);
    R.vm_vel[0] = ((v->sway_y - v->prev_sway_y) + (v->lag_x - v->prev_lag_x) / 0.3f
                   + (v->twist - v->prev_twist) * 0.3f) * tk / (2.0f * ty * aspect);
    R.vm_vel[1] = ((v->sway_p - v->prev_sway_p) + (v->lag_y - v->prev_lag_y) / 0.3f
                   + (v->kick - v->prev_kick) * 3.0f) * tk / (2.0f * ty);
  }
  cam_basis(yaw, pitch, roll, &R.cam_fwd, &R.cam_rgt, &R.cam_up);

  // Everything that casts is built BEFORE anything is drawn: the shadow pass
  // draws the same two VBOs the colour passes do, so the caster list cannot
  // drift from the visible geometry. (fx_build below reuses the scene buffer for
  // glow, which is why the solid figures have to be in it first.)
  // Hide the body once the camera is squeezed close (back to a wall): the near
  // plane would slice it open — culled backfaces read as transparency.
  g_cam_eye = eye;
  g_cam_frustum = frustum_of(&vp);
  g_cam_cull = 1;
  scene_build(p, alpha, cam_free || (third && tp_dist > 0.95f));
  g_cam_cull = 0;

  // Sun shadows. One ortho map over the whole arena, centred on it, far enough
  // back along the sun that nothing in the level is behind the near plane.
  int shadow_on = R.sm_fbo != 0;
  if (shadow_on) {
    v3 ld = v3_scale(atm_sun(), -1.0f);
    // A MAP BIGGER THAN THE SHADOW FOLLOWS THE CAMERA. The arena fits inside
    // one map centred on the origin; a large map does not, so the map is
    // centred on the eye instead, snapped to whole texels along the light's
    // own axes so standing still and turning never crawls the shadow edges.
    // Past it the border colour reads as lit, and the fog has taken over.
    v3 sc = {{0, 1.6f, 0}};
    if (ARENA_HALF > ARENA_HALF_DEF) {
      v3 rt = v3_norm(v3_cross(ld, fabsf(ld.y) > 0.99f ? (v3){{1, 0, 0}} : (v3){{0, 1, 0}}));
      v3 up = v3_cross(rt, ld);
      float tx = 2.0f * SHADOW_HALF / SHADOW_TEX;
      v3 c = {{eye.x, 1.6f, eye.z}};
      float cr = v3_dot(c, rt), cu = v3_dot(c, up);
      sc = v3_add(c, v3_add(v3_scale(rt, floorf(cr / tx) * tx - cr),
                            v3_scale(up, floorf(cu / tx) * tx - cu)));
    }
    v3 lc = v3_add(sc, v3_scale(ld, -SHADOW_DEPTH * 0.5f));
    R.light_vp = m4_mul(m4_ortho(SHADOW_HALF, SHADOW_HALF, 0.5f, SHADOW_DEPTH),
                        m4_look_dir(lc, ld));
    glBindFramebuffer(GL_FRAMEBUFFER, R.sm_fbo);
    glViewport(0, 0, SHADOW_TEX, SHADOW_TEX);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // Front faces cast: with back faces the ground's own shadow floats a texel
    // above it. The figures are closed manifolds, so either choice is watertight
    // for them, and front-facing is what keeps a wall's shadow at its base.
    glEnable(GL_CULL_FACE);
    glUseProgram(R.prog_depth);
    glUniformMatrix4fv(R.u_dvp, 1, GL_FALSE, R.light_vp.m);
    glBindVertexArray(R.vao_world);
    glDrawArrays(GL_TRIANGLES, 0, R.world_cast_verts);
    scene_draw();   // the same buffer, same guard — the casters ARE the figures
    rt_bind(scene_target(), R.width, R.height);
    glBindTexture(GL_TEXTURE_2D, R.sm_tex);   // stays bound for every lit pass
    // ...and on unit 1 through the no-compare sampler, for the PCSS blocker
    // search. Unit 1 is otherwise only touched by the post chain, after the
    // last lit pass, which always hands it back empty.
    gl_active_texture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, R.sm_tex);
    glBindSampler(1, R.sm_raw);
    gl_active_texture(GL_TEXTURE0);
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glUseProgram(R.prog_world);
  glUniformMatrix4fv(R.u_vp, 1, GL_FALSE, vp.m);
  glUniform3f(R.u_eye, eye.x, eye.y, eye.z);
  glUniform1f(R.u_detail, 1.0f);  // static world gets the surface detail
  // The floor's treatment. ONE place, because a uniform survives a program
  // rebind: the scope pass and the scene pass both inherit it, and a second
  // copy at either of them is a second copy that can disagree.
  { v3 sp = surf_params();
    glUniform4f(R.u_surf, sp.x, sp.y, sp.z, g_wind_ang); }
  // Yard paint: the open arenas that are working yards (ARENA, FOUNDRY), on
  // hard floors only — the shader also requires a non-sand treatment.
  glUniform2f(uloc(R.prog_world, "uYard"), ARENA_HALF,
              (g_map_id == 0 || g_map_id == 1) && g_theme.surf != SURF_SAND ? 1.0f : 0.0f);
  // The town the weathering belongs to (CITADEL/HARBOR/REFINERY are
  // MAP_DEFS 4..6), -1 on the arenas.
  glUniform1i(uloc(R.prog_world, "uTown"), g_map_id >= 4 && g_map_id <= 6 ? g_map_id - 4
                                           : g_map_id == MAP_COMPOUND_ID ? 0 : -1);
  {
    // THE LANTERNS COME ON AS THE SUN GOES DOWN: off above ~24 degrees, full
    // below ~12. The eight nearest to the eye light the world; the rest
    // still glow (their glass is SMAT_LAMP), they just do not throw light.
    float on = f_clamp((0.42f - atm_sun().y) / 0.20f, 0.0f, 1.0f);
    float L[8][4] = {{0}};
    float bd[8];
    for (int k = 0; k < 8; k++) bd[k] = 1e9f;
    for (int i = 0; i < g_nprops; i++) {
      if (g_props[i].kind != PROP_LAMP) continue;
      float dx = g_props[i].x - eye.x, dz = g_props[i].z - eye.z, dd = dx * dx + dz * dz;
      int worst = 0;
      for (int k = 1; k < 8; k++) if (bd[k] > bd[worst]) worst = k;
      if (dd < bd[worst]) {
        bd[worst] = dd;
        L[worst][0] = g_props[i].x; L[worst][1] = 3.80f; L[worst][2] = g_props[i].z;
        L[worst][3] = 6.5f * on;
      }
    }
    glUniform1f(uloc(R.prog_world, "uLampOn"), on);
    glUniform4fv(uloc(R.prog_world, "uLamp"), 8, &L[0][0]);
  }
  glUniform1f(R.u_emis, 0.0f);
  glUniform1i(R.u_sm, 0);
  glUniform1i(uloc(R.prog_world, "uSmRaw"), 1);
  glUniform1f(uloc(R.prog_world, "uSmPen"), SHADOW_DEPTH * 0.016f / (2.0f * SHADOW_HALF));
  glUniform1f(R.u_shadow, shadow_on ? 1.0f : 0.0f);
  glUniform1f(R.u_vm, 0.0f);
  // The world's own muzzle light. It rides the SAME `fresh` latch the flash
  // star does rather than a tick count, for the reason that latch exists: at a
  // low fps_cap a tick-counted window is spent before a frame can show it.
  // Fed from the shooter's published world muzzle — the player's own, because
  // this is the light in the player's eyes and twenty of them would be a strobe.
  // Read the player's own flash straight out of the ring, HERE, rather than
  // publishing it from the FX pass: the FX pass runs after this one, so a
  // published value is a frame late — and the harness renders exactly one frame
  // per `shot`, which means a frame late is never at all. The flash ring is
  // filled at event-push time during the tick, so it is already correct.
  // 0 = no shot (the uniform's documented contract). This was initialised to
  // 11.0 — the light was on EVERY frame, at a stale position, 37% brighter
  // than the actual flash frame's 8.0, and firing made the world momentarily
  // DIMMER. Measured: a wall patch 0.5 m away read L=80 before any shot and
  // L=184 permanently after one, against 169 on the flash frame itself.
  g_world_muz_c = 0.0f;
  float blast_c = g_blast_light * 90.0f;
  for (int i = 0; i < MAX_FLASHES; i++)
    if (g_flashes[i].src == g_local_ent && g_tick - g_flashes[i].t0 <= 2 &&
        (g_flashes[i].life > 0 || g_flashes[i].fresh)) {
      g_world_muz_c = 8.0f;
      g_world_muz_p = g_flashes[i].pos;
    }
  if (blast_c > g_world_muz_c) {            // a detonation lights the arena
    g_world_muz_c = blast_c;
    g_world_muz_p = g_blast_p;
  }
  if (g_world_muz_c > 0.001f) {
    glUniform3f(R.u_muzc, 1.00f * g_world_muz_c, 0.70f * g_world_muz_c,
                0.36f * g_world_muz_c);
    glUniform3f(R.u_muzp, g_world_muz_p.x, g_world_muz_p.y, g_world_muz_p.z);
  } else {
    glUniform3f(R.u_muzc, 0.0f, 0.0f, 0.0f);
  }
  glUniform2f(R.u_smstep, 1.0f / SHADOW_TEX, 1.0f / SHADOW_TEX);
  glUniform1f(R.u_smtexel, 2.0f * SHADOW_HALF / SHADOW_TEX);
  glUniformMatrix4fv(R.u_lightvp, 1, GL_FALSE, R.light_vp.m);
  glBindVertexArray(R.vao_world);
  glDrawArrays(GL_TRIANGLES, 0, R.world_verts);

  sky_pass(yaw, pitch, roll, fovy, aspect);

  // Dynamic scene: figures and round impact marks (solid), then the additive
  // light pass — tracers and sparks are glow, not geometry: depth-tested but not
  // depth-written, blended ONE/ONE.
  scene_draw_vp(&vp);

  // Scope picture-in-picture, stage 1: the SOLID scene. This has to happen here,
  // while the scene buffer still holds the figures — `fx_build` below reuses that
  // same buffer for tracers, and drawing the scope after it put the muzzle glow
  // in the lens and left every enemy out of it. An empty scope with a live bot
  // standing in it is the worst possible bug in a sniper rifle.
  float scope = 0.0f;
  mat4 svp = vp;
  // ...AND ONLY ONCE THE DISC CAN ACTUALLY OPEN. The picture is gated on the
  // ramp at SCOPE_ADS_OPEN below (`rad` is identically zero under it), so every
  // frame of the first half of a raise rendered the world, the sky and every
  // figure a second time into a 768-square target that was then not drawn. That
  // is the most expensive frame in the sequence — the one where the weapon is
  // moving — paying double for nothing. Gated on the SAME number the disc uses,
  // so the rendered picture is unchanged by construction.
  int scope_on = !cam_free && !third && p->alive && R.scope_fbo &&
                 p->wp.cur == WPN_SR && ads > SCOPE_ADS_OPEN;
  if (scope_on) {
    scope = ads;
    float szoom = SCOPE_FOV_DEG * (F_PI / 180.0f);
    svp = m4_mul(m4_perspective(szoom, 1.0f, ZNEAR, ZFAR),
                 m4_look(eye, yaw, pitch, roll));
    rt_bind(R.scope_fbo, R.scope_size, R.scope_size);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(R.prog_world);
    glUniformMatrix4fv(R.u_vp, 1, GL_FALSE, svp.m);
    glUniform1f(R.u_detail, 1.0f);
    glBindVertexArray(R.vao_world);
    glDrawArrays(GL_TRIANGLES, 0, R.world_verts);
    sky_pass(yaw, pitch, roll, szoom, 1.0f);
    scene_draw_vp(&svp);              // the figures, still in the buffer
    rt_bind(scene_target(), R.width, R.height);
    glUniformMatrix4fv(R.u_vp, 1, GL_FALSE, vp.m);
  }

  // Re-anchor the player's own streak onto the viewmodel's published muzzle,
  // FOV-remapped: the vm pass renders at VM_FOV_DEG while the world renders at
  // fovy, so the world-space point that lands on the same screen pixel as the
  // drawn barrel tip has its lateral camera-space offsets scaled by the
  // tangent ratio. The published tip is one frame stale (vm_build runs after
  // fx_build, which must finish before the vm pass reuses the scene buffer) —
  // invisible across 66 ms of tracer life; the pose-anchored spawn point
  // covers the first frame of a session.
  g_fx_muz_live = 0;
  // The LOCAL alive status: this is one half of the flash-ownership handshake
  // with the world-star suppression above — with g_local_ent < 0 (dedicated
  // server) it must be hard 0, or the two halves disagree and a shot draws
  // zero or two stars.
  g_fx_vm_flash = g_local_ent >= 0 && !cam_free && !third && p->alive && !g_vmorb.on;
  if (!cam_free && !third && p->alive && g_vm_muz_cam.z > 0.05f &&
      g_tick - g_vm_muz_tick <= 8) {
    v3 mf, mr, mu;
    cam_basis(yaw, pitch, roll, &mf, &mr, &mu);
    float mk = tanf(fovy * 0.5f) / tanf(VM_FOV_DEG * DEG2RAD * 0.5f);
    g_fx_muzzle = v3_add(eye, v3_add(v3_scale(mr, g_vm_muz_cam.x * mk),
                  v3_add(v3_scale(mu, g_vm_muz_cam.y * mk),
                         v3_scale(mf, g_vm_muz_cam.z))));
    g_fx_muz_live = 1;
  }
  g_fx_alpha = alpha;
  g_fx_muzzle_vis = g_fx_muzzle;
  g_fx_muz_live_vis = g_fx_muz_live;
  // HEAT HAZE: where the drawn muzzle lands on screen, through the viewmodel's
  // own projection, and how hard the barrel is shimmering.
  g_heat_shimmer = 0.0f;
  if (g_fx_muz_live && g_vm_muz_cam.z > 0.05f) {
    float hk = f_clamp((g_wpn_heat - 0.45f) / 0.8f, 0.0f, 1.0f);
    float ty = tanf(VM_FOV_DEG * DEG2RAD * 0.5f);
    g_heat_scr.x = 0.5f + 0.5f * g_vm_muz_cam.x / (g_vm_muz_cam.z * ty * aspect);
    g_heat_scr.y = 0.5f + 0.5f * g_vm_muz_cam.y / (g_vm_muz_cam.z * ty);
    g_heat_scr.z = 0.075f;
    g_heat_shimmer = hk * hk * (1.0f - 0.5f * ads);
  }
  puffs_build(eye);
  if (g_scene_floats) {
    fx_pass_begin();
    glBlendFunc(GL_DST_COLOR, GL_ZERO);       // multiply: smoke darkens the frame
    glUniform1f(R.u_emis, 2.0f);              // ...a transmittance, passed raw
    scene_draw_vp(&vp);
    glUniform1f(R.u_emis, 1.0f);
    glBlendFunc(GL_ONE, GL_ONE);
    fx_pass_end();
  }
  fx_build(eye);
  if (g_scene_floats) {
    fx_pass_begin();
    scene_draw_vp(&vp);
    if (scope_on) {  // stage 2: the same glow, seen through the glass
      rt_bind(R.scope_fbo, R.scope_size, R.scope_size);
      glUniformMatrix4fv(R.u_vp, 1, GL_FALSE, svp.m);
      scene_draw_vp(&svp);
      rt_bind(scene_target(), R.width, R.height);
      glUniformMatrix4fv(R.u_vp, 1, GL_FALSE, vp.m);
    }
    fx_pass_end();
  }

  // Viewmodel pass: depth cleared so the gun never clips into walls, own
  // fixed-FOV projection so the world FOV slider never distorts it. The
  // muzzle flash is an additive glow star riding the barrel tip.
  // Third person: the body carries the rifle, no viewmodel. Dead: no hands, no
  // rifle — "YOU DIED" over a fully shouldered combat-ready weapon read as a
  // glitch, and the sinking camera already carries the death (the scope gate
  // above matches, or a scoped death leaves a magnified disc with no rifle).
  depth_copy(0);   // the world's depth, before the viewmodel clears it
  if ((!cam_free && !third && p->alive) || g_vmorb.on) {
    v3 fwd, rgt, upv;
    cam_basis(yaw, pitch, roll, &fwd, &rgt, &upv);
    glClear(GL_DEPTH_BUFFER_BIT);
    R.vm_drawn = !g_vmorb.on;
    mat4 vmvp = m4_mul(m4_perspective(VM_FOV_DEG * DEG2RAD, aspect, 0.01f, 5.0f),
                       m4_look(eye, yaw, pitch, roll));
    glUniformMatrix4fv(R.u_vp, 1, GL_FALSE, vmvp.m);
    // Hero rig, in world directions built from the camera basis: fill over the
    // left shoulder, rim from the lower right and behind.
    // MEASURED, and it is a lighting bug rather than an art choice: the same
    // leather rendered Y 0.0876 on the support hand and 0.0439 on the trigger
    // hand — exactly 1.00 stop apart, with 64.5% of the bottom-right quadrant
    // under Y 0.06. The fill sat hard over the off shoulder and the trigger side
    // got nothing but the rim. Swung most of the way back toward the view axis
    // and lifted; it still models the form from the left, it just no longer
    // leaves the shooter's own firing hand in a hole.
    v3 fl = v3_norm(v3_add(v3_scale(rgt, -0.26f),
                   v3_add(v3_scale(upv, 0.66f), v3_scale(fwd, -0.36f))));
    v3 rm = v3_norm(v3_add(v3_scale(rgt, 0.74f),
                   v3_add(v3_scale(upv, -0.10f), v3_scale(fwd, 0.54f))));
    glUniform1f(R.u_vm, 1.0f);
    glUniform3f(R.u_fill, fl.x, fl.y, fl.z);
    glUniform3f(R.u_rim, rm.x, rm.y, rm.z);
    // The frame the micro-structure is anchored in. vm_build below refreshes it
    // for THIS frame's pose, so the upload has to follow the build, not precede
    // it — see the block after the vmorbit re-aim.
    // The flash sits ahead of and slightly above the bore: one guaranteed
    // BRIGHT frame (the `fresh` render latch — tick counting dies under tick
    // batching, see flash_t), dim for the remaining flash ticks, warm, and
    // scaled down in ADS where the sight picture has to stay readable.
    int mfresh = g_muzzle_fresh && g_tick - g_muzzle_fire_tick <= 12 &&
                 !g_vmorb.on;
    if ((g_muzzle_t > 0 || mfresh) && !g_vmorb.on) {
      // The magnum's blast lights the hands harder than the rifle's; a pistol
      // round throws less fire than either.
      float wk = p->wp.cur == WPN_SR ? 1.35f : p->wp.cur == WPN_PT ? 0.80f : 1.0f;
      float k = (mfresh ? 3.4f : 1.3f) * (1.0f - 0.45f * ads) * wk;
      v3 ml = v3_norm(v3_add(fwd, v3_scale(upv, 0.16f)));
      glUniform3f(R.u_muzl, ml.x, ml.y, ml.z);
      glUniform3f(R.u_muzc, 1.00f * k, 0.72f * k, 0.38f * k);
    } else {
      glUniform3f(R.u_muzc, 0.0f, 0.0f, 0.0f);
    }
    vm_build(p, alpha, eye, rgt, upv, fwd);
    if (!g_vmorb.on) {
      // The tracer re-anchor publish — from the ONE vm_build call whose basis
      // is the real camera (the harness probes must stay side-effect-free).
      v3 mc = v3_sub(g_vm_muzzle, eye);
      g_vm_muz_cam = (v3){{v3_dot(mc, rgt), v3_dot(mc, upv), v3_dot(mc, fwd)}};
      g_vm_muz_tick = g_tick;
    }
    if (g_vmorb.on) {
      // Re-aim the projection at one hand, keeping the pose that was just built.
      // The hero rig is rebuilt on the ORBIT basis: this is a MODEL inspection
      // view, and a grip lit from behind the player reads as a dark mass from
      // in front of the muzzle whatever its shape. What the player's own light
      // does to the hands is what the first-person shots in the battery are for.
      v3 of, orr, ou;
      // ORBIT ANGLES ARE RELATIVE TO THE WEAPON when the boot screen is driving,
      // and that is not a convenience. The shot list says "from the left, ten
      // degrees up"; measured against WORLD north that sentence means a
      // different picture for every heading the player happens to face, and the
      // heading changes inside a shot the moment a swap or a shoulder rotates
      // the model. Referenced to the weapon's own bore, a shot means the same
      // picture forever. The harness turntable keeps world angles: it is
      // inspecting a pose, and the pose is what it is aimed at.
      // ...and RELATIVE IN PITCH TOO, not only in yaw. (The shot list's pitches
      // are therefore measured off the BORE, not off the horizon: the low ready
      // holds the weapon ~34 degrees muzzle-down, so every macro angle carries
      // that.) Following the weapon's
      // heading alone leaves a macro shot at the mercy of everything that
      // pitches or cants it — a reload arcs the muzzle down 26 degrees, and a
      // 22-degree lens 0.3 m from the barrel then loses its subject off the
      // corner of the frame. Built in the weapon's OWN basis the shot is
      // locked off: the weapon holds still and the world swings behind it,
      // which is what a product shot of a moving object looks like and is also
      // how the magazine change hides. Roll is deliberately NOT followed —
      // m4_look takes 0 there — because a horizon that rolls reads as a fault
      // rather than as the object turning.
      float oyaw, opit;
      if (g_vmorb.piv_on) {
        float cy2 = cosf(g_vmorb.yaw), sy2 = sinf(g_vmorb.yaw);
        float cp2 = cosf(g_vmorb.pit), sp2 = sinf(g_vmorb.pit);
        v3 dir = v3_add(v3_scale(g_vm_mdl_x, cp2 * sy2),
                 v3_add(v3_scale(g_vm_mdl_y, sp2),
                        v3_scale(g_vm_mdl_z, cp2 * cy2)));
        dir = v3_norm(dir);
        oyaw = atan2f(dir.x, -dir.z);
        opit = asinf(f_clamp(dir.y, -1.0f, 1.0f));
      } else {
        oyaw = g_vmorb.yaw;
        opit = g_vmorb.pit;
      }
      cam_basis(oyaw, opit, 0.0f, &of, &orr, &ou);
      // WHICH = 3 pivots on the TRIGGER, not on a wrist. The trigger group is
      // 5 cm forward of the trigger wrist and 2 cm below it, so at any distance
      // close enough to judge a fingertip a wrist-pivoted orbit puts the blade
      // off the edge of the frame — which is exactly how a finger ended up
      // passing through a guard nobody had ever seen from the side.
      v3 piv = g_vmorb.which == 3 ? g_gun_trig_p
             : g_vmorb.which < 2  ? g_vm_hand_p[g_vmorb.which]
                                  : v3_scale(v3_add(g_vm_hand_p[0],
                                                    g_vm_hand_p[1]), 0.5f);
      // A STATION ON THE WEAPON'S OWN AXIS, not a wrist. Pivoting on a hand and
      // changing the distance is a zoom; moving the pivot along the bore while
      // the camera holds its radius is a TRACK, and a track down the length of
      // the object is the shot that says what the object is. Lifted a hair off
      // the axis so the lens rides over the receiver rather than through it.
      if (g_vmorb.piv_on)
        piv = v3_add(g_vm_mdl_o, v3_add(v3_scale(g_vm_mdl_z, g_vmorb.bore),
                                        v3_scale(g_vm_mdl_y, g_vmorb.rise)));
      float ofov = g_vmorb.fov > 1.0f ? g_vmorb.fov : 30.0f;
      // `comp` is a FRACTION OF THE FRAME, not an angle, and it is turned into
      // one here because here is the only place that knows the lens and the
      // aspect. As an angle it was a constant 13 degrees on a 26-degree lens —
      // half the frame — and the subject left the picture entirely.
      float ohf = 2.0f * atanf(tanf(ofov * DEG2RAD * 0.5f) * aspect);
      mat4 ovp = m4_mul(m4_perspective(ofov * DEG2RAD, aspect, 0.01f, 5.0f),
                        m4_look(v3_sub(piv, v3_scale(of, g_vmorb.dist)),
                                oyaw - g_vmorb.comp * ohf, opit, 0.0f));
      glUniformMatrix4fv(R.u_vp, 1, GL_FALSE, ovp.m);
      v3 ofl = v3_norm(v3_add(v3_scale(orr, -0.52f),
                      v3_add(v3_scale(ou, 0.62f), v3_scale(of, -0.35f))));
      v3 orm = v3_norm(v3_add(v3_scale(orr, 0.70f),
                      v3_add(v3_scale(ou, -0.22f), v3_scale(of, 0.62f))));
      glUniform3f(R.u_fill, ofl.x, ofl.y, ofl.z);
      glUniform3f(R.u_rim, orm.x, orm.y, orm.z);
      // ...AND THE EYE, which the turntable was not moving. `uEye` is what every
      // view-dependent term in the shader is built on — the whole specular
      // lobe, the environment reflection, the Fresnel and the aerial
      // perspective — and it was still the PLAYER's eye while the projection
      // orbited, so the one view that exists to judge a material showed the
      // highlights of a viewpoint it was not looking from. Orbiting to the far
      // side of the weapon changed the silhouette and nothing about the
      // shading. The pose is deliberately not re-solved (that is the point of
      // the turntable); only the camera moves, and the eye IS the camera.
      v3 oeye = v3_sub(piv, v3_scale(of, g_vmorb.dist));
      glUniform3f(R.u_eye, oeye.x, oeye.y, oeye.z);
    }
    // The weapon's own frame, uploaded AFTER vm_build so it describes the pose
    // that was just emitted rather than the previous frame's.
    glUniform3f(uloc(R.prog_world, "uWrist"), (float)p->wp.ammo[p->wp.cur],
                (float)wdef(&p->wp, p->wp.cur)->mag, g_ui_time);
    {
      // The armory: the weapon theme's paint and the gear's LED colour, which
      // the wrist display shares with the gauntlet's inlays.
      const wpn_theme_t *th = vm_theme(p->wp.cur, wvar(&p->wp, p->wp.cur));
      const gear_theme_t *gt = &GEAR_THEMES[armory_idx(g_cfg.gear_theme, GEAR_THEME_COUNT)];
      glUniform4f(uloc(R.prog_world, "uSkin"), th->pat, th->scale, th->metal, 0.0f);
      glUniform3fv(uloc(R.prog_world, "uSkinA"), 1, th->a);
      glUniform3fv(uloc(R.prog_world, "uSkinB"), 1, th->b);
      glUniform3fv(uloc(R.prog_world, "uSkinC"), 1, th->c);
      glUniform3fv(uloc(R.prog_world, "uSkinD"), 1, th->d);
      // The display's "on" colour at the shipped amber is (1, .55, .12): the
      // LED's authored colour pushed redder the way the inlay shader does.
      if (gt == GEAR_THEMES) glUniform3f(uloc(R.prog_world, "uWristOn"), 1.0f, 0.55f, 0.12f);
      else glUniform3f(uloc(R.prog_world, "uWristOn"), gt->led[0], gt->led[1] * 0.887f,
                       gt->led[2] * 1.2f);
    }
    glUniform3f(R.u_mdlo, g_vm_mdl_o.x, g_vm_mdl_o.y, g_vm_mdl_o.z);
    glUniform3f(R.u_mdlx, g_vm_mdl_x.x, g_vm_mdl_x.y, g_vm_mdl_x.z);
    glUniform3f(R.u_mdlz, g_vm_mdl_z.x, g_vm_mdl_z.y, g_vm_mdl_z.z);
    scene_draw();
    glUniform1f(R.u_vm, 0.0f);
    // The world pass that follows (the scope FBO, the HUD's own program) must
    // not inherit the orbit's eye; the next world draw sets it from the camera.
    glUniform3f(R.u_eye, eye.x, eye.y, eye.z);
    glUniform3f(R.u_muzc, 0.0f, 0.0f, 0.0f);
    if ((g_muzzle_t > 0 || mfresh) && !g_vmorb.on) {
      // The same star the world pass gives every other shooter, on the drawn
      // barrel tip in the viewmodel's own projection. Petal clock hashes the
      // shot index, so a burst flickers shot to shot; ADS shrinks it hard —
      // the sight picture has to stay readable through sustained fire. Not
      // under vmorbit: fx_seg billboards toward the PLAYER's eye, and the
      // turntable projects through the orbit camera — the star came out as
      // edge-on slivers over the wrist in exactly the frames the tool exists
      // to produce.
      scene_reset();
      float mf01 = mfresh ? 1.0f : (float)g_muzzle_t / (float)flash_ticks(p->wp.cur);
      g_muzzle_fresh = 0;
      float fs = 1.0f - 0.55f * ads;
      fx_flash(g_vm_muzzle, fwd, rgt, upv, eye,
               0.11f * fs * (0.55f + 0.45f * mf01), mf01,
               (uint32_t)p->wp.idx * 61u + 17u, p->wp.cur);
      fx_pass_begin();
      scene_draw();
      fx_pass_end();
    }
  }

  // HUD + menu: everything batches into one buffer, one draw call.
  if (R.vm_drawn) depth_copy(1);   // ...and the weapon's own, for DoF and masking
  post_pass(g_cfg.post_fx && !g_vmorb.on);
  rt_bind(R.present_fbo, R.width, R.height);
  g_ui_time += g_ui_dt;   // presentation clock (pulses, glides); never sim-read
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(R.prog_hud);
  glUniform2f(R.u_screen, (float)R.width, (float)R.height);
  glUniform1i(R.u_tex, 0);
  glUniform1i(R.u_img, 0);
  glBindTexture(GL_TEXTURE_2D, R.font_tex);   // unit 0 carried the shadow map
  glBindVertexArray(R.vao_hud);

  // ONE expression, TWO readers: the block below and the ui_begin fallback
  // under it. Written twice, the boot screen took the HUD branch out and left
  // the fallback thinking the HUD had already opened the batch — so no
  // ui_begin ran at all, g_uis kept the previous frame's value and the whole
  // menu drew at half scale into the corner of the frame.
  int hud_on = g_hud_visible && !g_home;
  if (hud_on) {
  // ...and NOT on the boot screen. Ammo, health, the killfeed and the FPS
  // counter belong to a match in progress; over a main menu they are a
  // second interface competing with the one the player is actually using.
  // The scope disc: its own pass, because it samples a different texture in
  // image mode. Drawn as a fan so the rim is a real circle instead of an
  // alpha-tested square, and only the disc is magnified — the world outside it
  // was already drawn at the normal FOV, which is the whole point.
  if (scope > 0.02f) {
    ui_begin();
    float cx = g_scope_cx, cy = g_scope_cy;
    float vw_ = g_ui_vw, vh_ = g_ui_vh;
    // Radius comes from the geometry (vm_build projected the ocular). The picture
    // may only appear once the eye is actually BEHIND the glass: while the rifle
    // is still coming up the ocular is off to the side, and a disc of magnified
    // world drawn there is a circle hanging in space next to the gun — which is
    // exactly what "the scope isn't attached to the weapon" looks like. Gate it on
    // the lens being on the view axis (within a third of its own radius) as well
    // as on the ADS ramp, so it opens where the tube is and nowhere else.
    float off = sqrtf((cx - vw_ * 0.5f) * (cx - vw_ * 0.5f) +
                      (cy - vh_ * 0.5f) * (cy - vh_ * 0.5f));
    float aligned = 1.0f - f_clamp((off - g_scope_r * 0.30f) /
                                   fmaxf(1.0f, g_scope_r * 0.55f), 0.0f, 1.0f);
    float rad = g_scope_r *
                smooth01(f_clamp((scope - SCOPE_ADS_OPEN) / 0.42f, 0.0f, 1.0f)) *
                aligned;
    if (rad > 0.5f) {
      // Glass TAKES light: coated optics transmit ~90% and tint slightly cool.
      // Tinted brighter than the world — which is what 1.02 did — the lens read as
      // a lamp instead of a window.
      const float glass[4] = {0.86f, 0.89f, 0.93f, 1.0f};
      const float grim[4] = {0.52f, 0.55f, 0.58f, 1.0f};   // radial vignette
      for (int i = 0; i < SCOPE_SEGS; i++) {
        float a0 = (float)i * F_TAU / SCOPE_SEGS;
        float a1 = (float)(i + 1) * F_TAU / SCOPE_SEGS;
        ui_push(cx, cy, 0.5f, 0.5f, glass);
        ui_push(cx + cosf(a0) * rad, cy + sinf(a0) * rad,
                0.5f + cosf(a0) * 0.5f, 0.5f - sinf(a0) * 0.5f, grim);
        ui_push(cx + cosf(a1) * rad, cy + sinf(a1) * rad,
                0.5f + cosf(a1) * 0.5f, 0.5f - sinf(a1) * 0.5f, grim);
      }
      // HDR picture through the ocular's optics (HUD_FS uImg 2). The eye
      // offset is how far the ocular centre sits off the view axis, in disc
      // radii: the exit-pupil shadow sweeps in from that side.
      glUniform1i(R.u_img, 2);
      grade_uniforms(R.prog_hud, g_cfg.post_fx);
      glUniform1i(uloc(R.prog_hud, "uAdapt"), 1);
      tex_on(1, R.ad_tex[R.ad_i]);
      { float ex = (cx - vw_ * 0.5f) / fmaxf(rad, 1.0f), ey = -(cy - vh_ * 0.5f) / fmaxf(rad, 1.0f);
        float mis = f_clamp(sqrtf(ex * ex + ey * ey) * 2.2f, 0.0f, 1.0f);
        glUniform4f(uloc(R.prog_hud, "uLens"), ex * 1.4f, ey * 1.4f, mis, 0.010f); }
      glBindTexture(GL_TEXTURE_2D, R.scope_tex);
      ui_flush();
      tex_on(1, 0);
      glUniform1i(R.u_img, 0);
      // Glass furniture, drawn over the picture in font/solid mode: a dark
      // vignette ring that fades the image into the tube wall, then the reticle
      // — a fine cross with a gap at the centre (so the aim point is never
      // covered), tapered thick posts outside it, and mil hashes down the lower
      // post. This is the sight picture, so it is HUD-crisp rather than modelled.
      ui_begin();
      glBindTexture(GL_TEXTURE_2D, R.font_tex);
      for (int i = 0; i < SCOPE_SEGS; i++) {
        float a0 = (float)i * F_TAU / SCOPE_SEGS;
        float a1 = (float)(i + 1) * F_TAU / SCOPE_SEGS;
        float r0 = rad * 0.86f, r1 = rad + 0.6f;
        const float in_[4] = {0.02f, 0.02f, 0.025f, 0.0f};
        const float out_[4] = {0.02f, 0.02f, 0.025f, 0.96f};
        ui_push(cx + cosf(a0) * r0, cy + sinf(a0) * r0, 0, 0, in_);
        ui_push(cx + cosf(a1) * r0, cy + sinf(a1) * r0, 0, 0, in_);
        ui_push(cx + cosf(a1) * r1, cy + sinf(a1) * r1, 0, 0, out_);
        ui_push(cx + cosf(a0) * r0, cy + sinf(a0) * r0, 0, 0, in_);
        ui_push(cx + cosf(a1) * r1, cy + sinf(a1) * r1, 0, 0, out_);
        ui_push(cx + cosf(a0) * r1, cy + sinf(a0) * r1, 0, 0, out_);
      }
      float gap = rad * 0.055f, thin = rad * 0.010f, thick = rad * 0.021f;
      for (int i = 0; i < 4; i++) {
        float dx = (i == 0) - (i == 1), dy = (i == 2) - (i == 3);
        ui_line(cx + dx * gap, cy + dy * gap, cx + dx * rad * 0.46f,
                cy + dy * rad * 0.46f, thin, 0.04f, 0.04f, 0.05f, 0.95f);
        ui_line(cx + dx * rad * 0.46f, cy + dy * rad * 0.46f,
                cx + dx * rad * 0.92f, cy + dy * rad * 0.92f, thick,
                0.04f, 0.04f, 0.05f, 0.95f);
      }
      // MIL-DOTS on all four arms, as etched in the glass of a real mil-dot
      // reticle — round, not hashes — with the holdover marks below the
      // centre kept as the longer bars a shooter ranges with.
      for (int arm = 0; arm < 4; arm++) {
        float dx = (arm == 0) - (arm == 1), dy = (arm == 2) - (arm == 3);
        for (int i = 1; i <= 3; i++) {
          float x = cx + dx * rad * 0.12f * (float)i, y = cy + dy * rad * 0.12f * (float)i;
          ui_disk(x, y, thin * 1.9f, 0.04f, 0.04f, 0.05f, 0.92f);
        }
      }
      for (int i = 1; i <= 4; i++) {  // holdover bars below centre
        float y = cy + rad * 0.12f * (float)i + rad * 0.06f;
        float hlen = rad * (i % 2 ? 0.030f : 0.055f);
        ui_line(cx - hlen, y, cx + hlen, y, thin * 0.8f, 0.04f, 0.04f, 0.05f, 0.85f);
      }
      ui_flush();
    }
  }
  glBindTexture(GL_TEXTURE_2D, R.font_tex);
  ui_begin();
  perf_draw();
  if (g_cfg.show_fps) {
    char text[16];
    snprintf(text, sizeof text, "%d FPS", fps);
    float tx = g_ui_vw - ui_text_width_tr(text, 0.7f, 0.6f) - 7;
    ui_text_sh(text, tx, 6, 0.7f, TXT_REG, 0.6f, C4(C_HAZE, 0.7f));
  }
  if (!g_menu_open && !cam_free && !g_vmorb.on) {
    g_ui_alpha = f_clamp(g_cfg.hud_alpha, 0.2f, 1.0f);
    // HUD SCALE: the whole in-match HUD lays itself out from g_ui_vw/vh, so a
    // larger virtual->pixel scale with a correspondingly smaller virtual space
    // grows every element about its own anchor (corners stay corners, the
    // crosshair stays centred, world markers re-project). Restored below
    // before the menu, which keeps its own scale.
    const float hud_base_s = g_uis, hud_base_vw = g_ui_vw, hud_base_vh = g_ui_vh;
    g_uis = hud_base_s * f_clamp(g_cfg.hud_scale, 0.75f, 1.5f);
    g_ui_vw = (float)R.width / g_uis;
    g_ui_vh = (float)R.height / g_uis;
    float ccx = g_ui_vw * 0.5f, ccy = g_ui_vh * 0.5f;
    int in_play = p->alive && !g_match.over;
    // Crosshair: the gap mirrors the live spread cone, so bloom/motion
    // penalties are visible before the first shot. Hidden while aiming.
    // A scoped rifle has no business showing a hip crosshair: the reticle in the
    // glass IS the aim, and two aiming marks on one screen is worse than either.
    // It goes the moment the rifle starts coming up, not half way through.
    // 0.95, NOT 0.5, AND THE DOT FADES IN OVER THE SAME BAND (67 ms of blind
    // gap, measured — see the red-dot comment below). The SNIPER keeps 0.02.
    float ch_hide = p->wp.cur == WPN_SR ? 0.02f : 0.95f;
    if ((ads < ch_hide || third) && in_play) {  // 3rd person: crosshair even in ADS
      // 2.5x on the honest cone projection — pure readability — plus the HIT
      // KICK, so the reticle reacts to LANDING a round and not only to firing
      // one. Everything else on this element is spread, which is a decision
      // the player made a tick ago; this is the consequence. Drawn as capsules
      // now: round caps are the GRATICULE stroke language.
      float gap = 3.0f + tanf(weapon_spread(p) * DEG2RAD) / tanf(fovy * 0.5f) * 450.0f
                + g_ch_kick;
      float a = 1.0f - ads / ch_hide;
      // The player's crosshair (Settings > HUD). Style 0, white, size 1 is the
      // shipped mark exactly: same arms, same outline, same alpha. The gap
      // still follows the spread in every style that has one.
      int cst = armory_idx(g_cfg.ch_style, CH_STYLE_COUNT);
      const float *cc = CH_COLORS[armory_idx(g_cfg.ch_color, CH_COLOR_COUNT)];
      float cs = f_clamp(g_cfg.ch_size, 0.5f, 2.0f);
      const float armlen = 4.6f * cs;
      if (cst != 1 && cst != 3) {
        for (int i = 0; i < 4; i++) {
          if (cst == 4 && i == 3) continue;              // T: no top arm
          float dx = (i == 0) - (i == 1), dy = (i == 2) - (i == 3);
          float x0 = ccx + dx * gap, y0 = ccy + dy * gap;
          float x1 = ccx + dx * (gap + armlen), y1 = ccy + dy * (gap + armlen);
          ui_capsule(x0, y0, x1, y1, 1.15f * cs, 0.02f, 0.03f, 0.04f, 0.50f * a);
          ui_capsule(x0, y0, x1, y1, 0.62f * cs, cc[0], cc[1], cc[2], 0.97f * a);
        }
      }
      if (cst == 3) {                                    // ring at the spread
        float rr = gap + armlen * 0.5f;
        for (int i = 0; i < 24; i++) {
          float t0 = (float)i / 24.0f * 6.2831853f, t1 = (float)(i + 1) / 24.0f * 6.2831853f;
          float x0 = ccx + cosf(t0) * rr, y0 = ccy + sinf(t0) * rr;
          float x1 = ccx + cosf(t1) * rr, y1 = ccy + sinf(t1) * rr;
          ui_capsule(x0, y0, x1, y1, 1.10f * cs, 0.02f, 0.03f, 0.04f, 0.45f * a);
          ui_capsule(x0, y0, x1, y1, 0.55f * cs, cc[0], cc[1], cc[2], 0.95f * a);
        }
      }
      if (cst == 1 || cst == 2 || cst == 3) {            // centre dot
        // A DOT IS ITS OWN MARK, not an arm's end: at the arm's 0.62 width it
        // was a 1.5 px speck at 720p. The lone DOT style gets the larger size.
        float dr = (cst == 1 ? 1.25f : 0.80f) * cs;
        ui_diskn(ccx, ccy, dr + 0.50f, 14, C4(0.02f, 0.03f, 0.04f, 0.55f * a));
        ui_diskn(ccx, ccy, dr, 14, C4(cc[0], cc[1], cc[2], 0.97f * a));
      }
    }
    // Sight picture once snapped in, exactly point-of-aim — the AR's red dot only.
    // The sniper has NO screen-space sight mark: its reticle is drawn inside the
    // scope disc, on the magnified image, and this camera-axis cross used to be
    // drawn on top of it. Two reticles at once, one of them not even in the glass.
    if (in_play && p->wp.cur == WPN_GR && p->wp.gr_cook >= NADE_PIN_T) {
      // The fuse, counted in the hand. Reads red in the last second.
      float left = (float)(NADE_FUSE - p->wp.gr_cook) * TICK_DT;
      char fs[16];
      snprintf(fs, sizeof fs, "%.1f", (double)left);
      float fw_ = ui_text_width(fs, 1.1f);
      if (left < 1.0f) ui_text_sh(fs, ccx - fw_ * 0.5f, ccy + 12, 1.1f, TXT_BOLD, 0, C4(1.0f, 0.29f, 0.23f, 1.0f));
      else ui_text_sh(fs, ccx - fw_ * 0.5f, ccy + 12, 1.1f, TXT_BOLD, 0, C4(C_BONE, 0.95f));
    }
    float dot_a = f_clamp((ads - 0.78f) / 0.17f, 0.0f, 1.0f);
    float glass_a = f_clamp((ads - 0.35f) / 0.40f, 0.0f, 1.0f);
    if (glass_a > 0.0f && in_play && !third && p->wp.cur == WPN_AR) {
      // The holo window was projected by vm_build in BASE units: draw it there.
      float ks = g_uis, kw = g_ui_vw, kh = g_ui_vh;
      g_uis = hud_base_s; g_ui_vw = hud_base_vw; g_ui_vh = hud_base_vh;
      holo_draw(hud_base_vw * 0.5f, hud_base_vh * 0.5f, glass_a, dot_a);
      g_uis = ks; g_ui_vw = kw; g_ui_vh = kh;
    }
    // THE HITMARKER. Four diagonal strokes; the levers are the lessons:
    // it POPS AND SETTLES off g_hitmark_t (a distinct beat per round, even
    // re-armed mid-life); the outline is CONCENTRIC, not a drop shadow (the
    // core is read against its border, not against its own impact spark);
    // a KILL IS BRIGHTER, NOT JUST REDDER (hue moves into the outline, the
    // core stays high-value — "sharper, not louder", same idiom as
    // SFX_HEAD_DUCK); the HEAD axis is a LAYER outside the base strokes.
    // Strokes start at 4.4 vu so the marker frames the aim point instead of
    // covering it; capsules give the strokes the round GRATICULE caps.
    if (g_hitmark_t > 0) {
      int kill = g_hitmark_kind >= HM_KILL, head = g_hitmark_kind & 1;
      float span = (float)(kill ? KILLMARK_TICKS : HITMARK_TICKS);
      float e = (float)g_hitmark_t / span;        // 1 at birth -> 0 at death
      float pop = 1.0f + 0.28f * e * e * e * e;
      float a = f_clamp(e * 3.0f, 0.0f, 1.0f) * (kill ? 0.95f : 0.88f);
      float in = 4.4f * pop, out = (kill ? 11.4f : 9.0f) * pop;
      float r  = 1.0f, g = kill ? 0.74f : 0.96f, b = kill ? 0.64f : 0.96f;
      float orr = kill ? 0.78f : 0.0f, og = kill ? 0.05f : 0.0f, ob = kill ? 0.03f : 0.0f;
      // PLATE: steel-blue strokes and a diamond in the gap — the armour took it.
      int plate = g_hitmark_plate && !kill;
      if (plate) { r = 0.62f; g = 0.86f; b = 1.0f; orr = 0.02f; og = 0.06f; ob = 0.12f; }
      float k = 0.7071f;
      // DIRECTIONAL: centred on where the round landed, within a hand's span of
      // the crosshair, so a spread shot that clipped the edge marks the edge.
      float hcx = ccx, hcy = ccy;
      {
        const float *m = R.cur_vp.m;
        v3 q = g_hitmark_at;
        float cw = m[3] * q.x + m[7] * q.y + m[11] * q.z + m[15];
        if (cw > 0.05f) {
          float nx = (m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12]) / cw;
          float ny = (m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13]) / cw;
          float ox = nx * g_ui_vw * 0.5f, oy = -ny * g_ui_vh * 0.5f;
          float ol = sqrtf(ox * ox + oy * oy);
          if (ol > 10.0f) { ox *= 10.0f / ol; oy *= 10.0f / ol; }
          hcx += ox; hcy += oy;
        }
      }
      if (plate) {
        float d = 2.4f * pop;
        ui_line(hcx - d, hcy, hcx, hcy - d, 0.32f, r, g, b, a);
        ui_line(hcx, hcy - d, hcx + d, hcy, 0.32f, r, g, b, a);
        ui_line(hcx + d, hcy, hcx, hcy + d, 0.32f, r, g, b, a);
        ui_line(hcx, hcy + d, hcx - d, hcy, 0.32f, r, g, b, a);
      }
      for (int pass = 0; pass < (head ? 2 : 1); pass++) {
        float i0 = pass ? (out + 1.5f) : in, o0 = pass ? (out + 4.2f) : out;
        float hwd = pass ? 0.28f : 0.34f;
        for (int sx = -1; sx <= 1; sx += 2)
          for (int sy = -1; sy <= 1; sy += 2) {
            ui_capsule(hcx + (float)sx * i0 * k, hcy + (float)sy * i0 * k,
                       hcx + (float)sx * o0 * k, hcy + (float)sy * o0 * k,
                       hwd + 0.55f, orr, og, ob, 0.72f * a);
            ui_capsule(hcx + (float)sx * i0 * k, hcy + (float)sy * i0 * k,
                       hcx + (float)sx * o0 * k, hcy + (float)sy * o0 * k,
                       hwd, r, g, b, a);
          }
      }
    }
    if (in_play) {
      // Ammo and health mirror each other in the bottom corners: a large figure
      // over a thin meter. Both bleed toward THREAT as they run out, and an ink
      // shadow keeps them readable on open sky and sand.
      const float hud_mw = 92.0f, hud_bot = g_ui_vh - 13.0f;
      if (p->wp.cur == WPN_KN) {         // a blade has no magazine to count
        float rxe = g_ui_vw - 16;
        ui_text_sh("KNIFE", rxe - ui_text_width("KNIFE", 1.25f), hud_bot - 15.0f, 1.25f,
                   TXT_BOLD, 0, C4(C_BONE, 0.92f));
      } else {
        float rxe = g_ui_vw - 16;
        int mag = wdef(&p->wp, p->wp.cur)->mag, cur = p->wp.ammo[p->wp.cur];
        float a01 = f_clamp((float)cur / (0.25f * (float)mag), 0.0f, 1.0f);
        char am[16], mg[16];
        snprintf(am, sizeof am, "%d", cur);
        snprintf(mg, sizeof mg, "/%d", mag);
        float cr = 0.941f + (1.000f - 0.941f) * (1 - a01);
        float cg = 0.957f + (0.294f - 0.957f) * (1 - a01);
        float cb2 = 0.973f + (0.227f - 0.973f) * (1 - a01);
        float aa = 0.96f;
        if (p->wp.reload_t > 0) {
          float bl = 0.6f + 0.4f * sinf(g_ui_time * 6.0f);
          cr = 1.000f; cg = 0.549f; cb2 = 0.102f; aa = bl;
        }
        float mgw = ui_text_width(mg, 0.85f);
        ui_text_sh(mg, rxe - mgw, hud_bot - 13.5f, 0.85f, TXT_REG, 0, C4(C_BONE, 0.60f));
        ui_text_sh(am, rxe - mgw - 3 - ui_text_width(am, 1.9f), hud_bot - 19.5f, 1.9f,
                   TXT_BOLD, 0, C4(cr, cg, cb2, aa));
        float f01 = f_clamp((float)cur / (float)mag, 0.0f, 1.0f);
        ui_rect(rxe - hud_mw, hud_bot, hud_mw, 2.4f, 0, 0, 0, 0.42f);
        ui_rect(rxe - hud_mw * f01, hud_bot, hud_mw * f01, 2.4f, cr, cg, cb2, aa);
      }
      {
        float hp01 = f_clamp((float)p->hp / PLAYER_HP, 0.0f, 1.0f);
        float tt = 1.0f - hp01;
        float cr = 0.941f + (1.000f - 0.941f) * tt;
        float cg = 0.957f + (0.294f - 0.957f) * tt;
        float cb2 = 0.973f + (0.227f - 0.973f) * tt;
        char hps[8];
        snprintf(hps, sizeof hps, "%d", p->hp);
        float pulse = p->hp <= 25 ? 0.65f + 0.30f * sinf(g_ui_time * 7.0f) : 0.96f;
        ui_text_sh(hps, 16, hud_bot - 19.5f, 1.9f, TXT_BOLD, 0, C4(cr, cg, cb2, pulse));
        ui_rect(16, hud_bot, hud_mw, 2.4f, 0, 0, 0, 0.42f);
        ui_rect(16, hud_bot, hud_mw * hp01, 2.4f, cr, cg, cb2, pulse);
      }
      // Low-HP vignette: a breathing THREAT edge, gradient so it never
      // reads as a solid frame over the game.
      if (p->hp <= 25) {
        // A whisper, not a lava filter: the pulsing THREAT number in the
        // panel carries the state; the edge only echoes it.
        float va = (0.5f + 0.5f * sinf(g_ui_time * 7.0f)) * 0.09f + 0.05f;
        ui_vgrad(0, 0, g_ui_vw, 24, C4(C_THREAT, va), C4(C_THREAT, 0));
        ui_vgrad(0, g_ui_vh - 24, g_ui_vw, 24, C4(C_THREAT, 0), C4(C_THREAT, va));
        ui_hgrad(0, 0, 24, g_ui_vh, C4(C_THREAT, va), C4(C_THREAT, 0));
        ui_hgrad(g_ui_vw - 24, 0, 24, g_ui_vh, C4(C_THREAT, 0), C4(C_THREAT, va));
      }
    }

    // Incoming damage: a brief THREAT edge wash (gradients, not slabs) plus
    // one ARC per live attacker — an arc IS a direction at a glance, and the
    // chords fade toward the wedge ends so it has no hard corner.
    if (g_dmg_t > 0 && p->alive) {
      float k = (float)g_dmg_t / DMG_TICKS;
      ui_vgrad(0, 0, g_ui_vw, 22, C4(C_THREAT, 0.22f * k), C4(C_THREAT, 0));
      ui_vgrad(0, g_ui_vh - 22, g_ui_vw, 22, C4(C_THREAT, 0), C4(C_THREAT, 0.22f * k));
      ui_hgrad(0, 0, 22, g_ui_vh, C4(C_THREAT, 0.22f * k), C4(C_THREAT, 0));
      ui_hgrad(g_ui_vw - 22, 0, 22, g_ui_vh, C4(C_THREAT, 0), C4(C_THREAT, 0.22f * k));
    }
    for (int di = 0; di < DMG_SLOTS && p->alive; di++) {
      if (g_dmg[di].life <= 0) continue;
      float dk = (float)g_dmg[di].life / DMG_TICKS;
      float ddx = g_dmg[di].from.x - feet.x, ddz = g_dmg[di].from.z - feet.z;
      float rel = atan2f(ddx, -ddz) - yaw;
      const float R0 = 46.0f, HALF = 0.35f;      // 40 deg of arc
      for (int seg = 0; seg < 8; seg++) {
        float a0 = rel - HALF + (2.0f * HALF) * (float)seg / 8.0f;
        float a1 = rel - HALF + (2.0f * HALF) * (float)(seg + 1) / 8.0f;
        float m = 1.0f - fabsf((float)seg - 3.5f) / 4.6f;
        ui_line(ccx + sinf(a0) * R0, ccy - cosf(a0) * R0,
                ccx + sinf(a1) * R0, ccy - cosf(a1) * R0,
                2.5f, 0.05f, 0.01f, 0.01f, 0.50f * dk * m);
        ui_line(ccx + sinf(a0) * R0, ccy - cosf(a0) * R0,
                ccx + sinf(a1) * R0, ccy - cosf(a1) * R0,
                1.7f, C_THREAT, 0.95f * dk * m);
      }
    }

    // Just joined a team game: offer the side choice once.
    if (g_ui_team_prompt && g_online && teams_on()) {
      g_ui_team_prompt = 0;
      if (!g_menu_open) ui_open_menu();
      ui_route(UI_PAGE_TEAM);
    }
    // TEAMMATES ARE MARKED. Two sides in the same kit is a game of guessing
    // who to shoot, so every living teammate carries a small SIGNAL chevron
    // over his head, and his name while he is near; enemies carry nothing.
    // HOSTAGE RESCUE marks the squad the same way (in the squad's green).
    if ((teams_on() || rescue_view()) && !g_match.over && g_local_ent >= 0) {
      v3 me_p = ent_pos(g_local_ent);
      ENT_FOREACH(e) {
        if (!team_mates(g_local_ent, e) || !ent_alive(e)) continue;
        if (rescue_view() && !resc_is_squad(e)) continue;
        v3 hp = ent_pos(e);
        float dd = v3_len(v3_sub(hp, me_p));
        hp.y += 2.05f;
        float sx, sy;
        if (!rescue_proj(hp, &sx, &sy)) continue;
        float a = 0.95f - 0.45f * f_clamp((dd - 10.0f) / 40.0f, 0.0f, 1.0f);
        int tc = team_of(e);
        float mc[3] = {TEAM_RGB(tc)};
        if (rescue_view()) { mc[0] = 0.45f; mc[1] = 0.95f; mc[2] = 0.60f; }
        ui_line(sx - 4.0f, sy - 3.0f, sx, sy + 1.5f, 1.3f, mc[0], mc[1], mc[2], a);
        ui_line(sx, sy + 1.5f, sx + 4.0f, sy - 3.0f, 1.3f, mc[0], mc[1], mc[2], a);
        if (dd < 22.0f) {
          const char *nm = ent_name(e);
          ui_text_sh(nm, sx - ui_text_width_tr(nm, 0.6f, 1.2f) * 0.5f, sy - 12.0f, 0.6f,
                     TXT_REG, 1.2f, C4(mc[0], mc[1], mc[2], a));
        }
      }
    }

    // THE SCORELINE, top centre: my frags against my best RIVAL — the whole
    // match state a player needs mid-fight. My number pulses on my own kill,
    // growing DOWN from a fixed cap line: a HUD element that moves when you
    // score is a bug, not a pop.
    // "me" is g_local_ent and the rival scan SKIPS that id rather than
    // index 0 — skipping 0 with a local id != 0 counts our own kills as the
    // lead and the margin reads N:N forever. Hidden once the match is over:
    // the winner hero owns the top centre then.
    int hgm = game_mode();
    if (!g_match.over && hgm != GM_RESCUE) {
      // GUN GAME and LAST MAN STANDING score on the mode's own number (rung,
      // rounds), so that is what the me:rival line counts there.
      // Rungs read 1-based.
      const int *sca = hgm == GM_LMS ? g_match.mscore : g_match.kills;
#define HUD_SC(e) (hgm == GM_GUN ? gg_level(e) + 1 : sca[e])
      int me = g_local_ent >= 0 ? HUD_SC(g_local_ent) : 0, lead = hgm == GM_GUN;
      ENT_FOREACH(i)
        if (i != g_local_ent && HUD_SC(i) > lead) lead = HUD_SC(i);
#undef HUD_SC
      // TEAM DEATHMATCH: my side against theirs.
      int mt = team_of(g_local_ent) == 1;
      if (hgm == GM_TDM) { me = g_team_score[mt]; lead = g_team_score[!mt]; }
      char msc[8], lsc[8];
      snprintf(msc, sizeof msc, "%d", me);
      snprintf(lsc, sizeof lsc, "%d", lead);
      float pk = g_score_pop > 0 ? (float)g_score_pop / SCORE_POP_TICKS : 0.0f;
      char sc2[16];
      snprintf(sc2, sizeof sc2, "%s:%s", msc, lsc);
      float ms = 0.9f + 0.25f * pk * pk;
      int ahead = me >= lead && me > 0;
      ui_text_sh(sc2, ccx - ui_text_width_tr(sc2, ms, 0.8f) * 0.5f, 8, ms,
                 TXT_REG, 0.8f,
                 ahead ? C4(C_BLAZE, 0.8f)
                 : lead > me ? C4(C_HAZE, 0.85f) : C4(C_HAZE, 0.8f));
      // ...and the one line of mode state a player needs under it.
      char cap[40];
      cap[0] = '\0';
      if (hgm == GM_GUN && g_local_ent >= 0) {
        int l = gg_level(g_local_ent);
        if (l == GG_LEVELS - 1)
          snprintf(cap, sizeof cap, "%s \x05 RUNG %d/%d \x05 KILL TO WIN",
                   WPN_DEF[GG_LADDER[l]].name, l + 1, GG_LEVELS);
        else
          snprintf(cap, sizeof cap, "%s \x05 RUNG %d/%d \x05 %d/%d KILLS",
                   WPN_DEF[GG_LADDER[l]].name, l + 1, GG_LEVELS,
                   gg_progress(g_local_ent), GG_RUNG_KILLS);
      } else if (hgm == GM_LMS) {
        int al = 0;
        ENT_FOREACH(e) if (ent_alive(e)) al++;
        snprintf(cap, sizeof cap, "ROUND %d \x05 %d ALIVE \x05 FIRST TO %d",
                 g_match.round_no, al, mode_round_goal());
      } else if (hgm == GM_SNIPER || hgm == GM_BLADES) {
        snprintf(cap, sizeof cap, "%s", GM_NAMES[hgm]);
      } else if (hgm == GM_TDM) {
        snprintf(cap, sizeof cap, "TEAM %s \x05 FIRST TO %d", TEAM_NAMES[mt],
                 g_online && g_net_fraglimit ? g_net_fraglimit : g_cfg.sp_frag);
      }
      if (cap[0] && hgm == GM_TDM)
        ui_text_sh(cap, ccx - ui_text_width_tr(cap, 0.7f, 1.2f) * 0.5f, 22, 0.7f,
                   TXT_REG, 1.2f, C4(TEAM_RGB(mt), 0.9f));
      else if (cap[0])
        ui_text_sh(cap, ccx - ui_text_width_tr(cap, 0.7f, 1.2f) * 0.5f, 22, 0.7f,
                   TXT_REG, 1.2f, C4(C_HAZE, 0.8f));
      // LAST MAN STANDING intermission: who took the round, and when the next.
      if (hgm == GM_LMS && g_match.round_t > 0) {
        char rb[40], rs[24];
        int rw = g_match.round_win;
        if (rw < 0) snprintf(rb, sizeof rb, "ROUND DRAWN");
        else if (rw == g_local_ent) snprintf(rb, sizeof rb, "ROUND WON");
        else snprintf(rb, sizeof rb, "%s TAKES THE ROUND", ent_name(rw));
        snprintf(rs, sizeof rs, "NEXT ROUND IN %d",
                 (g_match.round_t + TICK_HZ - 1) / TICK_HZ);
        float rbw = ui_text_width_tr(rb, 1.8f, 2.0f);
        ui_vgrad(0, g_ui_vh * 0.24f, g_ui_vw, 50, C4(0, 0, 0, 0.45f), C4(0, 0, 0, 0));
        ui_text_ex(rb, ccx - rbw * 0.5f + 1, g_ui_vh * 0.26f + 1, 1.8f, TXT_BLACK, 2.0f,
                   C4(0.04f, 0.03f, 0.02f, 0.85f));
        ui_text_ex(rb, ccx - rbw * 0.5f, g_ui_vh * 0.26f, 1.8f, TXT_BLACK, 2.0f,
                   rw == g_local_ent ? C4(C_BLAZE, 1) : C4(C_BONE, 1));
        ui_text_ex(rs, ccx - ui_text_width_tr(rs, 0.9f, 1.4f) * 0.5f,
                   g_ui_vh * 0.26f + 22, 0.9f, TXT_REG, 1.4f, C4(C_HAZE, 0.9f));
      }
    }
    // ...and the multi-kill banner, under the crosshair rather than over it,
    // with a soft glow and a small entrance pop.
    if (g_streak_t > 0 && g_streak_n >= 2) {
      static const char *NM[5] = {"DOUBLE KILL", "TRIPLE KILL", "QUAD KILL",
                                  "PENTA KILL", "RAMPAGE"};
      const char *nm = NM[g_streak_n - 2 < 4 ? g_streak_n - 2 : 4];
      float ba = f_clamp((float)g_streak_t / 40.0f, 0.0f, 1.0f);
      float pop2 = 1.0f + 0.18f *
                   f_clamp(((float)g_streak_t - (STREAK_TICKS - 8)) / 8.0f, 0.0f, 1.0f);
      float bs = 0.85f * pop2;
      float bw = ui_text_width_tr(nm, bs, 1.8f);
      ui_text_ex(nm, ccx - bw * 0.5f, ccy + 26, bs, TXT_REG, 1.8f,
                 C4(C_BLAZE, 0.85f * ba));
    }

    // Kill feed top-right: chips that slide in from the right edge, newest
    // first. Grammar: KILLER > [headshot] VICTIM. A remote human is tinted
    // SIGNAL wherever the local player would be BLAZE — the "never
    // disguised" promise the plan makes at three screens. Rows the
    // local player is IN get the wash treatment.
    {
      float fy = 32;
      for (int i = 0; i < KILLFEED_N; i++) {
        if (g_feed[i].life <= 0) continue;
        float fa = g_feed[i].life > 60 ? 1.0f : (float)g_feed[i].life / 60.0f;
        float tin = ui_easeout(
            f_clamp((float)(KILLFEED_TICKS - g_feed[i].life) / 7.0f, 0.0f, 1.0f));
        float slide = (1.0f - tin) * 24.0f;
        const char *kn = ent_name(g_feed[i].src), *vn = ent_name(g_feed[i].dst);
        int meK = g_feed[i].src == g_local_ent, meD = g_feed[i].dst == g_local_ent;
        int src_h = human_of_ent(g_feed[i].src) > 0;   // >0: a remote human
        int dst_h = human_of_ent(g_feed[i].dst) > 0;
        float knw = ui_text_width(kn, 0.8f), vnw = ui_text_width(vn, 0.8f);
        float chev = ui_text_width("\x01", 0.8f);
        float hsw = g_feed[i].hs ? ui_text_width("\x04", 0.8f) + 3 : 0;
        const float rowh = 9;
        // Pure quiet text at the ONE small HUD size — no plates, no shadows;
        // color alone carries who did what to whom.
        float rw = knw + 4 + chev + 3 + hsw + vnw;
        float tx = g_ui_vw - 10 - rw + slide, ty2 = fy;
        // TEAM DEATHMATCH: every name in its side's colour, so a row reads
        // as red-on-blue or blue-on-red at a glance (a team kill, with
        // friendly fire on, is one colour twice).
        int tk = teams_on() ? team_of(g_feed[i].src) : -1;
        int td = teams_on() ? team_of(g_feed[i].dst) : -1;
        if (tk >= 0)    ui_text_sh(kn, tx, ty2, 0.8f, meK ? TXT_BOLD : TXT_REG, 0, C4(TEAM_RGB(tk), 0.95f * fa));
        else if (meK)   ui_text_sh(kn, tx, ty2, 0.8f, TXT_REG, 0, C4(C_BLAZE, 0.9f * fa));
        else if (src_h) ui_text_sh(kn, tx, ty2, 0.8f, TXT_REG, 0, C4(C_SIGNAL, 0.9f * fa));
        else            ui_text_sh(kn, tx, ty2, 0.8f, TXT_REG, 0, C4(C_BONE, 0.85f * fa));
        tx += knw + 4;
        ui_text_sh("\x01", tx, ty2, 0.8f, TXT_REG, 0, C4(C_HAZE, 0.8f * fa));
        tx += chev + 3;
        if (g_feed[i].hs) {
          ui_text_sh("\x04", tx, ty2, 0.8f, TXT_REG, 0, C4(C_THREAT, 0.9f * fa));
          tx += hsw;
        }
        if (td >= 0)    ui_text_sh(vn, tx, ty2, 0.8f, meD ? TXT_BOLD : TXT_REG, 0, C4(TEAM_RGB(td), 0.95f * fa));
        else if (meD)   ui_text_sh(vn, tx, ty2, 0.8f, TXT_REG, 0, C4(C_THREAT, 0.9f * fa));
        else if (dst_h) ui_text_sh(vn, tx, ty2, 0.8f, TXT_REG, 0, C4(C_SIGNAL, 0.9f * fa));
        else            ui_text_sh(vn, tx, ty2, 0.8f, TXT_REG, 0, C4(C_BONE, 0.8f * fa));
        fy += rowh + 2;
      }
    }

    // Death screen: the world stays visible; a scrim pulls the eye to the
    // lower third where the broadcast caption sits, and the respawn is a
    // progress capsule instead of a decimal readout.
    // SPECTATING (LAST MAN STANDING, out of the round): the fight being
    // watched IS the picture, so the death frame's scrim and centre caption
    // step aside for ONE quiet line hugging the bottom edge. Not two lines
    // higher up: the follow camera frames the target in the lower third, and
    // a caption at vh-44 sat right on their legs whenever a wall pulled the
    // camera in. The strip below the feet is the only band that is never
    // the subject.
    int spec_on = !p->alive && !g_match.over ? spec_target() : -1;
    if (spec_on >= 0) {
      ui_vgrad(0, g_ui_vh - 30, g_ui_vw, 30, C4(0, 0, 0, 0), C4(0, 0, 0, 0.55f));
      const char *l = "SPECTATING ";
      const char *r = game_mode() == GM_RESCUE ? "  \x05  THE SQUAD FIGHTS ON" : "  \x05  BACK IN NEXT ROUND";
      const char *nm = ent_name(spec_on);
      const float sc = 0.8f, tr = 1.2f;
      float lw = ui_text_width_tr(l, sc, tr) + tr, nw = ui_text_width_tr(nm, sc, tr) + tr;
      float rw = ui_text_width_tr(r, sc, tr);
      float sx = ccx - (lw + nw + rw) * 0.5f, sy = g_ui_vh - 15;
      ui_text_ex(l, sx, sy, sc, TXT_REG, tr, C4(C_HAZE, 0.85f));
      ui_text_ex(nm, sx + lw, sy, sc, TXT_REG, tr, C4(C_BONE, 0.95f));
      ui_text_ex(r, sx + lw + nw, sy, sc, TXT_REG, tr, C4(C_HAZE, 0.7f));
    } else if (!p->alive && !g_match.over) {
      // The death frame is STAGED: an INK scrim from both edges controls
      // whatever the corpse camera happens to face, so the caption never
      // stands on raw mud. World desaturation would need a post pass; the
      // scrim buys ~the same control for six vertices.
      ui_rect(0, 0, g_ui_vw, g_ui_vh, 0.04f, 0.03f, 0.04f, 0.30f);
      ui_vgrad(0, 0, g_ui_vw, g_ui_vh * 0.40f, C4(0, 0, 0, 0.55f), C4(0, 0, 0, 0));
      ui_vgrad(0, g_ui_vh * 0.42f, g_ui_vw, g_ui_vh * 0.58f,
               C4(0, 0, 0, 0), C4(0, 0, 0, 0.72f));
      float dy0 = g_ui_vh * 0.56f;
      const char *d1 = "ELIMINATED";
      float w1 = ui_text_width_tr(d1, 2.6f, 2.0f);
      ui_text_ex(d1, ccx - w1 * 0.5f + 1, dy0 + 1, 2.6f, TXT_BLACK, 2.0f,
                 C4(0.05f, 0.01f, 0.01f, 0.85f));
      ui_text_ex(d1, ccx - w1 * 0.5f, dy0, 2.6f, TXT_BLACK, 2.0f, C4(C_THREAT, 1));
      char d2[40];
      snprintf(d2, sizeof d2, "BY %s", ent_name(g_match.last_killer));
      ui_text(d2, ccx - ui_text_width(d2, 1) * 0.5f, dy0 + 30, 1, C_BONE, 0.92f);
      if (!mode_respawns()) {
        // No redeploy in LAST MAN STANDING: the wait is for the round, not a
        // timer (and once the camera starts following someone, the branch
        // above takes over).
        const char *d3 = "OUT FOR THIS ROUND";
        ui_text_ex(d3, ccx - ui_text_width_tr(d3, 0.8f, 1.5f) * 0.5f, dy0 + 48, 0.8f,
                   TXT_REG, 1.5f, C4(C_HAZE, 0.9f));
        const char *d4 = "BACK IN NEXT ROUND";
        ui_text_ex(d4, ccx - ui_text_width_tr(d4, 0.7f, 1.2f) * 0.5f, dy0 + 60, 0.7f,
                   TXT_REG, 1.2f, C4(C_HAZE, 0.7f));
      } else {
        float rp = 1.0f - f_clamp((float)p->respawn_t / PLAYER_RESPAWN_TICKS, 0.0f, 1.0f);
        ui_rect(ccx - w1 * 0.5f, dy0 + 46, w1, 3, C_BONE, 0.14f);
        ui_rect(ccx - w1 * 0.5f, dy0 + 46, w1 * rp, 3, C_BLAZE, 0.95f);
        ui_text_ex("REDEPLOY", ccx - ui_text_width_tr("REDEPLOY", 0.8f, 1.5f) * 0.5f,
                   dy0 + 53, 0.8f, TXT_REG, 1.5f, C4(C_HAZE, 0.9f));
      }
    }

    // Match over: winner hero over a top scrim, graticule ruler under it;
    // the scoreboard forces on below. VICTORY is gated on g_local_ent, not
    // on a literal 0 — a high-human-id client wins matches too.
    rescue_hud(p, ccx, ccy);
    if (g_match.over && !rescue_view()) {
      ui_vgrad(0, 0, g_ui_vw, 96, C4(0, 0, 0, 0.60f), C4(0, 0, 0, 0));
      char msg[32];
      if (g_match.winner >= MAX_ENTS)   // a side took it
        snprintf(msg, sizeof msg, "%s", won_by_me(g_match.winner) ? "VICTORY" : "DEFEAT");
      else if (g_match.winner == g_local_ent) snprintf(msg, sizeof msg, "VICTORY");
      else snprintf(msg, sizeof msg, "%s WINS", ent_name(g_match.winner));
      float w1 = ui_text_width_tr(msg, 2.6f, 2.2f);
      ui_text_ex(msg, ccx - w1 * 0.5f + 1, 25, 2.6f, TXT_BLACK, 2.2f,
                 C4(0.04f, 0.03f, 0.02f, 0.85f));
      ui_text_ex(msg, ccx - w1 * 0.5f, 24, 2.6f, TXT_BLACK, 2.2f, C4(C_BLAZE, 1));
      ui_rect(ccx - w1 * 0.5f, 47, w1, 1, C_BONE, 0.30f);
      // ONLINE THE REMATCH IS NOT A TRIGGER PULL. The fire press is gated off
      // on a client (it would restart this client's arena only), so the old
      // string was an instruction that did nothing; the server's countdown
      // goes here instead. Offline the line is unchanged. subbuf outlives the
      // draw call: same block scope, and ui_text_ex emits its quads here.
      char subbuf[32];
      const char *sub = "PRESS FIRE FOR REMATCH";
      if (g_online) {
        snprintf(subbuf, sizeof subbuf, "NEW ROUND IN %d", match_over_secs());
        sub = subbuf;
      }
      ui_text_ex(sub, ccx - ui_text_width_tr(sub, 0.9f, 1.4f) * 0.5f, 54, 0.9f,
                 TXT_REG, 1.4f, C4(C_BONE, 0.85f));
    }

    hud_enemy_marks(p);
    if (g_score_held || (g_match.over && !rescue_view())) ui_scoreboard();
    hud_match_summary();
    hud_hints(p, ccx);
    g_ui_alpha = 1.0f;
    g_uis = hud_base_s; g_ui_vw = hud_base_vw; g_ui_vh = hud_base_vh;
  }
  }
  if (!hud_on) {
    glBindTexture(GL_TEXTURE_2D, R.font_tex);
    ui_begin();
  }
  // The frosted backdrop exists to seat a panel over a FROZEN world. The boot
  // screen is neither frozen nor a panel, and blurring the shot it is built
  // around would be blurring the content.
  if (g_menu_open && session_pauses() && !g_home) ui_menu_backdrop();
  if (g_menu_open) ui_menu();
  ui_flush();

  // Leave the present FBO bound on both targets so glReadPixels/SwapBuffers
  // see it. (The HUD already drew into it: the post chain resolved the scene.)
  glBindFramebuffer(GL_FRAMEBUFFER, R.present_fbo);
}

#define DEFAULT_SEED 1337u

