// code/anim/hands.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// What the weapon in a pair of hands is DOING this frame: the presentation
// state the MODEL needs and the pose does not. One struct and one solver,
// because the viewmodel and the third-person figure are two views of one
// mechanism — and until this existed only the first-person one had a reload at
// all. Filmed from outside, a magazine change was a man standing still holding a
// rifle for 1.8 seconds, which is exactly the first/third-person divergence this
// pass is about.
//
// Every field is 0..1 and every consumer scales it, so a weapon that does not
// have the part (a bolt gun has no charging handle) simply ignores the number.
typedef struct {
  float arc;    // the reload's body arc: the gun comes off the shoulder and back
  float cyc;    // bolt / charging-handle cycle, one lobe late in the reload
  float punch;  // the two beat impulses (magazine SEATS, bolt goes home), summed
  float mag;    // magazine travel out of the well
  // THE MAGAZINE IS EXCHANGED, NOT DIPPED. `pouch` is the support hand's trip
  // to the belt and back (0 at the well, 1 at the pouch, off-frame below):
  // the magazine in the hand rides it, and what comes back up is a FRESH one.
  // `drop_t` is seconds since an EMPTY magazine was released and fell free
  // (< 0: none falling); on a tactical reload the spent one is stripped and
  // pocketed instead, so nothing falls. `maghide` is the hand-magazine absent:
  // the hand going down empty after the drop.
  float pouch, drop_t;
  int   maghide;
  // THE TWO-MAGAZINE TACTICAL RELOAD. The hand fetches the FRESH magazine
  // first and comes back to the well holding it; it strips the old one with
  // the fresh one still in the same hand (both visible side by side), shifts
  // across (`pair` 0 -> 1) so the fresh one is under the well, seats it, and
  // only then stows the old one. Two magazines on screen at once is what makes
  // it read as an EXCHANGE rather than the same magazine going back in.
  float pair;
  int   old_vis, fresh_vis;     // which of the two are drawn
  float old_pouch, fresh_pouch; // how much of the hand's pouch trip each rides
  float pouch3;                 // the single-magazine view (third person)
  float index;                  // the fresh magazine's nose-forward tilt, 0..1
  int   retain;
  float hand;   // support hand off the fore-end and onto the magazine
  float trig;   // trigger travel, 0 = at rest on the blade
  // The BOLT's own 0..1, and it is a monotone RAMP where `cyc` is a hump. They
  // are two different things: `cyc` is what punches the POSE late in the reload
  // and has to return to zero, while a bolt goes up, back, forward and down and
  // never runs backwards. Driving the geometry off `cyc` would cycle the bolt
  // and then un-cycle it. Zero on the AR, whose reload ends on a bolt CATCH
  // rather than a handle throw.
  float bolt;
  float bhand;  // trigger hand off the grip and onto the bolt knob, 0..1
  float slide;  // the pistol's slide: 1 = fully back, held there on an empty mag
  float spoon;  // the grenade's lever has flown
  // The knife, as ONE timeline the viewmodel and the figure both play. x, y, z
  // are metres right/up/forward of the eye; pitch/yaw/roll radians, applied in
  // that order. guard is the off-hand's rise into the cross-guard.
  float kn[6], guard;
  // The grenade throw's body (third person only, see NADE_HOLD): trunk twist
  // and bend in radians, throwing-elbow lift and off-arm reach 0..1.
  float tw, bd, eu, lr;
} gunanim_t;

// One keyframe of the knife. t is the normalised swing time.
typedef struct { float t, k[6]; } knife_key_t;
// Held up and across the frame, flat of the blade to the eye: a knife pointed down
// the view axis foreshortens into a plank and hides the hand behind it. Every swing
// keeps the flat turned toward the camera, for the same reason.
#define KNIFE_HOLD {0.235f, -0.100f, 0.360f, -0.45f, 1.15f, 0.25f}
static const knife_key_t KNIFE_KEY[3][6] = {
  // Forehand: cock high and right, whip across, finish low and left.
  {{0.00f, KNIFE_HOLD},
   {0.15f, {0.300f, -0.010f, 0.310f, -0.25f,  1.35f,  0.85f}},
   {0.26f, {0.325f,  0.000f, 0.320f, -0.20f,  1.40f,  1.00f}},
   {0.36f, {0.070f, -0.085f, 0.420f, -0.55f,  0.75f,  0.15f}},
   {0.50f, {-0.210f, -0.170f, 0.360f, -0.85f,  0.10f, -0.65f}},
   {1.00f, KNIFE_HOLD}},
  // Backhand: across the body and back out the way it came, wrist turned over.
  {{0.00f, KNIFE_HOLD},
   {0.15f, {-0.110f, -0.040f, 0.320f, -0.35f, -0.95f, -0.75f}},
   {0.26f, {-0.150f, -0.035f, 0.330f, -0.30f, -1.05f, -0.90f}},
   {0.36f, {0.070f, -0.085f, 0.420f, -0.55f, -0.30f, -0.10f}},
   {0.50f, {0.340f, -0.120f, 0.360f, -0.85f,  0.75f,  0.55f}},
   {1.00f, KNIFE_HOLD}},
  // Thrust: the hand draws back to the hip and drives forward and in.
  {{0.00f, KNIFE_HOLD},
   {0.20f, {0.250f, -0.125f, 0.270f, -0.55f,  1.05f,  0.25f}},
   {0.34f, {0.245f, -0.120f, 0.245f, -0.52f,  1.00f,  0.25f}},
   {0.42f, {0.155f, -0.135f, 0.520f, -0.72f,  0.45f, -0.05f}},
   {0.54f, {0.150f, -0.130f, 0.560f, -0.74f,  0.42f, -0.05f}},
   {1.00f, KNIFE_HOLD}},
};
// The guard: blade upright across the centre line, edge out, off hand behind it.
static const float KNIFE_GUARD[6] = {0.135f, -0.085f, 0.300f, -0.42f, -0.22f, 0.50f};

// The grenade, third person. The first six numbers are the knife's timeline
// (the hand, metres right/up/forward of the eye, then pitch/yaw/roll); the last
// four drive the BODY, which is what makes a throw read as a throw from outside:
//   tw  trunk twist, radians: + turns the throwing shoulder back
//   bd  trunk bend, radians: + pitches the chest forward over the hips
//   eu  the throwing elbow's lift, 0 = hanging, 1 = up at shoulder height
//   lr  the off arm's reach toward the target, 0 = at the sternum, 1 = out
// A hand pushed from the hold to in front of the face with the trunk still
// square read as a push, not a throw: the power in a real one comes from the
// shoulders turning through 90 degrees and the chest coming over the front
// foot, with the arm drawn back behind the head and the elbow up.
enum { NK_TW = 6, NK_BD, NK_EU, NK_LR, NK_N };
// The hold, from the reference: grenade upright beside the right side of the
// head and a little ahead of it, upper arm down and out, forearm standing
// up, the off arm reaching forward. The hand turns as the first-person grip
// does (G_IDLE's pitch/yaw/roll, measured from the same eye), so the shell
// faces the target with the fingers round its front. The old yaw/roll of
// 1.00/0.42 laid the forearm flat across the face, turned in at the chest.
static const float NADE_HOLD[NK_N] = {0.300f, -0.010f, 0.150f,  0.30f, 0.80f, -0.77f, 0.10f,  0.00f, 0.0f, 0.85f};
// Cooking: grenade up by the ear, shoulder turned a little, off hand out.
static const float NADE_COCK[NK_N] = {0.310f,  0.030f, 0.080f,  0.40f, 0.80f, -0.75f, 0.35f, -0.05f, 0.0f, 0.90f};
// The full draw: hand behind and above the head, elbow up, shoulders turned
// side-on, chest leaning back off the front foot, off arm pointing.
static const float NADE_BACK[NK_N] = {0.250f,  0.120f, -0.130f, 0.60f, 1.10f, 0.25f,  0.95f, -0.20f, 1.0f, 1.0f};
// The release: arm extended high and forward, shoulders square past the
// target, chest coming over.
static const float NADE_OUT[NK_N]  = {0.110f,  0.170f, 0.480f, -0.80f, 0.60f, 0.10f, -0.45f,  0.34f, 0.6f, 0.2f};
// The follow-through: hand carried down across the body to the far hip.
static const float NADE_FOLL[NK_N] = {-0.110f, -0.400f, 0.380f, -1.50f, -0.10f, -0.30f, -0.70f, 0.50f, 0.0f, 0.0f};
// ...and the recovery out of it, before the next one comes up.
static const float NADE_DOWN[NK_N] = {0.020f, -0.450f, 0.300f, -1.20f, 0.30f, 0.00f, -0.20f,  0.15f, 0.0f, 0.0f};
// Underhand: swung back past the hip with the knees in it, through and up.
static const float NADE_LOWC[NK_N] = {0.200f, -0.420f, -0.020f, -0.60f, 0.40f, -0.10f, 0.45f,  0.25f, 0.0f, 0.7f};
static const float NADE_LOWO[NK_N] = {0.110f, -0.120f, 0.520f, -0.20f, -0.10f, 0.00f, -0.15f, 0.08f, 0.0f, 0.3f};
static const float NADE_LOWF[NK_N] = {0.090f,  0.020f, 0.500f, -0.60f, -0.10f, 0.00f, -0.25f, 0.02f, 0.0f, 0.1f};

