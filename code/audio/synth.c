// code/audio/synth.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Procedural audio — every sound is synthesized at startup into a 48 kHz
// interleaved STEREO float buffer (layered noise/sine partials through one-pole
// filters and exponential envelopes; seeded, so the WAV dump is reproducible).
// Both native backends play it (WASAPI on Windows, ALSA on Linux); the headless
// harness dumps WAVs + level stats via the `sfx DIR` script command instead.
// ---------------------------------------------------------------------------

[[maybe_unused]] static const char *SFX_NAME[SFX_COUNT] = {  // harness WAV dump
  "fire_ar", "fire_sr", "far_ar", "far_sr", "hit", "hit_head",
  "kill", "kill_head", "hurt", "impact", "whizz", "step", "dryfire",
  "reload_ar", "reload_sr", "flesh", "reload_ar_tac",
  "fire_pt", "reload_pt", "knife_swing", "knife_wall", "knife_block",
  "nade_pin", "nade_bounce", "blast", "blast_far", "nade_spoon", "nade_equip",
  "fire_ar2", "fire_ar3", "fire_ar4", "fire_ar5", "fire_sr2", "fire_sr3",
  "fire_pt2", "fire_pt3", "fire_pt4", "far_pt", "slap", "brass", "clink",
  "alarm", "snip", "chime",
  "fire_ak1", "fire_ak2", "fire_ak3", "fire_famas1", "fire_famas2", "fire_famas3",
  "fire_glock1", "fire_glock2", "fire_glock3", "fire_deagle1", "fire_deagle2", "fire_deagle3",
  "death1", "death2", "death3", "death4", "death5", "bodyfall1", "bodyfall2", "bodyfall3",
  "kill2", "kill3", "kill4", "kill_head2", "kill_head3", "die1", "die2",
};

#define SFX_HZ 48000
#define SFX_CH 2            // every buffer is interleaved stereo

// Buffers are STEREO now. A mono point source duplicated to both channels is
// the one thing that no amount of synthesis quality can compensate for: with
// L and R bit-identical the whole soundfield collapses to a line between the
// ears and nothing has size. What gives a gunshot its size is not the shot —
// it is the ROOM, and a room's tail is decorrelated between the ears. Every
// transient here stays centred (a point source must be a point) and every
// tail is generated twice from independent noise.
static float *g_sfx_buf[SFX_COUNT];   // interleaved L,R
static int    g_sfx_len[SFX_COUNT];   // FRAMES, not samples
static uint64_t g_rng_sfx;

#define SFX_SCRATCH (SFX_HZ * 2)
static float g_scr[SFX_SCRATCH];      // one mono layer at a time

static float *sfx_alloc(int id, float seconds) {
  // One noise stream PER SOUND. g_rng_sfx used to be a single stream consumed in
  // build order, which means changing the NUMBER or the LENGTH of the noise
  // layers in any one sound re-rolls the noise in every sound built after it.
  // That is not a cosmetic problem: it makes every A/B in this file a
  // measurement of noise realisation rather than of the change, and it was
  // observed while doing exactly that — a confirmation's descriptors moved after
  // an edit to a gunshot that shares nothing with it. Deterministic per id, so
  // the WAV dump is still reproducible.
  g_rng_sfx = 0x5F31Aull + 0x9E3779B97F4A7C15ull * (uint64_t)(id + 1);
  int n = (int)(seconds * SFX_HZ);
  g_sfx_len[id] = n;
  g_sfx_buf[id] = calloc((size_t)n * SFX_CH, sizeof(float));
  if (!g_sfx_buf[id]) fatal("out of memory");
  return g_sfx_buf[id];
}

// --- filters -----------------------------------------------------------
// One-pole, for gentle shaping. `hz` is the -3 dB corner; the coefficient is
// computed per sample so a layer can sweep its own filter.
typedef struct { float z; } op_t;
static float op_lp(op_t *s, float x, float hz) {
  float k = 1.0f - expf(-F_TAU * hz / SFX_HZ);
  s->z += (x - s->z) * k;
  return s->z;
}
static float op_hp(op_t *s, float x, float hz) { return x - op_lp(s, x, hz); }

