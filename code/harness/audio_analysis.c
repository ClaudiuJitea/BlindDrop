// code/harness/audio_analysis.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Audio analysis. Nobody can listen in the container this game is developed in,
// so a sound IS its descriptors. peak/RMS/crest/L-R correlation cover level and
// width and were enough while the work was "make these buffers not be mono" —
// they say nothing about the two qualities this round is about. "Fat" is a
// question about the SPECTRUM (how much of the energy is weight and how much is
// information) and "reverberant" is a question about the ENVELOPE DECAY (how long
// the room keeps answering, and whether it darkens as it goes). Both are
// measured here, so both are numbers.
// ---------------------------------------------------------------------------

#define AN_N 4096                 // Welch block: 11.7 Hz bins at 48 kHz
static float g_an_re[AN_N], g_an_im[AN_N], g_an_pw[AN_N / 2 + 1];

// Iterative radix-2 FFT, in place. Twiddles are recomputed per butterfly, which
// is wasteful and completely irrelevant: this runs on 15 buffers, once, in a
// test command.
static void an_fft(float *re, float *im, int n) {
  for (int i = 1, j = 0; i < n; i++) {              // bit-reversal permutation
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      float t = re[i]; re[i] = re[j]; re[j] = t;
      t = im[i]; im[i] = im[j]; im[j] = t;
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    float ang = -F_TAU / (float)len;
    for (int i = 0; i < n; i += len)
      for (int k = 0; k < len / 2; k++) {
        float c = cosf(ang * (float)k), s = sinf(ang * (float)k);
        int a = i + k, b = a + len / 2;
        float xr = re[b] * c - im[b] * s, xi = re[b] * s + im[b] * c;
        re[b] = re[a] - xr; im[b] = im[a] - xi;
        re[a] += xr;        im[a] += xi;
      }
  }
}

// Averaged power spectrum of the mono sum (Hann, 50% overlap). Averaging over
// blocks rather than one giant transform is what makes the band shares stable:
// a single FFT of a decaying shot is dominated by whichever block happens to
// straddle the transient.
static void an_spec(const float *b, int frames, int ch) {
  for (int k = 0; k <= AN_N / 2; k++) g_an_pw[k] = 0;
  int blocks = 0;
  for (int off = 0; off + AN_N <= frames || blocks == 0; off += AN_N / 2) {
    for (int i = 0; i < AN_N; i++) {
      float m = 0;
      if (off + i < frames)
        for (int c = 0; c < ch; c++) m += b[(off + i) * ch + c];
      float w = 0.5f - 0.5f * cosf(F_TAU * (float)i / (float)AN_N);
      g_an_re[i] = m * w;
      g_an_im[i] = 0;
    }
    an_fft(g_an_re, g_an_im, AN_N);
    for (int k = 0; k <= AN_N / 2; k++)
      g_an_pw[k] += g_an_re[k] * g_an_re[k] + g_an_im[k] * g_an_im[k];
    blocks++;
    if (off + AN_N >= frames) break;
  }
}

// Share of the energy in each of four bands plus the energy-weighted centroid.
// The band edges are the design vocabulary the shots are authored against: SUB
// is what you feel, LOW is the body, MID is where a rifle lives, HI is the
// crack and the information.
static void an_bands(float *band, float *centroid, float *a_gain) {
  double e[4] = {0, 0, 0, 0}, num = 0, den = 0, aw = 0;
  for (int k = 1; k <= AN_N / 2; k++) {
    float hz = (float)k * (float)SFX_HZ / (float)AN_N;
    double p = g_an_pw[k];
    e[hz < 120.0f ? 0 : hz < 500.0f ? 1 : hz < 2000.0f ? 2 : 3] += p;
    num += p * (double)hz;
    den += p;
    // A-weighting. Energy share alone cannot answer "is this fat" or "is this
    // louder than the gun", because the ear is ~25 dB less sensitive at 50 Hz
    // than at 3 kHz: a shot can put 67% of its energy under 120 Hz and still be
    // perceived as thin. This is the standard IEC 61672 curve, and the ratio it
    // produces turns the RMS into a perceived level.
    double f2 = (double)hz * (double)hz;
    double r = 12194.0 * 12194.0 * f2 * f2
             / ((f2 + 20.6 * 20.6)
                * sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9))
                * (f2 + 12194.0 * 12194.0));
    aw += p * r * r * 1.5849;          // +2.0 dB curve normalisation at 1 kHz
  }
  double tot = e[0] + e[1] + e[2] + e[3] + 1e-30;
  for (int i = 0; i < 4; i++) band[i] = (float)(100.0 * e[i] / tot);
  *centroid = (float)(num / (den + 1e-30));
  *a_gain = (float)(aw / tot);
}

