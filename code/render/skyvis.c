// code/render/skyvis.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ===========================================================================
// SKY VISIBILITY: how much of the sky a point in the static world can see.
//
// Every lit surface took the same open-sky ambient: the floor of a 3 m street
// canyon, the back of a shed and the middle of an empty yard all had one fill,
// so nothing was ever in shade at the scale of a BUILDING — only at the 0.55 m
// of the SSAO disc and the 2-3 cm of the contact rays. That uniform fill is the
// single most "block-out" thing left in the frame: a lane between two walls
// read as bright as open ground, and every reflective floor mirrored a full sky
// it could not possibly see.
//
// Built ONCE PER MAP, from the world itself, in two GPU passes:
//   HEIGHT  the casting world (solids + props, exactly the shadow casters)
//           rendered straight down into a 1024 R16F height field, then a
//           MAX-filtered mip chain over it (a coarse texel answers "the
//           tallest thing in this block", which is what a horizon search asks)
//   BAKE    for every 20 cm cell and six height layers, a horizon search over
//           12 azimuths x 8 radii (to 11 m) in that height field, reduced to
//           five numbers: the cosine-weighted sky a surface facing up, +x, -x,
//           +z and -z would see. Overhangs (a shed roof, a balcony, a canopy)
//           are the band between the roof's own edge and whatever stands
//           beyond it, so an open-sided shed still takes light from its sides.
// At runtime the world shader reads it with FOUR bilinear taps (two layers x
// two sets) and occludes only the SKY part of the ambient and the specular
// environment; the sun keeps its own shadow map. Figures and the viewmodel
// read the same field, which is what grounds a man in an alley: he darkens
// with the walls around him instead of glowing with open-sky fill.
//
// Cost: the bake is ~5 M texel-taps on a town map (a one-off stall at map
// load, ~10-20 ms on a handheld-class GPU); the runtime is 4 taps + ~40 ALU per
// lit fragment for diffuse AND specular together. Memory: 12.6 MB (town atlas
// 3072x1024 RGBA8) + 2.8 MB height field, 3.1 MB + 2.8 MB on a 24 m arena.
// ===========================================================================
#ifndef GL_TEXTURE_BASE_LEVEL
#define GL_TEXTURE_BASE_LEVEL 0x813C   // GL 1.2: not in the Windows 1.1 header
#endif
#define HF_TEX     1024
#define SV_LAYERS  6
// The layer heights live with the world shader (GLSL_SVY in atmos.c): the
// runtime and the bake read the SAME list.
#define SV_YLO   (-1.0f)
#define SV_YHI   63.0f

static const char *HF_VS =
  "#version 330 core\n"
  "layout(location=0) in vec3 aPos;\n"
  "uniform mat4 uVP; out float vY;\n"
  "void main(){ vY = aPos.y; gl_Position = uVP * vec4(aPos, 1.0); }\n";
static const char *HF_FS =
  "#version 330 core\n"
  "in float vY; out vec4 oCol;\n"
  "void main(){ oCol = vec4(vY, 0.0, 0.0, 1.0); }\n";
// One level of the MAX chain from the level under it (which is the texture's
// only visible level while this runs: base = max = L-1, so nothing samples the
// level being written).
static const char *HFMAX_FS =
  "#version 330 core\n"
  "uniform sampler2D uTex; out vec4 oCol;\n"
  "void main(){\n"
  "  ivec2 p = ivec2(gl_FragCoord.xy) * 2, m = textureSize(uTex, 0) - 1;\n"
  "  float a = texelFetch(uTex, min(p, m), 0).r;\n"
  "  a = max(a, texelFetch(uTex, min(p + ivec2(1, 0), m), 0).r);\n"
  "  a = max(a, texelFetch(uTex, min(p + ivec2(0, 1), m), 0).r);\n"
  "  a = max(a, texelFetch(uTex, min(p + ivec2(1, 1), m), 0).r);\n"
  "  oCol = vec4(a, 0.0, 0.0, 1.0);\n"
  "}\n";