// State-variable filter. A one-pole cannot resonate, and resonance is exactly
// what makes a band read as a BODY rather than as filtered hiss — the 150 Hz
// ring of a receiver, the 3 kHz crack of a supersonic round.
typedef struct { float lp, bp; } svf_t;
static float svf(svf_t *s, float x, float hz, float q) {
  // The Chamberlin topology is only stable while f + 1/q < 2, which puts a
  // hard ceiling near SR/6 — not the Nyquist limit one would assume. A tail
  // asked for 9.9 kHz here and the filter blew up: the whole headshot buffer
  // came out NaN, and NaN in an audio buffer is silence at best and a speaker
  // at worst. Clamped, and the damping clamped with it.
  float f = 2.0f * sinf(F_PI * f_clamp(hz, 20.0f, SFX_HZ / 6.5f) / SFX_HZ);
  float d = 1.0f / f_clamp(q, 0.5f, 20.0f);
  if (f + d > 1.9f) d = 1.9f - f;
  float hp = x - s->lp - s->bp * d;
  s->bp += f * hp;
  s->lp += f * s->bp;
  return s->bp;   // BANDPASS tap only — lp is state the hp line above needs
}

// --- envelopes ---------------------------------------------------------
// Attack is LINEAR and measured in seconds; decay is exponential with a shape
// exponent (>1 = faster at first, which is what a real impact does).
static float sfx_env(float t, float atk, float dec, float shape) {
  if (t < 0) return 0;
  float a = atk <= 0.0f ? 1.0f : (t < atk ? t / atk : 1.0f);
  float d = expf(-t / dec);
  return a * (shape == 1.0f ? d : powf(d, shape));
}

// --- layer placement ---------------------------------------------------
// Add `n` frames of the mono scratch into a stereo buffer at frame `at`.
// `pan` is -1..1; `width` delays one channel by a few samples, which is how a
// near-coincident pair hears a source that is not a point. Transients must
// stay at width 0 or the click smears and stops being a click.
static void sfx_lay(int id, int n, int at, float pan, int width) {
  float *b = g_sfx_buf[id];
  int len = g_sfx_len[id];
  float gl = sqrtf(0.5f * (1.0f - pan)), gr = sqrtf(0.5f * (1.0f + pan));
  for (int i = 0; i < n; i++) {
    int fl = at + i, fr = at + i + width;
    if (fl >= 0 && fl < len) b[fl * SFX_CH + 0] += g_scr[i] * gl * 1.4142f;
    if (fr >= 0 && fr < len) b[fr * SFX_CH + 1] += g_scr[i] * gr * 1.4142f;
  }
}

// Normalise the scratch to a peak of `amp`. Without this, `amp` means a
// different thing for every layer type and the mix is unpredictable: a sine
// sweep at amp 0.8 delivers 0.57 RMS while a band-passed noise burst at amp
// 0.95 delivers about 0.05, because a narrow band of white noise contains
// almost none of its energy. That one asymmetry is why the AR's "crack" layer
// was inaudible and the shot measured 96% below 250 Hz — a thud with a crack
// written into the source and none in the output. Every generator ends here,
// so `amp` is a level, comparable across layers.
static int scr_norm(int n, float amp) {
  float mx = 1e-9f;
  for (int i = 0; i < n; i++) { float a = g_scr[i] < 0 ? -g_scr[i] : g_scr[i];
                                if (a > mx) mx = a; }
  float g = amp / mx;
  for (int i = 0; i < n; i++) g_scr[i] *= g;
  return n;
}

// --- building blocks ---------------------------------------------------
// A band-limited noise burst into the scratch. This is the workhorse: a
// gunshot is four of these at different centres, widths and decays.
static int sfx_burst(float sec, float hz, float q, float atk, float dec,
                     float shape, float amp) {
  int n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  svf_t f1 = {0, 0}, f2 = {0, 0};
  for (int i = 0; i < n; i++) {
    float t = (float)i / SFX_HZ;
    float x = rng_rangef(&g_rng_sfx, -1, 1);
    x = svf(&f1, x, hz, q);
    x = svf(&f2, x, hz, q * 0.7f);       // two poles: a real band, not a tilt
    g_scr[i] = x * sfx_env(t, atk, dec, shape);
  }
  return scr_norm(n, amp);
}

