// code/render/renderer.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Renderer — GL 3.3 core. World: one static VBO, flat-lit boxes + checkered
// ground, distance fog. HUD: 5x7 bitmap font (digits + F/P/S), pixel-space
// quads, used for the FPS counter top-right.
// ---------------------------------------------------------------------------

// The sky model, the tonemap and the light rig live in ONE shared snippet
// because three consumers have to agree on them to the last digit: the sky
// pass itself, the world's aerial perspective (distant geometry fades into the
// sky that is actually BEHIND it, not into a hand-matched constant) and the
// environment reflection that gives metal its sheen. Matching three copies of a
// horizon color by hand is exactly the bug this avoids.
// Everything in here is LINEAR radiance; `tonemap` is the only place that
// converts to display values, so it is also the only place that has to change
// when the look is retuned.
// The viewmodel has its own projection AND its own depth clear, so the world
// shadow map does not cover it: the sun term alone leaves a black rifle an
// order of magnitude under the ground behind it, which is what "shot at a
// different exposure" looks like. Measured against the WORLD pass for the same
// albedo and roughness, the hero rig was about a stop short. Lifted — but only
// two thirds of the way: a full stop blew the slate SLEEVES out to near-white,
// which is the other failure mode. The weapon's own albedo floor carried the
// rest of that fix. Calibrated against the world pass, not eyeballed. (The note lives outside the macro: a backslash-continued macro
// cannot contain a comment.)
//
// The sun and sky are per-arena uniforms: see ATM below.
#define GLSL_NUM2(v) #v
#define GLSL_NUM(v)  GLSL_NUM2(v)
// The floor's baseline roughness, and it lives up here because the FRAGMENT
// SHADER needs it too: mesh_ground_vert writes looseness into the roughness
// channel as a signed offset from this value, and the ripple field reads it
// back to decide where loose sand exists at all. Two readers, one number —
// a hand-typed twin in the GLSL is how a compacted lane ends up rippled.
#define WORLD_GROUND_R 0.86f   // dry dusty floor: a wide, weak sheen at grazing
#define GLSL_ENV \
  /* Smooth value noise, SHARED. It lives here rather than in WORLD_FS because
     the sky pass needs the same function for its cloud layer, and two shaders
     each carrying their own copy of a noise basis is two chances for the ground
     and the sky to disagree about what a metre of noise is. */ \
  /* NO SINE. The fract(sin(dot(...))*k) idiom is one line and it is also, on a
     software rasteriser, a transcendental per lookup — and every noise here
     costs four of them. Measured on llvmpipe: the cloud deck alone evaluates
     twenty-four of those per sky pixel and the glitter field six per ground
     pixel, which was 0.87 and 0.61 ms/frame respectively. The fract-multiply
     construction is pure ALU, has better distribution than sin at large
     coordinates (where sin's argument reduction runs out of mantissa and the
     "random" numbers start repeating), and cost 1.1 ms/frame less. */ \
  "float vhash2(vec2 p){\n" \
  "  vec3 q = fract(vec3(p.xyx) * 0.1031);\n" \
  "  q += dot(q, q.yzx + 33.33);\n" \
  "  return fract((q.x + q.y) * q.z);\n" \
  "}\n" \
  "vec3 vhash3(vec2 p){\n" \
  "  vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));\n" \
  "  q += dot(q, q.yxz + 33.33);\n" \
  "  return fract((q.xxy + q.yzz) * q.zyx);\n" \
  "}\n" \
  "float vnoise2(vec2 p){\n" \
  "  vec2 i = floor(p), f = fract(p);\n" \
  "  f = f * f * (3.0 - 2.0 * f);\n" \
  "  return mix(mix(vhash2(i), vhash2(i + vec2(1.0, 0.0)), f.x),\n" \
  "             mix(vhash2(i + vec2(0.0, 1.0)), vhash2(i + vec2(1.0, 1.0)), f.x), f.y);\n" \
  "}\n" \
  /* 38 degrees up, not 62. A near-overhead sun lights three 90-degrees-apart
     faces of the same box to within a few percent of each other: no azimuthal
     cue anywhere in the frame, nothing for a cast shadow to subtract, and every
     material reads as overcast satin however good its BRDF is. */ \
  "uniform vec3 SUN_DIR, SUN_COL, SKY_HOR, SKY_TOP, SKY_GND, uGlow;\n" \
  "uniform float EXPOSURE, uAmb, uFog, uCloud;\n" \
  /* Sun up, sky down. Measured off the frames: the horizon landed at 185 display
     while sunlit ground reached 130, and NOTHING in any frame reached the
     tonemap's shoulder — an arena lit by a lightbox behind it, with no highlight
     end for any material to reach. A clear sky is not brighter than the ground it
     lights; it only was here because the ambient was paying for what the sun
     should have. */ \
  /* Exposure. Every light above is a radiance in the same unit as the sky the
     player can see, so ONE number decides where mid-grey lands and the balance
     between sun, sky and reflection is never re-derived per surface. */ \
  "vec3 sky_env(vec3 d, float disc){\n" \
  "  float t = clamp(d.y, -1.0, 1.0);\n" \
  /* ONE pow, not two. A ternary over two pow calls is flattened to a select by
     the driver, so both powers are computed on every sky and every world
     fragment (twice on a world fragment — env_gloss and the aerial term).
     abs(t) is t or -t exactly by sign, and at t == 0 both forms give 0. */ \
  "  bool up = t > 0.0;\n" \
  "  vec3 c = mix(SKY_HOR, up ? SKY_TOP : SKY_GND, pow(abs(t), up ? 0.42 : 0.45));\n" \
  "  float s = max(dot(d, SUN_DIR), 0.0);\n" \
  /* The low-sun horizon band: warm light banked along the horizon on the sun's
     side. It lives here so distant walls fog into the same glow. */ \
  "  float hb = 1.0 - abs(t);\n" \
  "  c += uGlow * hb * hb * hb * hb * hb * hb * (0.25 + 0.75 * s * s);\n" \
  /* THE SUN IS 0.53 DEGREES ACROSS, and it was drawn at four times that. The
     old term was pow(s, 900), whose half-power point is 2.25 degrees off axis —
     a soft blob the size of four suns, which is why the sky read as an
     overcast lightbox with a bright patch in it rather than as a clear day.
     A hard disc plus a real aureole is the correct decomposition, and it is
     also what makes the aureole readable: the two used to be one number.
     The disc's edge is antialiased on the DERIVATIVE of the cosine rather than
     on a fixed epsilon. A fixed epsilon either loses the disc entirely or
     strobes as the pixel grid crosses its rim, because the whole disc spans
     about a hundred-thousandth of the cosine's range. */ \
  "  float sun = 0.0;\n" \
  "  if (disc > 0.0) {\n" \
  "    float e = max(fwidth(s), 1.0e-7);\n" \
  "    sun = smoothstep(0.99998917 - e, 0.99998917 + e, s) * 26.0;\n" \
  "  }\n" \
  /* Two aureoles, not one. The tight one (~110) is the corona a clear sky has
     around a low sun; the broad one (8) is the forward-scattered haze over the
     whole quarter of the sky the sun is in. */ \
  /* THREE lobes, not two. The disc is 255 and the aureole's plateau is 184, and
     with nothing between them the transition happens in three pixels: a hard
     white cut-out pasted on a smooth sky, which reads as a hole punched in the
     backdrop rather than as a light source. The middle lobe is the GLARE — a
     degree and a third wide, which is what a camera and an eye both do with a
     source this bright. */ \
  "  c += SUN_COL * (disc * sun + disc * 1.7 * pow(s, 2600.0)\n" \
  "                + disc * 0.13 * pow(s, 110.0) + 0.085 * pow(s, 8.0));\n" \
  "  return c;\n" \
  "}\n" \
  /* The environment a SPECULAR surface sees, which is not the one the sky pass
     draws. Every reflecting surface in the frame used to sample sky_env at a
     fixed disc of 0.35, so a 0.86-rough concrete floor and a polished bolt
     mirrored the SAME 900-exponent pinprick sun at the same strength: the one
     cue that separates a matte metal from a bright one was absent from the only
     term that carries metal at all. Here both the lobe's width and its energy
     come from the roughness — a mirror gets the disc, a rough casting gets
     nothing but the gradient, and everything between gets a broad glossy smear
     that slides across the part as it turns, which is what "it reflects a
     little" actually looks like. The gradient half is shared with the sky pass
     BY CONSTRUCTION rather than by a matched constant: same function, disc off. */ \
  "vec3 env_gloss(vec3 d, float rough){\n" \
  "  vec3 c = sky_env(d, 0.0);\n" \
  "  float g = 1.0 - clamp(rough, 0.0, 1.0);\n" \
  "  float g2 = g * g;\n" \
  "  float s = max(dot(d, SUN_DIR), 0.0);\n" \
  "  return c + SUN_COL * (g2 * g2 * 3.2 * pow(s, mix(24.0, 760.0, g)));\n" \
  "}\n" \
  /* Filmic curve (ACES fit) then a gamma-2.0 encode — the albedo decode below
     is x*x, so an unlit surface round-trips to exactly its authored color. */ \
  "const vec3 VM_FILL = vec3(1.08, 1.17, 1.38);\n" \
  "const vec3 VM_RIM  = vec3(1.23, 1.31, 1.52);\n" \
  /* GGX + Smith, once. Every light — sun, viewmodel fill, viewmodel rim — is the
     same call with a different direction and radiance, which is the only way
     three lights stay consistent with each other. */ \
  "vec3 lit(vec3 n, vec3 V, vec3 L, vec3 diff, vec3 F0, float rough, vec3 rad){\n" \
  "  float NoL = max(dot(n, L), 0.0);\n" \
  "  if (NoL <= 0.0) return vec3(0.0);\n" \
  "  float NoV = max(dot(n, V), 1e-4);\n" \
  "  vec3 H = normalize(L + V);\n" \
  "  float NoH = max(dot(n, H), 0.0), VoH = max(dot(V, H), 0.0);\n" \
  "  float a = rough * rough, a2 = a * a;\n" \
  "  float dn = NoH * NoH * (a2 - 1.0) + 1.0;\n" \
  "  float D = a2 / (3.14159265 * dn * dn);\n" \
  "  float kv = a * 0.5;\n" \
  "  float Vis = 0.25 / max((NoL*(1.0-kv)+kv) * (NoV*(1.0-kv)+kv), 1e-4);\n" \
  "  vec3 F = F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);\n" \
  "  return (diff + D * Vis * F) * NoL * rad;\n" \
  "}\n" \
  /* SCENE-REFERRED NOW. The name is kept because forty call sites mean "the  \
     value this pixel finally holds"; what it holds is linear radiance at the \
     preset's exposure, and the ACES curve, the gamma-2 encode and the dither \
     all moved to POST_FS, where they run once over the finished frame. */ \
  "vec3 tonemap(vec3 x){ return clamp(x * EXPOSURE, vec3(0.0), vec3(256.0)); }\n" \
  /* The inverse of the display transform, for the colours that were AUTHORED \
     as display values — the additive glow (tracers, sparks, flash petals) and \
     the lit inlays. Each is put back into the scene at the radiance that the \
     post chain maps onto the colour it was authored as. */ \
  "vec3 aces_inv(vec3 y){\n" \
  "  y = clamp(y, 0.0, 0.985);\n" \
  "  vec3 a = 2.51 - 2.43 * y, b = 0.03 - 0.59 * y, c = -0.14 * y;\n" \
  "  return (-b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);\n" \
  "}\n" \
  "vec3 display_in(vec3 d){ return aces_inv(d * d); }\n"

