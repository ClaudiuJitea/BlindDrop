// code/audio/routing.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Audio routing. WHICH sound a game event becomes, how loud, how far to the
// side and how late, is ordinary presentation logic and lives outside the
// platform guard: both native builds feed it to the mixer below, and the
// headless harness feeds it to `sfxlog` and, through that same mixer, to
// `mixdown` — which is the only way any of this gets tested at all, since there
// is no way to listen in the container the game is developed in. The mixer is
// no longer behind the platform guard either; only the audio THREAD that drives
// it is backend-specific (WASAPI on Windows, ALSA on Linux).

// A voice carries its own L/R gains and a start DELAY. Both matter:
// panning is how a shot from behind-left becomes information rather than
// noise, and the delay is how the hit confirmation gets heard at all — see
// audio_on_event.
//
// The voice pool, the trigger ring and audio_mix are NOT platform code — they
// are float math — and keeping them behind #ifdef _WIN32 meant the only thing
// nobody could measure was the one signal the player actually hears. Every
// claim in the comments below about a confirmation sitting "+7.5 dB over its
// own shot" was a claim about the BUS, and the bus is where voice stealing,
// the scheduled delays and the master limiter's ducking all happen. The
// harness now renders it (`mixdown`), so those numbers are checked. Only the
// thread that feeds this is backend-specific (WASAPI on Windows, ALSA on Linux).
typedef struct {
  int active; const float *buf; int len;
  float pos, rate, gl, gr; int delay;
  int prio;                  // SFX_P_* — who ducks whom
  int dying;                 // fading out after a kill trigger — see audio_kill
  int tag;                   // 1 = the player's OWN voice: a kill trigger only
                             // fades matching tags, so cancelling your reload
                             // cannot silence a bot's on the same buffer
} voice_t;

// Three priorities, and the middle one exists because the duck below would
// otherwise take 6.7 dB off the two cues a player cannot afford to miss.
//   LEAD   your own hit/kill confirmation. Ducks everything else, is never
//          stolen, is never dropped.
//   KEEP   taking a round, and a round cracking past your head. Neither ducks
//          nor is ducked: this is the audio that tells you you are being shot
//          and from where, and trading it for your own hitmarker is a bad trade
//          however satisfying the hitmarker is. They barely overlap in spectrum
//          anyway — `hurt` is 72% under 120 Hz, a confirmation's fat is 200-800.
//   NORM   everything else.
// Derived from the sound id rather than passed in, so it cannot be forgotten at
// a call site and cannot be handed to something that is not a confirmation —
// EV_END plays the headshot-kill buffer and legitimately gets LEAD, because the
// end of a match is the one other moment that owns the mix.
// How many voices of ONE sound may be live at once. 0 = uncapped. Derived from
// the id for the same reason sfx_prio is: a call site cannot forget it and
// cannot hand it to something it does not belong on.
static int sfx_class_cap(int id) {
  // 52 and 12, measured rather than guessed. At 30 and 8 the caps became the
  // BINDING CONSTRAINT and total trigger loss got WORSE than the 64-slot pool
  // they were meant to relieve: 9-33 refusals per 20-bot soak against the
  // reference's 0-7, while the pool itself never went past 57 of 96. Per-class
  // concurrency reconstructed from `sfxlog` at 20 bots peaks at 49 far reports
  // and 8 steps, so a cap has to sit ABOVE the real demand to be a backstop
  // rather than a throttle — its job is that no ONE class can own the pool, not
  // that the pool fits.
  if (id == SFX_FAR_AR || id == SFX_FAR_SR || id == SFX_FAR_PT) return 52;
  if (id == SFX_SLAP) return 8;
  if (id == SFX_STEP) return 12;
  return 0;
}

enum { SFX_P_NORM, SFX_P_KEEP, SFX_P_LEAD };
static int sfx_prio(int id) {
  if (id == SFX_HIT || id == SFX_HIT_HEAD || id == SFX_KILL
      || id == SFX_KILL_HEAD || id == SFX_KILL2 || id == SFX_KILL3 || id == SFX_KILL4
      || id == SFX_KILL_HEAD2 || id == SFX_KILL_HEAD3) return SFX_P_LEAD;
  // A death voice is not ducked by the confirmation it arrives with: YOUR kill
  // is the death you most need to hear, and the duck was taking 6.7 dB off it.
  if (id == SFX_HURT || id == SFX_WHIZZ || id == SFX_DIE1 || id == SFX_DIE2
      || (id >= SFX_DEATH1 && id <= SFX_DEATH5)) return SFX_P_KEEP;
  return SFX_P_NORM;
}

// 64, not 48. Every buffer that got a real room also got longer — the AR 520
// -> 660 ms, the sniper 1050 -> 1900, the kill 500 -> 720 — so the same
// firefight now holds more voices alive at once. Measured at 12 hard bots with
// the player holding the trigger: 46 of 48 live and 9 triggers refused, which
// is a distant shot or a footstep simply not happening. 24 more slots cost
// 2.5 kB and one extra early-out per frame.
// 96, not 64, and for the third time the number came from a measurement rather
// than a guess. At 20 bots hard the mixer WANTS 69-71 concurrent voices and got
// 64, so it refused 5-8 triggers a second — and per-class capping the two
// hogs (see sfx_class_cap) turned those 5 refusals into 46-125, i.e. it moved
// the loss rather than removing it, because the demand is real: twenty bots
// firing in a 48 m box is genuinely that many simultaneous sources and a cap
// tight enough to fit 64 makes the arena sound empty. 32 more slots cost 1.5 kB
// and one more early-out per mixed buffer, and they take the whole 20-bot soak
// to dropped = 0 with the caps left only as a backstop against a pathological
// tick. The caps stay because they are what stops ONE class owning the pool.
// IT IS NOT FREE, and the cost is worth writing down for the next person raising
// it: the mixer is O(MAX_VOICES) per audio buffer and the trigger path runs two
// more O(MAX_VOICES) scans per trigger (the free-slot search and the class-cap
// count), so 64 -> 96 measured +9.3% wall clock on an audio-only script (25 s of
// bus rendered in 0.45 s) against +1.3% on a render script and +3.1% on a
// sim-only one. Nothing on the deploy target, and it buys dropped 23-84 -> 0.
#define MAX_VOICES 96
static voice_t g_voices[MAX_VOICES];

// A decibel UNDER the headroom the bus has, deliberately. One centred sound at
// routing gain 1.0 puts the master bus at 0.68 against a 0.92 limiter
// threshold, so 0.92/0.68 = +2.6 dB is the ceiling — but a confirmation does not
// arrive alone, it arrives on top of the shot that caused it, and a lead voice
// sitting exactly at the ceiling made the SUM overshoot and clip 1-3 samples per
// event. 1.20 is +1.6 dB, which puts the lead voice alone at 0.80 and leaves the
// ducked remainder of the mix room underneath. Buying the last decibel with
// level would be a bad trade anyway: a confirmation that pulls the master gain
// down does not get louder, it drags its own tail and every other sound in the
// arena down with it. The rest of the contrast comes from the sidechain duck in
// audio_mix, which costs nothing: 1.6 dB up plus 6.7 dB off everything else is
// an 8.3 dB step, and 8 dB is the difference between "a tick happened" and
// "that was mine".
#define SFX_LEAD_GAIN 1.20f

// `kill` inverts the trigger: instead of starting a voice for `id`, fade out
// every voice playing it WITH THE SAME TAG. It rides the same SPSC ring
// because the voice pool may only ever be mutated on the mixer thread.
typedef struct { int id; float gain, rate, pan; int delay; int kill, tag; } sfx_trig_t;
#define TRIG_RING 64
static sfx_trig_t     g_trig[TRIG_RING];
static _Atomic uint32_t g_trig_w, g_trig_r;
static float          g_dev_rate;   // 48 kHz samples per DEVICE sample (48000/device_hz)

// Every trigger is also recorded, always: the harness reads this back and it
// costs one branch. A mix is a sequence of decisions — which variant, how far
// to the side, how many milliseconds late — and none of those decisions can be
// checked by looking at a waveform.
typedef struct { long tick; int id; float gain, rate, pan, delay; } sfx_log_t;
#define SFX_LOG_N 512
static sfx_log_t g_sfx_log[SFX_LOG_N];
static int  g_sfx_log_n, g_sfx_log_on;
static long g_sfx_log_drops;   // decisions the full log refused — "every
                               // routing decision" must not silently mean
                               // "the first 512 of them"
