// code/render/atmos.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Atmospheres. Each arena gets one, picked from the map seed so every player in
// a match sees the same sky. Purely presentation: the sim never reads any of it.
// All five stay bright enough to read a figure against the ground.
// ---------------------------------------------------------------------------
typedef struct {
  const char *name;
  float sun[3], sun_col[3], hor[3], top[3], gnd[3], glow[3];
  float exposure, amb, fog, cloud;
  float bloom, sat, con, vig, tint[3], lift[3];
} atm_t;
static const atm_t ATM[] = {
  {"HIGH NOON",   {0.62f, 0.70f, 0.35f}, {3.25f, 2.86f, 2.24f}, {0.74f, 0.90f, 1.16f},
   {0.06f, 0.18f, 0.48f}, {0.40f, 0.35f, 0.28f}, {0.10f, 0.08f, 0.05f},
   0.36f, 1.05f, 7.0e-5f, 0.30f, 0.34f, 1.12f, 0.34f, 0.24f,
   {1.01f, 1.00f, 0.98f}, {0.000f, 0.002f, 0.006f}},
  {"GOLDEN HOUR", {0.80f, 0.31f, 0.52f}, {3.55f, 2.42f, 1.32f}, {1.34f, 1.12f, 0.84f},
   {0.56f, 0.50f, 0.54f}, {0.42f, 0.33f, 0.26f}, {0.95f, 0.46f, 0.18f},
   0.36f, 1.35f, 8.0e-5f, 0.34f, 0.42f, 1.10f, 0.34f, 0.26f,
   {1.02f, 0.99f, 0.96f}, {0.006f, 0.003f, 0.006f}},
  {"EMBER DUSK",  {-0.72f, 0.24f, 0.66f}, {3.50f, 1.85f, 0.95f}, {0.82f, 0.50f, 0.46f},
   {0.08f, 0.10f, 0.29f}, {0.34f, 0.25f, 0.26f}, {1.10f, 0.34f, 0.14f},
   0.44f, 1.80f, 1.0e-4f, 0.38f, 0.46f, 1.08f, 0.34f, 0.28f,
   {1.02f, 0.98f, 0.99f}, {0.012f, 0.006f, 0.016f}},
  {"COLD DAWN",   {-0.34f, 0.29f, -0.90f}, {3.00f, 2.40f, 2.20f}, {0.76f, 0.80f, 0.98f},
   {0.10f, 0.17f, 0.38f}, {0.30f, 0.30f, 0.34f}, {0.56f, 0.34f, 0.36f},
   0.45f, 1.70f, 1.0e-4f, 0.32f, 0.36f, 1.04f, 0.32f, 0.26f,
   {0.98f, 0.99f, 1.03f}, {0.002f, 0.006f, 0.014f}},
  {"STORMFRONT",  {0.46f, 0.64f, -0.62f}, {1.95f, 1.98f, 2.10f}, {0.60f, 0.66f, 0.76f},
   {0.15f, 0.19f, 0.28f}, {0.32f, 0.32f, 0.34f}, {0.05f, 0.06f, 0.08f},
   0.46f, 2.00f, 1.3e-4f, 0.68f, 0.22f, 0.96f, 0.38f, 0.30f,
   {0.98f, 1.00f, 1.02f}, {0.002f, 0.006f, 0.010f}},
  // BLUE HOUR: the sun just under the dunes, the lamps lit. COMPOUND's own —
  // the arena's seed pick below never reaches it.
  {"BLUE HOUR",   {0.52f, 0.15f, -0.84f}, {1.55f, 0.95f, 0.68f}, {0.46f, 0.46f, 0.66f},
   {0.035f, 0.07f, 0.22f}, {0.20f, 0.19f, 0.24f}, {0.70f, 0.34f, 0.26f},
   0.56f, 1.70f, 8.0e-5f, 0.26f, 0.48f, 1.02f, 0.34f, 0.32f,
   {0.97f, 0.99f, 1.04f}, {0.004f, 0.008f, 0.020f}},
};
#define ATM_COUNT ((int)(sizeof ATM / sizeof ATM[0]))
#define ATM_ARENA_N 5   // the presets ARENA draws from (BLUE HOUR is COMPOUND's)
static const atm_t *atm_now(void) {
  int ma = MAP_DEFS[map_id_valid(g_map_id)].atm;   // a handmade map's own sky
  if (ma >= 0 && ma < ATM_COUNT) return &ATM[ma];
  // A Murmur-style mix: map seeds step by a constant, which would otherwise walk
  // the presets in lockstep.
  uint32_t h = g_map_seed ^ 0x9e3779b9u;
  h ^= h >> 16; h *= 0x85ebca6bu; h ^= h >> 13; h *= 0xc2b2ae35u; h ^= h >> 16;
  return &ATM[h % (uint32_t)ATM_ARENA_N];
}
static v3 atm_sun(void) {
  const atm_t *a = atm_now();
  return v3_norm((v3){{a->sun[0], a->sun[1], a->sun[2]}});
}

typedef struct { GLint sun, col, hor, top, gnd, glow, exp, amb, fog, cloud; } atm_loc_t;

static const char *WORLD_VS =
  "#version 330 core\n"
  "layout(location=0) in vec3 aPos;\n"
  "layout(location=1) in vec3 aNrm;\n"
  "layout(location=2) in vec3 aCol;\n"
  "layout(location=3) in vec3 aMat;\n"   // roughness, metalness, ambient occl.
  "uniform mat4 uVP;\n"
  "uniform mat4 uLightVP;\n"
  "uniform float uSmTexel;\n"            // shadow texel size in world metres
  "out vec3 vNrm; out vec3 vCol; out vec3 vWorld; out vec3 vMat; out vec4 vLit;\n"
  // The shadow lookup is offset ALONG THE NORMAL by a texel and a half rather
  // than biased in depth: a depth bias has to be tuned per slope and leaks light
  // under contact points at grazing angles ("peter-panning"), while a normal
  // offset moves the sample sideways out of the surface's own shadow and needs
  // no slope term at all.
  "void main(){ vNrm=aNrm; vCol=aCol; vMat=aMat; vWorld=aPos;\n"
  "  vLit = uLightVP * vec4(aPos + normalize(aNrm) * uSmTexel * 1.7, 1.0);\n"
  "  gl_Position=uVP*vec4(aPos,1.0); }\n";

// Depth-only pass that fills the shadow map. Same vertex layout as the world, so
// it draws the very same VBOs — the caster list is "everything solid", with no
// second copy of the geometry to keep in sync.
static const char *DEPTH_VS =
  "#version 330 core\n"
  "layout(location=0) in vec3 aPos;\n"
  "uniform mat4 uVP;\n"
  "void main(){ gl_Position=uVP*vec4(aPos,1.0); }\n";
static const char *DEPTH_FS =
  "#version 330 core\n"
  "void main(){}\n";

