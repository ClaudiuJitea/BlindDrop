// code/render/corpses.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// --- corpses ---------------------------------------------------------------
// A BODY MUST OUTLIVE THE ENTITY THAT WAS IT. A dead bot IS its own corpse
// today: bot_tick's dead branch integrates the ragdoll and the moment
// `respawn_t` reaches zero `bot_spawn` teleports the same entity to a fresh
// point, so the body does not fall away, it BLINKS OUT — and the only lever on
// how long it lies there is the respawn delay, which is match pacing and must
// not move. So the pose is handed over: at the respawn the frozen skeleton, its
// weapon and its death state are copied into this pool, which owns the body
// from then on. It is presentation state in the same sense the tracers are —
// nothing in the sim may read it, and vfx_match_reset clears it.
//
// A corpse KEEPS MOVING after handover, on the same `rag_step` the entity ran,
// so it carries the full ragdoll, an interpolation twin and its own slack ramp.
// Measured over 2 seeds x 2500 ticks x 20 bots hard, 143 handovers: 1.4% are
// still airborne at the respawn and 8% are still travelling over 2 m/s — the
// respawn is 1.0 s (PLAYER_RESPAWN_TICKS 120) against 0.35-0.44 s of flight, so
// mid-flight handovers are rare BY CONSTRUCTION and it is the fast skids that
// made the old freeze-on-handover visible. The player's body is the case that
// makes the machinery mandatory: it enters the pool on the killing tick with a
// full launch still on it.
#define MAX_CORPSES     6      // sized against the vertex budget, not taste:
                               // see CORPSE_K
#define CORPSE_K        8      // the faceted profile tier. A body on the floor
                               // is scenery and does not need the 16-gon the
                               // living wear; measured, six corpses at their own
                               // distance tier plus a 20-bot hard brawl overran
                               // SCENE_MAX_VERTS by 60k and dropped triangles,
                               // which is holes in whatever is drawn last.
#define CORPSE_TICKS  540      // 4.5 s on the ground before it starts to go —
                               // long enough to walk past your own work, short
                               // enough that a 20-bot brawl is not a graveyard
#define CORPSE_DISS    18      // ...and 0.15 s to be GONE. This has now been
                               // both slower (84, with a travelling front) and
                               // in between (54): a body that takes a fifth of
                               // a second to leave is an event, one that takes
                               // most of a second is a cutscene you cannot
                               // skip, and in a firefight you are looking
                               // somewhere else by then anyway. The burst below
                               // is what carries the read; the geometry just
                               // has to get out of the way underneath it.
                               // break into pieces rather than blink out.
#define CORPSE_EMBERS  36      // fragments in the ONE-SHOT burst. Not a rate:
                               // the whole body leaves in a single spray on the
                               // first dissolve tick, which is the "puff". Six
                               // bodies x 36 x a 16-tick life is 345 against
                               // the 384-slot scenery region, and six
                               // simultaneous dissolves is already the
                               // pool-full case.
typedef struct {
  anim_t   anim;    // the skeleton, frozen in shape but not in PLACE
  rag_t    rag;     // ...and still integrating on the SHARED integrator, so a
                    // body handed over mid-flight goes on flying, tumbling,
                    // bouncing and slamming exactly as it did as an entity
  gunanim_t gs;     // the weapon's own state, so a body killed mid-reload does
                    // not snap its trigger and bolt closed on handover
  v3       pos, prev_pos;
  float    yaw;
  float    slack, prev_slack;  // the limb-collapse blend, ramped HERE as well as
                    // on the entity: a bot hands over a second after it died
                    // and is already fully slack, but the player's body enters
                    // the pool on the killing tick and would otherwise snap
                    // straight from a firing stance into a sprawl in one frame
  uint32_t seed;    // its own dissolve pattern
  int      born;    // the tick it was handed over. bot_tick steps the ENTITY
                    // and then hands the already-stepped body to the pool, and
                    // `vfx_tick` runs later in the SAME player_tick — so
                    // without this the handover tick integrated gravity, drag,
                    // friction and the topple twice (measured: 2.1x the tilt
                    // step and 2.3x the position step on that one tick), which
                    // is a one-frame pop at exactly the seam this whole change
                    // exists to remove. The player's body, added AFTER
                    // vfx_tick, got zero steps instead: one event, two
                    // integration counts.
  int      wpn, pal, life, used;
  int      owner;   // entity id the body was (0 = player, 1.. = bots): the
                    // `rag`/`ragx` proofs follow ONE body across the handover,
                    // and without ownership any other kill landing in their
                    // observation window silently re-targeted the follow
} corpse_t;
static corpse_t g_corpses_all[SRV_LOBBIES][MAX_CORPSES];
#define g_corpses (g_corpses_all[g_lobby])