static long g_sfx_log_tick;
// Whether the headless sim runs the audio routing at all. `sfxlog` wants the
// decisions, `mixdown` wants the signal, and neither may perturb a movement or
// determinism test that did not ask for audio. Harness-only, so the Windows
// build never reads it — the frame loop there drains events unconditionally.
[[maybe_unused]] static int g_audio_route;

// A flooded ring is the same audible symptom as a pool refusal — a sound that
// never played — so it gets a counter too, or the "dropped must read 0" proof
// contract has a loss channel it cannot see.
static long g_trig_flooded;

static void audio_play_tag(int id, float gain, float rate, float pan,
                           float delay_s, int tag) {
  // Anything this quiet is below the noise floor of a firefight; spawning it
  // only costs a voice that a nearer sound needs. `sfxlog` measured 64 sounds
  // wanting to be alive at once in a 20-bot fight against a 24-voice mixer,
  // and most of them were distant shots at gain 0.02.
  if (gain < 0.035f) return;
  if (id < 0 || id >= SFX_COUNT || !g_sfx_buf[id]) return;   // sampled-only, not in the pack
  // The log records ROUTING decisions and deliberately sits BEFORE the ring
  // admit: in the harness's routing-only mode (`sfxlog`) nothing consumes the
  // ring at all, so a log gated on admission would fall silent after 64
  // entries — the command drains the ring itself instead (see its loop).
  if (g_sfx_log_on) {
    if (g_sfx_log_n < SFX_LOG_N)
      g_sfx_log[g_sfx_log_n++] = (sfx_log_t){g_sfx_log_tick, id, gain, rate,
                                             f_clamp(pan, -1, 1), delay_s};
    else
      g_sfx_log_drops++;
  }
  uint32_t w = atomic_load_explicit(&g_trig_w, memory_order_relaxed);
  uint32_t r = atomic_load_explicit(&g_trig_r, memory_order_acquire);
  if (w - r >= TRIG_RING) { g_trig_flooded++; return; }  // flooded: drop,
                                                         // never block the game
  g_trig[w % TRIG_RING] = (sfx_trig_t){id, gain, rate, f_clamp(pan, -1, 1),
                                       (int)(delay_s * SFX_HZ), 0, tag};
  atomic_store_explicit(&g_trig_w, w + 1, memory_order_release);
}
static void audio_play(int id, float gain, float rate, float pan, float delay_s) {
  audio_play_tag(id, gain, rate, pan, delay_s, 0);
}

// Fade out every voice playing `id`. Exists for exactly one caller so far: a
// cancelled reload — the buffer would otherwise play to its end and announce
// A LIVE WEAPON (the bolt cue) for a magazine that never went in. A fade, not
// a stop: cutting a voice mid-body is a step to zero, i.e. a click.
static void audio_kill(int id, int tag) {
  uint32_t w = atomic_load_explicit(&g_trig_w, memory_order_relaxed);
  uint32_t r = atomic_load_explicit(&g_trig_r, memory_order_acquire);
  if (w - r >= TRIG_RING) { g_trig_flooded++; return; }
  g_trig[w % TRIG_RING] = (sfx_trig_t){.id = id, .kill = 1, .tag = tag};
  atomic_store_explicit(&g_trig_w, w + 1, memory_order_release);
}

static uint64_t g_rng_audio = 0xA0D10;
// The confirmation offset depends on WHICH weapon fired, and neither EV_HIT
// nor EV_KILL carries it. Rather than thread a weapon id through ent_damage
// and match_on_kill — sim signatures, changed for a presentation reason —
// latch it here: events are pushed in order within a tick and EV_FIRE always
// precedes the hit and kill it caused, so this is exact, not a guess.
static int      g_last_wpn;  // weapon of the player's most recent shot
static long     g_multi_t = -100000;  // tick of the player's last kill...
static int      g_multi_n;            // ...and how many are inside the window
#define MULTI_WIN 360        // ticks: 3 s. NOT the killfeed's window — that is
                             // KILLFEED_TICKS 480 (4 s) — this is the genre's
                             // own double-kill window, which is shorter.
static v3       g_listen;    // player eye
static v3       g_listen_r;  // ...and which way is right, for panning

// Where a sound sits relative to the listener: -1 hard left, +1 hard right,
// plus the distance it travelled. A source directly ahead or behind pans to
// centre, which is correct — this is a two-speaker model, not HRTF.
static float audio_place(v3 at, float *dist) {
  v3 d = v3_sub(at, g_listen);
  float len = sqrtf(v3_dot(d, d));
  *dist = len;
  if (len < 0.25f) return 0.0f;
  return f_clamp(v3_dot(d, g_listen_r) / len, -1.0f, 1.0f) * 0.85f;
}

// A take of this gun's report, never the one just played: two the same back to back
// is what makes a burst sound like a loop.
// The loadout guns' own takes, [slot: 0 AR, 1 PT][variant - 1]. Sampled-only:
// a variant whose takes are not in the pack falls back to its slot's.
static const int VAR_TAKE[2][2][3] = {
  {{SFX_FIRE_AK1, SFX_FIRE_AK2, SFX_FIRE_AK3}, {SFX_FIRE_FAMAS1, SFX_FIRE_FAMAS2, SFX_FIRE_FAMAS3}},
  {{SFX_FIRE_GLOCK1, SFX_FIRE_GLOCK2, SFX_FIRE_GLOCK3},
   {SFX_FIRE_DEAGLE1, SFX_FIRE_DEAGLE2, SFX_FIRE_DEAGLE3}},
};
static int var_has_takes(int wpn, int var) {
  return (wpn == WPN_AR || wpn == WPN_PT) && var >= 1 && var <= 2
      && g_sfx_buf[VAR_TAKE[wpn == WPN_PT][var - 1][0]];
}
static int sfx_shot_id(int wpn, int var) {
  if (var_has_takes(wpn, var)) {
    static int vlast[2][2];
    int *l = &vlast[wpn == WPN_PT][var - 1];
    int k = (*l + 1 + (int)rng_rangef(&g_rng_audio, 0.0f, 1.999f)) % 3;
    *l = k;
    int id = VAR_TAKE[wpn == WPN_PT][var - 1][k];
    if (g_sfx_buf[id]) return id;
  }
  static const int T[3][5] = {
    [WPN_AR] = {SFX_FIRE_AR, SFX_FIRE_AR2, SFX_FIRE_AR3, SFX_FIRE_AR4, SFX_FIRE_AR5},
    [WPN_SR] = {SFX_FIRE_SR, SFX_FIRE_SR2, SFX_FIRE_SR3},
    [WPN_PT] = {SFX_FIRE_PT, SFX_FIRE_PT2, SFX_FIRE_PT3, SFX_FIRE_PT4},
  };
  static const int N[3] = {[WPN_AR] = 5, [WPN_SR] = 3, [WPN_PT] = 4};
  static int last[3];
  if (wpn < 0 || wpn >= 3) return WPN_DEF[wpn].sfx_fire;
  int k = (last[wpn] + 1 + (int)rng_rangef(&g_rng_audio, 0.0f, (float)N[wpn] - 1.0f)) % N[wpn];
  last[wpn] = k;
  return T[wpn][k];
}

// How far a wall is in one direction: the arena's edge unless something is nearer.
static float audio_wall_dist(v3 o, v3 d) {
  float best = 1e9f;
  if (d.x > 0.01f) best = fminf(best, (ARENA_HALF - o.x) / d.x);
  if (d.x < -0.01f) best = fminf(best, (-ARENA_HALF - o.x) / d.x);
  if (d.z > 0.01f) best = fminf(best, (ARENA_HALF - o.z) / d.z);
  if (d.z < -0.01f) best = fminf(best, (-ARENA_HALF - o.z) / d.z);
  BP_RAY(i, o, d, best) {
    v3 nrm;
    float t = ray_aabb(o, d, g_solids[i].min, g_solids[i].max, &nrm);
    if (t >= 0.0f && t < best) best = t;
  }
  return best;
}

