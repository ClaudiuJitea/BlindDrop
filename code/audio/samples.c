// code/audio/samples.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// SAMPLED SOUNDS. Every kill-confirmation take (kill, kill2-4, kill_head,
// kill_head2-3) and the player's own death (die1-2) included: the synth's
// takes of those are the level reference and the fallback, the samples are what
// plays. The guns, steps, hits, kills, deaths, impacts, knife, grenade
// and mission sounds are recorded-style samples (generated with
// tools/gen_sfx.py, packed by tools/build_sfx.py into assets/sfx/pack.bin)
// EMBEDDED in the binary by the assembler, so the game still ships as one file.
// They are applied AFTER sfx_build_all, one buffer per SFX id of the same name:
//
//   * LEVEL COMES FROM THE SYNTH. A sample is scaled to the A-weighted loudness
//     of the synthesized buffer it replaces (or, for an id with no recipe of its
//     own, of a stated reference), over their common opening window, plus a
//     per-sound trim in dB. Every balance the synth was tuned for still holds.
//   * THE ATTACK COMES FROM THE SYNTH TOO, for the guns and the blasts. The
//     generator's reports arrive soft — measured 50 ms attacks where the synth
//     has a sub-millisecond crack — so the synth's own first tens of ms (its
//     crack, its click, its pressure front) are laid UNDER the sample: the
//     sample brings the body, the room and the realism, the synth the punch.
//   * RELOADS ARE ASSEMBLED, not replaced. A generated reload cannot follow the
//     animation, so the reload buffers are built here from one-shots (out, seat,
//     bolt/slide) placed on the very beats the pose plays (RELOAD_BEAT), over
//     the synth's own handling noise at half level.
//   * The slap (a shot's wall echo, placed at run time) stays synthesized. A
//     missing pack or a name matching nothing changes nothing.
// ---------------------------------------------------------------------------
#ifdef _WIN32
#define SFXPACK_SEC ".section .rdata,\"dr\"\n"
#define SFXPACK_VIS ""
#else
#define SFXPACK_SEC ".section .rodata\n"
#define SFXPACK_VIS ".hidden bd_sfxpack\n.hidden bd_sfxpack_end\n"
#endif
// The path is relative to the directory the compiler runs in: the repo root,
// for every Makefile target and for CI.
__asm__(SFXPACK_SEC ".balign 16\n.globl bd_sfxpack\n.globl bd_sfxpack_end\n" SFXPACK_VIS
        "bd_sfxpack:\n.incbin \"assets/sfx/pack.bin\"\nbd_sfxpack_end:\n.byte 0\n.text\n");
#ifdef _WIN32
extern const unsigned char bd_sfxpack[], bd_sfxpack_end[];
#else
extern const unsigned char bd_sfxpack[] __attribute__((visibility("hidden")));
extern const unsigned char bd_sfxpack_end[] __attribute__((visibility("hidden")));
#endif