static void grenade_anim(const weapon_t *w, float alpha, gunanim_t *g) {
  const float *pa = NADE_HOLD, *pb = NADE_HOLD;
  float u = 0.0f;
  float st[NK_N];   // the throw's start pose: outlives the branch, pa may point at it
  if (w->gr_t > 0) {
    float t = (float)w->gr_t - 1.0f + alpha;
    const float WIND = (float)NADE_WIND_T, REL = (float)NADE_RELEASE_T;
    const float FOLL = REL + 12.0f, RISE = (float)NADE_RISE_T;
    // The draw starts from wherever the cook had got to: a tap lets go with
    // the arm barely off the hold, a long cook with it already up at the cock.
    float c0 = smooth01(f_clamp(((float)(NADE_FUSE - w->gr_fuse) - (float)NADE_PIN_T + 6.0f) / 26.0f,
                                0.0f, 1.0f));
    for (int i = 0; i < NK_N; i++) st[i] = NADE_HOLD[i] + (NADE_COCK[i] - NADE_HOLD[i]) * c0;
    const float *back = w->gr_lob ? NADE_LOWC : NADE_BACK;
    const float *out  = w->gr_lob ? NADE_LOWO : NADE_OUT;
    const float *foll = w->gr_lob ? NADE_LOWF : NADE_FOLL;
    if (t < WIND) {                           // the draw
      pa = st; pb = back; u = smooth01(t / WIND);
    } else if (t < REL) {                     // the drive: fastest at the release
      pa = back; pb = out; u = (t - WIND) / (REL - WIND);
      u = u * u * (1.7f - 0.7f * u);
    } else if (t < FOLL) {                    // the arm carries on, bleeding speed
      pa = out; pb = foll; u = (t - REL) / (FOLL - REL);
      u = 1.0f - (1.0f - u) * (1.0f - u);
    } else if (t < RISE) {                    // ...and straightens out of it
      pa = foll; pb = NADE_DOWN; u = smooth01((t - FOLL) / (RISE - FOLL));
    } else {                                  // the next one comes up
      pa = NADE_DOWN; pb = NADE_HOLD;
      u = smooth01((t - RISE) / (float)(NADE_THROW_T - NADE_RISE_T));
    }
    // Gone from the hand from the release until the next one is up.
    g->bolt = t >= REL && t < RISE ? 1.0f : 0.0f;
    if (t < REL) g->spoon = g->slide = 1.0f;   // pin and lever stay gone
  } else if (w->gr_cook > 0) {
    pb = NADE_COCK;
    // The wind-up waits for the pin: a hand coming up to the shoulder while the
    // other one is still reaching for the ring drags the ring out of its reach.
    u = smooth01(f_clamp(((float)w->gr_cook - (float)NADE_PIN_T + 6.0f + alpha) / 26.0f,
                         0.0f, 1.0f));
    // The off hand reaches in, takes the ring, and draws away.
    g->guard = sinf(f_clamp(((float)w->gr_cook + alpha) / (float)(NADE_PIN_T + 10), 0.0f, 1.0f) * F_PI);
    g->slide = w->gr_cook >= NADE_PIN_T / 2 ? 1.0f : 0.0f;    // pin is out
    g->spoon = w->gr_cook >= NADE_PIN_T ? 1.0f : 0.0f;        // ...and the lever gone
  }
  for (int i = 0; i < 6; i++) g->kn[i] = pa[i] + (pb[i] - pa[i]) * u;
  g->tw = pa[NK_TW] + (pb[NK_TW] - pa[NK_TW]) * u;
  g->bd = pa[NK_BD] + (pb[NK_BD] - pa[NK_BD]) * u;
  g->eu = pa[NK_EU] + (pb[NK_EU] - pa[NK_EU]) * u;
  g->lr = pa[NK_LR] + (pb[NK_LR] - pa[NK_LR]) * u;
  // A live grenade in the hand: a tremor that builds as the fuse runs down.
  if (w->gr_cook > NADE_PIN_T) {
    float k = (float)w->gr_cook / (float)NADE_FUSE, c = (float)w->gr_cook + alpha;
    k = k * k * k * 0.0065f;
    g->kn[0] += sinf(c * 1.13f) * k;
    g->kn[1] += sinf(c * 1.61f + 1.0f) * k;
    g->kn[5] += sinf(c * 0.91f + 2.0f) * k * 3.0f;
  }
}

static void knife_anim(const weapon_t *w, float alpha, gunanim_t *g) {
  for (int i = 0; i < 6; i++) g->kn[i] = KNIFE_KEY[0][0].k[i];
  if (w->melee_t > 0) {
    int v = w->melee_v > 2 ? 2 : w->melee_v;       // wire-fed on a client
    const knife_key_t *kk = KNIFE_KEY[v];
    float t = ((float)w->melee_t - 1.0f + alpha) / (float)MELEE[v].total;
    t = f_clamp(t, 0.0f, 1.0f);
    int s = 0;
    while (s < 4 && t >= kk[s + 1].t) s++;
    float u = f_clamp((t - kk[s].t) / fmaxf(kk[s + 1].t - kk[s].t, 1e-4f), 0.0f, 1.0f);
    // The strike legs run on an ease-in: slow to leave the cock, fastest at the
    // target. Everything else eases both ways.
    u = (s == 2 || s == 3) ? u * u * (1.6f - 0.6f * u) : smooth01(u);
    for (int i = 0; i < 6; i++)
      g->kn[i] = kk[s].k[i] + (kk[s + 1].k[i] - kk[s].k[i]) * u;
  }
  float ga = w->prev_ads_s + (w->ads_s - w->prev_ads_s) * alpha;
  for (int i = 0; i < 6; i++) g->kn[i] += (KNIFE_GUARD[i] - g->kn[i]) * ga;
  g->guard = ga;
  // A blocked hit drives the guard back toward the chest and turns the wrist.
  float bk = w->prev_block_k + (w->block_k - w->prev_block_k) * alpha;
  g->kn[2] -= 0.075f * bk;
  g->kn[1] -= 0.030f * bk;
  g->kn[5] += 0.30f * bk;
}

