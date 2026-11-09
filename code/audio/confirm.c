// code/audio/confirm.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// THE CONFIRMATION MATRIX
//
// Four buffers, but NOT four compositions. What the player is being told has
// two independent axes, and each axis gets exactly ONE authored signature:
//
//   CONSEQUENCE (exclusive)  hit = the tick        kill = tick + mass + room
//   PRECISION   (additive)   body = nothing        head = + the ding
//
//                    body                       head
//     hit            sfx_hit_base               sfx_hit_base  + sfx_head_sig
//     kill           sfx_kill_base              sfx_kill_base + sfx_head_sig
//
// Consequence is a CHOICE (a shot either killed or it did not, so the two bases
// are alternatives, never summed); precision is a LAYER (the head signature is
// added on top of whichever base ran). That asymmetry is the whole design.
//
// This replaced four hand-authored sounds, and the failure that forced it was
// measurable and specific: `kill` and `kill_head` had drifted to 0.5 dBA and
// half an octave of spectral centroid apart — the closest pair in the entire
// set — because the head-ness of the head KILL had been re-invented as a
// brighter razor, a brighter ring, a brighter crunch and a higher room instead
// of being the same ding the head HIT uses. Four independent recipes cannot be
// kept in a relationship; two axes cannot fall out of one.
//
// Two consequences worth stating, because both are load-bearing:
//
//   The ding plays independently of lethality. The sniper does 150 damage
//   against 100 hp, so EVERY sniper hit is a kill, and `ent_damage` suppresses
//   EV_HIT when the victim's remaining hp is zero — the sniper therefore only
//   ever triggers `kill`/`kill_head`. With the signature as a layer, its
//   one-shot headshot carries the ding structurally. When it was a property of
//   four separate buffers, that was something you had to remember, and it had
//   already been forgotten.
//
//   The ladder is bought where the ear is, not where the headroom is. The
//   A-weighting curve peaks at 2-4 kHz, which is exactly where the ding lives,
//   so adding it raises the PERCEIVED level (the ruler `sfx_finish` gain-stages
//   against) far more than it raises the peak. The head variants therefore sit
//   above their body counterparts almost for free — no extra limiter drive, no
//   low end spent. It is also the band neither weapon occupies (AR 13% hi,
//   sniper 7%), so the signature never fights the shot that caused it.
// ---------------------------------------------------------------------------

// The PRECISION signature: "that was the head". One stack, laid identically on
// the hit and on the kill, so the cue means the same thing wherever it appears.
// `g` scales the whole signature against whatever base it is riding on.
//
// Metallic and bright by construction — a headshot should read as SHARPER, not
// as heavier. The weight underneath it belongs to the base and is deliberately
// not restated here; an earlier version gave the head hit its own thump, meat
// and weight layers at slightly different frequencies to the body hit's, which
// bought nothing except two sets of numbers to keep in sync.
//
// Called BEFORE the base's room, so the reverb hears the ding: a signature
// pasted on after the verb sits in front of the sound instead of inside it.
//
// SFX_HEAD_DUCK is the one number that makes a head variant read as SHARPER
// rather than merely LOUDER: the base is pulled down under the signature, so
// what arrives is the same event with a bright cue on top of it, not the same
// event with more of everything. It is the reason two axes do not collapse into
// a volume knob.
#define SFX_HEAD_DUCK 0.90f
static void sfx_head_sig(int id, float g) {
  int n;
  // The one layer that reaches above 8 kHz. Everything else here is a pure
  // partial or a band-passed burst, and the SVF is clamped at 7.4 kHz for
  // stability, so without an unfiltered tick the signature has no air at all.
  // Sub-millisecond, so it sharpens the attack rather than adding hiss.
  n = sfx_click(0.0015f, 0.00012f, 7000.0f, 0.80f * g);
  sfx_lay(id, n, 0, 0.0f, 0);                          // air
  n = sfx_ring(0.20f, 4500.0f, 6700.0f, 0.048f, 1.00f * g);
  sfx_lay(id, n, 0, 0.0f, 0);                          // THE DING
  n = sfx_ring(0.15f, 2950.0f, 5200.0f, 0.032f, 0.58f * g);
  sfx_lay(id, n, 2, 0.0f, 3);                          // beating = shimmer
  n = sfx_burst(0.06f, 2200.0f, 1.4f, 0.00002f, 0.011f, 1.6f, 1.75f * g);
  sfx_lay(id, n, 0, 0.0f, 1);                          // bone crack
  // 5400, not 6000: sfx_tail runs its second pole at hz*1.35 and the Chamberlin
  // SVF is only stable below SFX_HZ/6.5 = 7385 Hz, so 6000 asks for 8100 and is
  // silently clamped — the sizzle would not be the filter it says it is.
  // 5400*1.35 = 7290 leaves 95 Hz of margin.
  sfx_tail(id, (int)(0.001f * SFX_HZ), 0.18f, 5400.0f, 1.1f,
           0.003f, 0.032f, 0.16f * g);                       // sizzle
}