// Hand a body over to the pool. Called from the sim at the respawn, exactly
// like ev_push: presentation state written by the tick, never read back.
// The caller's interpolation twins (prev_pos, prev_slack, A->prev_pose) come
// WITH the body: initialising them to the current values discarded twins both
// callers were already maintaining, so the corpse's first rendered frame
// interpolated against itself — a one-frame position/pose pop at exactly the
// handover seam this pool exists to hide.
static void corpse_add(const anim_t *A, v3 pos, v3 prev_pos, rag_t rag,
                       float yaw, float slack, float prev_slack, gunanim_t gs,
                       int wpn, int pal, int owner) {
  int slot = -1, oldest = 1 << 30;
  for (int i = 0; i < MAX_CORPSES; i++) {
    if (!g_corpses[i].used) { slot = i; break; }
    if (g_corpses[i].life < oldest) { oldest = g_corpses[i].life; slot = i; }
  }
  g_corpses[slot] = (corpse_t){*A, rag, gs, pos, prev_pos, yaw,
                               slack, prev_slack,
                               fx_hash((uint32_t)g_tick * 2654435761u +
                                       (uint32_t)slot * 40503u),
                               (int)g_tick, wpn, pal,
                               CORPSE_TICKS + CORPSE_DISS, 1, owner};
}

// Live particles at the worst tick, and how many of them were in the RESERVED
// region below PARTS_SKID_LO. The body burst is nearly three times the stone
// one now, so the reserve's headroom stopped being something to reason about
// and became something to print: `budget` reports both.
static int g_part_peak, g_part_res_peak;

static float skid_h(uint32_t *h) {
  *h = fx_hash(*h);
  return (float)(*h >> 8) * (1.0f / 16777216.0f);
}
static float skid_r(uint32_t *h, float lo, float hi) {
  return lo + (hi - lo) * skid_h(h);
}

// Ground sparks off a DEAD body that is still moving. Driven off rag_t — the
// integrator's own `grounded` and horizontal velocity — and deliberately NOT
// off anim_t: a dead pose is frozen the tick the player stops being solved,
// and reading it as motion is the exact defect anim_t.live_t was added for
// (vfx_skid once ran a spark fountain nailed to a corpse through the whole
// death screen). The ragdoll velocity is the honest number — it is what
// actually moves the body. Called from rag_advance, so both owners (the
// entity's dead branch and the corpse pool) get it from one place.
#define RAG_SPARK_V   2.20f    // m/s of grind below which concrete just scuffs
static void vfx_rag_sparks(const rag_t *r, v3 pos) {
  if (!r->grounded) return;
  float sp = sqrtf(r->vel.x * r->vel.x + r->vel.z * r->vel.z);
  if (sp < RAG_SPARK_V) return;
  if ((g_tick & 1) == 0) return;   // every other tick: one sliding body costs
                                   // the scenery region what one sliding
                                   // PLAYER does, and chatter reads better
                                   // than a continuous hose
  float hot = f_clamp((sp - RAG_SPARK_V) / 4.0f, 0.0f, 1.0f);
  int n = 1 + (int)(hot * 2.0f);
  uint32_t h = (uint32_t)g_tick * 2246822519u ^ (uint32_t)(int)(pos.x * 977.0f);
  float inv = 1.0f / sp;
  for (int e = 0; e < n; e++) {
    int slot = -1;
    for (int i = PARTS_SKID_LO; i < MAX_PARTICLES; i++)
      if (g_parts[i].life <= 0) { slot = i; break; }
    if (slot < 0) return;
    // Thrown BACKWARD along the drag and splayed up: the direction a grain
    // torn off a moving contact actually leaves in, same as the boot skid.
    float back = sp * skid_r(&h, 0.18f, 0.48f);
    v3 v = {{-r->vel.x * inv * back + skid_r(&h, -1.1f, 1.1f),
             skid_r(&h, 1.0f, 2.9f),
             -r->vel.z * inv * back + skid_r(&h, -1.1f, 1.1f)}};
    float y = pos.y + 0.05f;
    g_parts[slot] = (particle_t){
        .pos = {{pos.x + skid_r(&h, -0.25f, 0.25f), y,
                 pos.z + skid_r(&h, -0.25f, 0.25f)}},
        .vel = v, .y0 = pos.y, .sz = 0.7f + 0.5f * hot,
        .life = 18, .life0 = 18, .kind = 2};
  }
}

