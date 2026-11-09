// code/render/passes.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ONE upload per rebuild, not one per pass. g_scene is filled by scene_build and
// then drawn from up to three times — the shadow depth pass, the main view and
// the scope FBO — with nothing writing the buffer in between; fx_build and
// vm_build refill it for their own passes and flag it again. Re-sending 12.7 MB
// of identical vertices per extra pass measured -0.80 ms/frame at 20 bots, and
// the marginal cost of each further redundant upload is +0.37 ms.
static void scene_bind(void) {
  glBindVertexArray(R.vao_scene);
  glBindBuffer(GL_ARRAY_BUFFER, R.vbo_scene);
  if (g_scene_dirty) {
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(g_scene_floats * sizeof(float)),
                 g_scene, GL_STREAM_DRAW);
    g_scene_dirty = 0;
  }
}

static void scene_draw(void) {
  if (!g_scene_floats) return;
  scene_bind();
  glDrawArrays(GL_TRIANGLES, 0, g_scene_floats / VERT_FLOATS);
}

// The same buffer, minus the chunks this frustum cannot see. Everything BETWEEN
// chunks (impact marks, and any builder that never opened one) is always drawn:
// a run absorbs the gaps, so untagged geometry can only ever be over-drawn, not
// dropped. Runs are merged, so the common case — a camera looking at the fight —
// is still one or two draw calls, and the worst case is bounded by the chunk
// count rather than by the vertex count.
static void scene_draw_vp(const mat4 *vp) {
  if (!g_scene_floats) return;
  scene_bind();
  frustum_t fr = frustum_of(vp);
  int run0 = 0;
  for (int i = 0; i < g_nschunk; i++) {
    if (frustum_sees(&fr, g_schunk[i].c, g_schunk[i].r)) continue;
    // The gap before this chunk stays in the run; the chunk itself does not.
    if (g_schunk[i].f0 > run0)
      glDrawArrays(GL_TRIANGLES, run0 / VERT_FLOATS,
                   (g_schunk[i].f0 - run0) / VERT_FLOATS);
    run0 = g_schunk[i].f1;
  }
  if (g_scene_floats > run0)
    glDrawArrays(GL_TRIANGLES, run0 / VERT_FLOATS,
                 (g_scene_floats - run0) / VERT_FLOATS);
}

// Sky AFTER the world at depth 1.0 (LEQUAL, so only background pixels shade),
// then hand the world program back with detail OFF for the moving geometry that
// follows — that epilogue is load-bearing and easy to lose. Both consumers, the
// main view and the scope FBO, go through here, so a rolled camera cannot end up
// with an unrolled sky in one of them: a horizon sliding across a static gradient
// is the failure, and it was previously prevented by two copies staying in sync
// by hand. The scope target is square, so it passes aspect 1.
static void sky_pass(float yaw, float pitch, float roll, float fovy, float aspect) {
  v3 sf, sr, su;
  cam_basis(yaw, pitch, roll, &sf, &sr, &su);
  float ty = tanf(fovy * 0.5f);
  glUseProgram(R.prog_sky);
  glUniform3f(R.u_sky_fwd, sf.x, sf.y, sf.z);
  glUniform3f(R.u_sky_right, sr.x, sr.y, sr.z);
  glUniform3f(R.u_sky_up, su.x, su.y, su.z);
  glUniform2f(R.u_sky_tan, ty * aspect, ty);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glBindVertexArray(R.vao_sky);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  glUseProgram(R.prog_world);
  glUniform1f(R.u_detail, 0.0f);  // moving geometry: no world-anchored detail
}

// Additive light pass: depth-tested, not depth-written, ONE/ONE, vertex colour
// straight to the screen. Both halves were written out by hand twice — once for
// tracers and sparks, once for the muzzle flash — which is two chances to leave
// the depth mask or the cull face off for everything that draws after.
static void fx_pass_begin(void) {
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glUniform1f(R.u_emis, 1.0f);   // glow: vertex color straight to the screen
}
static void fx_pass_end(void) {
  glUniform1f(R.u_emis, 0.0f);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glDepthMask(GL_TRUE);
}

// A render target is an FBO AND its viewport; binding one without the other is
// how a scope pass ends up drawn at window resolution. Deliberately does NOT
// touch u_vp: that location belongs to prog_world, so the matrix write stays at
// the call site next to whichever program is actually current.
static void rt_bind(GLuint fbo, int w, int h) {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, w, h);
}

// Renders one frame: world from the interpolated player camera, then HUD.
// Camera can be overridden: fixed (debug shots) or bot-following (films the
// bot's walk/topple animations from a chase offset).
typedef struct {
  int active, bot, orbit;
  v3 eye, boff;
  float yaw, pitch, blook;
  float orbit_radius, orbit_height, orbit_rate, orbit_look, orbit_phase;
} cam_override_t;

