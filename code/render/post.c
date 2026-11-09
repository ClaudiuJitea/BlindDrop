// code/render/post.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ===========================================================================
// THE HDR POST CHAIN. Every lit shader writes LINEAR radiance (already scaled by
// the preset's EXPOSURE, see tonemap() in GLSL_ENV) into an RGBA16F scene
// target, and the display transform happens exactly once, here. That is what
// lets a muzzle flash be brighter than the sky instead of both clipping at
// 255, lets the bloom see energy rather than an already-compressed colour, and
// lets an eye adaptation move the exposure without re-shading the frame.
//   METER  : centre-weighted log luminance into a mipmapped RG16F target
//   ADAPT  : 1x1 ping-pong, exponential approach to the metered key
//   AO     : half res — SSAO, screen-space contact shadows and convex-edge
//            catch-lights, all from the world depth (taken before the
//            viewmodel pass, so the gun neither occludes nor is occluded)
//   VOL    : half res — sun in-scattering raymarched through the shadow map
//   BLOOM  : dual-Kawase, five levels down and back up
//   POST   : composite, motion blur, viewmodel depth of field, heat haze,
//            chromatic aberration, ACES, grade, vignette, dither
// ===========================================================================
#define GLSL_DEPTH \
  "uniform vec2 uTanXY;\n"   /* tan of the half FOV, x and y */ \
  "uniform vec2 uNF;\n"      /* near, far */ \
  "float lin_z(float d){ float z = d * 2.0 - 1.0;\n" \
  "  return 2.0 * uNF.x * uNF.y / (uNF.y + uNF.x - z * (uNF.y - uNF.x)); }\n" \
  "vec3 view_pos(vec2 uv, float d){ float z = lin_z(d);\n" \
  "  return vec3((uv * 2.0 - 1.0) * uTanXY * z, -z); }\n" \
  "float ign(vec2 f){ return fract(52.9829189 * fract(dot(f, vec2(0.06711056, 0.00583715)))); }\n"

static const char *LUM_FS =
  "#version 330 core\n"
  "in vec2 vUV; uniform sampler2D uTex; uniform sampler2D uVmD; out vec4 oCol;\n"
  "void main(){\n"
  "  vec3 c = texture(uTex, vUV).rgb;\n"
  "  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
  // Centre-weighted, and the player's own weapon does not vote: a dark rifle
  // filling the bottom third would otherwise brighten the whole world.
  "  vec2 q = vUV - vec2(0.5, 0.55);\n"
  "  float w = exp(-dot(q, q) * 5.0) * (texture(uVmD, vUV).r < 1.0 ? 0.05 : 1.0);\n"
  "  oCol = vec4(log(max(l, 1e-4)) * w, w, 0.0, 1.0);\n"
  "}\n";

static const char *ADAPT_FS =
  "#version 330 core\n"
  "in vec2 vUV; uniform sampler2D uLum, uPrev; uniform float uLod, uK, uKey;\n"
  "uniform vec2 uRange; out vec4 oCol;\n"
  "void main(){\n"
  "  vec2 m = textureLod(uLum, vec2(0.5), uLod).rg;\n"
  "  float avg = exp(m.r / max(m.g, 1e-6));\n"
  // HALF the metered error, as a camera's exposure-compensation curve does:
  // a town at dusk is SUPPOSED to be darker than an arena at noon, and a full
  // correction would grade every map to the same grey. Dark interiors still
  // get a stop of help; a lit courtyard is left as authored.
  "  float tgt = clamp(0.5 * log2(uKey / max(avg, 1e-4)), uRange.x, uRange.y);\n"
  // A dead band around neutral: a frame that is already well exposed does
  // not drift, so walking across one sunlit courtyard holds still.
  "  tgt = sign(tgt) * max(abs(tgt) - 0.40, 0.0);\n"
  "  float prev = texture(uPrev, vec2(0.5)).r;\n"
  "  oCol = vec4(mix(prev, tgt, uK), avg, 0.0, 1.0);\n"
  "}\n";