static void vfx_skid(const anim_t *A, int ent) {
  short *clk = &g_skid_t[ent];
  float sp = sqrtf(A->vel_s.x * A->vel_s.x + A->vel_s.z * A->vel_s.z);
  // A body in the air scrapes nothing, and only a real skid marks a floor.
  if (A->slide_s < 0.55f || A->air_s > 0.35f || sp < 3.0f) { *clk = -1; return; }
  float hot = f_clamp((sp - SKID_V_LO) / (SKID_V_HI - SKID_V_LO), 0.0f, 1.0f);

  // --- boot: dust ----------------------------------------------------------
  // The old emitter, unchanged in rate, velocity, lifetime and level: a boot
  // skidding raises dust and a knuckle on concrete throws sparks, and only the
  // second one needed rewriting. The lower ankle IS the planted boot, so with
  // the airborne case already rejected above there is nothing left for a
  // height gate to say.
  v3 ank = A->pose.ankle[0].y < A->pose.ankle[1].y ? A->pose.ankle[0]
                                                  : A->pose.ankle[1];
  if ((g_tick % 3) == 1)
    for (int i = 0; i < MAX_PARTICLES; i++)
      if (g_parts[i].life <= 0) {
        v3 v = {{-A->vel_s.x * 0.22f + rng_rangef(&g_rng_vfx, -0.5f, 0.5f),
                 rng_rangef(&g_rng_vfx, 0.5f, 1.4f),
                 -A->vel_s.z * 0.22f + rng_rangef(&g_rng_vfx, -0.5f, 0.5f)}};
        float y = fmaxf(ank.y - 0.04f, 0.02f);   // just under the sole
        g_parts[i] = (particle_t){.pos = {{ank.x, y, ank.z}}, .vel = v,
                                  .y0 = y, .sz = 1.0f,
                                  .life = PARTICLE_TICKS + 6,
                                  .life0 = PARTICLE_TICKS + 6, .kind = 1};
        break;
      }

  // --- knuckle: sparks -----------------------------------------------------
  v3 fist = A->pose.wri[0];                     // the dragging brace glove
  // The contact gate is a SCHMITT TRIGGER, the same idiom as the pad's
  // 0.30/0.20 analog trigger and for the same reason: `strike` fires on every
  // 0->1 edge, so a fist that chatters across one threshold — which it does at
  // the end of every slide (measured: fist clearance -0.015 m at t=265,
  // +0.018 at t=271) and over any step in the floor — would slap the ground
  // twice.
  if (fist.y > ank.y + (*clk < 0 ? SKID_CONTACT_DY : SKID_CONTACT_DY + 0.05f)) {
    *clk = -1;
    return;
  }
  int strike = *clk < 0;                        // the tick the knuckle lands
  *clk = (short)(*clk < 0 || *clk > 30000 ? 0 : *clk + 1);
  // Rate is CONTINUOUS, not a period/burst pair. The old gate emitted
  // 1+(int)(2*hot) sparks every period-th tick — three integer steps over the
  // whole speed range, and a 40 Hz strobe at the bottom of it. The FLARE rode
  // the same gate, so the one thing that marks WHERE the contact is blinked
  // out two ticks in three for the entire tail of a slide. Here a fixed number
  // of attempts each survive with probability `rate`: smooth, stateless, and —
  // because the draw comes from a hash of (tick, entity, attempt) rather than
  // of the POOL SLOT — a spark no longer changes what it looks like because
  // somebody else's shower took the low slots first.
  float rate = hot * (0.30f + 0.70f * hot);     // 0.30 at a crawl, 1.0 flat out
  int attempts = SKID_ATTEMPTS;
  if (strike) { rate = 1.0f; attempts = SKID_ATTEMPTS * 2; }   // the slap
  float sy = fmaxf(fist.y - 0.03f, 0.015f);     // the struck plane
  v3 fwd = {{A->vel_s.x / sp, 0.0f, A->vel_s.z / sp}};    // the fan's axis
  v3 lat = {{-fwd.z, 0.0f, fwd.x}};                       // across it
  for (int n = 0; n < attempts; n++) {
    uint32_t h = (uint32_t)g_tick * 2654435761u + (uint32_t)ent * 2246822519u +
                 (uint32_t)n * 3266489917u;
    if (skid_h(&h) > rate) continue;
    int slot = -1;
    for (int i = PARTS_SKID_LO; i < MAX_PARTICLES; i++)
      if (g_parts[i].life <= 0) { slot = i; break; }
    if (slot < 0) break;   // pool pressure thins the spray; it never breaks it
    // A HERO is bigger, brighter and three times as long-lived, so a handful
    // of arcs carry across the frame while the cheap majority stays a tight
    // bright spray at the contact. All-cheap reads as a fizz, all-hero as a
    // firework, and neither is a knuckle on concrete. Read this fraction
    // against the LIFETIMES and not off this line: at 0.20 spawned, with 38
    // ticks of life against the crowd's 15.5, heroes were 38% of what is on
    // screen at any instant. 0.14 puts them at 28%.
    int hero = skid_h(&h) < 0.14f;
    // ALONG THE TRAVEL: what the fragment KEPT of the skid. A knuckle tears
    // grit off a floor it is running over, so a fragment is DRAGGED and then
    // left behind as the hand outruns it — it is not fired backward out of a
    // gun. The old model ejected at 16-42% of the skid BACKWARD IN WORLD,
    // which put 25-31 m/s between fragment and contact and stretched the
    // shower into a stripe: measured on a 1280x720 frame, 624 px wide and
    // 110 px tall, i.e. half the screen at an 8:1 aspect, which reads as speed
    // lines painted on the floor rather than as a spray. Keeping most of it
    // forward halves the separation rate (measured 624 -> 368 px) and the
    // SPREAD in what is kept is what gives the trail a dense mouth and a thin
    // tail instead of a uniform stripe.
    float keep = hero ? skid_r(&h, -0.10f, 0.82f) : skid_r(&h, -0.06f, 0.50f);
    float side = skid_r(&h, -0.115f, 0.115f) * (strike ? 1.6f : 1.0f);
    // LIFT is the primary spread, not an afterthought. Measured on the old
    // emitter the MEDIAN spark sat 15 mm off the floor against a 1.5 m
    // horizontal spread — a 1:100 fan, which is a line, and it is exactly what
    // the renders showed. A grind fan is a plane standing ON the surface:
    // shallow for most fragments and steep for a few, so u*u keeps the crowd
    // grazing and puts the tail up where it reads against the body. Scaled by
    // heat so a crawl barely lifts, then clamped — see SKID_VY_MAX.
    float u    = skid_h(&h);
    float rise = (0.85f + 4.60f * u * u) * (0.34f + 0.66f * hot) *
                 (strike ? 1.25f : 1.0f);
    int   lf  = hero ? 30 + (int)skid_r(&h, 0.0f, 16.0f)
                     : 10 + (int)skid_r(&h, 0.0f, 11.0f);
    // sz is size AND heat AND streak length in one number, so folding `hot`
    // into it is what makes a dying skid throw a few small dull embers instead
    // of the same spark less often.
    float sz  = (hero ? skid_r(&h, 1.50f, 2.10f) : skid_r(&h, 0.70f, 1.05f)) *
                (0.62f + 0.38f * hot) * (strike ? 1.22f : 1.0f);
    v3 v = v3_add(v3_scale(fwd, sp * keep), v3_scale(lat, sp * side));
    v.y = fminf(rise, SKID_VY_MAX);
    // Scatter the ORIGIN a few centimetres too. Every fragment leaving one
    // mathematical point makes the spray read as a fan of rays from a pin;
    // a knuckle plate is 60 mm across and the spray has to have a mouth.
    v3 o = v3_add(v3_add((v3){{fist.x, sy, fist.z}},
                         v3_scale(lat, skid_r(&h, -0.032f, 0.032f))),
                  v3_scale(fwd, skid_r(&h, -0.020f, 0.020f)));
    g_parts[slot] = (particle_t){.pos = o, .vel = v,
                                 .y0 = sy, .sz = sz,
                                 .life = lf, .life0 = lf, .kind = 2};
  }
  // The flare anchors the spray to a POINT. Without it the sparks read as
  // spawning in mid-air a hand's width off the body — a fragment is gone from
  // the contact within two ticks and nothing marks where it came from. EVERY
  // contact tick and not every period-th one, so the hot spot is steady rather
  // than strobing through the whole tail of a slide; the strike tick gets a
  // wider, brighter one, and that is the only frame in this effect allowed to
  // be big.
  for (int i = PARTS_SKID_LO; i < MAX_PARTICLES; i++)
    if (g_parts[i].life <= 0) {
      float k = strike ? 1.55f : 1.0f;
      g_parts[i] = (particle_t){
          .pos = {{fist.x, sy + 0.022f, fist.z}},
          .vel = {{0.0f, 0.0f, 0.0f}},                   // unused for kind 3
          .y0 = sy, .sz = (0.032f + 0.030f * hot) * k,   // arm half-length
          .life = 4, .life0 = 4, .kind = 3};
      break;
    }
}