// A pitched body: a sine that sweeps down, which is what a pressure wave
// expanding away from a muzzle actually does.
static int sfx_sweep(float sec, float f0, float f1_, float ftau,
                     float dec, float shape, float amp) {
  int n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  float ph = 0;
  for (int i = 0; i < n; i++) {
    float t = (float)i / SFX_HZ;
    float f = f1_ + (f0 - f1_) * expf(-t / ftau);
    ph += F_TAU * f / SFX_HZ;
    g_scr[i] = sinf(ph) * sfx_env(t, 0.0002f, dec, shape);
  }
  return scr_norm(n, amp);
}

// The transient. Everything else is body; THIS is the part that makes a shot
// read as an explosion rather than as a sound effect, and it is why the old
// shots felt soft: their peak arrived 1.6 ms after onset. Unfiltered noise,
// sub-millisecond, no width.
static int sfx_click(float sec, float dec, float hp_hz, float amp) {
  int n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  op_t h = {0};
  for (int i = 0; i < n; i++) {
    float t = (float)i / SFX_HZ;
    float x = op_hp(&h, rng_rangef(&g_rng_sfx, -1, 1), hp_hz);
    g_scr[i] = x * expf(-t / dec);
  }
  return scr_norm(n, amp);
}

// Two decaying partials with a little inharmonicity — a struck metal object.
static int sfx_ring(float sec, float f1_, float f2_, float dec, float amp) {
  int n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  for (int i = 0; i < n; i++) {
    float t = (float)i / SFX_HZ;
    g_scr[i] = sinf(F_TAU * f1_ * t) * expf(-t / dec)
             + sinf(F_TAU * f2_ * t) * expf(-t / (dec * 0.62f)) * 0.55f;
  }
  return scr_norm(n, amp);
}

// Room tail: band-limited noise that RISES for a few ms before it decays,
// because a room takes time to fill. Written straight into the buffer with an
// independent noise stream per channel — that decorrelation IS the width.
static void sfx_tail(int id, int at, float sec, float hz, float q,
                     float rise, float dec, float amp) {
  float *b = g_sfx_buf[id];
  int len = g_sfx_len[id], n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  for (int c = 0; c < SFX_CH; c++) {
    svf_t f1 = {0, 0}, f2 = {0, 0};
    for (int i = 0; i < n; i++) {
      float t = (float)i / SFX_HZ;
      float x = rng_rangef(&g_rng_sfx, -1, 1);
      x = svf(&f1, x, hz, q);
      x = svf(&f2, x, hz * 1.35f, q * 0.8f);
      g_scr[i] = x * (1.0f - expf(-t / rise)) * expf(-t / dec);
    }
    // Normalised like every other generator, so `amp` is a level here too.
    // Left un-normalised it was the one place where a number meant something
    // different, and the consequence was measurable: the distant shots — which
    // are almost entirely tail — came out with an L/R correlation of 0.95, as
    // narrow as a shot fired at your ear, because the tail could not get loud
    // enough relative to the direct layers to widen anything.
    scr_norm(n, amp);
    for (int i = 0; i < n; i++) {
      int fr = at + i;
      if (fr >= 0 && fr < len) b[fr * SFX_CH + c] += g_scr[i];
    }
  }
}

// Early reflections: a handful of delayed, filtered, alternately-panned copies
// of what has been written so far. Three of these turn "a sound" into "a sound
// happening somewhere", and they cost a few hundred adds.
static void sfx_reflect(int id, int upto, const float *dly, const float *gn,
                        int nref, float lp_hz) {
  float *b = g_sfx_buf[id];
  int len = g_sfx_len[id];
  // The source region is SNAPSHOT first, and that is a fix, not a tidy-up. This
  // used to write `b[i+d]` while reading `b[i]` over the same range, so as soon
  // as `upto` exceeded the shortest delay the generator fed on its own output:
  // the tap at d reappeared at 2d, 3d, 4d..., which is not "a handful of
  // delayed copies" but an IIR comb. Measured on the shipped calls, the sniper's
  // near-cover set ran 6240 frames against a 768-frame first tap — eight
  // recursions of a 62.5 Hz comb at g=0.34, so about +3.6 dB of resonant peaks
  // every 62.5 Hz across the whole low end; the AR's set combed at 91 Hz and
  // both far bakes at 41.7 Hz. That is a colouration nobody chose and it lands
  // exactly where the shots were measuring bass-heavy. Taps are now independent
  // copies of the same source, which is what an early reflection is. A LATER
  // CALL still reads what an earlier one wrote — that is the mechanism the
  // sniper's arena returns use to be echoes of its near reflections, and it is
  // deliberate; only self-feeding within one call was ever wrong.
  int ns = upto;
  if (ns > SFX_SCRATCH / SFX_CH) ns = SFX_SCRATCH / SFX_CH;
  for (int i = 0; i < ns * SFX_CH; i++) g_scr[i] = b[i];
  for (int r = 0; r < nref; r++) {
    int d = (int)(dly[r] * SFX_HZ);
    for (int c = 0; c < SFX_CH; c++) {
      op_t lp = {0};
      float g = gn[r] * (c == (r & 1) ? 1.0f : 0.66f);   // alternate the side
      for (int i = 0; i < ns && i + d < len; i++)
        b[(i + d) * SFX_CH + c] += op_lp(&lp, g_scr[i * SFX_CH + c], lp_hz) * g;
    }
  }
}