static const char *AO_FS =
  "#version 330 core\n"
  GLSL_DEPTH
  "in vec2 vUV; uniform sampler2D uDepth; uniform vec2 uPix;\n"
  "uniform vec3 uSunV;\n"     // sun direction in VIEW space
  // The sun shadow map, for the catch-light below: a glint is the SUN on a
  // chamfer, and a chamfer in shade has none. uSmOn 0 = no map bound.
  "uniform sampler2DShadow uSm; uniform mat4 uInvVP, uLightVP; uniform vec3 uSunW;\n"
  "uniform float uSmOn;\n"
  "out vec4 oCol;\n"
  // Reconstruct at the depth TEXEL's centre, never at the sample's own uv: a
  // nearest-filtered depth paired with an off-centre position is a sawtooth on
  // every slanted surface, and the normal built from it striped every wall.
  "vec2 snapuv(vec2 uv){ vec2 ts = vec2(textureSize(uDepth, 0)); return (floor(uv * ts) + 0.5) / ts; }\n"
  "vec3 P(vec2 uv){ uv = snapuv(uv); return view_pos(uv, texture(uDepth, uv).r); }\n"
  "void main(){\n"
  "  vec2 cuv = snapuv(vUV);\n"
  "  float d = texture(uDepth, cuv).r;\n"
  "  if (d >= 1.0) { oCol = vec4(1.0, 1.0, 0.0, 1.0); return; }\n"
  "  vec3 p = view_pos(cuv, d);\n"
  // Normal from the SMALLER of the two one-sided differences on each axis, so
  // a pixel on a silhouette takes its own face's slope, not the gap's.
  "  vec3 px1 = P(cuv + vec2(uPix.x, 0.0)) - p, px0 = p - P(cuv - vec2(uPix.x, 0.0));\n"
  "  vec3 py1 = P(cuv + vec2(0.0, uPix.y)) - p, py0 = p - P(cuv - vec2(0.0, uPix.y));\n"
  "  vec3 dx = abs(px1.z) < abs(px0.z) ? px1 : px0;\n"
  "  vec3 dy = abs(py1.z) < abs(py0.z) ? py1 : py0;\n"
  "  vec3 n = normalize(cross(dx, dy));\n"
  "  float z = -p.z;\n"
  "  float jit = ign(gl_FragCoord.xy) * 6.2831853;\n"
  // --- SSAO: a 12-tap Alchemy estimator over a 0.55 m world-space disc.
  "  float rad = 0.55;\n"
  "  vec2 rs = rad / (z * uTanXY * 2.0);\n"
  "  float occ = 0.0;\n"
  "  for (int i = 0; i < 12; i++) {\n"
  "    float a = jit + float(i) * 2.3999632;\n"           // golden angle
  "    float r = sqrt((float(i) + 0.5) / 12.0);\n"
  "    vec2 o = vec2(cos(a), sin(a)) * r * rs;\n"
  "    vec3 v = P(cuv + o) - p;\n"
  "    float vv = dot(v, v);\n"
  "    occ += max(dot(v, n) - 0.012 * z, 0.0) / (vv + 0.004)\n"
  "         * (1.0 - smoothstep(rad * rad, rad * rad * 4.0, vv));\n"
  "  }\n"
  "  float ao = clamp(1.0 - occ * (2.0 * 0.11 / 12.0), 0.0, 1.0);\n"
  "  ao = ao * ao;\n"
  // --- CONTACT SHADOWS: what the 15-30 mm shadow texel cannot hold. A short
  // ray toward the sun in view space; any depth sample in front of the ray by
  // less than a hand's thickness is a blocker.
  "  float cs = 1.0;\n"
  "  if (dot(n, uSunV) > 0.02 && z < 40.0) {\n"
  "    float len = mix(0.10, 0.55, clamp(z / 30.0, 0.0, 1.0));\n"
  "    vec3 q = p + n * 0.01 * z;\n"
  "    for (int i = 1; i <= 10; i++) {\n"
  "      float t = (float(i) + jit * 0.159) / 10.0;\n"
  "      vec3 r = q + uSunV * (len * t * t);\n"
  "      vec2 ruv = (r.xy / (-r.z * uTanXY)) * 0.5 + 0.5;\n"
  "      if (ruv.x < 0.0 || ruv.y < 0.0 || ruv.x > 1.0 || ruv.y > 1.0) break;\n"
  "      float sz = lin_z(texture(uDepth, ruv).r);\n"
  "      float dz = -r.z - sz;\n"
  "      if (dz > 0.015 && dz < 0.35) { cs = 1.0 - (1.0 - t) * 0.9; break; }\n"
  "    }\n"
  "  }\n"
  // --- EDGE CATCH-LIGHTS: a real wall corner is never knife-sharp, and the
  // 2-3 mm chamfer on it is where the sun glints. Convex creases are found in
  // the depth itself: a neighbour that falls away behind this pixel's plane
  // with a different normal is the other face of an outside corner.
  "  float edge = 0.0;\n"
  "  vec3 en = n;\n"
  "  for (int i = 0; i < 4; i++) {\n"
  "    vec2 o = (i == 0 ? vec2(1, 0) : i == 1 ? vec2(-1, 0) : i == 2 ? vec2(0, 1) : vec2(0, -1)) * uPix * 1.5;\n"
  "    vec3 a = P(cuv + o) - p;\n"
  "    float behind = -dot(a, n) / (length(a) + 1e-4);\n"
  "    float e = smoothstep(0.08, 0.45, behind) * (1.0 - smoothstep(0.08, 0.30, length(a) / z));\n"
  "    if (e > edge) { edge = e; en = normalize(n + normalize(a - n * dot(a, n))); }\n"
  "  }\n"
  "  edge *= max(dot(en, uSunV), 0.0) * (1.0 - smoothstep(6.0, 22.0, z));\n"
  // ...AND ONLY WHERE THE SUN ACTUALLY REACHES. The term was gated on the
  // facing alone, so a figure standing in a building's shade had every bevel
  // of every limb outlined in sunlight — and with the crease found from
  // quantized depth, the outline came out as a dashed rule along each edge
  // (the refinery's grazing sun made it the whole figure). The point is pulled
  // 6 cm toward the sun first: the edge pixel IS a front face in the map, and
  // testing it at its own depth would shadow every lit corner by itself.
  "  if (edge > 0.0 && uSmOn > 0.5) {\n"
  "    vec4 w = uInvVP * vec4(cuv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);\n"
  "    vec4 l = uLightVP * vec4(w.xyz / w.w + uSunW * 0.06, 1.0);\n"
  "    vec3 lp = l.xyz / l.w * 0.5 + 0.5;\n"
  "    if (lp.z < 1.0) {\n"
  "      vec2 t = 1.0 / vec2(textureSize(uSm, 0));\n"
  "      edge *= 0.25 * (texture(uSm, vec3(lp.xy + vec2(-t.x, -t.y), lp.z))\n"
  "                    + texture(uSm, vec3(lp.xy + vec2( t.x, -t.y), lp.z))\n"
  "                    + texture(uSm, vec3(lp.xy + vec2(-t.x,  t.y), lp.z))\n"
  "                    + texture(uSm, vec3(lp.xy + vec2( t.x,  t.y), lp.z)));\n"
  "    }\n"
  "  }\n"
  "  oCol = vec4(ao, cs, edge, 1.0);\n"
  "}\n";

