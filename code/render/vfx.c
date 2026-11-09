// code/render/vfx.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// VFX + dynamic scene batch. All lifetimes are TICKS, never wall-clock, so
// every effect is deterministic and capturable in headless screenshots
// (tracer visible at `wait 2`, gone by `wait 12`). VFX spawn at ev_push time
// (pure data), decay in vfx_tick, and are drawn into one STREAM_DRAW batch
// with the world shader. Impact marks persist as a ring buffer — firing a
// mag at a wall leaves a readable spray chart.
// ---------------------------------------------------------------------------

#define MAX_TRACERS   24
#define MAX_PARTICLES 1024
// The pool is SPLIT, not merely shared. An EV_IMPACT spark is FEEDBACK — it
// says where the round landed — while a skid spark is scenery, and a shared
// pool lets the loudest emitter win: measured on the old 64-slot pool at 20
// bots / hard, occupancy sat at its ceiling on every seed (peak 64, of which
// 51-59 were slide dust) and 8-14% of all impact-spark spawns were silently
// refused. Slots below PARTS_SKID_LO belong to impacts and boot dust; the skid
// emitter never scans them. Impacts still scan from 0 upward, so they get
// their own region first and spill into idle skid slots when nothing slides.
// Sized off measured demand against a deliberately oversized pool (so the
// numbers are demand, not a clamp): reserved peaks at 114, skid at 219.
// RE-MEASURED after the body burst went from 8 fragments to 22: the reserve
// ran at its 160 ceiling on every 20-bot hard seed, i.e. it had stopped being
// a guarantee and become a clamp. At 288 the same three seeds report a true
// demand of 226/268/244 and a whole-pool peak of 292/328/333 of 768, so both
// halves have real headroom again — and `budget` now prints both occupancies
// so the next person sizing this has the number instead of an argument.
// RE-MEASURED AGAIN when the kill burst added 44 more reserved slots on the
// player's own kills. At 288 the same five 20-bot hard seeds read a reserve
// peak of 288/255/234/279/242 — and that first number IS the ceiling, i.e. on
// seed 1337 the region had gone back to being a clamp, which is the exact state
// the paragraph above sized it out of. (The tell that it was the burst: the
// pre-change binary reads 257 on that seed and is byte-identical on the other
// four, because a player kill only lands on the busiest tick some of the time.)
// At 384 the same five seeds read 289/255/234/279/242, so the true demand on
// the seed that was clamped is 289 — it was only just binding, which is
// precisely why nothing about it was visible without the number — and the worst
// case now has 95 slots of headroom instead of none. The skid half keeps 384
// against a measured demand of 219, and the whole-pool peak is unmoved at
// 410/768, because the pool was never the constraint; the SPLIT was.
// RE-MEASURED AGAIN when the world impact went from 8 fragments to 18 and
// acquired a contact flare. On the same five 20-bot hard seeds the reserved
// region ran 322/343/347 of 384 — 90% of a region whose whole purpose is that a
// bullet impact's feedback can never be refused, i.e. one busy tick from being
// a clamp again, which is the exact state the two paragraphs above sized it out
// of twice. 448 of 896 restores ~100 slots of headroom on the worst seed; the
// skid half keeps its own against a measured demand of 219, and the pool costs
// 44 bytes a slot. THE SPLIT IS THE CONSTRAINT, NOT THE POOL, and it took a
// 31-SEED sweep to see it: a three-seed soak said 337-375 of 448, but seeds
// 31337 and 88 run the reserved region to 447 and 445 of 448 — one and three
// free slots, i.e. straight back to the clamp this paragraph exists to prevent,
// on a region whose whole purpose is that a bullet impact's feedback can never
// be refused. Measured on the same sweep the WHOLE pool never exceeds 557 of
// 1024 and the skid half's true demand is ~200, so the fix is not more pool but
// a different division of it: at 640/1024 the same worst seeds read 447 and 445
// of 640 (193 and 195 free) while the skid half keeps 384 against that 200.
// Sweep more than three seeds before believing a headroom number here.
#define PARTS_SKID_LO 640
// The two numbers above are coupled and far apart in the file: if the split ever
// climbs past the pool, both skid loops become no-ops and the whole effect
// disappears silently, with nothing to see but an absent spark.
static_assert(PARTS_SKID_LO < MAX_PARTICLES, "skid region must be inside the pool");
#define MAX_MARKS     64
#define MAX_FLASHES   8
#define TRACER_TICKS  19     // ceiling on a bolt's life: the arena diagonal
// LONG, AND PROPORTIONAL TO THE SHOT, because the shooter watches it from behind.
// Past the first couple of metres a round runs nearly down the view axis: filmed,
// a fixed 9 m streak on a 17 m shot spanned 25 px beside the crosshair and read as
// nothing. So the nose flies out from the barrel, the tail lets go only when the
// nose is most of the way there, and then it runs in to the impact. Read from the
// gun: a streak leaving it, reaching the target, and collapsing into the hit.
#define TRACER_LEN    9.0f   // the shortest streak, metres
#define TRACER_SPAN   0.85f  // ...and as a fraction of the shot
// THE BOLT IS SLOWED FOR THE EYE. At a true 900 m/s a 17 m shot is over in 2.3
// ticks, which is at most one rendered frame at 60 Hz: filmed at that speed the
// round simply never appeared. 3.4 m/tick (408 m/s) is what a tracer looks like
// to a human, and the pistol runs a little under it. Nothing in the sim reads
// this; the hit has already been decided on the same line.
#define TRACER_V      3.4f
#define FLASH_TICKS   3
// THE ROUND HAS A CHARACTER PER WEAPON. One streak for all three guns meant a
// pistol round, a 5.56 burst and a .338 crossing the map all drew the same
// orange line at the same speed, so the only way to tell what was shooting at
// you was the sound. What differs here is what differs in the world: the
// rifle round is quick and thin, the magnum is faster, hotter, wider and
// leaves a HEAT WAKE hanging on its line for half a second (the shimmer a
// heavy round leaves in the air, and the read that tells a man where the
// sniper is), and the pistol's slow, fat, short and warm. All of it is the
// EYE's: the hit is still decided on the same line, on the tick it was fired.
// `v` is display m/tick (TRACER_V, still the AR's: faster and the tail lets go
// of the barrel before a frame has shown it there — the first two metres are
// most of the streak's length ON SCREEN. The magnum is quicker and pays for it
// with a span that holds the tail on the barrel the whole flight), `len`
// and `span` are TRACER_LEN / TRACER_SPAN per weapon, `w` and `k` scale width
// and brightness, the colours run bore -> 14 m downrange, `wake` is ticks.
typedef struct {
  float v, len, span, w, k;
  v3 hot, cool, hhot, hcool;
  int wake;
} tracer_look_t;
static const tracer_look_t TRACER_LOOK[3] = {
  [WPN_AR] = {3.4f, 9.0f, 0.85f, 1.00f, 1.00f,
              {{1.65f, 1.36f, 0.92f}}, {{1.10f, 0.50f, 0.14f}},
              {{0.46f, 0.28f, 0.11f}}, {{0.22f, 0.09f, 0.03f}}, 0},
  [WPN_SR] = {5.2f, 14.0f, 1.00f, 1.35f, 1.30f,
              {{1.90f, 1.78f, 1.48f}}, {{1.45f, 0.86f, 0.36f}},
              {{0.52f, 0.40f, 0.24f}}, {{0.30f, 0.15f, 0.06f}}, 66},
  [WPN_PT] = {2.8f, 6.0f, 0.80f, 0.90f, 0.90f,
              {{1.60f, 1.12f, 0.58f}}, {{1.00f, 0.36f, 0.08f}},
              {{0.44f, 0.22f, 0.07f}}, {{0.20f, 0.06f, 0.02f}}, 0},
};
static const tracer_look_t *tracer_look(int wpn) {
  return &TRACER_LOOK[wpn == WPN_SR ? WPN_SR : wpn == WPN_PT ? WPN_PT : WPN_AR];
}
// How long each gun's muzzle star burns. The brake on the magnum throws a
// blast that is visibly longer than the rifle's flash hider lets out.
static int flash_ticks(int wpn) { return wpn == WPN_SR ? 5 : FLASH_TICKS; }
// RICOCHETS. A round meeting a hard face at a glancing angle does not stop in
// it, it skips — and a tracer that visibly skips off the wall beside a man is
// the clearest read in the genre that a round went THERE and was not a hit.
// Purely visual (the sim already stopped the round), delayed to the moment the
// bolt arrives, and hashed rather than drawn from g_rng_vfx so the scatter of
// every other effect is unchanged.
#define RICO_COS      0.42f   // |cos| to the face below which a round may skip
#define RICO_LEN      9.0f    // the furthest a skip is drawn, metres
// 14 and 26, not 10 and 20 (117 ms / 217 ms). The marker is decremented on the
// tick it is armed, so 10 rendered as 9 ticks = 75 ms — three frames at 60 fps
// and one and a half at the fps_cap of 30 the menu offers. It also now spends
// its first third on a POP, so the settled shape needs a life of its own.
#define HITMARK_TICKS 14
#define KILLMARK_TICKS 26
// THE MARKER IS ARMED ON A DELAY, and it is the same trick — for the same
// measured reason — that the audio layer already applies with wpn_def_t.sfx_hit_t.
// Measured at 1.5 m on an AR body hit, the background luminance inside the 48x48
// px the marker occupies runs L=104/130/90/52/31 on ticks +1..+5, with 614 and
// 655 of its own 2304 pixels BRIGHTER than its white core on the first two — so
// the confirmation was being armed exactly on the two frames its own impact
// spark peaks, and measured +0.34 stops of contrast there against +2.62 stops at
// 8 m where the spark is small. Waiting out the same offset the tick already
// waits puts it on a background 4.2x darker.
#define HITMARK_ARM_MIN 2      // ticks: never zero, or a contact hit is swamped
#define PARTICLE_TICKS 26
// A round landing on a MAN throws a burst too, and it is a different material
// from a rock chip: fragments a body stopped come BACK along the round rather
// than off a surface normal, they are gone in a tenth of a second, and they are
// redder. Same integrator as the world spark (arc hard, never bounce — a body
// is not a floor), different numbers and a different draw.
// 18, NOT 8, and the number is a measurement rather than taste. A wall hit at
// 8 m painted 150-200 px against a body hit's 462 and a kill burst's 216 px of
// vertical extent — the effect the player triggers on EVERY MISSED ROUND, i.e.
// the one he sees most often in the whole game, was the smallest of the three.
// Impacts are the exact traffic the reserved region below PARTS_SKID_LO exists
// to guarantee (see that block for the current split and measured occupancy),
// so 10 more fragments x 26 ticks is inside what impact_free_slot can always
// find. Re-measure `budget` after touching this.
#define IMPACT_SPARKS  18      // per burst off WORLD geometry: a stone chip
#define HITSPARK_N     22      // ...and off a MAN. Nearly three times the stone
                               // burst, because a round stopping in armour and
                               // webbing is the most violent thing that happens
                               // in a firefight and the one the player is
                               // playing FOR — at 8 it read as a flicker at any
                               // range past a couple of metres. Sized against
                               // the pool rather than guessed — see the
                               // PARTS_SKID_LO block for the CURRENT reserve
                               // split and its measured occupancy (that block
                               // is kept current; this one only claims the
                               // burst fits inside what impact_free_slot can
                               // always find).
#define HITSPARK_TICKS 20      // must OUTLIVE TRACER_TICKS (8): the streak's
                               // tail collapses onto this exact point and owns
                               // the first few ticks, so a shorter burst is
                               // buried inside the tracer that caused it
#define HITSPARK_BACK  4.4f    // m/s of back-spray toward the shooter, sampled
                               // 0.55-1.0 of it (the stone chip leaves at
                               // 1.5-4.5 along a surface normal; a body absorbs)
#define HITSPARK_SIDE  2.7f    // m/s of scatter in the frame perpendicular to
                               // the ROUND, not to world axes: comparable to
                               // HITSPARK_BACK or the burst is a needle
#define HITSPARK_IN    0.020f  // m ALONG the ray, INTO the box. The hitbox is
                               // 440 x 600 mm against a 341 mm-deep carrier, so
                               // its face stands well proud of the mesh: backing
                               // the spawn OFF the box would hang the burst in
                               // open air beside the man.