// CONSEQUENCE, alternative 1: a body being hit and still standing.
//
// This has to be heard THROUGH the shot that caused it, which is a spectrum
// problem and not a volume problem: the two sounds are simultaneous by
// definition. The AR carries real energy up to ~4 kHz, so the tick sits above
// it at 2.65/3.95 kHz.
//
// Four coupled ideas, each answering a different question:
//   razor + tick    WHERE and WHAT (survives being under a gunshot)
//   impact snap     that it was a bullet and not a UI beep
//   meat            the mid band, which an early version left at 2% — the hole
//                   between the tick and the thump is why it read as thin
//   thump + weight  the part you feel.
// Two things had to be learned expensively here, and both are about WHERE fat
// lives. (1) Not in the bottom octave: a sine carries ~10 dB more energy than a
// band-passed burst at the same peak, so a 155->62 Hz sweep at amp 0.42 alone
// put 33% of the sound under 120 Hz, and gain-staged to a perceived loudness it
// then needed so much gain that the limiter took the crest from 15.2 dB to 8.4.
// A squashed hitmarker is not a heavy one. The weight is a 104 Hz ring at amp
// 0.26 (felt on headphones, cheap in headroom) and the actual fat is 190-780 Hz,
// where the ear is 20 dB more sensitive. (2) The low layers are OFFSET 6-10 ms:
// laid at frame 0 their slow, large excursions add to the transient's peak, so
// the composite crest forces the limiter down on the whole sound. Six
// milliseconds apart the two peaks never meet — and crack-then-thud is what an
// impact is anyway.
// THE ONSET, authored once and laid by BOTH consequence bases. Whatever else a
// confirmation is, its first twenty milliseconds are "a round arrived", and the
// two bases disagreeing about that is what put the kill under the hit.
static void sfx_hit_attack(int id, float g) {
  int n;
  n = sfx_click(0.003f, 0.00026f, 2600.0f, 1.55f * g);
  sfx_lay(id, n, 0, 0.0f, 0);                          // razor
  n = sfx_ring(0.075f, 2650.0f, 3950.0f, 0.017f, 1.30f * g);
  sfx_lay(id, n, 0, 0.0f, 0);                          // the tick
  n = sfx_burst(0.055f, 1900.0f, 1.3f, 0.00002f, 0.010f, 1.6f, 2.60f * g);
  sfx_lay(id, n, 0, 0.0f, 1);                          // impact snap
}

// THE ROOM IS PART OF THE CONSEQUENCE AXIS, so it is authored once per
// consequence and applied to both precision variants. It used to be two literal
// parameter quintuples written out twice each, i.e. the "identical within the
// pair" contract held only as long as four call sites were edited together —
// exactly the drift the two-axis design above exists to make impossible.
static void sfx_room_hit(int id) {   // 8 ms of decay, so the ring is not a beep
  sfx_verb(id, (int)(0.09f * SFX_HZ), 0.005f, 0.013f, 0.13f, 3400.0f, 0.20f);
}
static void sfx_room_kill(int id) {  // a real room: a kill has size a hit lacks
  sfx_verb(id, (int)(0.30f * SFX_HZ), 0.011f, 0.030f, 0.42f, 2200.0f, 0.30f);
}

static void sfx_hit_base(int id, float g) {
  int n;
  sfx_hit_attack(id, g);
  n = sfx_burst(0.09f, 780.0f, 1.1f, 0.00005f, 0.020f, 1.3f, 2.80f * g);
  sfx_lay(id, n, (int)(0.001f * SFX_HZ), 0.0f, 2);     // meat
  n = sfx_ring(0.06f, 430.0f, 660.0f, 0.016f, 0.85f * g);
  sfx_lay(id, n, (int)(0.002f * SFX_HZ), 0.0f, 2);     // the thock
  n = sfx_burst(0.15f, 190.0f, 0.95f, 0.00015f, 0.042f, 1.1f, 0.90f * g);
  sfx_lay(id, n, (int)(0.006f * SFX_HZ), 0.0f, 3);     // the thump
  n = sfx_ring(0.11f, 104.0f, 158.0f, 0.028f, 0.26f * g);
  sfx_lay(id, n, (int)(0.010f * SFX_HZ), 0.0f, 0);     // the weight
}

