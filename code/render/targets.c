// code/render/targets.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// --- the HDR chain's targets -----------------------------------------------
// One colour texture, optionally with a depth attachment (renderbuffer or
// texture) — every target in the chain is one of these.
static GLuint tex_make(int w, int h, GLenum ifmt, GLenum fmt, GLenum type, GLenum filt) {
  GLuint t;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexImage2D(GL_TEXTURE_2D, 0, (GLint)ifmt, w, h, 0, fmt, type, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)filt);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt == GL_NEAREST ? GL_NEAREST : GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}
static GLuint fbo_make(GLuint color, GLuint depth_tex) {
  GLuint f;
  glGenFramebuffers(1, &f);
  glBindFramebuffer(GL_FRAMEBUFFER, f);
  if (color) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
  else glDrawBuffer(GL_NONE);
  if (depth_tex) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    fatal("hdr: a post-chain framebuffer is incomplete (RGBA16F unsupported?)");
  return f;
}

static void scene_update(void) {
  if (R.scene_fbo && R.post_w == R.width && R.post_h == R.height) return;
  if (R.scene_fbo) {
    GLuint f[] = {R.scene_fbo, R.z_fbo[0], R.z_fbo[1], R.ao_fbo, R.vol_fbo, R.lum_fbo,
                  R.ad_fbo[0], R.ad_fbo[1], R.bl_fbo[0], R.bl_fbo[1], R.bl_fbo[2],
                  R.bl_fbo[3], R.bl_fbo[4]};
    GLuint t[] = {R.scene_tex, R.z_tex[0], R.z_tex[1], R.ao_tex, R.vol_tex, R.lum_tex,
                  R.ad_tex[0], R.ad_tex[1], R.bl_tex[0], R.bl_tex[1], R.bl_tex[2],
                  R.bl_tex[3], R.bl_tex[4]};
    glDeleteFramebuffers((GLsizei)(sizeof f / sizeof f[0]), f);
    glDeleteTextures((GLsizei)(sizeof t / sizeof t[0]), t);
    glDeleteRenderbuffers(1, &R.scene_zrb);
  }
  int w = R.width, h = R.height, hw = (w + 1) / 2, hh = (h + 1) / 2;
  R.post_w = w; R.post_h = h;
  R.scene_tex = tex_make(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, GL_LINEAR);
  glGenRenderbuffers(1, &R.scene_zrb);
  glBindRenderbuffer(GL_RENDERBUFFER, R.scene_zrb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  R.scene_fbo = fbo_make(R.scene_tex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, R.scene_zrb);
  for (int i = 0; i < 2; i++) {
    R.z_tex[i] = tex_make(w, h, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, GL_NEAREST);
    R.z_fbo[i] = fbo_make(0, R.z_tex[i]);
  }
  R.ao_tex = tex_make(hw, hh, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
  R.ao_fbo = fbo_make(R.ao_tex, 0);
  R.vol_tex = tex_make(hw, hh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, GL_LINEAR);
  R.vol_fbo = fbo_make(R.vol_tex, 0);
  for (int i = 0; i < 5; i++) {
    R.bl_w[i] = w >> (i + 1); R.bl_h[i] = h >> (i + 1);
    if (R.bl_w[i] < 1) R.bl_w[i] = 1;
    if (R.bl_h[i] < 1) R.bl_h[i] = 1;
    R.bl_tex[i] = tex_make(R.bl_w[i], R.bl_h[i], GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, GL_LINEAR);
    R.bl_fbo[i] = fbo_make(R.bl_tex[i], 0);
  }
  // The meter: 128x64, mipped down to one texel that holds the weighted mean.
  R.lum_tex = tex_make(128, 64, GL_RG16F, GL_RG, GL_HALF_FLOAT, GL_LINEAR_MIPMAP_LINEAR);
  glGenerateMipmap(GL_TEXTURE_2D);
  R.lum_levels = 7;
  R.lum_fbo = fbo_make(R.lum_tex, 0);
  for (int i = 0; i < 2; i++) {
    R.ad_tex[i] = tex_make(1, 1, GL_RG16F, GL_RG, GL_HALF_FLOAT, GL_NEAREST);
    R.ad_fbo[i] = fbo_make(R.ad_tex[i], 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  glClearColor(0.71f, 0.73f, 0.75f, 1.0f);
  R.ad_frame = -10;
  R.have_prev = 0;
  glBindTexture(GL_TEXTURE_2D, R.font_tex);
  glBindFramebuffer(GL_FRAMEBUFFER, R.present_fbo);
}
static GLuint scene_target(void) { return R.msaa_fbo ? R.msaa_fbo : R.scene_fbo; }

// Copy the depth the frame holds right now into z_tex[which]. Called twice:
// before the viewmodel clears depth (the world) and after it (the weapon).
static void depth_copy(int which) {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_target());
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R.z_fbo[which]);
  glBlitFramebuffer(0, 0, R.width, R.height, 0, 0, R.width, R.height,
                    GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, scene_target());
}

static mat4 m4_inverse(mat4 a) {
  const float *m = a.m;
  float inv[16];
  inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
  inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
  inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
  inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
  inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
  inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
  inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
  inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
  inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
  inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
  inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
  inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
  inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
  inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
  inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
  inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
  float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
  mat4 r;
  float id = fabsf(det) > 1e-30f ? 1.0f / det : 0.0f;
  for (int i = 0; i < 16; i++) r.m[i] = inv[i] * id;
  return r;
}

static GLint uloc(GLuint prog, const char *name) { return glGetUniformLocation(prog, name); }
static void fs_tri(GLuint fbo, int w, int h) {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, w, h);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}
static void depth_uniforms(GLuint prog) {
  glUniform2f(uloc(prog, "uTanXY"), R.tan_x, R.tan_y);
  glUniform2f(uloc(prog, "uNF"), ZNEAR, ZFAR);
}
// The grade uniforms, shared by the frame composite and the scope picture.
static void grade_uniforms(GLuint prog, int fx) {
  const atm_t *a = atm_now();
  glUniform1f(uloc(prog, "uSat"), a->sat);
  glUniform1f(uloc(prog, "uCon"), a->con);
  glUniform1f(uloc(prog, "uVig"), a->vig);
  glUniform3fv(uloc(prog, "uTint"), 1, a->tint);
  glUniform3fv(uloc(prog, "uLift"), 1, a->lift);
  glUniform1f(uloc(prog, "uFx"), fx ? 1.0f : 0.0f);
}

// Heat haze over the player's muzzle: published by the weapon code (Phase 6).
static float g_heat_shimmer;
static int   g_post_dbg;          // harness `postdbg`: show one post channel
static v3    g_heat_scr;          // x, y in uv, radius in uv

// Eye adaptation + the whole post chain, scene -> present buffer. ALWAYS runs:
// the display transform lives here now, so `fx` only switches the effects.
static void post_pass(int fx) {
  const atm_t *a = atm_now();
  msaa_resolve();
  glDepthMask(GL_TRUE);
  if (!R.vm_drawn) {   // no weapon this frame: its depth is "nothing"
    glBindFramebuffer(GL_FRAMEBUFFER, R.z_fbo[1]);
    glClear(GL_DEPTH_BUFFER_BIT);
  }
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(R.vao_sky);
  glBindSampler(1, 0);          // the PCSS sampler must not filter the chain

  // METER + ADAPT. A frame that does not directly follow the last one (the
  // first frame, a harness `shot` after a `wait`, a map change) snaps to the
  // metered exposure instead of easing from a stale one.
  glUseProgram(R.prog_lum);
  glUniform1i(uloc(R.prog_lum, "uTex"), 0);
  glUniform1i(uloc(R.prog_lum, "uVmD"), 1);
  glBindTexture(GL_TEXTURE_2D, R.scene_tex);
  tex_on(1, R.z_tex[1]);
  fs_tri(R.lum_fbo, 128, 64);
  glBindTexture(GL_TEXTURE_2D, R.lum_tex);
  glGenerateMipmap(GL_TEXTURE_2D);
  int src = R.ad_i, dst = 1 - R.ad_i;
  int snap = R.ad_frame != R.frame_no - 1;
  R.ad_frame = R.frame_no;
  glUseProgram(R.prog_adapt);
  glUniform1i(uloc(R.prog_adapt, "uLum"), 0);
  glUniform1i(uloc(R.prog_adapt, "uPrev"), 1);
  glUniform1f(uloc(R.prog_adapt, "uLod"), (float)R.lum_levels);
  // Brightening is slow (an eye opening in a dark alley, ~1.6 s), darkening
  // faster — both measured on presentation time, never on the sim clock.
  glUniform1f(uloc(R.prog_adapt, "uK"), snap ? 1.0f : 1.0f - expf(-g_ui_dt * 1.4f));
  glUniform1f(uloc(R.prog_adapt, "uKey"), 0.10f);
  // Range in stops, around the preset's authored exposure: the art direction
  // stays in charge, adaptation only rescues the extremes.
  // The authored look is the target: no map's normal range of light is
  // corrected at all (the dead band below covers ~0.03 .. 0.3 mean luminance).
  // A +0.43-stop lift on the shaded harbour read as a chalky, over-exposed
  // frame, so only genuinely dark interiors are helped, and only a little.
  glUniform2f(uloc(R.prog_adapt, "uRange"), -0.30f, 0.60f);
  tex_on(1, R.ad_tex[src]);
  fs_tri(R.ad_fbo[dst], 1, 1);
  R.ad_i = dst;

  if (fx) {
    // AO / contact shadows / edges.
    glUseProgram(R.prog_ao);
    depth_uniforms(R.prog_ao);
    glUniform1i(uloc(R.prog_ao, "uDepth"), 0);
    glUniform2f(uloc(R.prog_ao, "uPix"), 1.0f / (float)R.width, 1.0f / (float)R.height);
    v3 sn = atm_sun();
    glUniform3f(uloc(R.prog_ao, "uSunV"), v3_dot(sn, R.cam_rgt), v3_dot(sn, R.cam_up),
                -v3_dot(sn, R.cam_fwd));
    glUniform1i(uloc(R.prog_ao, "uSm"), 1);
    glUniform1f(uloc(R.prog_ao, "uSmOn"), R.sm_fbo ? 1.0f : 0.0f);
    glUniformMatrix4fv(uloc(R.prog_ao, "uInvVP"), 1, GL_FALSE, R.inv_vp.m);
    glUniformMatrix4fv(uloc(R.prog_ao, "uLightVP"), 1, GL_FALSE, R.light_vp.m);
    glUniform3f(uloc(R.prog_ao, "uSunW"), sn.x, sn.y, sn.z);
    tex_on(1, R.sm_tex);
    glBindTexture(GL_TEXTURE_2D, R.z_tex[0]);
    fs_tri(R.ao_fbo, (R.width + 1) / 2, (R.height + 1) / 2);

    // Volumetric sun.
    glUseProgram(R.prog_vol);
    glUniform1i(uloc(R.prog_vol, "uDepth"), 0);
    glUniform1i(uloc(R.prog_vol, "uSm"), 1);
    glUniformMatrix4fv(uloc(R.prog_vol, "uInvVP"), 1, GL_FALSE, R.inv_vp.m);
    glUniformMatrix4fv(uloc(R.prog_vol, "uLightVP"), 1, GL_FALSE, R.light_vp.m);
    glUniform3f(uloc(R.prog_vol, "uEye"), g_cam_eye.x, g_cam_eye.y, g_cam_eye.z);
    glUniform3f(uloc(R.prog_vol, "uSun"), sn.x, sn.y, sn.z);
    glUniform3fv(uloc(R.prog_vol, "uSunC"), 1, a->sun_col);
    // Density follows the preset's haze: a storm front is thick, high noon clear.
    glUniform1f(uloc(R.prog_vol, "uDens"), R.sm_fbo ? 0.0030f + a->fog * 42.0f : 0.0f);
    glUniform1f(uloc(R.prog_vol, "uExp"), a->exposure);
    tex_on(1, R.sm_tex);
    fs_tri(R.vol_fbo, (R.width + 1) / 2, (R.height + 1) / 2);

    // Bloom: down five levels with the soft knee on the first, back up additively.
    glUseProgram(R.prog_bdown);
    glUniform1i(uloc(R.prog_bdown, "uTex"), 0);
    for (int i = 0; i < 5; i++) {
      int sw = i ? R.bl_w[i - 1] : R.width, sh = i ? R.bl_h[i - 1] : R.height;
      glBindTexture(GL_TEXTURE_2D, i ? R.bl_tex[i - 1] : R.scene_tex);
      glUniform2f(uloc(R.prog_bdown, "uHp"), 0.5f / (float)sw, 0.5f / (float)sh);
      glUniform1f(uloc(R.prog_bdown, "uThr"), i ? -1.0f : 1.6f);
      fs_tri(R.bl_fbo[i], R.bl_w[i], R.bl_h[i]);
    }
    glUseProgram(R.prog_bup);
    glUniform1i(uloc(R.prog_bup, "uTex"), 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = 4; i > 0; i--) {
      glBindTexture(GL_TEXTURE_2D, R.bl_tex[i]);
      glUniform2f(uloc(R.prog_bup, "uHp"), 0.5f / (float)R.bl_w[i], 0.5f / (float)R.bl_h[i]);
      fs_tri(R.bl_fbo[i - 1], R.bl_w[i - 1], R.bl_h[i - 1]);
    }
    glDisable(GL_BLEND);
  }

  // COMPOSITE into the present buffer.
  GLuint pp = R.prog_post;
  glUseProgram(pp);
  depth_uniforms(pp);
  grade_uniforms(pp, fx);
  const char *names[7] = {"uTex", "uAO", "uVol", "uBloom", "uAdapt", "uDepth", "uVmD"};
  GLuint texs[7] = {R.scene_tex, R.ao_tex, R.vol_tex, R.bl_tex[0], R.ad_tex[R.ad_i],
                    R.z_tex[0], R.z_tex[1]};
  for (int i = 0; i < 7; i++) {
    glUniform1i(uloc(pp, names[i]), i);
    tex_on(i, texs[i]);
  }
  glUniform1f(uloc(pp, "uBloomK"), 0.05f + a->bloom * 0.16f);
  glUniform1f(uloc(pp, "uCA"), 0.0065f);
  glUniform1f(uloc(pp, "uMotion"), R.have_prev && !snap ? 0.32f : 0.0f);
  glUniform1f(uloc(pp, "uAds"), R.ads_now);
  glUniform1f(uloc(pp, "uVmOn"), R.vm_drawn ? 1.0f : 0.0f);
  glUniform1f(uloc(pp, "uTime"), g_ui_time);
  glUniform1f(uloc(pp, "uAspect"), (float)R.width / (float)R.height);
  glUniform4f(uloc(pp, "uHeat"), g_heat_scr.x, g_heat_scr.y, fmaxf(g_heat_scr.z, 0.01f),
              g_heat_shimmer);
  glUniform3fv(uloc(pp, "uSunC"), 1, a->sun_col);
  glUniform1f(uloc(pp, "uDbg"), (float)g_post_dbg);
  glUniform2f(uloc(pp, "uVmVel"), R.vm_vel[0], R.vm_vel[1]);
  glUniformMatrix4fv(uloc(pp, "uInvVP"), 1, GL_FALSE, R.inv_vp.m);
  glUniformMatrix4fv(uloc(pp, "uPrevVP"), 1, GL_FALSE, R.prev_vp.m);
  fs_tri(R.present_fbo, R.width, R.height);
  for (int i = 6; i >= 1; i--) tex_on(i, 0);
  R.prev_vp = R.cur_vp;
  R.have_prev = 1;
  glBindTexture(GL_TEXTURE_2D, R.font_tex);
}

// THE HOLOGRAPHIC SIGHT, in two layers. The GLASS: an anti-reflection coated
// pane — faintly cyan where it is seen straight through, magenta toward the
// rim where the coating's Fresnel lift shows, with one soft reflection streak
// from the sky. The RETICLE: a 1 MOA dot inside a 65 MOA ring with range
// ticks under it, on the point of aim (collimated, so parallax-free), and
// visible only where the window actually is — sway the rifle hard enough and
// the ring is cut off by the frame, which is what a real holo does.
// Window-local coordinates (u along the window's x, v along its y, -1..1)
// come from inverting the projected half-axes.
static int holo_local(float x, float y, float *u, float *v) {
  float a = g_holo_ex[0], b = g_holo_ey[0], c = g_holo_ex[1], d = g_holo_ey[1];
  float det = a * d - b * c;
  if (fabsf(det) < 1e-4f) return 0;
  float dx = x - g_holo_cx, dy = y - g_holo_cy;
  *u = (d * dx - b * dy) / det;
  *v = (-c * dx + a * dy) / det;
  return 1;
}
static float holo_clip(float x, float y) {
  float u, v;
  if (!holo_local(x, y, &u, &v)) return 0.0f;
  float e = fmaxf(fabsf(u), fabsf(v));
  return 1.0f - smooth01(f_clamp((e - 0.86f) / 0.14f, 0.0f, 1.0f));
}
static void holo_draw(float ccx, float ccy, float glass_a, float dot_a) {
  if (g_holo_ex[0] == 0.0f && g_holo_ey[1] == 0.0f) return;
  g_ui_mode = UIM_RAW;
  float su = g_hf_solid_u, sv = g_hf_solid_v;
  // Glass: a 4x4 patch so the Fresnel tint can vary across it.
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++) {
      float u0 = -1.0f + 0.5f * (float)i, v0 = -1.0f + 0.5f * (float)j;
      float uu[4] = {u0, u0 + 0.5f, u0 + 0.5f, u0}, vv[4] = {v0, v0, v0 + 0.5f, v0 + 0.5f};
      float px[4], py[4], col[4][4];
      for (int k = 0; k < 4; k++) {
        px[k] = g_holo_cx + g_holo_ex[0] * uu[k] + g_holo_ey[0] * vv[k];
        py[k] = g_holo_cy + g_holo_ex[1] * uu[k] + g_holo_ey[1] * vv[k];
        float r = fminf(1.0f, uu[k] * uu[k] * 0.5f + vv[k] * vv[k] * 0.5f);
        float fr = r * r;                      // the coating's rim lift
        // Reflection streak along the window's upper diagonal.
        float st = expf(-powf((uu[k] * 0.7f + vv[k] * 0.7f + 0.35f) * 3.2f, 2.0f));
        col[k][0] = 0.55f + 0.40f * fr + 0.30f * st;
        col[k][1] = 0.80f - 0.42f * fr + 0.30f * st;
        col[k][2] = 0.95f + 0.05f * st;
        col[k][3] = glass_a * (0.035f + 0.075f * fr + 0.05f * st);
        if (g_holo_kind) {   // a red dot's coating reflects amber, not the holo's blue-green
          col[k][0] = 1.00f; col[k][1] = 0.55f + 0.20f * st; col[k][2] = 0.20f + 0.25f * st;
          col[k][3] = glass_a * (0.05f + 0.10f * fr + 0.05f * st);
        }
      }
      ui_push(px[0], py[0], su, sv, col[0]); ui_push(px[1], py[1], su, sv, col[1]);
      ui_push(px[2], py[2], su, sv, col[2]); ui_push(px[0], py[0], su, sv, col[0]);
      ui_push(px[2], py[2], su, sv, col[2]); ui_push(px[3], py[3], su, sv, col[3]);
    }
  if (dot_a <= 0.0f) return;
  // Reticle, clipped per element by the window.
  float cr = 1.0f, cg = 0.20f, cb = 0.09f;
  float dc = holo_clip(ccx, ccy);
  if (dc > 0.0f) {
    ui_glow(ccx, ccy, 4.6f, 1.0f, 0.12f, 0.05f, 0.34f * dot_a * dc);
    ui_disk(ccx, ccy, 1.25f, cr, cg + 0.02f, cb, 0.98f * dot_a * dc);
  }
  if (g_holo_kind) return;   // a red dot is just the dot: no ring, no range ticks
  float rr = 11.5f;
  for (int i = 0; i < 48; i++) {
    float a0 = (float)i * F_TAU / 48.0f, a1 = (float)(i + 1) * F_TAU / 48.0f;
    float x0 = ccx + cosf(a0) * rr, y0 = ccy + sinf(a0) * rr;
    float x1 = ccx + cosf(a1) * rr, y1 = ccy + sinf(a1) * rr;
    float k = fminf(holo_clip(x0, y0), holo_clip(x1, y1));
    if (k <= 0.0f) continue;
    ui_line(x0, y0, x1, y1, 0.45f, cr, cg, cb, 0.80f * dot_a * k);
  }
  // Four cardinal ticks on the ring, and the range ticks under the dot.
  for (int i = 0; i < 4; i++) {
    float dx = (i == 0) - (i == 1), dy = (i == 2) - (i == 3);
    float x0 = ccx + dx * (rr - 1.6f), y0 = ccy + dy * (rr - 1.6f);
    float x1 = ccx + dx * (rr + 0.3f), y1 = ccy + dy * (rr + 0.3f);
    float k = holo_clip(x0, y0);
    if (k > 0.0f) ui_line(x0, y0, x1, y1, 0.5f, cr, cg, cb, 0.85f * dot_a * k);
  }
  for (int i = 1; i <= 3; i++) {
    float y = ccy + 3.2f * (float)i;
    float hl = i == 2 ? 1.5f : 0.9f;
    float k = holo_clip(ccx, y);
    if (k > 0.0f) ui_line(ccx - hl, y, ccx + hl, y, 0.36f, cr, cg, cb, 0.75f * dot_a * k);
  }
}