// THE PLACE YOU ARE STANDING. The rooms baked into the reports are the arena in
// general, and they sound the same in the open and with your back to a crate. So
// each of your own shots asks the real geometry how far off the world is on four
// sides and hears the nearest come back, on that side, after the time sound takes to get
// there and back. A wall at your shoulder is the difference you can hear with your
// eyes shut.
static void audio_slap(int wpn) {
  static const float LVL[3] = {[WPN_AR] = 0.46f, [WPN_SR] = 0.62f, [WPN_PT] = 0.42f};
  static const float RATE[3] = {[WPN_AR] = 1.0f, [WPN_SR] = 0.80f, [WPN_PT] = 1.16f};
  if (wpn < 0 || wpn >= 3) return;
  v3 o = v3_add(g_listen, (v3){{0, -0.15f, 0}});
  v3 rt = g_listen_r, fw = {{-rt.z, 0, rt.x}};
  v3 dirs[4] = {rt, v3_scale(rt, -1.0f), fw, v3_scale(fw, -1.0f)};
  static const float PAN[4] = {0.72f, -0.72f, 0.0f, 0.0f};
  // The two nearest only: they are the ones the ear places, and four on every round
  // of a held trigger cost a 20-bot fight five times the dropped sounds.
  float ds[4];
  int near0 = 0, near1 = -1;
  for (int i = 0; i < 4; i++) {
    ds[i] = audio_wall_dist(o, dirs[i]);
    if (ds[i] < ds[near0]) near0 = i;
  }
  for (int i = 0; i < 4; i++)
    if (i != near0 && (near1 < 0 || ds[i] < ds[near1])) near1 = i;
  for (int k = 0; k < 2; k++) {
    int i = k ? near1 : near0;
    float d = ds[i];
    if (d > 30.0f) continue;
    float g = LVL[wpn] / (1.0f + d * 0.17f);
    audio_play(SFX_SLAP, g * rng_rangef(&g_rng_audio, 0.9f, 1.0f),
               RATE[wpn] * rng_rangef(&g_rng_audio, 0.95f, 1.05f), PAN[i], 2.0f * d / 343.0f);
  }
}