#define HITSPARK_HEAD_K 1.55f  // a headshot is the SAME burst, hotter and
                               // faster — a layer, exactly as the audio's head
                               // signature is a layer on whichever base ran
#define HITSPARK_NEAR  0.50f   // m from the CAMERA under which a body spark is
                               // not drawn. The one body the camera can be
                               // inside is the player's own: the head box is
                               // +/-0.14 m about the eye and ZNEAR is 0.12, so
                               // without this a frontal headshot paints the
                               // crosshair with additive orange.
#define HITSPARK_FADE  2.60f   // ...and where that ease is complete. A target's
                               // head at contact range is 0.6-1.5 m out, so the
                               // blowout is not confined to the player's own
                               // body and a step at 0.5 m cannot cover it.
// Stated as a coupling above, enforced here — the same idiom, and for the same
// reason, as the skid region's assert eleven lines up: a burst that dies inside
// the tracer that caused it looks exactly like a burst that never spawned.
static_assert(HITSPARK_TICKS > TRACER_TICKS,
              "the body spark must outlive the tracer that lands on it");
// A KILL IS THE EVENT THE WHOLE GAME IS PLAYED FOR, AND IN THE WORLD IT LOOKED
// EXACTLY LIKE A GRAZE. The hitmarker and the kill feed are both SCREEN SPACE;
// out in the arena a killing round pushed the same kind-4 body spark a 26-damage
// nick does, so the one moment worth celebrating had no read at all in the place
// the player is actually looking — at the man.
//
// It is a DIFFERENT DISTRIBUTION, not a bigger one, and that is the whole reason
// it reads as a different event: kind 4 sprays BACK along the round because a
// body absorbs it, while this goes radially outward off the wound with an
// upward bias, because what it describes is a body coming apart rather than a
// round arriving. Simply doubling kind 4 in place was measured first and reads
// as a fat hit, not as a kill — the back-spray silhouette is the recognisable
// part, so more of it says "more damage" and never says "dead".
//
// THE PLAYER'S OWN KILLS ONLY, which is the split this file already makes
// everywhere else: the tracer, the muzzle star and the body spark belong to
// EVERY shooter because they describe the world, while the hitmarker and the
// damage flash are the player's alone because they are feedback about the
// player. A kill burst is feedback. It is also a budget fact — a 20-bot hard
// match produces a kill about every 32 ticks, and 44 slots of celebration on
// each of them would hold the reserved region full against the sparks whose job
// is to say where rounds are landing.
#define KILLBURST_N      44     // 2x the body spark's 22 ...
#define KILLBURST_TICKS  34     // ...living 1.7x as long. Long enough for the
                                // arc to be a shape rather than a flash, short
                                // enough that it is gone before the corpse has
                                // finished falling and stops being an event.
#define KILLBURST_SP     11.0f  // m/s peak, sampled 0.62-1.0 of it, against the
                                // body spark's 2.4-4.4. The streak length is a
                                // time constant times the SPEED, so this is
                                // most of why the burst draws lines and the
                                // hit draws dots.
// A BURST OF FIXED WORLD SIZE CANNOT READ AT BOTH ENDS OF THIS ARENA, and the
// numbers are not close: at a fixed 11 m/s the same kill measured 11.06% of the
// frame at 1.5 m — a screen-filling wash that buried the man who had just died
// — and 0.06% at 20 m, which is 305 pixels and not a cue at all. Both readings
// are the SAME defect, because what the player perceives is an ANGLE and world
// metres divide by range. The burst is therefore sized in angular terms: its
// speed, and so its whole spatial extent and its streak lengths with it, scales
// with the range the kill happened at, referenced to the distance this arena is
// mostly fought at. That is a presentation quantity and nothing physical, which
// is exactly what it should be — a kill burst is feedback about the player's
// own shot, not debris being simulated.
//
// The range comes from the wound latch's own `dist`, which is the length
// ev_push already carried: the shooter's origin IS the player's eye (weapon_fire
// fires from player_eye_pos), so |at - from| is the eye-to-wound distance with
// no new coupling to the camera and nothing re-derived.
#define KILLBURST_REF    8.0f   // m. Reference range: the burst is exactly as
                                // authored here, and this is where the arena's
                                // long sightlines put most duels.
#define KILLBURST_K_LO   0.20f  // ...and the clamps. Below this a contact kill
#define KILLBURST_K_HI   2.10f  // blows out; above it a far kill turns into a
                                // 20 m firework hanging over the arena.
#define KILLBURST_BACK   0.30f  // x speed, toward the shooter. Half of a sphere
                                // points into the body and is correctly
                                // occluded by it, so the centroid has to be
                                // moved onto the shooter's side or half the
                                // fragments are paid for and never seen.
#define KILLBURST_UP     0.34f  // x speed, world up. Gravity here is 22, so
                                // without this the burst is flat within a
                                // dozen ticks and the arc never happens.
#define KILLBURST_DRAG   0.965f // per tick. The kill burst leaves 2-4x faster
                                // than any other spark in the file, and drag is
                                // what makes that a fragment that SLOWS: at a
                                // constant speed every streak draws the same
                                // length for its whole life and the burst reads
                                // as a firework instead of as debris.
// BRIGHTNESS CANNOT FIX A COVERAGE PROBLEM, and that is worth writing down
// because the near-camera ease is the obvious place to reach for and it does
// not work. Measured in the same 120x120 box round the crosshair that caught
// the body spark's blowout, a contact kill at 1 m painted 36% of it against the
// 18.5% that was a defect worth fixing there. Widening this burst's own ease to
// 3.40 m moved that to 32.2% — nothing — because the core colour opens at 4.30
// against a display ceiling of 1.0, so two thirds of the brightness can come off
// it and every pixel it covers is still white. It also tore a hole at the other
// end: at 0.6 m the ease had already taken the burst to 3% of its level and the
// widened one made it invisible. What actually moved the number was the RANGE
// CLAMP, i.e. the geometry: KILLBURST_K_LO at 0.20 reads 18.3-19.9% at 1 m and
// leaves 8 m untouched, because the clamp stops binding at 1.6 m. So there is no
// separate constant here — the near ease is HITSPARK_NEAR/HITSPARK_FADE, shared
// with the body spark, and where the burst is culled outright stays a property
// of the camera rather than of the effect.
//
// KNOWN DELIBERATE GAP, in the file's own idiom: at ~0.6 m and closer the burst
// is all but gone (measured 0.02% of the frame), because the shared ease is at
// 5% of its level that close in and the fragments that fly at the camera are
// culled outright. That is the ease doing exactly its job — a kill inside arm's
// reach is the one that could blind the player — and at that range the
// hitmarker and the kill feed carry the confirmation on their own.
#define KILLBURST_STREAK 0.040f // s of velocity drawn as the fragment's line,
                                // against 0.030 on the body spark
#define KILLBURST_SLOW   0.30f  // ...of the peak speed is the SLOWEST fragment,
                                // against the body spark's 0.55. A narrow speed
                                // band gives every fragment the same streak
                                // length, and 44 equal-length lines radiating
                                // from one point is a sea urchin rather than
                                // debris — it was the single most artificial
                                // thing about the first tune. Widening the band
                                // leaves a dense hot core of near-stationary
                                // fragments with a few long fliers out of it,
                                // which is what a burst actually looks like,
                                // and it costs nothing.
// THE ANGULAR RANGE SCALE, now shared by all three impact bursts instead of
// living only on the kill. Same reference distance and the same clamps: what the
// player perceives is an ANGLE, and world metres divide by range, so a burst of
// fixed world size is two different effects at the two ends of a 48 m arena.
// It scales GEOMETRY only — widths and streak lengths — never heat or colour.
static float fx_range_k(float d) {
  return f_clamp(d / KILLBURST_REF, KILLBURST_K_LO, KILLBURST_K_HI);
}

// BRASS. One discrete object leaving the weapon per trigger pull, which is the
// cheapest "the mechanism cycled" cue the genre has and the only one the player
// sees LEAVE the gun rather than happen to something else.
//
// THE PLAYER'S OWN, like the hitmarker and the kill burst, and here the reason is
// arithmetic rather than taste: at 720 rpm one casing lives 0.85 s, so a single
// shooter holds ~10 slots and twenty-one of them would hold 215 — most of the
// reserved region, spent on objects three pixels wide at the far end of the
// arena. It is spawned off EV_FIRE's own muzzle and direction, so nothing new is
// derived and nothing reads the camera.
#define BRASS_TICKS  102     // 0.85 s: long enough to arc and land in frame
#define BRASS_OUT    1.35f    // m/s to the shooter's right — an AR ejects right
#define BRASS_UP     1.55f    // ...and up, so it arcs rather than drops
#define BRASS_FWD    0.35f   // a little forward, off the ejection port
#define BRASS_SPIN   17.0f   // rad/s of tumble. A casing that does not tumble is
                             // a stick; the glint IS the read, and it comes from
                             // the length changing as it turns.
#define BRASS_LEN    0.016f  // half-length of the drawn sliver, 32 mm case
#define BRASS_PORT_F 0.42f   // m in front of the eye: where the receiver renders
#define SCORE_POP_TICKS 24   // 0.2 s of pulse on the frag number
#define STREAK_TICKS   150   // 1.25 s of banner — shorter than the 3 s window
                             // it counts, so a triple replaces its own double
                             // rather than queueing behind it
#define KILLFEED_N     5
#define KILLFEED_TICKS 480   // 4 s on screen, the last 0.5 s fading
#define DMG_TICKS      36    // incoming-damage flash

