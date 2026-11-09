// code/core/math.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Math — column-major mat4, y-up right-handed, yaw 0 looks down -z.
// ---------------------------------------------------------------------------

typedef union { struct { float x, y, z; }; float e[3]; } v3;

// PI AND ITS QUARTER TURNS, ONE SPELLING EACH. These were eight different
// literals over ~70 sites, and two of them were the SAME constant written
// twice at DIFFERENT PRECISION: pi/2 was 0x3fc90fda in the UI primitives and
// 0x3fc90fdb in the ragdoll and the figure — one ULP apart, i.e. exactly the
// value drift the one-copy rule exists to stop. F_PI_2 below is the
// correctly-rounded one, and the UI is the side that moved (a rounded-corner
// sweep angle; the menu and gameplay frames stayed md5-identical). Tau's two
// spellings were already bit-identical as float, so folding them costs
// nothing and stops the next pair from differing. The GLSL source strings
// keep their own literal text — a shader cannot see a C macro.
#define F_PI    3.14159265f
#define F_TAU   6.2831853f     // 2*pi — the full turn every ring loop divides
#define F_PI_2  1.57079633f    // quarter turn
#define F_3PI_2 4.71238898f    // three quarter turns
#define F_PI_4  0.7853982f     // eighth turn: the 45 deg threshold and the
                               // 8-way cover fan both step by this

static v3    v3_add(v3 a, v3 b) { return (v3){{a.x + b.x, a.y + b.y, a.z + b.z}}; }
static v3    v3_scale(v3 a, float s) { return (v3){{a.x * s, a.y * s, a.z * s}}; }
static v3    v3_lerp(v3 a, v3 b, float t) { return v3_add(a, v3_scale(v3_add(b, v3_scale(a, -1.0f)), t)); }
static float v3_dot(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static v3    v3_cross(v3 a, v3 b) {
  return (v3){{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}};
}
// THE RECIPROCAL IS OPAQUE ON PURPOSE, AND THIS IS NOT SUPERSTITION — IT IS A
// MEASURED MISCOMPILE OF THE SHIPPED BUILD. Written as the obvious
// `v3_scale(a, 1.0f / l)`, gcc 14.2 at the Makefile's own OPT
// (-O3 -ffast-math -funroll-loops -flto) decides it knows the result is a unit
// vector and folds a component-times-scalar product against that belief. The
// decisive measurement — one printf, one build, the SAME spine value in both
// terms of the same line:
//     -O2        spine.y=0.9154579  0.4f*spine.y=0.3661832   (correct)
//     ship flags spine.y=0.9154579  0.4f*spine.y=0.4000000   (spine.y taken as 1)
// In skel_solve that is `chest = pelvis + spine*0.40`, and EVERYTHING above the
// waist — neck, head, both shoulders, the gun anchor, both wrists — is built
// from chest, so every shipped build put a running figure's whole upper body
// ~34 mm high with its trunk lean quietly removed. `figm` cannot see it: it
// measures a STANDING figure, where spine.y is 1.0 and the two agree exactly.
// Bisected: needs -O3 together with -funsafe-math-optimizations,
// -ffinite-math-only and -fno-math-errno; -O2 -ffast-math and -O3 alone are
// both fine, which is why nothing caught it. The volatile forces the division
// to be a real value rather than an assumption; measured cost over 120 rendered
// frames at 20 bots, three runs each: 3314/3283/3191 ms before against
// 3288/3129/3203 ms after, i.e. free.
static v3 v3_norm(v3 a) {
  float l = sqrtf(v3_dot(a, a));
  if (!(l > 1e-6f)) return (v3){{0, 0, 0}};
  volatile float inv = 1.0f / l;
  return v3_scale(a, inv);
}
static v3    v3_sub(v3 a, v3 b) { return (v3){{a.x - b.x, a.y - b.y, a.z - b.z}}; }
static float v3_len(v3 a) { return sqrtf(v3_dot(a, a)); }
static float f_clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
// NaN/inf tests that SURVIVE -ffast-math, which is what this binary ships with:
// -ffinite-math-only lets the compiler fold isnan()/isfinite()/v != v to a
// constant, so a guard against a hostile or mis-parsed float has to read the
// bits. Two functions rather than a flag, because the callers really differ — a
// config value may legitimately be +/-inf (it clamps to the bound); a noise
// radius, a script argument and a wire angle may not.
static int f_is_nan(float v) {
  uint32_t b; memcpy(&b, &v, sizeof b); return (b & 0x7FFFFFFFu) > 0x7F800000u;
}
static int f_is_nonfinite(float v) {
  uint32_t b; memcpy(&b, &v, sizeof b); return (b & 0x7FFFFFFFu) >= 0x7F800000u;
}
static float smooth01(float t) { return t * t * (3.0f - 2.0f * t); }
static float wrap_pi(float a) {
  // Guard, not a rewrite. Past |a| ~ 2.56e8 the subtraction below is a float32
  // no-op and the loop can never terminate — `look 1e20 0` hung the harness
  // forever with no output (yaw is accumulated unwrapped in player_look, and
  // anim_tick feeds it straight in). fmodf is exact and unreachable for any
  // angle the sim itself produces, so the locomotion path stays byte-identical
  // rather than being perturbed by an ulp on every call. It also terminates on
  // inf/NaN, where fmodf yields NaN and both comparisons below are then false.
  if (fabsf(a) > 1.0e6f) a = fmodf(a, F_TAU);
  while (a > F_PI) a -= F_TAU;
  while (a < -F_PI) a += F_TAU;
  return a;
}
// Rodrigues rotation of p around unit axis u.
static v3 v3_rot_axis(v3 p, v3 u, float ang) {
  float c = cosf(ang), s = sinf(ang);
  return v3_add(v3_add(v3_scale(p, c), v3_scale(v3_cross(u, p), s)),
                v3_scale(u, v3_dot(u, p) * (1.0f - c)));
}

typedef struct { float m[16]; } mat4;

static mat4 m4_mul(mat4 a, mat4 b) {
  mat4 o;
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++) {
      float s = 0;
      for (int k = 0; k < 4; k++) s += a.m[k * 4 + r] * b.m[c * 4 + k];
      o.m[c * 4 + r] = s;
    }
  return o;
}