static const char *VOL_FS =
  "#version 330 core\n"
  "in vec2 vUV; uniform sampler2D uDepth; uniform sampler2DShadow uSm;\n"
  "uniform mat4 uInvVP, uLightVP; uniform vec3 uEye, uSun, uSunC; uniform float uDens, uExp;\n"
  "out vec4 oCol;\n"
  "float ign(vec2 f){ return fract(52.9829189 * fract(dot(f, vec2(0.06711056, 0.00583715)))); }\n"
  "void main(){\n"
  "  float d = texture(uDepth, vUV).r;\n"
  "  vec4 w = uInvVP * vec4(vUV * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);\n"
  "  vec3 wp = w.xyz / w.w;\n"
  "  vec3 rd = wp - uEye; float L = length(rd); rd /= L;\n"
  "  L = min(L, 70.0);\n"
  "  float mu = dot(rd, uSun);\n"
  // Henyey-Greenstein, g = 0.72: a dusty atmosphere scatters forward, so the
  // shafts are a feature of looking TOWARD the sun and nearly vanish away from it.
  "  float g = 0.72, ph = (1.0 - g * g) / (12.566 * pow(1.0 + g * g - 2.0 * g * mu, 1.5));\n"
  "  ph += 0.02;\n"
  "  const int N = 20;\n"
  "  float st = L / float(N), j = ign(gl_FragCoord.xy), acc = 0.0;\n"
  "  for (int i = 0; i < N; i++) {\n"
  "    vec3 x = uEye + rd * ((float(i) + j) * st);\n"
  "    vec4 l = uLightVP * vec4(x, 1.0);\n"
  "    vec3 lp = l.xyz / l.w * 0.5 + 0.5;\n"
  "    float lit = (lp.z >= 1.0) ? 1.0 : texture(uSm, lp);\n"
  "    acc += lit * exp(-x.y * 0.05);\n"   // a little denser near the ground
  "  }\n"
  "  vec3 ins = uSunC * ph * uDens * acc * st * uExp;\n"
  "  oCol = vec4(ins, 1.0);\n"
  "}\n";