static int ent_wvar(int ent, int wpn);   // defined with ent_weapon
static v3 ent_pos(int e);                // defined with the rescue sim
static int ev_var(const event_t *e) {
  if (e->src < 0 || e->src >= MAX_ENTS || (e->wpn != WPN_AR && e->wpn != WPN_PT)) return 0;
  return ent_wvar(e->src, e->wpn);
}
// The shooter's variant voice: a lower, heavier report for the AK and the
// Deagle, a snappier one for the FAMAS and the Glock. Variant 0 is (1, 1). A
// variant with its OWN recorded takes keeps their pitch: re-pitching an AK
// recording would only make it a worse AK.
static void audio_var_voice(const event_t *e, float *rate, float *lvl) {
  int v = ev_var(e);
  if (!v) return;
  if (!var_has_takes(e->wpn, v)) *rate *= WPN_VAR[e->wpn == WPN_PT][v].pitch;
  *lvl *= WPN_VAR[e->wpn == WPN_PT][v].level;
}
// A MAN GOING DOWN, as the world hears it: one of the death voices at the
// victim, and his body reaching the floor a beat later. Never the same voice
// twice running. `local` = it is the player: centred and close. `mine` = the
// player made this kill: he is heard whatever the range, because at 25 m the
// falloff alone had him at 0.13 — under the confirmation, i.e. silence.
static void audio_death(int victim, int local, int mine) {
  static int last_v = -1, last_f = -1;
  if (!g_sfx_buf[SFX_DEATH1]) return;                 // the pack does not carry them
  int v = (last_v + 1 + (int)rng_rangef(&g_rng_audio, 0.0f, 3.999f)) % 5;
  int f = (last_f + 1 + (int)rng_rangef(&g_rng_audio, 0.0f, 1.999f)) % 3;
  last_v = v; last_f = f;
  float pan = 0.0f, dist = 0.0f, att = 1.0f;
  if (!local) {
    pan = audio_place(ent_pos(victim), &dist);
    // Gentler than a step's falloff: a man going down is the outcome of the
    // fight, and the fights in these arenas happen at 15-30 m.
    att = f_clamp(1.0f / (1.0f + dist * dist * 0.008f), 0.0f, 1.0f);
    if (mine && att < 0.60f) att = 0.60f;
  }
  float t = dist / 343.0f;
  audio_play(SFX_DEATH1 + v, (local ? 0.85f : 1.0f) * att,
             rng_rangef(&g_rng_audio, 0.93f, 1.05f), pan, t);
  audio_play(SFX_BODYFALL1 + f, (local ? 0.75f : 0.95f) * att,
             rng_rangef(&g_rng_audio, 0.92f, 1.06f), pan,
             t + rng_rangef(&g_rng_audio, 0.38f, 0.55f));
}
// One of `n` takes, never the one played last time from the same set.
static int audio_take(const int *ids, int n, int *last) {
  int k = (*last + 1 + (int)rng_rangef(&g_rng_audio, 0.0f, (float)(n - 1) - 0.001f)) % n;
  *last = k;
  return ids[k];
}
static void audio_on_event(const event_t *e) {
  switch (e->type) {
  case EV_FIRE: {
    float rate = rng_rangef(&g_rng_audio, 0.985f, 1.015f);
    float vlvl = 1.0f;
    audio_var_voice(e, &rate, &vlvl);
    if (e->src == g_local_ent) {  // your own weapon: dead centre, full level
      g_last_wpn = e->wpn;
      // PER WEAPON, because both buffers are PEAK-normalised and both were
      // routed at the same gain — so a one-shot rifle arrived at the bus
      // exactly as hard as an assault rifle, by construction, with 4.5 dB of
      // headroom sitting unused in front of it. Measured on the bus, a single
      // shot of either peaked at -4.5 dB with the limiter at 1.00. No bot
      // carries the sniper and two sniper shots cannot overlap at 20 rpm, so
      // that headroom is free. Verify with `mixdown` that the limiter stays at
      // 1.00 and `clipped` at 0, including in a 12-bot firefight.
      float lvl = (e->wpn == WPN_SR ? 1.12f : 1.0f) * vlvl;
      audio_play(sfx_shot_id(e->wpn, ev_var(e)),
                 lvl * rng_rangef(&g_rng_audio, 0.92f, 1.0f), rate, 0.0f, 0.0f);
      audio_slap(e->wpn);
      break;
    }
    // Someone else's. Distance is not a volume knob: past ~18 m what reaches
    // you is mostly the town, not the muzzle, so the near and far bakes are
    // CROSSFADED rather than one of them being turned down. Both play in the
    // overlap, which is also what happens in air.
    v3 o = v3_add(e->a, v3_scale(e->b, -e->f));
    float dist, pan = audio_place(o, &dist);
    float fmix = f_clamp((dist - 12.0f) / 14.0f, 0.0f, 1.0f);
    float att = f_clamp(1.0f / (1.0f + dist * dist * 0.0032f), 0.06f, 1.0f);
    // Sound travels: 343 m/s is ~3 ms per metre, and across a 48 m arena that
    // is 140 ms — long enough to hear, and it is most of why a distant shot
    // reads as distant rather than as quiet.
    float t = dist / 343.0f;
    int id_near = sfx_shot_id(e->wpn, ev_var(e));
    int id_far  = e->wpn == WPN_SR ? SFX_FAR_SR : e->wpn == WPN_PT ? SFX_FAR_PT : SFX_FAR_AR;
    // Equal power, not linear. The two bakes are uncorrelated, so their POWERS
    // add rather than their amplitudes, and a linear crossfade between them
    // drops ~3 dB at the midpoint — an audible hole at about 19 m, which is
    // exactly the range most of this arena's fighting happens at.
    if (fmix < 0.98f)
      audio_play(id_near, vlvl * att * sqrtf(1.0f - fmix) * 0.95f, rate, pan, t);
    if (fmix > 0.02f)
      audio_play(id_far, vlvl * att * sqrtf(fmix), rate, pan * 0.7f, t);
    break;
  }
  case EV_HIT:
  case EV_HIT_HEAD: {
    // THE WORLD HEARS IT, WHOEVER FIRED. This runs before and independently of
    // the screen-space confirmation below, because the two are different things
    // for different listeners: the LEAD tick is feedback about the player and is
    // centred and undistanced, this is an event at a POINT in the arena and
    // every shooter makes it. Measured before this, two identical scripts at the
    // same range routed `fire_ar` + `impact` at a WALL and `fire_ar` + the
    // player's own confirmation and NOTHING ELSE at a bot — so every bot-on-bot
    // hit in a 20-bot arena was silent at the target. Same distance gate and the
    // same propagation delay SFX_IMPACT uses, at NORM so a confirmation still
    // ducks it 6.7 dB. Deliberately NOT suppressed on the player's own hits:
    // that would reproduce, in audio, exactly the defect the body spark exists
    // to fix.
    float fd, fp = audio_place(e->a, &fd);
    // 0.52 and an 11 m gate, not 0.85 and 14. This fires on EVERY hit by EVERY
    // shooter, and measured on the bus that new traffic drove the master limiter
    // from -0.3 dB to -2.6 dB over a 12-bot fight — i.e. the new sound was
    // quietly compressing every other sound in the game. A world event that says
    // "someone over there was hit" does not need the level of a round arriving
    // beside your own head.
    if (fd < 11.0f)
      audio_play(SFX_FLESH, f_clamp(0.52f / (1.0f + fd * 0.16f), 0.05f, 0.48f),
                 rng_rangef(&g_rng_audio, 0.92f, 1.10f), fp, fd / 343.0f);
    // A hit that KILLED gets the kill confirmation and nothing else. Both
    // events fire on the same tick from the same shot, and at 45/55 ms the two
    // sub-millisecond bright transients landed 10 ms apart with 0.5 dB between
    // them: not two events, one smeared attack — and their sum drove the master
    // limiter to pull the bus 2 dB, so the most rewarding moment in the game
    // was also its quietest per source. `k` is the victim's remaining hp.
    if (e->src == g_local_ent && e->k <= 0) break;
    if (e->src == g_local_ent && hit_on_plate(e))
      audio_play(SFX_CLINK, 0.40f, rng_rangef(&g_rng_audio, 0.95f, 1.06f), 0.0f,
                 WPN_DEF[e->wpn < WPN_COUNT ? e->wpn : 0].sfx_hit_t);
    if (e->src == g_local_ent) {
      // DELAYED, and that is the whole trick. The confirmation is
      // simultaneous with the shot that caused it, and the shot now has a real
      // 2.5 kHz crack — measured, the tick went from +21.6 dB over the gunshot
      // in its own band to +4.4 dB the moment the gun got its crack back. You
      // cannot win that fight in the frequency domain against your own weapon.
      // You can trivially win it in time: 45 ms later the crack is 16 dB down
      // and the tick is alone. It also reads as consequence rather than as
      // coincidence, which is exactly the feel being asked for.
      // Per WEAPON, not one constant: the sniper's report is three times as
      // long as the AR's, so 45 ms is still inside its crack. Measured at
      // 130 ms the sniper's confirmation goes from -3.0 dB under its own shot
      // to +7.5 dB over it, and is still well inside the window where it reads
      // as caused by the trigger pull.
      audio_play(e->type == EV_HIT_HEAD ? SFX_HIT_HEAD : SFX_HIT, SFX_LEAD_GAIN,
                 rng_rangef(&g_rng_audio, 0.99f, 1.01f), 0.0f,
                 WPN_DEF[g_last_wpn].sfx_hit_t);
    } else if (e->dst == g_local_ent) {
      // Taking a round: its own dark sound, panned to where it came from, so
      // the audio tells you which way to turn. NOT on the killing round —
      // EV_KILL's death voice (the 0.72-rate hurt) is the one confirmation,
      // and without this gate the death tick routed two full-gain copies of
      // the same buffer a fourth apart at delay 0: the src==0 suppression
      // above, mirrored for the victim.
      if (e->k <= 0) break;
      float dist, pan = audio_place(e->a, &dist);
      audio_play(SFX_HURT, 1.0f, rng_rangef(&g_rng_audio, 0.94f, 1.06f),
                 pan * 0.8f, 0.0f);
    }
    break;
  }
  case EV_KILL:
    if (e->src == g_local_ent) {
      // MULTI-KILL ESCALATION, for zero new synthesis. Two kills inside three
      // seconds is a distinct thing that happens and the game said exactly the
      // same sentence both times; the precedent for answering it with a PITCH is
      // already in this file — EV_END replays the headshot-kill buffer a whole
      // tone down. Up a tone, then two: unison / 1.122 / 1.260, capped, and
      // reset by the window itself so it cannot creep.
      // The index is read BEFORE it is advanced, or the FIRST kill of a life
      // plays the double-kill pitch — which is what the first version did.
      if (g_tick - g_multi_t > MULTI_WIN) g_multi_n = 0;
      g_multi_t = g_tick;
      static const float MRATE[3] = {1.0f, 1.122f, 1.260f};
      static const int KB[4] = {SFX_KILL, SFX_KILL2, SFX_KILL3, SFX_KILL4};
      static const int KH[3] = {SFX_KILL_HEAD, SFX_KILL_HEAD2, SFX_KILL_HEAD3};
      static int kb_last = -1, kh_last = -1;
      int kid = e->k ? audio_take(KH, 3, &kh_last) : audio_take(KB, 4, &kb_last);
      // A hair of pitch on top of the take, so even a repeat of a take two
      // kills apart is not bit-identical.
      audio_play(kid, SFX_LEAD_GAIN,
                 MRATE[g_multi_n] * rng_rangef(&g_rng_audio, 0.975f, 1.025f), 0.0f,
                 WPN_DEF[g_last_wpn].sfx_hit_t);
      if (g_multi_n < 2) g_multi_n++;
    }
    else if (e->dst == g_local_ent) {
      // Your own death: its own sound (sfx_die), not the hurt thud slowed down.
      static const int KD[2] = {SFX_DIE1, SFX_DIE2};
      static int kd_last = -1;
      audio_play(audio_take(KD, 2, &kd_last), 1.0f,
                 rng_rangef(&g_rng_audio, 0.97f, 1.03f), 0.0f, 0.0f);
    }
    // ...and every death, the player's included, is a man going down where he
    // stood — the confirmation above is the HUD's, this is the world's.
    if (e->dst >= 0 && e->dst < MAX_ENTS)
      audio_death(e->dst, e->dst == g_local_ent, e->src == g_local_ent);
    break;
  case EV_STEP: {
    float dist, pan = audio_place(e->a, &dist);
    // Your own boots are felt more than heard, so they sit low and centred;
    // everyone else's are the information. Crouching halves it — that is the
    // whole reason to crouch-walk. Pitch varies per step so a sprint reads as
    // footfalls rather than as a machine gun.
    // The speed term divides by the RUN SPEED, not by a literal 5. At
    // mv_run_speed 12 the old constant saturated at 5 m/s, so the top 58% of the
    // speed range carried NO level information at all and a 12 m/s sprint was
    // the same sound as a 5 m/s jog — with only 1.55x of cadence between them
    // over a 4x speed range. It also follows the MOVEMENT slider now, which is
    // the one-copy rule every other mv_* consumer already obeys.
    float rs = g_cfg.mv[MV_RUN] > 1.0f ? g_cfg.mv[MV_RUN] : 1.0f;
    float sp = f_clamp(e->f / rs, 0.22f, 1.0f);
    float g = (e->src == g_local_ent ? 0.30f : 1.15f)
            * (0.30f + 0.90f * sp * sp)
            * (1.0f - 0.55f * e->g)
            * f_clamp(1.0f / (1.0f + dist * dist * 0.020f), 0.0f, 1.0f);
    // A FLOOR, not a fade to nothing. The measured median routed step gain was
    // 0.09 against a 0.035 audibility cut-off, i.e. most steps in a 20-bot match
    // existed only to be inaudible while holding a mean of 4 voices — 10 of the
    // 69 wanted at the pool's overflow instant. Dropping the bottom band outright
    // is what pays for the other-entity constant going 0.85 -> 1.15 above: the
    // steps you CAN hear get louder and the ones you cannot stop existing.
    // OTHER ENTITIES ONLY. The floor's whole argument is the 20-bot voice budget
    // — "most steps exist only to be inaudible" — and the player's own two boots
    // are neither a budget problem nor decoration. Applied to them it silenced
    // the player's crouch-walk COMPLETELY at every speed a crouch permits
    // (0.30 x 0.45 x the clamped speed term = 0.046, always under 0.10), which
    // is a stealth buff smuggled in as a mix fix.
    if (e->src != g_local_ent && g < 0.10f) break;
    audio_play(SFX_STEP, g, rng_rangef(&g_rng_audio, 0.86f, 1.16f),
               e->src == g_local_ent ? 0.0f : pan, dist / 343.0f);
    break;
  }
  case EV_NADE: {
    float dist, pan = audio_place(e->a, &dist);
    // `gain`, not `near`: windef.h defines near/far as macros (see far above).
    float gain = f_clamp(1.0f / (1.0f + dist * dist * 0.02f), 0.03f, 0.9f);
    if (e->k == 1) audio_play(SFX_NADE_BOUNCE, gain * f_clamp(e->f / 7.0f, 0.15f, 1.0f),
                              rng_rangef(&g_rng_audio, 0.92f, 1.10f), pan, 0.0f);
    if (e->k == 2) audio_play(SFX_NADE_SPOON, gain, rng_rangef(&g_rng_audio, 0.96f, 1.05f), pan, 0.0f);
    if (e->k == 3) audio_play(SFX_NADE_EQUIP, gain, 1.0f, pan, 0.0f);
    break;
  }
  case EV_BLAST: {
    float dist, pan = audio_place(e->a, &dist);
    float fmix = f_clamp((dist - 8.0f) / 16.0f, 0.0f, 1.0f);
    float att = f_clamp(1.0f / (1.0f + dist * dist * 0.0022f), 0.14f, 1.0f);
    float t = dist / 343.0f;
    if (fmix < 0.98f) audio_play(SFX_BLAST, att * sqrtf(1.0f - fmix), 1.0f, pan, t);
    if (fmix > 0.02f) audio_play(SFX_BLAST_FAR, att * sqrtf(fmix) * 1.2f, 1.0f, pan * 0.7f, t);
    break;
  }
  case EV_MELEE: {
    float dist, pan = e->src == g_local_ent ? 0.0f : audio_place(e->a, &dist);
    float att = e->src == g_local_ent ? 0.8f
              : f_clamp(1.0f / (1.0f + dist * dist * 0.03f), 0.04f, 0.8f);
    audio_play(e->k ? SFX_KNIFE_WALL : SFX_KNIFE_SWING, att,
               rng_rangef(&g_rng_audio, 0.94f, 1.06f), pan, 0.0f);
    break;
  }
  case EV_BLOCK: {
    float dist, pan = audio_place(e->a, &dist);
    audio_play(SFX_KNIFE_BLOCK, f_clamp(1.0f / (1.0f + dist * dist * 0.02f), 0.05f, 1.0f),
               e->k ? 1.12f : 0.94f, pan, 0.0f);
    break;
  }
  case EV_LAND: {
    // The same boot, heavier and slower. No new synthesis: a landing IS a step
    // with the whole body's mass behind it, so it is the step buffer pitched
    // down and driven by the impact speed. Gated a little tighter than a step
    // because it is a bigger sound.
    float dist, pan = audio_place(e->a, &dist);
    float k = f_clamp((e->f - 1.6f) / 7.0f, 0.15f, 1.0f);
    float g = (e->src == g_local_ent ? 0.85f : 1.5f) * (0.35f + 0.85f * k)
            * f_clamp(1.0f / (1.0f + dist * dist * 0.016f), 0.0f, 1.0f);
    if (g < 0.10f) break;
    audio_play(SFX_STEP, g, 0.66f + 0.14f * (1.0f - k) +
               rng_rangef(&g_rng_audio, -0.03f, 0.03f),
               e->src == g_local_ent ? 0.0f : pan, dist / 343.0f);
    break;
  }
  // The dry click is a hands-on cue: it says YOUR magazine is empty, so it is
  // the local shooter's only. Ungated it played centred and full gain for any
  // entity — the "literal == 0 means me" class, one identity late. The event
  // printer already reads g_local_ent here.
  case EV_DRYFIRE:
    if (e->src == g_local_ent) audio_play(SFX_DRY, 1.0f, 1.0f, 0.0f, 0.0f);
    break;
  case EV_RELOAD:
    // The player's own reload is in his hands: centred, close, undistanced. A
    // bot's is an event at a point in the arena and is routed like one.
    // The rate jitter is +/-2.5% — about +/-43 cents — and deliberately far
    // tighter than the footstep's +/-15%, because reload TEMPO is a gameplay
    // quantity here and every event offset is a FRACTION of reload_ticks. It
    // exists because this is the most repetition-exposed buffer in the set: 1.3
    // seconds long, played 15-25 times a match, and until now the only repeated
    // cue in the file with no variation at all.
    // `k` is the tactical flag. Only the AR splits: a bolt gun works its action
    // whether or not the chamber was loaded, so the sniper has one buffer.
    {
    // A NEUTRALISED src is dropped. net_ev_to_live lands a hostile entity id on
    // -1, and `src + 2` would then hand it tag 1 — the PLAYER's reserved tag —
    // so a later cancel would fade the player's own reload. There is also no
    // entity to place the sound at.
    if (e->src < 0) break;
    int rid = (e->k && e->wpn == WPN_AR) ? SFX_RELOAD_AR_TAC
                                         : WPN_DEF[e->wpn].sfx_reload;
    if (e->src == g_local_ent) {
      // tag 1 = player-owned: the cancel's audio_kill fades only this voice.
      audio_play_tag(rid, 0.9f,
                     rng_rangef(&g_rng_audio, 0.975f, 1.025f), -0.12f, 0.0f, 1);
    } else {
      float dist, pan = audio_place(e->a, &dist);
      if (dist < 14.0f)
        audio_play_tag(rid,
                       0.75f * f_clamp(1.0f / (1.0f + dist * dist * 0.028f), 0.0f, 1.0f),
                       rng_rangef(&g_rng_audio, 0.96f, 1.04f), pan, dist / 343.0f,
                       e->src + 2);
    }
    }
    break;
  // The tag is entity-specific, so a bot's aborted reload fades only its own
  // voice rather than every nearby bot sharing the same weapon buffer.
  case EV_RELOAD_CANCEL:
    {
      if (e->src < 0) break;                 // same reserved-tag rule as above
      int tag = e->src == g_local_ent ? 1 : e->src + 2;
      audio_kill(WPN_DEF[e->wpn].sfx_reload, tag);
      if (e->wpn == WPN_AR) audio_kill(SFX_RELOAD_AR_TAC, tag);
    }
    break;
  case EV_END:
    // The victory sting is the SAME BUFFER as the headshot kill, one whole tone
    // down, and it was played at delay 0 while the kill that won the match was
    // waiting out its per-weapon confirmation offset. So on the one shot in a
    // match that matters most, the answer arrived 45 ms (AR) or 130 ms (sniper)
    // BEFORE the thing it was answering — two plays of one buffer, a semitone
    // apart, overlapping: exactly the smeared-double-attack failure this file
    // already paid for once with EV_HIT and EV_KILL. It now starts a clear
    // 380 ms after the confirmation, which is past the kill's own T20 (270 ms),
    // so it reads as a consequence instead of a flam.
    //
    // AND IT HAS TO READ THE WINNER. `e->k` is the winning entity and was
    // simply never consulted, so the loudest sound in the game — the headshot
    // kill at LEAD priority, ducking every other voice 6.7 dB — was the sound
    // of LOSING. Measured: `bots 6; skill hard; fraglimit 1; sfxlog 400`
    // prints `matchover winner=ONYX` with the player on zero kills and, one
    // tick earlier, `kill_head gain=1.20`. A defeat now gets the dull, slow
    // answer the player's own death already uses (rate 0.72; SFX_HURT routes
    // as KEEP — riding the lead bus unducked without ducking anything), so it
    // neither ducks the arena nor congratulates anybody.
    // g_local_ent, not a literal 0: EV_END.k is the WINNER's entity id, and on
    // a client that id is the own slot, never 0.
    if (won_by_me(e->k))
      audio_play(SFX_KILL_HEAD, SFX_LEAD_GAIN, 0.85f, 0.0f,
                 WPN_DEF[g_last_wpn].sfx_hit_t + 0.38f);
    else
      audio_play(SFX_HURT, 0.85f, 0.62f, 0.0f, 0.30f);
    break;
  // Listed rather than defaulted, so -Wswitch makes a new ev_type_t a COMPILE
  // ERROR here instead of a silently inaudible event. ev_push's exhaustive
  // printer is gated on `g_headless`, so on any WINDOWED target this switch and
  // vfx_on_event's are the only enforcement that exists.
  case EV_IMPACT: {
    // A ROUND ARRIVING IN THE WORLD IS ITS OWN EVENT, and it has to be routed
    // off that event rather than off the shot's outcome. It used to be spawned
    // in audio_shot_world from `EV_FIRE.k == 1`, which was exactly equivalent
    // while a wall always stopped the round — and stopped being equivalent the
    // moment one could cross a wall and hit a body behind it: the shot then
    // carries k = 2/3, and the wall the shooter is looking at threw sparks and
    // left a hole in total silence. EV_IMPACT is pushed for every arrival,
    // entry holes included, so this is right by construction.
    // Only impacts you could plausibly pick out. Past ~22 m a round hitting a
    // wall is inside the tail of the shot that fired it, and spawning it just
    // spends a voice a nearer sound needs.
    if (e->k) break;                 // a through hole: see the push site
    float dist, pan = audio_place(e->a, &dist);
    // A ROUND HITTING THE WALL BESIDE YOUR HEAD AND ONE 20 M AWAY SOUNDED THE
    // SAME. The old law spans 7.6 dB from 0.5 m to 20 m — measured over 992
    // logged impacts only 21 (2.1%) ever reached its 0.85 clamp — where the
    // physics is ~32 dB. Split law: beyond the seam the old curve, which is
    // tuned and which most impacts live on; inside it an inverse-square, giving
    // 0.5 m -> 0.98. The SEAM IS THE CURVES' OWN CROSSING (1.33 m, where the
    // inverse-square meets the far branch's 0.85 clamp) and not a round number:
    // split at 2.5 m the law stepped UP by 2.8 dB — an impact at 2.3 m routed
    // QUIETER than one at 3.2 m, inverting the exact distance cue the split was
    // built to restore. The near case earns its level by being the cue that
    // says a round just missed your head.
    if (dist < 22.0f)
      audio_play(SFX_IMPACT,
                 dist < 1.33f
                     ? f_clamp(1.0f / (1.0f + 0.10f * dist * dist), 0.08f, 1.0f)
                     : f_clamp(1.2f / (1.0f + dist * 0.12f), 0.10f, 0.85f),
                 rng_rangef(&g_rng_audio, 0.90f, 1.12f), pan, dist / 343.0f);
    break;
  }
  case EV_RESPAWN:
    break;
  }
}