// A round in flight, for the eye. The sim is hitscan, so the impact already
// happened; this is the bolt that visibly crosses the same line at the weapon's
// display speed (TRACER_V). `a` is fixed in the WORLD once anchored: a bullet does not
// stay attached to the gun that fired it, however the shooter turns afterwards.
typedef struct {
  v3 a, b, d;       // launch point, end point, shot direction
  int life, life0, src, wpn;
  int anchor;       // still to be placed on the muzzle by the next rendered frame
  float v;          // metres per tick
  int wake;         // ticks of heat wake after the bolt lands (TRACER_LOOK)
  int delay;        // ticks before the bolt starts (a ricochet waits for its parent)
  int rico;         // 1 = a skip off a face: starts at `a`, no 3 m lead, thinner
  int smoke;        // muzzle smoke still to be released at the anchored muzzle
  int seen;         // a frame has drawn it: `base` holds the age it was first seen at
  float base;
} tracer_t;
// `fresh` is a render latch, not a lifetime: it marks a flash no frame has
// shown yet and is cleared by the first draw. Tick counting alone cannot
// survive tick BATCHING — app_frame runs every due tick and renders once, so
// at 60 fps the bright tick was decremented away before the render, and at an
// fps_cap of 30 the whole 3-tick flash could die unseen. The latch guarantees
// exactly one full-strength frame per shot at any frame rate; `t0` bounds it
// (12 ticks) so a harness script that fires and only screenshots much later
// does not get a bright star from a long-dead shot.
typedef struct { v3 pos, dir; int life, src; long t0; uint32_t seed; int fresh, wpn; } flash_t;
// kind: 0 = impact spark, 1 = skid dust, 2 = skid spark, 3 = contact flare.
// `life0` is the spawn life, because the fade has to normalise against the
// particle's OWN lifetime once lifetimes vary — a "hero" skid spark outlives a
// cheap one 3:1 and a shared PARTICLE_TICKS divisor would leave it clipped
// white for the first two thirds of its life. `y0` is the plane the fragment
// was struck FROM, so a slide across a platform skips off the platform and not
// off world zero. `sz` is PER-KIND and carries two different units, which is
// worth stating because everything else in this file is one unit per field:
// on a kind-2 skid spark it is a dimensionless size/heat scale (0.70 cheap ..
// 2.10 hero) feeding both ribbon width and brightness; on a kind-3 contact
// flare it is the ARM HALF-LENGTH IN METRES (0.032 .. 0.096) of the asterisk
// fx_build draws, which derives its own width from it.
// `y0` carries a second unit on the kind-5 kill burst, in the same idiom and
// for the same reason `sz` does: that kind never bounces, so the skip plane is
// dead weight on it, and it holds the burst's RANGE SCALE instead (see
// KILLBURST_REF) — a pure geometry multiplier on the fragment widths, kept out
// of `sz` so that sizing a burst for its range cannot also change its heat.
// `y0` is only sampled at spawn, so fragments already in flight when the slider
// leaves a raised surface keep skipping on the plane they were struck from —
// bounded by their remaining life (~0.4 s worst case) and left as it is,
// because re-probing the floor per particle per tick costs more than the defect.
typedef struct { v3 pos, vel; float y0, sz; int life, life0, kind; } particle_t;
static v3 g_cam_eye;   // the render camera (defined with the culling state below)
// AN IMPACT MARK IS A CRATER IN A SURFACE, NOT A STICKER ON IT, and it used to
// be both wrong things at once: an 84 mm disc of flat near-black — measured 4.6
// stops under a shadowed crate and 5.8 under a sunlit wall, and that SPREAD is
// the defect a host-relative albedo fixes, when a full cast shadow here is
// 2.94 and the whole AO ramp is 0.41 — drawn with no idea which face it belonged
// to, so a round landing near a corner left a black disc hanging in open air
// past the edge. Both are fixed by knowing the FACE: the record carries the face
// rectangle projected into the mark's own tangent frame (so every vertex can be
// radially clamped into it) and the host's own albedo (so the crater is a value
// OF that surface and has no silhouette step to read as a decal).
typedef struct {
  v3       pos, nrm;        // lifted centre, face normal
  v3       surf;            // the host face's albedo — the crater is relative
  float    rad;             // this mark's own radius
  float    u0, u1, v0, v1;  // clip window in axis_frame(nrm)'s (t1,t2), and
                            // u0 <= 0 <= u1, v0 <= 0 <= v1 by construction:
                            // that is what makes the radial clamp correct
  float    ao;              // the host face's own RENDERED occlusion, flat —
                            // NOT ao_at(y). mesh_quad emits only the four face
                            // CORNERS, so the world pass interpolates AO
                            // linearly across a whole face while ao_at is a
                            // ramp that saturates at 0.55 m: on a 2.75 m wall
                            // the two disagree by 0.33, and a mark drawn on the
                            // ramp renders ~50% brighter in linear than the wall
                            // it is in. A decal has to reproduce the host's
                            // INTERPOLATION, not the host's formula.
  float    rough;           // ...and the host's finish, for the same reason
  uint32_t seed;            // size / rotation / rim wobble, no RNG stream
  // THE CRATER HAD NO BIRTH. It was written at its final size and value and
  // drawn identically from the first frame to the last — 13 px at 8 m and 4 px
  // at 20 m, forever, with no dust, no spall flash and nothing directional. That
  // is the one permanent record a shot leaves and it arrived without an event.
  // For MARK_BLOOM_T ticks the DRAWN radius runs from MARK_BLOOM_K down to 1.0
  // and the outer rings brighten with it: a spall cloud that settles into the
  // hole. Deliberately in the SOLID pass and deliberately DARK-then-settling,
  // not an additive puff — an additive haze over this game's light surfaces
  // measured invisible at five times its level, which left a decal as the one
  // untried mechanism. The MERGE test and the depth-plane stagger both keep
  // using the SETTLED radius, so two fresh neighbours can never transiently
  // depth-tie against each other.
  long     t0;              // the tick it was stamped
  int      used;
} mark_t;
#define MARK_R        0.030f  // 60 mm across, against 84. A 5.56 round in
                              // concrete leaves a 10-25 mm entry crater inside
                              // a 40-70 mm spall zone.
#define MARK_R_VAR    0.12f   // +/-12% per mark: 30 marks on one wall must not
                              // read as one stamp repeated
#define MARK_R1       0.50f   // core plateau edge, x MARK_R: a 30 mm hole. 0.36
                              // put a 21.6 mm core inside a 60 mm crater, which
                              // is sub-pixel past ~10 m: measured against a
                              // shot-free baseline of the same frame, two marks
                              // at 20 m moved five pixels in total and two of
                              // them were BRIGHTER than the wall, i.e. the halo
                              // survived and the hole did not. Widening the CORE
                              // rather than the mark keeps the silhouette, the
                              // clip and the halo exactly where they were.
#define MARK_R2       0.66f   // spall halo peak — a RING, not an outline
#define MARK_K        8       // fan segments: the authored-facet family, and the
                              // rim is deliberately irregular anyway
// TWO COPLANAR DECALS ARE A DEPTH TIE, AND A DEPTH TIE IS A FLICKER. Every mark
// used to sit at exactly MARK_LIFT along its face's normal, so any two that
// overlapped fought for the same pixels and the pair rendered as a torn blob
// whose seams moved with the camera — measured, a 0.1 mm camera nudge over four
// stacked holes changed 30425 pixels. Two mechanisms fix it and both are needed.
// MERGE is the primary one: a new hole landing on top of an old one REPLACES it
// in the ring instead of being stacked on it, which is also what a second round
// through the same spot actually does to a wall. MARK_PLANES is the backstop for
// the partial overlaps merging leaves behind: the lift is staggered across a few
// depth planes by slot, so two rim-grazing marks are never coplanar.
#define MARK_MERGE    0.66f   // x (r1+r2): merge inside the SPALL radii, i.e.
                              // before the two haloes can overlap at all
#define MARK_PLANES   4       // distinct depth planes the ring cycles through
#define MARK_PLANE_DZ 0.0005f // 0.5 mm apart — above the 0.29 mm depth quantum
                              // at 24 m, and the mark is a pixel wide by the
                              // range where the quantum grows past it
#define MARK_LIFT     0.003f  // normal offset. The 24-bit depth quantum at
                              // ZNEAR 0.12 is d^2*4.96e-7 m = 1.14 mm at the
                              // arena's 48 m diagonal, so this keeps 2.6x margin
                              // at the worst sightline in the game. It halves
                              // the LIFT; what shows edge-on is dominated by the
                              // crater's own on-plane extent, so the sliver is
                              // now a 2 px pale rim rather than a 1 px black
                              // blade — which is the part that matters.
#define MARK_CORE_K   0.47f   // crater albedo x the host's: 2.0 stops under
#define MARK_HALO_K   1.12f   // fresh fractured material: +0.43 stop. Cannot
                              // overflow — the brightest palette entry is 0.83
                              // and col_tint's widest is 1.07: 0.995.
#define MARK_JIT_LO   0.86f   // rim wobble on the two outer rings only, +/-14%:
#define MARK_JIT_SPAN 0.28f   // a perfect circle is the strongest decal cue
#define MARK_ROUGH    0.95f   // the crater itself: scorched, no highlight left
#define MARK_BLOOM_K  3.1f    // x MARK_R at birth...
#define MARK_BLOOM_T  30      // ...collapsing to 1.0 over a quarter second
#define MARK_BLOOM_C  1.30f   // ...and the spall rings that much brighter with it
typedef struct { int src, dst, hs, life; } feed_t;

static tracer_t   g_tracers[MAX_TRACERS];
static flash_t    g_flashes[MAX_FLASHES];      // world-pass muzzle-flash stars
static particle_t g_parts[MAX_PARTICLES];
static mark_t     g_marks[MAX_MARKS];
static int        g_mark_head;
static int        g_muzzle_t;                  // viewmodel muzzle flash ticks left
static int        g_muzzle_fresh;              // render latch, see flash_t.fresh
static long       g_muzzle_fire_tick;          // its 12-tick staleness bound
// THE SCREEN NOW CARRIES THE SAME TWO AXES THE AUDIO DOES. `kind` is
// CONSEQUENCE (hit / kill) in bit 1 and PRECISION (body / head) in bit 0, which
// is the confirmation matrix exactly as THE CONFIRMATION MATRIX states it, and
// the head half is drawn as an additive LAYER on whichever base ran rather than
// as a fourth marker — the same reason sfx_head_sig is a layer. Before this the
// screen was a 1x2 matrix against the audio's 2x2: a 52-damage headshot and a
// 26-damage graze drew the byte-identical 42x42 px white X.
enum { HM_BODY = 0, HM_HEAD = 1, HM_KILL = 2, HM_KILL_HEAD = 3 };
static int        g_hitmark_t, g_hitmark_kind;  // ticks left + which of the four
static int        g_hitmark_arm, g_hitmark_pend; // the per-weapon arming delay
// The DIRECTIONAL half: where the round actually landed (the marker is drawn
// on that point's projection, so a hit at the edge of the spread cone marks
// the edge), and whether it landed on the plate carrier (the armour clink).
static v3         g_hitmark_at, g_hitmark_at_pend;
static int        g_hitmark_plate, g_hitmark_plate_pend;
static float      g_ch_kick;   // crosshair gap kick, in virtual px, decaying
// HIT-STOP. The player's own KILL only, and bounded twice over.
//
// A fixed-step sim can be time-scaled safely in a way a variable one cannot: the
// accumulator decides WHEN ticks run, never WHICH, so slowing it changes nothing
// about the tick sequence, the events, or any proof — `trace`, `parity` and the
// whole gate stay byte-identical because the harness drives `sim_advance`
// directly and never enters `app_frame` at all. What the player gets is 50 ms in
// which the world crawls while the kill burst opens and the confirmation lands,
// which is the genre's single loudest "the game noticed" signal.
//
// NOT ON A HIT. This is a 4-round-TTK competitive sim; stalling on ordinary
// damage would be a handling change wearing a feedback costume. And it is capped
// so a double kill cannot stack into a stutter: the counter is SET, never added
// to, so ten kills in a second still cost one window.
#define HITSTOP_KILL 5       // ticks (42 ms) of slow motion on a body kill...
#define HITSTOP_HEAD 7       // ...and on a headshot, which is the rarer event
#define HITSTOP_SCALE 0.18f  // ...at this fraction of real time
static int        g_hitstop;
// The world's muzzle light. Computed IN the world pass by scanning the flash
// ring directly (see render_frame) — the FX pass runs AFTER the world pass,
// and publishing from there would light the world one frame late, which on a
// two-tick flash rendered exactly once per harness `shot` is never at all.
static float      g_world_muz_c;
static v3         g_world_muz_p;
// THE SCORE EXISTED ONLY WHILE TAB WAS HELD. A match is a 30-step progress bar
// (sp_fraglimit) and the only way to see where you were in it was to stop
// aiming and hold the scoreboard — so every kill was a transient row in a feed
// that fades in four seconds and nothing else. `g_score_pop` is a pulse on the
// player's own kill; the numbers themselves all live in g_match already.
// `g_streak_t` is the multi-kill banner, on the SAME 3 s window the audio's
// pitch escalation uses, so the two cannot disagree about what a double is.
static int        g_score_pop;   // ticks of scale pulse on the frag readout
static int        g_streak_n;    // 2 = double, 3 = triple, ... 0 = nothing
static int        g_streak_t;    // ticks the banner has left
static long       g_streak_last_t = -100000;  // VFX-side window latch, see EV_KILL
static feed_t     g_feed[KILLFEED_N];          // kill feed, newest first
static int        g_dmg_t;                     // incoming-damage flash ticks
// FOUR DIRECTIONS, NOT ONE. `g_dmg_from` was a single vector overwritten by
// every hit and `g_dmg_t` a single shared timer, so in a 20-bot free-for-all two
// attackers on opposite sides produced ONE tick that flipped between them and
// the second hit reset the first's fade — i.e. the cue that is supposed to say
// WHERE said "somewhere" the moment more than one person was shooting you. Each
// slot now fades on its own; the edge wash stays driven by the shared g_dmg_t
// timer, which is what keeps "I am being hit" a single unmistakable state.
#define DMG_SLOTS 4
static struct { v3 from; int life; } g_dmg[DMG_SLOTS];
static uint64_t   g_rng_vfx = 0x5EEDu;         // particle scatter only
// EV_KILL carries no position — match_on_kill pushes src/dst/k and nothing else
// — so the kill burst takes the wound from the EV_HIT of the SAME TICK. That is
// exact rather than lucky: ent_damage pushes the hit event and then calls
// match_on_kill, in that order, and ev_push preserves order. It is also the
// idiom the audio layer already uses to put a weapon id onto EV_HIT/EV_KILL.
// Two things make it safe rather than merely convenient. The tick is stored WITH
// the point, so a kill that arrives with no hit in front of it — nothing does
// today, a fall or a melee would — spawns nothing instead of a burst at a stale
// wound somewhere else in the arena. And only the PLAYER's own hits are latched,
// which is what keeps a bot shooting the player on the same tick from
// overwriting the wound the player's own killing round just made.
// `dist` is |at - from|, i.e. the range the shot was taken at — see
// KILLBURST_REF for what it is for. It is the length ev_push's own `a` and `b`
// already imply and the EV_HIT case already computes to normalise the direction,
// so carrying it here costs one float and derives nothing twice.
static struct { v3 at, dir; float dist; long tick; } g_kill_wound = {.tick = -1};
// THE SAME WOUND, BUT FOR ANY VICTIM. g_kill_wound above answers "where did MY
// killing round land" and is filled only for g_local_ent's own hits, which is
// all the killburst ever needed. A CLIENT needs it for everybody: online
// nothing on this machine runs ent_damage, so the only description of a death
// it will ever get is the EV_HIT*/EV_KILL pair the server sends — and that
// pair carries exactly what rag_launch takes (a = impact, b = shooter origin,
// i = damage, k = headshot). Indexed by ENTITY id, so it is MAX_ENTS wide.
static struct { v3 at, from; long tick; int dmg, hs; } g_wound[MAX_ENTS];
// Alive state as of the previous tick, so a HUMAN's online death is an EDGE
// rather than a level — a level fires the launch (and the corpse handover) on
// every tick the body is down. The bots answer the same question with
// g_cl_body instead, which is declared up with net_cl_apply_ent, its first
// consumer: a bot's body is a position that needs an OWNER for as long as it
// lies there, and an edge cannot say who owns one that was already down when
// this client joined.
static unsigned char g_cl_alive_prev[MAX_ENTS];
// THE MARKER'S ARMING DELAY NEEDS ITS OWN LATCH. The audio layer's `g_last_wpn`
// looks like the obvious source and is the wrong one for this consumer: it is
// assigned in audio_on_event, which runs in the frame drain AFTER the ticks,
// while vfx_on_event runs at PUSH time inside the tick — so a marker armed from
// it reads the weapon of a shot drained in a PREVIOUS frame, and the first hit
// after a swap arms on the other weapon's offset (5 ticks against 16, a 92 ms
// difference in confirmation latency). Worse in the harness, where the audio
// route is off for any plain screenshot script: g_last_wpn is then permanently
// 0, so a battery filming the SNIPER's hitmarker would film the AR's arming
// delay — a probe measuring something the player never sees, which is the class
// of defect the file's "a probe must stay a side-effect-free observer" rule
// exists for. Latched from EV_FIRE instead: events are pushed in order within a
// tick and EV_FIRE always precedes the hit it caused, so this is exact.
static int g_vfx_wpn;