// One metal/dielectric BRDF for everything solid: GGX specular + Smith
// visibility, F0 from metalness, and — the term that actually sells metal — an
// environment reflection sampled from the same sky the player can see. A metal
// lit by a sun highlight alone renders as black plastic; what makes it read as
// steel is that its whole surface mirrors the sky gradient.
// Surface detail — stable world-anchored micro roughness, panel seams on tall
// faces, ground checker/grid — is gated by uDetail so the static world gets it
// but moving geometry (bots, VFX, viewmodel) stays calm: world-anchored noise
// would shimmer on anything that moves through it.
// uEmis short-circuits the whole rig for the additive FX pass (tracers, sparks,
// muzzle glow), which wants its vertex color on screen untouched.
static const char *WORLD_FS =
  "#version 330 core\n"
  GLSL_ENV
  "in vec3 vNrm; in vec3 vCol; in vec3 vWorld; in vec3 vMat; in vec4 vLit;\n"
  "uniform vec3 uEye;\n"
  "uniform float uDetail;\n"
  "uniform float uEmis;\n"
  "uniform float uShadow;\n"              // 0 = no shadow map bound
  "uniform float uVm;\n"                  // 1 = viewmodel: add the hero rig
  "uniform vec3 uMuzC;\n"                 // muzzle-flash radiance (0 = no shot)
  "uniform vec3 uMuzL;\n"                 // direction from the gun to the flash
  "uniform vec3 uMuzP;\n"                 // ...and where it is, for the WORLD
  "uniform vec3 uFill, uRim;\n"
  // The weapon's OWN frame, published by vm_build once per viewmodel pass. See
  // the micro-structure block in main() for why a finish cannot be anchored in
  // world space on something the player carries.
  "uniform vec3 uMdlO, uMdlX, uMdlZ;\n"   // no uMdlY: nothing reads an up axis
  // THE FLOOR'S OWN TREATMENT, one vec4: ripple strength, glitter strength,
  // standing-water level and the wind heading. A THEME property rather than a
  // global look, because the single thing that would undo every other detail in
  // the frame is a puddle in a desert. See surf_kind_t.
  "uniform vec4 uSurf;\n"
  "uniform vec2 uYard;\n"               // yard paint: arena half-size, on (see YARD)
  "uniform int uTown;\n"
  "uniform vec3 uWrist;\n"
  "uniform vec3 uWristOn;\n"            // wrist display's lit colour (gear LED)
  // WEAPON THEME (see WPN_THEMES): x = pattern + 1, 0 is the factory finish and
  // skips the block entirely; y = pattern scale per metre; z = metalness the
  // themed paint takes on (gold); w unused. A..C are the pattern's display
  // colours, D the tint for the dark furniture.
  "uniform vec4 uSkin;\n"
  "uniform vec3 uSkinA, uSkinB, uSkinC, uSkinD;\n"
  "uniform float uLampOn;\n"             // 0 by day .. 1 at dusk: lanterns lit
  "uniform vec4 uLamp[8];\n"             // the nearest lanterns: position, power               // wrist display: ammo, magazine, clock                 // 0 citadel, 1 harbor, 2 refinery, -1 none
  "uniform vec2 uSmStep;\n"               // one shadow texel in UV
  "uniform sampler2DShadow uSm;\n"
  "out vec4 oCol;\n"
  // 4x4 taps of a hardware-compared shadow map: each tap is itself bilinear, so
  // sixteen of them at half-texel spacing give a penumbra about three texels
  // wide — enough that a 48 m arena in one map still reads as a soft edge
  // instead of a staircase.
  // Smooth value noise, one and two dimensional. Interpolated rather than
  // hashed per cell because every consumer below is at a scale where a cell is
  // several pixels across: a hard-edged cell reads as a checker or as speckle,
  // and both of those are the opposite of the surface finish they stand in for.
  "float vhash1(float x){ return fract(sin(x * 127.1) * 43758.5453); }\n"
  "float vnoise1(float x){\n"
  "  float i = floor(x), f = fract(x);\n"
  "  f = f * f * (3.0 - 2.0 * f);\n"
  "  return mix(vhash1(i), vhash1(i + 1.0), f);\n"
  "}\n"
  // PERCENTAGE-CLOSER SOFT SHADOWS. A real penumbra widens with the distance
  // from the caster: the foot of a wall casts a razor line, its parapet ten
  // metres up a soft one. Step 1 finds the average BLOCKER depth over a disc
  // (the raw depths, through a second sampler on the same texture with the
  // compare switched off); step 2 sizes the filter from the receiver-blocker
  // gap and runs a hardware-compared Poisson filter that wide. The disc is a
  // FIXED pattern, not rotated per pixel: with no temporal filter to average
  // it, a rotated disc is grain, and sixteen bilinear-compared taps on a fixed
  // one are smooth. uSmPen turns depth-units of gap into uv of penumbra (the
  // sun's 0.53 degrees, widened for sky scatter).
  "uniform sampler2D uSmRaw;\n"
  "uniform float uSmPen;\n"
  "const vec2 PD[16] = vec2[](\n"
  "  vec2(-0.9420, -0.3991), vec2(0.9456, -0.7689), vec2(-0.0942, -0.9294), vec2(0.3450, 0.2939),\n"
  "  vec2(-0.9159, 0.4577), vec2(-0.8154, -0.8791), vec2(-0.3828, 0.2768), vec2(0.9748, 0.7565),\n"
  "  vec2(0.4432, -0.9751), vec2(0.5374, -0.4737), vec2(-0.2650, -0.4189), vec2(0.7920, 0.1909),\n"
  "  vec2(-0.2419, 0.9971), vec2(-0.8141, 0.9144), vec2(0.1998, 0.7864), vec2(0.1438, -0.1410));\n"
  "float sun_shadow(){\n"
  "  if (uShadow < 0.5) return 1.0;\n"
  "  vec3 p = vLit.xyz / vLit.w;\n"
  "  if (p.z >= 1.0) return 1.0;\n"
  "  p = p * 0.5 + 0.5;\n"
  "  float srch = uSmStep.x * 9.0, bsum = 0.0, bn = 0.0;\n"
  "  for (int i = 0; i < 16; i += 2) {\n"
  "    float b = texture(uSmRaw, p.xy + PD[i] * srch).r;\n"
  "    if (b < p.z - 0.0004) { bsum += b; bn += 1.0; }\n"
  "  }\n"
  "  if (bn < 0.5) return 1.0;\n"     // nothing between this point and the sun
  "  float gap = p.z - bsum / bn;\n"
  "  float w = clamp(gap * uSmPen, uSmStep.x * 1.2, uSmStep.x * 9.0);\n"
  "  float s = 0.0;\n"
  "  for (int i = 0; i < 16; i++) s += texture(uSm, vec3(p.xy + PD[i] * w, p.z));\n"
  "  return s * 0.0625;\n"
  "}\n"
  // --- BUILDING MATERIALS ----------------------------------------------------
  // Every box in the world used to be one flat albedo with a little grain on it,
  // and that, more than the lighting, is what read as a block-out: a warehouse
  // and a crate were the same surface at different sizes. Each material is a
  // HEIGHT field (relief, turned into a normal by finite differences) plus an
  // APPEARANCE (albedo multiplier, roughness offset), both functions of a
  // world-anchored planar coordinate, so a facade built from forty boxes still
  // carries one continuous brick bond across all of them.
  // THE FOOTPRINT RULE from the floor applies here unchanged: every term fades
  // itself out on `fp` (metres per pixel) before its own wavelength reaches
  // Nyquist and hands what it lost to the average, so a wall is detailed at arm's
  // length and dead calm at forty metres instead of crawling.
  "float mfade(float lam, float fp) { return 1.0 - smoothstep(lam * 0.16, lam * 0.45, fp); }\n"
  // Distance to the nearest joint of a running-bond grid, in metres, and the cell
  // id. `w`, `h`: unit size including the joint; odd rows shift by `sh` of a unit.
  "vec3 bond(vec2 uv, float w, float h, float sh) {\n"
  "  float row = floor(uv.y / h);\n"
  "  float uu = uv.x / w + sh * mod(row, 2.0);\n"
  "  float col = floor(uu);\n"
  "  vec2 f = vec2(fract(uu) * w, fract(uv.y / h) * h);\n"
  "  float dm = min(min(f.x, w - f.x), min(f.y, h - f.y));\n"
  "  return vec3(dm, col, row);\n"
  "}\n"
  // Ashlar: courses of one height, blocks of a length that varies per course.
  "vec3 ashlar(vec2 uv) {\n"
  "  float h = 0.34;\n"
  "  float row = floor(uv.y / h);\n"
  "  float w = 0.62 + 0.30 * vhash2(vec2(row, 3.7));\n"
  "  float uu = uv.x / w + vhash2(vec2(row, 9.1));\n"
  "  float col = floor(uu);\n"
  "  vec2 f = vec2(fract(uu) * w, fract(uv.y / h) * h);\n"
  "  float dm = min(min(f.x, w - f.x), min(f.y, h - f.y));\n"
  "  return vec3(dm, col, row);\n"
  "}\n"
  // A weld bead: welded plate and pipe are joined in rings, one every 2 m
  // along the run and one per 2.4 m course up a column. A raised, rippled bead.
  "float weld(vec2 uv){\n"
  "  float a = abs(fract(uv.x / 2.0) - 0.5) * 2.0, b = abs(fract(uv.y / 2.4) - 0.5) * 2.4;\n"
  "  float d = min(a, b);\n"
  "  return (1.0 - smoothstep(0.004, 0.011, d)) * (0.8 + 0.2 * sin((uv.x + uv.y) * 900.0));\n"
  "}\n"
  "float fbm4(vec2 p){ return vnoise2(p) * 0.52 + vnoise2(p * 2.07 + 3.1) * 0.26\n"
  "                         + vnoise2(p * 4.13 + 7.7) * 0.14 + vnoise2(p * 8.21 + 1.3) * 0.08; }\n"
  // HEIGHT, in metres, of material m at uv. Joints are recesses, panels are flat,
  // sheet is corrugated. Only the relief that survives the footprint is returned.
  "float mat_h(int m, vec2 uv, float fp) {\n"
  "  if (m == 2) {                                       // brick, 225 x 75 mm\n"
  "    vec3 b = bond(uv, 0.225, 0.075, 0.5);\n"
  "    return 0.006 * smoothstep(0.003, 0.009, b.x) * mfade(0.075, fp)\n"
  "         + 0.0012 * vnoise2(uv * 55.0) * mfade(0.018, fp);\n"
  "  }\n"
  "  if (m == 1) {                                       // lime render\n"
  "    float sp = smoothstep(0.745, 0.752, vnoise2(uv * 0.55 + 17.0 + vnoise2(uv * 7.0) * 0.05))\n"
  "             * (1.0 - smoothstep(0.4, 2.6, uv.y));\n"
  "    float h = 0.0016 * (vnoise2(uv * 24.0) * 0.6 + vnoise2(uv * 61.0) * 0.4)\n"
  "            * mfade(0.04, fp);\n"
  "    if (sp > 0.0) {                                   // spalled: mud brick behind\n"
  "      vec3 b = bond(uv, 0.31, 0.10, 0.5);\n"
  "      h -= sp * (0.012 - 0.004 * smoothstep(0.004, 0.010, b.x) * mfade(0.10, fp));\n"
  "    }\n"
  "    return h;\n"
  "  }\n"
  "  if (m == 3) {                                       // form-cast concrete\n"
  "    vec3 p = bond(uv, 2.4, 1.2, 0.0);\n"
  "    vec2 t = abs(fract(uv / 0.6) - 0.5) * 0.6;        // form-tie holes\n"
  "    float tie = 1.0 - smoothstep(0.008, 0.013, length(t));\n"
  "    return -0.005 * (1.0 - smoothstep(0.004, 0.010, p.x)) * mfade(0.08, fp)\n"
  "           - 0.006 * tie * mfade(0.05, fp)\n"
  "           + 0.0010 * vnoise2(uv * 40.0) * mfade(0.025, fp);\n"
  "  }\n"
  "  if (m == 4) {                                       // corrugated sheet, 76 mm\n"
  "    float s = sin(uv.x * 82.67) * 0.5 + 0.5;\n"
  "    float lf = fract(uv.y / 2.8);\n"
  "    float lap = 1.0 - smoothstep(0.006, 0.022, min(lf, 1.0 - lf) * 2.8);\n"
  "    return 0.0045 * s * mfade(0.076, fp) + 0.004 * lap * mfade(0.06, fp);\n"
  "  }\n"
  "  if (m == 5) {                                       // container: trapezoid rib\n"
  "    float t = fract(uv.x / 0.278);\n"
  "    float r = smoothstep(0.16, 0.30, t) - smoothstep(0.66, 0.80, t);\n"
  "    float yb = mod(uv.y, 2.6);\n"
  "    float rail = 1.0 - smoothstep(0.10, 0.13, yb) * (1.0 - smoothstep(2.44, 2.47, yb));\n"
  "    return 0.018 * r * (1.0 - rail) * mfade(0.278, fp);\n"
  "  }\n"
  "  if (m == 6) {                                       // dressed stone\n"
  "    vec3 b = ashlar(uv);\n"
  "    return 0.010 * smoothstep(0.004, 0.030, b.x) * mfade(0.30, fp)\n"
  "         + 0.0035 * vnoise2(uv * 9.0 + b.yz) * mfade(0.10, fp);\n"
  "  }\n"
  "  if (m == 7) {                                       // planking\n"
  "    vec3 b = bond(vec2(uv.x, uv.y), 1.2, 0.145, 0.37);\n"
  "    return 0.003 * smoothstep(0.002, 0.006, b.x) * mfade(0.145, fp);\n"
  "  }\n"
  "  if (m == 8) {                                       // copper: standing seams\n"
  "    float sx = abs(fract(uv.x / 0.46) - 0.5) * 0.46;\n"
  "    return 0.006 * (1.0 - smoothstep(0.004, 0.012, sx)) * mfade(0.46, fp)\n"
  "         + 0.0008 * vnoise2(uv * 40.0) * mfade(0.03, fp);\n"
  "  }\n"
  "  if (m == 9 || m == 10) {                            // steel: weld rings\n"
  "    float w = weld(uv);\n"
  "    return 0.0025 * w * mfade(0.05, fp) + 0.0004 * vnoise2(uv * 90.0) * mfade(0.01, fp);\n"
  "  }\n"
  "  return 0.0;\n"
  "}\n"
  // APPEARANCE: albedo (linear, in and out), roughness, and the metal flag a
  // painted sheet keeps at zero — paint is a dielectric whatever it is on.
  "void mat_look(int m, vec2 uv, float fp, inout vec3 alb, inout float rough) {\n"
  "  if (m == 2) {\n"
  "    vec3 b = bond(uv, 0.225, 0.075, 0.5);\n"
  "    vec3 r3 = vhash3(b.yz + vec2(0.37, 11.1));\n"
  "    float fb = mfade(0.075, fp);\n"
  // Per-brick value and a little hue: one in eight came out of the kiln dark.
  "    vec3 bt = vec3(0.86 + 0.26 * r3.x) * mix(vec3(1.0), vec3(0.80, 0.86, 0.95), step(0.875, r3.y));\n"
  "    bt *= 0.94 + 0.12 * vnoise2(uv * 18.0);\n"
  "    vec3 mortar = vec3(0.30, 0.285, 0.26) * (0.9 + 0.2 * vnoise2(uv * 30.0));\n"
  "    float mm = 1.0 - smoothstep(0.003, 0.006 + fp * 0.6, b.x);\n"
  "    vec3 det = mix(alb * bt, mortar, mm);\n"
  "    vec3 avg = mix(alb, mortar, 0.13);                 // what the bond averages to\n"
  "    alb = mix(avg, det, fb);\n"
  "    rough = clamp(mix(rough, 0.95, mm * fb) + 0.08, 0.045, 1.0);\n"
  "  } else if (m == 1) {\n"
  "    float bl = vnoise2(uv * 0.45) * 0.5 + vnoise2(uv * 2.3) * 0.3 + vnoise2(uv * 9.0) * 0.2;\n"
  "    alb *= 0.94 + 0.11 * bl;\n"
  "    float sp = smoothstep(0.745, 0.752, vnoise2(uv * 0.55 + 17.0 + vnoise2(uv * 7.0) * 0.05))\n"
  "             * (1.0 - smoothstep(0.4, 2.6, uv.y));\n"
  "    if (sp > 0.0) {\n"
  "      vec3 b = bond(uv, 0.31, 0.10, 0.5);\n"
  "      float mm = (1.0 - smoothstep(0.004, 0.008, b.x)) * mfade(0.10, fp);\n"
  "      vec3 mud = alb * vec3(0.78, 0.70, 0.60) * (0.88 + 0.24 * vhash2(b.yz));\n"
  "      alb = mix(alb, mix(mud, mud * 0.7, mm), sp);\n"
  // The broken edge of the render, in its own shadow.
  "      alb *= 1.0 - 0.45 * sp * (1.0 - sp) * 4.0 * mfade(0.03, fp);\n"
  "    }\n"
  // Hairline cracks in the render: the zero set of a warped noise.
  "    float cw = mfade(0.05, fp);\n"
  "    if (cw > 0.01) {\n"
  "      float cn = vnoise2(uv * 1.7 + vnoise2(uv * 6.0) * 0.6);\n"
  "      float cr = 1.0 - smoothstep(0.002, 0.007, abs(cn - 0.5));\n"
  "      alb *= 1.0 - 0.16 * cr * cw * smoothstep(0.62, 0.85, vnoise2(uv * 0.4 + 5.0));\n"
  "    }\n"
  "    rough = clamp(rough + 0.10, 0.045, 1.0);\n"
  "  } else if (m == 3) {\n"
  "    vec3 p = bond(uv, 2.4, 1.2, 0.0);\n"
  "    float pv = vhash2(p.yz + vec2(4.1, 1.3));\n"
  "    alb *= 0.95 + 0.10 * pv;\n"
  "    alb *= 0.92 + 0.16 * (vnoise2(uv * 1.3) * 0.6 + vnoise2(uv * 5.0) * 0.4);\n"
  // Pour lines: the lifts of a wall cast in stages leave faint horizontal bands.
  "    alb *= 1.0 - 0.05 * smoothstep(0.3, 0.9, vnoise2(vec2(uv.x * 0.3, uv.y * 4.0)));\n"
  "    float jm = (1.0 - smoothstep(0.004, 0.010 + fp * 0.6, p.x)) * mfade(0.08, fp);\n"
  "    vec2 t = abs(fract(uv / 0.6) - 0.5) * 0.6;\n"
  "    float tie = (1.0 - smoothstep(0.008, 0.013, length(t))) * mfade(0.05, fp);\n"
  "    alb *= 1.0 - 0.32 * jm - 0.45 * tie;\n"
  "    rough = clamp(rough + 0.05, 0.045, 1.0);\n"
  "  } else if (m == 4) {\n"
  "    vec3 sh = bond(uv, 0.84, 2.8, 0.0);               // one sheet per cell\n"
  "    alb *= 0.93 + 0.12 * vhash2(sh.yz + vec2(2.2, 8.8));\n"
  // Rust bleeds down from every lap and up from the foot.
  "    float lapd = fract(uv.y / 2.8);\n"
  "    float run = vnoise2(vec2(uv.x * 9.0, uv.y * 0.7));\n"
  "    float rs = smoothstep(0.62, 0.90, run) * (1.0 - smoothstep(0.0, 0.55, lapd) * 0.7)\n"
  "             + smoothstep(0.55, 0.0, uv.y) * 0.6 * vnoise2(uv * 4.0);\n"
  "    rs = clamp(rs, 0.0, 1.0) * smoothstep(0.35, 0.65, vnoise2(uv * 0.35 + 3.0));\n"
  "    alb = mix(alb, vec3(0.16, 0.075, 0.035), rs * 0.55);\n"
  "    float s = sin(uv.x * 82.67) * 0.5 + 0.5;\n"
  "    alb *= 1.0 - 0.10 * (1.0 - s) * mfade(0.076, fp);  // the troughs hold dirt\n"
  "    rough = clamp(rough - 0.20 + 0.30 * rs, 0.30, 1.0);\n"
  "  } else if (m == 5) {\n"
  "    float yb = mod(uv.y, 2.6);\n"
  "    float rail = 1.0 - smoothstep(0.10, 0.13, yb) * (1.0 - smoothstep(2.44, 2.47, yb));\n"
  // Sun-faded paint, scuffs to primer, rust at the rails and the corners.
  "    alb *= 0.90 + 0.16 * vnoise2(uv * 0.7);\n"
  "    float sc = smoothstep(0.72, 0.86, vnoise2(uv * vec2(3.0, 9.0))) * mfade(0.06, fp);\n"
  "    alb = mix(alb, alb * 0.62 + vec3(0.03), sc * 0.6);\n"
  "    float rs = smoothstep(0.55, 0.85, vnoise2(uv * 2.2 + 7.0))\n"
  "             * (rail * 0.9 + 0.35 * (1.0 - smoothstep(0.0, 0.9, yb)));\n"
  "    alb = mix(alb, vec3(0.15, 0.07, 0.035), clamp(rs, 0.0, 1.0) * 0.7);\n"
  "    alb *= 1.0 - 0.18 * rail;\n"
  "    rough = clamp(rough - 0.22 + 0.25 * rs, 0.30, 1.0);\n"
  "  } else if (m == 6) {\n"
  "    vec3 b = ashlar(uv);\n"
  "    vec3 r3 = vhash3(b.yz + vec2(5.5, 2.5));\n"
  "    float fb = mfade(0.30, fp);\n"
  "    vec3 det = alb * (0.86 + 0.24 * r3.x) * (0.94 + 0.12 * vnoise2(uv * 7.0 + b.yz));\n"
  "    float mm = 1.0 - smoothstep(0.004, 0.009 + fp * 0.6, b.x);\n"
  "    det = mix(det, alb * 0.62, mm);\n"
  "    alb = mix(alb * 0.97, det, fb);\n"
  "    rough = clamp(rough + 0.08, 0.045, 1.0);\n"
  "  } else if (m == 7) {\n"
  "    vec3 b = bond(uv, 1.2, 0.145, 0.37);\n"
  "    vec3 r3 = vhash3(b.yz + vec2(1.9, 6.4));\n"
  "    float fb = mfade(0.145, fp);\n"
  "    float grain = vnoise2(vec2(uv.x * 2.5, uv.y * 70.0) + r3.xy * 40.0) * mfade(0.02, fp);\n"
  "    vec3 det = alb * (0.84 + 0.28 * r3.x) * (0.92 + 0.14 * grain);\n"
  "    float gap = 1.0 - smoothstep(0.002, 0.005 + fp * 0.6, b.x);\n"
  "    det = mix(det, alb * 0.35, gap);\n"
  "    alb = mix(alb * 0.97, det, fb);\n"
  "    rough = clamp(rough + 0.04, 0.045, 1.0);\n"
  "  } else if (m == 8) {\n"
  // VERDIGRIS. Copper that has weathered for decades is not one green: the
  // patina is thickest and palest where rain runs and pools (the lower courses,
  // the seams), and the brown metal still shows where it is sheltered.
  "    float run = fbm4(vec2(uv.x * 2.2, uv.y * 0.35));\n"
  "    float pat = smoothstep(0.30, 0.75, run + (1.0 - smoothstep(0.0, 4.0, uv.y - 7.6)) * 0.25);\n"
  "    vec3 bronze = vec3(0.20, 0.11, 0.06), green = vec3(0.24, 0.46, 0.52) * vec3(1.05, 1.15, 0.95);\n"
  "    vec3 pale = vec3(0.42, 0.62, 0.58);\n"
  "    vec3 c = mix(bronze, mix(green, pale, smoothstep(0.55, 0.95, run)), pat);\n"
  "    alb = c * c * (0.90 + 0.20 * vnoise2(uv * 6.0));\n"
  "    rough = clamp(mix(0.35, 0.80, pat), 0.045, 1.0);\n"
  "  } else if (m == 9) {\n"
  // Weathered steel: mill-scale mottle, brushed along the run, the weld rings
  // heat-tinted, and rust bleeding down from each of them.
  "    float w = weld(uv);\n"
  "    float mot = fbm4(uv * 1.4);\n"
  "    float brush = vnoise2(vec2(uv.x * 3.0, uv.y * 240.0)) * mfade(0.01, fp);\n"
  "    alb *= 0.88 + 0.22 * mot + 0.06 * brush;\n"
  "    alb = mix(alb, alb * vec3(0.62, 0.52, 0.48), w * 0.6);\n"
  "    float bleed = smoothstep(0.62, 0.92, vnoise2(vec2(uv.x * 7.0, uv.y * 0.5))) * smoothstep(0.7, 0.0, fract(uv.y / 2.4));\n"
  "    alb = mix(alb, vec3(0.13, 0.06, 0.03), bleed * 0.35);\n"
  "    rough = clamp(rough - 0.25 + 0.2 * mot + 0.3 * bleed - 0.08 * brush, 0.18, 1.0);\n"
  "  } else if (m == 10) {\n"
  // HAZARD PAINT: safety yellow with 45-degree black bands on the ends of a
  // run, chipped to primer and rusty steel wherever it has been knocked.
  "    float d = uv.x + uv.y;\n"
  "    float band = step(0.5, fract(d / 0.30)) * step(0.82, vnoise2(vec2(floor(uv.x / 3.0), 7.0)) + 0.4);\n"
  "    alb = mix(alb, vec3(0.012), band * mfade(0.15, fp));\n"
  // Chips gather where paint is knocked: low on a post, and in clusters.
  "    float cn = fbm4(uv * 5.5) + vnoise2(uv * 31.0) * 0.10 + fbm4(uv * 0.6) * 0.18 - 0.09;\n"
  "    float chip = smoothstep(0.80, 0.84, cn);\n"
  "    float rust = smoothstep(0.86, 0.92, cn);\n"
  "    alb = mix(alb, vec3(0.16, 0.16, 0.15), chip);\n"
  "    alb = mix(alb, vec3(0.14, 0.065, 0.03), rust);\n"
  "    alb *= 0.92 + 0.12 * vnoise2(uv * 0.8);\n"
  "    rough = clamp(rough - 0.12 + 0.25 * chip, 0.25, 1.0);\n"
  "  }\n"
  // --- PER-TOWN WEATHERING, over whatever the material is ---------------
  // Citadel: sun-baked sandstone grain in every masonry face. Harbor: salt
  // efflorescence creeping up from the foot of every wall, worst on steel.
  "  if (uTown == 0 && (m == 6 || m == 1 || m == 3)) {\n"
  "    float sf = fbm4(uv * 3.3) * 0.6 + fbm4(uv * 11.0 + 4.0) * 0.4 * mfade(0.03, fp);\n"
  "    alb *= 0.90 + 0.22 * sf;\n"
  "    alb = mix(alb, alb * vec3(1.08, 0.96, 0.84), smoothstep(0.55, 0.85, fbm4(uv * 0.5 + 9.0)) * 0.5);\n"
  "  }\n"
  "  if (uTown == 1) {\n"
  "    float salt = smoothstep(0.55, 0.85, fbm4(uv * 2.6) + (1.0 - smoothstep(0.0, 1.4, uv.y)) * 0.45)\n"
  "               * (1.0 - smoothstep(0.4, 2.0, uv.y)) * ((m == 4 || m == 5 || m == 9) ? 1.0 : 0.6);\n"
  "    alb = mix(alb, vec3(0.62, 0.62, 0.60), salt * 0.45 * mfade(0.03, fp));\n"
  "    rough = mix(rough, 1.0, salt * 0.6);\n"
  "  }\n"
  "}\n"
  // A seven-segment digit in a cell of half-extent (0.5, 1): bit order abcdefg.
  "float seg7(vec2 p, int d){\n"
  "  int m[10] = int[](0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F);\n"
  "  int b = m[clamp(d, 0, 9)];\n"
  "  if (abs(p.x) > 0.62 || abs(p.y) > 1.08) return 0.0;\n"
  "  float r = 0.0;\n"
  "  vec2 H = vec2(0.30, 0.075), V = vec2(0.075, 0.36);\n"
  "  vec2 q[7] = vec2[](vec2(0.0, 0.9), vec2(0.40, 0.45), vec2(0.40, -0.45), vec2(0.0, -0.9),\n"
  "                     vec2(-0.40, -0.45), vec2(-0.40, 0.45), vec2(0.0, 0.0));\n"
  "  for (int i = 0; i < 7; i++) {\n"
  "    if ((b & (1 << i)) == 0) continue;\n"
  "    vec2 e = (i == 0 || i == 3 || i == 6) ? H : V;\n"
  "    vec2 dd = abs(p - q[i]) - e;\n"
  "    float sd = length(max(dd, 0.0)) + min(max(dd.x, dd.y), 0.0);\n"
  "    r = max(r, 1.0 - smoothstep(0.0, 0.05, sd));\n"
  "  }\n"
  "  return r;\n"
  "}\n"
  // Cellular (Worley) F1 distance: the pitted micro-texture of a moulded
  // polymer, which is a field of tiny craters rather than a smooth noise.
  "float worley2(vec2 p){\n"
  "  vec2 i = floor(p), f = fract(p); float d = 1.0;\n"
  "  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {\n"
  "    vec2 g = vec2(float(x), float(y));\n"
  "    vec2 o = vhash3(i + g).xy;\n"
  "    vec2 r = g + o - f; d = min(d, dot(r, r));\n"
  "  }\n"
  "  return sqrt(d);\n"
  "}\n"
  // Bump from a scalar height through screen-space derivatives (Mikkelsen):
  // no tangent frame needed, so it works on every part of every model.
  "vec3 bumpn(vec3 n, float h, float k){\n"
  "  vec3 dpx = dFdx(vWorld), dpy = dFdy(vWorld);\n"
  "  vec3 r1 = cross(dpy, n), r2 = cross(n, dpx);\n"
  "  float det = dot(dpx, r1);\n"
  "  vec3 g = sign(det) * (dFdx(h) * r1 + dFdy(h) * r2);\n"
  "  return normalize(abs(det) * n - g * k);\n"
  "}\n"
  "void main(){\n"
  // 2 = MULTIPLY (smoke): a transmittance, not a colour — it passes untouched.
  // 1 = additive glow, authored as display colour: lifted back into radiance.
  "  if (uEmis > 1.5) { oCol = vec4(vCol, 1.0); return; }\n"
  "  if (uEmis > 0.5) { oCol = vec4(display_in(vCol) * 1.35, 1.0); return; }\n"
  "  vec3 n = normalize(vNrm);\n"
  "  vec3 alb = vCol * vCol;\n"                 // authored display -> linear
  "  vec3 dv = vWorld - uEye;\n"
  "  float d = length(dv);\n"
  "  vec3 V = -dv / d;\n"
  "  float det = uDetail * exp(-d * 0.035);\n"  // detail fades out with range
  // GATED ON uDetail, WHICH IS ZERO FOR EVERY FIGURE AND THE WHOLE VIEWMODEL.
  // uDetail has three writers: the sky pass sets 0, the two world draws set 1.
  // So scene_draw_vp (all figures, corpses, marks) and the viemodel pass ran
  // this block with uDetail == 0, where every term below is an exact identity
  // (alb *= 1.0, vMat.x + 0.0) — roughly 120 scalar ops including a
  // transcendental, on about a quarter of the frame's shaded fragments.
  "  float rough;\n"
  "  if (uDetail > 0.0) {\n"
  "  vec3 cell = floor(vWorld * 14.0);\n"       // ~7 cm grain, world-anchored
  "  float h = fract(sin(dot(cell, vec3(127.1, 311.7, 74.7))) * 43758.5453);\n"
  // Two octaves, not one: a single 7 cm grain is film grain, and what makes
  // concrete read as concrete is patchiness a hand's width across UNDER that
  // grain. The coarse octave also breaks the roughness, so the sheen on a wall
  // stops being one unbroken sweep.
  // INTERPOLATED, not celled. This octave is 59 cm across and it was a HARD cell
  // hash, i.e. a mosaic of rectangular value patches on a regular grid — laid
  // over every wall and the whole floor, with no courses and no gravity, which
  // reads as per-quad randomisation rather than as a material. It was survivable
  // when the floor had nothing else on it; against the new surface work it is
  // the most visible repetition artifact in the frame, and the mid-scale mottle
  // beats against it instead of hiding it. Projected onto whichever plane the
  // face lies in, which needs no UV set and is the same choice the seam term
  // below already makes.
  "  vec2 cp2 = abs(n.y) > 0.5 ? vWorld.xz\n"
  "           : (abs(n.x) > 0.5 ? vWorld.zy : vWorld.xy);\n"
  "  float h2 = vnoise2(cp2 * 1.7);\n"
  "  alb *= 1.0 + (h - 0.5) * 0.075 * det + (h2 - 0.5) * 0.085 * uDetail;\n"
  "  rough = clamp(vMat.x + (h2 - 0.5) * 0.16 * uDetail, 0.045, 1.0);\n"
  "  } else rough = clamp(vMat.x, 0.045, 1.0);\n"
  "  float metal = vMat.y, ao = vMat.z;\n"
  // THE STATIC WORLD'S MATERIAL, out of the AO channel's integer part (see
  // mesh_push). Decoded only under uDetail: nothing else carries one.
  "  int smat = 0;\n"
  "  if (uDetail > 0.0 && ao > 1.5) {\n"
  "    float mq = floor(ao * 0.25 + 1e-3);\n"
  "    smat = int(mq); ao -= mq * 4.0;\n"
  "  }\n"
  // GLASS, LANTERNS AND SIGNS. Not relief materials at all: a window is a
  // mirror over a room, a lantern and an LED are light sources.
  "  vec3 emi = vec3(0.0);\n"
  "  if (smat >= 13) {\n"
  "    float on = smat == 14 ? 1.0 : uLampOn;\n"
  "    if (on > 0.02) {\n"
  "      float k = smat == 14 ? 6.5 : 11.0;\n"
  "      vec3 e = alb * mix(0.6, k, on);\n"
  "      e = mix(e, sky_env(-V, 0.0), 1.0 - exp(-d * d * uFog));\n"
  "      oCol = vec4(tonemap(e), 1.0); return;\n"
  "    }\n"
  "  }\n"
  "  if (smat >= 11) {\n"
  "    vec3 tint = alb;\n"
  "    alb = vec3(0.004); rough = 0.035; metal = 0.0;\n"
  "    vec3 ambi = mix(SKY_GND * 0.44, mix(SKY_HOR, SKY_TOP, 0.45) * 0.40, 0.5) * uAmb;\n"
  "    if (smat == 11 && abs(n.y) < 0.5) {\n"
  // INTERIOR MAPPING: the window is a portal into a room that is not modelled.
  // The view ray is intersected, in the shader, with the walls, floor and
  // ceiling of an imaginary room behind the facade — one room per bay and
  // storey — so every window has depth, a back wall, parallax as the player
  // moves, and a different room from its neighbour. Some rooms have their
  // lights on at dusk; some have the blind half down.
  "      vec3 T = abs(n.x) > 0.5 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);\n"
  "      vec3 rs = vec3(uTown == 0 ? 2.7 : 3.4, 3.2, 3.6);\n"
  "      vec3 o = vec3(dot(vWorld, T) / rs.x, vWorld.y / rs.y, 0.0);\n"
  "      vec2 cell = floor(o.xy); vec2 f = fract(o.xy);\n"
  "      vec3 rd = normalize(vWorld - uEye);\n"
  "      vec3 d = vec3(dot(rd, T) / rs.x, rd.y / rs.y, max(dot(rd, -n), 0.05) / rs.z);\n"
  "      float tx = d.x > 0.0 ? (1.0 - f.x) / d.x : -f.x / min(d.x, -1e-4);\n"
  "      float ty = d.y > 0.0 ? (1.0 - f.y) / d.y : -f.y / min(d.y, -1e-4);\n"
  "      float tz = 1.0 / d.z;\n"
  "      float t = min(tz, min(tx, ty));\n"
  "      vec3 h = vec3(f, 0.0) + d * t;\n"
  "      vec3 r3 = vhash3(cell + vec2(float(uTown) * 7.3, 1.7));\n"
  "      vec3 wall = mix(vec3(0.62, 0.56, 0.46), vec3(0.46, 0.52, 0.56), r3.x) * (0.75 + 0.25 * r3.z);\n"
  "      vec3 c;\n"
  "      if (t == ty) c = d.y < 0.0 ? vec3(0.30, 0.22, 0.15) : vec3(0.78, 0.76, 0.72);\n"
  "      else if (t == tx) c = wall * 0.85;\n"
  "      else {\n"
  "        c = wall;\n"
  // A dark piece of furniture against the back wall, a picture above it.
  "        if (h.y < 0.32 && abs(h.x - 0.3 - 0.4 * r3.z) < 0.22) c *= 0.35;\n"
  "        if (h.y > 0.48 && h.y < 0.66 && abs(h.x - 0.5) < 0.12) c = mix(c, vec3(0.35, 0.22, 0.18), 0.7);\n"
  "      }\n"
  "      c *= 1.0 - 0.45 * h.z;\n"                         // deeper is darker
  "      float lit = step(0.70, r3.y) * uLampOn;\n"
  "      vec3 light = mix(ambi * 0.55, vec3(1.0, 0.70, 0.42) * 1.6, lit);\n"
  "      if (t == ty && d.y > 0.0) light += vec3(1.0, 0.8, 0.55) * 1.2 * lit;\n"   // the ceiling lamp
  "      vec3 room = c * c * light;\n"
  // The blind: drawn on the glass, so it does not parallax.
  "      float bl = r3.z * 0.55;\n"
  "      if (f.y > 1.0 - bl) room = mix(room, vec3(0.62, 0.56, 0.46) * mix(ambi * 0.7, vec3(1.0, 0.75, 0.5) * 0.9, lit), 0.92);\n"
  "      emi = room;\n"
  "    } else {\n"
  "      emi = tint * tint * ambi * 0.35;\n"                // a vehicle's dark cabin
  "    }\n"
  "  }\n"
  "  if (smat > 0 && smat < 11 && n.y > -0.5) {\n"
  "    bool top = n.y > 0.5;\n"
  "    vec3 T = top ? vec3(1.0, 0.0, 0.0) : (abs(n.x) > 0.5 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0));\n"
  "    vec3 B = top ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);\n"
  "    vec2 uv = vec2(dot(vWorld, T), dot(vWorld, B));\n"
  // A brick or a rendered wall is capped, not bricked, on top.
  "    int m = (top && (smat == 2 || smat == 1)) ? 3 : smat;\n"
  "    vec3 dwx = dFdx(vWorld), dwy = dFdy(vWorld);\n"
  "    float fpw = sqrt(max(dot(dwx, dwx), dot(dwy, dwy)));\n"
  "    mat_look(m, uv, fpw, alb, rough);\n"
  // Relief by forward differences at about a pixel: a smaller step resolves
  // detail the footprint fade has already removed, a larger one blurs the joint.
  "    float e = max(fpw * 0.75, 0.0015);\n"
  "    float h0 = mat_h(m, uv, fpw);\n"
  "    float hu = (mat_h(m, uv + vec2(e, 0.0), fpw) - h0) / e;\n"
  "    float hv = (mat_h(m, uv + vec2(0.0, e), fpw) - h0) / e;\n"
  "    n = normalize(n - T * hu - B * hv);\n"
  "  }\n"
  // Negative metalness flags the viewmodel sleeves. The pattern is anchored to
  // the weapon's frame, so it rides with the arms instead of crawling over them.
  "  if (metal < -0.5) {\n"
  "    float mf = metal;\n"
  "    metal = 0.0;\n"
  "    vec3 cq = vWorld - uMdlO;\n"
  "    vec3 mx = uMdlX, mz = uMdlZ, my = cross(mz, mx);\n"
  "    vec3 lp = vec3(dot(cq, mx), dot(cq, my), dot(cq, mz));\n"
  "    vec2 cp = vec2(lp.x + lp.y * 0.8, lp.z) * 24.0;\n"
  "    float knit = vnoise2(cp * 30.0);\n"
  "    float ew = clamp(length(fwidth(normalize(vNrm))) * 2.6, 0.0, 1.0);\n"
  // THE WEAPON THEME, before the finish below: the theme is the PAINT, and the
  // finish (grain, wear to bare metal, brushing) still happens to it. Only
  // weapon parts carry a fractional flag — .3 the light primary family, .2 the
  // dark furniture — and the gloves, drawn in the same pass, stay integral.
  // The pattern is projected onto the face's own dominant plane IN THE WEAPON'S
  // FRAME, so it is painted on the gun rather than crawling over it.
  "    float tk = fract(-mf + 0.0005);\n"
  "    if (uSkin.x > 0.5 && tk > 0.1) {\n"
  "      vec3 nw = abs(vec3(dot(vNrm, mx), dot(vNrm, my), dot(vNrm, mz)));\n"
  "      vec2 pp = (nw.x > nw.y && nw.x > nw.z ? lp.zy : nw.y > nw.z ? lp.zx : lp.xy) * uSkin.y;\n"
  "      int pat = int(uSkin.x + 0.5) - 1;\n"
  "      vec3 pc = uSkinA;\n"
  "      if (pat == 1 || pat == 2) {\n"            // blotch camo; 2 = the same, pixelated
  "        vec2 q = pat == 2 ? floor(pp * 7.0) / 7.0 : pp;\n"
  "        float n1 = vnoise2(q) * 0.65 + vnoise2(q * 2.3 + 17.0) * 0.35;\n"
  "        float n2 = vnoise2(q * 1.3 + 41.0) * 0.6 + vnoise2(q * 3.1 + 5.0) * 0.4;\n"
  "        pc = mix(pc, uSkinB, step(0.50, n1));\n"
  "        pc = mix(pc, uSkinC, step(0.60, n2));\n"
  "      } else if (pat == 3) {\n"                 // tiger stripes, across the bore
  "        float w = pp.x + (vnoise2(pp * vec2(0.6, 1.4)) - 0.5) * 1.6 + pp.y * 0.35;\n"
  "        float s = sin(w * 5.2) * 0.5 + 0.5 + (vnoise2(pp * 3.0 + 9.0) - 0.5) * 0.45;\n"
  "        pc = mix(uSkinA, uSkinB, step(0.62, s));\n"
  "        pc = mix(pc, uSkinC, step(0.70, vnoise2(pp * 0.9 + 23.0)) * step(s, 0.30));\n"
  "      } else if (pat == 4) {\n"                 // fade: muzzle-to-butt gradient
  "        float t = clamp(dot(lp, vec3(0.18, 0.55, 1.0)) * uSkin.y * 0.25 + 0.45, 0.0, 1.0);\n"
  "        pc = t < 0.5 ? mix(uSkinA, uSkinB, t * 2.0) : mix(uSkinB, uSkinC, t * 2.0 - 1.0);\n"
  "      }\n"
  // Relative to the family's own reference albedo, so the baked occlusion and
  // the step between a dark part and a darker one both survive the repaint.
  "      float lum = dot(alb, vec3(0.3333));\n"
  "      if (tk > 0.25) alb = pc * pc * clamp(lum / 0.277, 0.45, 1.5);\n"
  "      else alb = min(uSkinD * uSkinD * (lum / 0.0361), vec3(0.85));\n"
  "    }\n"
  // THE WRIST DISPLAY (mf -8): the amber panel on the support gauntlet shows
  // the magazine — two seven-segment digits and a fill bar — in the same
  // backlit acrylic as the inlays, so it reads as part of the glove. UV rides
  // in the vertex colour (the panel is built for it in vm_build).
  "    if (mf < -7.5) {\n"
  "      vec2 uv = vCol.rg;\n"
  "      float ammo = uWrist.x, mag = max(uWrist.y, 1.0);\n"
  "      float low = step(ammo, mag * 0.25);\n"
  "      float blink = low * step(0.5, fract(uWrist.z * 2.2));\n"
  "      vec3 on = mix(uWristOn, vec3(1.0, 0.16, 0.06), low);\n"
  "      vec3 bg = vec3(0.10, 0.045, 0.012);\n"
  "      float lit = 0.0;\n"
  "      int a = int(ammo + 0.5);\n"
  "      for (int k = 0; k < 2; k++) {\n"
  "        int dg = k == 0 ? (a / 10) % 10 : a % 10;\n"
  "        if (k == 0 && a < 10) continue;\n"
  "        vec2 c = vec2(uv.x - (k == 0 ? 0.30 : 0.70), uv.y - 0.60) * vec2(4.6, 3.3);\n"
  "        lit = max(lit, seg7(c, dg));\n"
  "      }\n"
  "      float bar = step(0.10, uv.y) * step(uv.y, 0.20) * step(0.08, uv.x) * step(uv.x, 0.92);\n"
  "      float fill = step(uv.x, 0.08 + 0.84 * ammo / mag);\n"
  "      lit = max(lit, bar * mix(0.18, 1.0, fill));\n"
  "      lit *= 1.0 - 0.75 * blink;\n"
  "      vec3 e = mix(bg, on * 3.2, lit);\n"
  "      oCol = vec4(display_in(sqrt(clamp(e / 3.2, 0.0, 1.0))) * 3.0 + e * 0.15, 1.0);\n"
  "      return;\n"
  "    }\n"
  "    if (mf < -6.5) {\n"            // 45-deg diamond-knurled tactical grip leather
  "      vec3 nl = abs(vec3(dot(vNrm, mx), dot(vNrm, my), dot(vNrm, mz)));\n"
  "      nl *= nl; nl *= nl; nl /= nl.x + nl.y + nl.z + 1e-4;\n"
  "      vec3 sg = vec3(0.0);\n"
  "      for (int i = 0; i < 3; i++) {\n"
  "        vec2 uv = (i == 0 ? lp.yz : i == 1 ? lp.xz : lp.xy) * 340.0;\n"
  "        vec2 d2 = vec2(uv.x + uv.y, uv.x - uv.y);\n"
  "        vec2 f2 = abs(fract(d2) - 0.5) * 2.0;\n"
  "        sg[i] = 1.0 - smoothstep(0.56, 0.86, max(f2.x, f2.y));\n"
  "      }\n"
  "      float stip = dot(sg, nl);\n"
  "      alb *= (0.64 + 0.56 * stip) * (0.94 + 0.12 * knit);\n"
  "      rough = clamp(rough + 0.14 - 0.28 * stip, 0.24, 1.0);\n"
  "    } else if (mf < -3.5) {\n"
  "      float gr = vnoise2(cp * 42.0) * 0.6 + vnoise2(cp * 7.0) * 0.4;\n"
  "      float blot = vnoise2(cp * 1.6);\n"
  "      if (mf < -5.5) {\n"             // painted alloy: wear opens to bare metal
  "        float wear = smoothstep(0.42, 0.74, ew * (0.55 + 0.75 * blot) + (gr - 0.5) * 0.30);\n"
  "        alb *= 0.90 + 0.20 * gr;\n"
  "        alb = mix(alb, vec3(0.20, 0.19, 0.18), wear);\n"
  "        metal = 0.85 * wear;\n"
  "        rough = clamp(rough + (blot - 0.5) * 0.26 - 0.20 * wear, 0.14, 1.0);\n"
  "      } else if (mf < -4.5) {\n"      // anodised alloy: a brushed metallic sheen
  "        float brush = vnoise2(vec2(lp.z * 34.0, (lp.x + lp.y) * 900.0));\n"
  // ANISOTROPIC: the brushing is a field of grooves along the bore, so the
  // normal is tilted ACROSS it only — the highlight stretches into a streak
  // along the rail instead of sitting as a round spot.
  "        float bh = vnoise1((lp.x + lp.y) * 2600.0) * 0.6 + vnoise1((lp.x - lp.y) * 7100.0) * 0.4;\n"
  "        n = bumpn(n, bh * 0.00006, 1.0);\n"
  // EDGE WEAR off the curvature: anodising is a few microns thick and the
  // first thing a holster or a barricade takes off a sharp edge, so the
  // arrises go to bright bare aluminium.
  "        float wear = smoothstep(0.38, 0.80, ew * (0.65 + 0.70 * blot) + (gr - 0.5) * 0.25);\n"
  "        alb = alb * (1.25 + 0.10 * brush) + vec3(0.010) * ew;\n"
  "        alb = mix(alb, vec3(0.50, 0.50, 0.52), wear);\n"
  "        metal = mix(0.30, 0.95, wear);\n"
  "        rough = clamp(rough - 0.06 + (brush - 0.5) * 0.10 + (blot - 0.5) * 0.08 - 0.12 * wear, 0.16, 1.0);\n"
  "      } else {\n"                     // moulded polymer: matte, cellular grain
  // A moulded texture is EDM pitting on the tool — a field of small craters
  // (Worley F1), not a smooth noise. ~0.4 mm cells, with the roughness
  // scattering cell to cell so the sheen breaks up like stippled nylon.
  "        float wc = worley2(cp * 70.0);\n"
  "        float wc2 = worley2(cp * 170.0 + 7.0);\n"
  "        float pit = smoothstep(0.05, 0.55, wc) * 0.7 + smoothstep(0.05, 0.6, wc2) * 0.3;\n"
  "        n = bumpn(n, pit * 0.000020 * (1.0 - smoothstep(0.005, 0.014, length(fwidth(cp)))), 1.0);\n"
  "        alb *= (0.90 + 0.20 * gr) * (0.93 + 0.09 * pit);\n"
  "        rough = clamp(rough + 0.10 + (gr - 0.5) * 0.16 + (vhash3(floor(cp * 52.0)).z - 0.5) * 0.10, 0.2, 1.0);\n"
  "      }\n"
  "    } else if (mf < -2.5) {\n"        // lit inlay: ignores the scene's light
  // An LED behind amber acrylic: well past the bloom knee (>4 in scene
  // units), so the strip glows into the glove around it instead of reading as
  // orange paint.
    // The tint is pushed redder than the authored amber because ACES walks a
  // saturated orange toward yellow as it compresses: what comes out of the
  // curve is the amber that went in.