static uint32_t sfxpack_u32(const unsigned char *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// A-weighted RMS of the first `frames` frames of an interleaved stereo buffer.
static float sfx_aw_rms(const float *b, int frames) {
  awt_t aw[SFX_CH] = {0};
  double sq = 0;
  int n = frames * SFX_CH;
  for (int i = 0; i < n; i++) {
    float a = aw_run(&aw[i % SFX_CH], b[i]);
    sq += (double)a * (double)a;
  }
  return n > 0 ? (float)sqrt(sq / (double)n) : 0.0f;
}

static float sfx_knee(float x) {                    // sfx_finish's soft knee
  float a = x < 0 ? -x : x;
  if (a > 0.55f) {
    float over = a - 0.55f;
    a = 0.55f + over / (1.0f + over * 2.6f);
    x = x < 0 ? -a : a;
  }
  return x;
}

// The pack, found by name. Returns a freshly allocated float copy (stereo,
// interleaved) and its length in frames, or NULL.
static float *sfxpack_load(const char *want, int *frames_out) {
  const unsigned char *p = bd_sfxpack, *end = bd_sfxpack_end;
  if (end - p < 12 || memcmp(p, "BDSX", 4) || sfxpack_u32(p + 4) != 1) return NULL;
  uint32_t count = sfxpack_u32(p + 8);
  size_t table = 12 + (size_t)count * 32;
  if ((size_t)(end - p) < table) return NULL;
  const unsigned char *data = p + table;
  size_t data_n = (size_t)(end - data) / 2;
  for (uint32_t e = 0; e < count; e++) {
    const unsigned char *t = p + 12 + (size_t)e * 32;
    if (strncmp((const char *)t, want, 24)) continue;
    uint32_t frames = sfxpack_u32(t + 24), off = sfxpack_u32(t + 28);
    if (frames == 0 || frames > (uint32_t)(SFX_HZ * 10)) return NULL;
    if ((size_t)off + (size_t)frames * SFX_CH > data_n) return NULL;
    float *b = malloc((size_t)frames * SFX_CH * sizeof(float));
    if (!b) return NULL;
    const unsigned char *s = data + (size_t)off * 2;
    for (size_t i = 0; i < (size_t)frames * SFX_CH; i++)
      b[i] = (float)(int16_t)(s[2 * i] | s[2 * i + 1] << 8) / 32767.0f;
    *frames_out = (int)frames;
    return b;
  }
  return NULL;
}

static int g_sfx_sampled;   // how many ids the pack replaced (harness / log)

// Per id: the synth buffer the level is matched to (-1 = its own), a trim in
// dB on top, and how many ms of that synth buffer's attack go under the sample
// (0 = none).
typedef struct { int ref; float db; float head_ms; } sfx_sample_rule_t;
static sfx_sample_rule_t sfx_rule(int id) {
  switch (id) {
    case SFX_FIRE_AR: case SFX_FIRE_AR2: case SFX_FIRE_AR3: case SFX_FIRE_AR4:
    case SFX_FIRE_AR5: case SFX_FIRE_SR: case SFX_FIRE_SR2: case SFX_FIRE_SR3:
      return (sfx_sample_rule_t){-1, 1.5f, 60.0f};
    case SFX_FIRE_PT: case SFX_FIRE_PT2: case SFX_FIRE_PT3: case SFX_FIRE_PT4:
      return (sfx_sample_rule_t){-1, 3.0f, 50.0f};
    case SFX_FIRE_AK1: case SFX_FIRE_AK2: case SFX_FIRE_AK3:
      return (sfx_sample_rule_t){SFX_FIRE_AR, 2.5f, 60.0f};
    case SFX_FIRE_FAMAS1: case SFX_FIRE_FAMAS2: case SFX_FIRE_FAMAS3:
      return (sfx_sample_rule_t){SFX_FIRE_AR, 1.5f, 55.0f};
    case SFX_FIRE_GLOCK1: case SFX_FIRE_GLOCK2: case SFX_FIRE_GLOCK3:
      return (sfx_sample_rule_t){SFX_FIRE_PT, 3.0f, 45.0f};
    case SFX_FIRE_DEAGLE1: case SFX_FIRE_DEAGLE2: case SFX_FIRE_DEAGLE3:
      return (sfx_sample_rule_t){SFX_FIRE_PT, 5.0f, 60.0f};
    case SFX_BLAST:     return (sfx_sample_rule_t){-1, 6.0f, 120.0f};
    case SFX_BLAST_FAR: return (sfx_sample_rule_t){-1, 4.0f, 60.0f};
    // +5 dB: matched to the hurt thud alone they sat at -23 dBA, 12 dB under the
    // confirmation they arrive with, and a man going down was simply not heard.
    case SFX_DEATH1: case SFX_DEATH2: case SFX_DEATH3: case SFX_DEATH4: case SFX_DEATH5:
      return (sfx_sample_rule_t){SFX_HURT, 5.0f, 0.0f};
    // The kill takes, trimmed so the set sits together (~-15.5 dBA over 300 ms,
    // measured with `sfx`) and every head take a step ABOVE it: the ladder
    // hit < kill < head kill must survive whichever take comes up.
    case SFX_KILL3:      return (sfx_sample_rule_t){-1, -1.8f, 0.0f};
    case SFX_KILL4:      return (sfx_sample_rule_t){-1, -0.3f, 0.0f};
    case SFX_KILL_HEAD:  return (sfx_sample_rule_t){-1, 2.8f, 0.0f};
    case SFX_KILL_HEAD2: return (sfx_sample_rule_t){-1, 0.8f, 0.0f};
    case SFX_KILL_HEAD3: return (sfx_sample_rule_t){-1, 0.8f, 0.0f};
    case SFX_BODYFALL1: case SFX_BODYFALL2: case SFX_BODYFALL3:
      return (sfx_sample_rule_t){SFX_STEP, 6.0f, 0.0f};
    default: return (sfx_sample_rule_t){-1, 0.0f, 0.0f};
  }
}

// One reload, assembled: the one-shots of `ev` at fractions of the reload
// timeline `t` (seconds), over the synth's buffer at half level, the whole then
// set `db` above the synth reload's own loudness.
// `hit` is where the one-shot's own defining transient sits inside it, in
// seconds after its (trimmed) onset — measured on the bank. The CLIP is placed
// so that transient, not the clip's first sample, lands on the beat: the sniper's
// bolt locks 430 ms into its cycle, so placing by onset put that beat late.
// Every value was checked against the built buffer (transient vs beat, +-5 ms).
typedef struct { const char *name; float frac; float gain; float hit; } rl_ev_t;
static void sfx_build_reload(int id, float t, const rl_ev_t *ev, int nev, float db) {
  float *syn = g_sfx_buf[id];
  int len = g_sfx_len[id];
  if (!syn || len <= 0) return;
  float *b = malloc((size_t)len * SFX_CH * sizeof(float));
  if (!b) return;
  // The synth's handling bed is only glue now, at 15%: at half level its
  // filtered-noise "handling" was most of what a reload sounded like, and it is
  // what read as synthetic. The real handling is the rl_handle rustle below.
  for (int i = 0; i < len * SFX_CH; i++) b[i] = syn[i] * 0.15f;
  int placed = 0;
  for (int k = 0; k < nev; k++) {
    int fr = 0;
    float *s = sfxpack_load(ev[k].name, &fr);
    if (!s) continue;
    float pk = 1e-6f;
    for (int i = 0; i < fr * SFX_CH; i++) pk = fmaxf(pk, fabsf(s[i]));
    int at = (int)((ev[k].frac * t - ev[k].hit) * SFX_HZ);
    if (at < 0) at = 0;
    for (int i = 0; i < fr && at + i < len; i++)
      for (int c = 0; c < SFX_CH; c++)
        b[(at + i) * SFX_CH + c] += s[i * SFX_CH + c] / pk * 0.5f * ev[k].gain;
    free(s);
    placed++;
  }
  if (!placed) { free(b); return; }
  float want = sfx_aw_rms(syn, len) * powf(10.0f, db / 20.0f), got = sfx_aw_rms(b, len);
  float g = got > 1e-6f ? want / got : 1.0f;
  for (int i = 0; i < len * SFX_CH; i++) b[i] = sfx_knee(b[i] * g);
  g_sfx_buf[id] = b;   // the synth buffer is freed by the caller's sweep
}

static void sfx_apply_samples(void) {
  // The synth buffers are kept until every sample has been matched: a new id
  // takes its level and its attack from a reference that may itself be
  // replaced in this same pass.
  float *syn[SFX_COUNT];
  int syn_len[SFX_COUNT];
  for (int i = 0; i < SFX_COUNT; i++) { syn[i] = g_sfx_buf[i]; syn_len[i] = g_sfx_len[i]; }
  float *out[SFX_COUNT] = {0};
  int out_len[SFX_COUNT] = {0};
  for (int id = 0; id < SFX_COUNT; id++) {
    if (id == SFX_SLAP || id == SFX_RELOAD_AR || id == SFX_RELOAD_AR_TAC
        || id == SFX_RELOAD_SR || id == SFX_RELOAD_PT || !SFX_NAME[id]) continue;
    sfx_sample_rule_t r = sfx_rule(id);
    int ref = r.ref >= 0 ? r.ref : id;
    if (!syn[ref]) continue;                       // nothing to match against
    int frames = 0;
    float *nb = sfxpack_load(SFX_NAME[id], &frames);
    if (!nb) continue;
    // Loudness over the common opening window (at most 300 ms), where the ear
    // judges a transient's level; a long tail must not dilute it.
    int w = frames < syn_len[ref] ? frames : syn_len[ref];
    if (w > SFX_HZ * 3 / 10) w = SFX_HZ * 3 / 10;
    float want = sfx_aw_rms(syn[ref], w) * powf(10.0f, r.db / 20.0f);
    float got = sfx_aw_rms(nb, w);
    float g = got > 1e-6f ? want / got : 1.0f;
    int hn = (int)(r.head_ms * 0.001f * SFX_HZ);
    if (hn > syn_len[ref]) hn = syn_len[ref];
    if (hn > frames) hn = frames;
    float hk = powf(10.0f, r.db / 20.0f) * 0.85f;   // the attack rides the same trim
    for (int i = 0; i < frames; i++) {
      // The synth's attack, held for the first 60% of its window and faded out
      // over the rest, so the hand-off into the sample's body has no seam.
      float hw = 0.0f;
      if (i < hn) {
        float u = (float)i / (float)hn;
        hw = u < 0.6f ? 1.0f : 1.0f - (u - 0.6f) / 0.4f;
      }
      for (int c = 0; c < SFX_CH; c++) {
        float x = nb[i * SFX_CH + c] * g;
        if (hw > 0.0f) x += syn[ref][i * SFX_CH + c] * hw * hk;
        nb[i * SFX_CH + c] = sfx_knee(x);
      }
    }
    out[id] = nb; out_len[id] = frames;
  }
  for (int id = 0; id < SFX_COUNT; id++) {
    if (!out[id]) continue;
    g_sfx_buf[id] = out[id];   // the synth buffer is freed by the sweep below
    g_sfx_len[id] = out_len[id];
    g_sfx_sampled++;
  }
  // The reloads, on the pose's own beats (see RELOAD_BEAT and the synth
  // builders whose event fractions these follow).
  float hz = (float)TICK_HZ;
  float t_ar = (float)WPN_DEF[WPN_AR].reload_ticks / hz;
  float t_tac = (float)WPN_DEF[WPN_AR].reload_tac_ticks / hz;
  float t_sr = (float)WPN_DEF[WPN_SR].reload_ticks / hz;
  float t_pt = (float)WPN_DEF[WPN_PT].reload_ticks / hz;
  const rl_ev_t AR[] = {{"rl_handle", 0.030f, 0.30f, 0.0f}, {"rl_ar_out", 0.175f, 0.8f, 0.008f},
                        {"rl_handle", 0.330f, 0.25f, 0.0f},
                        {"rl_ar_in", RELOAD_BEAT[WPN_AR][0], 1.0f, 0.012f},
                        {"rl_ar_bolt", RELOAD_BEAT[WPN_AR][1], 1.0f, 0.003f}};
  const rl_ev_t TAC[] = {{"rl_handle", 0.030f, 0.30f, 0.0f}, {"rl_ar_out", 0.185f, 0.8f, 0.008f},
                         {"rl_handle", 0.520f, 0.25f, 0.0f}, {"rl_ar_in", 0.880f, 1.0f, 0.012f}};
  // The bolt one-shot is a whole cycle (lift, draw, forward, down): placed by
  // its LOCK (hit 0.430), so the lock lands on the second beat and the lift and
  // draw run in the 0.43 s before it, where the pose works the bolt.
  const rl_ev_t SR[] = {{"rl_handle", 0.030f, 0.30f, 0.0f}, {"rl_sr_out", 0.185f, 0.8f, 0.004f},
                        {"rl_handle", 0.340f, 0.25f, 0.0f},
                        {"rl_sr_in", RELOAD_BEAT[WPN_SR][0], 1.0f, 0.015f},
                        {"rl_sr_bolt", RELOAD_BEAT[WPN_SR][1], 1.0f, 0.430f}};
  const rl_ev_t PT[] = {{"rl_handle", 0.020f, 0.22f, 0.0f}, {"rl_pt_out", 0.120f, 0.8f, 0.002f},
                        {"rl_pt_in", RELOAD_BEAT[WPN_PT][0], 1.0f, 0.002f},
                        {"rl_pt_slide", RELOAD_BEAT[WPN_PT][1], 1.0f, 0.020f}};
  // Louder than the synth's: a reload is the one moment the player is told the
  // weapon is not ready, and it has to read over a firefight. The pistol's synth
  // sat at -30 dBA, 10 dB under the AR's, which is why it seemed absent.
  sfx_build_reload(SFX_RELOAD_AR, t_ar, AR, 5, 4.0f);
  sfx_build_reload(SFX_RELOAD_AR_TAC, t_tac, TAC, 4, 4.0f);
  sfx_build_reload(SFX_RELOAD_SR, t_sr, SR, 5, 4.0f);
  sfx_build_reload(SFX_RELOAD_PT, t_pt, PT, 4, 10.0f);
  // ONE owner for the replaced synth buffers: freed here and only here.
  for (int i = 0; i < SFX_COUNT; i++)
    if (syn[i] && syn[i] != g_sfx_buf[i]) free(syn[i]);
}