// The world-space barrel tip of a posed figure's rifle — defined with the gun
// tables below, declared here because the tracer spawn needs it.
static v3 pose_muzzle(const pose_t *P, int wpn);
// Defined with the FX primitives below, declared here for the same reason: the
// skid emitter needs deterministic variety and must NOT take it from
// g_rng_vfx. See skid_h.
static uint32_t fx_hash(uint32_t x);
// Perpendicular frame for a mark or a tracer whose only anchor is one
// direction — defined with the FX primitives below, declared here because
// mark_stamp resolves its clip window in exactly that frame.
static void axis_frame(v3 n, v3 *t1, v3 *t2);
// Hashed scatter, declared here because the body-hit burst wants the same
// deterministic variety the skid emitter uses and for the same reason.
static float skid_h(uint32_t *h);
static float skid_r(uint32_t *h, float lo, float hi);

// Free up to `want` slots for an impact by evicting the shortest-lived skid
// DECORATION. An impact spark is FEEDBACK — it says where the round landed —
// and a skid spark is scenery, so when the pool is genuinely full the scenery
// gives way: the same rule as the tracer's "the player's own streak must never
// be the one that silently vanishes" and the audio layer's lead voice. The
// reserve below PARTS_SKID_LO has so far been enough on its own (see the
// PARTS_SKID_LO block for the current split and its measured occupancy), but
// that is a measurement of the current tune and this makes it a guarantee. A
// no-op whenever the pool has room, so it cannot move any existing frame.
// One kind-3 contact flare, in the reserved region, so a burst is anchored to
// the point it came off. Shared by the world impact and the body hit rather
// than written twice; vfx_skid keeps its own because its size tracks a drag
// speed that only it has.
static void fx_flare(v3 at, float sz, int life) {
  for (int i = 0; i < MAX_PARTICLES; i++)
    if (g_parts[i].life <= 0) {
      g_parts[i] = (particle_t){.pos = at, .vel = (v3){{0, 0, 0}}, .y0 = 0.0f,
                                .sz = sz, .life = life, .life0 = life, .kind = 3};
      return;
    }
}

static void impact_free_slot(int want) {
  int free_n = 0;
  for (int i = 0; i < MAX_PARTICLES && free_n < want; i++)
    if (g_parts[i].life <= 0) free_n++;
  while (free_n < want) {
    int victim = -1, low = 1 << 30;
    for (int i = PARTS_SKID_LO; i < MAX_PARTICLES; i++)
      if (g_parts[i].life > 0 && g_parts[i].life < low) {
        low = g_parts[i].life;
        victim = i;
      }
    if (victim < 0) break;          // nothing but reserved traffic left
    g_parts[victim].life = 0;
    free_n++;
  }
}

// Stamp one crater on the face the round entered.
//
// Everything the draw needs is resolved HERE, once per round, because the 64
// live marks are rebuilt into the scene batch every frame: anything left for the
// draw is paid 64x per frame instead of once. The record is then self-contained
// — no solid index survives it — so a mark can never index a stale g_solids.
//
// The face rectangle is projected onto the disc's own tangent axes by interval
// arithmetic over all three world axes rather than by switching on which axis
// the normal lies along: t1 and t2 are perpendicular to n, so the normal's own
// axis contributes exactly zero to both intervals and never has to be a case.
static void mark_stamp(v3 at, v3 n, int solid) {
  v3 t1, t2;
  axis_frame(n, &t1, &t2);
  v3 mn, mx, surf;
  float aom, rough;
  if (solid >= g_num_solids) solid = -1;   // EV_IMPACT.i rides the wire:
                                           // a hostile index reads as "no host
                                           // solid", never as g_solids[32767]
  if (solid >= 0) {
    mn = g_solids[solid].min; mx = g_solids[solid].max;
    surf = g_solids[solid].color;
    rough = WORLD_SOLID_R;
    // The host's RENDERED occlusion at this point: mesh_build sets the ramp,
    // mesh_quad emits the four face corners and the rasteriser interpolates
    // between them, so a +y face is uniformly ao_at(mx.y), a -y face is
    // ao_at(mn.y), and a side face is a straight lerp up its own height.
    float lo = ao_ramp(mn.y, 0.0f, WORLD_AO_RATE, WORLD_AO_MIN);
    float hi = ao_ramp(mx.y, 0.0f, WORLD_AO_RATE, WORLD_AO_MIN);
    float h = mx.y - mn.y;
    aom = n.y > 0.5f ? hi : n.y < -0.5f ? lo
        : h > 1e-4f ? lo + (hi - lo) * f_clamp((at.y - mn.y) / h, 0.0f, 1.0f)
        : lo;
  } else {                                  // the arena floor is its own plane
    mn = (v3){{-ARENA_HALF, 0.0f, -ARENA_HALF}};
    mx = (v3){{ ARENA_HALF, 0.0f,  ARENA_HALF}};
    // The floor is no longer ao_off: mesh_ground_vert bakes a contact skirt into
    // it, and a crater stamped at a flat 1.0 sits BRIGHTER than the ground it is
    // in wherever that skirt bites. One function, two readers.
    surf = g_theme.ground; aom = ground_skirt_ao(at.x, at.z);
    rough = WORLD_GROUND_R;
  }
  // MERGE FIRST: if this round landed on top of an existing hole on the same
  // plane, take THAT slot rather than a fresh one. Same-plane is tested on the
  // normal and the offset along it, so a mark on the far side of a thin wall or
  // on the neighbouring face of a corner is never mistaken for an overlap.
  int slot = g_mark_head, merged = 0;
  for (int i = 0; i < MAX_MARKS; i++) {
    const mark_t *m = &g_marks[i];
    if (!m->used || v3_dot(m->nrm, n) < 0.99f) continue;
    v3 d = v3_sub(at, m->pos);
    // `at` is the raw impact point but m->pos is the LIFTED centre, so the
    // along-normal distance for a genuine same-face overlap is exactly the old
    // mark's own lift: MARK_LIFT + up to (MARK_PLANES-1)*MARK_PLANE_DZ. The
    // threshold must clear the DEEPEST lift — at 0.004 every plane-3 mark
    // could never be merged onto and plane-2 sat at zero margin, i.e. the
    // documented PRIMARY mechanism silently never fired for 25-50% of the
    // ring. 2 mm of slack on top still cannot reach the far side of the
    // thinnest wall or a neighbouring corner face.
    if (fabsf(v3_dot(d, n)) >
        MARK_LIFT + (float)(MARK_PLANES - 1) * MARK_PLANE_DZ + 0.002f)
      continue;                                      // a different plane
    float lat = sqrtf(fmaxf(0.0f, v3_dot(d, d) - v3_dot(d, n) * v3_dot(d, n)));
    if (lat < (MARK_R + m->rad) * MARK_MERGE) { slot = i; merged = 1; break; }
  }
  // The lift is purely along n and t1.n == t2.n == 0, so the window below is
  // the same whether it is measured from the impact point or the lifted centre.
  // It is staggered by SLOT, and the window is inset by the largest of them, so
  // the deepest plane still cannot poke past the face's edge.
  float lift = MARK_LIFT + (float)(slot % MARK_PLANES) * MARK_PLANE_DZ;
  const float lift_max = MARK_LIFT + (MARK_PLANES - 1) * MARK_PLANE_DZ;
  v3 p = v3_add(at, v3_scale(n, lift));
  float u0 = 0, u1 = 0, v0 = 0, v1 = 0;
  for (int k = 0; k < 3; k++) {
    float lo = mn.e[k] - p.e[k], hi = mx.e[k] - p.e[k];
    float a = t1.e[k] * lo, b = t1.e[k] * hi;
    u0 += a < b ? a : b;  u1 += a < b ? b : a;
    a = t2.e[k] * lo;     b = t2.e[k] * hi;
    v0 += a < b ? a : b;  v1 += a < b ? b : a;
  }
  uint32_t h = fx_hash((uint32_t)g_tick * 2654435761u + (uint32_t)slot);
  g_marks[slot] = (mark_t){
    .pos = p, .nrm = n, .surf = surf, .ao = aom, .rough = rough,
    .seed = h, .t0 = g_tick, .used = 1,
    .rad = MARK_R * (1.0f - MARK_R_VAR +
                     2.0f * MARK_R_VAR * (float)((h >> 8) & 255) / 255.0f),
    // Inset by the lift, then clamped at the centre. The inset is what stops the
    // 3 mm stand-off showing as a sliver past a corner when the face is seen
    // edge-on; the clamp is what stops a round that lands ON an edge producing
    // an inside-out window and vanishing — it leaves half a crater instead.
    .u0 = f_clamp(u0 + lift_max, u0, 0.0f), .u1 = f_clamp(u1 - lift_max, 0.0f, u1),
    .v0 = f_clamp(v0 + lift_max, v0, 0.0f), .v1 = f_clamp(v1 - lift_max, 0.0f, v1)};
  // A merge reuses a slot, so the ring's age order — and therefore which hole
  // the next 64 rounds evict — is only advanced when a NEW one was taken.
  if (!merged) g_mark_head = (g_mark_head + 1) % MAX_MARKS;
}

// THE BLAST'S PICTURE. Everything else in the FX pass is additive, which can only
// add light: that is right for a fireball and useless for smoke, which is dark. So
// puffs are drawn in a second pass that MULTIPLIES the frame, white at the rim (no
// change) and the puff's density at the centre, which gives soft dark smoke with no
// alpha channel anywhere. Clods are the same thing, small, heavy and dense.
#define MAX_PUFFS 1100
#define MAX_SCORCH 14
#define MAX_FIRE  64
#define MAX_SHOCK 6
#define MAX_BRANDS 27
#define SHOCK_LIFE 26
typedef struct { v3 pos, vel; float r0, r1, dens, grav, drag; int life, life0; float soot; } puff_t;
typedef struct { v3 pos; float r; int life; } scorch_t;
typedef struct { v3 pos, vel; float r; int life, life0, dull; } fire_t;
typedef struct { v3 pos; float gy; int life, open; uint32_t seed; } shock_t;
// A brand: a burning scrap on a real arc, drawing its own line of smoke across the sky.
typedef struct { v3 pos, vel; int life, life0; } brand_t;
static puff_t   g_puffs[MAX_PUFFS];
static scorch_t g_scorch[MAX_SCORCH];
static fire_t   g_fire[MAX_FIRE];
static shock_t  g_shock[MAX_SHOCK];
static brand_t  g_brands[MAX_BRANDS];
static int      g_shock_at;
static float    g_shake, g_blast_light;
static v3       g_blast_p;
static int      g_scorch_at;