"      oCol = vec4(display_in(sqrt(alb) * vec3(1.0, 0.86, 0.72)) * vec3(3.4, 2.2, 1.6), 1.0);\n"
  "      return;\n"
  "    } else if (mf < -1.5) {\n"        // glove: 2x2 twill aramid, triplanar
  // A 2/2 TWILL: each thread passes over two and under two, shifted one per
  // row, which is what draws the diagonal rib a Kevlar or carbon weave is
  // recognised by. Each cell is a warp or a weft float with a rounded
  // profile, and the float on top is the one that catches the light — so the
  // weave is a pattern in the NORMAL and the SPECULAR, not a printed check.
  "      vec3 nl = abs(vec3(dot(vNrm, mx), dot(vNrm, my), dot(vNrm, mz)));\n"
  "      nl *= nl; nl *= nl; nl /= nl.x + nl.y + nl.z + 1e-4;\n"
  "      vec3 th = vec3(0.0), ov = vec3(0.0);\n"
  "      for (int i = 0; i < 3; i++) {\n"
  "        vec2 m2 = (i == 0 ? lp.yz : i == 1 ? lp.xz : lp.xy) * 620.0;\n"
  "        vec2 ci = floor(m2), f = fract(m2);\n"
  "        float warp = step(mod(ci.x + ci.y, 4.0), 1.5);\n"
  "        float prof = warp > 0.5 ? sin(f.x * 3.14159) : sin(f.y * 3.14159);\n"
  "        float along = warp > 0.5 ? sin(f.y * 3.14159) : sin(f.x * 3.14159);\n"
  "        th[i] = prof * (0.75 + 0.25 * along);\n"
  "        ov[i] = warp;\n"
  "      }\n"
  "      float tw = dot(th, nl), wp = dot(ov, nl);\n"
  "      float wv = 1.0 - smoothstep(0.0008, 0.0022, fwidth(lp.x) + fwidth(lp.y));\n"
  // SUBTLE, and that is the correction: at 0.22 mm of relief and glossy
  // thread tops every float caught the rim light and the sky as its own
  // highlight, which read across the whole glove as white powder. Aramid is
  // a MATTE fibre; the weave is a texture you see in the albedo and barely
  // in the sheen.
  "      n = bumpn(n, tw * 0.00007 * wv, 1.0);\n"
  "      alb *= (0.95 + 0.06 * tw * wv) * (0.92 + 0.16 * knit) * (1.0 + 0.04 * (wp - 0.5) * wv);\n"
  "      rough = clamp(rough + 0.08 - 0.04 * tw * wv, 0.45, 1.0);\n"
  "    } else {\n"                       // sleeve: plain ripstop
  "      vec2 rg = abs(fract(cp * 1.5) - 0.5);\n"
  "      alb *= (0.86 + 0.28 * knit) * (1.0 - 0.14 * step(0.47, max(rg.x, rg.y)));\n"
  "    }\n"
  // A metallic theme (gold) is a plated finish: it overrides what the
  // polymer and paint branches decided, on the primary family only.
  "    if (uSkin.x > 0.5 && uSkin.z > 0.0 && tk > 0.25) {\n"
  "      metal = max(metal, uSkin.z);\n"
  "      rough = min(rough, 0.26);\n"
  "    }\n"
  "  }\n"
  // --- WEAPON MICRO-STRUCTURE, IN THE WEAPON'S OWN FRAME --------------------
  // A finish is what separates steel from a moulding, and there was none of it
  // anywhere in the stream: one roughness and one albedo per PART means a
  // receiver flat is a single unbroken sweep of highlight however good the BRDF
  // over it is, which is most of why the whole rifle read as one charcoal
  // casting. The world's own answer — the `det` grain above — cannot be reused
  // here, because it is anchored in WORLD space: on an object the player
  // carries, world-anchored noise crawls with scratches at every step. So the
  // viewmodel publishes its own basis and the structure is a function of MODEL
  // position. It rides with the rifle and is therefore dead still in the hand,
  // which is exactly what a finish is. (Proved rather than assumed: the same
  // pose rendered from two world positions 3 m apart differs by 0.06/255 mean
  // over the weapon's own pixels while the world behind it differs by 12.)
  // METAL ONLY, and the gate is not fussiness: the gloves, the sleeves and the
  // polymer furniture are in this same draw call, and broaching marks on a
  // nylon glove is a bug rather than a detail.
  //
  // EVERY SCALE HERE IS A FINE ONE, and that is a correction. The first version
  // carried the world surface's two-octave argument across unchanged — a fine
  // grain plus a hand's-width patchiness under it — and a hand's width on a
  // 31 mm receiver is the whole part: it rendered as 2 cm blocks of value with
  // hard edges, which reads as a stain rather than as a finish. What a
  // phosphate coating actually looks like at arm's length is a fine mottle,
  // and what a machined flat looks like is directional. Those are the two
  // things modelled, plus lathe rings for the parts that were turned.
  "  if (uVm > 0.5 && metal > 0.35) {\n"
  "    vec3 q = vWorld - uMdlO;\n"
  "    float zb = dot(q, uMdlZ);\n"                 // along the bore
  // The coordinate ACROSS the bore and IN this face's own surface is the cross
  // product of the two, which needs neither a UV set nor a triplanar blend to
  // be right on every face of every part. On a face square to the bore that
  // product collapses; `bore` is the weight that fades the directional terms
  // out exactly there, and the fallback keeps the grain from smearing.
  "    vec3 tv = cross(uMdlZ, n);\n"
  "    float tl = length(tv);\n"
  "    float xb = tl > 1e-3 ? dot(q, tv) / tl : dot(q, uMdlX);\n"
  "    float bore = 1.0 - abs(dot(n, uMdlZ));\n"
  // Phosphate mottle: ~1.5 mm cells, INTERPOLATED. A hard-edged cell grid at
  // this size is a checker, not a crystal coating.
  "    float gh = vnoise2(vec2(xb, zb) * 670.0);\n"
  // Brushing, drawing and broaching all run ALONG the bore, so what varies is
  // the coordinate across it — soft-edged strokes about a third of a millimetre
  // apart. Smoothed rather than hashed: a one-pixel hard stroke aliases into
  // speckle and stops reading as direction, which is the only thing this term
  // is here for.
  "    float br = vnoise1(xb * 3400.0) - 0.5;\n"
  // Turning marks run the OTHER way. A barrel, a bolt and a suppressor tube are
  // turned on a lathe, so their grain is rings around the bore — a function of
  // the position ALONG it, not across it. Only the bright family gets them: a
  // phosphate conversion coating buries its substrate's tool marks, which is
  // most of what phosphate is for, so the gate is the AUTHORED roughness rather
  // than the one this block is in the middle of modulating.
  // Gated on its own factor, which is exactly 0 at MAT_PARK_R 0.44 — i.e. over
  // the largest metal area on the weapon, where the two fsin behind vnoise1 were
  // being computed and then multiplied away.
  "    float la = 0.0;\n"
  "    if (vMat.x < 0.40)\n"
  "      la = (vnoise1(zb * 2100.0) - 0.5)\n"
  "         * (1.0 - smoothstep(0.26, 0.40, vMat.x));\n"
  // Amplitudes, backed off from the first pass that made them visible at all.
  // At 0.075/0.15 the receiver read as BRUSHED ALUMINIUM — a texture pasted on
  // a shape rather than a finish belonging to it, and loud enough at arm's
  // length to compete with the sights. A finish is supposed to be the thing you
  // notice second.
  "    rough = clamp(rough * (0.90 + 0.19 * gh)\n"
  "                  + br * 0.042 * bore + la * 0.075, 0.035, 1.0);\n"
  "    alb *= 1.0 + (gh - 0.5) * 0.085 + br * 0.040 * bore;\n"
  "  }\n"
  // Geometric specular antialiasing. A curved metal edge narrower than a pixel
  // puts the whole GGX lobe between samples, so the highlight appears and
  // vanishes as the gun sways — sparkle that no amount of MSAA touches, because
  // MSAA samples coverage, not shading. Widening the lobe by the normal's own
  // screen-space variance turns a sub-pixel bright highlight into a dim wide one,
  // which is what the average over that pixel actually is.
  // MOVED BELOW THE SURFACE TREATMENTS, and that is the point: it used to run
  // before them, so the one term in the frame that perturbs a normal per PIXEL
  // — the ripple field on the floor — was the one term it did not cover. It is
  // a no-op for everything it used to see (the seam block only touches `rough`
  // through `det`, which is 0 on every moving surface), and it is now the
  // filter for the ripples, the glitter and the puddle edge as well.
  "  if (uDetail > 0.0 && abs(n.y) < 0.5) {\n"  // vertical faces: panel seams
  "    float sc = abs(n.x) > 0.5 ? vWorld.z : vWorld.x;\n"
  "    float sf = abs(fract(sc / 3.0) - 0.5) * 3.0;\n"
  "    float sm = smat > 0 ? 0.0 : 1.0 - smoothstep(0.02, 0.05, sf);\n"
  "    alb *= 1.0 - 0.22 * det * sm;\n"
  // Weathering. Streaks run down each panel from its top, sand banks up against
  // its foot, and broad patches break up the flat paint. All of it is anchored to
  // the world and costs a few noise taps on structure pixels only.
  // A MATERIAL wall gets rain streaks, not drapes: at 2.7/m and 26% the streak
  // field laid 37 cm vertical bands over every wall, which against real brick
  // and render reads as a curtain hanging in front of it. Narrow, sparse, faint.
  "    float sk = smat > 0 ? 7.5 : 2.7;\n"
  "    float streak = vnoise2(vec2(sc * sk, vWorld.y * 0.22 + floor(sc / 3.0)));\n"
  "    float patchy = vnoise2(vec2(sc, vWorld.y) * 0.62);\n"
  "    alb *= 1.0 - (smat > 0 ? 0.15 : 0.26) * smoothstep(0.58, 0.90, streak);\n"
  "    alb *= 0.90 + 0.20 * patchy;\n"
  "    float dust = 1.0 - smoothstep(0.0, 0.42 + 0.40 * patchy, vWorld.y);\n"
  "    alb = mix(alb, vec3(0.47, 0.39, 0.29), dust * 0.36);\n"
  "    rough = mix(rough, 1.0, 0.5 * det * sm);\n"
  "  }\n"
  // --- THE FLOOR ----------------------------------------------------------
  // What was here was a 2 m checker and an 8 m seam grid: a floor pretending to
  // be a tiled hall, on an outdoor arena, in every theme. Half the screen is
  // this surface and it carried less information than a wall.
  //
  // THE FOOTPRINT IS THE GOVERNING QUANTITY and every term below reads it. `fp`
  // is how many metres of ground one pixel covers, straight out of the world
  // position's own derivatives — no UV set, no mip chain, and correct at every
  // range and every grazing angle for free. A term whose own wavelength has
  // fallen under 2*fp is past Nyquist: drawing it anyway is not detail, it is
  // noise that crawls when the player walks. So each term fades itself out on
  // fp and hands what it lost to ROUGHNESS, which is the honest average of a
  // wrinkle too small to resolve. That single rule is what lets the floor be
  // this detailed up close and stay dead calm at thirty metres.
  "  float wet = 0.0, glint = 0.0, oil = 0.0;\n"
  "  vec3 film = vec3(0.0);\n"
  "  if (n.y > 0.95 && vWorld.y < 0.05) {\n"
  "    vec2 P = vWorld.xz;\n"
  // ONE sqrt, not two: sqrt is monotone and correctly rounded, so the max of the
  // lengths is the sqrt of the max of the squared lengths, bit for bit.
  "    vec2 dpx = dFdx(P), dpy = dFdy(P);\n"
  "    float fp = sqrt(max(dot(dpx, dpx), dot(dpy, dpy)));\n"
  // GATED ON THE THEME. surf_params returns uSurf.x == 0 for GRIT and WET, and
  // then nv/loose/rv/lam/vis feed only the else branch below, which is
  // `clamp(rough + 0.0, ...)` on a rough already inside that range — a literal
  // no-op costing two vnoise2 and three smoothsteps per floor pixel.
  "    if (uSurf.x > 0.0) {\n"
  "    float nv = max(dot(n, V), 0.0);\n"
  // --- AEOLIAN RIPPLES ----------------------------------------------------
  // NOT fbm. Isotropic noise on a floor reads as dirt or as nothing; what makes
  // sand read as sand is long COHERENT PARALLEL crestlines with a slow drift in
  // heading. And they are asymmetric — a gentle windward rise and a short steep
  // lee face — which is the whole reason the field has a direction you can see.
  // Real impact ripples run 10-15 cm crest to crest and about 4 mm tall, i.e. a
  // maximum slope near 12 degrees. Almost every sand shader authors them three
  // to five times too steep, and a floor of 45-degree corrugations reads as
  // corrugated iron.
  // WHERE THERE ARE RIPPLES AT ALL is not a shader question, and this is the
  // one line that connects the fragment work to the bake. mesh_ground_vert
  // writes looseness into the roughness channel as an offset either side of
  // WORLD_GROUND_R — drift raises it, traffic lowers it — so reading it back
  // here puts ripples in the lee of the walls and takes them off the packed
  // lanes for free. Without it the whole floor was corrugated at one uniform
  // amplitude from wall to wall, which reads as ribbed fabric rather than as a
  // place the wind has been over. The patch mask on top of it is the second
  // half: even loose sand ripples unevenly.
  "    float loose = clamp((vMat.x - " GLSL_NUM(WORLD_GROUND_R) ") * 8.0 + 0.75,\n"
  "                        0.0, 1.0)\n"
  "               * smoothstep(0.14, 0.52, vnoise2(P * 0.13));\n"
  // The two fades are computed BEFORE the field they fade, so a pixel whose
  // ripples would be filtered away to nothing does not pay to compute them.
  // Past about fifteen metres, and at every grazing angle, that is most of the
  // floor in frame — the whole ground treatment measured 2.65 ms/frame on
  // llvmpipe and this is the half of it that nobody could see.
  // THE WAVELENGTH VARIES, and that is what makes crests MERGE AND SPLIT. At a
  // constant 12.5 cm the field is dead-regular corduroy — every crest the same
  // period, the same amplitude, no bifurcation anywhere in forty-eight metres,
  // which reads as a woven blind. ONE noise drives both the wavelength and the
  // amplitude together, because the ripple index (wavelength over height) is
  // roughly constant in real sand: a shorter ripple is a shallower ripple, and
  // driving them independently is how a field ends up with tall crests packed
  // tight, which nothing does.
  "    float rv = vnoise2(P * 0.105);\n"
  "    float lam = 0.125 * (0.78 + 0.44 * rv);\n"
  // The GRAZING fade is the load-bearing one and it was too gentle. Measured,
  // the ripple's relative contrast is 5.6% on sunlit floor and 24.8% in a
  // wall's shadow — and deleting the albedo term changes neither number, so the
  // carrier is not colour. It is the ENVIRONMENT reflection: at a grazing view
  // the reflected ray runs near the horizon, a 14-degree crest swings it above
  // and below that horizon, and sky-against-ground is the largest value step
  // the environment has. It is also where the moire at combat distance comes
  // from. Both go away by fading the field harder as the view flattens, which
  // is the same direction the Nyquist fade already wants.
  "    float vis = (1.0 - smoothstep(lam * 0.45, lam * 1.8, fp))\n"
  "              * smoothstep(0.10, 0.42, nv) * loose;\n"
  "    if (vis > 0.004) {\n"
  // The heading barely moves, and that is the correction. At 0.55 rad of swing
  // the crestlines curved into concentric arcs and rings seen from above — sand
  // does not make rings, and a field whose direction varies that much has no
  // wind in it, only noise. Real crestlines are long, near-parallel, and drift
  // a few degrees over tens of metres.
  "      float ang = uSurf.w + 0.16 * (vnoise2(P * 0.028) - 0.5);\n"
  "      vec2 wd = vec2(cos(ang), sin(ang));\n"
  // The warp has to be TWO octaves and weak. At one octave and 0.22 the field
  // showed the noise's own cell grid as a lattice of diamonds laid over the
  // crests — the warp was strong enough, and its wavelength close enough to the
  // ripple's, that the carrier read the carrier. Two decorrelated octaves at
  // half the amplitude bend the crestlines without ever agreeing on a grid.
  // The warp bends a crest ALONG its own line; it may not be strong enough to
  // build a second set of crests ACROSS it. At 0.115/0.075 the field came out
  // woven — two directions of comparable energy, which is fabric. Aeolian
  // ripples are strongly anisotropic by definition, and the anisotropy is the
  // only thing that tells you which way the wind blew.
  "      float u = dot(P, wd) / lam\n"
  "             + 0.055 * vnoise2(P * 0.31) + 0.030 * vnoise2(P * 3.10);\n"
  "      float sq = fract(u);\n"
  // Asymmetric, and the lee face is 0.34 rather than the measured 0.28: at 0.28
  // the crest is a razor line one pixel wide that the normal cannot filter, and
  // a ripple crest is a rounded ridge of loose grains, not a fold.
  "      float hh = sq < 0.66 ? sq / 0.66 : (1.0 - sq) / 0.34;\n"
  "      float dh = sq < 0.66 ? 1.0 / 0.66 : -1.0 / 0.34;\n"
  // Two independent fades, and both are needed. `fp` is Nyquist. `nv` is the
  // OTHER failure: a bump on a flat quad has no parallax and no self-occlusion,
  // so edge-on it stops reading as relief and starts reading as a printed
  // pattern — exactly where the footprint is largest anyway, so the two fades
  // agree about direction and only disagree about which artefact they prevent.

  // 9 mm on a 12.5 cm wavelength, i.e. a 14-degree lee face. Measured ripples
  // are nearer 4 mm and 7 degrees; this is the same stylisation the figure
  // already carries (ANSUR II with the head at 1.05), and at 4 mm the field was
  // invisible at every range a player is ever at.
  "      float amp = 0.0090 * uSurf.x * vis * (0.62 + 0.50 * rv)\n"
  // A low sun rakes across every crest, so the same relief reads far stronger.
  "                * clamp(SUN_DIR.y * SUN_DIR.y * 2.2, 0.16, 1.0);\n"
  "      vec2 gr = wd * (dh * amp / lam);\n"
  "      n = normalize(n - vec3(gr.x, 0.0, gr.y));\n"
  // The albedo term is deliberately weak. At 0.15 the crests carried the read on
  // their own COLOUR, which meant the field looked identical in shadow and in
  // sun — a printed pattern rather than relief. Half that, and what makes a
  // ripple visible is the normal, i.e. the light.
  "      alb *= 1.0 + (hh - 0.5) * 0.075 * uSurf.x * vis;\n"
  "      rough = clamp(rough + 0.11 * uSurf.x * (1.0 - vis), 0.045, 1.0);\n"
  "    } else {\n"
  // What the early-out skipped still has to arrive as ROUGHNESS, or there is a
  // visible ring around the player where relief turns to plastic.
  "      rough = clamp(rough + 0.11 * uSurf.x, 0.045, 1.0);\n"
  "    }\n"
  "    }\n"
  // --- GRIT ---------------------------------------------------------------
  // The other floor: dry warm concrete with dust and gravel on it. No relief at
  // all — a pebble 8 mm across is sub-pixel past four metres and modelling it as
  // a normal buys nothing but sparkle. It is albedo and roughness only, which is
  // what loose grit on a hard floor actually changes.
  // --- SLABS --------------------------------------------------------------
  // A hard floor is POURED, and a poured floor is cut into bays: without the
  // joints the yard of a warehouse district was one seamless sheet of grey
  // with nothing to tell you its scale. 3 m sawcut bays, a value step per bay
  // from separate pours, and dirt packed into the cuts. Sand gets none of it.
  "    if (uSurf.x == 0.0) {\n"
  "      vec2 fb = fract(P / 3.0) * 3.0;\n"
  "      float dj = min(min(fb.x, 3.0 - fb.x), min(fb.y, 3.0 - fb.y));\n"
  "      float jv = mfade(0.07, fp);\n"
  "      float jm = (1.0 - smoothstep(0.005, 0.011 + fp * 0.6, dj)) * jv;\n"
  "      float bay = vhash2(floor(P / 3.0) + vec2(13.0, 29.0));\n"
  "      alb *= (0.95 + 0.10 * bay) * (1.0 - 0.38 * jm);\n"
  "      rough = clamp(rough + 0.10 * jm, 0.045, 1.0);\n"
  "    }\n"
  // --- YARD ---------------------------------------------------------------
  // THE ARENA AND THE FOUNDRY ARE WORKING YARDS, and their floors said nothing:
  // forty-eight metres of poured grey with joints in it, the emptiest thing in
  // the frame. A yard is painted — a safety lane walked round the perimeter, a
  // chevron hazard band at the foot of every wall — and the paint is worn
  // through where traffic and weather took it. Oil stains pool in a few bays
  // and read by their SHEEN as much as their value. All of it is a function of
  // world position, so it costs no geometry, cannot z-fight, never touches
  // collision or a bot's path, and fades on the footprint like every other
  // floor term. Yellow and black, never red: red at any height is the enemy.
  "    if (uYard.y > 0.5 && uSurf.x == 0.0) {\n"
  "      vec2 ap = abs(P);\n"
  "      float dw = uYard.x - max(ap.x, ap.y);\n"
  "      float lv = mfade(0.30, fp);\n"
  "      float ln = (1.0 - smoothstep(0.085, 0.105 + fp, abs(dw - 1.6))) * lv;\n"
  // A WALKWAY down the yard's long axis: two lines 1.5 m apart through the
  // middle, the one leading line an open box arena otherwise lacks.
  "      ln = max(ln, (1.0 - smoothstep(0.065, 0.085 + fp, abs(ap.x - 0.75))) * lv\n"
  "                    * step(dw, uYard.x));\n"
  "      float band = smoothstep(0.03, 0.05, dw) * (1.0 - smoothstep(0.55, 0.57 + fp, dw));\n"
  "      float along = ap.x > ap.y ? P.y * sign(P.x) : -P.x * sign(P.y);\n"
  "      float chev = smoothstep(0.46, 0.54, fract((along + dw) / 0.6));\n"
  "      float wear = smoothstep(0.30, 0.62, vnoise2(P * 1.7) * 0.6 + vnoise2(P * 9.0) * 0.4);\n"
  "      float paint = max(ln, band * lv) * (0.30 + 0.70 * wear);\n"
  "      vec3 yel = vec3(0.50, 0.36, 0.045);\n"
  "      vec3 pc = mix(vec3(0.030), yel, band > 0.5 ? chev : 1.0);\n"
  "      alb = mix(alb, pc, paint * 0.88);\n"
  "      rough = mix(rough, 0.58, paint * 0.6);\n"
  // Stains stay MATTE. Glossy oil mirrored the sky into white patches in the
  // sun corridor, which is glare across exactly the floor a fight is read on.
  "      float st = vnoise2(P * 1.25 + 31.0) * 0.7 + vnoise2(P * 4.1 + 7.0) * 0.3;\n"
  "      float stain = smoothstep(0.78, 0.86, st) * (1.0 - band) * (1.0 - ln);\n"
  "      alb *= 1.0 - 0.34 * stain;\n"
  "      rough = mix(rough, 0.62, stain * 0.5);\n"
  "    }\n"
  "    float gvis = 1.0 - smoothstep(0.02, 0.09, fp);\n"
  "    if (gvis > 0.004) {\n"
  "      float gz = vnoise2(P * 23.0);\n"
  "      alb *= 1.0 + (gz - 0.5) * 0.16 * det * gvis;\n"
  "      rough = clamp(rough + (gz - 0.5) * 0.10 * det * gvis, 0.045, 1.0);\n"
  "    }\n"
  // --- SUN GLITTER --------------------------------------------------------
  // Mica in sand, and it is three invariants rather than a noise function.
  // (1) The cells are anchored in WORLD space, so a spark belongs to a place on
  //     the ground and does not crawl when the player walks — the same argument
  //     the weapon's micro-structure makes in its own frame, one boundary out.
  // (2) The cell SIZE tracks the pixel footprint in power-of-two steps, so there
  //     is about one facet per pixel at every range. That is what stops sparkle
  //     density climbing with distance until the floor is television static, and
  //     the power-of-two quantisation is what keeps the field still WITHIN a
  //     level while the camera moves.
  // (3) It is gated by the specular CORRIDOR toward the sun. Real glitter is not
  //     everywhere; it is a band between you and the light, which is also why
  //     this whole block is arithmetically absent over most of the frame.
  // The rarity threshold is the fourth thing and the one that decides whether it
  // reads as mica or as tinsel: only about one cell in fourteen is reflective.
  "    if (uSurf.y > 0.0) {\n"
  "      vec3 Rv = reflect(-V, n);\n"
  "      float corr = pow(max(dot(Rv, SUN_DIR), 0.0), 3.5) * det;\n"
  "      if (corr > 0.0015) {\n"
  "        vec3 Hs = normalize(SUN_DIR + V);\n"
  // THREE TIMES the footprint, not one. At one cell per pixel every spark was a
  // single pixel at +68 over its ground with no spatial support at all, which is
  // the exact definition of a highlight that cannot be filtered and must
  // scintillate under camera motion — peripheral flicker in the lower third of
  // the screen, which in this genre is a false motion cue that costs a crosshair
  // placement. Three across gives each spark ~2 px of support and drops the
  // density to a ninth, which is also what it needed on looks.
  "        float lod = max(log2(max(fp / 0.0038, 1.0)), 0.0);\n"
  "        float l0 = floor(lod), lf = lod - l0;\n"
  "        float spark = 0.0;\n"
  // The upper level is only mixed in while the cross-fade is actually between
  // two of them. For most of the screen `lf` sits at one end and the second
  // evaluation is multiplied by zero.
  "        int lmax = lf > 0.02 ? 2 : 1;\n"
  "        for (int L = 0; L < lmax; L++) {\n"
  "          float cs = 0.0038 * exp2(l0 + float(L)) * 3.0;\n"
  "          vec2 gi = floor(P / cs);\n"
  "          vec3 r3 = vhash3(gi + vec2(11.3, 7.7) * (l0 + float(L)));\n"
  "          if (r3.z > 0.928) {\n"
  "            vec3 fn = normalize(vec3((r3.x - 0.5) * 0.21, 1.0,\n"
  "                                     (r3.y - 0.5) * 0.21));\n"
  "            float dt = max(dot(fn, Hs), 0.0);\n"
  // A soft DISC inside the cell rather than the whole cell: a flat-topped
  // square of light is a pixel block, and a spark is round.
  "            vec2 cf = fract(P / cs) - 0.5;\n"
  "            float cw = 1.0 - smoothstep(0.14, 0.46, length(cf));\n"
  "            spark += (L == 0 ? 1.0 - lf : lf) * cw\n"
  "                   * exp(-2400.0 * (1.0 - dt));\n"
  "          }\n"
  "        }\n"
  "        glint = spark * corr * uSurf.y;\n"
  "      }\n"
  "    }\n"
  // --- STANDING WATER -----------------------------------------------------
  // One height field, TWO thresholds. The film is where the field is under the
  // water level; the damp halo is the same field at a looser one, so the halo is
  // automatically concentric with and proportional to the puddle it surrounds
  // for one noise evaluation instead of two. Wet ground is darker because water
  // in the pores lowers the index step and light takes another absorption round
  // before it comes back out — a measured factor near five on porous stone, and
  // the same 0.20 floor Remember Me shipped.
  // The BAND is not a nicety: a hard mask edge is a one-pixel BRDF flip and it
  // crawls, which is the single most visible failure this whole surface can have.
  "    if (uSurf.z > 0.0) {\n"
  "      float hgt = vnoise2(P * 0.155) * 0.64 + vnoise2(P * 0.41) * 0.36;\n"
  "      float pool = smoothstep(-0.045, 0.045, uSurf.z - hgt);\n"
  // The halo is WIDER than the film by a real margin. At a 0.075 offset it was
  // a rim a few pixels across and the pools read as bright cut-outs lying ON
  // the floor rather than in it — the halo is the only contact cue a puddle
  // has, since there is no geometry under it to occlude anything.
  "      float damp = smoothstep(-0.13, 0.13, uSurf.z + 0.125 - hgt);\n"
  // 0.72, NOT 0.32, and the first number was a misread of the source it came
  // from. The shipped wet-surface model is mix(1, 0.2, (1 - metal) * porosity)
  // — the 0.2 is the floor for a FULLY POROUS material, and it is reached only
  // after the porosity factor scales it. Concrete's porosity is about 0.25, so
  // damp concrete lands near 0.8 of dry, not at a fifth of it.
  // Measured, the 0.32 version put 63.8% of this theme's floor INSIDE the
  // figures' own luminance band (blotches at 69-88 against a 155 lit floor,
  // i.e. an 84 LSB swing on open ground) and produced soft dark lobes that read
  // as "something tall is standing just out of frame". That is not an art
  // defect, it is a phantom occluder on every angle a player holds.
  "      alb *= mix(1.0, 0.72, damp) * mix(1.0, 0.76, pool);\n"
  // NEVER a literal zero. A mirror-flat plane under a bright sun at a shallow
  // angle is the textbook specular-firefly generator, and neither MSAA (which
  // samples coverage) nor the PCF (which is shadows) touches it.
  "      rough = mix(rough, 0.05, pool);\n"
  "      n = normalize(mix(n, vec3(0.0, 1.0, 0.0), pool));\n"
  // A COMPLETELY still film is a mirror, and a mirror the size of a room lying
  // on a floor reads as a hole in the world rather than as water. Two
  // centimetres of wind chop, faded on the same footprint every other term
  // uses, is enough to break the reflection into something that moves with the
  // surface. It is also the term that gives the specular filter downstream
  // something to work with, which is what keeps the sun's glare off it stable.
  "      float ws = 1.0 - smoothstep(0.02, 0.10, fp);\n"
  "      vec2 wv = vec2(vnoise2(P * 9.0 + vec2(3.1, 1.7)) - 0.5,\n"
  "                     vnoise2(P * 9.0 - vec2(5.3, 2.9)) - 0.5);\n"
  "      n = normalize(n + vec3(wv.x, 0.0, wv.y) * 0.05 * pool * ws);\n"
  "      wet = pool;\n"
  "    }\n"
  // PAINTED MARKINGS on the port and refinery yards: dashed lane lines on a
  // 14 m grid, worn away where the traffic runs. Yellow on the quay, white
  // in the plant. (Under a building they are simply never seen.)
  "    if (uTown >= 1) {\n"
  "      float lw = 0.065;\n"
  "      vec2 gd = abs(fract(P / 14.0 + 0.5) - 0.5) * 14.0;\n"
  "      float dx = step(fract(P.y / 2.4), 0.58), dz = step(fract(P.x / 2.4), 0.58);\n"
  "      float ln = max((1.0 - smoothstep(lw, lw + fp, gd.x)) * dx,\n"
  "                     (1.0 - smoothstep(lw, lw + fp, gd.y)) * dz);\n"
  "      vec2 tk = abs(fract(P / 2.8 + 0.5) - 0.5) * 2.8;\n"
  "      ln = max(ln, (1.0 - smoothstep(lw, lw + fp, tk.x)) * step(gd.y, 1.2) * step(0.2, gd.y));\n"
  "      float wear = smoothstep(0.22, 0.62, vnoise2(P * 1.3) * 0.7 + vnoise2(P * 7.0) * 0.3);\n"
  "      vec3 paint = uTown == 1 ? vec3(0.80, 0.63, 0.12) : vec3(0.86, 0.86, 0.83);\n"
  "      float pm = ln * wear * 0.85 * mfade(0.13, fp);\n"
  "      alb = mix(alb, paint * paint, pm);\n"
  "      rough = mix(rough, 0.62, pm);\n"
  "    }\n"
  // OIL ON THE REFINERY FLOOR: drips under the racks spread into slicks a
  // few hundred nanometres thick, and a film that thin interferes — the
  // rainbow is a function of the film's thickness AND the viewing angle, so
  // it slides as the player walks. Dark, glossy, and a coloured F0.
  "    if (uTown == 2) {\n"
  "      float om = fbm4(P * 0.21 + 3.0);\n"
  "      oil = smoothstep(0.68, 0.73, om) * (1.0 - smoothstep(0.03, 0.12, fp) * 0.5);\n"
  "      if (oil > 0.0) {\n"
  "        float th = fbm4(P * 0.9) * 0.7 + (om - 0.68) * 5.0;\n"
  "        float nv2 = max(dot(n, V), 0.0);\n"
  "        film = 0.5 + 0.5 * cos(6.2832 * (th + (1.0 - nv2) * 0.9 + vec3(0.0, 0.33, 0.67)));\n"
  "        film = vec3(0.045) + (film - 0.5) * 0.045;\n"
  "        alb *= mix(1.0, 0.30, oil);\n"
  "        rough = mix(rough, 0.07, oil);\n"
  "      }\n"
  "    }\n"
  "  }\n"
  // Geometric specular antialiasing, now downstream of every term that touches a
  // normal. See the note above the seam block.
  "  vec3 dnx = dFdx(n), dny = dFdy(n);\n"
  "  rough = min(1.0, sqrt(rough * rough + 0.55 * (dot(dnx, dnx) + dot(dny, dny))));\n"
  // Water is a dielectric with an index of 1.33, so its reflectance at normal
  // incidence is 0.02 — HALF the 0.04 the substrate under it has. A puddle is
  // therefore darker than the dry floor everywhere except along the grazing
  // corridor where Fresnel takes over, which is exactly what a puddle looks like.
  "  vec3 F0 = mix(mix(vec3(0.04), alb, metal), vec3(0.02), wet);\n"
  "  F0 = mix(F0, film, oil);\n"
  "  vec3 diff = alb * (1.0 - metal);\n"
  "  float NoV = max(dot(n, V), 1e-4);\n"
  // Diffuse irradiance: the same sky, smoothed to a hemisphere lobe. Sun-facing
  // AO is only partly applied — a crease is dark in ambient light, not in a
  // direct beam.
  "  vec3 amb = mix(SKY_GND * 0.44, mix(SKY_HOR, SKY_TOP, 0.45) * 0.40,\n"
  "                 n.y * 0.5 + 0.5);\n"
  "  vec3 irr = amb * ao * uAmb;\n"
  "  float fr = pow(1.0 - NoV, 5.0);\n"
  "  vec3 Fe = F0 + (max(vec3(1.0 - rough), F0) - F0) * fr;\n"
  // SIXTEEN SHADOW TAPS ONLY WHERE THE ANSWER IS READ. `sh` has exactly three
  // readers: lit() with SUN_COL * sh (and lit returns vec3(0) before it touches
  // its radiance argument when dot(n, SUN_DIR) <= 0), the glint term, and the
  // puddle term — both guarded. So on any fragment facing away from the sun with
  // no glint and no water the whole 4x4 PCF plus its projective divide was
  // computed and discarded. This is NOT a change to the filter (see the PCF entry
  // under "Open, measured, not fixed"); it is the filter not running where its
  // result is unobservable. Measured single-threaded, min of 8: -6.6% of the
  // frame on seed 1337 (36 solids) and -13.9% on seed 2 (52), pixel-identical.
  "  float sh = 0.0;\n"
  "  if (dot(n, SUN_DIR) > 0.0 || glint > 0.0 || wet > 0.0)\n"
  "    sh = mix(0.45, 1.0, ao) * sun_shadow();\n"
  "  vec3 env = env_gloss(reflect(-V, n), rough) * mix(1.0, ao, 0.75);\n"
  // INSIDE THE FILM, THE SUN DISC IS SUPPRESSED. env_gloss hands a 0.05-rough
  // surface a lobe about seven hundred wide, i.e. a near-delta reflection of a
  // near-delta sun: one pixel of pure white that no filter in the frame can
  // average, sliding across the water as the camera moves. What a puddle
  // actually shows is a broad glare, so the reflection swaps to the plain sky
  // gradient plus a lobe two orders wider. The sky half is the SAME function,
  // so the puddle and the horizon behind it cannot disagree about the sky.
  "  if (wet > 0.0) {\n"
  "    vec3 Rw = reflect(-V, n);\n"
  "    vec3 wenv = sky_env(Rw, 0.0)\n"
  "              + SUN_COL * 0.42 * pow(max(dot(Rw, SUN_DIR), 0.0), 90.0);\n"
  // ...AND IT IS SCALED BY THE SUN TERM, which is a sky-VISIBILITY proxy and not
  // a fudge: a film of water a hand's width from a three-metre wall has half its
  // hemisphere behind that wall, so it cannot reflect a whole sky. Measured
  // without it, a sheet lying in a wall's shadow read 132-163 against a 25-46
  // shadow band — 90 to 120 LSB of local contrast in exactly the strip where
  // players hold angles, which makes a man standing on it a free silhouette and
  // a man standing beside it invisible. Which of the two you get is decided by
  // where the noise put the water, and that is a competitive-integrity defect
  // rather than a look.
  "    env = mix(env, wenv * mix(1.0, ao, 0.75) * mix(0.45, 1.0, sh), wet);\n"
  "  }\n"

  // Grazing-angle sheen. A GGX lobe that wide is numerically almost nothing, so
  // a uniform, a boot and a sandbag all came out as dead flat mass with no edge
  // at all — and the edge is precisely what separates a figure from the ground
  // behind it. Real cloth scatters forward off its fibres exactly there, and the
  // colour it scatters is the sky's, so the rim comes out cool against the warm
  // floor without a second light having to be invented for it.
  "  vec3 sheen = diff * rough * rough * pow(1.0 - NoV, 4.0) * 0.85 * amb;\n"
  // Bounce off the floor: down-facing surfaces near the ground pick up warm light
  // reflected off it. Without it the underside of everything — boot soles, the
  // magwell, a helmet brim's shadow side — goes to the same dead ambient as a
  // surface in a cave, and boots over bright sand came out darker than a chest.
  // Guarded, not multiplied by zero: the down-facing term's exp ran on every
  // up-facing and vertical fragment in the frame for a product that is 0 there.
  "  float bnc = 0.0;\n"
  "  if (n.y < 0.0) bnc = -n.y * exp(-max(vWorld.y, 0.0) * 0.85);\n"
  // ...AND SIDEWAYS, which is what the down-facing version was missing. A
  // VERTICAL face a hand's width above bright ground has half its hemisphere
  // filled by that ground, and without the term a 20 cm stone in open desert
  // sun rendered its shaded side at 39/255 — a neutral near-black chip, which
  // at range turned a line of clustered rubble into a dashed rule along every
  // wall base. The 0.45 m falloff is short on purpose: this is the bounce a
  // small object standing ON the floor gets, not a wash over the whole arena.
  "  float bncs = (1.0 - abs(n.y)) * exp(-max(vWorld.y, 0.0) * 2.2);\n"
  // ...AND AT BUILDING SCALE. A street is a light box: the sunlit half of it
  // throws light into the shaded facade across the way, which is most of the
  // light a shaded wall in a real town ever gets, and without it every wall out
  // of the sun went to a flat near-black slab with its brick, render and joints
  // invisible. Static world only (uDetail): figures keep exactly the contrast
  // against these walls they were tuned for, and a wall lifting behind a man
  // only separates him further from it.
  "  float bncw = uDetail > 0.0\n"
  "             ? (1.0 - abs(n.y)) * exp(-max(vWorld.y, 0.0) * 0.09) * mix(0.6, 1.0, ao) : 0.0;\n"
  "  vec3 bounce = diff * (bnc + bncs * 0.55 + bncw * 0.50) * SKY_GND * 1.35;\n"
  "  vec3 col = lit(n, V, SUN_DIR, diff, F0, rough, SUN_COL * sh)\n"
  "           + diff * irr\n"
  "           + env * Fe * (1.0 - 0.62 * rough)\n"
  "           + sheen * mix(0.55, 1.0, ao) + bounce;\n"
  // The glitter rides in AS A LIGHT, after the shadow term and scaled by it: a
  // spark in a wall's shadow is a spark with no sun on it, and adding this
  // before `sh` was the difference between mica and fairy lights.
  "  if (glint > 0.0) col += SUN_COL * glint * sh * 0.32;\n"
  // The viewmodel is not in the world: it has its own projection and its own
  // depth clear, and the world shadow map does not cover it. A black rifle lit
  // by the world sun alone therefore sits six to thirty times darker in linear
  // than the ground behind it, which is exactly what "shot at a different
  // exposure" looks like. So it gets a hero rig — a fill over the player's off
  // shoulder to model the form, and a hard rim from the far side to cut the
  // silhouette off the background. Both are view-relative, so the gun keeps its
  // read whichever way the player is facing.
  "  if (uVm > 0.5) {\n"
  "    col += lit(n, V, uFill, diff, F0, rough, VM_FILL);\n"
  "    col += lit(n, V, uRim, diff, F0, rough, VM_RIM);\n"
  // A view-space rim on top: the upper silhouette catches a cool edge in any
  // arena, so a blade or a gauntlet stays crisp against walls of its own colour.
  "    { float fr = 1.0 - clamp(dot(n, V), 0.0, 1.0);\n"
  "      col += vec3(0.42, 0.48, 0.58) * fr * fr * fr * (0.35 + 0.65 * clamp(n.y * 0.5 + 0.5, 0.0, 1.0)); }\n"
  // A shot is the brightest light source in the player's world for two ticks,
  // and it used to illuminate nothing: the additive flash ribbon was drawn in
  // front of a rifle that stayed exactly as dark as it had been the tick
  // before, so firing read as a decal stuck on the screen rather than as an
  // event happening to the weapon. One more light, warm, from the muzzle, and
  // only on the pass that draws the gun and the hands.
  "    if (uMuzC.r > 0.001) col += lit(n, V, uMuzL, diff, F0, rough, uMuzC);\n"
  "  }\n"
  // ...AND THE SAME SHOT LIGHTS THE WORLD, which it did not. Measured, a wall
  // 1.4 m in front of the muzzle brightened by 0.09 of 255 on the bright flash
  // frame — 0.07% — so the brightest light source in the player's world
  // illuminated the weapon and nothing else, which is the same "firing read as a
  // decal" failure the viewmodel light was added to fix with the boundary moved
  // one step out.
  //
  // On the WORLD pass it has to be a POINT light and not the viewmodel's
  // directional one: a directional term lights the far perimeter exactly as hard
  // as the crate you are shooting over, which is not a muzzle flash, it is a
  // lightning strike. `uMuzP` is the world muzzle and the falloff is a plain
  // inverse square with a soft core, so it reaches the wall you are hugging and
  // is arithmetically nothing at across-the-arena range.
  "  if (uVm < 0.5 && uMuzC.r > 0.001) {\n"
  "    vec3 md = uMuzP - vWorld;\n"
  "    float md2 = dot(md, md);\n"
  // k = 0.6 and radiance 8.0, swept against the wall-patch measurement rather
  // than guessed: 1/(1+d^2*0.6) puts a wall at 1.4 m up 7.2% in mean luminance
  // on the bright flash frame (it was +0.25%, i.e. nothing), 8 m at 0.0% and
  // 24 m at 0.0%. So it reaches the wall you are hugging and the crate you are
  // shooting over, and is arithmetically absent everywhere else.
  "    float att = 1.0 / (1.0 + md2 * 0.6);\n"
  "    if (att > 0.002)\n"
  "      col += lit(n, V, normalize(md), diff, F0, rough, uMuzC * att);\n"
  "  }\n"
  // Twice the aerial perspective. A figure at 30 m measured 4% off the wall
  // behind it, and no work on the figure buys contrast the ATMOSPHERE has to
  // provide: haze lifts and desaturates the wall, not the man in front of it.
  "  col += emi * (1.0 - fr);\n"
  // LAMPLIGHT: the nearest lanterns at dusk, warm, falling off with the
  // square of the distance and aimed down (the cap shades the sky).
  "  if (uLampOn > 0.02) {\n"
  "    for (int i = 0; i < 8; i++) {\n"
  "      vec3 lv = uLamp[i].xyz - vWorld;\n"
  "      float l2 = dot(lv, lv);\n"
  "      if (uLamp[i].w <= 0.0 || l2 > 144.0) continue;\n"
  "      vec3 L = lv * inversesqrt(l2);\n"
  "      float att = uLamp[i].w / (1.0 + l2 * 0.32) * (1.0 - smoothstep(64.0, 144.0, l2));\n"
  "      float spot = 0.25 + 0.75 * smoothstep(0.05, 0.6, L.y);\n"
  "      col += lit(n, V, L, diff, F0, rough, vec3(1.0, 0.72, 0.45) * att * spot);\n"
  "    }\n"
  "  }\n"
  "  col = mix(col, sky_env(-V, 0.0), 1.0 - exp(-d * d * uFog));\n"
  // ALPHA = "this is the static world". Nothing read the scene's alpha, so it
  // carries the one bit the composite needs to keep the edge catch-light (see
  // AO_FS) on the world's chamfered corners and OFF the moving geometry: the
  // figures are flat-faceted by design, every facet edge is a convex crease in
  // the depth buffer, and the catch-light outlined the whole body in a dashed,
  // glowing wireframe.
  "  oCol = vec4(tonemap(col), uDetail > 0.0 ? 1.0 : 0.0);\n"
  "}\n";