static gunanim_t gun_anim(const weapon_t *w, float alpha) {
  gunanim_t g = {0};
  // A weapon at rest HAS its magazine (model group 1 on every weapon). Set
  // before any early return — zero here drew every rifle without one.
  g.old_vis = 1;
  int cur = w->cur;
  g.trig = w->prev_trig_s + (w->trig_s - w->prev_trig_s) * alpha;
  if (cur == WPN_KN) {
    knife_anim(w, alpha, &g);
    return g;
  }
  if (cur == WPN_GR) {
    grenade_anim(w, alpha, &g);
    return g;
  }
  if (cur == WPN_PT) {
    // The slide runs on the weapon's own cycle: back fast, home on the spring,
    // and locked open on the last round until the reload drops it.
    float fb[WP_COUNT];
    float cyc = 60.0f / gun_feel(w, WPN_PT, fb)[WP_RPM] * (float)TICK_HZ;
    float c = 1.0f - ((float)w->cool[WPN_PT] - alpha) / fmaxf(cyc, 1.0f);
    if (w->cool[WPN_PT] > 0)
      g.slide = c < 0.28f ? c / 0.28f : (1.0f - c) / 0.72f;
    if (w->ammo[WPN_PT] == 0 && w->reload_t == 0) g.slide = 1.0f;
  }
  g.drop_t = -1.0f;
  if (w->reload_t <= 0) return g;
  // The SAME duration the sim is counting down, or every beat in the animation
  // lands on a different tick from the sound it is supposed to share.
  float ticks = (float)(w->reload_tac ? WPN_DEF[cur].reload_tac_ticks
                                      : WPN_DEF[cur].reload_ticks);
  float prog = 1.0f - (float)w->reload_t / ticks;
  // The AR's TACTICAL reload is a different mechanism with its own seat
  // fraction (see RELOAD_SEAT_TAC): the seat is the LAST event, the charging
  // handle and the bolt release do not happen. The SR is exempt — a bolt gun
  // works its action either way, and its two durations are identical.
  int tac_ar = w->reload_tac && cur != WPN_SR;
  float seat = tac_ar ? RELOAD_SEAT_TAC : RELOAD_BEAT[cur][0];
  // THE ARC ENDS ON THE SEAT. At 1.25 it held above 0.85 from 30% to 52% of the
  // reload — 275 ms, 22% of the whole thing, with nothing happening — and then
  // sat at exactly zero for 100 ms before the bolt. At 1.79 the arc lands on the
  // magazine seating, so the seat beat punches a STILL weapon exactly the way
  // the bolt beat already did, which is why the bolt read and the seat did not.
  // 1.79 is 1/seat for the 0.560 beats; the tac arc lands on ITS seat.
  g.arc = sinf(f_clamp(prog * (tac_ar ? 1.0f / RELOAD_SEAT_TAC : 1.79f),
                       0, 1) * F_PI);
  // THE SECOND HALF OF THE RELOAD WAS DEAD. `arc`'s argument clamps, so it
  // reaches zero at prog 0.559 and STAYS there, and the seat impulse is spent by
  // 0.73 — from there to the bolt at 0.96 nothing drove the pose at all.
  // Measured: 45 changed pixels of 921600 between the frame at prog 0.921 and
  // the standing IDLE frame. This is the charging-handle pull (the AR) or the
  // bolt cycle (the SR): the weapon coming IN toward the eye and canting the
  // OTHER way from the magazine change, so it can never read as a second,
  // smaller mag swap. One lobe, zero at both ends, and one tick SHORT of the
  // remaining progress — the last tick of an AR reload is at prog 0.99342, so a
  // lobe that is zero at exactly 1.0 is still at 0.052 when the reload ends.
  // ...and not at all on a tactical AR reload: there is no charging-handle
  // pull, so a cyc hump would be the pose inventing an event the audio does
  // not have (the same rule that already gates g.bolt below).
  g.cyc = tac_ar ? 0.0f
        : sinf(f_clamp((prog - 0.60f) / (0.40f - 1.0f / ticks), 0, 1) * F_PI);
  // THE BOLT BEAT HAS TO REACH ZERO ON THE LAST TICK, AND x*e^(1-x) CANNOT.
  // There are only 0.04 of progress left after the beat — 6 ticks on the AR — so
  // that impulse was still at 79% of its peak on the final tick, and one tick
  // later every term becomes zero at once: 32 mm of translation and 10.5 degrees
  // of roll gone in 8.3 ms. Measured through a magenta recolour of the receiver
  // group, the weapon's screen centroid moved 32.0 px between reload_t 1 and 0
  // against 0.9-12.3 px on the four ticks before it. A half-sine lobe is zero at
  // both ends by construction and its window is one tick short of the remaining
  // progress, so the last tick lands exactly ON the zero.
  // The impulse's time constant is per-variant DATA, not one number: the beat
  // has to decay to nothing before prog 1.0 or the end of the reload cuts it
  // in one tick (the documented x*e^(1-x) failure). The empty seat at 0.560
  // has 20.6 constants of runway at 0.021; the tac seat at 0.880 had 5.3 and
  // still read 7% of peak on the final tick — 0.0075 restores ~15 constants.
  float x1 = (prog - seat) / (tac_ar ? 0.0075f : 0.021f);
  // Guarded because the numerator is DATA: a beat set within one tick of 1.0
  // makes this zero or negative, i.e. a NaN offset on the whole viewmodel — an
  // invisible weapon from a plausible one-character edit.
  float bw = fmaxf((1.0f - RELOAD_BEAT[cur][1]) - 1.0f / ticks, 1.0f / ticks);
  float x2 = f_clamp((prog - RELOAD_BEAT[cur][1]) / bw, 0.0f, 1.0f);
  // No second beat on the tactical AR: the 1.45-amplitude bolt-release punch
  // fired at prog 0.96 into audio silence, for a bolt event that does not
  // exist on this reload.
  g.punch = (x1 > 0.0f ? 1.20f * x1 * expf(1.0f - x1) : 0.0f)
          + (tac_ar ? 0.0f : 1.45f * sinf(x2 * F_PI));
  // THE HAND ARRIVES FIRST, THEN THE MAGAZINE MOVES. They used to start
  // together, so at any mid-travel tick the hand was still a fraction of the way
  // from the fore-end while the magazine had already dropped — 24 mm of daylight
  // between a fist and the thing it is supposed to be holding, which is the
  // whole reason the grip did not read.
  g.mag = prog < 0.22f ? 0.0f
        : prog < 0.30f ? smooth01((prog - 0.22f) / 0.08f)
        : prog < 0.40f ? 1.0f
        // The insert is a POWER curve, not a smoothstep: a magazine goes in
        // slowly and then slams the last centimetre, and a symmetric ease spends
        // its speed in the middle where nothing happens. Cubed, 70% of the travel
        // is in the last third and it lands exactly on the seat. CLAMPED because
        // the branch bound (`seat`) and the ramp's window are two separate
        // numbers: they coincide today — GUN_MAG_U's 0.40+0.16 ends on
        // RELOAD_BEAT[*][0] = 0.560 and GUN_MAG_U_TAC's window ends on
        // RELOAD_SEAT_TAC — and the clamp is what keeps a beat MOVED without its
        // window from running u past 1. The sniper's old 0.860 seat against the
        // 0.16 window reached u = 2.875, where 1 - u^3 is -22.7: a magazine
        // twenty metres under the arena.
        //
        // IT HAS TO BE 1 - u^3, NOT (1 - u)^3, and the two are mirror images.
        // The shipped expression was the second: it put 34.7 mm of travel in the
        // first 51 ms and 0.94 mm in the last 51 ms, i.e. the magazine COASTED
        // TO A STOP about 50 ms BEFORE the seat sound and there was nothing left
        // to slam. Measured against the comment's own claim — 70% of the travel
        // in the last third — 1-u^3 gives 0.704 at u = 2/3 (exact) while
        // (1-u)^3 gives 0.037, off by a factor of 19. The filmed viewmodel
        // agreed: the changed-pixel mask against idle hit its GLOBAL MINIMUM
        // over the whole reload (30958 px, 3.4% of the frame) on the seat tick.
        : prog < seat
            ? (tac_ar
                 ? 1.0f - GUN_MAG_U_TAC(prog) * GUN_MAG_U_TAC(prog) * GUN_MAG_U_TAC(prog)
                 : 1.0f - GUN_MAG_U(prog) * GUN_MAG_U(prog) * GUN_MAG_U(prog))
            : 0.0f;
  // THE EXCHANGE. Empty: the release at 21.5% lets the spent magazine fall
  // free (and land on the floor — see the world magazines) while the hand
  // goes straight to the pouch, comes back up with a fresh one and meets the
  // well at 40%, where the insert above takes over.
  // Retained (tactical, either rifle): the two-magazine technique, see `pair`.
  int retain = w->reload_tac != 0;
  g.retain = retain;
  g.old_vis = 1;
  g.fresh_vis = 0;
  if (cur == WPN_AR || cur == WPN_SR) {
    if (!retain) {
      // A SPEED RELOAD. The support hand does not wait for the magazine: it
      // leaves the fore-end and is already on its way to the pouch when the
      // thumb hits the release (21.5%) and the empty falls clear. It reaches
      // the rig at 30%, closes on the fresh magazine, and brings it back up in
      // an arc, nose tilted forward to index it against the well (35-45%);
      // the last 11% is the insert, ending on the seat.
      g.pouch = prog < 0.12f ? 0.0f
              : prog < 0.30f ? smooth01((prog - 0.12f) / 0.18f)
              : prog < 0.35f ? 1.0f
              : prog < 0.45f ? 1.0f - smooth01((prog - 0.35f) / 0.10f) : 0.0f;
      g.mag = prog < 0.215f ? 0.0f
            : prog < 0.45f ? 1.0f
            : prog < seat ? 1.0f - powf(f_clamp((prog - 0.45f) / (seat - 0.45f), 0.0f, 1.0f), 3.0f)
            : 0.0f;
      g.maghide = 0;
      if (prog >= 0.215f) g.drop_t = (prog - 0.215f) * ticks * TICK_DT;
      g.old_vis = prog < 0.215f;
      g.fresh_vis = prog >= 0.30f;          // it comes out of the pouch in the hand
      g.pair = 0.0f;
      g.old_pouch = 0.0f;
      g.fresh_pouch = prog < seat ? 1.0f : 0.0f;
      g.index = prog >= 0.30f && prog < seat ? g.pouch : 0.0f;
      g.pouch3 = prog >= 0.30f ? g.pouch : 0.0f;
      g.maghide = prog >= 0.215f && prog < 0.30f;   // third person: hand empty
    } else {
      // THE TACTICAL RELOAD, at a hand's natural pace: to the pouch (8-20%),
      // close on the fresh magazine (20-23%), back to the well with it held
      // nose-forward (23-31%), strip the old one beside it (31-37%), shift
      // across (37-43%), insert (43% to the seat), stow the old one after.
      float st0 = seat + 0.008f;
      float st1 = st0 + (tac_ar ? 0.050f : 0.060f);
      float rt0 = st1 + 0.010f, rt1 = rt0 + (tac_ar ? 0.040f : 0.060f);
      g.pouch = prog < 0.08f ? 0.0f
              : prog < 0.20f ? smooth01((prog - 0.08f) / 0.12f)
              : prog < 0.23f ? 1.0f
              : prog < 0.31f ? 1.0f - smooth01((prog - 0.23f) / 0.08f)
              : prog < st0 ? 0.0f
              : prog < st1 ? smooth01((prog - st0) / (st1 - st0))
              : prog < rt0 ? 1.0f
              : prog < rt1 ? 1.0f - smooth01((prog - rt0) / (rt1 - rt0)) : 0.0f;
      g.mag = prog < 0.31f ? 0.0f
            : prog < 0.37f ? smooth01((prog - 0.31f) / 0.06f)
            : prog < 0.43f ? 1.0f
            : prog < seat ? 1.0f - powf(f_clamp((prog - 0.43f) / (seat - 0.43f), 0.0f, 1.0f), 3.0f)
            : 0.0f;
      g.pair = smooth01(f_clamp((prog - 0.37f) / 0.06f, 0.0f, 1.0f));
      g.old_vis = prog < st1;
      g.fresh_vis = prog >= 0.20f;
      g.old_pouch = prog >= 0.31f ? 1.0f : 0.0f;
      g.fresh_pouch = prog < seat ? 1.0f : 0.0f;
      g.index = prog >= 0.20f && prog < 0.31f ? g.pouch : 0.0f;
      g.pouch3 = 0.0f;
    }
  }
  // The support hand leaves the fore-end at 10%, is on the magazine by 22% —
  // before the magazine starts moving — and only starts letting go at 72%
  // (tactical AR: the hand rides the magazine to ITS later seat, then makes a
  // quick return — the seat is the event that ends this mechanism).
  g.hand = smooth01(f_clamp((prog - (cur == WPN_AR || cur == WPN_SR ? 0.07f : 0.10f)) / 0.12f, 0.0f, 1.0f)) *
           (tac_ar ? smooth01(f_clamp((0.995f - prog) / 0.025f, 0.0f, 1.0f))
                   : smooth01(f_clamp((0.90f - prog) / 0.18f, 0.0f, 1.0f)));
  // The AR racks its charging handle on the same window the sniper works its
  // bolt on, and off the same ramp — one mechanism, two shapes.
  // ...and it does not happen at all on a TACTICAL reload of the AR: the bolt
  // never locked back, so there is nothing to release. The sniper works its
  // action either way, which is why it is exempt.
  g.bolt = tac_ar
         ? 0.0f
         : f_clamp((prog - 0.660f) / (0.330f - 1.0f / ticks), 0.0f, 1.0f);
  if (cur == WPN_SR) {
    // A bolt gun's reload ENDS with the action being worked, and that is the
    // half of it a player recognises. The window opens after the magazine is
    // seated and closes one tick short of the end, for the same reason every
    // other lobe here does.
    // The right hand leaves the grip for the knob and comes back. It has to be
    // BACK before the reload ends — a weapon that goes live with the shooter's
    // hand off the grip reads as a dropped rifle.
    g.bhand = smooth01(f_clamp((prog - 0.620f) / 0.060f, 0.0f, 1.0f)) *
              smooth01(f_clamp((0.995f - prog) / 0.040f, 0.0f, 1.0f));
  }
  return g;
}