// The near-miss crack is spawned from the SHOT event rather than from its own
// event type: it is a property of the round's whole flight segment, which only
// EV_FIRE carries. (The impact that used to live here moved onto EV_IMPACT —
// see there.)
// The last fly-by, so a burst walking past reads as one crack and a rattle
// rather than as six copies of a 200 ms buffer. Measured, six triggers land
// inside one 0.5 s window at 20 bots, and the cue is SFX_P_KEEP in a pool that
// already refuses triggers at that bot count.
static long  g_whizz_t = -1000;
static float g_whizz_g;
#define WHIZZ_HOLD 11        // ticks: 90 ms
// 4.2 m, not 3.0. A supersonic round is plainly audible further out than that,
// and with the level law now concave the added band arrives quiet by
// construction — measured, everything past 2.5 m routes under 0.06 — so it
// widens what you can HEAR without widening what spends voices.
#define WHIZZ_MISS 4.2f

static void audio_shot_world(const event_t *e) {
  if (e->type != EV_FIRE) return;
  // EXCLUDE ONLY ROUNDS THAT HIT THE LISTENER. The old guard rejected `k == 2 ||
  // k == 3`, i.e. ANY round that terminated on a body — and measured over 7200
  // ticks of 12-bot hard that is 36.7% of every round fired, including the one
  // that kills the man standing beside you. A round that hit YOU correctly gets
  // `hurt` and no crack; a round that hit somebody else should still crack past
  // your ear if the geometry says so, and the segment test below already handles
  // the flight ending at the victim.
  if (e->src != g_local_ent && e->dst != g_local_ent) {
    // A round that went past you. The event's `a` is the IMPACT point, not the
    // muzzle, and measuring along the ray from there put the closest-approach
    // parameter behind the listener for exactly the case that matters: a round
    // that passes your head and buries itself in the wall BEHIND you scored
    // t < 0 and was silent. Measured from the muzzle, and required to lie on
    // the actual flight segment, so a round that stopped short of you cannot
    // crack past your ear either.
    v3 muzzle = v3_sub(e->a, v3_scale(e->b, e->f));
    v3 ax = v3_norm(e->b);
    v3 d = v3_sub(g_listen, muzzle);
    float t = v3_dot(d, ax);
    v3 perp = v3_sub(d, v3_scale(ax, t));
    float miss = sqrtf(v3_dot(perp, perp));
    if (t > 0.5f && t < e->f - 0.5f && miss < WHIZZ_MISS) {
      // ...and WHICH side it went past, which was hardcoded to centre.
      float pan = miss > 0.01f
                ? f_clamp(v3_dot(perp, g_listen_r) / miss, -1.0f, 1.0f) * 0.9f
                : 0.0f;
      // A CONCAVE LEVEL LAW, because a linear one cannot say "that was CLOSE".
      // Measured over 275 real triggers, 17% land inside 0.5 m and 40% in
      // 0.5-1.0 m — and `1 - miss/3` puts 0.2 m only 2.9 dB over 1.0 m, i.e.
      // the whole band that holds 57% of the population is flat. The exponent is
      // 3.3 and NOT the 2.2 it was first solved at, because the gate was widened
      // from 3.0 m to 4.2 afterwards and an exponent is only a contrast over a
      // SPAN: at 2.2 over 4.2 m the same pair came out 4.26 dB, i.e. a third of
      // the contrast given straight back. At 3.3 it is 6.40 dB again (0.783 vs
      // 0.375) and 2.5 m routes 0.047, so widening what you can HEAR costs no
      // voices — everything past the old gate arrives inaudible by construction.
      float k = f_clamp(1.0f - miss / WHIZZ_MISS, 0.0f, 1.0f);
      float g = powf(k, 3.3f) * 0.92f;
      // THE CRACK ARRIVES BEFORE THE BANG. The round covers `t` metres at its
      // muzzle velocity and its bow wave then travels `miss` metres of air to
      // the ear; the report covers the whole `t` at 343 m/s. That difference is
      // 1.8 ms per metre of shot at 900 m/s — 35 ms at this arena's median
      // engagement. The rate is 1.80 ms per metre of shot at 900 m/s, so the
      // arena's 68 m diagonal would be 122 ms; in practice the gate's own
      // segment test keeps almost every trigger well inside that.
      float v = WPN_DEF[e->wpn].v_round;
      float delay = t / (v > 1.0f ? v : 343.0f) + miss / 343.0f;
      // One crack per 90 ms unless the new one is closer, so a burst is a crack
      // and a rattle. The louder of a cluster is the one that survives, which is
      // also the one that was nearest.
      if (g_tick - g_whizz_t < WHIZZ_HOLD && g <= g_whizz_g) return;
      g_whizz_t = g_tick;
      g_whizz_g = g;
      // A near pass is SHARPER as well as louder: the rate rides the same curve,
      // so the bow wave's whole spectrum shifts up as it gets closer instead of
      // only its level.
      audio_play(SFX_WHIZZ, g, rng_rangef(&g_rng_audio, 0.92f, 1.10f)
                 * (0.90f + 0.22f * k), pan, delay);
    }
  }
}