static const char *BDOWN_FS =
  "#version 330 core\n"
  "in vec2 vUV; uniform sampler2D uTex; uniform vec2 uHp; uniform float uThr; out vec4 oCol;\n"
  "vec3 S(vec2 o){ vec3 c = texture(uTex, vUV + o).rgb;\n"
  // Soft-knee threshold on the first level only (uThr < 0 passes all). The
  // luminance clamp keeps a single sun-glint pixel from becoming a disc.
  "  if (uThr >= 0.0) { float l = max(c.r, max(c.g, c.b));\n"
  "    float k = clamp(l - uThr + 0.5, 0.0, 1.0); k = k * k * 0.5;\n"
  "    c *= max(k, l - uThr) / max(l, 1e-4); c = min(c, vec3(24.0)); }\n"
  "  return c; }\n"
  "void main(){\n"
  "  vec3 c = S(vec2(0.0)) * 4.0 + S(-uHp) + S(uHp) + S(vec2(uHp.x, -uHp.y)) + S(vec2(-uHp.x, uHp.y));\n"
  "  oCol = vec4(c * 0.125, 1.0);\n"
  "}\n";
static const char *BUP_FS =
  "#version 330 core\n"
  "in vec2 vUV; uniform sampler2D uTex; uniform vec2 uHp; out vec4 oCol;\n"
  "vec3 S(vec2 o){ return texture(uTex, vUV + o * uHp).rgb; }\n"
  "void main(){\n"
  "  vec3 c = S(vec2(-2, 0)) + S(vec2(2, 0)) + S(vec2(0, -2)) + S(vec2(0, 2))\n"
  "         + (S(vec2(-1, 1)) + S(vec2(1, 1)) + S(vec2(1, -1)) + S(vec2(-1, -1))) * 2.0;\n"
  "  oCol = vec4(c / 12.0, 1.0);\n"
  "}\n";

// The display transform, shared by the frame and by the scope picture (which
// is composited in the HUD pass, after this one).
#define GLSL_DISPLAY \
  "vec3 aces(vec3 x){ return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }\n" \
  "vec3 grade(vec3 c, vec2 uv, float fx){\n" \
  "  if (fx < 0.5) return c;\n" \
  "  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));\n" \
  "  c = mix(vec3(l), c, uSat) * uTint;\n" \
  "  c = mix(c, c * c * (3.0 - 2.0 * c), uCon);\n" \
  "  c += uLift * (1.0 - l) * (1.0 - l);\n" \
  "  vec2 q = uv - 0.5;\n" \
  "  return c * (1.0 - uVig * smoothstep(0.16, 0.74, dot(q, q) * 2.0));\n" \
  "}\n"