// Envelope decay, optionally band-limited. Two "when does it arrive" numbers,
// and the gap between them is the whole transient-versus-body story:
//   t_atk  time to the loudest SAMPLE — proves the sub-millisecond attack
//          exists at all. Measured on a smoothed envelope it cannot be seen:
//          a 30 Hz follower integrates over 5 ms and reported 7.6 ms for a
//          shot whose transient is 0.5 ms long.
//   t_eng  time to the loudest smoothed ENVELOPE — where the sound's WEIGHT
//          actually lands. This is the one that catches a confirmation whose
//          peak has drifted 70 ms past the event it confirms.
// t20/t60 run from t_eng to the LAST time the envelope is still within
// 20/60 dB of it, which is exactly what "reverberant" asks: not how long the
// buffer is, but how long it is still audible. Running the filters twice
// (once for the peak, once for the crossings) beats storing an envelope for a
// 2.5 s buffer and is bit for bit identical — the filters are deterministic.
static void an_decay(const float *b, int frames, int ch, float lo_hz, float hi_hz,
                     float *t_atk, float *t_eng, float *t20, float *t60) {
  float pk = 1e-12f, raw = 1e-12f;
  int i_pk = 0, i_raw = 0, i20 = 0, i60 = 0;
  for (int pass = 0; pass < 2; pass++) {
    op_t h1 = {0}, h2 = {0}, l1 = {0}, l2 = {0}, sm = {0};
    for (int i = 0; i < frames; i++) {
      float m = 0;
      for (int c = 0; c < ch; c++) m += b[i * ch + c];
      if (lo_hz > 0) { m = op_hp(&h1, m, lo_hz); m = op_hp(&h2, m, lo_hz); }
      if (hi_hz > 0) { m = op_lp(&l1, m, hi_hz); m = op_lp(&l2, m, hi_hz); }
      float a = m < 0 ? -m : m;
      float env = op_lp(&sm, a, 30.0f);                // ~5 ms integration
      if (pass == 0) {
        if (a > raw)   { raw = a; i_raw = i; }
        if (env > pk)  { pk = env; i_pk = i; }
      } else if (i >= i_pk) {
        if (env > pk * 0.1f)   i20 = i;                // -20 dB
        if (env > pk * 0.001f) i60 = i;                // -60 dB
      }
    }
  }
  *t_atk = (float)i_raw / (float)SFX_HZ;
  *t_eng = (float)i_pk / (float)SFX_HZ;
  *t20   = (float)(i20 - i_pk) / (float)SFX_HZ;
  *t60   = (float)(i60 - i_pk) / (float)SFX_HZ;
}