// CONSEQUENCE, alternative 2: the target goes down. The loudest event in the
// game and the only one allowed to be.
//
// Everything the hit does, plus the two things a hit must NOT have: mass that
// keeps developing after the transient, and a real room. Structure, one
// question per layer:
//   razor + ring    the confirmation, in the band the gun does not own
//   crunch          bone. This is the "heftig" — a 1.15 kHz burst with a 15 ms
//                   decay, the sound of something breaking rather than being hit
//   meat + mass     430 Hz and 105 Hz, the two loudest layers in the sound
//   body tone       a 330/500 Hz RING, because 24 ms at 330 Hz is eight cycles
//                   and a burst that short is a random-phase lottery per build
//   the drop        an 88/132 Hz ring at +11 ms, the pressure of a body letting
//                   go. A ring and not a sweep: a sweep at the same peak carries
//                   ~10 dB more energy and this sound has already spent its
//                   low-end budget on the mass.
//   the arrival     at +95 ms, low-passed to nothing but weight: the body
//                   reaching the floor. It is what makes a kill feel FINAL, and
//                   being pure low end it cannot smear the tick.
//   resolution      two notes, 6 dB under everything else. They are 6 dB down
//                   because they were once level with the impact, which put the
//                   ENERGY peak of the kill 70 ms after the kill (`eng` printed
//                   70.6 ms) — that does not read as caused by the trigger pull,
//                   it reads as a jingle that happened afterwards. `eng` has to
//                   stay inside the ~50 ms window in which the ear fuses two
//                   events into one.
// THE CONSEQUENCE AXIS WAS INVERTED IN THE ATTACK. Over the first 20 ms the
// kill measured 4.57 dB LESS 2-6 kHz energy and 1.99 dB less 500 Hz-2 kHz than
// the HIT it replaces, and A-weighted it was 2.59 dB quieter at 20 ms and
// 2.94 dB quieter at 40 ms — it only overtook the hit at a 150 ms integration.
// On the bus, in a real four-round AR burst whose last round kills, the three
// hits read -7.98 / -8.08 / -8.58 dBA over their own 40 ms windows and the KILL
// read -10.62: the most rewarding event in the game announced itself with the
// dullest, quietest onset of the four. The cause is visible in the layer table —
// the kill had re-authored a second, DARKER transient (2100 Hz razor, 2450/3650
// ring, 1150 Hz crunch) where the hit has 2600 / 2650-3950 / 1900.
//
// So the kill now SHARES the hit's transient trio at bake time and its own
// layers are only what a kill ADDS: the crunch, the mass, the drop, the arrival
// and the room. That is the same refactor the precision axis already got — one
// authored signature, laid identically wherever it appears — and it does not
// violate "the two bases are alternatives, never summed", which is about PLAY
// time (one voice, one buffer) and not about sharing an attack at build time.
// THE TAKES. One kill sound for every kill was the complaint, and it was fair:
// the same buffer at the same pitch forty times a match stops being information
// and becomes a jingle. Each take keeps the STRUCTURE above (shared onset,
// crunch, meat, body tone, mass, drop, arrival, room) and moves what a real
// impact varies — how hard the bone gives, how big the man is, how long the
// body takes to reach the floor, a little kit rattling or not — so they are
// recognisably the same event and never the same recording.
typedef struct {
  float crunch_hz, meat_hz, tone1, tone2, mass_hz, drop1, drop2, arrive_s;
  float gear;   // kit rattling as he goes: 0 = none
} kill_take_t;
static const kill_take_t KILL_TAKE[4] = {
  {1150.0f, 430.0f, 330.0f, 500.0f, 105.0f, 88.0f, 132.0f, 0.095f, 0.00f},
  { 980.0f, 385.0f, 292.0f, 452.0f,  96.0f, 80.0f, 121.0f, 0.115f, 0.35f},
  {1320.0f, 470.0f, 362.0f, 545.0f, 112.0f, 94.0f, 141.0f, 0.085f, 0.00f},
  {1060.0f, 405.0f, 311.0f, 474.0f, 100.0f, 84.0f, 127.0f, 0.130f, 0.28f},
};
// NO RESOLUTION NOTES ANY MORE. The kill used to end on two pitched rings a
// fifth apart (1180/1770 Hz at +70 and +118 ms): a tiny two-note jingle after
// every kill. Even 6 dB down it was what the ear kept — a sound effect, not a
// body — and it is what made the confirmation read as ridiculous. What a kill
// adds over a hit is mass and finality, and the arrival carries the finality.
static void sfx_kill_base(int id, float g, const kill_take_t *k) {
  int n;
  sfx_hit_attack(id, g);                               // the SAME onset
  n = sfx_burst(0.085f, k->crunch_hz, 1.2f, 0.00003f, 0.015f, 1.5f, 2.30f * g);
  sfx_lay(id, n, 0, 0.0f, 1);                          // the crunch
  n = sfx_burst(0.13f, k->meat_hz, 1.0f, 0.00007f, 0.032f, 1.2f, 2.10f * g);
  sfx_lay(id, n, (int)(0.001f * SFX_HZ), 0.0f, 2);     // meat
  n = sfx_ring(0.09f, k->tone1, k->tone2, 0.024f, 0.85f * g);
  sfx_lay(id, n, (int)(0.002f * SFX_HZ), 0.0f, 2);     // the body tone
  n = sfx_burst(0.22f, k->mass_hz, 0.95f, 0.00020f, 0.070f, 1.0f, 1.05f * g);
  sfx_lay(id, n, (int)(0.007f * SFX_HZ), 0.0f, 3);     // the mass
  n = sfx_ring(0.16f, k->drop1, k->drop2, 0.045f, 0.42f * g);
  sfx_lay(id, n, (int)(0.011f * SFX_HZ), 0.0f, 0);     // the drop
  n = sfx_burst(0.16f, 120.0f, 0.9f, 0.00030f, 0.045f, 1.0f, 0.62f * g);
  sfx_lay(id, n, (int)(k->arrive_s * SFX_HZ), 0.05f, 4);   // the arrival
  if (k->gear > 0.0f) {                                // buckles and plates
    n = sfx_burst(0.07f, 2300.0f, 2.2f, 0.00010f, 0.014f, 1.4f, k->gear * g);
    sfx_lay(id, n, (int)((k->arrive_s - 0.012f) * SFX_HZ), -0.15f, 2);
    n = sfx_burst(0.05f, 1700.0f, 2.0f, 0.00010f, 0.010f, 1.4f, k->gear * 0.7f * g);
    sfx_lay(id, n, (int)((k->arrive_s + 0.018f) * SFX_HZ), 0.20f, 2);
  }
}