static const char *POST_FS =
  "#version 330 core\n"
  GLSL_DEPTH
  "in vec2 vUV;\n"
  "uniform sampler2D uTex, uAO, uVol, uBloom, uAdapt, uDepth, uVmD;\n"
  "uniform float uSat, uCon, uVig; uniform vec3 uTint, uLift;\n"
  "uniform float uFx, uBloomK, uCA, uMotion, uAds, uVmOn, uTime, uAspect;\n"
  "uniform vec4 uHeat;\n"       // screen xy, radius, strength
  "uniform float uDbg;\n"
  "uniform vec2 uVmVel;\n"      // the weapon's own screen motion this frame
  "uniform vec3 uSunC;\n"
  "uniform mat4 uInvVP, uPrevVP;\n"
  "out vec4 oCol;\n"
  GLSL_DISPLAY
  "void main(){\n"
  "  vec2 uv = vUV;\n"
  // Heat haze over a barrel that has been working: a refractive shimmer
  // that rises, centred on the muzzle's screen position.
  "  if (uHeat.w > 0.001 && uFx > 0.5) {\n"
  "    vec2 hd = (uv - uHeat.xy) * vec2(uAspect, 1.0);\n"
  "    float hf = exp(-dot(hd, hd) / (uHeat.z * uHeat.z)) * smoothstep(-0.05, 0.10, uv.y - uHeat.y + 0.06);\n"
  "    uv += vec2(sin(uv.y * 140.0 - uTime * 19.0), cos(uv.x * 110.0 + uTime * 13.0))\n"
  "          * 0.0022 * uHeat.w * hf;\n"
  "  }\n"
  "  float zw = texture(uDepth, uv).r;\n"
  "  float zv = texture(uVmD, uv).r;\n"
  "  bool vm = uVmOn > 0.5 && zv < 1.0;\n"
  "  vec2 q = uv - 0.5;\n"
  "  float r2 = dot(q, q);\n"
  "  vec3 c;\n"
  "  if (uFx > 0.5) {\n"
  // Lateral chromatic aberration: zero over the middle of the frame, where
  // the aim point is, growing with the square of the radius.
  "    vec2 off = q * uCA * r2;\n"
  "    c = vec3(texture(uTex, uv - off).r, texture(uTex, uv).g, texture(uTex, uv + off).b);\n"
  "  } else c = texture(uTex, uv).rgb;\n"
  "  if (uFx > 0.5 && !vm && uMotion > 0.0) {\n"
  // Camera motion blur by reprojection: where this pixel's world point sat on
  // last frame's screen. Only the camera's motion — the world is static and a
  // figure's own motion is too small to read at 120 Hz.
  "    vec4 w = uInvVP * vec4(uv * 2.0 - 1.0, zw * 2.0 - 1.0, 1.0);\n"
  "    vec4 pp = uPrevVP * vec4(w.xyz / w.w, 1.0);\n"
  "    vec2 vel = (uv - (pp.xy / pp.w * 0.5 + 0.5)) * uMotion;\n"
  "    float vl = length(vel);\n"
  "    if (vl > 0.0015) {\n"
  "      vel *= min(vl, 0.022) / vl;\n"
  "      vec3 acc = c; float wsum = 1.0;\n"
  "      float j = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n"
  "      for (int i = 1; i <= 6; i++) {\n"
  "        vec2 su = uv - vel * ((float(i) - j) / 6.0 - 0.5);\n"
  "        if (uVmOn > 0.5 && texture(uVmD, su).r < 1.0) continue;\n"
  "        acc += texture(uTex, su).rgb; wsum += 1.0;\n"
  "      }\n"
  "      c = acc / wsum;\n"
  "    }\n"
  "  }\n"
  // WEAPON MOTION BLUR: the viewmodel smears along its own screen motion this
  // frame (its springs, not the camera's), sampling only weapon pixels so the
  // world never streaks into the gun. Taps are centred, so the blur has no
  // direction bias and needs none.
  "  if (vm && uFx > 0.5 && dot(uVmVel, uVmVel) > 1e-6) {\n"
  "    vec2 vv = uVmVel * vec2(1.0, uAspect);\n"
  "    float vl = length(vv); if (vl > 0.02) vv *= 0.02 / vl;\n"
  "    vec3 acc = c; float wsum = 1.0;\n"
  "    for (int i = 0; i < 6; i++) {\n"
  "      vec2 su = uv + vv * ((float(i) + 0.5) / 6.0 - 0.5);\n"
  "      if (texture(uVmD, su).r >= 1.0) continue;\n"
  "      acc += texture(uTex, su).rgb; wsum += 1.0;\n"
  "    }\n"
  "    c = acc / wsum;\n"
  "  }\n"
  "  if (vm && uAds > 0.02 && uFx > 0.5) {\n"
  // Depth of field on the weapon itself while aimed: the eye focuses on the
  // front sight and the target, so the receiver and the rear of the optic —
  // the parts nearest the face — go soft. Circle of confusion from the
  // viewmodel's own depth; taps that land off the weapon are rejected so the
  // world never bleeds into the gun.
  "    float vz = 2.0 * 0.01 * 5.0 / (5.0 + 0.01 - (zv * 2.0 - 1.0) * (5.0 - 0.01));\n"
  "    float coc = uAds * smoothstep(0.30, 0.10, vz) * 0.0045;\n"
  "    if (coc > 0.0004) {\n"
  "      vec3 acc = c; float wsum = 1.0;\n"
  "      for (int i = 0; i < 10; i++) {\n"
  "        float a = float(i) * 2.3999632, r = sqrt((float(i) + 0.5) / 10.0);\n"
  "        vec2 su = uv + vec2(cos(a), sin(a) * uAspect) * r * coc;\n"
  "        if (texture(uVmD, su).r >= 1.0) continue;\n"
  "        acc += texture(uTex, su).rgb; wsum += 1.0;\n"
  "      }\n"
  "      c = acc / wsum;\n"
  "    }\n"
  "  }\n"
  "  if (!vm && zw < 1.0 && uFx > 0.5) {\n"
  // Depth-aware 4-tap gather of the half-res occlusion, so a wall's AO does
  // not halo onto the sky or onto a figure standing in front of it.
  "    vec2 hp = 1.0 / vec2(textureSize(uAO, 0));\n"
  "    float zc = lin_z(zw);\n"
  "    vec3 a = vec3(0.0); float ws = 0.0;\n"
  "    for (int i = 0; i < 4; i++) {\n"
  "      vec2 o = vec2(i & 1, i >> 1) * hp - hp * 0.5;\n"
  "      float zz = lin_z(texture(uDepth, uv + o).r);\n"
  "      float wt = 1.0 / (0.02 + abs(zz - zc) / zc);\n"
  "      a += texture(uAO, uv + o).rgb * wt; ws += wt;\n"
  "    }\n"
  "    a /= ws;\n"
  "    c *= mix(1.0, a.r, 0.80) * mix(1.0, a.g, 0.55);\n"
  "    c += uSunC * a.b * 0.10 * a.g * texture(uTex, uv).a;\n"
  "  }\n"
  "  if (!vm && uFx > 0.5) c += texture(uVol, uv).rgb;\n"
  "  if (uFx > 0.5) c += texture(uBloom, uv).rgb * uBloomK;\n"
  "  if (uDbg > 0.5) {\n"     // `postdbg N` in the harness: one channel, raw
  "    vec4 a = texture(uAO, uv);\n"
  "    c = uDbg < 1.5 ? a.rrr : uDbg < 2.5 ? a.ggg : uDbg < 3.5 ? a.bbb\n"
  "      : uDbg < 4.5 ? texture(uVol, uv).rgb * 4.0 : texture(uBloom, uv).rgb;\n"
  "    oCol = vec4(c, 1.0); return;\n"
  "  }\n"
  "  c *= exp2(texture(uAdapt, vec2(0.5)).r);\n"
  "  c = sqrt(aces(c));\n"
  "  c = grade(c, vUV, uFx);\n"
  "  float dth = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n"
  "  oCol = vec4(c + (dth - 0.5) * 0.0039216, 1.0);\n"
  "}\n";