static mat4 m4_perspective(float fovy, float aspect, float zn, float zf) {
  float f = 1.0f / tanf(fovy * 0.5f);
  mat4 o = {0};
  o.m[0] = f / aspect;
  o.m[5] = f;
  o.m[10] = (zf + zn) / (zn - zf);
  o.m[11] = -1.0f;
  o.m[14] = 2.0f * zf * zn / (zn - zf);
  return o;
}

static mat4 m4_ortho(float hw, float hh, float zn, float zf) {
  mat4 o = {0};
  o.m[0] = 1.0f / hw;
  o.m[5] = 1.0f / hh;
  o.m[10] = -2.0f / (zf - zn);
  o.m[14] = -(zf + zn) / (zf - zn);
  o.m[15] = 1.0f;
  return o;
}

// The one view-matrix fill. m4_look_dir (shadow map) and m4_look (every
// camera) differ only in how they build the basis; shadow sampling depends on
// the two agreeing on row layout and the -fwd sign to the last digit, so the
// fill exists once.
static mat4 m4_from_basis(v3 eye, v3 right, v3 up, v3 fwd) {
  mat4 o = {0};
  o.m[0] = right.x; o.m[4] = right.y; o.m[8]  = right.z; o.m[12] = -v3_dot(right, eye);
  o.m[1] = up.x;    o.m[5] = up.y;    o.m[9]  = up.z;    o.m[13] = -v3_dot(up, eye);
  o.m[2] = -fwd.x;  o.m[6] = -fwd.y;  o.m[10] = -fwd.z;  o.m[14] = v3_dot(fwd, eye);
  o.m[15] = 1.0f;
  return o;
}

// View matrix for a DIRECTION rather than a yaw/pitch pair: the sun has no
// position, only a direction, and building its basis from yaw/pitch would put a
// singularity right where a high sun lives.
static mat4 m4_look_dir(v3 eye, v3 fwd) {
  v3 up = fabsf(fwd.y) > 0.95f ? (v3){{1, 0, 0}} : (v3){{0, 1, 0}};
  v3 right = v3_norm(v3_cross(fwd, up));
  up = v3_cross(right, fwd);
  return m4_from_basis(eye, right, up, fwd);
}

// Camera basis with a ROLL about the view axis. Leaning rolls the picture, and
// there are four consumers that have to agree on it to the last digit — the view
// matrix, the sky pass's ray basis, the scope's second view and the viewmodel's
// own projection — so the basis is built once, here, and handed out. A rolled
// world with an unrolled sky is a horizon sliding across a static gradient.
static void cam_basis(float yaw, float pitch, float roll, v3 *fwd, v3 *rgt, v3 *up) {
  v3 f = {{cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw)}};
  v3 r = v3_norm(v3_cross(f, (v3){{0, 1, 0}}));
  v3 u = v3_cross(r, f);
  float c = cosf(roll), s = sinf(roll);
  *fwd = f;
  *rgt = v3_add(v3_scale(r, c), v3_scale(u, -s));
  *up  = v3_add(v3_scale(u, c), v3_scale(r,  s));
}

static mat4 m4_look(v3 eye, float yaw, float pitch, float roll) {
  v3 fwd, right, up;
  cam_basis(yaw, pitch, roll, &fwd, &right, &up);
  return m4_from_basis(eye, right, up, fwd);
}