// A-weighting, as six one-poles. The IEC 61672 curve is four real pole pairs
// and four zeros at DC, which is exactly a cascade of two high-passes at
// 20.6 Hz, one at 107.7, one at 737.9 and two low-passes at 12194 — so this is
// the curve's actual structure, not a fit to it, and the 1.2589 is its standard
// +2.0 dB normalisation to unity at 1 kHz.
//
// It exists because gain-staging to a FLAT RMS target is what made "make it
// fatter" self-defeating, and the measurement is worth writing down: adding a
// 88 Hz mass layer and a 128 -> 46 Hz drop to the kill raised its unweighted
// level 3.4 dB and LOWERED its A-weighted level 5 dB. Flat RMS lives almost
// entirely in the bottom two octaves, so a loud sub layer does not add weight —
// it forces sfx_finish's gain down and takes 5 dB off the crack, the tick and
// the crunch, i.e. off everything the player actually hears. The kill came out
// with 73% of its energy under 120 Hz, a 248 Hz centroid, and 2% left above
// 2 kHz. Weighted, the low layers cost what they are worth and no more.
typedef struct { op_t h1, h2, h3, h4, l1, l2; } awt_t;
static float aw_run(awt_t *s, float x) {
  x = op_hp(&s->h1, x, 20.6f);
  x = op_hp(&s->h2, x, 20.6f);
  x = op_hp(&s->h3, x, 107.7f);
  x = op_hp(&s->h4, x, 737.9f);
  x = op_lp(&s->l1, x, 12194.0f);
  x = op_lp(&s->l2, x, 12194.0f);
  return x * 1.2589f;
}

// Gain-stage FIRST, then limit, then a peak safety. The old code did
// tanh-then-peak-normalise, which is the wrong order twice over: tanh at a
// fixed drive distorts quiet material as hard as loud material, and
// peak-normalising alone means two sounds sharing a "peak" can differ by 15 dB
// of actual loudness — measured, this set spanned -11.8 to -27.7 dB RMS at one
// peak. Here the sound is first brought to a target RMS, and only then is the
// transient that now pokes out soft-limited. That order is why a limiter makes
// something louder instead of just quieter, and it is what lets a shot keep a
// sub-millisecond attack AND sit at a usable level.
//
// `win_s` is the window the RMS is measured over, and it exists because the
// whole-buffer RMS is not a loudness: give a shot a two-second hall and the
// same sound spread over four times the frames measures 6 dB quieter, so
// gain-staging to a fixed whole-buffer target hands a long sound far more gain
// than a short one and the master limiter then eats the transient it was
// supposed to protect. Measuring the first ~300 ms — which is what the ear
// judges an impulsive sound by — makes `target_rms` mean the same thing for a
// 140 ms hitmarker and a 1.9 s rifle report. 0 = whole buffer.
static void sfx_finish(int id, float peak, float target_rms, float win_s) {
  float *b = g_sfx_buf[id];
  int n = g_sfx_len[id] * SFX_CH;
  if (n <= 0) return;
  int w = win_s > 0 ? (int)(win_s * SFX_HZ) * SFX_CH : n;
  if (w > n || w <= 0) w = n;
  // A-weighted, per channel — see aw_run. `target_rms` is therefore a LOUDNESS,
  // and the peak safety below is what charges a bass-heavy sound for the
  // headroom it eats: a layer stack that needs 12 dB of gain to reach its
  // weighted target will run into the limiter and come back squashed, which is
  // the signal that the low end is too big rather than the level too low.
  awt_t aw[SFX_CH] = {0};
  double sq = 0;
  for (int i = 0; i < w; i++) {
    float a = aw_run(&aw[i % SFX_CH], b[i]);
    sq += (double)a * (double)a;
  }
  float rms = (float)sqrt(sq / (double)w) + 1e-9f;
  float g = target_rms > 0 ? target_rms / rms : 1.0f;
  for (int i = 0; i < n; i++) {
    float x = b[i] * g, a = x < 0 ? -x : x;
    if (a > 0.55f) {                      // soft knee, then a hard-ish ceiling
      float over = a - 0.55f;
      a = 0.55f + over / (1.0f + over * 2.6f);
      x = x < 0 ? -a : a;
    }
    b[i] = x;
  }
  float mx = 1e-6f;
  for (int i = 0; i < n; i++) {
    float a = b[i] < 0 ? -b[i] : b[i];
    if (a > mx) mx = a;
  }
  // `peak` only bites BELOW 0.9346 — that is where the soft knee above
  // asymptotes (0.55 + 1/2.6), so nothing leaving that loop can exceed it and
  // a peak argument at or over 0.94 is a knob with nothing on the other end.
  // Six call sites deliberately sit there (they want only the knee); the rest
  // pass 0.62-0.93 and are really asking for headroom.
  if (mx > peak) { float k = peak / mx; for (int i = 0; i < n; i++) b[i] *= k; }
}