// Spoons in the air: presentation only, tumbling once and gone.
// DEBRIS: masonry and concrete do not only throw dust, they spall — a few
// solid chips of the struck surface's own colour that tumble out, bounce and
// lie on the floor until they fade. Solid geometry (scene pass, they cast),
// presentation only.
typedef struct { v3 pos, prev, vel, axis, col; float a, spin, sz; int life; } chip_t;
// SPENT MAGAZINES ON THE FLOOR. An empty reload drops the magazine free, and
// it has to LAND somewhere: it falls from the shooter's magwell, skips once and
// lies on its side for ~9 s before it sinks away. Every shooter's, bots too —
// a magazine on the ground is a tell of where a fight was. Presentation only.
typedef struct { v3 pos, prev, vel, axis; float a, spin, head; int life, delay, src, wpn, var, rest; } wmag_t;
#define MAX_WMAGS 24
static wmag_t g_wmags[MAX_WMAGS];
static int g_wmag_at;
#define MAX_CHIPS 48
static chip_t g_chips[MAX_CHIPS];
static int g_chip_at;
static float g_wpn_heat;          // the player's barrel, 0..1.6, presentation only
static int g_wpn_heat_tick;       // last shot
static v3  g_fx_muzzle_vis;       // the drawn muzzle, published by the renderer
static int g_fx_muz_live_vis;
#define MAX_SPOONS 8
typedef struct { v3 pos, prev, vel, axis; float a; int life; } spoon_t;
static spoon_t g_spoons[MAX_SPOONS];

// IMPACT DUST. Its own small pool and its own pass, because neither existing one
// can draw it: the puff pass MULTIPLIES (it can only darken, so a light plume of
// sand off a light wall came out as a black dot of soot), and the particle pool's
// reserve is sized and proved for sparks (budget / part_res_peak). This is an
// ADDITIVE, soft, sunlit haze tinted by what was struck — faint by design: it is
// the body of the hit, the sparks are its edge. Oldest-first eviction, so a long
// burst simply recycles its own earliest clouds.
#define MAX_DUST 96
typedef struct { v3 pos, vel, col; float r0, r1; int life, life0, wisp; } dust_t;
static dust_t g_dust[MAX_DUST];
static int    g_dust_at;
static void dust_add(v3 pos, v3 vel, v3 col, float r0, float r1, int life) {
  g_dust[g_dust_at] = (dust_t){pos, vel, col, r0, r1, life, life, 0};
  g_dust_at = (g_dust_at + 1) % MAX_DUST;
}

static void puff_add(v3 pos, v3 vel, float r0, float r1, float dens, float grav,
                     float drag, int life, float soot) {
  for (int i = 0; i < MAX_PUFFS; i++)
    if (g_puffs[i].life <= 0) {
      g_puffs[i] = (puff_t){pos, vel, r0, r1, dens, grav, drag, life, life, soot};
      return;
    }
}

// MUZZLE SMOKE. The burnt powder a shot leaves hanging at the barrel, which
// the flash (three ticks) cannot say and which is how a sustained burst reads
// as a gun that has been WORKING: a thin grey breath per rifle round that
// rises and drifts, a puff for the pistol, and for the magnum the brake's
// signature — the ports throw it sideways as a pair of clouds either side of
// the muzzle, the tell that gives a sniper's position away. Drawn in the
// multiply pass like every other smoke (puffs_build), hashed off the tick and
// the muzzle rather than drawn from g_rng_vfx, so nothing else's scatter moves.
// `own` is the player's own first-person shot: thinner and pushed a little
// down the bore, because a cloud on the lens is not a sight picture.
static void vfx_muzzle_smoke(v3 at, v3 dir, int wpn, int own) {
  v3 fw = v3_norm(dir);
  v3 side = v3_cross(fw, (v3){{0, 1, 0}});
  float sl = v3_len(side);
  side = sl > 1e-3f ? v3_scale(side, 1.0f / sl) : (v3){{1, 0, 0}};
  uint32_t h = fx_hash((uint32_t)g_tick * 2654435761u ^
                       (uint32_t)(int)(at.x * 97.0f + at.z * 131.0f) ^ 0x534D4B45u);
  float dk = own ? 0.55f : 1.0f;
  if (own) at = v3_add(at, v3_scale(fw, 0.30f));
  v3 up = {{0, 0.30f, 0}};
  if (wpn == WPN_SR) {
    puff_add(v3_add(at, v3_scale(fw, 0.15f)), v3_add(v3_scale(fw, 2.4f), up),
             0.05f, 0.34f, 0.11f * dk, -0.30f, 0.93f, 130, 0.9f);
    for (int sd = -1; sd <= 1; sd += 2) {
      float j = skid_r(&h, 0.8f, 1.2f);
      puff_add(v3_add(at, v3_scale(side, 0.06f * (float)sd)),
               v3_add(v3_add(v3_scale(side, 2.8f * j * (float)sd), v3_scale(fw, 0.6f)), up),
               0.04f, 0.30f, 0.10f * dk, -0.25f, 0.92f, 120 + (int)(20.0f * j), 0.9f);
    }
    puff_add(at, v3_add(v3_scale(fw, 0.5f), up), 0.08f, 0.48f, 0.07f * dk,
             -0.20f, 0.97f, 200, 0.9f);
  } else if (wpn == WPN_PT) {
    puff_add(v3_add(at, v3_scale(fw, 0.06f)), v3_add(v3_scale(fw, 0.9f * skid_r(&h, 0.7f, 1.2f)), up),
             0.02f, 0.13f, 0.09f * dk, -0.22f, 0.93f, 48, 0.9f);
  } else {
    puff_add(v3_add(at, v3_scale(fw, 0.10f)), v3_add(v3_scale(fw, 1.3f * skid_r(&h, 0.7f, 1.2f)), up),
             0.025f, 0.17f, 0.10f * dk, -0.25f, 0.94f, 60, 0.9f);
  }
}

static void vfx_blast(v3 at, int open) {
  #define RV(lo, hi) rng_rangef(&g_rng_vfx, lo, hi)
  g_blast_p = v3_add(at, (v3){{0, 0.5f, 0}});
  g_blast_light = 1.0f;
  // The camera takes the shock front, harder the closer it is.
  float cd = v3_len(v3_sub(g_listen, at));
  g_shake = fmaxf(g_shake, f_clamp(1.0f - cd / 36.0f, 0.0f, 1.0f));
  fx_flare(v3_add(at, (v3){{0, 0.35f, 0}}), 1.15f, 5);
  // Everything that rises from the ground rises from THIS ground: a charge bursts
  // on its way down as often as at rest, and the cloud must still have a root.
  float gy = rag_support(at);
  v3 g0 = {{at.x, gy, at.z}};
  // The fireball: a hard, brief burst, as a fragmentation charge's is.
  for (int i = 0, n = 0; i < MAX_FIRE && n < 16; i++)
    if (g_fire[i].life <= 0) {
      v3 d = {{RV(-1, 1), RV(0.1f, 1.2f), RV(-1, 1)}};
      g_fire[i] = (fire_t){v3_add(at, v3_scale(d, 0.30f)), v3_scale(d, RV(0.012f, 0.045f)),
                           RV(0.75f, 1.9f), (int)RV(10, 26), 0, 0};
      g_fire[i].life0 = g_fire[i].life;
      n++;
    }
  // The heart of it: dull fire that stays alight inside the smoke as it rises.
  for (int i = 0, n = 0; i < MAX_FIRE && n < 11; i++)
    if (g_fire[i].life <= 0) {
      v3 p = {{at.x + RV(-1.3f, 1.3f), gy + RV(0.3f, 1.9f), at.z + RV(-1.3f, 1.3f)}};
      g_fire[i] = (fire_t){p, (v3){{RV(-0.003f, 0.003f), RV(0.004f, 0.012f), RV(-0.003f, 0.003f)}},
                           RV(0.7f, 1.5f), (int)RV(50, 125), 0, 1};
      g_fire[i].life0 = g_fire[i].life;
      n++;
    }
  // The shock front, seen: a shell of bent air, and the rays of the first instant.
  g_shock[g_shock_at] = (shock_t){v3_add(at, (v3){{0, 0.35f, 0}}), gy, SHOCK_LIFE, open,
                                  (uint32_t)(g_tick * 2654435761u)};
  g_shock_at = (g_shock_at + 1) % MAX_SHOCK;
  // Burning scraps thrown high, each trailing smoke.
  for (int i = 0, n = 0; i < MAX_BRANDS && n < 9; i++)
    if (g_brands[i].life <= 0) {
      float a = (float)n / 9.0f * F_TAU + RV(-0.3f, 0.3f), sp = RV(2.5f, 7.5f);
      int l = (int)RV(55, 110);
      g_brands[i] = (brand_t){v3_add(g0, (v3){{0, 0.3f, 0}}),
                              (v3){{cosf(a) * sp, RV(6.5f, 13.0f), sinf(a) * sp}}, l, l};
      n++;
    }
  // Fragments, fast and flat and gone in a tenth of a second, then the embers:
  // slower hot grit that hangs, arcs over and dies on the way down.
  impact_free_slot(80);
  for (int n = 0, i = 0; i < MAX_PARTICLES && n < 78; i++)
    if (g_parts[i].life <= 0) {
      int ember = n >= 44, life = ember ? (int)RV(50, 120) : 15;
      float a = RV(0, F_TAU), up = ember ? RV(0.5f, 1.3f) : RV(0.04f, 0.62f);
      float sp = ember ? RV(3.0f, 9.0f) : RV(16.0f, 34.0f);
      g_parts[i] = (particle_t){.pos = v3_add(at, (v3){{0, 0.10f, 0}}),
          .vel = {{cosf(a) * sp, up * sp, sinf(a) * sp}}, .y0 = gy,
          .sz = ember ? RV(0.40f, 0.80f) : RV(0.7f, 1.4f),
          .life = life, .life0 = life, .kind = 2};
      n++;
    }
  if (open) {
    // The ground ring: the shock front drags dust out flat along the floor.
    for (int i = 0; i < 40; i++) {
      float a = (float)i / 40.0f * F_TAU + RV(-0.1f, 0.1f), sp = RV(9.0f, 17.0f);
      puff_add(v3_add(g0, (v3){{cosf(a) * 0.4f, 0.14f, sinf(a) * 0.4f}}),
               (v3){{cosf(a) * sp, RV(0.2f, 0.9f), sinf(a) * sp}},
               0.30f, RV(1.2f, 2.0f), 0.16f, -0.02f, 0.950f, (int)RV(160, 340), 0.0f);
    }
    // The spray: dirt jets thrown up and out of the crater, falling back as dust.
    for (int i = 0; i < 16; i++) {
      float a = RV(0, F_TAU), sp = RV(1.0f, 5.0f);
      puff_add(v3_add(g0, (v3){{0, 0.15f, 0}}), (v3){{cosf(a) * sp, RV(7.0f, 14.0f), sinf(a) * sp}},
               0.16f, RV(0.8f, 1.3f), 0.28f, 5.0f, 0.982f, (int)RV(100, 200), 0.0f);
    }
  }
  // Thrown earth: heavy clods on real arcs.
  for (int i = 0; i < 46; i++) {
    float a = RV(0, F_TAU), sp = RV(2.5f, 10.0f), up = RV(3.0f, 11.5f);
    float r = RV(0.025f, 0.085f);
    puff_add(v3_add(g0, (v3){{0, 0.12f, 0}}), (v3){{cosf(a) * sp, up, sinf(a) * sp}},
             r, r, 0.62f, 9.81f, 0.999f, (int)RV(55, 110), 0.0f);
  }
  // The body of the cloud: smoke punched out in a few uneven lobes, stopped dead by
  // the air within a few metres, then swelling and climbing slowly on its own heat.
  // Lobes are what make it billow: one even burst reads as a balloon.
  for (int l = 0; l < 7; l++) {
    float la = RV(0, F_TAU), le = RV(0.08f, 1.2f), ls = RV(2.0f, 8.5f);
    v3 ld = {{cosf(la) * cosf(le), sinf(le), sinf(la) * cosf(le)}};
    for (int i = 0; i < 8; i++) {
      v3 d = v3_norm(v3_add(ld, (v3){{RV(-0.4f, 0.4f), RV(-0.25f, 0.4f), RV(-0.4f, 0.4f)}}));
      puff_add(v3_add(g0, (v3){{d.x * 0.4f, 0.35f + fabsf(d.y) * 0.4f, d.z * 0.4f}}),
               v3_scale(d, ls * RV(0.6f, 1.1f)), RV(0.35f, 0.7f), RV(1.1f, 2.5f),
               RV(0.06f, 0.14f), RV(-0.9f, -0.2f), 0.985f, (int)RV(500, 1400), RV(0.3f, 0.8f));
    }
  }
  // What stays: a wide thin haze on the floor, long after the bang.
  if (open)
    for (int i = 0; i < 16; i++) {
      float a = RV(0, F_TAU), rr = RV(0.5f, 3.0f);
      puff_add(v3_add(g0, (v3){{cosf(a) * rr, RV(0.4f, 1.2f), sinf(a) * rr}}),
               (v3){{cosf(a) * 0.5f, 0.05f, sinf(a) * 0.5f}},
               1.2f, RV(3.5f, 5.5f), 0.055f, -0.04f, 0.995f, (int)RV(1400, 2300), 0.3f);
    }
  if (open && gy < 0.05f) {
    g_scorch[g_scorch_at] = (scorch_t){at, RV(2.0f, 2.7f), 7200};
    g_scorch_at = (g_scorch_at + 1) % MAX_SCORCH;
  }
  #undef RV
}