// One line of descriptors for a stereo buffer, in the format both `sfx` and
// `mixdown` print. `win_s` (0 = whole buffer) is the window the LOUDNESS and
// the SPECTRUM are measured over, and it has to be a window for the same reason
// sfx_finish takes one: a shot with a 1.4 s hall on it is 80% tail by frame
// count, so its whole-buffer band split describes the room and says nothing
// about whether the gun has a crack. Measured over the first 300 ms instead,
// the split is the SHOT — comparable between a weapon with a big room and one
// without. Whether the room darkens is a separate question and T60lo/T60hi
// answer it.
static void an_report(const char *tag, const float *b, int frames, float win_s) {
  float peak = 1e-9f;
  double sq = 0;
  int ns = frames * SFX_CH;
  for (int k = 0; k < ns; k++) {
    float a = b[k] < 0 ? -b[k] : b[k];
    if (a > peak) peak = a;
    sq += (double)b[k] * (double)b[k];
  }
  // Stereo width, printed rather than assumed: the L/R correlation of a
  // duplicated mono buffer is exactly 1.0, and a soundfield made of
  // 1.0-correlated sources has no size however good the synthesis is.
  double sl = 0, sr = 0, slr = 0;
  for (int k = 0; k < frames; k++) {
    double l = b[k * SFX_CH], r = b[k * SFX_CH + 1];
    sl += l * l; sr += r * r; slr += l * r;
  }
  double den = sqrt(sl * sr);
  double corr = den > 1e-12 ? slr / den : 1.0;
  double rms = 10.0 * log10(sq / (double)ns + 1e-30);
  // ONE loudness ruler for the whole set. Whole-buffer RMS is not a loudness:
  // the same shot given a two-second hall measures 6 dB quieter purely because
  // it is spread over four times the frames, so it cannot be used to compare a
  // 140 ms hitmarker against a 1.9 s rifle report — and comparing those two is
  // the entire question when the ask is "make the confirmation hit harder".
  // The first 300 ms is what the ear judges an impulse by, and it is what
  // sfx_finish now gain-stages against.
  int nw = win_s > 0 ? (int)(win_s * SFX_HZ) : frames;
  if (nw > frames || nw <= 0) nw = frames;
  double sw = 0;
  for (int k = 0; k < nw * SFX_CH; k++) sw += (double)b[k] * (double)b[k];
  double lw = 10.0 * log10(sw / (double)(nw * SFX_CH) + 1e-30);

  float band[4], cent, awg, atk, eng, t20, t60, d0, d1, d2, lo60, hi60;
  an_spec(b, nw, SFX_CH);
  an_bands(band, &cent, &awg);
  // dBA is the number "fatter / harder-hitting" is actually about. Unweighted RMS
  // rewards putting energy at 40 Hz, where headphones roll off and the ear is
  // 25 dB down; dBA is what the player experiences as loudness, so it is the
  // ruler the confirmations are balanced against the gunshots on.
  // (long), because `frames` is an int frame count and `mixdown` hands this
  // (N + 360) * 400 of them: past N = 5009 — 42 s, well inside what a firefight
  // soak asks for — `frames * 1000` overflows int, which is UB at -O3. gcc
  // happens to fold it to /48 and print the right number, so the only build
  // that shows it is the project's own ASan/UBSan one.
  printf("sfx %-9s len=%4ldms peak=%5.1fdB rms=%6.1fdB L%03ld=%6.1f/%6.1fdBA "
         "crest=%4.1fdB LRcorr=%+.2f\n", tag, (long)frames * 1000 / SFX_HZ,
         20.0 * log10((double)peak), rms, (long)nw * 1000 / SFX_HZ, lw,
         lw + 10.0 * log10((double)awg + 1e-30),
         20.0 * log10((double)peak) - rms, corr);

  an_decay(b, frames, SFX_CH, 0, 0, &atk, &eng, &t20, &t60);
  // Per-band T60 is what separates "the tail is longer" from "the tail is a
  // room": in a real space the high band dies first, so a sound whose decay IS
  // its room must read T60lo > T60hi, and a tail built from one noise burst has
  // them equal by construction (the old sniper: 878 against 910, i.e. it got
  // BRIGHTER as it died). Read it only on sounds long enough for the room to be
  // what is decaying — on a 300 ms confirmation the direct layers still dominate
  // both bands, and a deliberately bright late layer (the headshot's shimmer)
  // legitimately evens them out.
  an_decay(b, frames, SFX_CH, 0, 300.0f, &d0, &d1, &d2, &lo60);
  an_decay(b, frames, SFX_CH, 2000.0f, 0, &d0, &d1, &d2, &hi60);
  printf("sfx+          sub/low/mid/hi=%2.0f/%2.0f/%2.0f/%2.0f%% cent=%5.0fHz "
         "atk=%4.1fms eng=%5.1fms T20=%4dms T60=%4dms lo=%4dms hi=%4dms\n",
         (double)band[0], (double)band[1], (double)band[2], (double)band[3],
         (double)cent, (double)(atk * 1000.0f), (double)(eng * 1000.0f),
         (int)(t20 * 1000.0f), (int)(t60 * 1000.0f),
         (int)(lo60 * 1000.0f), (int)(hi60 * 1000.0f));
}

// 16-bit stereo WAV, hand-rolled 44-byte header — the audio counterpart of the
// PNG writer: lets sound design iterate headless (dump, inspect, listen).
static int write_wav(const char *path, const float *buf, int frames) {
  FILE *f = fopen(path, "wb");
  if (!f) { fprintf(stderr, "cannot write %s\n", path); return 0; }
  int n = frames * SFX_CH;
  uint32_t data = (uint32_t)n * 2, riff = 36 + data, rate = SFX_HZ;
  uint32_t brate = (uint32_t)(SFX_HZ * 2 * SFX_CH);
  uint8_t h[44] = {'R','I','F','F', riff, riff >> 8, riff >> 16, riff >> 24,
                   'W','A','V','E','f','m','t',' ', 16,0,0,0, 1,0, SFX_CH,0,
                   rate, rate >> 8, rate >> 16, rate >> 24,
                   brate, brate >> 8, brate >> 16, brate >> 24,
                   2 * SFX_CH,0, 16,0,
                   'd','a','t','a', data, data >> 8, data >> 16, data >> 24};
  fwrite(h, 1, 44, f);
  for (int i = 0; i < n; i++) {
    int16_t s = (int16_t)(f_clamp(buf[i], -1.0f, 1.0f) * 32767.0f);
    uint8_t b[2] = {(uint8_t)(s & 0xFF), (uint8_t)((uint16_t)s >> 8)};
    fwrite(b, 1, 2, f);
  }
  int err = ferror(f);
  if (fclose(f) != 0) err = 1;
  if (err) fprintf(stderr, "cannot write %s\n", path);
  return !err;
}