// The particle census. Sampled TWICE per tick — see vfx_tick's own comment for
// why the top sample is not redundant with the bottom one.
static void part_census(void) {
  int live = 0, res = 0;
  for (int i = 0; i < MAX_PARTICLES; i++)
    if (g_parts[i].life > 0) { live++; if (i < PARTS_SKID_LO) res++; }
  if (live > g_part_peak) g_part_peak = live;
  if (res > g_part_res_peak) g_part_res_peak = res;
}

static void vfx_tick(void) {
  // TWO samples per tick, both feeding the same maxima. This TOP one is the
  // reserved region's true worst instant: event-driven impact/hit/kill spawns
  // landed during the entity ticks and this function's reap has not run yet —
  // sampled only after the reap, a tick where the reserve genuinely hit its
  // ceiling mid-tick printed below it, on the one counter whose job is proving
  // the reserve never clamps. The END sample (below) catches this function's
  // own spawns (skid dust/sparks/flares, corpse embers).
  part_census();
  // The player's emitter is gated on ALIVE for the same reason a bot's is, and
  // it is not symmetry for its own sake: player_tick stops calling anim_tick the
  // moment the player dies (it only copies prev_pose), so g_player_anim FREEZES
  // with whatever slide_s and vel_s it held on the death tick. Dying mid-slide
  // therefore left a fist parked on the floor at a permanently non-zero skid
  // speed, and the emitter ran every tick of the 3 s death screen and straight
  // through the respawn — a spark fountain nailed to the corpse. The bots were
  // gated from the start; the player never was.
  for (int h = 0; h < MAX_HUMANS; h++)   // P1-59: every human skids, own id
    if (g_humans_on[h] && g_tick - g_player_anim[h].live_t <= 1)
      vfx_skid(&g_player_anim[h], ent_of_human(h));
  for (int b = 0; b < g_match.nbots; b++)
    if (g_bots[b].active && g_bots[b].alive) vfx_skid(&g_bots[b].anim, b + 1);
  for (int i = 0; i < MAX_TRACERS; i++)
    if (g_tracers[i].life > 0) g_tracers[i].life--;
  for (int i = 0; i < MAX_FLASHES; i++)
    if (g_flashes[i].life > 0) g_flashes[i].life--;
  for (int i = 0; i < MAX_DUST; i++)
    if (g_dust[i].life > 0) {        // thrown off the face, braked hard, settling
      dust_t *q = &g_dust[i];
      q->life--;
      q->vel = v3_scale(q->vel, 0.88f);
      q->vel.y -= 0.25f * TICK_DT;
      q->pos = v3_add(q->pos, v3_scale(q->vel, TICK_DT));
      if (q->pos.y < q->r0) q->pos.y = q->r0;
    }
  if (g_online) nade_tick(0);
  for (int i = 0; i < MAX_WMAGS; i++)
    if (g_wmags[i].life > 0) {
      wmag_t *q = &g_wmags[i];
      if (q->delay > 0) {   // still in the gun: ride along with the shooter
        q->delay--;
        float yaw = ent_yaw(q->src);
        v3 fw = {{sinf(yaw), 0.0f, -cosf(yaw)}}, rt = {{cosf(yaw), 0.0f, sinf(yaw)}};
        q->pos = v3_add(ent_pos(q->src), v3_add(v3_scale(fw, 0.34f),
                        v3_add(v3_scale(rt, 0.06f), (v3){{0, 1.02f, 0}})));
        q->prev = q->pos;
        continue;
      }
      q->life--;
      q->prev = q->pos;
      if (q->rest) {        // the last two seconds: sink into the floor
        if (q->life < 2 * TICK_HZ) q->pos.y -= 0.03f * TICK_DT;
        continue;
      }
      q->vel.y -= 9.81f * TICK_DT;
      q->pos = v3_add(q->pos, v3_scale(q->vel, TICK_DT));
      q->a += q->spin * TICK_DT;
      float fl = rag_support(q->pos) + 0.013f;
      if (q->pos.y < fl && q->vel.y < 0.0f) {
        q->pos.y = fl;
        if (q->vel.y > -1.2f) { q->rest = 1; q->vel = (v3){{0, 0, 0}}; }
        else {
          q->vel = (v3){{q->vel.x * 0.35f, -q->vel.y * 0.22f, q->vel.z * 0.35f}};
          q->spin *= 0.4f;
          // A polymer box on concrete: a dull clack, by distance.
          v3 dl = v3_sub(q->pos, g_cam_eye);
          float dd = v3_len(dl);
          float pan = dd > 1e-3f ? f_clamp(v3_dot(dl, R.cam_rgt) / dd, -1.0f, 1.0f) : 0.0f;
          if (dd < 14.0f)
            audio_play(SFX_NADE_BOUNCE, 0.45f / (1.0f + dd * 0.3f), 1.55f, pan, dd / 343.0f);
        }
      }
    }
  for (int i = 0; i < MAX_CHIPS; i++)
    if (g_chips[i].life > 0) {
      chip_t *q = &g_chips[i];
      q->life--;
      q->prev = q->pos;
      if (q->spin == 0.0f) continue;            // at rest
      q->vel.y -= 9.81f * TICK_DT;
      q->pos = v3_add(q->pos, v3_scale(q->vel, TICK_DT));
      q->a += q->spin * TICK_DT;
      float fl = rag_support(q->pos) + q->sz * 0.5f;
      if (q->pos.y < fl && q->vel.y < 0.0f) {
        q->pos.y = fl;
        q->vel = (v3){{q->vel.x * 0.45f, -q->vel.y * 0.30f, q->vel.z * 0.45f}};
        q->spin *= 0.5f;
        if (q->vel.y < 0.35f) { q->vel = (v3){{0, 0, 0}}; q->spin = 0.0f; }
      }
    }
  // THE BARREL COOLS, and while it is still hot after a burst it smokes: thin
  // wisps rising off the muzzle once the trigger is off.
  g_wpn_heat *= expf(-0.32f * TICK_DT);
  if (g_wpn_heat > 0.55f && g_tick - g_wpn_heat_tick > 8 && (g_tick % 6) == 0 &&
      g_fx_muz_live_vis) {
    uint32_t wh = fx_hash((uint32_t)g_tick * 0x9E3779B9u);
    float k = f_clamp((g_wpn_heat - 0.55f) * 1.4f, 0.0f, 1.0f);
    dust_add(g_fx_muzzle_vis, (v3){{skid_r(&wh, -0.05f, 0.05f), skid_r(&wh, 0.18f, 0.34f), skid_r(&wh, -0.05f, 0.05f)}},
             v3_scale((v3){{0.20f, 0.20f, 0.21f}}, 0.6f + 0.6f * k), 0.008f, skid_r(&wh, 0.04f, 0.07f),
             (int)skid_r(&wh, 70.0f, 120.0f));
    // A wisp lives at the muzzle, half a metre from the lens: it opts out of
    // the near-lens ease every other burst takes (see the dust draw).
    g_dust[(g_dust_at + MAX_DUST - 1) % MAX_DUST].wisp = 1;
  }
  for (int i = 0; i < MAX_SPOONS; i++)
    if (g_spoons[i].life > 0) {
      spoon_t *q = &g_spoons[i];
      q->life--;
      q->prev = q->pos;
      q->vel.y -= 9.81f * TICK_DT;
      q->pos = v3_add(q->pos, v3_scale(q->vel, TICK_DT));
      q->a += 0.42f;
      if (q->pos.y < 0.03f) {
        q->pos.y = 0.03f;
        q->vel = (v3){{q->vel.x * 0.4f, -q->vel.y * 0.28f, q->vel.z * 0.4f}};
      }
    }
  g_shake *= expf(-4.2f * TICK_DT);
  for (int i = 0; i < MAX_SHOCK; i++)
    if (g_shock[i].life > 0) g_shock[i].life--;
  for (int i = 0; i < MAX_BRANDS; i++)
    if (g_brands[i].life > 0) {
      brand_t *q = &g_brands[i];
      q->life--;
      q->vel.y -= 9.81f * TICK_DT;
      q->vel = v3_scale(q->vel, 0.996f);
      q->pos = v3_add(q->pos, v3_scale(q->vel, TICK_DT));
      if (q->pos.y < rag_support(q->pos)) q->life = 0;
      else if ((g_tick & 3) == 0) {
        float f = (float)q->life / (float)q->life0;
        puff_add(q->pos, v3_scale(q->vel, 0.06f), 0.09f, rng_rangef(&g_rng_vfx, 0.35f, 0.75f),
                 0.10f + 0.16f * f, -0.5f, 0.985f, (int)rng_rangef(&g_rng_vfx, 180, 420), 0.8f);
      }
    }
  g_blast_light *= expf(-12.0f * TICK_DT);
  for (int i = 0; i < MAX_FIRE; i++)
    if (g_fire[i].life > 0) {
      g_fire[i].life--;
      g_fire[i].pos = v3_add(g_fire[i].pos, g_fire[i].vel);
    }
  for (int i = 0; i < MAX_SCORCH; i++)
    if (g_scorch[i].life > 0) g_scorch[i].life--;
  for (int i = 0; i < MAX_PUFFS; i++)
    if (g_puffs[i].life > 0) {
      puff_t *q = &g_puffs[i];
      q->life--;
      q->vel.y -= q->grav * TICK_DT;
      if (q->r1 >= 0.2f) {
        // Smoke rolls: a little eddy that depends on where it is, and a light wind.
        q->vel.x += (0.55f + sinf(q->pos.y * 1.3f + q->pos.z * 0.7f + (float)i) * 1.5f) * TICK_DT;
        q->vel.z += cosf(q->pos.y * 1.1f + q->pos.x * 0.7f + (float)i * 1.7f) * 1.5f * TICK_DT;
      }
      q->vel = v3_scale(q->vel, q->drag);
      q->pos = v3_add(q->pos, v3_scale(q->vel, TICK_DT));
      if (q->pos.y < q->r0 * 0.5f) {
        q->pos.y = q->r0 * 0.5f;
        if (q->r1 >= 0.2f) q->vel.y = 0.0f;             // dust runs on along the floor
        else q->vel = (v3){{0, 0, 0}};
      }
    }
  for (int i = 0; i < MAX_PARTICLES; i++)
    if (g_parts[i].life > 0) {
      particle_t *q = &g_parts[i];
      if (q->kind == 3) { q->life--; continue; }   // flare: welded to the contact
      if (q->kind == 1) {          // dust hangs — drag beats gravity on a mote
        q->vel.y -= GRAVITY * 0.10f * TICK_DT;
        q->vel = v3_scale(q->vel, 0.94f);
      } else {                     // sparks arc, and hard
        q->vel.y -= GRAVITY * 1.2f * TICK_DT;
        if (q->kind == 2) q->vel = v3_scale(q->vel, 0.982f);  // air on an ember
        else if (q->kind == 5) q->vel = v3_scale(q->vel, KILLBURST_DRAG);
      }
      q->pos = v3_add(q->pos, v3_scale(q->vel, TICK_DT));
      // SKIP. A fragment that reaches the floor still moving does not stop, it
      // bounces, and that second lower arc is most of what separates "sparks"
      // from "orange confetti". Impact sparks are deliberately NOT bounced:
      // they are struck off a wall at any orientation, and y0 only means a
      // floor for something struck off the floor.
      // A CASING SKIPS ONCE AND LIES STILL. `y0` is the tumble phase on this
      // kind, not a floor, so the floor is queried live: "world zero is fine,
      // brass is only ejected over open ground" was false the moment the
      // player fought on the 0.55 m centre platform — the map's one guaranteed
      // landmark — where every case fell through the top and settled half a
      // metre inside it. rag_support is the same one-copy surface query the
      // corpses use; ~10 live casings make it cheap.
      if (q->kind == 6 && q->vel.y < 0.0f) {
        float fl = rag_support(q->pos) + 0.012f;
        if (q->pos.y < fl) {
          q->pos.y = fl;
          if (q->vel.y < -0.6f) {   // a real landing, not the settling chatter
            v3 dl = v3_sub(q->pos, g_cam_eye);
            float dd = v3_len(dl);
            float pan = dd > 1e-3f ? f_clamp(v3_dot(dl, R.cam_rgt) / dd, -1.0f, 1.0f) : 0.0f;
            float gn = f_clamp(-q->vel.y * 0.28f, 0.15f, 1.0f) / (1.0f + dd * 0.35f);
            audio_play(SFX_BRASS, gn * 0.85f, 0.92f + 0.16f * q->sz - 0.08f, pan, 0.0f);
          }
          q->vel.y *= -0.34f;
          q->vel.x *= 0.55f;
          q->vel.z *= 0.55f;
          if (q->vel.y < 0.4f) q->vel = (v3){{0, 0, 0}};   // settled: stop tumbling
        }
      }
      if (q->kind == 2 && q->pos.y < q->y0 && q->vel.y < 0.0f) {
        q->pos.y = q->y0;
        q->vel.y *= -0.38f;
        q->vel.x *= 0.72f;
        q->vel.z *= 0.72f;
      }
      q->life--;
    }
  // Corpses: count down, and throw a few embers while they come apart. The
  // fragments are the same kind-4 body spark the hits use, so the dissolve is
  // made of the material the body was already shedding.
  // A FULL POOL MUST START A DISSOLVE, NEVER A POP. Eviction is the NORMAL case
  // at high bot counts, not an edge: instrumented at 20 bots hard, the arena
  // wants 27-32 concurrent bodies for a 4.5 s lie and has six slots, so 94
  // corpses were deleted in 30 s and every one of them mid-lie at a mean age of
  // 1.65 s — i.e. at the top bot count the feature undid itself and bodies
  // blinked out exactly as they used to. Forcing the oldest into its dissolve
  // while the pool is full turns those 94 pops into 94 dissolves, because the
  // next kill lands ~32 ticks later and the 54-tick dissolve is already most of
  // the way through.
  {
    int used = 0, oldest = -1, low = 1 << 30;
    for (int c = 0; c < MAX_CORPSES; c++) {
      if (!g_corpses[c].used) continue;
      used++;
      if (g_corpses[c].life < low) { low = g_corpses[c].life; oldest = c; }
    }
    if (used == MAX_CORPSES && oldest >= 0 && g_corpses[oldest].life > CORPSE_DISS)
      g_corpses[oldest].life = CORPSE_DISS;
  }
  for (int c = 0; c < MAX_CORPSES; c++) {
    corpse_t *cp = &g_corpses[c];
    if (!cp->used) continue;
    if (cp->born == (int)g_tick) continue;   // handed over this tick — already
                                             // stepped by whoever owned it
    if (--cp->life <= 0) { cp->used = 0; continue; }
    // FINISH THE FLIGHT THE ENTITY STARTED, on the same integrator it started
    // on. The handover happens at the respawn, one second after the kill, and a
    // body launched at up to 10.4 m/s is usually still in the air then — the
    // old pool finished only the horizontal SKID, so a corpse handed over
    // mid-cartwheel snapped level and slid. `rag_step` is the whole physics and
    // there is exactly one copy of it; the frozen pose travels with the body
    // exactly as it did inside bot_tick.
    cp->prev_pos = cp->pos;
    cp->prev_slack = cp->slack;
    rag_advance(&cp->rag, &cp->pos, cp->prev_pos, &cp->slack, &cp->anim);
    if (cp->life != CORPSE_DISS) continue;   // ONE tick, ONE spray
    // THE PUFF — the whole body leaves in a single spray on the first dissolve
    // tick. A rate emitter cannot do this: spread over the dissolve it is a
    // trickle whatever the count, and a trickle reads as smoke.
    //
    // SPAWNED AROUND cp->pos, NOT AT THE POSE JOINTS. anim_t.pose holds the
    // frozen STANDING skeleton — a corpse only lies down because rag_death
    // rotates it at draw time (scene_draw_figure takes the death_t separately)
    // — so a joint reads y = 0.90 for a body whose own ragx height is 0.46.
    // Measured: the fragments came off at hip height ABOVE a body lying flat,
    // a floating puff with the corpse underneath it. Re-deriving the rendered
    // joints here would be a second copy of rag_death; the body's occupied
    // VOLUME is what the effect actually needs, and cp->pos plus the lying
    // extent is that volume.
    uint32_t h = (uint32_t)g_tick * 2654435761u + (uint32_t)c * 40503u;
    v3 mid = v3_add(cp->pos, (v3){{0.0f, 0.22f, 0.0f}});
    for (int e = 0; e < CORPSE_EMBERS; e++) {
      int slot = -1;
      for (int i = PARTS_SKID_LO; i < MAX_PARTICLES; i++)
        if (g_parts[i].life <= 0) { slot = i; break; }
      if (slot < 0) break;
      // A point in the lying body's box: wide and flat, which is the shape a
      // body on the ground actually has.
      v3 pj = {{mid.x + skid_r(&h, -0.55f, 0.55f),
                cp->pos.y + skid_r(&h, 0.04f, 0.44f),
                mid.z + skid_r(&h, -0.55f, 0.55f)}};
      v3 d = v3_sub(pj, mid);
      float dl = sqrtf(v3_dot(d, d));
      d = dl > 0.05f ? v3_scale(d, 1.0f / dl)
                     : (v3){{skid_r(&h, -1.0f, 1.0f), 0.4f,
                             skid_r(&h, -1.0f, 1.0f)}};
      // SIZED AGAINST THE BODY, NOT AGAINST TASTE: 16 ticks is 0.13 s, so a
      // spray that should be about as wide as the thing that vanished needs
      // ~9 m/s. Judged at 5.5 m — at 1.8 m everything looks generous.
      float sp = skid_r(&h, 6.0f, 13.0f);
      v3 v = {{d.x * sp + skid_r(&h, -1.4f, 1.4f),
               d.y * sp * 0.45f + skid_r(&h, 0.9f, 2.4f),
               d.z * sp + skid_r(&h, -1.4f, 1.4f)}};
      g_parts[slot] = (particle_t){.pos = pj, .vel = v, .y0 = cp->pos.y,
                                   .sz = skid_r(&h, 1.10f, 1.90f),
                                   .life = 16, .life0 = 16, .kind = 2};
    }
  }
  if (g_muzzle_t > 0) g_muzzle_t--;
  if (g_hitmark_arm > 0 && --g_hitmark_arm == 0) {
    g_hitmark_kind = g_hitmark_pend;
    g_hitmark_at = g_hitmark_at_pend;
    g_hitmark_plate = g_hitmark_plate_pend && g_hitmark_pend < HM_KILL;
    g_hitmark_t = g_hitmark_kind >= HM_KILL ? KILLMARK_TICKS : HITMARK_TICKS;
    // A hit KICKS the crosshair open and it springs back — the reaction lands
    // on the one element the player is already staring at, and it costs one
    // float. A kill kicks 2.4x as hard, which is the only place the reticle
    // says anything about consequence.
    g_ch_kick += g_hitmark_kind >= HM_KILL ? 3.0f : 1.2f;
  }
  if (g_hitmark_t > 0) g_hitmark_t--;
  g_ch_kick *= 0.87f;   // ~8 ticks to a third: a snap, not a breath
  if (g_dmg_t > 0) g_dmg_t--;
  for (int i = 0; i < KILLFEED_N; i++)
    if (g_feed[i].life > 0) g_feed[i].life--;
  if (g_score_pop > 0) g_score_pop--;
  for (int i = 0; i < DMG_SLOTS; i++)
    if (g_dmg[i].life > 0) g_dmg[i].life--;
  // The banner DISPLAY fades at STREAK_TICKS but the COUNT lives for the whole
  // MULTI_WIN (the EV_KILL window check above resets it): zeroing the count
  // with the display made a kill 1.25-3 s after the second one a TRIPLE to the
  // audio's pitch ladder and a fresh single to the banner.
  if (g_streak_t > 0) g_streak_t--;
  // The END sample — this function's own spawns; see the TOP sample's comment.
  part_census();
}