// THE BAKE. Atlas: SV_LAYERS tiles across, two sets down. Set 0 holds the four
// horizontal normals (+x, -x, +z, -z), set 1 the up-facing value in .r and
// the roofed flag in .g.
//   up-facing  : the cosine-weighted sky below elevation e is sin^2(e)
//   vertical   : ... and for a wall facing the azimuth it is (2e + sin 2e)/pi,
//                weighted across azimuths by the cosine to the wall's normal
// A point under cover (the column above it is taller than it) sees sky only in
// the band between the cover's edge and the occluders beyond it; with no edge
// inside 11 m that azimuth is closed.
static const char *SVBAKE_FS =
  "#version 330 core\n"
  GLSL_SVY
  "uniform sampler2D uHf; uniform float uExt, uHfTexel; uniform int uN;\n"
  "out vec4 oCol;\n"
  "const float RAD[8] = float[](0.30, 0.60, 1.00, 1.60, 2.60, 4.20, 6.80, 11.0);\n"
  "float hf(vec2 xz, float lod){ return textureLod(uHf, xz / (2.0 * uExt) + 0.5, lod).r; }\n"
  "void main(){\n"
  "  ivec2 f = ivec2(gl_FragCoord.xy);\n"
  "  int layer = f.x / uN, set = f.y / uN;\n"
  "  vec2 t = (vec2(f.x - layer * uN, f.y - set * uN) + 0.5) / float(uN);\n"
  "  vec2 xz = (t * 2.0 - 1.0) * uExt;\n"
  "  float y = SVY[layer];\n"
  "  float hc = hf(xz, 0.0);\n"
  "  bool cov = hc > y + 0.15;\n"
  "  float vup = 0.0; vec4 vh = vec4(0.0), wh = vec4(0.0);\n"
  "  for (int d = 0; d < 12; d++) {\n"
  "    float a = (float(d) + 0.5) * 0.5235988;\n"
  "    vec2 dir = vec2(cos(a), sin(a));\n"
  "    float to = 0.0, tr = 1e4;\n"
  "    bool edge = !cov;\n"
  "    for (int i = 0; i < 8; i++) {\n"
  "      float r = RAD[i];\n"
  "      float lod = max(log2(r * 0.35 / uHfTexel), 0.0);\n"
  // The cover's EDGE is found on the unfiltered level: a max-filtered texel
  // that straddles the edge still holds the roof, which pushed every edge out
  // by half a footprint and closed the whole floor of an open-sided shed.
  "      if (!edge) {\n"
  "        float h0 = hf(xz + dir * r, 0.0);\n"
  "        if (h0 < hc - 0.30) { edge = true; tr = (hc - y) / r; }\n"
  "        continue;\n"
  "      }\n"
  "      float h = hf(xz + dir * r, lod);\n"
  "      to = max(to, (h - y) / r);\n"
  "    }\n"
  "    float eo = atan(max(to, 0.0));\n"
  "    float er = edge ? (cov ? atan(tr) : 1.5707963) : 0.0;\n"
  "    float so = sin(eo), sr = sin(er);\n"
  "    vup += max(sr * sr - so * so, 0.0);\n"
  "    float fv = max((2.0 * er + sin(2.0 * er)) - (2.0 * eo + sin(2.0 * eo)), 0.0) / 3.14159265;\n"
  "    vec4 w = max(vec4(dir.x, -dir.x, dir.y, -dir.y), 0.0);\n"
  "    vh += w * fv; wh += w;\n"
  "  }\n"
  // Set 1 .g: this cell is ROOFED. The height field cannot see a door or a
  // window, so a roofed space's number is a lower bound and the runtime floors
  // it higher than an open canyon's (SV_MIN in atmos.c).
  "  oCol = set == 0 ? vh / max(wh, vec4(1e-4)) : vec4(vup / 12.0, cov ? 1.0 : 0.0, 0.0, 1.0);\n"
  "}\n";

static int g_sv_dbg;   // harness `svdbg N`: show the field itself
static struct {
  GLuint prog_hf, prog_max, prog_bake;
  GLuint hf_tex, hf_fbo, hf_rb, sv_tex, sv_fbo;
  int    n, levels, dirty, ok, tried;
  float  ext;
} SV;