static void vfx_on_event(const event_t *e) {
  switch (e->type) {
  case EV_FIRE: {
    if (e->src == g_local_ent) g_vfx_wpn = e->wpn;   // the hitmarker's arming delay
    if (e->src == g_local_ent) {
      // The muzzle is `a - b*f`, exactly as the fly-by crack reconstructs it —
      // one expression, no new geometry query. `right` is the shot direction
      // crossed with world up, which is where a port faces on a level rifle and
      // is close enough on a pitched one that nobody could name the error.
      v3 md = v3_norm(e->b);
      v3 mz = v3_sub(e->a, v3_scale(md, e->f > 0.0f ? e->f : 1.0f));
      v3 rt = v3_cross(md, (v3){{0, 1, 0}});
      float rl = v3_len(rt);
      rt = rl > 1e-4f ? v3_scale(rt, 1.0f / rl) : (v3){{1, 0, 0}};
      uint32_t bh = fx_hash((uint32_t)g_tick * 2654435761u + 0x42524153u);
      for (int i = 0; i < MAX_PARTICLES; i++)
        if (g_parts[i].life <= 0) {
          v3 v = v3_add(v3_scale(rt, BRASS_OUT * skid_r(&bh, 0.80f, 1.25f)),
                v3_add(v3_scale((v3){{0, 1, 0}}, BRASS_UP * skid_r(&bh, 0.75f, 1.30f)),
                       v3_scale(md, BRASS_FWD * skid_r(&bh, -0.4f, 1.0f))));
          // SPAWNED WHERE THE RECEIVER IS, NOT WHERE THE MUZZLE IS. For the
          // player those are not the same point: `weapon_fire` fires from
          // `player_eye_pos`, so the reconstructed muzzle is the CAMERA — and a
          // case ejected sideways from the camera crosses the frustum in two
          // ticks and is gone before it can be seen. BRASS_PORT_F puts it out at
          // the distance the viewmodel's own receiver renders at, where the
          // screen is 0.42 m wide at this FOV and 100 mm of travel is a quarter
          // of the way to the edge rather than all of it.
          g_parts[i] = (particle_t){
              .pos = v3_add(mz, v3_add(v3_scale(rt, 0.135f),
                            v3_add(v3_scale(md, BRASS_PORT_F),
                                   (v3){{0, -0.155f, 0}}))),
              .vel = v, .y0 = skid_r(&bh, 0.0f, F_TAU),   // tumble phase
              .sz = skid_r(&bh, 0.85f, 1.15f),
              .life = BRASS_TICKS, .life0 = BRASS_TICKS, .kind = 6};
          break;
        }
    }
    // The streak leaves the BARREL, not a point in front of the camera: the
    // shooter's pose anchors the same rifle model scene_draw_figure draws, so
    // its muzzle is where the round visibly leaves. (The pose is the previous
    // tick's — it solves at the end of the entity's tick — which at 120 Hz is
    // invisible.) The player's own first-person streak is additionally
    // re-anchored every frame in fx_build onto the viewmodel's published
    // muzzle; this pose point is its first-frame fallback and its
    // third-person truth.
    // Three-range dispatch: for a human id above MAX_BOTS this was a
    // g_bots[src - 1] OOB READ straight into pose_muzzle (A.2).
    // A wire event whose src was neutralized to -1 (net_ev_to_live) has no
    // shooter to anchor a muzzle on — without this guard it reads g_bots[-2].
    if (e->src < 0) break;
    int mh = human_of_ent(e->src);
    const pose_t *P = mh >= 0 ? &g_player_anim[mh].pose
                              : &g_bots[e->src - 1].anim.pose;
    v3 muz = pose_muzzle(P, e->wpn);
    // The pool cannot overflow at the default tune (fire cooldown 10 ticks >
    // the 8-tick life caps concurrency at one per shooter), but a DEV rpm
    // near the 1000 cap makes consecutive shots overlap — and the PLAYER's
    // own streak must never be the one that silently vanishes, so when no
    // slot is free it takes the oldest (the audio layer's lead-voice rule).
    {
      int slot = -1, oldest = -1, low = 1 << 30;
      for (int i = 0; i < MAX_TRACERS; i++) {
        if (g_tracers[i].life <= 0) { slot = i; break; }
        if (g_tracers[i].life < low) { low = g_tracers[i].life; oldest = i; }
      }
      if (slot < 0 && e->src == g_local_ent) slot = oldest;
      if (slot >= 0)
      {
        const tracer_look_t *tl = tracer_look(e->wpn);
        float mv = tl->v;
        float sl = v3_len(v3_sub(e->a, muz));
        float fly = (sl + fmaxf(tl->len, tl->span * sl)) / mv;
        // +2 for the ticks before the first frame that draws it (see `base`).
        int life = (int)fly + 4;
        life = life < 3 ? 3 : life > TRACER_TICKS ? TRACER_TICKS : life;
        // A miss into the open sky has no line worth leaving a wake on.
        int wake = e->k != 0 || sl < GUN_RANGE * 0.5f ? tl->wake : 0;
        g_tracers[slot] = (tracer_t){.a = muz, .b = e->a, .d = e->b, .life = life + wake,
                                     .life0 = life + wake, .src = e->src, .wpn = e->wpn,
                                     .anchor = 1, .v = mv, .wake = wake, .smoke = 1};
      }
    }
    if (e->src == g_local_ent) {
      g_wpn_heat = fminf(g_wpn_heat + (e->wpn == WPN_SR ? 0.30f : e->wpn == WPN_PT ? 0.10f : 0.075f), 1.6f);
      g_wpn_heat_tick = (int)g_tick;
      g_muzzle_t = flash_ticks(e->wpn);
      g_muzzle_fresh = 1;             // one guaranteed bright frame, any fps
      g_muzzle_fire_tick = g_tick;
    }
    // Every shooter gets a world-pass star; whether it is DRAWN is the
    // renderer's per-camera call (fx_build skips the player's while the
    // viewmodel pass owns the flash) — gating the spawn on the third-person
    // CONFIG here left a free-cam film of the player firing with no flash.
    for (int i = 0; i < MAX_FLASHES; i++)
      if (g_flashes[i].life <= 0) {
        g_flashes[i] = (flash_t){muz, e->b, flash_ticks(e->wpn), e->src, g_tick,
                                 (uint32_t)(e->i * 61 + e->src * 7), 1, e->wpn};
        break;
      }
    break;
  }
  case EV_IMPACT: {
    // The range the round was taken at, for the angular scale. EV_IMPACT's `f`
    // is otherwise unused and gun_shoot already has the distance, so this is
    // carried rather than re-derived — the same idiom the kill burst uses for
    // g_kill_wound.dist.
    float rk = fx_range_k(e->f > 0.0f ? e->f : KILLBURST_REF);
    // WHAT THE ROUND HIT decides what comes off it — the same surface material
    // the world shader draws (g_solid_mat), so a wall that LOOKS like brick
    // also answers like brick. Steel sheet throws the full shower of hot
    // sparks; masonry throws fewer, cooler chips and a cloud of its own dust;
    // timber barely sparks at all and puffs; the floor kicks up whatever the
    // theme's ground is. The spark COUNT only ever goes down from
    // IMPACT_SPARKS, so the reserve impact_free_slot keeps is unchanged.
    int sm = e->i >= 0 && e->i < g_num_solids ? g_solid_mat[e->i] : -1;
    int metal = smat_metal(sm);
    int nsp = metal ? IMPACT_SPARKS : sm == SMAT_WOOD ? IMPACT_SPARKS / 3
            : sm < 0 ? IMPACT_SPARKS / 2 : (IMPACT_SPARKS * 2) / 3;
    float heat = metal ? 1.15f : sm == SMAT_WOOD ? 0.55f : sm < 0 ? 0.70f : 0.80f;
    impact_free_slot(IMPACT_SPARKS + 1);
    for (int n = 0, i = 0; i < MAX_PARTICLES && n < nsp; i++)
      if (g_parts[i].life <= 0) {  // sparks: fast, short, mostly along the normal
        v3 v = {{rng_rangef(&g_rng_vfx, -3.4f, 3.4f),
                 rng_rangef(&g_rng_vfx, -0.8f, 4.2f),
                 rng_rangef(&g_rng_vfx, -3.4f, 3.4f)}};
        g_parts[i] = (particle_t){
            .pos  = v3_add(e->a, v3_scale(e->b, 0.02f)),
            .vel  = v3_add(v, v3_scale(e->b, rng_rangef(&g_rng_vfx, 1.5f, 4.5f))),
            .y0 = rk, .sz = heat,
            .life = PARTICLE_TICKS, .life0 = PARTICLE_TICKS, .kind = 0};
        n++;
      }
    // THE DUST. Hashed, not drawn from g_rng_vfx: the spark scatter above stays
    // exactly what it was for every other effect sharing the stream.
    if (!metal) {
      uint32_t dh = fx_hash((uint32_t)(int)(e->a.x * 173.0f) * 2654435761u ^
                            (uint32_t)(int)(e->a.z * 211.0f + e->a.y * 97.0f) ^ (uint32_t)g_tick);
      int sand = sm < 0 && g_theme.surf == SURF_SAND;
      int np = sand ? 3 : 2;
      float big = sand ? 1.5f : sm < 0 ? 1.1f : 1.0f;
      // Brick is fired clay, render is lime, timber is sawdust — each its own
      // colour; concrete, stone and paint are a cold grey; the floor follows its
      // theme. Linear, additive, and deliberately dim (see g_dust).
      v3 dc = sm == SMAT_BRICK ? (v3){{0.20f, 0.10f, 0.06f}}
            : sm == SMAT_STUCCO ? (v3){{0.20f, 0.17f, 0.13f}}
            : sm == SMAT_WOOD ? (v3){{0.19f, 0.14f, 0.08f}}
            : sand ? (v3){{0.22f, 0.17f, 0.10f}}
            : (v3){{0.15f, 0.15f, 0.14f}};
      // THE CITADEL IS SANDSTONE AND LIME: a round into it raises a warm plume,
      // bigger and longer than grey concrete's puff.
      if (g_map_id == 4 && (sm == SMAT_STUCCO || sm == SMAT_STONE || sm == SMAT_CONCRETE || sm < 0)) {
        dc = (v3){{0.24f, 0.18f, 0.11f}};
        np += 2;
        big *= 1.35f;
      }
      // Spall chips of the host's own colour.
      {
        v3 hc = e->i >= 0 && e->i < g_num_solids ? g_solids[e->i].color : g_theme.ground;
        int nch = sm == SMAT_WOOD ? 2 : 3 + (int)(dh % 3u);
        for (int k = 0; k < nch; k++) {
          v3 t1, t2;
          axis_frame(e->b, &t1, &t2);
          chip_t *c = &g_chips[g_chip_at];
          g_chip_at = (g_chip_at + 1) % MAX_CHIPS;
          v3 v = v3_add(v3_scale(e->b, skid_r(&dh, 1.2f, 3.4f)),
                 v3_add(v3_scale(t1, skid_r(&dh, -1.6f, 1.6f)), v3_scale(t2, skid_r(&dh, -1.6f, 1.6f))));
          v3 ax = v3_norm((v3){{skid_r(&dh, -1, 1), skid_r(&dh, -1, 1), skid_r(&dh, -1, 1) + 0.01f}});
          float tn = skid_r(&dh, 0.55f, 0.95f);
          *c = (chip_t){.pos = v3_add(e->a, v3_scale(e->b, 0.02f)), .vel = v, .axis = ax,
                        .col = v3_scale(hc, tn), .a = skid_r(&dh, 0.0f, F_TAU),
                        .spin = skid_r(&dh, 9.0f, 26.0f), .sz = skid_r(&dh, 0.006f, 0.016f),
                        .life = 360};
          c->prev = c->pos;
        }
      }
      for (int k = 0; k < np; k++) {
        v3 t1, t2;
        axis_frame(e->b, &t1, &t2);
        v3 v = v3_add(v3_scale(e->b, skid_r(&dh, 0.9f, 2.4f) * big),
               v3_add(v3_scale(t1, skid_r(&dh, -0.5f, 0.5f)), v3_scale(t2, skid_r(&dh, -0.5f, 0.5f))));
        dust_add(v3_add(e->a, v3_scale(e->b, 0.06f)), v, v3_scale(dc, 1.8f * skid_r(&dh, 0.8f, 1.2f)),
                 0.06f * big, skid_r(&dh, 0.28f, 0.42f) * big, (int)skid_r(&dh, 60.0f, 100.0f));
      }
    }
    // THE CONTACT FLARE, which impacts never had. vfx_skid's own comment states
    // the reason it exists, verbatim: "a fragment is gone from the contact
    // within two ticks and nothing marks where it came from" — and measured on a
    // ground shot at 3.4 m the spark cloud's bounding box had separated ~90 px
    // from the 10x5 px hole by t+20 with nothing at all at the contact point.
    // It is the existing kind 3 with no new draw code, sized angularly like the
    // rest and living just long enough to be the anchor rather than the event.
    // THE FLARE IS THE HERO OF A WALL HIT. It is what says "a round arrived
    // HERE", it is the only part of the burst that does not move, and it is a
    // three-armed asterisk with no straight silhouette at any camera angle,
    // which is exactly the read a strike wants. Sized angularly like the rest.
    fx_flare(v3_add(e->a, v3_scale(e->b, 0.02f)), 0.115f * rk, 9);
    mark_stamp(e->a, e->b, e->i);
    // THE SKIP (see RICO_COS). Only a round that STOPPED here (k = 0: a
    // through-hole is not a glance) and only off a shooter's own bolt, which
    // gun_shoot pushed one event earlier and which carries the line it flew.
    if (e->k == 0 && e->src >= 0) {
      tracer_t *pt = NULL;
      for (int i = 0; i < MAX_TRACERS; i++) {
        tracer_t *t = &g_tracers[i];
        if (t->life > 0 && !t->rico && t->src == e->src) {
          v3 dd = v3_sub(t->b, e->a);
          if (v3_dot(dd, dd) < 1e-6f) { pt = t; break; }
        }
      }
      if (pt) {
        v3 d = v3_norm(pt->d), n = e->b;
        float ci = -v3_dot(d, n);
        uint32_t h = fx_hash((uint32_t)g_tick * 2246822519u + (uint32_t)(e->src + 1) * 40503u
                             + (uint32_t)(int)(e->a.y * 1000.0f));
        if (ci > 0.0f && ci < RICO_COS && skid_r(&h, 0.0f, 1.0f) < 0.80f) {
          v3 r = v3_sub(d, v3_scale(n, 2.0f * v3_dot(d, n)));
          v3 t1, t2;
          axis_frame(r, &t1, &t2);
          r = v3_norm(v3_add(r, v3_add(v3_scale(t1, skid_r(&h, -0.14f, 0.14f)),
                                       v3_scale(t2, skid_r(&h, -0.14f, 0.14f)))));
          // ...and never back INTO the face it left.
          if (v3_dot(r, n) < 0.03f) r = v3_norm(v3_add(r, v3_scale(n, 0.06f - v3_dot(r, n))));
          v3 o = v3_add(e->a, v3_scale(n, 0.02f)), wn;
          int ws;
          float L = fminf(RICO_LEN * skid_r(&h, 0.45f, 1.0f), trace_world(o, r, &wn, &ws));
          int slot = -1;
          for (int i = 0; i < MAX_TRACERS; i++)
            if (g_tracers[i].life <= 0) { slot = i; break; }
          if (slot >= 0 && L > 0.5f) {
            float pl = v3_len(v3_sub(pt->b, pt->a));
            int delay = (int)fmaxf(0.0f, (pl - 3.0f) / pt->v);
            float v = pt->v * 0.75f;
            int life = delay + (int)((L + 2.2f) / v) + 4;
            g_tracers[slot] = (tracer_t){.a = o, .b = v3_add(o, v3_scale(r, L)), .d = r,
                                         .life = life, .life0 = life, .src = e->src,
                                         .wpn = pt->wpn, .v = v, .delay = delay, .rico = 1};
          }
        }
      }
    }
    break;
  }
  case EV_HIT:
  case EV_HIT_HEAD: {
    // A BODY UNDER FIRE SPARKS AT THE POINT THE ROUND LANDED ON. `a` is that
    // point and `b` is the shooter's origin, so the round's own direction is a
    // subtract and nothing here re-derives the hitscan. EVERY shooter gets it,
    // like the tracer and the world-pass muzzle star above: a bot shooting
    // another bot is a body under fire and the player is watching it. Only the
    // SCREEN-SPACE half of the confirmation — the hitmarker below — is the
    // player's alone. A KILLING round sparks too: ent_damage pushes EV_HIT
    // unconditionally and only the AUDIO suppresses it at hp 0, so copying that
    // gate here would silently make the sniper — 150 damage against 100 hp, so
    // every hit is a kill — the one weapon with no impact sparks at all.
    // Variety is hashed off (tick, victim, shooter, spark index) rather than
    // drawn from g_rng_vfx, for the reason spelled out at skid_h: out of the
    // shared stream the scatter of every later world impact would depend on
    // whether somebody across the map happened to be hit at the same moment.
    v3 d = v3_sub(e->a, e->b);
    float dl = v3_len(d);
    if (dl > 1e-4f) {
      d = v3_scale(d, 1.0f / dl);
      v3 t1, t2;
      axis_frame(d, &t1, &t2);
      float hot = e->type == EV_HIT_HEAD ? HITSPARK_HEAD_K : 1.0f;
      uint32_t base = (uint32_t)g_tick * 2654435761u +
                      (uint32_t)(e->dst + 1) * 2246822519u +
                      (uint32_t)(e->src + 1) * 3266489917u;
      impact_free_slot(HITSPARK_N + 1);   // ...+ the contact flare
      int n = 0;
      for (int i = 0; i < MAX_PARTICLES && n < HITSPARK_N; i++)
        if (g_parts[i].life <= 0) {
          uint32_t h = base + (uint32_t)n * 40503u;
          float sp = HITSPARK_BACK * hot * skid_r(&h, 0.55f, 1.0f);
          v3 v = v3_add(v3_scale(d, -sp),
                v3_add(v3_scale(t1, skid_r(&h, -HITSPARK_SIDE, HITSPARK_SIDE)),
                       v3_scale(t2, skid_r(&h, -HITSPARK_SIDE, HITSPARK_SIDE))));
          v.y += skid_r(&h, 0.15f, 1.30f);
          g_parts[i] = (particle_t){
              .pos = v3_add(e->a, v3_scale(d, HITSPARK_IN)),
              .vel = v, .y0 = fx_range_k(dl), .sz = hot,
              .life = HITSPARK_TICKS, .life0 = HITSPARK_TICKS, .kind = 4};
          n++;
        }
      fx_flare(v3_add(e->a, v3_scale(d, HITSPARK_IN)),
               0.052f * hot * fx_range_k(dl), 6);
      // ...and if this round turns out to have been the killing one, the
      // EV_KILL two pushes later needs the wound it landed on. See g_kill_wound.
      if (e->src == g_local_ent) g_kill_wound = (typeof(g_kill_wound)){e->a, d, dl, g_tick};
      // ...and for the VICTIM, whoever shot him: the client's ragdoll launch
      // reads this. Bounds-checked because dst is wire data on a client.
      if (e->dst >= 0 && e->dst < MAX_ENTS)
        g_wound[e->dst] = (typeof(g_wound[0])){e->a, e->b, g_tick, e->i, 0};
    }
    if (e->src == g_local_ent) {          // hitmarker: only the player's own hits
      // Armed on the weapon's OWN confirmation offset, so the marker lands in
      // the hole the impact spark leaves rather than on top of its peak. A kill
      // that arrives while a hit is still pending overwrites it below, which is
      // right: one round, one confirmation.
      int arm = (int)(WPN_DEF[g_vfx_wpn].sfx_hit_t * (float)TICK_HZ + 0.5f);
      g_hitmark_arm = arm > HITMARK_ARM_MIN ? arm : HITMARK_ARM_MIN;
      g_hitmark_pend = e->type == EV_HIT_HEAD ? HM_HEAD : HM_BODY;
      g_hitmark_at_pend = e->a;
      g_hitmark_plate_pend = hit_on_plate(e);
    } else if (e->dst == g_local_ent) {   // taking damage: flash + direction memory
      g_dmg_t = DMG_TICKS;
      // Refresh the slot already pointing that way (same shooter keeping fire on
      // you is ONE arc that stays lit, not four stacked ones), otherwise take
      // the most faded slot.
      {
        int best = 0, bl = 1 << 30;
        for (int i = 0; i < DMG_SLOTS; i++) {
          v3 d0 = v3_sub(g_dmg[i].from, e->b);
          if (g_dmg[i].life > 0 && v3_dot(d0, d0) < 9.0f) { best = i; bl = -1; break; }
          if (g_dmg[i].life < bl) { bl = g_dmg[i].life; best = i; }
        }
        g_dmg[best].from = e->b;
        g_dmg[best].life = DMG_TICKS;
      }
    }
    break;
  }
  case EV_KILL:
    if (!g_home && e->dst == g_local_ent) {
      g_ms.life = 0;
      g_cfg.st_deaths++;
    }
    if (!g_home && e->src == g_local_ent && e->dst != g_local_ent) {
      g_ms.life++;
      if (g_ms.life > g_ms.best) g_ms.best = g_ms.life;
      if (g_ms.best > g_cfg.st_best) g_cfg.st_best = g_ms.best;
      if (e->k) { g_ms.hs++; g_cfg.st_hs++; }
      g_cfg.st_kills++;
    }
    if (e->src == g_local_ent) {
      // The kill REPLACES whatever hit was still waiting to be armed — the two
      // are pushed on the same tick by ent_damage, and a smeared pair of
      // confirmations is exactly what the audio layer's EV_HIT suppression
      // exists to prevent. The delay itself is kept: the kill burst is the
      // brightest thing in the frame on the ticks that follow.
      int arm = (int)(WPN_DEF[g_vfx_wpn].sfx_hit_t * (float)TICK_HZ + 0.5f);
      g_hitmark_arm = arm > HITMARK_ARM_MIN ? arm : HITMARK_ARM_MIN;
      g_hitmark_pend = e->k ? HM_KILL_HEAD : HM_KILL;
      g_hitmark_t = 0;
      // SET, not accumulated — see HITSTOP_KILL. A second kill inside the window
      // re-arms it to the same length rather than extending it.
      g_hitstop = e->k ? HITSTOP_HEAD : HITSTOP_KILL;
      g_score_pop = SCORE_POP_TICKS;
      // The banner counts the SAME 3 s window the kill pitch does, but off its
      // OWN latch: g_multi_t is written in the audio DRAIN, which runs after
      // the ticks — and in the harness not at all unless the audio route is on
      // — so reading it here left the banner permanently dead in every
      // screenshot script and one frame stale in the window. The identical
      // hazard is documented (and fixed the same way) at g_vfx_wpn.
      if (g_tick - g_streak_last_t > MULTI_WIN) g_streak_n = 0;
      g_streak_last_t = g_tick;
      if (++g_streak_n >= 2) g_streak_t = STREAK_TICKS;
      // THE KILL BURST. Same tick as the wound or nothing at all.
      if (g_kill_wound.tick == g_tick) {
        v3 d = g_kill_wound.dir;   // the round's own direction of travel
        float hot = e->k ? HITSPARK_HEAD_K : 1.0f;   // headshot, same layer
                                                     // idiom as the body spark
                                                     // and the audio's head
                                                     // signature
        // Hashed off (tick, victim, headshot) and not drawn from g_rng_vfx, for
        // the reason spelled out at skid_h: out of the shared stream the
        // scatter of every later world impact would depend on whether somebody
        // across the map happened to die at the same moment.
        uint32_t base = (uint32_t)g_tick * 2654435761u +
                        (uint32_t)(e->dst + 1) * 2246822519u + 0x4B494C4Cu;
        // Angular sizing — see KILLBURST_REF. It scales the GEOMETRY only, and
        // it rides in `y0` rather than in `sz` because those are two different
        // questions: `sz` is the burst's HEAT and drives the colour, and folding
        // a range factor into it would make a 20 m kill 1.7x brighter as well
        // as bigger, pushing the halo past the display headroom that the
        // ONE/ONE blend leaves and clipping the whole thing to white.
        float rk = fx_range_k(g_kill_wound.dist);
        impact_free_slot(KILLBURST_N);
        int n = 0;
        for (int i = 0; i < MAX_PARTICLES && n < KILLBURST_N; i++)
          if (g_parts[i].life <= 0) {
            uint32_t h = base + (uint32_t)n * 40503u;
            // A GENUINE SPHERE, not a disc in the frame perpendicular to the
            // round. The disc was the obvious construction and it is wrong for
            // exactly the camera this game is usually watched from: a ring
            // built on (t1,t2) is EDGE ON to anyone standing off the shot line,
            // so a burst that read as a star down the sights collapsed to a
            // vertical bar from three metres to the side. The orientation the
            // round contributes belongs in the BIAS terms below, where it
            // shifts the whole cloud instead of flattening it.
            float cz = skid_r(&h, -1.0f, 1.0f);
            float cr = sqrtf(f_clamp(1.0f - cz * cz, 0.0f, 1.0f));
            float ca = skid_r(&h, 0.0f, F_TAU);
            float sp = KILLBURST_SP * hot * rk *
                       skid_r(&h, KILLBURST_SLOW, 1.0f);
            v3 v = {{cr * cosf(ca) * sp, cz * sp, cr * sinf(ca) * sp}};
            v = v3_add(v, v3_scale(d, -sp * KILLBURST_BACK));
            v.y += sp * KILLBURST_UP;
            g_parts[i] = (particle_t){
                .pos = v3_add(g_kill_wound.at, v3_scale(d, HITSPARK_IN)),
                .vel = v, .y0 = rk, .sz = hot,
                .life = KILLBURST_TICKS, .life0 = KILLBURST_TICKS, .kind = 5};
            n++;
          }
      }
    }
    memmove(g_feed + 1, g_feed, sizeof g_feed - sizeof g_feed[0]);
    g_feed[0] = (feed_t){e->src, e->dst, e->k, KILLFEED_TICKS};
    break;
  // Listed, not defaulted — see audio_on_event. A new event type that wants a
  // visual has to be answered here or the build fails.
  case EV_DRYFIRE:
  case EV_RELOAD:
    if (!e->k && (e->wpn == WPN_AR || e->wpn == WPN_SR) && e->src >= 0) {
      // Released at 21.5% of the reload (gun_anim's release beat), from the
      // magwell: hip height, a forearm in front of the body.
      float yaw = ent_yaw(e->src);
      v3 fw = {{sinf(yaw), 0.0f, -cosf(yaw)}}, rt = {{cosf(yaw), 0.0f, sinf(yaw)}};
      uint32_t mh = fx_hash((uint32_t)g_tick * 2246822519u ^ (uint32_t)e->src);
      wmag_t *q = &g_wmags[g_wmag_at];
      g_wmag_at = (g_wmag_at + 1) % MAX_WMAGS;
      *q = (wmag_t){.pos = v3_add(e->a, v3_add(v3_scale(fw, 0.34f), v3_add(v3_scale(rt, 0.06f),
                                                (v3){{0, 1.02f, 0}}))),
                    .vel = v3_add(v3_scale(rt, skid_r(&mh, -0.3f, 0.2f)), (v3){{0, -0.6f, 0}}),
                    .axis = v3_norm(v3_add(rt, (v3){{0, skid_r(&mh, -0.3f, 0.3f), 0}})),
                    .a = 0.0f, .spin = skid_r(&mh, 5.0f, 9.0f), .head = yaw + skid_r(&mh, -0.8f, 0.8f),
                    .life = 9 * TICK_HZ, .src = e->src, .wpn = e->wpn,
                    .var = ent_wvar(e->src, e->wpn),
                    .delay = (int)(0.215f * (float)WPN_DEF[e->wpn].reload_ticks)};
      q->prev = q->pos;
    }
    break;
  case EV_RELOAD_CANCEL:
    // A cancelled reload before the release never dropped anything.
    for (int i = 0; i < MAX_WMAGS; i++)
      if (g_wmags[i].life > 0 && g_wmags[i].src == e->src && g_wmags[i].delay > 0)
        g_wmags[i].life = 0;
    break;
  case EV_MELEE: break;
  case EV_NADE:
    if (!e->k && g_online) nade_add(e->a, e->b, e->i, e->src);
    if (e->k == 2)
      for (int i = 0; i < MAX_SPOONS; i++)
        if (g_spoons[i].life <= 0) {
          g_spoons[i] = (spoon_t){e->a, e->a, e->b, v3_norm((v3){{0.6f, 0.2f, 0.7f}}), 0.0f, 150};
          break;
        }
    break;
  case EV_BLAST:
    vfx_blast(e->a, e->g > 0.5f);
    if (g_online)                           // the local copy is spent
      for (int i = 0; i < MAX_NADES; i++)
        if (g_nades[i].fuse > 0 && v3_len(v3_sub(g_nades[i].pos, e->a)) < 2.5f)
          g_nades[i].fuse = 0;
    break;
  case EV_BLOCK: {
    // Steel on steel: a hard white star where the blades met. On a client this
    // is also how the two men learn what the server decided, since the guard's
    // jolt and the attacker's stagger are not in the snapshot.
    fx_flare(e->a, e->k ? 0.11f : 0.07f, 5);
    int vh = human_of_ent(e->dst), ah = human_of_ent(e->src);
    if (vh >= 0) g_players[vh].wp.block_k = e->k ? 1.0f : 0.6f;
    if (ah >= 0 && e->k) g_players[ah].wp.stagger_t = MELEE_STAGGER_TICKS;
    break;
  }
  case EV_END:
    // Once per match: LMS ends rounds without an EV_END, the rescue keeps its
    // own debrief and is not a match against anyone.
    if (!g_home && !rescue_view() && !g_ms.counted) {
      g_ms.counted = 1;
      g_cfg.st_matches++;
      if (won_by_me(e->k)) g_cfg.st_wins++;
    }
    break;
  case EV_RESPAWN:
  case EV_STEP:
  case EV_LAND:
    break;
  }
}