// A body handed to the presentation layer at the respawn so it can outlive the
// entity that was it — see the corpse pool with the VFX. Declared here because
// both death paths call it and the pool is defined a long way below.
static void corpse_add(const anim_t *A, v3 pos, v3 prev_pos, rag_t rag,
                       float yaw, float slack, float prev_slack, gunanim_t gs,
                       int wpn, int pal, int owner);

static void bot_fire(int bi) {
  bot_t *b = &g_bots[bi];
  v3 o = {{b->pos.x, b->pos.y + bot_eye(b), b->pos.z}};
  // The player's shot, fired by a bot: same fire rate, spread model, bloom,
  // recoil climb and pattern, damage and hitscan, all off the live config. This
  // used to be a second copy of the algorithm with the difficulty tier supplying
  // its own rpm and a single flat spread number — bots could not bloom, could
  // not climb, and never noticed a weapon config change.
  gun_shoot(bi + 1, &b->wp, o, b->yaw, b->pitch, b->vel, b->grounded,
            &g_rng_bot, &b->anim);
}

// Harness puppet: drives one bot with player-style movement intents so every
// locomotion animation (run all directions, turn, crouch, jump, slide) can be
// exercised deterministically from scripts and filmed with cambot.
// THE SLIDE-END TEST AND THE GROUND/AIR DISPATCH, ONE COPY. The bot tick and the
// puppet tick must not be able to disagree about what a slide is — they have
// diverged twice before (a third copy of the movement numbers, and a slide
// cooldown that elapsed at 2x for a bot). The only thing that differs is the
// walk speed, so that is a parameter and not a flag. Returns the new planar
// speed, which only a slide step changes.
static float mv_locomote(bot_t *b, float wx, float wz, float wlen,
                         float sp2d, float walk_sp) {
  if (b->sliding && mv_slide_should_end(b->grounded, 1, sp2d)) b->sliding = 0;
  if (!b->grounded) { mv_air(&b->vel, wx, wz, wlen, sp2d); return sp2d; }
  if (b->sliding) return mv_slide_step(&b->vel, sp2d, wx, wz, wlen);
  mv_walk(&b->vel, wx, wz, wlen, walk_sp);
  return sp2d;
}