// Sky: fullscreen triangle at depth 1.0 drawn after the world (LEQUAL passes
// only where nothing else did — near-zero overdraw). The view ray is rebuilt
// from the camera basis; the horizon matches the world's fog by construction,
// since both call sky_env and both tonemap it.
static const char *SKY_VS =
  "#version 330 core\n"
  "out vec2 vNdc;\n"
  "void main(){\n"
  "  vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);\n"
  "  vNdc = p;\n"
  "  gl_Position = vec4(p, 1.0, 1.0);\n"
  "}\n";

// ...and ONE THIN CLOUD DECK, which lives in this pass and nowhere else. It is
// deliberately NOT in sky_env: that function is also the fog colour at the
// horizon and the environment every specular surface reflects, and a cloud
// field in either of those is a wall that changes colour as you walk and a
// rifle that mirrors weather. The deck fades out below rd.y = 0.24, so at the
// horizon — the one place the sky pass and the world fog meet — there is
// nothing there and the seam is preserved by construction.
//
// FOUR RULES, and the first two are what separate cloud from noise soup:
//   - The coverage is a hard THRESHOLD on the noise, not a multiply by it.
//     fbm used directly as an alpha is fuzz at every setting; gaps have to
//     reach true zero or the whole sky reads as haze.
//   - Coverage stays LOW. The reflex that 0.5 is balanced is wrong — half
//     coverage of soft noise is overcast grey mush. A quarter reads as weather.
//   - The domain is WARPED by its own low octave, which is what makes an edge
//     combed and wispy instead of a round blob. Blobs read as a noise texture;
//     asymmetry reads as air that has been moving.
//   - The lift is PROPORTIONAL to the sky behind it rather than a fixed white.
//     A white cloud against the zenith is the highest-contrast edge in the
//     frame, and enemy silhouettes are read by local contrast against exactly
//     that background. Proportional keeps the deck inside the sky's own band.
//
// The ray-plane divide is guarded three ways — a floor on rd.y, a clamp on the
// distance, and the alpha fade — because as rd.y goes to zero the sample point
// runs to infinity, its screen derivative explodes, and the noise there aliases
// into a shimmering band right along the horizon.
static const char *SKY_FS =
  "#version 330 core\n"
  GLSL_ENV
  "in vec2 vNdc; out vec4 oCol;\n"
  "uniform vec3 uFwd, uRight, uUp; uniform vec2 uTan;\n"
  "float deck(vec2 p){\n"
  "  float w = vnoise2(p * 0.55) - 0.5;\n"
  "  vec2 q = p + vec2(w, -w * 0.6) * 1.15;\n"
  "  return vnoise2(q) * 0.56 + vnoise2(q * 2.31) * 0.30 + vnoise2(q * 5.3) * 0.14;\n"
  "}\n"
  // Height of a mountain range around the horizon. Sampling on the unit circle
  // rather than on the azimuth angle leaves no seam behind the player.
  "float ridge(vec2 az, float f, float off){\n"
  "  vec2 p = az * f + off;\n"
  "  float a = 1.0 - abs(vnoise2(p) * 2.0 - 1.0);\n"
  "  float b = 1.0 - abs(vnoise2(p * 2.7 + 3.0) * 2.0 - 1.0);\n"
  "  float c = 1.0 - abs(vnoise2(p * 7.1 + 9.0) * 2.0 - 1.0);\n"
  "  return a * a * 0.62 + b * b * 0.28 + c * 0.10;\n"
  "}\n"
  "void main(){\n"
  "  vec3 dir = normalize(uFwd + uRight * (vNdc.x * uTan.x) + uUp * (vNdc.y * uTan.y));\n"
  "  vec3 c = sky_env(dir, 1.0);\n"
  "  float up = dir.y;\n"
  "  if (up > 0.03) {\n"
  "    float t = min(1.0 / max(up, 0.05), 14.0);\n"
  "    vec2 p = dir.xz * t * 1.05;\n"
  "    float n = deck(p);\n"
  "    float cov = uCloud;\n"
  "    float dens = smoothstep(1.0 - cov, 1.0 - cov + 0.24, n);\n"
  "    dens *= smoothstep(0.03, 0.22, up);\n"
  "    if (dens > 0.002) {\n"
  // One offset sample TOWARD the sun, no march: where the deck is thick on the
  // sun side, this pixel is behind it and its underside goes grey. That single
  // extra tap is what gives a cloud a dark base instead of a uniform dimming.
  "      float d2 = smoothstep(1.0 - cov, 1.0 - cov + 0.24,\n"
  "                            deck(p + normalize(SUN_DIR.xz) * 0.42));\n"
  "      float litc = exp(-d2 * 1.9) * (1.0 - exp(-d2 * 2.0 - 0.55));\n"
  "      vec3 base = sky_env(dir, 0.0);\n"
  "      float sd = max(dot(dir, SUN_DIR), 0.0);\n"
  // Forward scattering: a cloud between the eye and the sun lights up at its
  // thin edges — the silver lining — which a symmetric lobe never produces.
  // Two Henyey-Greenstein lobes, a sharp forward one and a weak back one.
  "      float hgf = (1.0 - 0.36) / pow(1.0 + 0.36 - 1.2 * sd, 1.5);\n"
  "      float hgb = (1.0 - 0.09) / pow(1.0 + 0.09 + 0.6 * sd, 1.5);\n"
  "      float ph = mix(hgb, hgf, 0.72) * 0.45;\n"
  "      float edge = dens * (1.0 - dens) * 4.0;\n"
  "      vec3 cc = base * mix(0.92, 1.70, litc) + SUN_COL * litc * (0.20 + 0.65 * sd * sd * sd)\n"
  "              + SUN_COL * edge * ph * 0.30;\n"
  "      c = mix(c, cc, dens * 0.92);\n"
  "    }\n"
  // THE SECOND LAYER: high cirrus, three times further up, stretched along
  // the wind and much thinner. It is what puts depth into the sky — two layers
  // sliding at different parallax read as altitude, one reads as a ceiling.
  "    float t2 = min(3.2 / max(up, 0.05), 40.0);\n"
  "    vec2 p2 = dir.xz * t2 * 0.35;\n"
  "    vec2 wd = vec2(0.80, 0.60);\n"
  "    vec2 pr = vec2(dot(p2, wd), dot(p2, vec2(-wd.y, wd.x)) * 4.0);\n"
  "    float ci = vnoise2(pr * 0.7) * 0.6 + vnoise2(pr * 2.1 + 5.0) * 0.3 + vnoise2(pr * 6.3) * 0.1;\n"
  "    float cd = smoothstep(0.66, 0.90, ci) * smoothstep(0.08, 0.34, up) * (0.25 + 0.75 * uCloud);\n"
  "    if (cd > 0.002) {\n"
  "      float sd2 = max(dot(dir, SUN_DIR), 0.0);\n"
  "      float hg2 = (1.0 - 0.49) / pow(1.0 + 0.49 - 1.4 * sd2, 1.5);\n"
  "      vec3 ic = sky_env(dir, 0.0) * 1.25 + SUN_COL * (0.10 + 0.22 * hg2);\n"
  "      c = mix(c, ic, cd * 0.32);\n"
  "    }\n"
  "  }\n"
  // Two mountain ranges beyond the arena walls, hazed by distance. The near one
  // also closes the space below the horizon, which is what a raised camera sees.
  "  vec2 az = normalize(dir.xz + vec2(1e-5));\n"
  "  float aa = max(fwidth(up), 1e-4);\n"
  "  vec3 hz = sky_env(normalize(vec3(dir.x, 0.012, dir.z)), 0.0);\n"
  "  vec3 far = hz * mix(vec3(0.80), SKY_TOP * 1.9, 0.24);\n"
  "  vec3 nr = hz * mix(vec3(0.52), SKY_GND * 1.4, 0.30);\n"
  "  float h1 = 0.006 + 0.082 * ridge(az, 3.1, 7.0);\n"
  "  c = mix(c, mix(hz, far, smoothstep(-0.01, h1, up)),\n"
  "          1.0 - smoothstep(h1 - aa, h1 + aa, up));\n"
  "  float h2 = -0.004 + 0.046 * ridge(az, 5.3, 41.0);\n"
  "  c = mix(c, mix(hz * 0.86, nr, smoothstep(-0.025, h2, up)),\n"
  "          1.0 - smoothstep(h2 - aa, h2 + aa, up));\n"
  // Below the ranges a raised camera sees open desert running to their feet.
  "  c = mix(c, mix(hz * 0.78, SKY_GND * 0.82, smoothstep(0.0, 0.26, -up)),\n"
  "          smoothstep(-0.002, -0.022, up));\n"
  "  oCol = vec4(tonemap(c), 1.0);\n"
  "}\n";