// A room, rather than an impression of one. `sfx_tail` fakes reverberation with
// band-limited noise that rises and then decays; that is a good trick and it has
// two limits that are exactly what stands between this sniper and a rifle going
// off in a stadium. The noise does not KNOW what the sound was, so what comes
// back off the walls is not the shot's own spectrum — and every band decays at
// the same rate, so the room never DARKENS. Measured on the old sniper: T60 in
// the low band 878 ms against 910 ms in the high band, i.e. the "room" got
// brighter as it died, which no real space does.
//
// A feedback delay network fixes both for fifty lines: four delay lines of
// mutually incommensurate length, one damping filter each, mixed through an
// orthogonal matrix so energy circulates and diffuses without either growing or
// collapsing, read out on two taps that share no delay line and are therefore
// decorrelated. Because it is fed the sound that was already written, the tail
// is that sound's reflections.
//   upto      frames of already-written direct sound to feed in
//   predelay  seconds to the first reflection
//   size_s    base delay-line length — the room's mean reflection interval;
//             this is the SIZE knob, and it is what a "big hall" means
//   rt60      seconds to decay 60 dB (at DC; damping shortens the top)
//   damp_hz   one-pole corner in each line. Air and plaster eat the high end,
//             and this is the term that makes T60lo exceed T60hi.
//   wet       reverb peak as a fraction of the DRY peak, so it is a level
//             comparable with every other generator's `amp`.
//
// SIX lines, not four, and the input staggered across them. A four-line network
// fed a broadband shot is a comb filter with four teeth: the impulse arrives in
// all four lines on the same sample, so the first few round trips are still
// coherent and the "room" rings on a pitch. Six incommensurate lines plus
// per-line injection offsets of a few hundred samples scatter that first
// wavefront, which is what an actual wall does.
#define VERB_LINES 6
// 250 ms of line. The longest ratio is 1.97, so this silently clamps any size_s
// above 0.127 s while rt60 goes on promising a bigger room — a failure with no
// symptom except a room that stops growing. The largest size_s in use is the
// sniper report's 0.055 (longest line 0.108 s, 43% of the ceiling).
#define VERB_MAX   (SFX_HZ / 4)
static float g_verb_z[VERB_LINES][VERB_MAX];