static void bot_puppet_tick(bot_t *b, int bi) {
  float diff = wrap_pi(b->pup_face - b->yaw);
  float mt = 5.5f * TICK_DT;
  b->yaw += f_clamp(diff * 9.0f * TICK_DT, -mt, mt);
  // Same lean ramp the player's sim runs — the testbed exists to film the gait
  // the game actually plays, and a lean eased at a different rate than the
  // player's is exactly the kind of second copy this file keeps deleting.
  {
    float r = fabsf(b->pup_lean_t) > fabsf(b->pup_lean) ? LEAN_IN_RATE : LEAN_OUT_RATE;
    b->pup_lean += (b->pup_lean_t - b->pup_lean) * ease_k(r);
  }

  if (b->pup_jump) {
    b->pup_jump = 0;
    if (b->grounded) b->vel.y = mv_jump_speed();
  }
  // The puppet drives the PLAYER's locomotion too. It used to carry a third copy
  // of the movement numbers (its own slide boost, decay, exit threshold, accel and
  // crouch scale) — and since the puppet is what every animation sequence and
  // every `kine`/`skel` contact test is filmed with, that meant the animation
  // battery was validating a gait the game never runs. Scripts still set the walk
  // speed in m/s; everything else comes from the shared helpers.
  float ml = sqrtf(b->pup_mx * b->pup_mx + b->pup_mz * b->pup_mz);
  float wx = ml > 0.01f ? b->pup_mx / ml : 0.0f;
  float wz = ml > 0.01f ? b->pup_mz / ml : 0.0f;
  float wlen = ml > 0.01f ? 1.0f : 0.0f;
  float sp2d = sqrtf(b->vel.x * b->vel.x + b->vel.z * b->vel.z);
  if (b->slide_cd > 0) b->slide_cd--;
  if (b->pup_slide) {
    b->pup_slide = 0;
    if (!b->sliding && b->grounded) {
      // Same launch as the player's: steer into the travel, take the shared boost.
      if (wlen < 0.5f) { wx = sinf(b->yaw); wz = -cosf(b->yaw); wlen = 1.0f; }
      float boost = mv_slide_boost();
      // The standstill teleport is testbed-ONLY and stays local: a script may
      // `puppet slide` from a dead stop, which the shared launch (a multiplier
      // on the speed you already had) cannot express, and the shared path must
      // never assign a velocity — that is the teleport a human has no access to.
      // KNOWN, deliberately left alone here: `sp2d = boost` understates the
      // launch for the one case a script can reach and the game cannot (a puppet
      // already moving faster than SLIDE BOOST), so the mv_slide_step below sees
      // the boost rather than the kept speed. Fixing it moves every filmed slide
      // frame, so it wants its own animation-battery pass, not this refactor.
      if (sp2d < 0.05f) { b->vel.x = wx * boost; b->vel.z = wz * boost; }
      else mv_slide_launch(&b->vel, sp2d);
      sp2d = boost;
      b->sliding = 1;
      b->slide_cd = SLIDE_COOLDOWN_TICKS;
    }
  }
  float pup_sp = b->pup_crouch ? fminf(b->pup_speed, g_sim_mv[MV_CROUCH_SP])
                               : b->pup_speed;
  sp2d = mv_locomote(b, wx, wz, wlen, sp2d, pup_sp);
  b->vel.y -= GRAVITY * TICK_DT;
  b->grounded = 0;
  // The puppet's crouch is REAL, not visual: collider and hitbox eye follow
  // the stance exactly as bot_tick's do, or every proof staged on a crouched
  // puppet validates a standing collider nobody rendered.
  b->crouch = b->pup_crouch;
  float pup_eye_tgt = b->crouch ? EYE_CROUCH : EYE_STAND;
  b->eye += (pup_eye_tgt - b->eye) * f_clamp(EYE_LERP_RATE * TICK_DT, 0, 1);
  ent_move(&b->pos, &b->vel, &b->grounded,
           b->crouch ? HEIGHT_CROUCH : HEIGHT_STAND);
  arena_clamp(&b->pos);
  anim_tick(&b->anim, b->pos, b->vel, b->yaw, b->pitch, b->grounded,
            b->pup_crouch, 0.0f, b->sliding, b->wp.ads_s, b->pup_lean,
            b->pup_ready, bi + 1);  // REAL entity id (A.1): hardwired 1, every
                                    // puppet != bot 0 pushed EV_STEP/EV_LAND
                                    // with .src = 1 and stepped from the wrong
                                    // body — no tuck, see bot_tick
}

// Tactical cover: the direction that puts geometry between this bot and its
// target. Probes a ring of candidate steps and keeps the nearest one whose line
// to the target is BLOCKED — so "take cover" is a real geometric claim about the
// arena, not a fixed retreat vector. Only called when the bot has a reason to
// break the line (empty mag, or hurt and low), and a FAILED probe backs off
// for cov_retry ticks: it is a couple of dozen ray casts, and re-running all
// of them every tick for a whole reload (98.8% of probes fail at 20 bots —
// often no cover ring answers) made this path a quarter of all world traces
// while a 12 m/s bot moves 10 cm/tick against the ring's 2.6 m radius.
static int bot_cover_spot_point(const bot_t *b, v3 threat_point, v3 *out) {
  for (int r = 0; r < 2; r++) {          // nearest ring first: reachable beats ideal
    float rad = r ? 5.0f : 2.6f;
    for (int k = 0; k < 8; k++) {
      // The ring STARTS at cov_phase, which the stuck escape advances. Without
      // it the scan is deterministic in the bot's own frame, so a bot that
      // failed to reach a spot re-probed, got the identical point back and
      // walked into the same wall — the commitment was not the problem, the
      // candidate was.
      float a = (float)((k + b->cov_phase) & 7) * F_PI_4;
      v3 c = {{b->pos.x + sinf(a) * rad, b->pos.y, b->pos.z + cosf(a) * rad}};
      if (fabsf(c.x) > ARENA_HALF - 1.0f || fabsf(c.z) > ARENA_HALF - 1.0f) continue;
      if (!aabb_fits(c, PLAYER_RADIUS, HEIGHT_STAND)) continue;
      v3 eo = {{c.x, c.y + EYE_STAND, c.z}};
      if (v3_len(v3_sub(threat_point, eo)) <= 0.5f) continue;
      if (los_clear(eo, threat_point)) continue;  // still exposed
      *out = c;
      return 1;
    }
  }
  return 0;
}

static int bot_cover_spot(const bot_t *b, v3 *out) {
  return bot_cover_spot_point(b, ent_aimpoint(b->tgt), out);
}