// A slide has TWO ground contacts and they are two different physical events,
// so they are two particle classes rather than one with a colour swap: a rubber
// sole dragged over grit lifts a mote that HANGS (drag beats gravity on
// something that light), and the glove's knuckle plate dragged across concrete
// at 20 m/s strikes hot fragments that ARC, SKIP and cool from white through
// orange to red. Rate, ejection speed, streak length and burst size all scale
// off ONE input — how fast the contact is actually being dragged — which is
// what makes a fast slide read as more violent than a slow one without a
// second set of numbers to keep in sync.
//
// A contact test has to be a height above the ENTITY'S OWN FLOOR, never above
// world zero. The old absolute 0.28 m gate silently turned the whole effect
// off on anything raised — the arena's centre platform is 0.55 m tall and
// every crate cluster has a 0.5-1.1 m step — so a slide across the one piece
// of geometry the whole map is built around threw nothing at all. The LOWER
// ANKLE is the floor reference a planted foot already provides, and it needs
// no new plumbing; 0.15 m above it lands within a centimetre of where the old
// absolute number sat on flat ground (lower ankle 0.10-0.14 in a slide).
#define SKID_CONTACT_DY 0.15f  // fist clearance over the foot that is planted
#define SKID_V_LO       3.4f   // m/s: barely dragging — a few lazy sparks
#define SKID_V_HI      21.5f   // m/s: a full-speed entry — peak violence
// SKID_V_HI is not a taste number, it is the entry speed, and getting it wrong
// took the speed scaling out of the effect entirely. EVERY slide in this game
// launches at the SAME speed — mv_slide_launch takes max(SLIDE BOOST, current)
// and SLIDE BOOST is 2.5 x RUN = 30 m/s — so the only thing that ever varies is
// the DECAY. Measured on a puppet slide, the smoothed anim velocity this
// emitter reads runs 21.7 -> 3.9 m/s over the 96 ticks the fist is down.
// Against the old ceiling of 15 that pinned `hot` at 1.0 for the first 30 of
// them: constant rate, constant burst, constant flare, i.e. a third of the
// shower with no speed in it at all — which is why a deliberately SLOW 4.5 m/s
// entry rendered within 13% of a full-speed one (2725 px of spark against
// 2417 at the same station). At 21.5 the curve is monotone from the first
// tick, and the late slide measures 178 px against the early slide's 2418.
#define SKID_ATTEMPTS   5      // spawn tries per contact tick; see `rate`
#define SKID_VY_MAX     5.30f  // m/s of lift — the E-SPORTS CEILING on this
// effect, and the reason it is a clamp rather than a range end: `rise` is
// scaled by heat and multiplied again on the strike tick, and three
// multipliers with nothing at the end of them is how an effect gets into the
// crosshair. GRAVITY is 22 and the spark integrator runs it at 1.2x, so
// apex = vy^2/(2*26.4) = 0.53 m — below the knee of a standing man and below
// the chest of a crouched one. Nothing this emitter can throw is able to cross
// the readable mass of a target standing in it.