// THE PLAYER'S OWN DEATH, first person. It used to be the hurt thud replayed a
// fourth down, which is "you were hit" said slower — easy to miss under the
// fight that caused it, and it said nothing final. This is the one sound in the
// game allowed to take its time: a heavy blow to the chest, the world dropping
// away under it, two heartbeats, and the ears ringing as everything goes. All
// low and dark except the ring, so it can never be mistaken for a confirmation.
static int sfx_tone(float sec, float hz, float atk, float dec, float wob, float amp) {
  int n = (int)(sec * SFX_HZ);
  if (n > SFX_SCRATCH) n = SFX_SCRATCH;
  for (int i = 0; i < n; i++) {
    float t = (float)i / SFX_HZ;
    float e = (t < atk ? t / atk : 1.0f) * expf(-t / dec);
    g_scr[i] = sinf(F_TAU * hz * t) * e * (1.0f - wob + wob * sinf(F_TAU * 5.3f * t));
  }
  return scr_norm(n, amp);
}
static void sfx_die(int id, float low, float ring_hz, float beat_s) {
  int n;
  n = sfx_click(0.005f, 0.00080f, 260.0f, 0.70f);
  sfx_lay(id, n, 0, 0.0f, 0);                          // the blow
  n = sfx_burst(0.20f, 170.0f * low, 0.9f, 0.00025f, 0.060f, 1.1f, 1.20f);
  sfx_lay(id, n, 0, 0.0f, 3);                          // through the chest
  n = sfx_ring(0.30f, 64.0f * low, 97.0f * low, 0.100f, 0.80f);
  sfx_lay(id, n, (int)(0.004f * SFX_HZ), 0.0f, 0);     // the body
  n = sfx_sweep(1.10f, 150.0f * low, 34.0f, 0.30f, 0.38f, 1.0f, 0.75f);
  sfx_lay(id, n, (int)(0.010f * SFX_HZ), 0.0f, 0);     // the floor falling away
  for (int k = 0; k < 2; k++) {                        // lub-dub, slowing
    n = sfx_burst(0.14f, 68.0f * low, 0.8f, 0.0040f, 0.038f, 1.0f, k ? 0.42f : 0.58f);
    sfx_lay(id, n, (int)((0.36f + beat_s * (float)k) * SFX_HZ), 0.0f, 2);
  }
  n = sfx_tone(1.60f, ring_hz, 0.060f, 0.55f, 0.25f, 0.14f);
  sfx_lay(id, n, (int)(0.090f * SFX_HZ), 0.0f, 0);     // the ears ringing
  sfx_tail(id, (int)(0.008f * SFX_HZ), 0.90f, 300.0f, 0.7f, 0.020f, 0.30f, 0.22f);
  sfx_verb(id, (int)(0.60f * SFX_HZ), 0.014f, 0.050f, 1.10f, 900.0f, 0.35f);
}