static void sfx_verb(int id, int upto, float predelay, float size_s,
                     float rt60, float damp_hz, float wet) {
  float *b = g_sfx_buf[id];
  int len = g_sfx_len[id];
  if (upto > len) upto = len;
  if (upto > SFX_SCRATCH) upto = SFX_SCRATCH;
  // The direct sound has to be SNAPSHOT before anything is added back into the
  // buffer: the wet write at frame f lands ahead of the read at f-predelay, so
  // reading b[] directly would feed the network its own output and turn a
  // decaying room into an accumulating one.
  float dry_pk = 1e-9f;
  for (int i = 0; i < upto; i++) {
    g_scr[i] = 0.5f * (b[i * SFX_CH] + b[i * SFX_CH + 1]);
    for (int c = 0; c < SFX_CH; c++) {
      float a = b[i * SFX_CH + c] < 0 ? -b[i * SFX_CH + c] : b[i * SFX_CH + c];
      if (a > dry_pk) dry_pk = a;
    }
  }
  static const float LR[VERB_LINES] = {1.00f, 1.13f, 1.31f, 1.51f, 1.73f, 1.97f};
  static const int   IN_OFF[VERB_LINES] = {0, 37, 79, 131, 191, 263};
  int dl[VERB_LINES], idx[VERB_LINES];
  float fb[VERB_LINES];
  for (int i = 0; i < VERB_LINES; i++) {
    dl[i] = (int)(size_s * LR[i] * SFX_HZ);
    if (dl[i] < 16)       dl[i] = 16;
    if (dl[i] > VERB_MAX) dl[i] = VERB_MAX;
    // 60 dB over rt60 seconds, charged once per trip around this line.
    fb[i] = powf(10.0f, -3.0f * (float)dl[i] / ((float)SFX_HZ * rt60));
  }
  int pd = (int)(predelay * SFX_HZ);
  // Pass 0 measures the wet peak so `wet` can mean a level; pass 1 writes.
  // Re-running the network is bit for bit identical — it has no RNG in it.
  float gain = 1.0f;
  for (int pass = 0; pass < 2; pass++) {
    memset(g_verb_z, 0, sizeof g_verb_z);
    for (int i = 0; i < VERB_LINES; i++) idx[i] = 0;
    op_t lp[VERB_LINES] = {{0}, {0}, {0}, {0}, {0}, {0}};
    float wet_pk = 1e-9f;
    for (int f = 0; f < len; f++) {
      float o[VERB_LINES], sum = 0;
      for (int i = 0; i < VERB_LINES; i++) { o[i] = g_verb_z[i][idx[i]]; sum += o[i]; }
      // Householder reflection y = x - (2/N)*sum(x). Orthogonal, so the loop is
      // lossless before the per-line gain, which is what lets rt60 be an actual
      // number instead of something found by trial and error.
      float s = sum * (2.0f / (float)VERB_LINES);
      for (int i = 0; i < VERB_LINES; i++) {
        int src = f - pd - IN_OFF[i];
        float in = (src >= 0 && src < upto) ? g_scr[src] : 0.0f;
        float v = (o[i] - s) * fb[i] + in * ((i & 1) ? -0.4f : 0.4f);
        g_verb_z[i][idx[i]] = op_lp(&lp[i], v, damp_hz);
        idx[i] = idx[i] + 1 >= dl[i] ? 0 : idx[i] + 1;
      }
      // The two taps share no delay line, so L and R are genuinely different
      // rooms heard from one head rather than one room heard twice.
      float wl = (o[0] + o[2] + o[4]) * 0.333f;
      float wr = (o[1] + o[3] + o[5]) * 0.333f;
      if (pass == 0) {
        float a = wl < 0 ? -wl : wl, c = wr < 0 ? -wr : wr;
        if (c > a) a = c;
        if (a > wet_pk) wet_pk = a;
      } else {
        b[f * SFX_CH]     += wl * gain;
        b[f * SFX_CH + 1] += wr * gain;
      }
    }
    if (pass == 0) gain = wet * dry_pk / wet_pk;
  }
}

// A short percussive "clack" — the mechanical building block. Kept as a
// helper because the gunshots' bolt detail and dry fire all want the same
// thing. The reload's magazine events do NOT come through here: they are
// `sfx_mech` below, which is this plus the mass a clack has none of.
static void sfx_clack(int id, int at, float tone_hz, float amp) {
  int n = sfx_burst(0.055f, tone_hz, 1.1f, 0.00004f, 0.0075f, 1.3f, amp);
  sfx_lay(id, n, at, 0.0f, 0);
  n = sfx_ring(0.05f, tone_hz * 0.42f, 190.0f, 0.011f, amp * 0.55f);
  sfx_lay(id, n, at, 0.0f, 0);
}