// Variety from a HASH of (tick, slot) rather than from g_rng_vfx — the same
// trick fx_flash uses for its petals, and here it is load-bearing rather than
// tidy. This emitter wants six numbers per spark and up to four sparks a tick
// for EVERY sliding entity: 2880 draws a second per fast slider, against the
// handful an impact takes. Out of the shared stream, the scatter of a bullet
// impact would then depend on whether somebody across the map happens to be
// sliding, and how fast — one effect silently re-rolling another's noise is
// the exact defect sfx_alloc's per-sound reseed exists for, and it cost this
// file a round of A/Bs that were really measurements of noise realisation.
// Hashing costs nothing and is just as deterministic: g_tick and the pool slot
// both are, so two runs of one script stay byte-identical.
// Ticks of UNBROKEN knuckle contact, per entity (0 = the tick it touches
// down, -1 = not scraping), ENTITY-id indexed (0 = human 0, 1.. = bots, 21+ =
// humans) — so MAX_ENTS, per the three-range rule: this array was the latent
// out-of-bounds WRITE the id-space comment warns about, sized MAX_BOTS + 1
// while written through `ent`. It lives here rather than in anim_t because it
// is presentation state — nothing in the sim may read it — and
// vfx_match_reset already owns clearing everything else world-anchored. Two
// things need it and neither can be had statelessly: the STRIKE (a shower
// that starts at the rate it ends at has no impact in it — measured, the
// contact tick was byte-identical to the fiftieth) and the flare's continuity.
static short g_skid_t_all[SRV_LOBBIES][MAX_ENTS];
#define g_skid_t (g_skid_t_all[g_lobby])