// The COVER LEG of bot steering, one copy for both branches that can want it
// (a visible target, and a target the bot only believes in). The need test, the
// commit rule, the 0.9 m arrival radius and the 10-tick re-probe cooldown are
// one quantity each — split across two call sites they drifted silently, which
// is exactly the failure the one-copy rule exists to prevent.
//
// `threat` NULL = cover from the bot's own target (the probe is lazy: an
// aimpoint is only taken when a probe actually runs), otherwise cover from that
// point. Writes *wx/*wz/*pace ONLY while approaching; on arrival it clears
// b->in_cover so the caller's range-keeping branch picks the bot up in the same
// tick. RETURNS whether the bot was in cover mode at all this tick, which is
// what the caller must branch on — b->in_cover alone cannot tell "arrived" from
// "never wanted cover", and the two hand the bot to different branches.
static int bot_cover_steer(bot_t *b, const v3 *threat,
                           float *wx, float *wz, float *pace) {
  int need_cover = b->wp.reload_t > 0 || (b->hurt_t > 0 && b->hp < 45);
  if (!need_cover) b->cov_valid = 0;
  // COMMIT to one spot. Re-probing every few ticks and walking the current best
  // direction makes a bot dither between two equally good pieces of cover: one
  // measured 2 s to cross 2.3 m. A person picks a wall and goes to it.
  if (need_cover && !b->cov_valid && b->cov_retry == 0) {
    b->cov_valid = threat ? bot_cover_spot_point(b, *threat, &b->cov_p)
                          : bot_cover_spot(b, &b->cov_p);
    if (!b->cov_valid) b->cov_retry = 10;    // ~83 ms; the arena barely moved
  }
  b->in_cover = need_cover && b->cov_valid;
  if (!b->in_cover) return 0;
  float cdx = b->cov_p.x - b->pos.x, cdz = b->cov_p.z - b->pos.z;
  float cl = sqrtf(cdx * cdx + cdz * cdz);
  if (cl < 0.9f) {
    // Arrived: hold here, the line is already broken, and hand the bot to the
    // pre-aim/peek branch at the call site.
    b->cov_valid = 0;
    b->in_cover = 0;
    // ...but do not re-arm the probe on the very NEXT tick. Clearing cov_valid
    // alone put a bot standing in its own cover through bot_cover_spot (a
    // couple of dozen ray casts) every tick for the whole reload. Holding
    // cov_valid instead was worse in the other direction — measured, it took
    // cover_pct from 8.0 to 1.0 at seed 1337, i.e. the tactic nearly stopped
    // firing. The cooldown the FAILED-probe path already uses is the right
    // lever: same behaviour, one probe every 10 ticks instead of 120/s.
    b->cov_retry = 10;
    return 1;
  }
  *wx = cdx / cl;
  *wz = cdz / cl;
  *pace = 1.0f;
  return 1;
}

// THE SWING ANCHOR. A flank without a navmesh is one decision — "go stand over
// THERE instead of trading from here" — and the only thing that makes it read
// as clever rather than as a wide strafe is WHERE there is. Three candidates,
// swung 54/80/106 degrees around the TARGET from the bearing the bot is on now,
// at the range the duel is already being fought at (clamped to 8..14 m so a
// point-blank fight does not produce a 2 m shuffle and a 25 m one does not send
// the bot to the far wall). A candidate that the target CANNOT currently see
// wins outright: arriving out of the target's view is the whole point, and it
// is the difference between reappearing at a new angle and jogging across the
// open. The shallower swing breaks ties — the cheapest flank that still works.
// One rotation, no atan2: the bearing is the vector target->bot, rotated.
static int bot_flank_spot(const bot_t *b, v3 tpos, int side, v3 *out) {
  float dx = b->pos.x - tpos.x, dz = b->pos.z - tpos.z;
  float r = sqrtf(dx * dx + dz * dz);
  if (r < 1.0f) return 0;
  float ux = dx / r, uz = dz / r;
  float rad = f_clamp(r, 8.0f, 14.0f);
  v3 te = {{tpos.x, tpos.y + EYE_STAND - ENT_CHEST_OFF, tpos.z}};
  static const float PHI[3] = {0.95f, 1.40f, 1.85f};
  int have = 0;
  float best = -1.0e30f;
  for (int k = 0; k < 3; k++) {
    float cs = cosf(PHI[k]), sn = sinf(PHI[k]) * (float)side;
    v3 c = {{tpos.x + (ux * cs - uz * sn) * rad, 0.0f,
             tpos.z + (ux * sn + uz * cs) * rad}};
    if (fabsf(c.x) > ARENA_HALF - 1.0f || fabsf(c.z) > ARENA_HALF - 1.0f) continue;
    if (!aabb_fits(c, PLAYER_RADIUS, HEIGHT_STAND)) continue;
    v3 ce = {{c.x, c.y + EYE_STAND, c.z}};
    float score = los_clear(ce, te) ? 0.0f : 100.0f;
    score -= (float)k;
    if (!have || score > best) { best = score; *out = c; have = 1; }
  }
  return have;
}

// One release, so every path that ends a swing also pays for the next one.
static void bot_flank_release(bot_t *b) {
  b->flank_t = 0;
  b->flank_cd = (int)rng_rangef(&g_rng_bot, 600.0f, 1400.0f);
}

// The flank's CLOCKS, at the top of the tick beside slide_cd and prefire_cd
// rather than inside the steering leg — the same lesson slide_cd already
// carries, and for a sharper reason. The leg only runs while a bot is ENGAGING,
// so timers living in it stop the moment the bot takes cover, loses sight or
// turns to a noise: a cooldown claimed before a twenty-second lull would never
// expire (flanking quietly decays out of a long match), and a commitment
// interrupted by a lost target would FREEZE and resume later against whoever
// the bot found next — walking to an anchor planned around somebody else, which
// is the "the bot ran off to nowhere" tell. Nothing here decides anything; the
// decision stays in the leg.
static void bot_flank_tick(bot_t *b) {
  if (b->flank_cd > 0) b->flank_cd--;
  if (b->flank_t == 0) return;
  int abort = b->wp.reload_t > 0 || (b->hurt_t > 0 && b->hp < 45) || b->sliding;
  if (--b->flank_t == 0 || abort) bot_flank_release(b);
}

// The WALK half, split out because TWO branches can own a committed swing. The
// decision needs a visible target; finishing the move does not — the anchor is
// a fixed world point, so a bot whose swing takes it behind a wall carries on
// to it and comes out at the new angle, which is the entire tactic. Driving it
// only from the engaging branch meant every swing that broke line of sight
// silently stopped being walked while its timer kept running: measured at seed
// 1337, 8 bots hard, 20 commitments produced 5 arrivals, and 12 of the 15
// failures were "lost sight, stood in the search behaviour until the clock ran
// out". A flank nobody finishes is wandering with a goal.
static int bot_flank_walk(bot_t *b, float *wx, float *wz, float *pace) {
  if (b->flank_t == 0) return 0;
  float dx = b->flank_p.x - b->pos.x, dz = b->flank_p.z - b->pos.z;
  float l = sqrtf(dx * dx + dz * dz);
  if (l < 1.4f) {   // arrived: hand the bot back to the duel from the new angle
    b->flank_arrived++;
    bot_flank_release(b);
    return 0;
  }
  *wx = dx / l;
  *wz = dz / l;
  *pace = 1.0f;
  return 1;
}

static void bot_deadlock_release(bot_t *b) {
  b->dl_t = 0;
  b->dl_cd = (int)rng_rangef(&g_rng_bot, 240.0f, 600.0f);
}

// Its clock, at the top of the tick with the others.
static void bot_deadlock_tick(bot_t *b) {
  if (b->dl_cd > 0) b->dl_cd--;
  if (b->dl_t > 0 && --b->dl_t == 0) bot_deadlock_release(b);
}