static const char *HUD_VS =
  "#version 330 core\n"
  "layout(location=0) in vec2 aPos;\n"   // pixels, origin top-left
  "layout(location=1) in vec2 aUV;\n"
  "layout(location=2) in vec4 aCol;\n"
  "layout(location=3) in float aMode;\n" // <1 = SDF threshold, >=2 = raw alpha
  "uniform vec2 uScreen;\n"
  "out vec2 vUV; out vec4 vCol; out float vMode;\n"
  "void main(){ vUV=aUV; vCol=aCol; vMode=aMode;\n"
  "  gl_Position=vec4(2.0*aPos.x/uScreen.x-1.0, 1.0-2.0*aPos.y/uScreen.y, 0.0, 1.0); }\n";

// uImg selects what the bound texture MEANS: 0 = the glyph atlas, 1 = an image
// (RGB straight through, tinted for the scope's slight glass warmth).
// The atlas stores 1 - distance-to-stroke-skeleton (normalized over HF_DMAX
// px), so aMode < 1 IS the stroke weight: thresholding closer to 1 gives a
// thinner stroke, closer to 0 a bolder one — every text weight, outline and
// pick-thickness comes out of ONE atlas in ONE draw call. aMode >= 2 bypasses
// the threshold and uses the texel as raw alpha (solid slot, soft glow slot).
static const char *HUD_FS =
  "#version 330 core\n"
  "in vec2 vUV; in vec4 vCol; in float vMode;\n"
  "uniform sampler2D uTex; uniform int uImg;\n"
  "uniform sampler2D uAdapt;\n"
  "uniform float uSat, uCon, uVig, uFx; uniform vec3 uTint, uLift;\n"
  "uniform vec4 uLens;\n"            // scope centre (uv), eye misalignment, CA
  "out vec4 oCol;\n"
  GLSL_DISPLAY
  "void main(){\n"
  "  if (uImg == 1) { oCol = vec4(texture(uTex, vUV).rgb * vCol.rgb, vCol.a); return; }\n"
  // THE SCOPE PICTURE, which is HDR (it is a second render of the world) and so
  // gets the same display transform as the frame, plus the optics of a real
  // ocular: barrel distortion toward the rim, lateral colour at the field stop,
  // and an exit pupil that only lines up when the eye is on the axis.
  "  if (uImg == 2) {\n"
  "    vec2 d = vUV - 0.5;\n"
  "    float r2 = dot(d, d) * 4.0;\n"
  "    vec2 bd = d * (1.0 - 0.10 * r2);\n"
  "    vec2 ca = d * uLens.w * r2;\n"
  "    vec3 c = vec3(texture(uTex, 0.5 + bd - ca).r, texture(uTex, 0.5 + bd).g,\n"
  "                  texture(uTex, 0.5 + bd + ca).b);\n"
  "    c *= exp2(texture(uAdapt, vec2(0.5)).r);\n"
  "    c = grade(sqrt(aces(c)), vec2(0.5), uFx);\n"
  // Eye-relief shadow: a crescent that sweeps in from the side the eye is off
  // by, and closes the picture down when the eye is far off the axis.
  "    vec2 ec = d * 2.0 - uLens.xy;\n"
  "    float eb = 1.0 - smoothstep(0.80 - uLens.z * 0.6, 1.10 - uLens.z * 0.6, length(ec));\n"
  "    c *= mix(0.0, 1.0, eb) * vCol.rgb;\n"
  "    oCol = vec4(c, vCol.a); return;\n"
  "  }\n"
  "  float v = texture(uTex, vUV).r;\n"
  "  float a;\n"
  "  if (vMode >= 2.0) a = v;\n"
  "  else { float w = max(fwidth(v), 1e-4); a = smoothstep(vMode - w, vMode + w, v); }\n"
  "  oCol = vec4(vCol.rgb, vCol.a * a); }\n";

