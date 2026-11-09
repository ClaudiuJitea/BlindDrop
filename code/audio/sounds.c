// code/audio/sounds.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// Every sound is built the same way: a sub-millisecond TRANSIENT, then a
// CRACK (the band that carries the "rifle"), then a BODY, then a SUB, then
// mechanical detail, then early REFLECTIONS, then a decorrelated TAIL. The old
// shots had the body and the sub and almost nothing else: measured, 94% of the
// AR's energy sat below 250 Hz and only 2.9% above 2 kHz, which is a thud, not
// a report. The band split is the design, and `sfx` prints it back.
// ---------------------------------------------------------------------------
// --- THE PHYSICS OF A SHOT ----------------------------------------------
// Everything above is filtered noise, and filtered noise is what a gunshot sounds
// like when it is described rather than fired. These are the parts that are not
// noise: a shock front, the round's own sonic boom, gas tearing out of the muzzle,
// steel hitting steel, and a microphone that cannot take it.

// The muzzle blast: a Friedlander pressure wave, the standard shape of a free-air
// shock. A vertical step, a decay through zero in T seconds, then the suction
// behind it. The step is what no band-passed burst can do: it has energy at every
// frequency at once, in phase, and that coherence is what reads as a detonation.
static int sfx_friedlander(float T, float amp) {
  int n = (int)(T * 6.0f * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  op_t rough = {0}, dc = {0};
  for (int i = 0; i < n; i++) {
    float t = (float)i / SFX_HZ, u = t / T;
    float grit = op_lp(&rough, rng_rangef(&g_rng_sfx, -1, 1), 7000.0f);
    g_scr[i] = op_hp(&dc, (1.0f - u) * expf(-1.7f * u) * (1.0f + grit * 0.9f), 90.0f);
  }
  return scr_norm(n, amp);
}

// The round's own boom: a supersonic bullet drags an N-wave, up, straight down
// through zero and back, over a fraction of a millisecond. Almost all of it is
// above 2 kHz. A pistol round at the speed of sound does not make one.
static int sfx_nwave(float T, float amp) {
  int n = (int)(T * SFX_HZ) + 2;
  for (int i = 0; i < n; i++) g_scr[i] = 1.0f - 2.0f * (float)i / (float)(n - 1);
  return scr_norm(n, amp);
}

// The sizzle on top of a report: two high-passes in series, because this is the one
// band svf() cannot reach. It is capped near 7.4 kHz for stability, and that cap
// was why every shot in this file measured 1-2% of its energy above 6 kHz.
static int sfx_air(float sec, float hz, float dec, float amp) {
  int n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  op_t a = {0}, b = {0};
  for (int i = 0; i < n; i++) {
    float t = (float)i / SFX_HZ;
    float x = op_hp(&b, op_hp(&a, rng_rangef(&g_rng_sfx, -1, 1), hz), hz * 1.3f);
    g_scr[i] = x * expf(-t / dec);
  }
  return scr_norm(n, amp);
}

// Propellant gas: a band of noise whose level is itself noise, so it TEARS instead
// of hissing. This is the rough edge on a real report, and the thing a smooth
// exponential envelope can never have.
static int sfx_gas(float sec, float hz, float q, float dec, float rough_hz, float amp) {
  int n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  svf_t f = {0, 0};
  op_t m = {0};
  float mg = 0.0f;
  for (int i = 0; i < n; i++) g_scr[i] = rng_rangef(&g_rng_sfx, -1, 1);
  for (int i = 0; i < n; i++) {
    float t = (float)i / SFX_HZ;
    float am = op_lp(&m, rng_rangef(&g_rng_sfx, -1, 1), rough_hz);
    mg = fmaxf(mg, fabsf(am));
    g_scr[i] = svf(&f, g_scr[i], hz, q) * expf(-t / dec) * am;
  }
  for (int i = 0; i < n; i++) g_scr[i] = g_scr[i] / (mg + 1e-6f);
  return scr_norm(n, amp);
}

// Struck steel: five partials in the ratios of a bar rather than a string, the high
// ones dying first. Each take strikes it a little differently.
static int sfx_modal(float sec, float hz, float dec, float amp) {
  static const float R_[5] = {1.0f, 1.59f, 2.41f, 3.37f, 4.53f};
  int n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  memset(g_scr, 0, (size_t)n * sizeof g_scr[0]);
  for (int k = 0; k < 5; k++) {
    float f = hz * R_[k] * rng_rangef(&g_rng_sfx, 0.985f, 1.015f);
    float d = dec / (1.0f + 0.55f * (float)k), g = rng_rangef(&g_rng_sfx, 0.6f, 1.0f) / (1.0f + 0.4f * (float)k);
    if (f > SFX_HZ * 0.45f) continue;
    for (int i = 0; i < n; i++) {
      float t = (float)i / SFX_HZ;
      g_scr[i] += sinf(F_TAU * f * t) * expf(-t / d) * g;
    }
  }
  return scr_norm(n, amp);
}

static void sfx_strike(int id, float at_s, float hz, float dec, float amp, float pan) {
  int n = sfx_click(0.002f, 0.00030f, 2600.0f, amp * 0.8f);
  sfx_lay(id, n, (int)(at_s * SFX_HZ), pan, 0);
  n = sfx_modal(dec * 6.0f, hz, dec, amp);
  sfx_lay(id, n, (int)(at_s * SFX_HZ), pan, 1);
}

// A spent case landing somewhere near your boots: brass, not steel, so high and
// short, and it skips.
static void sfx_brass(int id, float at_s, float amp) {
  float pan = rng_rangef(&g_rng_sfx, 0.18f, 0.42f), gap = rng_rangef(&g_rng_sfx, 0.07f, 0.11f);
  for (int k = 0; k < 3; k++, amp *= 0.5f, gap *= 0.62f) {
    int n = sfx_modal(0.04f, rng_rangef(&g_rng_sfx, 4300.0f, 5600.0f), 0.0065f, amp);
    sfx_lay(id, n, (int)(at_s * SFX_HZ), pan, 1);
    at_s += gap;
  }
}

// A microphone next to a gun is not linear and neither is an ear. Rounding the very
// top of the wave off is what turns a thin spike into a solid hit: it moves energy
// out of one sample into the milliseconds around it and adds the harmonics a
// recorded report is full of. Soft, so a quiet tail passes through untouched.
static void sfx_drive(int id, int upto, float drive) {
  float *b = g_sfx_buf[id], mx = 1e-6f;
  int n = upto * SFX_CH, len = g_sfx_len[id] * SFX_CH;
  if (n > len) n = len;
  for (int i = 0; i < n; i++) mx = fmaxf(mx, fabsf(b[i]));
  float g = drive / mx;
  for (int i = 0; i < n; i++) b[i] = tanhf(b[i] * g) / g;
}

// The first few milliseconds of any firearm. T is how long the shock takes to pass,
// which is what separates a pistol from a rifle more than level does. The second
// copy is the same shock off the ground, a couple of metres further round.
static void sfx_report(int id, float T, float blast, float boom, float air, float air_dec) {
  float gd = rng_rangef(&g_rng_sfx, 0.0026f, 0.0040f);
  int n = sfx_friedlander(T, blast);
  sfx_lay(id, n, 0, 0.0f, 0);
  n = sfx_friedlander(T * 1.2f, blast * 0.42f);
  sfx_lay(id, n, (int)(gd * SFX_HZ), 0.0f, 0);
  if (boom > 0.0f) { n = sfx_nwave(0.00028f, boom); sfx_lay(id, n, 2, 0.0f, 0); }
  n = sfx_air(0.05f, 6200.0f, air_dec, air);
  sfx_lay(id, n, 0, 0.0f, 1);
}

static void sfx_build_all(void) {
  int n;  // sfx_alloc reseeds g_rng_sfx per sound id — no global seed here

  // ---- AR: sharp, mid-forward, short room -------------------------------
  // 0.66 s, not 0.52: the room below decays over 440 ms from a 9 ms predelay,
  // and a reverb tail that runs off the end of its buffer is a step to zero —
  // i.e. a click, at the exact moment the ear is listening to the decay.
  static const int AR_TAKE[5] = {SFX_FIRE_AR, SFX_FIRE_AR2, SFX_FIRE_AR3, SFX_FIRE_AR4, SFX_FIRE_AR5};
  for (int fv = 0; fv < 5; fv++) {
    int fid = AR_TAKE[fv];
    sfx_alloc(fid, 0.66f);
    float vj = rng_rangef(&g_rng_sfx, 0.94f, 1.06f);      // no two chambers ring alike
    sfx_report(fid, 0.00125f, 2.3f, 1.5f, 1.75f, 0.0095f);
    n = sfx_click(0.004f, 0.00055f, 1800.0f, 0.90f);
    sfx_lay(fid, n, 0, 0.0f, 0);                 // transient
    n = sfx_burst(0.08f, 4300.0f * vj, 1.7f, 0.00002f, 0.011f, 1.7f, 0.88f);
    sfx_lay(fid, n, 0, 0.0f, 0);                 // snap
    n = sfx_burst(0.10f, 2500.0f / vj, 1.4f, 0.00003f, 0.022f, 1.5f, 1.30f);
    sfx_lay(fid, n, 0, 0.0f, 0);                 // crack
    n = sfx_burst(0.12f, 1150.0f * vj, 1.2f, 0.00006f, 0.034f, 1.2f, 0.95f);
    sfx_lay(fid, n, 1, -0.06f, 2);               // upper body
    n = sfx_burst(0.16f, 260.0f, 1.0f, 0.00010f, 0.036f, 1.0f, 0.40f);
    sfx_lay(fid, n, 2, 0.05f, 3);                // body
    n = sfx_sweep(0.20f, 190.0f, 62.0f, 0.026f, 0.050f, 1.0f, 0.46f);
    sfx_lay(fid, n, 0, 0.0f, 0);                 // sub punch
    n = sfx_gas(0.11f, 1700.0f * vj, 0.8f, 0.022f, 900.0f, 1.05f);
    sfx_lay(fid, n, 1, 0.0f, 2);                         // gas tearing out of the muzzle
    // The action, on its real clock: a 700 rpm bolt goes back, stops in the buffer,
    // comes home. Then the buffer spring, ringing on behind it: that thin twang is
    // the thing about this kind of rifle everybody knows and nobody can name.
    sfx_clack(fid, (int)(0.006f * SFX_HZ), 3400.0f, 0.20f);
    sfx_strike(fid, 0.012f * vj, 1850.0f, 0.0055f, 0.17f, 0.06f);   // carrier back
    sfx_strike(fid, 0.031f / vj, 980.0f, 0.0080f, 0.15f, 0.04f);    // into the buffer
    sfx_strike(fid, 0.073f * vj, 2300.0f, 0.0065f, 0.17f, 0.06f);   // home
    n = sfx_modal(0.16f, 2150.0f * vj, 0.040f, 0.075f);
    sfx_lay(fid, n, (int)(0.020f * SFX_HZ), 0.08f, 2);   // the spring
    (void)sfx_brass;   // the case is a real particle now: see SFX_BRASS
    sfx_drive(fid, (int)(0.11f * SFX_HZ), 2.6f);
    {   // nearby cover: 1.9 / 3.6 / 5.8 m, there and back
      static const float D[3] = {0.011f, 0.021f, 0.034f};
      static const float G[3] = {0.30f, 0.20f, 0.13f};
      sfx_reflect(fid, (int)(0.09f * SFX_HZ), D, G, 3, 3200.0f);
    }
    {   // ...and the arena. Both weapons are fired inside the same 48 m box, so
        // both get the same wall returns — the AR's are just quieter and darker
        // under a shot that is itself over in 80 ms.
      static const float D[2] = {0.140f, 0.198f};
      static const float G[2] = {0.14f, 0.09f};
      sfx_reflect(fid, (int)(0.07f * SFX_HZ), D, G, 2, 900.0f);
    }
    // One real room instead of two noise tails. The old pair measured T60 338 ms
    // in the low band against 407 ms in the high band — a "room" that got
    // BRIGHTER as it died, which no physical space does. Short, mid-forward,
    // and it now decays 250 ms/1.5 kHz-damped the way a concrete bay does.
    sfx_verb(fid, (int)(0.24f * SFX_HZ), 0.009f, 0.027f, 0.44f, 2600.0f,
             0.30f);
    sfx_tail(fid, (int)(0.006f * SFX_HZ), 0.22f, 2600.0f, 0.9f,
             0.005f, 0.055f, 0.13f);   // air, on top of the walls
    sfx_finish(fid, 0.97f, 0.1748f, 0.30f);
  }

  // ---- Sniper: a pressure wave, and then the whole arena answering --------
  // "Mightier" and "more reverberant" are two different jobs solved in two
  // different places, and the old shot did neither: it was a slightly lower AR
  // with a longer noise tail bolted on.
  //   POWER is the first 60 ms. A big rifle is a pressure STEP, not a louder
  //   crack, so the transient is longer and its high-pass corner drops from
  //   1200 to 850 Hz (a click filtered at 1.2 kHz has no chest in it), the
  //   crack moves down out of the AR's band, and the sub sweep reaches 45 Hz.
  //   (45, not the 33 that "bigger" suggests — see the trap below.)
  //   REVERBERATION is everything after 120 ms, and it is not a longer burst of
  //   noise. It is THIS arena: ARENA_HALF is 24 m, so a shot fired in it comes
  //   back off the side walls after 2*24/343 = 140 ms, off a corner at 34 m
  //   after 198 ms and off the far wall at 42 m after 245 ms. Those three
  //   delays are the difference between a loud gun and a gun fired indoors.
  // A trap on the way here, and it is the reason the sub sweep stops at 45 Hz
  // rather than at the 33 Hz that "bigger" suggests: `amp` normalises a layer's
  // PEAK, and a sine carries about ten times the energy of a band-passed noise
  // burst at the same peak. Pushing the sweep to 33 Hz at amp 0.85 put 67% of
  // the shot's energy under 120 Hz, dropped the crack band to 1%, and made the
  // "mightier" rifle measure 3 dB QUIETER than the AR on the A-weighted scale —
  // headphones roll off below 50 Hz and the ear is 25 dB down there, so that
  // energy is spent buying nothing while eating the limiter's whole headroom.
  // Power is 60-150 Hz plus a transient, and the sniper's authority over the AR
  // is that it owns the 600 Hz chest band the AR cannot reach.
  static const int SR_TAKE[3] = {SFX_FIRE_SR, SFX_FIRE_SR2, SFX_FIRE_SR3};
  for (int fv = 0; fv < 3; fv++) {
    int fid = SR_TAKE[fv];
  sfx_alloc(fid, 2.45f);
  float vj = rng_rangef(&g_rng_sfx, 0.95f, 1.05f);
  sfx_report(fid, 0.00210f, 5.6f, 4.2f, 4.2f, 0.0140f);
  n = sfx_gas(0.20f, 1250.0f * vj, 0.75f, 0.042f, 520.0f, 2.9f);
  sfx_lay(fid, n, 1, 0.0f, 3);                         // a big case's worth of gas
  n = sfx_click(0.007f, 0.00090f, 1150.0f, 1.70f);
  sfx_lay(fid, n, 0, 0.0f, 0);                 // transient
  // A ONE-SHOT WEAPON HAS TO BE FRIGHTENING, and fright lives in the crack, not
  // in the bottom octave. Measured over the first 80 ms the shot was
  // 20/22/49/9% across sub/low/mid/hi with a 878 Hz centroid — a big, round,
  // slightly muffled boom. What makes a heavy rifle read as dangerous, and what
  // makes it LOCATABLE, is the same thing: a hard point-source transient with a
  // lot of energy above 2 kHz, because that is the band the ear localises with
  // and the band that survives being scary. Every crack layer up, the pressure
  // sweep down; the sub is what you feel, the crack is what you fear.
  n = sfx_burst(0.055f, 5600.0f, 1.9f, 0.00002f, 0.0085f, 1.9f, 4.90f);
  sfx_lay(fid, n, 0, 0.0f, 0);                 // whip
  n = sfx_burst(0.105f, 3600.0f, 1.7f, 0.00002f, 0.019f, 1.8f, 4.40f);
  sfx_lay(fid, n, 0, 0.0f, 0);                 // snap
  // 2500, NOT 1750, AND THIS IS THE ONE THAT MADE THE SNIPER BLUNT. Measured on
  // the bus over the first 80 ms, the sniper beat the AR by +4.7 dB at
  // 500 Hz-2 kHz and by only +0.8 dB at 2-6 kHz — the band this file's own rule
  // calls fright and localisation. The cause is visible in the layer table: the
  // SR's only long-decay layer had been deliberately moved DOWN out of 2-6 kHz,
  // so its 2-6 kHz band fell 7.9 dB in the first 25 ms against its own
  // 500 Hz-2 kHz falling 1.6 — 6.3 dB of self-darkening per 25 ms, against the
  // AR's 2.4. Back at 2500 with a longer decay the band goes 54.5 -> 58.1 dB,
  // i.e. from level with the AR to +3.6 dB over it, while 500-2k drops only
  // 0.8 dB and 60-150 only 0.3. This does NOT collide with "the sniper owns the
  // 600 Hz chest band the AR cannot reach" — that is the separate 620 Hz burst
  // below, untouched, and 500-2k stays 3 dB over the AR. `sec` stays 0.17 so the
  // burst draws the same number of RNG samples and every layer built after it is
  // byte-identical.
  n = sfx_burst(0.17f, 2500.0f, 1.35f, 0.00004f, 0.048f, 1.4f, 3.20f);
  sfx_lay(fid, n, 0, 0.0f, 0);                 // crack
  // THE SNARL. A big rifle does not just crack, it TEARS: the report falls in
  // pitch over the first tenth of a second as the muzzle blast expands, and
  // that falling edge is the single most recognisable thing about a heavy
  // bolt gun. A noise burst cannot do it — it has no pitch to fall — so this is
  // a swept tone, laid 3 ms behind the transient so its slow excursion never
  // sums into the transient's peak and forces the limiter down on both.
  // 1.20, not 0.46: AT 0.46 THE SNARL DOES NOT EXIST IN THE OUTPUT. Deleting the
  // whole layer costs 0.28 dB A-weighted over the first 80 ms and 1.04 dB in its
  // own 700-2000 Hz band, and a tone-in-noise measurement in the ERB around its
  // own instantaneous frequency puts it at -1.1 to +5.6 dB over the noise
  // masking it — at or under threshold. It is buried by the 1750 Hz crack above
  // it and by the 620 Hz chest burst it falls into. At 1.20 the same measurement
  // reads +5.6 to +13.6 dB, i.e. a falling pitch you can actually hear.
  // The standing rule that a sine carries 10-15 dB more energy than noise at
  // the same peak is not a reason to leave it: the buffer's raw pre-finish
  // peak is 7.61 at amp 0.46 and 7.61 at amp 2.60 — the click and the whip set the peak and the snarl never
  // touches it — so this costs gain staging, not headroom, and the shot's own
  // 0-80 ms A-weighted level goes UP 0.5 dB.
  n = sfx_sweep(0.125f, 2150.0f, 560.0f, 0.028f, 0.066f, 1.25f, 1.20f);
  sfx_lay(fid, n, (int)(0.003f * SFX_HZ), 0.0f, 0);
  n = sfx_burst(0.26f, 620.0f, 1.1f, 0.00008f, 0.062f, 1.1f, 1.85f);
  sfx_lay(fid, n, 1, -0.07f, 3);               // chest
  n = sfx_burst(0.34f, 150.0f, 1.0f, 0.00012f, 0.090f, 1.0f, 0.95f);
  sfx_lay(fid, n, 2, 0.06f, 4);                // body
  // Bottoming out at 45 Hz bought nothing — headphones roll off below 50 and
  // the ear is 25 dB down there — while eating the limiter headroom the crack
  // now needs. Shorter, shallower, and 0.42 instead of 0.50.
  // The sweep spread its excursion over 400 ms — twice the length of the shot —
  // so at 80 ms it was still near 130 Hz, i.e. most of the pitch drop happened
  // after the ear had already decided how big the gun was. Measured over the
  // first 80 ms the "mightier" rifle was 5.6 dB DOWN on the AR at 20-120 Hz and
  // 4.1 dB down at 120-300, in the exact window the comment above calls POWER.
  // Shorter, higher, louder, and laid 8 ms behind the transient so the two peaks
  // never sum into the limiter.
  // 1.05, not 0.88, and this is the OTHER half of the snarl's gain cost. Raising
  // the snarl re-stages sfx_finish, and measured on the bus that took the
  // sniper's 60-150 Hz advantage over the AR from +1.1 dB to -0.1 — i.e. the
  // sharpening was being paid for out of the band the brief calls MENACING. This
  // layer IS that band (removing it costs 13.8 dB there and nothing anywhere
  // else), it is a tone so it draws no RNG, and its excursion is slow enough at
  // +8 ms that it never sums into the transient's peak.
  n = sfx_sweep(0.26f, 165.0f, 62.0f, 0.030f, 0.062f, 1.0f, 1.05f);
  sfx_lay(fid, n, (int)(0.008f * SFX_HZ), 0.0f, 0);  // the pressure
  sfx_drive(fid, (int)(0.15f * SFX_HZ), 2.3f);
  {   // nearby cover: 2.7 / 5 / 8 / 12 m
    static const float D[4] = {0.016f, 0.029f, 0.047f, 0.071f};
    static const float G[4] = {0.34f, 0.26f, 0.19f, 0.13f};
    sfx_reflect(fid, (int)(0.13f * SFX_HZ), D, G, 4, 2200.0f);
  }
  {   // the arena: side walls, a corner, the far wall, and one second-order
      // return. Low-passed hard — 40 m of air and two bounces of concrete leave
      // little above a kilohertz, which is why a wall return is a BOOM and not
      // a second gunshot.
    // MEASURED, and it was a second gunshot rather than a room: on a 25 ms
    // envelope the sniper decayed for 100 ms and then jumped +8.4 dB, ending
    // only 3.3 dB below its own opening window, while the AR in the same 48 m
    // box decayed monotonically. A return arriving above the direct sound's
    // current level is heard as another shot — and it threw the localisation
    // away 125 ms in, collapsing L/R correlation from +0.95 at 75 ms to +0.01
    // at 175. It was also eating the gain budget: 58% of the sniper's 300 ms
    // `sfx_finish` window was reflections against the AR's 7.7%, so the shot
    // itself was being normalised down to make room for its own echoes.
    // RAISED with the FDN's size_s, and only with it. At the shipped 0.094 the
    // reverb's own first burst landed on top of these, and measured against the
    // 30 ms preceding each tap two of the three wall returns were at or BELOW
    // what came before them (+1.0 / +0.0 / -2.7 dB) — they contributed 0.14 to
    // 0.46 dB and were the layer with the longest comment in the file. With the
    // room shrunk to the arena's actual modal spacing they have air to arrive
    // in, and these gains put the taps back at -0.4 / -1.6 / -0.4 dB of contrast
    // while holding the hall's 500 ms level at -17.9 dB against the old -16.5.
    static const float D[4] = {0.140f, 0.198f, 0.245f, 0.292f};
    static const float G[4] = {0.38f, 0.29f, 0.21f, 0.15f};
    sfx_reflect(fid, (int)(0.16f * SFX_HZ), D, G, 4, 1200.0f);
  }
  sfx_clack(fid, (int)(0.34f * SFX_HZ), 1000.0f, 0.42f);   // bolt back
  sfx_clack(fid, (int)(0.50f * SFX_HZ), 1700.0f, 0.36f);   // bolt home
  // The hall. Fed 560 ms of direct sound, which deliberately INCLUDES the wall
  // returns and the bolt cycle: a real room is re-excited by its own echoes and
  // by everything else that happens in it, and that late re-excitement is what
  // stops the decay sounding like a fader being pulled down. 1.35 s of RT60
  // damped at 2.2 kHz over an 82 ms mean reflection interval — a big concrete
  // box, which is what the arena is.
  // A bigger, longer, darker box. 1.35 s of RT60 left the last 900 ms of the
  // buffer at -54 dB, i.e. inaudible, so the "hall" the sniper is supposed to
  // have was a number in a comment rather than something the player hears.
  //
  // WET AND PREDELAY DECIDE WHETHER IT IS A ROOM OR A SECOND GUNSHOT. At 0.52
  // wet behind a 30 ms predelay the 25 ms envelope decayed for 100 ms and then
  // JUMPED +8.4 dB, ending only 3.3 dB below its own opening window — a return
  // arriving above the direct sound's current level is heard as another shot,
  // and it threw the localisation away 125 ms in (L/R correlation +0.95 at
  // 75 ms to +0.01 at 175). At 0.33 behind 14 ms the tail simply continues the
  // decay and then holds: the sniper sits at -14 dB where the AR has fallen to
  // -35, which is what a stadium sounds like. At 1.95 s of RT60 the decay is
  // still there when the next thing happens,
  // which is what makes a one-shot rifle sound like it was fired somewhere
  // rather than at you. The buffer went 1.90 -> 2.45 s with it: a reverb tail
  // that runs off the end of its buffer is a step to zero, i.e. a click, at
  // exactly the moment the ear is listening to the decay.
  // SIZE_S 0.055, NOT 0.094, AND THE PARAGRAPH ABOVE WAS MEASURING THE WRONG
  // THING. Everything it says about wet and predelay is true and stays; what it
  // missed is that the FDN's own mean reflection interval is set by size_s, and
  // at 0.094 that is 94 ms = 32 m of travel — longer than the arena's 24 m
  // half-width — so the reverb's first burst arrives at 108-199 ms, AFTER the
  // direct sound has decayed rather than filling in under it. Measured on the
  // shipped buffer, the 25 ms envelope decays for 100 ms and then REVERSES:
  // +2.3 dB broadband, +5.9 dB at 2-6 kHz and +11.1 dB above 6 kHz. Layer
  // ablation identifies it exactly: with sfx_verb removed the decay is monotone
  // and removing either reflection set leaves the rebound intact. That is the
  // file's own "a reflection above the direct sound's CURRENT level is a second
  // gunshot" rule, still being broken by the very call that claims to have fixed
  // it. At 0.055 — which is the mean free path of a 48 m court, 35-55 ms — the
  // worst 25 ms rise inside the first 400 ms falls from +3.45 dB to +1.43 and
  // the trough-to-peak rebound from +5.8 dB to +1.6. Not monotone, but no longer
  // a second gunshot. The perceived SIZE is carried by rt60 (1.95 s,
  // untouched) and by the wall taps above, not by size_s. It also buys the
  // sniper's kill confirmation 2 dB of clear air at its own 130 ms offset,
  // which had been landing squarely on this rebound.
  sfx_verb(fid, (int)(0.62f * SFX_HZ), 0.014f, 0.055f, 1.95f, 2000.0f,
           0.33f);
  sfx_finish(fid, 0.99f, 0.2680f, 0.30f);
  }

  // ---- The same two shots heard from across the arena --------------------
  // Distance is not a volume knob. Air eats the high end, the direct crack
  // arrives dry and thin, and everything else you hear is the town: a long,
  // dark, wide tail. Baking a second variant and crossfading by range costs
  // one buffer and is the difference between "quieter" and "further away".
  for (int fr = 0; fr < 2; fr++) {
    int id = fr ? SFX_FAR_SR : SFX_FAR_AR;
    sfx_alloc(id, fr ? 1.30f : 0.85f);
    // No sharp click: at 40 m the wavefront has been smeared by the air and by
    // everything it reflected off on the way. Leaving one in made the peak 40x
    // the RMS, and the limiter then pulled the whole sound down to -33 dB —
    // "distant" came out as "inaudible" rather than as "far away".
    n = sfx_burst(0.05f, 1100.0f, 0.8f, 0.0010f, 0.010f, 1.1f, 0.40f);
    sfx_lay(id, n, 0, 0.0f, 0);
    n = sfx_burst(0.10f, fr ? 900.0f : 1250.0f, 1.1f, 0.00008f,
                  fr ? 0.020f : 0.014f, 1.3f, 0.55f);
    sfx_lay(id, n, 0, 0.0f, 0);                        // thinned crack
    n = sfx_burst(0.26f, fr ? 150.0f : 210.0f, 0.9f, 0.0004f,
                  fr ? 0.075f : 0.055f, 1.0f, 0.52f);
    sfx_lay(id, n, 2, 0.0f, 2);                        // the "thump"
    {
      static const float D[4] = {0.024f, 0.043f, 0.068f, 0.099f};
      static const float G[4] = {0.40f, 0.31f, 0.23f, 0.16f};
      sfx_reflect(id, (int)(0.12f * SFX_HZ), D, G, 4, 1400.0f);
    }
    // At 30 m what reaches you is the arena, not the muzzle: the tail has to
    // be the loudest layer, and being generated per channel it is also the only
    // decorrelated one. With the direct layers dominating, the far bakes
    // measured L/R 0.97 — as narrow as a mono shot fired at your ear.
    sfx_tail(id, (int)(0.018f * SFX_HZ), fr ? 1.10f : 0.72f,
             fr ? 320.0f : 380.0f, 0.7f, 0.030f, fr ? 0.290f : 0.185f, 0.62f);
    sfx_tail(id, (int)(0.026f * SFX_HZ), fr ? 0.80f : 0.55f,
             fr ? 900.0f : 1150.0f, 0.8f, 0.040f, fr ? 0.150f : 0.105f, 0.55f);
    sfx_finish(id, 0.92f, 0.0876f, 0.0f);
  }

  // ---- The four confirmations --------------------------------------------
  // Two axes, three recipes, four buffers. See THE CONFIRMATION MATRIX above
  // `sfx_head_sig` for what each piece is and why it is shaped that way.
  //
  // The ROOM belongs to the consequence axis and is therefore identical within
  // each pair: both hits get the same 130 ms box, both kills the same 420 ms
  // room. An earlier version gave the head kill a bigger, brighter, longer room
  // than the body kill — the same mistake as the brighter razor and the
  // brighter crunch, head-ness re-invented in a term that has nothing to do
  // with precision, and three more numbers to keep in sync for it.
  //
  // The four `target_rms` figures ARE the ladder, and gain-staging them on an
  // A-weighted ruler is what makes it affordable: measured over the first
  // 300 ms against the AR's own report at -17.4 dBA, the set has to climb
  // hit < headshot < kill < headshot kill with every rung ABOVE the gun that
  // caused it. `sfx` prints all four back; they must stay monotone.

  sfx_alloc(SFX_HIT, 0.30f);
  sfx_hit_base(SFX_HIT, 1.00f);
  sfx_room_hit(SFX_HIT);
  sfx_finish(SFX_HIT, 0.94f, 0.4200f, 0.30f);

  // The two signature gains differ, and the reason is not taste. `g` scales the
  // signature against WHATEVER BASE IT IS RIDING ON, and the kill base is a
  // much bigger sound than the hit base — same ding at the same number would
  // read as a sizzle on the hit (measured: 63% of the buffer's energy above
  // 2 kHz) and as almost nothing on the kill (centroid moved 451 -> 1083 Hz and
  // the level did not move at all). These two numbers put it at a comparable
  // PROMINENCE over each host. The identity of the cue — its frequencies, its
  // envelope, its internal balance — is in `sfx_head_sig` and is not scalable
  // from here, which is the point.
  sfx_alloc(SFX_HIT_HEAD, 0.30f);
  sfx_hit_base(SFX_HIT_HEAD, SFX_HEAD_DUCK);
  sfx_head_sig(SFX_HIT_HEAD, 0.80f);
  sfx_room_hit(SFX_HIT_HEAD);
  sfx_finish(SFX_HIT_HEAD, 0.95f, 0.2620f, 0.30f);

  // Four body takes and three head takes (KILL_TAKE); routing never plays the
  // same one twice running. Every take is gain-staged to the SAME target, so
  // the ladder above holds whichever one comes up.
  // 0.3750, not 0.3470: sharing the hit's transient made the kill's own onset
  // brighter and its gain stage therefore quieter, and measured the 300 ms
  // ladder step from hit_head to kill had thinned to 0.22 dB. Re-derived rather
  // than left, because that ladder IS the consequence axis.
  {
    static const int KB[4] = {SFX_KILL, SFX_KILL2, SFX_KILL3, SFX_KILL4};
    static const int KH[3] = {SFX_KILL_HEAD, SFX_KILL_HEAD2, SFX_KILL_HEAD3};
    for (int t = 0; t < 4; t++) {
      sfx_alloc(KB[t], 0.72f);
      sfx_kill_base(KB[t], 1.00f, &KILL_TAKE[t]);
      sfx_room_kill(KB[t]);
      sfx_finish(KB[t], 0.96f, 0.3750f, 0.30f);
    }
    for (int t = 0; t < 3; t++) {
      sfx_alloc(KH[t], 0.72f);
      sfx_kill_base(KH[t], SFX_HEAD_DUCK, &KILL_TAKE[t]);
      sfx_head_sig(KH[t], 1.45f);
      sfx_room_kill(KH[t]);
      sfx_finish(KH[t], 0.96f, 0.3120f, 0.30f);
    }
  }

  // ---- Your own death ----------------------------------------------------
  // Two takes: a heavier man with a lower ring, and a lighter one with a
  // higher ring and a quicker heartbeat.
  sfx_alloc(SFX_DIE1, 1.90f);
  sfx_die(SFX_DIE1, 1.00f, 3650.0f, 0.30f);
  sfx_finish(SFX_DIE1, 0.94f, 0.2400f, 0.40f);
  sfx_alloc(SFX_DIE2, 1.90f);
  sfx_die(SFX_DIE2, 1.12f, 4150.0f, 0.26f);
  sfx_finish(SFX_DIE2, 0.94f, 0.2400f, 0.40f);

  // ---- Taking a round ----------------------------------------------------
  // Deliberately dark and dull: it must never be mistaken for your own
  // hitmarker, which is the mistake the old build made by replaying the hit
  // tick at 0.62x rate. No energy above 1.2 kHz, and it thumps rather than
  // ticks — the information is "you are being hit", delivered in the chest.
  sfx_alloc(SFX_HURT, 0.30f);
  n = sfx_click(0.004f, 0.00060f, 300.0f, 0.60f);
  sfx_lay(SFX_HURT, n, 0, 0.0f, 0);
  n = sfx_burst(0.16f, 380.0f, 0.9f, 0.00020f, 0.055f, 1.2f, 1.05f);
  sfx_lay(SFX_HURT, n, 0, 0.0f, 3);
  n = sfx_burst(0.12f, 900.0f, 1.1f, 0.00008f, 0.028f, 1.3f, 0.55f);
  sfx_lay(SFX_HURT, n, 0, 0.0f, 2);   // enough edge to survive a laptop
  n = sfx_sweep(0.26f, 145.0f, 52.0f, 0.030f, 0.075f, 1.0f, 0.62f);
  sfx_lay(SFX_HURT, n, 0, 0.0f, 0);
  sfx_tail(SFX_HURT, (int)(0.006f * SFX_HZ), 0.26f, 260.0f, 0.7f,
           0.012f, 0.075f, 0.30f);
  sfx_finish(SFX_HURT, 0.90f, 0.0663f, 0.30f);

  // ---- A round arriving in the world -------------------------------------
  sfx_alloc(SFX_IMPACT, 0.26f);
  n = sfx_click(0.003f, 0.00035f, 2800.0f, 0.80f);
  sfx_lay(SFX_IMPACT, n, 0, 0.0f, 0);
  n = sfx_burst(0.09f, 3100.0f, 1.3f, 0.00004f, 0.014f, 1.6f, 0.70f);
  sfx_lay(SFX_IMPACT, n, 0, 0.0f, 1);                  // stone chip
  n = sfx_burst(0.14f, 520.0f, 1.0f, 0.00012f, 0.038f, 1.2f, 0.60f);
  sfx_lay(SFX_IMPACT, n, 1, 0.0f, 2);
  n = sfx_burst(0.20f, 130.0f, 0.9f, 0.00030f, 0.055f, 1.0f, 0.45f);
  sfx_lay(SFX_IMPACT, n, 2, 0.0f, 3);                  // dust thump
  sfx_tail(SFX_IMPACT, (int)(0.005f * SFX_HZ), 0.22f, 900.0f, 0.8f,
           0.008f, 0.060f, 0.24f);
  sfx_finish(SFX_IMPACT, 0.85f, 0.0940f, 0.30f);

  // ---- ...and a round arriving in a MAN -----------------------------------
  // A ROUND LANDING IN A BODY WAS SILENT IN THE WORLD WHILE A ROUND LANDING IN A
  // WALL WAS NOT. Measured, two identical scripts at the same range: aimed at a
  // bot, sfxlog routes the shot and the player's own screen-space confirmation
  // and nothing else; aimed at a wall it routes the shot and `impact` at gain
  // 0.49. So a bot shooting another bot five metres in front of you produced a
  // muzzle report and complete silence at the target, and every bot-on-bot hit
  // in a 20-bot arena did the same. This is the AUDIO half of the fix this file
  // already made on the visual side — "a round landing on a MAN sparks too", a
  // distinct spark kind, for every shooter.
  //
  // It is deliberately NOT a gore sound and deliberately not the confirmation:
  // the LEAD tick is screen space and belongs to the player, this is world space
  // and belongs to everyone. Where it lives is the file's own rule — the fat is
  // 200-800 Hz — with a short slap on top so it survives being under gunfire,
  // and a thump at +8 ms so its excursion never sums into the slap's peak.
  sfx_alloc(SFX_FLESH, 0.22f);
  n = sfx_click(0.0025f, 0.00030f, 1400.0f, 1.10f);
  sfx_lay(SFX_FLESH, n, 0, 0.0f, 0);                   // the arrival
  n = sfx_burst(0.075f, 1900.0f, 1.2f, 0.00004f, 0.016f, 1.4f, 2.20f);
  sfx_lay(SFX_FLESH, n, 0, 0.0f, 1);                   // the slap
  n = sfx_burst(0.13f, 620.0f, 1.0f, 0.00010f, 0.034f, 1.2f, 1.55f);
  sfx_lay(SFX_FLESH, n, (int)(0.002f * SFX_HZ), 0.0f, 2);  // the meat
  n = sfx_burst(0.17f, 190.0f, 0.9f, 0.00025f, 0.050f, 1.0f, 0.80f);
  sfx_lay(SFX_FLESH, n, (int)(0.008f * SFX_HZ), 0.0f, 3);  // the thump
  // Per-event reflections, in the reload's idiom rather than a global verb: what
  // comes back off a body hit at 8 m is the nearest wall, not the whole arena.
  {
    static const float D[2] = {0.032f, 0.086f};
    static const float G[2] = {0.20f, 0.11f};
    sfx_reflect(SFX_FLESH, (int)(0.07f * SFX_HZ), D, G, 2, 1100.0f);
  }
  sfx_finish(SFX_FLESH, 0.86f, 0.0700f, 0.30f);

  // ---- One going past your head -------------------------------------------
  // A supersonic round's bow wave: a very short, very bright band that sweeps
  // DOWN as it passes and pans across. Nothing else in the mix says "that was
  // close" as economically.
  // Built CENTRED. The first version baked a left-to-right pan sweep into the
  // buffer, so every round in the game passed you from the left — the one cue
  // whose entire job is telling you which side it came from. The side belongs
  // at the call site, where the geometry is known; the buffer just gets two
  // independent noise streams so the bow wave has width.
  // THE CRACK HAD NO CRACK. Measured, this buffer was 0% below 500 Hz and 90%
  // above 2 kHz with a centroid of 4735 Hz and an attack of 1.7 ms — 5.7x
  // BLUNTER than the AR report it is supposed to precede, and its own hi band
  // sat at parity with the hi band of a 20-bot firefight bus, i.e. masked to a
  // wash. A supersonic bow wave is an N-WAVE: a near-instant pressure step with
  // real energy well below a kilohertz, then the hiss of the round's wake. So:
  // a razor at frame 0 for the step, a 640 Hz body a few milliseconds behind it
  // for the part you FEEL (this file's own 'fat lives at 200-800 Hz' rule, never
  // applied here), and the existing sweep shortened so it reads as a PASS rather
  // than as a hiss. The two independent noise streams stay — LRcorr +0.01 is
  // what gives the cue its width, and it is the one number here that was right.
  sfx_alloc(SFX_WHIZZ, 0.30f);
  n = sfx_click(0.0025f, 0.00022f, 3600.0f, 3.40f);
  sfx_lay(SFX_WHIZZ, n, 0, 0.0f, 0);                   // the pressure step
  // KNOWN EXCEPTION to "every generator ends in scr_norm": this wake sweep
  // writes filtered noise straight into the buffer at a hard 3.0, so its level
  // rides the filter's response instead of a normalised peak. It is per-channel
  // decorrelated (that is the point of writing it inline), the buffer is
  // peak-normalised by sfx_finish anyway, and the shipped balance was measured
  // on the bus — re-plumbing it through scr_norm would re-roll a tuned cue for
  // consistency's sake alone.
  for (int c = 0; c < 2; c++) {
    int m = (int)(0.16f * SFX_HZ);
    svf_t f1 = {0, 0};
    float *b = g_sfx_buf[SFX_WHIZZ];
    for (int i = 0; i < m; i++) {
      float t = (float)i / SFX_HZ;
      float hz = 5200.0f * expf(-t / 0.018f) + 700.0f;
      float x = svf(&f1, rng_rangef(&g_rng_sfx, -1, 1), hz, 2.2f);
      b[i * SFX_CH + c] += x * sfx_env(t, 0.0006f, 0.024f, 1.3f) * 3.0f;
    }
  }
  // 1.50, and the number is a sweep rather than the 1.7-2.3 rule of thumb this
  // file quotes for a mid burst that has to be heard UNDER something else. This
  // one is not under anything — it is the second loudest layer in a 300 ms
  // buffer whose whole job is to be startling — and the sweep says what it costs:
  // 1.00 gives 44/54 mid/hi at a 3046 Hz centroid, 2.10 gives 63/32 at 2121, and
  // an N-wave wants BOTH the crack and the body. 1.50 lands at 54/43 and 2583 Hz
  // with the attack still at 0.0 ms. Laid at +5 ms so its excursion never sums
  // into the razor's peak, which is the rule every low layer in this file obeys.
  n = sfx_burst(0.055f, 640.0f, 1.2f, 0.00004f, 0.014f, 1.3f, 1.50f);
  sfx_lay(SFX_WHIZZ, n, (int)(0.005f * SFX_HZ), 0.0f, 2);   // the body
  // ...AND IT HAPPENS IN THE ARENA. This was one of only three sounds in the
  // file with neither a verb nor a reflection — a supersonic crack in a 48 m
  // concrete box is the single most obviously ROOMED thing in it. Two dark taps
  // off the arena's own dimensions, hard low-passed for the same reason the
  // gunshots' wall returns are: two bounces of concrete leave little above a
  // kilohertz, so a return is a slap and not a second crack.
  {
    static const float D[2] = {0.055f, 0.140f};
    static const float G[2] = {0.14f, 0.08f};
    sfx_reflect(SFX_WHIZZ, (int)(0.09f * SFX_HZ), D, G, 2, 1800.0f);
  }
  sfx_finish(SFX_WHIZZ, 0.86f, 0.0896f, 0.30f);

  // ---- a boot landing ------------------------------------------------------
  // Short, dry and quiet. Its job is information, not drama: it is the only
  // continuous channel telling you about someone you cannot see, so it has to
  // survive being played twice a second under gunfire without becoming
  // annoying. Grit on top of a soft thump, and the routing pitches every one
  // of them differently so a run does not machine-gun.
  // THE ONE CUE WHOSE JOB IS DIRECTION HAD NOTHING IN THE BAND THE EAR
  // LOCALISES WITH. Measured, this buffer was 29/64/7/0 sub/low/mid/hi with a
  // centroid of 248 Hz — ZERO energy above 2 kHz — and an L/R correlation of
  // +0.98, i.e. no width either. The authored 2400 Hz "grit" was the reason: at
  // amp 0.55 against a 135 Hz weight layer at 0.70 it obeys the trap this file
  // documents twice (a narrow band of noise carries almost none of its energy at
  // a given PEAK, so a mid burst needs 1.7-2.3 to be heard at all), so the grit
  // was in the source and not in the output. Raised, lengthened, and given a
  // separate 5.2 kHz scuff — and the two of them get INDEPENDENT noise per
  // channel, the way SFX_WHIZZ gets its +0.01 correlation, instead of the 1-3
  // SAMPLE channel offset that was buying nothing.
  sfx_alloc(SFX_STEP, 0.16f);
  // The two bright layers are generated PER CHANNEL from independent noise,
  // not laid once and offset by a sample or two. The direction of a footstep
  // belongs at the CALL SITE (audio_place pans it), so nothing may be baked in;
  // what the buffer owes is WIDTH, and two independent realisations of the same
  // band are the only thing that actually delivers it. A 1-3 sample offset does
  // not — measured, LRcorr stayed at +0.98 — and a pan baked into the buffer is
  // the defect SFX_WHIZZ already paid for once (every round passing on the left).
  for (int c = 0; c < 2; c++) {
    float *b = g_sfx_buf[SFX_STEP];
    n = sfx_burst(0.05f, 2400.0f, 0.9f, 0.00008f, 0.016f, 1.5f, 2.20f);
    for (int i = 0; i < n; i++) b[i * SFX_CH + c] += g_scr[i];
    n = sfx_burst(0.04f, 5200.0f, 1.4f, 0.00005f, 0.006f, 1.6f, 1.40f);
    for (int i = 0; i < n; i++) b[(i + 1) * SFX_CH + c] += g_scr[i];
  }
  n = sfx_burst(0.10f, 620.0f, 0.8f, 0.00020f, 0.022f, 1.3f, 0.75f);
  sfx_lay(SFX_STEP, n, 0, 0.0f, 2);                    // sole
  n = sfx_burst(0.13f, 135.0f, 0.9f, 0.00040f, 0.034f, 1.1f, 0.62f);
  sfx_lay(SFX_STEP, n, 1, 0.0f, 3);                    // weight
  // ...and a boot lands in the arena like everything else does. One near tap
  // and one far, hard low-passed: a step is heard at ground level across a
  // concrete court, so what comes back is dark and short.
  {
    static const float D[2] = {0.022f, 0.062f};
    static const float G[2] = {0.24f, 0.13f};
    sfx_reflect(SFX_STEP, (int)(0.05f * SFX_HZ), D, G, 2, 1400.0f);
  }
  sfx_finish(SFX_STEP, 0.70f, 0.0477f, 0.30f);

  // ---- dry fire and reload -------------------------------------------------
  // A hammer falling on an empty chamber is a CLACK WITH WEIGHT. The rewrite
  // turned it into nearly pure click — the 80-250 Hz share fell from 82% to
  // 5% — which reads as a UI tick rather than as a mechanism. The low body is
  // back, and it is the loudest layer.
  sfx_alloc(SFX_DRY, 0.12f);
  sfx_clack(SFX_DRY, 0, 2600.0f, 0.55f);
  n = sfx_burst(0.05f, 4200.0f, 1.6f, 0.00003f, 0.006f, 1.5f, 0.30f);
  sfx_lay(SFX_DRY, n, 0, 0.0f, 0);
  n = sfx_burst(0.09f, 175.0f, 1.0f, 0.00012f, 0.022f, 1.2f, 0.95f);
  sfx_lay(SFX_DRY, n, 1, 0.0f, 2);
  n = sfx_ring(0.08f, 330.0f, 520.0f, 0.016f, 0.45f);
  sfx_lay(SFX_DRY, n, 1, 0.0f, 1);
  sfx_finish(SFX_DRY, 0.62f, 0.0374f, 0.30f);

  // THE PISTOL: a short bright crack with far less low end than the rifles and
  // a slide clack right behind it, so it reads as smaller at any range.
  // A short barrel dumps its gas at pressure: a much shorter shock than a rifle's,
  // a hard flat bark and no boom, because the round is barely at the speed of sound.
  // The slide is the character: back, stop, forward, each one a real strike.
  static const int PT_TAKE[4] = {SFX_FIRE_PT, SFX_FIRE_PT2, SFX_FIRE_PT3, SFX_FIRE_PT4};
  for (int fv = 0; fv < 4; fv++) {
    int fid = PT_TAKE[fv];
    sfx_alloc(fid, 0.72f);
    float vj = rng_rangef(&g_rng_sfx, 0.94f, 1.06f);
    sfx_report(fid, 0.00082f, 2.1f, 0.0f, 1.45f, 0.0075f);
    n = sfx_click(0.012f, 0.0016f, 2400.0f, 0.7f);
    sfx_lay(fid, n, 0, 0.0f, 0);
    n = sfx_burst(0.09f, 2900.0f * vj, 1.3f, 0.00003f, 0.012f, 1.5f, 0.95f);
    sfx_lay(fid, n, 0, 0.0f, 0);                       // the crack
    n = sfx_burst(0.16f, 1250.0f / vj, 0.9f, 0.00010f, 0.030f, 1.3f, 0.95f);
    sfx_lay(fid, n, 2, -0.04f, 1);
    n = sfx_burst(0.20f, 300.0f, 0.8f, 0.00020f, 0.028f, 1.2f, 0.42f);
    sfx_lay(fid, n, 4, 0.04f, 2);
    n = sfx_sweep(0.12f, 210.0f, 90.0f, 0.018f, 0.024f, 1.0f, 0.22f);
    sfx_lay(fid, n, (int)(0.003f * SFX_HZ), 0.0f, 0);  // the wrist takes it
    n = sfx_gas(0.08f, 2100.0f * vj, 0.8f, 0.014f, 1100.0f, 0.70f);
    sfx_lay(fid, n, 1, 0.0f, 2);
    sfx_strike(fid, 0.016f * vj, 2500.0f, 0.0045f, 0.14f, 0.05f);   // slide back
    sfx_strike(fid, 0.030f / vj, 1450.0f, 0.0065f, 0.15f, 0.05f);   // on the frame
    sfx_strike(fid, 0.062f * vj, 3100.0f, 0.0045f, 0.15f, 0.05f);   // back in battery
    sfx_drive(fid, (int)(0.09f * SFX_HZ), 2.4f);
    {  // the same arena every other gun is fired in
      static const float D[3] = {0.012f, 0.023f, 0.037f};
      static const float G[3] = {0.28f, 0.19f, 0.12f};
      sfx_reflect(fid, (int)(0.08f * SFX_HZ), D, G, 3, 3000.0f);
    }
    sfx_verb(fid, (int)(0.16f * SFX_HZ), 0.008f, 0.024f, 0.40f, 2800.0f, 0.26f);
    sfx_finish(fid, 0.90f, 0.128f, 0.20f);
  }

  // Its own distant report, not the rifle's played fast: shorter, higher, no weight.
  sfx_alloc(SFX_FAR_PT, 0.70f);
  n = sfx_burst(0.07f, 1500.0f, 1.0f, 0.00008f, 0.011f, 1.3f, 0.55f);
  sfx_lay(SFX_FAR_PT, n, 0, 0.0f, 0);
  n = sfx_burst(0.16f, 380.0f, 0.9f, 0.0004f, 0.036f, 1.0f, 0.42f);
  sfx_lay(SFX_FAR_PT, n, 2, 0.0f, 2);
  {
    static const float D[4] = {0.024f, 0.043f, 0.068f, 0.099f};
    static const float G[4] = {0.38f, 0.29f, 0.21f, 0.14f};
    sfx_reflect(SFX_FAR_PT, (int)(0.10f * SFX_HZ), D, G, 4, 1600.0f);
  }
  sfx_tail(SFX_FAR_PT, (int)(0.018f * SFX_HZ), 0.60f, 520.0f, 0.7f, 0.024f, 0.130f, 0.55f);
  sfx_tail(SFX_FAR_PT, (int)(0.026f * SFX_HZ), 0.45f, 1400.0f, 0.8f, 0.030f, 0.080f, 0.50f);
  sfx_finish(SFX_FAR_PT, 0.90f, 0.0700f, 0.0f);

  // What a wall throws back: no top, no bottom, and short. A hard surface returns
  // the middle of a shot, and the air on the way eats the rest.
  // A CASE ON THE GROUND, as its own buffer now: the casing is a physical
  // particle that bounces, and each bounce plays this — so the tinkle lands
  // when the brass does, not at a fixed offset baked into the shot.
  sfx_alloc(SFX_BRASS, 0.08f);
  n = sfx_modal(0.05f, 4900.0f, 0.0062f, 0.30f);
  sfx_lay(SFX_BRASS, n, 0, 0.0f, 1);
  n = sfx_modal(0.04f, 7300.0f, 0.0038f, 0.14f);
  sfx_lay(SFX_BRASS, n, 1, 0.0f, 1);
  n = sfx_click(0.002f, 0.00030f, 2600.0f, 0.20f);
  sfx_lay(SFX_BRASS, n, 0, 0.0f, 0);
  sfx_finish(SFX_BRASS, 0.42f, 0.030f, 0.0f);
  // ARMOUR: a hard, bright ping with a short ring — the confirmation that the
  // round hit, and that the plate took it.
  sfx_alloc(SFX_CLINK, 0.22f);
  n = sfx_modal(0.20f, 3150.0f, 0.045f, 0.34f);
  sfx_lay(SFX_CLINK, n, 0, 0.0f, 1);
  n = sfx_modal(0.14f, 5420.0f, 0.022f, 0.18f);
  sfx_lay(SFX_CLINK, n, 0, 0.0f, 1);
  n = sfx_click(0.003f, 0.00040f, 3800.0f, 0.36f);
  sfx_lay(SFX_CLINK, n, 0, 0.0f, 0);
  sfx_finish(SFX_CLINK, 0.60f, 0.060f, 0.0f);

  // HOSTAGE RESCUE. THE SIREN: one rise and fall of a motor siren across the
  // compound — a saturated tone gliding 420..900 Hz, heard from a distance
  // (soft top, a long decorrelated tail per ear).
  sfx_alloc(SFX_ALARM, 2.3f);
  {
    int len = (int)(2.0f * SFX_HZ);
    float ph = 0.0f, ph2 = 0.0f;
    op_t lp = {0};
    for (int i = 0; i < len && i < SFX_SCRATCH; i++) {
      float t = (float)i / SFX_HZ;
      float f = 660.0f - 240.0f * cosf(F_PI * t);        // up and back down in 2 s
      ph += F_TAU * f / SFX_HZ;
      ph2 += F_TAU * f * 2.003f / SFX_HZ;
      float x = tanhf(2.4f * sinf(ph)) + 0.25f * sinf(ph2);
      float env = f_clamp(t / 0.12f, 0.0f, 1.0f) * f_clamp((2.0f - t) / 0.35f, 0.0f, 1.0f);
      g_scr[i] = op_lp(&lp, x, 2600.0f) * env * 0.5f;
    }
    sfx_lay(SFX_ALARM, len, 0, -0.15f, 0);
    sfx_lay(SFX_ALARM, len, (int)(0.045f * SFX_HZ), 0.15f, 24);
  }
  sfx_tail(SFX_ALARM, (int)(0.05f * SFX_HZ), 0.50f, 900.0f, 0.8f, 0.040f, 0.400f, 0.35f);
  sfx_finish(SFX_ALARM, 0.70f, 0.080f, 0.0f);
  // THE CUT: two clips of a blade through a cable tie.
  sfx_alloc(SFX_SNIP, 0.30f);
  n = sfx_click(0.003f, 0.00035f, 3400.0f, 0.55f);
  sfx_lay(SFX_SNIP, n, 0, 0.0f, 0);
  n = sfx_modal(0.06f, 6100.0f, 0.008f, 0.22f);
  sfx_lay(SFX_SNIP, n, 0, 0.0f, 1);
  n = sfx_click(0.004f, 0.00045f, 2700.0f, 0.70f);
  sfx_lay(SFX_SNIP, n, (int)(0.11f * SFX_HZ), 0.0f, 0);
  n = sfx_modal(0.08f, 5200.0f, 0.010f, 0.25f);
  sfx_lay(SFX_SNIP, n, (int)(0.11f * SFX_HZ), 0.0f, 1);
  sfx_finish(SFX_SNIP, 0.70f, 0.060f, 0.0f);
  // AN OBJECTIVE: two soft bell partials, a fifth apart.
  sfx_alloc(SFX_CHIME, 1.1f);
  {
    int len = (int)(1.0f * SFX_HZ);
    for (int i = 0; i < len && i < SFX_SCRATCH; i++) {
      float t = (float)i / SFX_HZ, t2 = t - 0.14f;
      float a = sinf(F_TAU * 880.0f * t) * expf(-t / 0.30f) + 0.3f * sinf(F_TAU * 2640.0f * t) * expf(-t / 0.08f);
      float b2 = t2 > 0.0f ? sinf(F_TAU * 1318.5f * t2) * expf(-t2 / 0.38f) +
                             0.25f * sinf(F_TAU * 3955.0f * t2) * expf(-t2 / 0.09f) : 0.0f;
      g_scr[i] = (a + b2) * 0.4f * f_clamp(t / 0.004f, 0.0f, 1.0f);
    }
    sfx_lay(SFX_CHIME, len, 0, 0.0f, 12);
  }
  sfx_finish(SFX_CHIME, 0.55f, 0.050f, 0.0f);

  sfx_alloc(SFX_SLAP, 0.26f);
  n = sfx_burst(0.09f, 1500.0f, 0.8f, 0.0004f, 0.016f, 1.2f, 0.80f);
  sfx_lay(SFX_SLAP, n, 0, 0.0f, 2);
  n = sfx_burst(0.16f, 480.0f, 0.8f, 0.0008f, 0.036f, 1.1f, 0.65f);
  sfx_lay(SFX_SLAP, n, 2, 0.0f, 4);
  sfx_tail(SFX_SLAP, (int)(0.008f * SFX_HZ), 0.22f, 900.0f, 0.7f, 0.010f, 0.055f, 0.30f);
  sfx_finish(SFX_SLAP, 0.86f, 0.090f, 0.0f);

  // Reload, timed from reload_ticks like every other: drop, seat, slide.
  {
    float t_pt = (float)WPN_DEF[WPN_PT].reload_ticks / (float)TICK_HZ;
    sfx_alloc(SFX_RELOAD_PT, t_pt + 0.20f);
    sfx_clack(SFX_RELOAD_PT, (int)(0.12f * t_pt * SFX_HZ), 1500.0f, 0.42f);
    sfx_clack(SFX_RELOAD_PT, (int)(RELOAD_BEAT[WPN_PT][0] * t_pt * SFX_HZ), 900.0f, 0.70f);
    sfx_clack(SFX_RELOAD_PT, (int)(RELOAD_BEAT[WPN_PT][1] * t_pt * SFX_HZ), 2200.0f, 0.82f);
    sfx_finish(SFX_RELOAD_PT, 0.62f, 0.040f, 0.30f);
  }

  // THE KNIFE. A swing is air: filtered noise whose band rises and falls with
  // the blade speed. A wall is a dull chip, a block is two pieces of steel.
  sfx_alloc(SFX_KNIFE_SWING, 0.30f);
  n = sfx_burst(0.26f, 1500.0f, 1.2f, 0.060f, 0.070f, 1.4f, 0.75f);
  sfx_lay(SFX_KNIFE_SWING, n, 0, 0.0f, 2);
  n = sfx_burst(0.20f, 3600.0f, 1.8f, 0.050f, 0.050f, 1.4f, 0.40f);
  sfx_lay(SFX_KNIFE_SWING, n, (int)(0.02f * SFX_HZ), 0.0f, 2);
  sfx_finish(SFX_KNIFE_SWING, 0.50f, 0.045f, 0.20f);

  sfx_alloc(SFX_KNIFE_WALL, 0.22f);
  sfx_clack(SFX_KNIFE_WALL, 0, 1800.0f, 0.70f);
  n = sfx_burst(0.09f, 620.0f, 1.0f, 0.00015f, 0.022f, 1.2f, 0.70f);
  sfx_lay(SFX_KNIFE_WALL, n, 1, 0.0f, 1);
  sfx_finish(SFX_KNIFE_WALL, 0.56f, 0.050f, 0.12f);

  sfx_alloc(SFX_KNIFE_BLOCK, 0.60f);
  n = sfx_ring(0.55f, 2350.0f, 3720.0f, 0.120f, 0.75f);
  sfx_lay(SFX_KNIFE_BLOCK, n, 0, 0.0f, 1);
  n = sfx_click(0.010f, 0.0014f, 3000.0f, 0.95f);
  sfx_lay(SFX_KNIFE_BLOCK, n, 0, 0.0f, 0);
  sfx_finish(SFX_KNIFE_BLOCK, 0.68f, 0.060f, 0.14f);

  // THE GRENADE. Pin: a small metal scrape and tick. Bounce: a short dull clank.
  sfx_alloc(SFX_NADE_PIN, 0.22f);
  sfx_clack(SFX_NADE_PIN, 0, 3400.0f, 0.42f);
  n = sfx_burst(0.07f, 5200.0f, 2.2f, 0.004f, 0.018f, 1.3f, 0.34f);
  sfx_lay(SFX_NADE_PIN, n, (int)(0.012f * SFX_HZ), 0.0f, 0);
  sfx_finish(SFX_NADE_PIN, 0.46f, 0.030f, 0.12f);

  sfx_alloc(SFX_NADE_BOUNCE, 0.20f);
  sfx_clack(SFX_NADE_BOUNCE, 0, 1250.0f, 0.60f);
  n = sfx_ring(0.14f, 1700.0f, 2750.0f, 0.030f, 0.42f);
  sfx_lay(SFX_NADE_BOUNCE, n, 1, 0.0f, 0);
  sfx_finish(SFX_NADE_BOUNCE, 0.52f, 0.040f, 0.10f);

  // The spoon leaving: one bright sprung-steel ping. Equip: leather taking a load.
  sfx_alloc(SFX_NADE_SPOON, 0.34f);
  n = sfx_ring(0.30f, 4150.0f, 6300.0f, 0.070f, 0.70f);
  sfx_lay(SFX_NADE_SPOON, n, 0, 0.0f, 1);
  sfx_clack(SFX_NADE_SPOON, 0, 3800.0f, 0.34f);
  sfx_finish(SFX_NADE_SPOON, 0.50f, 0.034f, 0.12f);

  sfx_alloc(SFX_NADE_EQUIP, 0.24f);
  n = sfx_burst(0.20f, 900.0f, 0.8f, 0.018f, 0.050f, 1.3f, 0.60f);
  sfx_lay(SFX_NADE_EQUIP, n, 0, 0.0f, 2);
  n = sfx_burst(0.12f, 2600.0f, 1.2f, 0.010f, 0.030f, 1.3f, 0.26f);
  sfx_lay(SFX_NADE_EQUIP, n, (int)(0.03f * SFX_HZ), 0.0f, 2);
  sfx_finish(SFX_NADE_EQUIP, 0.40f, 0.030f, 0.12f);

  // A DETONATION IS THE LOW END. A hard broadband crack for the shock front, a
  // long sub-heavy thump under it, and the arena answering. Peak-limited here
  // and placed by distance at the router, like a gunshot.
  sfx_alloc(SFX_BLAST, 2.40f);
  n = sfx_click(0.016f, 0.0030f, 1200.0f, 1.0f);
  sfx_lay(SFX_BLAST, n, 0, 0.0f, 0);
  n = sfx_burst(0.70f, 62.0f, 0.7f, 0.0006f, 0.190f, 1.1f, 1.00f);
  sfx_lay(SFX_BLAST, n, 0, 0.0f, 1);
  n = sfx_burst(0.45f, 240.0f, 0.8f, 0.0003f, 0.095f, 1.2f, 0.90f);
  sfx_lay(SFX_BLAST, n, 1, 0.0f, 2);
  n = sfx_burst(0.30f, 1900.0f, 0.8f, 0.0002f, 0.050f, 1.3f, 0.62f);
  sfx_lay(SFX_BLAST, n, 2, 0.0f, 2);
  sfx_tail(SFX_BLAST, (int)(0.05f * SFX_HZ), 1.9f, 180.0f, 0.7f, 0.020f, 0.42f, 0.34f);
  sfx_finish(SFX_BLAST, 0.92f, 0.120f, 0.30f);

  // Far away the crack is gone and what arrives is the rumble.
  sfx_alloc(SFX_BLAST_FAR, 2.60f);
  n = sfx_burst(0.80f, 72.0f, 0.7f, 0.006f, 0.26f, 1.1f, 1.00f);
  sfx_lay(SFX_BLAST_FAR, n, 0, 0.0f, 1);
  n = sfx_burst(0.50f, 320.0f, 0.8f, 0.004f, 0.11f, 1.2f, 0.34f);
  sfx_lay(SFX_BLAST_FAR, n, 1, 0.0f, 2);
  sfx_tail(SFX_BLAST_FAR, (int)(0.08f * SFX_HZ), 2.1f, 150.0f, 0.7f, 0.040f, 0.45f, 0.34f);
  sfx_finish(SFX_BLAST_FAR, 0.80f, 0.100f, 0.30f);

  // RELOADS ARE PER WEAPON AND TIMED FROM reload_ticks. The single shared
  // buffer was 1.30 s long with its last clack at 1.12 s, against reloads of
  // 1.80 s (AR) and 2.50 s (sniper): the audio announced a live weapon 680 ms
  // early on one gun and 1380 ms early on the other, then left silence while
  // the player believed they could shoot. Every offset below is a fraction of
  // the real duration, so the sound can never drift from the mechanic again.
  {
    const float hz = (float)TICK_HZ;
    float t_ar = (float)WPN_DEF[WPN_AR].reload_ticks / hz;
    // FIVE events, not four. The old set had no magazine FALLING FREE in it —
    // the one moment of a reload that is pure gravity and rattle rather than a
    // struck part — and left 300-600 ms gaps of digital silence between the
    // rest, which is what made it read as clicks rather than as a mechanism.
    // The order is the mechanic: release, mag drops out, fresh mag arrives,
    // mag SEATS, bolt goes home.
    //
    // The seat is where the WEIGHT is (118 Hz, the lowest mass in either
    // reload) and the bolt is where the KNACK is (2100 Hz tone, least mass, and
    // the loudest of the five). That split is deliberate and it is the whole
    // "fatter AND crisper": they are different questions and they belong to
    // different events. The bolt staying loudest also keeps `eng` — the energy
    // peak — on the event that ANNOUNCES A LIVE WEAPON, which is the one thing
    // in this sound the player acts on.
    // The BUFFER is the reload plus a tail; the OFFSETS stay fractions of the
    // reload itself. Allocating exactly reload_ticks truncated the final event
    // mid-decay — a step to zero, i.e. a click, on the one cue in this sound
    // the player acts on. The symptom is unmistakable once you know it: T20 and
    // T60 come back EQUAL (59 ms both), because neither is a decay any more,
    // both are the distance to the end of the buffer.
    sfx_alloc(SFX_RELOAD_AR, t_ar + 0.22f);
    sfx_mech(SFX_RELOAD_AR, (int)(0.055f * t_ar * SFX_HZ), 1900.0f, 240.0f,
             0.42f, 0.10f);                              // mag release button
    sfx_mech(SFX_RELOAD_AR, (int)(0.175f * t_ar * SFX_HZ), 950.0f, 190.0f,
             0.54f, 0.55f);                              // magazine falls free
    sfx_mech(SFX_RELOAD_AR, (int)(0.360f * t_ar * SFX_HZ), 760.0f, 150.0f,
             0.34f, 0.30f);                              // fresh mag to the well
    // 820, not 1150: a magazine body going into an alloy well is not a 1.1 kHz
    // ping, and at 1150 the event's LOUDEST band was 700 Hz-2 kHz with the meat
    // 3.3 dB under it. 98, not 122: the ring's fundamental has to land INSIDE
    // the 20-120 Hz band rather than 2 Hz above it — this file's own note says
    // a ~104 Hz ring at amp 0.30 is enough to be FELT, and the whole reload
    // measured -43.5 dB at 20-120 Hz against the gunshot's -22.3.
    sfx_mech(SFX_RELOAD_AR, (int)(RELOAD_BEAT[WPN_AR][0] * t_ar * SFX_HZ), 820.0f, 98.0f,
             0.94f, 0.22f);                              // THE SEAT
    // The bolt has to STAY the loudest event, and that is a contract rather
    // than a preference: `eng` — the energy peak — is the moment the sound says
    // A LIVE WEAPON, and it is the one thing in this sound the player acts on.
    // Fattening the seat moved `eng` from 1217 ms straight onto it at 721 ms in
    // one edit, exactly as this file's own note warns (the master limiter
    // squashes a peaky bolt harder than a sustained low thud). Every level
    // change in this block has to be re-checked against `eng`.
    sfx_mech(SFX_RELOAD_AR, (int)(RELOAD_BEAT[WPN_AR][1] * t_ar * SFX_HZ), 2100.0f, 235.0f,
             1.66f, 0.14f);                              // bolt release
    // A SIXTH EVENT, and it fills the largest hole in the sound: between the
    // magazine seating and the bolt going home there were 386 ms of sample-exact
    // silence in one block — the mechanism stopped existing for a third of a
    // second, in exactly the window where the ANIMATION also goes static. This
    // is the hand leaving the magwell for the charging handle, the one moment of
    // a reload that is travel rather than a struck part. At amp 0.30 it lands
    // ~10 dB under the seat, so it cannot threaten `eng`.
    // The two widest AR holes are 0.36 -> 0.56 and 0.56 -> 0.76 of progress.
    sfx_bed(SFX_RELOAD_AR, (int)(0.235f * t_ar * SFX_HZ), 0.130f, 2600.0f, 0.14f, 7);
    sfx_bed(SFX_RELOAD_AR, (int)(0.400f * t_ar * SFX_HZ), 0.150f, 2300.0f, 0.16f, 5);
    sfx_bed(SFX_RELOAD_AR, (int)(0.600f * t_ar * SFX_HZ), 0.140f, 1750.0f, 0.13f, 9);
    sfx_bed(SFX_RELOAD_AR, (int)(0.680f * t_ar * SFX_HZ), 0.110f, 2900.0f, 0.10f, 3);
    sfx_mech(SFX_RELOAD_AR, (int)(0.760f * t_ar * SFX_HZ), 620.0f, 165.0f,
             0.30f, 0.0f);                               // hand to the handle
    n = sfx_burst(0.10f, 2600.0f, 1.4f, 0.00004f, 0.012f, 1.4f, 0.66f);
    sfx_lay(SFX_RELOAD_AR, n, (int)(RELOAD_BEAT[WPN_AR][1] * t_ar * SFX_HZ), 0.10f, 2);
    // ...AND THE TACTICAL VARIANT, which is the same mechanism with the one
    // event that cannot have happened removed. With a round still chambered the
    // bolt never locked back, so there is no bolt RELEASE — the reload ends on
    // the magazine seating and a hand coming back to the grip. The offsets are
    // FRACTIONS of the tac clock but NOT the empty reload's fractions: with no
    // bolt work after it, THE SEAT is the last event and moves out to 0.880 —
    // and the pose reads RELOAD_SEAT_TAC so the two consumers stay one
    // timeline (they drifted 331 ms apart when the pose kept the empty 0.560).
    //
    // A SECOND BUFFER RATHER THAN A PLAYBACK RATE. Playing the empty buffer at
    // 152/124 to fit would shift it up four semitones and turn a magazine change
    // into a different weapon; 0.5 MB buys the mechanism actually being right.
    {
      float t_tac = (float)WPN_DEF[WPN_AR].reload_tac_ticks / hz;
      sfx_alloc(SFX_RELOAD_AR_TAC, t_tac + 0.22f);
      sfx_mech(SFX_RELOAD_AR_TAC, (int)(0.055f * t_tac * SFX_HZ), 1900.0f, 240.0f,
               0.42f, 0.10f);                            // mag release button
      sfx_mech(SFX_RELOAD_AR_TAC, (int)(0.185f * t_tac * SFX_HZ), 950.0f, 190.0f,
               0.54f, 0.55f);                            // magazine falls free
      sfx_mech(SFX_RELOAD_AR_TAC, (int)(0.400f * t_tac * SFX_HZ), 760.0f, 150.0f,
               0.34f, 0.30f);                            // fresh mag to the well
      sfx_bed(SFX_RELOAD_AR_TAC, (int)(0.260f * t_tac * SFX_HZ), 0.130f, 2600.0f, 0.14f, 7);
      sfx_bed(SFX_RELOAD_AR_TAC, (int)(0.450f * t_tac * SFX_HZ), 0.150f, 2300.0f, 0.16f, 5);
      // THE SEAT IS THE LAST EVENT HERE, so it is also the one `atk`/`eng` must
      // land on — the same contract the empty reload puts on its bolt, moved to
      // whichever event actually ends the mechanism. It is louder than the empty
      // reload's seat for exactly that reason.
      sfx_mech(SFX_RELOAD_AR_TAC, (int)(0.880f * t_tac * SFX_HZ), 820.0f, 98.0f,
               1.55f, 0.22f);                            // THE SEAT
      sfx_verb(SFX_RELOAD_AR_TAC, g_sfx_len[SFX_RELOAD_AR_TAC], 0.006f, 0.028f,
               0.15f, 3000.0f, 0.14f);
      sfx_finish(SFX_RELOAD_AR_TAC, 0.93f, 0.1050f, 0.30f);
    }
    // A ROOM ON THE RELOAD, and the recorded reason for refusing one is
  // numerically stale. The rejection was computed against reload_ticks = 300 and
  // said the sniper's last two events land at 2.15 and 2.38 s, past sfx_verb's
  // 2.000 s snapshot cap; the SHIPPED value is 214 ticks, so they land at
  // 1.409 s and 1.703 s — 297 ms and 591 ms INSIDE it. Measured, this was the
  // only sound in the game with no room at all: 100 ms after every transient
  // the buffer is 46-51 dB down, against the AR report's 11.6, the sniper's 5.9,
  // the kill's 8.9 and even a wall impact's 19.4, with an L/R correlation of
  // +0.98 against the AR shot's +0.67. A reload is heard at 30 cm, so the room
  // is SMALL and DARK — 28 ms of size, damped at 3 kHz, at a wet level low
  // enough that sfx_finish's gain stage barely notices it and `eng` cannot
  // migrate off the last event, which is this buffer's own contract.
  // RT60 0.15, NOT 0.42, AND THE NUMBER IS THE BUFFER'S OWN TAIL. A reverb tail
  // that runs off the end of its buffer is a step to zero, i.e. a click, at
  // exactly the moment the ear is listening to the decay — this file's own rule,
  // and at 0.42 s against the 269 ms of buffer left after the last authored
  // event it was being broken: the final 5 ms measured -41 dBFS peak where the
  // reference was exact digital silence, and `T60` had quietly become the
  // distance to the end of the buffer (atk 1217 + T60 259 = 1476 against a
  // length of 1487), which is the milder form of the "T20 and T60 come back
  // EQUAL" tell. A 30 cm-to-1 m gear return does not want a long tail anyway.
  sfx_verb(SFX_RELOAD_AR, g_sfx_len[SFX_RELOAD_AR], 0.006f, 0.028f, 0.15f,
           3000.0f, 0.14f);
  sfx_finish(SFX_RELOAD_AR, 0.88f, 0.1320f, 0.0f);

    // THE SNIPER CHANGES A MAGAZINE TOO. This used to be a five-round stripper
    // load — bolt lift, five rounds pressed in one at a time, bolt forward and
    // down — and the whole of it was audio for an animation that did not exist:
    // the viewmodel just rolled the rifle over for 1.8 seconds with nothing
    // moving on it. The mechanism is now an AICS box change finished by working
    // the action, and the sound describes that, event for event, off the same
    // RELOAD_BEAT the pose reads.
    //
    // It stays a DIFFERENT reload from the AR's, and the difference is in the
    // parts rather than in the sequence: a steel bolt body in a steel raceway
    // against an alloy carrier in an alloy receiver. Every event is lower, and
    // the two bolt events are the only ones with real metal ring in them.
    float t_sr = (float)WPN_DEF[WPN_SR].reload_ticks / hz;
    sfx_alloc(SFX_RELOAD_SR, t_sr + 0.22f);
    sfx_mech(SFX_RELOAD_SR, (int)(0.050f * t_sr * SFX_HZ), 1620.0f, 210.0f,
             0.46f, 0.10f);                              // paddle release
    sfx_mech(SFX_RELOAD_SR, (int)(0.185f * t_sr * SFX_HZ), 840.0f, 160.0f,
             0.60f, 0.55f);                              // the box falls free
    sfx_mech(SFX_RELOAD_SR, (int)(0.380f * t_sr * SFX_HZ), 700.0f, 132.0f,
             0.36f, 0.30f);                              // fresh box to the well
    // THE SEAT. 92 Hz rather than the AR's 98: a ten-round .308 box is heavier
    // than a STANAG and it lands in a chassis rather than in an alloy lower.
    sfx_mech(SFX_RELOAD_SR, (int)(RELOAD_BEAT[WPN_SR][0] * t_sr * SFX_HZ), 760.0f, 92.0f,
             0.84f, 0.24f);
    // The hand leaving the grip for the knob, in the window where the animation
    // is travel rather than a struck part — the same hole the AR's sixth event
    // fills, at the same level relative to the seat.
    // The sniper's worst hole is 276 ms starting at 0.225 — the AR got a sixth
    // event for exactly this and the SR's went to 0.700 instead, leaving the
    // window that needed it empty.
    sfx_bed(SFX_RELOAD_SR, (int)(0.250f * t_sr * SFX_HZ), 0.190f, 2100.0f, 0.17f, 6);
    sfx_bed(SFX_RELOAD_SR, (int)(0.450f * t_sr * SFX_HZ), 0.150f, 1600.0f, 0.14f, 11);
    sfx_bed(SFX_RELOAD_SR, (int)(0.620f * t_sr * SFX_HZ), 0.130f, 2600.0f, 0.11f, 4);
    sfx_mech(SFX_RELOAD_SR, (int)(0.700f * t_sr * SFX_HZ), 600.0f, 150.0f,
             0.30f, 0.0f);                               // hand to the bolt
    sfx_mech(SFX_RELOAD_SR, (int)(0.790f * t_sr * SFX_HZ), 1150.0f, 176.0f,
             0.52f, 0.20f);                              // bolt lift + draw
    // BOLT DOWN AND LOCKED stays the loudest event in the buffer, because `eng`
    // — the energy peak — is the moment the sound says A LIVE WEAPON, and on
    // this weapon that moment is the bolt handle, not the magazine.
    sfx_mech(SFX_RELOAD_SR, (int)(RELOAD_BEAT[WPN_SR][1] * t_sr * SFX_HZ), 1900.0f, 205.0f,
             1.78f, 0.15f);
    n = sfx_burst(0.10f, 2300.0f, 1.4f, 0.00004f, 0.013f, 1.4f, 0.62f);
    sfx_lay(SFX_RELOAD_SR, n, (int)(RELOAD_BEAT[WPN_SR][1] * t_sr * SFX_HZ), -0.08f, 2);
    sfx_verb(SFX_RELOAD_SR, g_sfx_len[SFX_RELOAD_SR], 0.006f, 0.028f, 0.15f,
           3000.0f, 0.14f);
  sfx_finish(SFX_RELOAD_SR, 0.88f, 0.1210f, 0.0f);
  }
}