static void audio_event(const event_t *e) { audio_on_event(e); audio_shot_world(e); }

// MAX_VOICES mixer, fed through a lock-free SPSC ring: the game thread posts
// triggers, the audio thread (WASAPI or ALSA — or, headless, the `mixdown`
// command) starts voices and mixes.
static float g_lim_g = 1.0f;   // master limiter gain state
// Two measurement hooks, one compare each. They exist because the two ways
// this mixer can quietly ruin a sound are both invisible in the output WAV
// unless you know what to look for: the master limiter ducking the WHOLE bus
// (so a "louder" confirmation just makes everything else quieter), and a
// trigger being dropped or a voice stolen because the pool ran out.
static float g_lim_min = 1.0f;   // deepest bus gain reduction seen
static int   g_voice_peak;       // most voices simultaneously audible
static long  g_trig_dropped;     // triggers the pool refused
// ...and the ones sfx_class_cap refused, which is a DIFFERENT fact. A cap
// refusal is a policy decision about a 31st simultaneous distant report; a pool
// overflow is the mixer running out of room. Folded into one number, `mixdown`
// cannot tell a healthy budget from a starved one — and tuning a cap against a
// number that conflates them is how the first attempt took `dropped` from 5 to
// 125 and could have read that as progress.
static long  g_trig_capped;
static float g_duck_env;         // lead-voice envelope driving the sidechain
static float g_duck_min = 1.0f;  // deepest duck applied to the non-lead bus