// Frosted-glass menu backdrop: an attribute-less fullscreen triangle and one
// dual-Kawase tap, ping-ponged a few times at quarter resolution — the
// logarithmic-cost blur every modern menu uses, in two tiny shaders.
static const char *BLUR_VS =
  "#version 330 core\n"
  "out vec2 vUV;\n"
  "void main(){ vec2 p = vec2(gl_VertexID==1?3.0:-1.0, gl_VertexID==2?3.0:-1.0);\n"
  "  vUV = p * 0.5 + 0.5; gl_Position = vec4(p, 0.0, 1.0); }\n";
static const char *BLUR_FS =
  "#version 330 core\n"
  "in vec2 vUV; uniform sampler2D uTex; uniform vec2 uPix;\n"
  "out vec4 oCol;\n"
  "void main(){\n"
  "  vec3 c = texture(uTex, vUV).rgb * 4.0;\n"
  "  c += texture(uTex, vUV + uPix).rgb;\n"
  "  c += texture(uTex, vUV - uPix).rgb;\n"
  "  c += texture(uTex, vUV + vec2(uPix.x, -uPix.y)).rgb;\n"
  "  c += texture(uTex, vUV - vec2(uPix.x, -uPix.y)).rgb;\n"
  "  oCol = vec4(c * 0.125, 1.0); }\n";   // exact, and three fdiv fewer

// Post stack: the tonemapped frame is already display-referred, so bloom keys on
// what survives near the top of the range. A soft knee keeps it from popping.