// ONE mechanical event, with mass and a space around it — the reload's
// counterpart to `sfx_head_sig`: a single authored signature that every moving
// metal part in a reload is an instance of, so "a magazine seating" and "a bolt
// going home" differ in their two frequencies and their weight rather than in
// two independently invented layer stacks.
//
// `sfx_clack` (which the gunshots still use for their bolt detail) is a burst
// plus a ring and nothing else, and four of them in 1.8 s of silence is exactly
// what the AR reload was. Measured, that reads as: 2% of the energy below
// 120 Hz, a 1077 Hz centroid, and T60 61 ms — no mass at all, and the ONLY
// sound in the game with no reflections in it, so it sat outside the arena
// every other sound sits inside. A magazine going into a receiver is 300 g of
// steel arriving in an aluminium housing; it has a bottom end.
//
//   knack     the transient. Sub-millisecond and high — this is the CRISPNESS
//             the brief asks for, and it must stay the loudest SAMPLE of the
//             event.
//   metal     the band that says what the part is made of (`tone_hz`)
//   spring    magazine spring, detent, gear rattle. The connective tissue that
//             makes it a mechanism rather than four isolated clicks.
//   mass      `mass_hz`, at +6 ms. The offset is the same rule the hit
//             confirmation is built on and for the same reason: laid at frame 0
//             a slow low excursion adds to the transient's peak, the composite
//             crest forces the limiter down on the whole sound, and the knack
//             is what gets eaten. Six milliseconds apart the two peaks never
//             meet — and metal-then-weight is what an impact IS.
//   room      two dark early taps at +11/+19 ms — the per-event reflections:
//             a reload is heard at 30 cm, so what comes back per event is your
//             own gear and the wall a metre away, not a hall. A small global
//             sfx_verb room now ALSO runs under them (see the reload builders):
//             the old "the sniper's last events land past sfx_verb's snapshot
//             cap" arithmetic went stale when reload_ticks shortened — the
//             shipped events land at 1.41/1.70 s, inside the 2.00 s cap.
// THE HOLES BETWEEN THE EVENTS. Measured with a 30 Hz envelope against -35 dB
// of the buffer's own peak, the AR reload is above threshold for only 33.7% of
// its 1267 ms and the sniper for 28.9% of its 1783, with gaps of 70-181 ms (AR)
// and 89-276 ms (SR) — which is the same "four clacks in 1.8 s of silence"
// failure sfx_mech was written to fix, reduced but not removed. What fills a
// hole is NOT another struck event: a sixth clack would fight `eng`, which is a
// contract on this buffer (atk and eng must both land on the LAST event, or the
// sound announces a live weapon early). It is the CONTINUOUS noise a mechanism
// makes between its events — a magazine spring, a glove on a polymer well, gear
// shifting on a carrier — which is broadband, quiet, and has no transient at all
// to compete with.
//
// Deliberately 12-18 dB under the struck events it bridges: it must be audible
// as texture and inaudible as an event. `width` gives each bed its own channel
// offset so the bridge is wider than the clacks it sits between, which is also
// what stops it reading as part of either one.
static void sfx_bed(int id, int at, float dur, float hz, float amp, int width) {
  int n = sfx_burst(dur, hz, 0.85f, 0.020f, dur * 0.55f, 1.0f, amp);
  sfx_lay(id, n, at, 0.0f, width);
}