// The menu-pause reaches the AUDIO TIMELINE too: freezing only the tick
// accumulator lets every in-flight voice run ahead of the frozen sim — a
// paused mid-reload played its seat and bolt beats ("A LIVE WEAPON") over a
// world in which the magazine never moved, then completed silently on resume.
// The mixer thread reads the flag and (a) ramps a master pause envelope to
// zero over ~35 ms — a step to silence is a click — then (b) FREEZES voice
// positions and scheduled delays until the flag clears. Harness mixdowns never
// set the flag, so every audio proof is byte-identical.
static _Atomic int g_audio_pause;
static float       g_pause_env = 1.0f;

static void audio_mix(float *out, uint32_t frames, int channels) {
  uint32_t r = atomic_load_explicit(&g_trig_r, memory_order_relaxed);
  uint32_t w = atomic_load_explicit(&g_trig_w, memory_order_acquire);
  for (; r != w; r++) {
    sfx_trig_t t = g_trig[r % TRIG_RING];
    if (t.kill) {  // fade out, don't cut — see audio_kill; tags must match
      for (int i = 0; i < MAX_VOICES; i++)
        if (g_voices[i].active && g_voices[i].buf == g_sfx_buf[t.id] &&
            g_voices[i].tag == t.tag) {
          // Still inside its scheduled delay = never was audible: freeing the
          // slot outright cannot click, and a fade would let it start first.
          if (g_voices[i].delay > 0) g_voices[i].active = 0;
          else g_voices[i].dying = 1;
        }
      continue;
    }
    // Equal-power pan. Linear panning drops 3 dB in the middle, which on a
    // sound sweeping across the field reads as a hole in the centre.
    // PER-CLASS CONCURRENCY CAPS, and they exist because two classes were
    // measured spending the pool on things nobody can hear. At 20 bots hard the
    // mixer wants 69-71 voices against 64: reconstructed per class, the mean
    // live set is far_ar 22.1 and step 4.0 against impact 0.8 and whizz 0.2 —
    // i.e. the distant-report bake alone holds a third of the pool, its
    // attenuation floor does not bind until 70 m in a 48 m arena, and half the
    // steps route under gain 0.09. A cap is better than a steal here: the 9th
    // simultaneous distant report carries no information the other eight do not,
    // and dropping it is silent while stealing is a click.
    int cap = sfx_class_cap(t.id);
    if (cap > 0) {
      // AUDIBLE voices only. A voice sitting on its propagation delay is not
      // making any sound yet and one that is fading out is nearly done, so
      // counting either makes the effective cap tighter than the number says —
      // at 20 bots a far report holds a slot for its 100 ms of flight time
      // before it can be heard at all.
      int live = 0;
      for (int i = 0; i < MAX_VOICES; i++)
        if (g_voices[i].active && !g_voices[i].dying && g_voices[i].delay == 0
            && g_voices[i].buf == g_sfx_buf[t.id]) live++;
      if (live >= cap) { g_trig_capped++; continue; }
    }
    float gl = t.gain * sqrtf(0.5f * (1.0f - t.pan));
    float gr = t.gain * sqrtf(0.5f * (1.0f + t.pan));
    int slot = -1;
    for (int i = 0; i < MAX_VOICES; i++)
      if (!g_voices[i].active) { slot = i; break; }
    int prio = sfx_prio(t.id);
    int lead = prio == SFX_P_LEAD;
    if (slot < 0) {
      // Steal, but only from a voice that is already MOSTLY PLAYED and quieter
      // than the newcomer. Cutting a voice mid-body is an instantaneous jump to
      // zero, which is a click — and a click during a firefight is exactly when
      // you can least afford one. If nothing qualifies, drop the new trigger
      // instead: a 49th simultaneous sound is not information.
      //
      // A LEAD voice is exempt from both halves of that. It may never be stolen
      // (the pool filling up must not be able to swallow your hitmarker), and
      // when one arrives to a full pool it takes the quietest non-lead slot
      // whatever state that voice is in. A 12-bot firefight already measured 5
      // dropped triggers against 46 of 48 voices live, so "the confirmation
      // sometimes just does not play" was reachable in normal play.
      // ...and KEEP is exempt from being stolen BY A NORM VOICE, for the same
      // reason it is exempt from the duck: `hurt` and the fly-by crack are the
      // two cues that tell you you are being shot and from where, and they were
      // droppable exactly when you are being shot at most — measured, 10 of the
      // 69 voices wanted at a 20-bot overflow. A LEAD confirmation may still
      // take one, because a confirmation outranks everything by construction.
      float worst = 1e9f;
      for (int i = 0; i < MAX_VOICES; i++) {
        if (g_voices[i].prio == SFX_P_LEAD) continue;
        if (g_voices[i].prio == SFX_P_KEEP && prio == SFX_P_NORM) continue;
        float g = g_voices[i].gl + g_voices[i].gr;
        if (!lead && g_voices[i].pos < (float)g_voices[i].len * 0.55f) continue;
        if (g < worst) { worst = g; slot = i; }
      }
      if (slot < 0 || (!lead && worst > gl + gr)) { g_trig_dropped++; continue; }
    }
    // The delay arrives in 48 kHz samples and is consumed one per DEVICE frame,
    // so on a 44.1 kHz device every scheduled offset ran 8.8% long — including
    // the propagation delays that carry distance and the confirmation offsets
    // that keep the hit tick out from under the shot. Converted here, where
    // g_dev_rate (48000/device_hz) is known and is read on the thread that set
    // it. The limiter's release coefficient below is device-rate-relative for
    // the same reason.
    g_voices[slot] = (voice_t){1, g_sfx_buf[t.id], g_sfx_len[t.id],
                               0, t.rate, gl, gr,
                               (int)((float)t.delay / (g_dev_rate > 0.01f
                                                       ? g_dev_rate : 1.0f)),
                               prio, 0, t.tag};
  }
  atomic_store_explicit(&g_trig_r, r, memory_order_release);

  // THE VOLUME KNOB IS AN OUTPUT TRIM, NOT A DRIVE. It used to be applied
  // BEFORE the limiter, so turning it up fed the limiter harder instead of
  // making anything louder: measured on one 20-bot fight, 0.40 -> 1.00 is 8.0 dB
  // of nominal gain that delivered 4.3 dB of loudness and cost 3.9 dB of crest,
  // with the limiter running -0.4 dB at 0.40 and -8.4 dB at 1.00. The internal
  // pre-limiter level is now PINNED at the old default (0.70), so a session at
  // the default volume is bit-identical to before and every level relationship
  // in the file — the lead gain, the duck step, the confirmation ladder — is
  // measured against one fixed bus whatever the player has the knob at.
  // ...and the trim is RELATIVE to that pinned level, or the knob is applied
  // twice. Pinning the pre-limiter bus at 0.70 and then multiplying the output
  // by g_cfg.vol makes the DEFAULT session 20*log10(0.7) = 3.10 dB quieter than
  // it was — measured, an isolated AR shot went -30.6 -> -33.7 dBA and every bus
  // number in this file would have been quoted against a bus 3.1 dB below the
  // one they were tuned on. Dividing by the reference restores exact identity at
  // the default and turns the knob into a real trim in both directions; the soft
  // knee below still runs after it, so >1.0 cannot clip.
  const float vol = VOL_REF;
  const float out_vol = g_cfg.vol / VOL_REF;
  int paused = atomic_load_explicit(&g_audio_pause, memory_order_relaxed);
  for (uint32_t f = 0; f < frames; f++) {
    // Pause envelope: fade to silence, then hold every voice in place. The
    // fade-out keeps ADVANCING the voices (a frozen position under a live
    // gain would loop one sample), so resume picks up ~35 ms past the freeze
    // point — inaudible against a fade-in from silence.
    if (paused) {
      g_pause_env *= 0.9945f;
      if (g_pause_env < 1e-4f) {  // fully faded: freeze voices, emit silence
        if (channels == 1) out[f] = 0;
        else for (int c = 0; c < channels; c++)
          out[f * (uint32_t)channels + (uint32_t)c] = 0;
        continue;
      }
    } else if (g_pause_env < 1.0f) {
      g_pause_env += (1.0f - g_pause_env) * 0.0055f;
      if (g_pause_env > 0.9995f) g_pause_env = 1.0f;
    }
    float sl = 0, sr = 0, ll = 0, lr = 0, kl = 0, kr = 0;
    int live = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
      voice_t *v = &g_voices[i];
      if (!v->active) continue;
      if (v->delay > 0) { v->delay--; continue; }  // scheduled, not yet audible
      if (v->dying) {  // ~35 ms exponential fade at 48 kHz, then the slot frees
        v->gl *= 0.9945f;
        v->gr *= 0.9945f;
        if (v->gl + v->gr < 1e-4f) { v->active = 0; continue; }
      }
      int ip = (int)v->pos;
      if (ip + 2 >= v->len) { v->active = 0; continue; }
      float fr = v->pos - (float)ip;
      const float *b = v->buf + ip * SFX_CH;
      // Four points, not two. A straight line between samples is a low-pass: 2 dB
      // down at 10 kHz halfway between them, on every shot pitched off exactly 1.0.
      const float *pb = ip > 0 ? b - SFX_CH : b;
      float f2 = fr * fr, f3 = f2 * fr;
      float w0 = 0.5f * (-f3 + 2.0f * f2 - fr), w1 = 0.5f * (3.0f * f3 - 5.0f * f2 + 2.0f);
      float w2 = 0.5f * (-3.0f * f3 + 4.0f * f2 + fr), w3 = 0.5f * (f3 - f2);
      float xl = (pb[0] * w0 + b[0] * w1 + b[SFX_CH] * w2 + b[2 * SFX_CH] * w3) * v->gl;
      float xr = (pb[1] * w0 + b[1] * w1 + b[SFX_CH + 1] * w2 + b[2 * SFX_CH + 1] * w3) * v->gr;
      // KEEP voices bypass the duck by riding the lead bus WITHOUT feeding the
      // sidechain: they are neither attenuated nor allowed to attenuate.
      if (v->prio == SFX_P_LEAD)      { ll += xl; lr += xr; }
      else if (v->prio == SFX_P_KEEP) { kl += xl; kr += xr; }
      else                            { sl += xl; sr += xr; }
      v->pos += v->rate * g_dev_rate;
      live++;
    }
    if (live > g_voice_peak) g_voice_peak = live;
    // Sidechain duck. This is where the confirmation gets to be the loudest
    // thing in the game, and the reason it is done here rather than by turning
    // the buffer up is arithmetic: one shot already puts the bus at 0.68 against
    // a 0.92 limiter threshold, so there is +2.6 dB of headroom and no more. A
    // confirmation loud enough to dominate by level alone drives the master
    // limiter 4-8 dB down, which does not make the confirmation louder — it
    // makes everything else quieter WITH it, including its own tail, and it
    // pumps every other sound in the arena for 170 ms. Ducking the non-lead bus
    // instead spends nothing: the contrast is the same, the peak is not, and the
    // gunfire comes back up under the confirmation's own decay.
    // Attack ~2.5 ms so the duck is already down when the confirmation's own
    // transient arrives; release ~130 ms so it lifts under the decay instead of
    // chattering. Both device-rate relative, for the reason the limiter's
    // release is: a coefficient in samples means a different time on a 44.1 kHz
    // device.
    float lm = fabsf(ll) > fabsf(lr) ? fabsf(ll) : fabsf(lr);
    float d_atk = 0.0084f * g_dev_rate, d_rel = 0.00016f * g_dev_rate;
    if (lm > g_duck_env) g_duck_env += (lm - g_duck_env) * d_atk;
    else                 g_duck_env += (lm - g_duck_env) * d_rel;
    // A DEEPER FLOOR, AND IT IS THE LAST FREE LEVER IN THE SYSTEM. Measured in a
    // real 20-bot hard fight, a confirmation adds only 0.55 dB to the loudest
    // 40 ms window of the arena and ranks at the 96th percentile of those
    // windows rather than the 100th — while the bus is ALREADY 2.8 dB compressed
    // by the firefight before the confirmation arrives, so there is no level
    // headroom left to buy separation with and SFX_LEAD_GAIN is already at the
    // limiter's ceiling. Ducking harder costs nothing: it attenuates only the
    // NORM bus, it moves no peak, and it drives no limiter. 1.55 also means the
    // floor is actually reached FASTER, and deeper: the old 1.30 already bottomed
    // out on a plain body hit, so this is depth rather than reachability.
    // `hurt` and the fly-by crack are SFX_P_KEEP and ride the lead bus without
    // feeding the sidechain, so the two cues that tell you you are being shot
    // are exempt from this by construction — which is the whole reason the
    // middle priority exists.
    float duck = 1.0f - 1.55f * g_duck_env;
    if (duck < 0.32f) duck = 0.32f;              // -9.9 dB, never silence
    sl = (sl * duck + ll + kl) * vol * 1.4142f * g_pause_env;
    sr = (sr * duck + lr + kr) * vol * 1.4142f * g_pause_env;
    if (duck < g_duck_min) g_duck_min = duck;
    // Master limiter, not a hard clamp. Clamping a mix of six overlapping
    // shots is distortion on every peak; a limiter with a fast attack and a
    // ~170 ms release ducks the whole bus for a moment instead, which is what
    // makes a firefight loud AND clean. The gain state is per-buffer and
    // smoothed, so it can never introduce a click of its own.
    float mag = fabsf(sl) > fabsf(sr) ? fabsf(sl) : fabsf(sr);
    float want = mag > 0.92f ? 0.92f / mag : 1.0f;
    float rel = 0.00012f * g_dev_rate;   // ~170 ms whatever the device runs at
    if (want < g_lim_g) g_lim_g += (want - g_lim_g) * 0.55f;      // fast down
    else                g_lim_g += (want - g_lim_g) * rel;        // slow up
    if (g_lim_g < g_lim_min) g_lim_min = g_lim_g;
    sl *= g_lim_g; sr *= g_lim_g;
    sl *= out_vol; sr *= out_vol;
    // The safety net is a soft knee, not a clamp. The limiter above needs a few
    // samples to react, so the leading edge of a transient that arrives on top
    // of an already-loud bus can overshoot: measured in the worst case tested —
    // 12 hard bots, the player firing and landing hits — a hard clamp squared
    // off 4 samples per 21 seconds. Four samples is not audible on its own, but
    // a clamp is the one nonlinearity here with a CORNER in it, and it is
    // reached exactly at the loudest moments. This rational knee is transparent
    // under 0.90 and asymptotes to 1.0 from below, so the output cannot clip by
    // construction and there is nothing left to count.
    for (int c = 0; c < 2; c++) {
      float *x = c ? &sr : &sl;
      float a = *x < 0 ? -*x : *x;
      if (a > 0.90f) {
        float over = a - 0.90f;
        a = 0.90f + over / (1.0f + over * 10.0f);
        *x = *x < 0 ? -a : a;
      }
    }
    // Address the first two channels and ZERO the rest. Writing L/R alternating
    // into every channel of a 5.1 or 7.1 mix format puts a full copy of the
    // programme in the centre, the LFE and both surround pairs — which on a
    // surround setup collapses the stereo image the whole rebuild exists to
    // create, and sends the sub a full-range signal.
    if (channels == 1) {
      // The knee above bounded sl and sr PER CHANNEL, so their centred sum can
      // still reach ~1.34 — the no-clip-by-construction guarantee has to be
      // re-applied to the one sample this endpoint actually outputs.
      float m = (sl + sr) * 0.7071f;
      float a = m < 0 ? -m : m;
      if (a > 0.90f) {
        float over = a - 0.90f;
        a = 0.90f + over / (1.0f + over * 10.0f);
        m = m < 0 ? -a : a;
      }
      out[f] = m;
    } else {
      for (int c = 0; c < channels; c++)
        out[f * (uint32_t)channels + (uint32_t)c] =
            c == 0 ? sl : (c == 1 ? sr : 0.0f);
    }
  }
}