// THE DEADLOCK LEG. Runs BEFORE cover and the flank, because when a bot is
// welded to a corner the thing that is wrong is usually the destination one of
// those two chose — overriding them is the point. One decision, from the
// situation the player can read off the bot, and then it is committed: a
// healthy bot with rounds in the gun at conversational range walks IN, and
// anything else leaves and rebuilds the angle. The wall-follow further down
// still applies, which is what lets a PUSH come around the corner instead of
// pressing on it.
static int bot_deadlock_steer(bot_t *b, float tdist, float *wx, float *wz,
                              float *pace) {
  if (b->dl_t == 0 && b->dl_cd == 0 && b->pin > BOT_DEADLOCK_PIN &&
      b->tgt >= 0) {
    int can_push = b->hp >= BOT_PUSH_HP && b->wp.reload_t == 0 &&
                   b->wp.ammo[b->wp.cur] > 0 && tdist <= BOT_PUSH_DIST;
    if (can_push) {
      b->dl_p = b->last_seen;
      b->dl_push = 1;
      b->dl_t = (int)rng_rangef(&g_rng_bot, 90.0f, 150.0f);
    } else if (bot_cover_spot_point(b, ent_aimpoint(b->tgt), &b->dl_p)) {
      b->dl_push = 0;
      b->dl_t = (int)rng_rangef(&g_rng_bot, 90.0f, 150.0f);
    } else {
      b->dl_cd = 60;   // nowhere to break to; do not re-probe every tick
    }
  }
  if (b->dl_t == 0) return 0;
  float dx = b->dl_p.x - b->pos.x, dz = b->dl_p.z - b->pos.z;
  float l = sqrtf(dx * dx + dz * dz);
  // A push is done when it is ON him; a break is done when it reaches the spot.
  if (l < (b->dl_push ? 3.0f : 1.2f)) {
    bot_deadlock_release(b);
    return 0;
  }
  *wx = dx / l;
  *wz = dz / l;
  *pace = 1.0f;
  return 1;
}

// The FLANK LEG, shaped exactly like bot_cover_steer above and for the same
// reason: the trigger, the commitment and the release are one quantity each.
// Returns whether it owns the steering this tick; the caller keeps its
// strafe/range-keeping leg for every tick it does not.
//
// Commitment is the behaviour. A bot that re-decides every tick dithers (the
// cover leg already paid for that lesson), so a flank is claimed once, run to
// the anchor, and only abandoned for a reason a person would abandon it for:
// the magazine ran out, it got hurt badly, it went into a slide. It keeps
// FIGHTING while it moves — the trigger discipline further down fires whenever
// the aim happens to settle — because a bot that stops shooting to reposition
// reads as running away, which is the failure mode this tactic is most likely
// to produce. And it never crouches while flanking: a crouched boot carries
// 65% less, and a flank the player cannot hear coming is the one that gets
// called a spawn cheat.
static int bot_flank_steer(bot_t *b, const bot_skill_t *sk, float tdist,
                           float *wx, float *wz, float *pace) {
  // Rate, not habit. sk->tac is the one tactic knob (never a gun-handling
  // number), and 0.0007 per tick at tac 0.60 is one attempt per ~20 s of
  // engagement — with the 5..12 s cooldown that lands at a few percent of
  // bot-ticks, the same order the slide sits at.
  if (b->flank_t == 0 && b->flank_cd == 0 && b->grounded &&
      b->wp.reload_t == 0 && !(b->hurt_t > 0 && b->hp < 45) && !b->sliding &&
      tdist > 7.0f && tdist < 28.0f &&
      rng_rangef(&g_rng_bot, 0.0f, 1.0f) < sk->tac * 0.0007f) {
    // The way it was already leaning — a swing the player can read and learn
    // beats a coin flip. strafe_dir is -1/0/+1 and the plant (0) must not fold
    // into one side, or a mirror-symmetric arena gets a 60/40 bias and every
    // bot teaches the player to watch the same shoulder.
    int side = b->strafe_dir ? (b->strafe_dir > 0 ? 1 : -1)
                             : ((rng_next(&g_rng_bot) & 1) ? 1 : -1);
    // last_seen, never ent_pos(tgt): the anchor is planned ONCE, out of what
    // the bot actually observed. The two are the same value on the commit tick
    // (the trigger requires a visible target), and that is the point — the
    // property "a flank never reads live target state" is structural here
    // rather than incidental, so it survives the next person editing the gate.
    if (bot_flank_spot(b, b->last_seen, side, &b->flank_p))
      b->flank_t = (int)rng_rangef(&g_rng_bot, 220.0f, 460.0f);
  }
  return bot_flank_walk(b, wx, wz, pace);
}

// The remembered aim point — belief_pos DISPLACED BY THE UNCERTAINTY that the
// dead reckoning has already been accumulating. belief_radius was computed,
// grown and capped and then never read as anything but a boolean gate, so a
// player who broke line of sight was pre-aimed and pre-fired at with exactly
// zero positional error for the whole memory window. This is the one chokepoint
// every consumer already shares (pre-aim, pre-fire, the cover probe's threat
// point, the search anchor scoring), so the error lands in all four at once and
// the radius becomes load-bearing instead of decorative. The direction is
// frozen at the moment sight broke (bot_belief_observe rolls it while the
// target is still visible), which is what keeps the aim from shivering: the bot
// is wrong in ONE direction and stays wrong that way, like a person is.
static v3 bot_belief_aimpoint(const bot_t *b) {
  return (v3){{b->belief_pos.x + sinf(b->belief_err_a) * b->belief_radius,
               b->belief_pos.y + EYE_STAND - ENT_CHEST_OFF,
               b->belief_pos.z + cosf(b->belief_err_a) * b->belief_radius}};
}

static v3 bot_search_clamped(v3 p) {
  p.y = 0.0f;
  p.x = f_clamp(p.x, -ARENA_HALF + PLAYER_RADIUS, ARENA_HALF - PLAYER_RADIUS);
  p.z = f_clamp(p.z, -ARENA_HALF + PLAYER_RADIUS, ARENA_HALF - PLAYER_RADIUS);
  return p;
}

static void bot_search_candidates(const bot_t *b, v3 cand[6]) {
  v3 fwd = {{b->last_seen_vel.x, 0.0f, b->last_seen_vel.z}};
  float sp2 = fwd.x * fwd.x + fwd.z * fwd.z;
  if (sp2 > 0.25f * 0.25f) {
    fwd = v3_scale(fwd, 1.0f / sqrtf(sp2));
  } else {
    fwd = (v3){{b->belief_pos.x - b->pos.x, 0.0f,
                b->belief_pos.z - b->pos.z}};
    float len2 = fwd.x * fwd.x + fwd.z * fwd.z;
    fwd = len2 > 1e-6f ? v3_scale(fwd, 1.0f / sqrtf(len2))
                       : (v3){{sinf(b->yaw), 0.0f, -cosf(b->yaw)}};
  }
  v3 left_vec = {{-fwd.z, 0.0f, fwd.x}};
  v3 right_vec = {{fwd.z, 0.0f, -fwd.x}};
  v3 first_side = b->search_dir >= 0 ? left_vec : right_vec;
  v3 second_side = b->search_dir >= 0 ? right_vec : left_vec;
  const float side_step = 4.0f;
  const float close_step = 2.8f;
  const float long_step = 5.6f;
  cand[0] = b->belief_pos;
  cand[1] = b->last_seen;
  cand[2] = v3_add(b->last_seen, v3_scale(first_side, side_step));
  cand[3] = v3_add(b->last_seen, v3_scale(second_side, side_step));
  cand[4] = v3_add(b->last_seen, v3_scale(fwd, close_step));
  cand[5] = v3_add(b->last_seen, v3_scale(fwd, long_step));
  for (int i = 0; i < 6; i++) cand[i] = bot_search_clamped(cand[i]);
}

static unsigned bot_search_valid_mask(const v3 cand[6], const int fits[6]) {
  unsigned mask = 0;
  for (int i = 0; i < 6; i++) {
    if (!fits[i]) continue;
    int duplicate = 0;
    for (int j = 0; j < i; j++) {
      if (!(mask & (1u << j))) continue;
      v3 d = v3_sub(cand[i], cand[j]);
      if (v3_dot(d, d) <= BOT_SEARCH_EPS * BOT_SEARCH_EPS) {
        duplicate = 1;
        break;
      }
    }
    if (!duplicate) mask |= 1u << i;
  }
  return mask;
}

// No candidate survived the filter: search the believed position itself. Both
// dead ends in bot_search_plan_next take exactly this exit.
static void bot_search_fallback(bot_t *b, const bot_skill_t *sk) {
  b->search_pos = bot_search_clamped(b->belief_pos);
  b->search_t = sk->search_ticks;
  b->search_index = 0;
  b->search_visited = 0;
}