static void sfx_mech(int id, int at, float tone_hz, float mass_hz, float amp,
                     float spring) {
  int n, d6 = (int)(0.006f * SFX_HZ);
  n = sfx_click(0.0025f, 0.00016f, tone_hz * 1.8f, 1.45f * amp);
  sfx_lay(id, n, at, 0.0f, 0);                              // knack
  n = sfx_burst(0.065f, tone_hz, 1.15f, 0.00004f, 0.0110f, 1.3f, 2.70f * amp);
  sfx_lay(id, n, at, 0.0f, 1);                              // metal
  if (spring > 0.0f) {
    n = sfx_burst(0.09f, 3100.0f, 1.6f, 0.00006f, 0.026f, 1.5f, spring * amp);
    sfx_lay(id, n, at + (int)(0.003f * SFX_HZ), 0.0f, 2);    // spring
  }
  // 0.13, not 0.62. `amp` is a PEAK, and a two-partial ring carries roughly
  // 15 dB more ENERGY than a band-passed noise burst at the same peak — so a
  // mass ring set by eye against the metal burst's number buries it. Measured
  // at 0.62: every event in both reloads came out 76-93% inside 120-500 Hz
  // with a 257-482 Hz centroid, i.e. five thuds where the tone that says
  // WHICH PART MOVED should have been. The weight burst can stay high for the
  // same reason in reverse: as noise it is cheap in energy for its peak.
  // ...AND THE MASS RING IS WEIGHTED BY THE EVENT'S OWN MASS FREQUENCY, exactly
  // as the meat burst below already is. Measured per event with an 85 ms window
  // on each authored offset, the AR magazine SEAT delivered 45.5 dB at
  // 20-120 Hz against the gunshot's 55.6, the kill confirmation's 58.9 and a
  // FOOTSTEP's 43.5 — the moment the brief calls a satisfying SEAT was two
  // decibels above a boot. The 200-800 Hz meat landed (55.5 dB, above the AR
  // report's own 50.6), so the file's "fat lives at 200-800" rule worked; what
  // was missing is the bottom that says MASS. This file's own note four blocks
  // up says a ~104 Hz ring at amp 0.30 is enough to be FELT and the shipped
  // value was 53% of that with a fifth of the kill's low-band decay. Weighted,
  // only the two SEATS (mass_hz 92-98) get more and the bolts (235 Hz) are
  // untouched — which is right, because a bolt's job is the knack.
  // SQUARED, so only the two seats move. The linear ratio is 1.53 at the AR
  // seat's 98 Hz and gave +2.5 dB where the target band is 5-8; squared it is
  // 2.34 and the bolt at 235 Hz still clamps to 1.0, i.e. untouched.
  float lk = f_clamp(150.0f / mass_hz, 1.0f, 1.60f);
  float low = 0.17f * lk * lk;
  n = sfx_ring(0.115f, mass_hz, mass_hz * 1.52f, 0.030f * (low / 0.17f),
               low * amp);
  sfx_lay(id, n, at + d6, 0.0f, 0);                         // mass
  n = sfx_burst(0.075f, mass_hz * 1.35f, 0.95f, 0.00012f, 0.017f, 1.1f,
                0.55f * amp);
  sfx_lay(id, n, at + d6, 0.0f, 3);                         // the weight
  // THE MEAT, and it was simply not here. Measured per event on the AR reload,
  // every one of the five came back with 0-3% below 120 Hz and 68-70% in the
  // 500 Hz-2 kHz band: the sound was all tone and knack with a thin 120-160 Hz
  // mass under it and NOTHING in the 200-800 Hz band where weight actually
  // lives. This project's own rule from the hit confirmations — "fat does not
  // live in the bottom octave, it lives at 200-800 Hz" — was never applied to
  // the reload. One band-passed burst at 3.4x the mass frequency, 8 ms behind
  // the transient so the two peaks never sum into the limiter.
  // Weighted by the event's own mass: a 122 Hz magazine seat wants all of this
  // and a 235 Hz bolt wants little, because the bolt's job is the KNACK and its
  // meat burst lands at 800 Hz where it eats exactly the band the knack needs.
  // Unweighted, one edit took the bolt's share above 2 kHz from 65% to 25% and
  // made the magazine RELEASE the brightest event in the sound.
  // The ceiling is 1.45 and not 1.30 because at the seat's mass frequency the
  // raw ratio is 1.43 and was being clipped — the clamp was throttling the one
  // event in the reload that most wants meat, while every other event (0.73 to
  // 1.17) sits below the ceiling and is untouched by the change.
  float meat = 0.95f * f_clamp(175.0f / mass_hz, 0.42f, 1.45f);
  n = sfx_burst(0.085f, mass_hz * 3.40f, 1.05f, 0.00010f, 0.021f, 1.15f,
                meat * amp);
  sfx_lay(id, n, at + (int)(0.008f * SFX_HZ), 0.0f, 2);     // 200-800 Hz
  n = sfx_burst(0.07f, tone_hz * 0.55f, 1.0f, 0.00006f, 0.014f, 1.2f,
                0.30f * amp);
  sfx_lay(id, n, at + (int)(0.011f * SFX_HZ), -0.18f, 2);    // near tap
  n = sfx_burst(0.09f, tone_hz * 0.40f, 0.9f, 0.00008f, 0.020f, 1.1f,
                0.19f * amp);
  sfx_lay(id, n, at + (int)(0.019f * SFX_HZ), 0.22f, 4);     // far tap
}