static void sv_init(void) {
  SV.prog_hf = gl_program(HF_VS, HF_FS);
  SV.prog_max = gl_program(BLUR_VS, HFMAX_FS);
  SV.prog_bake = gl_program(BLUR_VS, SVBAKE_FS);
  glGenTextures(1, &SV.hf_tex);
  glBindTexture(GL_TEXTURE_2D, SV.hf_tex);
  SV.levels = 0;
  for (int s = HF_TEX; s >= 1; s >>= 1, SV.levels++)
    glTexImage2D(GL_TEXTURE_2D, SV.levels, GL_R16F, s, s, 0, GL_RED, GL_HALF_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenRenderbuffers(1, &SV.hf_rb);
  glBindRenderbuffer(GL_RENDERBUFFER, SV.hf_rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, HF_TEX, HF_TEX);
  glGenFramebuffers(1, &SV.hf_fbo);
  glGenFramebuffers(1, &SV.sv_fbo);
  SV.ok = 1;
  SV.dirty = 1;
}

// Run at the top of a frame whose world was (re)built. Leaves the default
// state the frame expects: unit 0 active, culling on, depth test on, no blend.
static void sv_build(void) {
  if (!SV.ok) sv_init();
  SV.dirty = 0;
  float E = ARENA_HALF + 3.0f;
  SV.ext = E;
  // Cell ~20 cm on every map: 256 cells over a 54 m arena, 512 over a 110 m town.
  int n = E > 30.0f ? 512 : 256;
  if (n != SV.n) {
    if (SV.sv_tex) glDeleteTextures(1, &SV.sv_tex);
    SV.sv_tex = tex_make(n * SV_LAYERS, n * 2, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, SV.sv_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SV.sv_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { SV.ok = 0; return; }
    SV.n = n;
  }
  // HEIGHT. Straight down; higher is nearer, so LESS keeps the top surface.
  float rng = SV_YHI - SV_YLO;
  mat4 m = {0};
  m.m[0] = 1.0f / E;
  m.m[9] = 1.0f / E;
  m.m[6] = -2.0f / rng;
  m.m[14] = 1.0f + 2.0f * SV_YLO / rng;
  m.m[15] = 1.0f;
  glBindTexture(GL_TEXTURE_2D, SV.hf_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, SV.levels - 1);
  glBindFramebuffer(GL_FRAMEBUFFER, SV.hf_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SV.hf_tex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, SV.hf_rb);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { SV.ok = 0; return; }
  glViewport(0, 0, HF_TEX, HF_TEX);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // the floor is y = 0 and is not drawn
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDisable(GL_CULL_FACE);                // either winding: the TOP surface wins
  glDisable(GL_BLEND);
  glUseProgram(SV.prog_hf);
  glUniformMatrix4fv(uloc(SV.prog_hf, "uVP"), 1, GL_FALSE, m.m);
  glBindVertexArray(R.vao_world);
  glDrawArrays(GL_TRIANGLES, 0, R.world_cast_verts);
  // MAX mips.
  glDisable(GL_DEPTH_TEST);
  glBindVertexArray(R.vao_sky);
  glUseProgram(SV.prog_max);
  glUniform1i(uloc(SV.prog_max, "uTex"), 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
  for (int L = 1; L < SV.levels; L++) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, L - 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, L - 1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SV.hf_tex, L);
    int s = HF_TEX >> L;
    glViewport(0, 0, s, s);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, SV.levels - 1);
  // BAKE.
  glUseProgram(SV.prog_bake);
  glUniform1i(uloc(SV.prog_bake, "uHf"), 0);
  glUniform1f(uloc(SV.prog_bake, "uExt"), E);
  glUniform1f(uloc(SV.prog_bake, "uHfTexel"), 2.0f * E / (float)HF_TEX);
  glUniform1i(uloc(SV.prog_bake, "uN"), n);
  fs_tri(SV.sv_fbo, n * SV_LAYERS, n * 2);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, R.width, R.height);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

// Per frame, before the first world-program draw: unit 7 is nobody else's.
static void sv_bind(void) {
  if (SV.dirty || !SV.tried) { SV.tried = 1; sv_build(); }
  tex_on(7, SV.ok ? SV.sv_tex : 0);
  glUseProgram(R.prog_world);
  glUniform1i(uloc(R.prog_world, "uSv"), 7);
  glUniform4f(uloc(R.prog_world, "uSvXf"), 0.5f / SV.ext, 1.0f / (float)(SV.n ? SV.n : 1),
              SV.ok ? 1.0f : 0.0f, (float)g_sv_dbg);
}