// A match restart must not carry stale feedback into the fresh match — and
// the arena is regenerated, so world-anchored VFX (impact marks, tracers,
// sparks) would float in mid-air on the new map.
static void vfx_match_reset(void) {
  memset(g_feed, 0, sizeof g_feed);
  memset(g_tracers, 0, sizeof g_tracers);
  memset(g_puffs, 0, sizeof g_puffs);
  memset(g_spoons, 0, sizeof g_spoons);
  memset(g_scorch, 0, sizeof g_scorch);
  memset(g_fire, 0, sizeof g_fire);
  memset(g_shock, 0, sizeof g_shock);
  memset(g_brands, 0, sizeof g_brands);
  memset(g_nades, 0, sizeof g_nades);
  g_shake = g_blast_light = 0.0f;
  memset(g_flashes, 0, sizeof g_flashes);
  memset(g_dust, 0, sizeof g_dust);
  g_muzzle_fresh = 0;
  memset(g_parts, 0, sizeof g_parts);
  memset(g_skid_t, 0xFF, sizeof g_skid_t);   // 0xFFFF = -1 = nobody is scraping
  memset(g_marks, 0, sizeof g_marks);
  memset(g_corpses, 0, sizeof g_corpses);
  memset(g_player_rag, 0, sizeof g_player_rag);  // no leak is constructible
  memset(g_player_corpse_q, 0, sizeof g_player_corpse_q);
                               // (the handover runs at the end of the same
                               // world tick that can set it) but it is
                               // cross-tick state and this is where the rest
                               // of the pool's state is owned
  g_mark_head = 0;
  g_dmg_t = 0;
  memset(g_dmg, 0, sizeof g_dmg);
  // ALL FOUR halves of the marker, not just the visible one: a marker still
  // waiting out its per-weapon arming delay when the match restarts would arm
  // itself into the new one, and the crosshair kick would ride across with it.
  g_hitmark_t = g_hitmark_arm = g_hitmark_pend = g_hitmark_kind = 0;
  g_ch_kick = 0.0f;
  g_multi_n = 0;               // a new match does not start on a double kill
  g_hitstop = 0;
  g_score_pop = g_streak_n = g_streak_t = 0;
  g_streak_last_t = -100000;
}