static void bot_search_plan_next(bot_t *b, const bot_skill_t *sk) {
  v3 cand[6];
  bot_search_candidates(b, cand);
  int fits[6];
  for (int i = 0; i < 6; i++)
    fits[i] = aabb_fits(cand[i], PLAYER_RADIUS, HEIGHT_STAND);
  unsigned valid = bot_search_valid_mask(cand, fits);
  if (!valid) {
    bot_search_fallback(b, sk);
    return;
  }
  if (!(valid & ~b->search_visited)) {
    // The finite set is exhausted. Start the next sweep with the side order
    // reversed; within a sweep search_dir stays fixed so mask bits keep their
    // physical meaning.
    b->search_visited = 0;
    b->search_dir = -b->search_dir;
    v3 tmp = cand[2];
    cand[2] = cand[3];
    cand[3] = tmp;
    int tmp_fit = fits[2];
    fits[2] = fits[3];
    fits[3] = tmp_fit;
    valid = bot_search_valid_mask(cand, fits);
  }
  v3 threat_point = bot_belief_aimpoint(b);
  v3 prev_dir = v3_sub(b->search_pos, b->pos);
  prev_dir.y = 0.0f;
  float prev_len2 = prev_dir.x * prev_dir.x + prev_dir.z * prev_dir.z;
  int have_prev = prev_len2 > 0.25f;
  if (have_prev) prev_dir = v3_scale(prev_dir, 1.0f / sqrtf(prev_len2));

  int best_i = -1, los_calls = 0;
  float best_score = -1.0e30f;
  for (int i = 0; i < 6; i++) {
    if (!(valid & (1u << i)) || (b->search_visited & (1u << i))) continue;
    v3 anchor_eye = {{cand[i].x, cand[i].y + EYE_STAND, cand[i].z}};
    v3 to_threat = v3_sub(threat_point, anchor_eye);
    float threat_d2 = v3_dot(to_threat, to_threat);
    int opens_los = 0, has_cover = 0;
    if (threat_d2 > 0.25f && los_calls < 16) {
      opens_los = los_clear(anchor_eye, threat_point);
      los_calls++;
      has_cover = !opens_los;
    }
    v3 to_anchor = v3_sub(cand[i], b->pos);
    float dist2 = to_anchor.x * to_anchor.x + to_anchor.z * to_anchor.z;
    float novelty = 0.0f;
    // dist2 IS this candidate's 2D length; it used to be computed a second
    // time into `anchor_len2` from the identical expression.
    if (have_prev && dist2 > 1e-6f) {
      v3 nd = v3_scale((v3){{to_anchor.x, 0.0f, to_anchor.z}},
                       1.0f / sqrtf(dist2));
      novelty = (1.0f - f_clamp(v3_dot(prev_dir, nd), -1.0f, 1.0f)) * 40.0f;
    }
    float score = (opens_los ? 1000.0f : 0.0f) + (has_cover ? 80.0f : 0.0f) +
                  novelty - dist2 * 0.02f;
    if (best_i < 0 || score > best_score + BOT_SEARCH_EPS ||
        (fabsf(score - best_score) <= BOT_SEARCH_EPS && i < best_i)) {
      best_score = score;
      best_i = i;
    }
  }

  if (best_i < 0) {
    bot_search_fallback(b, sk);
    return;
  }
  b->search_pos = cand[best_i];
  b->search_t = sk->search_ticks;
  b->search_visited |= 1u << best_i;
  b->search_index = (best_i + 1) % 6;
}

static void bot_belief_clear(bot_t *b) {
  // A finished fight leaves something behind. This is the ONE place a target
  // stops being a target — lost or killed — so it is where the alert window is
  // armed, aimed at the last place the bot actually knew about. Without it the
  // bot dropped straight into patrol the tick its memory expired: rifle slung,
  // strolling out of its own cover four and a half seconds after a firefight.
  if (b->last_seen_tick) {
    b->alert_p = b->last_seen;
    b->alert_t = BOT_ALERT_TICKS;
  }
  b->belief = BOT_BELIEF_VISIBLE;
  b->last_seen_vel = (v3){{0, 0, 0}};
  b->belief_pos = (v3){{0, 0, 0}};
  b->belief_radius = 0.0f;
  b->last_seen_tick = 0;
  bot_focus_invalidate(b);
  b->search_dir = 1;
  b->reacquire_count = 0;
  b->threat_src = -1;
  b->threat_t = 0;
  b->threat_score = 0;
}

static void bot_belief_observe(bot_t *b, int e) {
  v3 pos = ent_pos(e);
  v3 vel = {{0, 0, 0}};
  if (b->belief == BOT_BELIEF_VISIBLE && b->last_seen_tick > 0 &&
      g_tick > b->last_seen_tick) {
    float inv_dt = 1.0f / ((float)(g_tick - b->last_seen_tick) * TICK_DT);
    vel.x = (pos.x - b->last_seen.x) * inv_dt;
    vel.z = (pos.z - b->last_seen.z) * inv_dt;
    float speed = sqrtf(vel.x * vel.x + vel.z * vel.z);
    float max_speed = g_sim_mv[MV_RUN];
    if (speed > max_speed && speed > 1e-6f) {
      float s = max_speed / speed;
      vel.x *= s;
      vel.z *= s;
    }
  }
  // The error angle is rolled inside the seed, on the LAST tick this bot can
  // still see, and then frozen for the whole memory window.
  b->belief = BOT_BELIEF_VISIBLE;
  bot_belief_seed(b, pos, vel);
  b->search_t = 0;
}

static void bot_belief_predict(bot_t *b) {
  float speed = sqrtf(b->last_seen_vel.x * b->last_seen_vel.x +
                      b->last_seen_vel.z * b->last_seen_vel.z);
  b->belief_pos.x += b->last_seen_vel.x * TICK_DT;
  b->belief_pos.z += b->last_seen_vel.z * TICK_DT;
  b->belief_radius += 0.015f + speed * TICK_DT * 0.20f;
  if (b->belief_radius > 4.5f) b->belief_radius = 4.5f;
}

typedef struct {
  int ent;
  float score, dist;
} bot_target_choice_t;

static float bot_target_score(const bot_t *b, int e, float dist, float align) {
  float fov01 = f_clamp((align - BOT_FOV_COS) / (1.0f - BOT_FOV_COS),
                        0.0f, 1.0f);
  float score = 1000.0f / (1.0f + dist) + 350.0f * fov01;
  if (b->threat_t > 0 && b->threat_src == e)
    score += 650.0f + 70.0f * (float)b->threat_score;
  if (b->tgt == e) score += BOT_TARGET_HOLD_BONUS;
  return score;
}

static int bot_target_better(bot_target_choice_t a, bot_target_choice_t b) {
  if (a.ent < 0) return 0;
  if (b.ent < 0) return 1;
  if (a.score > b.score + BOT_TARGET_EPS) return 1;
  if (b.score > a.score + BOT_TARGET_EPS) return 0;
  if (a.dist < b.dist - BOT_TARGET_EPS) return 1;
  if (b.dist < a.dist - BOT_TARGET_EPS) return 0;
  return a.ent < b.ent;
}

static int bot_target_breaks_hidden(const bot_t *b, bot_target_choice_t c) {
  if (c.ent < 0) return 0;
  if (b->threat_t > 0 && b->threat_src == c.ent) return 1;
  return c.score > BOT_TARGET_HOLD_BONUS + BOT_TARGET_HIDDEN_BREAK_MARGIN;
}

[[maybe_unused]] static const char *bot_belief_label(const bot_t *b) {
  if (!b->alive || b->tgt < 0) return "patrol";
  switch (b->belief) {
    case BOT_BELIEF_VISIBLE: return "visible";
    case BOT_BELIEF_OCCLUDED_HOLD: return "hold";
    case BOT_BELIEF_INVESTIGATE: return "investigate";
    case BOT_BELIEF_SEARCH_SWEEP: return "search";
  }
  return "patrol";
}

