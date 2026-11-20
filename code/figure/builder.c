// code/figure/builder.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// A FIGURE BUILDER IS A SOURCE DECOMPOSITION AND MUST NOT BE ANYTHING ELSE, and
// under -O3 -ffast-math that is not free. Reassociation is legal here, so once a
// block stops being part of scene_draw_figure gcc re-associates its node
// arithmetic differently and vertices move by ~1 ULP — which sounds like nothing
// and is not: tools/proof.sh compares rendered frames by md5, and figcheck's
// `near` tier is a 1 mm THRESHOLD, so a 2 um change in a 0.415 mm gap flips a
// count. Measured on the 12-bot soak and the 20-bot frame: 3 pixels of 921600,
// max channel delta 13, and near 34 -> 37. Pinning the builders inline is what
// keeps the IL close enough to the original to land on the same bits; left to
// gcc's own judgement, splitting out even the torso alone moved both.
// This is also why the head and the leg loop are still INLINE in the caller
// below rather than builders of their own: neither could be made bit-identical
// in any form tried (inline, out-of-line, its own frame recomputed, the boot
// alone), so they stay put. That is the standing rule for this file: a diff you
// cannot explain is not a diff you may tune away.
#define FIG_INLINE static inline __attribute__((always_inline))

// The corpse solve, run in place on the INTERPOLATED pose: gravity drape,
// rigid tip about the shot's own axis, wall clamp and settle. Presentation
// only — the sim skeleton is untouched — and it emits no geometry, so it
// establishes no batch state. Only called when the body is actually slack.
FIG_INLINE void fig_death_pose(pose_t *P, const death_t *dth, v3 pos,
                               float yaw, int wpn) {
  // No whole-body ground plane here any more: the settle, the drapes and the
  // floor guard all measure each joint against the surface under ITSELF via
  // rag_support(). The old single ray under the pelvis (patched by
  // gy = max(gy, pos.y) for airborne bodies and bodies across cover) answered
  // one height for a 1.8 m body and put whole corpses on the wrong plane —
  // and once the per-joint readers replaced it, the ray was every DEAD figure
  // casting against every solid once a frame and discarding the answer, the
  // same waste the LIVING figures shed when the blob shadow died.
  // Death: the body COLLAPSES first, then the whole posed skeleton tips
  // backward around the feet. Tipping alone (which is all this used to do) reads
  // as a toppling statue — the giveaway is a corpse that still holds its rifle
  // at the chest with locked knees. So the limbs are re-solved toward a slack
  // sprawl on the way down: knees fold and splay, the arms let go and fall out
  // to the sides, the rifle drops with the trigger hand.
  // The slack runs far ahead of the tip: a body drops its weapon and folds the
  // instant it is hit, and only then falls. Blending them at the same rate is
  // what makes the first half of the fall read as a statue tipping.
  float c = f_clamp(dth->slack * 2.2f, 0.0f, 1.0f);        // legs
  float ca = f_clamp(dth->slack * 4.5f, 0.0f, 1.0f);        // arms + head:
  // staggered slackening — simultaneous reads as collapsing, staggered
  // (grip and gaze die first, the frame follows) reads as dying.
  v3 up = {{0, 1, 0}};
  v3 sp = v3_norm(v3_sub(P->chest, P->pelvis));
  v3 pr = v3_norm(v3_sub(P->hip[1], P->hip[0]));
  v3 pf = v3_norm(v3_cross(sp, pr));
  float reach = (ANIM_THIGH + ANIM_SHIN) * 0.80f;
  for (int i = 0; i < 2; i++) {
    float sd = i ? 1.0f : -1.0f;
    v3 d = v3_norm(v3_add(v3_scale(up, -1.0f),
                   v3_add(v3_scale(pr, sd * 0.30f), v3_scale(pf, 0.34f))));
    v3 ank = v3_lerp(P->ankle[i], v3_add(P->hip[i], v3_scale(d, reach)), c);
    v3 toe = v3_add(ank, v3_sub(P->toe[i], P->ankle[i]));
    v3 pole = v3_norm(v3_add(v3_scale(pf, 1.0f), v3_scale(up, 0.45f)));
    P->knee[i] = ik_solve(P->hip[i], ank, ANIM_THIGH, ANIM_SHIN, pole);
    P->ankle[i] = ank;
    P->toe[i] = toe;
  }
  for (int i = 0; i < 2; i++) {
    float sd = i ? 1.0f : -1.0f;
    v3 slack = v3_add(P->chest, v3_add(v3_scale(pr, sd * 0.34f),
               v3_add(v3_scale(up, -0.40f), v3_scale(pf, 0.06f))));
    v3 wri = v3_lerp(P->wri[i], slack, ca);
    v3 pole = v3_norm(v3_add(v3_scale(pr, sd * 0.8f), v3_scale(up, -0.6f)));
    P->elb[i] = ik_solve(P->sho[i], wri, ANIM_UARM, ANIM_FARM, pole);
    P->wri[i] = wri;
  }
  P->gun_o = v3_lerp(P->gun_o, v3_add(P->wri[1], v3_scale(P->gun_f, 0.07f)), ca);
  // The head drops off-axis in the FIRST fifth of the fall — a chin that
  // stays proudly level through the whole topple is the statue giveaway.
  P->head = v3_lerp(P->head, v3_add(P->neck, v3_add(v3_scale(pr, 0.055f),
                    v3_scale(up, 0.075f))), ca);
  // A wall slam crumples the body INTO itself — joints collapse toward the
  // pelvis. That compression is what reads as mass arriving somewhere.
  if (dth->slam > 0.01f) {
    float k = 0.26f * dth->slam;
    for (int i = 0; i < 2; i++) {
      P->knee[i] = v3_lerp(P->knee[i], P->pelvis, k * 0.5f);
      P->ankle[i] = v3_lerp(P->ankle[i], P->pelvis, k);
      P->toe[i] = v3_lerp(P->toe[i], P->pelvis, k);
      P->elb[i] = v3_lerp(P->elb[i], P->chest, k * 0.6f);
      P->wri[i] = v3_lerp(P->wri[i], P->chest, k);
    }
    P->head = v3_lerp(P->head, P->chest, k * 0.45f);
    P->neck = v3_lerp(P->neck, P->chest, k * 0.45f);
  }
  // Tilt is a PHYSICAL angle now, integrated from the shot's spin, about the
  // axis the bullet set — not a fixed 86 degrees about the victim's own facing.
  float th = dth->tilt;
  v3 axis = dth->axis;
  if (v3_dot(axis, axis) < 1e-6f) axis = (v3){{cosf(yaw), 0, sinf(yaw)}};
  // Spin about the victim's OWN vertical, applied BEFORE the topple with the
  // topple axis turned by the same angle — which is exactly the same thing as
  // spinning the already-fallen body about the WORLD vertical, because
  // R_y * R_a == R_(R_y a) * R_y. That identity is the whole reason it goes
  // here: composed the other way round a flat corpse would roll about its own
  // spine, composed this way it pivots on the floor, which is what a body with
  // leftover spin actually does when it lands. (`pr` is deliberately NOT turned
  // with it: its only reader past this point is dot(pr, axis), and both sides of
  // that dot get the same rotation, so it is invariant.)
  if (dth->yaw != 0.0f) {
    float cy = cosf(dth->yaw), sy = sinf(dth->yaw);
    #define DYAW(P, OX, OZ) do { float _x = (P).x - (OX), _z = (P).z - (OZ); \
      (P).x = (OX) + _x * cy + _z * sy;                                      \
      (P).z = (OZ) - _x * sy + _z * cy; } while (0)
    axis = (v3){{axis.x * cy + axis.z * sy, 0.0f, -axis.x * sy + axis.z * cy}};
    v3 *y = (v3 *)P;
    for (int k = 0; k < POSE_NPOS; k++) DYAW(y[k], pos.x, pos.z);
    for (int k = POSE_NPOS; k < (int)(sizeof(pose_t) / sizeof(v3)); k++)
      DYAW(y[k], 0.0f, 0.0f);
    #undef DYAW
  }
  v3 *j = (v3 *)P;
  for (int k = 0; k < POSE_NPOS; k++)
    j[k] = v3_add(pos, v3_rot_axis(v3_sub(j[k], pos), axis, th));
  for (int k = POSE_NPOS; k < (int)(sizeof(pose_t) / sizeof(v3)); k++)
    j[k] = v3_rot_axis(j[k], axis, th);
  // THE LIMBS TRAIL THE TRUNK. Everything above rotates the solved pose as one
  // rigid body, which is what a spitted mannequin does; a thrown body's free
  // ends lag behind its trunk by whatever it is turning at. `swing` is that lag
  // in radians and the displacement is its arc — cross(axis, r) is the
  // tangential direction at each joint, so this is literally "where this joint
  // WAS `swing` radians ago", and it goes to zero on its own the moment the
  // rotation stops. The two sides get different gains because a body whose arms
  // trail symmetrically is still a mannequin, just a floppier one. The mid
  // joints are re-solved by the IK below, so an elbow follows its wrist.
  // The IK poles the mid joints are solved with, carried forward so the clamp
  // pass below re-solves on whatever the LAST stage that placed an end effector
  // wanted rather than on a second, hard-coded set.
  v3 kpole[2] = {pf, pf};
  v3 epole[2] = {v3_add((v3){{0, -0.5f, 0}}, v3_scale(pr, -0.8f)),
                 v3_add((v3){{0, -0.5f, 0}}, v3_scale(pr, 0.8f))};
  if (fabsf(dth->swing) > 0.001f) {
    #define LAGJ(P, G) do { \
      v3 _r = v3_sub((P), pos); \
      (P) = v3_add((P), v3_scale(v3_cross(axis, _r), -dth->swing * DEATH_LAG_K * (G))); \
    } while (0)
    LAGJ(P->wri[0], 1.00f); LAGJ(P->wri[1], 0.72f);
    LAGJ(P->ankle[0], 0.80f); LAGJ(P->toe[0], 0.80f);
    LAGJ(P->ankle[1], 0.55f); LAGJ(P->toe[1], 0.55f);
    LAGJ(P->head, 0.45f);
    #undef LAGJ
    for (int i = 0; i < 2; i++) {
      P->elb[i] = ik_solve(P->sho[i], P->wri[i], ANIM_UARM, ANIM_FARM, epole[i]);
      P->knee[i] = ik_solve(P->hip[i], P->ankle[i], ANIM_THIGH, ANIM_SHIN, kpole[i]);
    }
  }
  // SETTLE FIRST, THEN DRAPE — the order is the whole difference between a
  // corpse and a gymnast. Rotating rigidly about the entity origin swings every
  // horizontally offset joint UPWARD (and, past 90 degrees, DOWNWARD through
  // the floor), so the body has to be dropped onto the surface before anything
  // can be laid on that surface. The drape below targets an absolute plane at
  // gy; with the settle running after it, every centimetre the settle moved the
  // body was a centimetre the already-placed limbs were moved OFF the ground.
  // Filmed: a body that over-rotated 52 degrees past flat had its feet pinned
  // at gy+0.085 and then the whole skeleton lifted 1.33 m — a shoulder stand
  // with both legs straight up, held for a second and a half. Harmless while
  // the tilt could not exceed 1.52 rad, which is exactly why it survived until
  // the tumble came off its clamp.
  float sfrac = f_clamp(dth->slack * 1.6f, 0.0f, 1.0f);
  {
    // Against each joint's OWN surface, not one plane under the pelvis: a 1.8 m
    // body spans more ground than one ray can describe, and dropping all of it
    // onto the height that one ray found is how a corpse ends up lying flat in
    // the air past a platform lip.
    float lo0 = 1e9f;
    for (int k = 0; k < POSE_NPOS; k++)
      lo0 = fminf(lo0, j[k].y - POSE_HALF[k] - rag_support(j[k]));
    float s0 = (lo0 - 0.004f) * sfrac;
    for (int k = 0; k < POSE_NPOS; k++) j[k].y -= s0;
  }
  // Gravity drape. The slack solve above folds the limbs while the body is
  // still standing, so its "down" is down in the STANDING frame; the rigid tip
  // then swings that downward fold into a body-forward one and the corpse ends
  // up supine holding its heels and hands a hand's width off the floor, knees
  // in the air. Once the body is past halfway over, ease the free ends of the
  // limbs down onto the surface and re-solve the mid joint: the knees roll
  // apart, the arms drop out to the sides, and the body actually rests on the
  // ground it is lying on. Targets stay inside limb reach of the anchor, or the
  // IK would straighten the leg toward the floor and take the hip with it.
  // ...and only once it has stopped TURNING. A body cannot settle onto a
  // surface it is still rotating relative to, and draping mid-tumble is what
  // put the legs out straight at origin height through the whole flight: the
  // deck-chair silhouette. `swing` is already the measure of "still turning".
  float dr = f_clamp((th - 0.60f) / 0.70f, 0.0f, 1.0f) * c *
             (1.0f - 0.85f * f_clamp(fabsf(dth->swing) / 0.35f, 0.0f, 1.0f));
  if (dr > 0.001f) {
    // Which leg ends up straight follows the side the body rolls toward, so
    // corpses do not all settle into the same silhouette.
    int lead = v3_dot(pr, axis) > 0.0f;
    // THE TRUNK HAS TO LIE DOWN TOO, and this is the half the drape never did.
    // It laid the four limbs on the ground and left pelvis, chest, neck, head,
    // hips and shoulders wherever the rigid tip about the FEET had put them —
    // and a tip about the feet is not a fall, it is a pivot, so the trunk ends
    // up over the pivot rather than beside it. Measured with `ragx`'s joint dump
    // on a settled corpse: toes 0.06, ankles 0.12, chest 0.64, shoulder 0.84.
    // That is a man kneeling with his boots on the floor, which is exactly what
    // it rendered as (span 1.33 m against a 1.79 m man, hgt 0.93 m).
    //
    // Each trunk joint eases to its OWN mesh radius above the surface UNDER
    // ITSELF — not onto one plane — so a body that comes to rest across a crate
    // drapes over it instead of being flattened onto the floor beside it. That
    // is also the answer to the single-`gy` failure: `gy` is one ray under the
    // pelvis, and a 1.8 m body spans more ground than one ray can describe.
    static const int TRUNK[6] = {0, 1, 8, 9, 15, 17};   // hips, shoulders,
    P->pelvis.y += (rag_support(P->pelvis) + POSE_HALF[14] - P->pelvis.y) * dr;
    P->neck.y   += (rag_support(P->neck)   + POSE_HALF[16] - P->neck.y)   * dr;
    for (int q = 0; q < 6; q++) {                       // chest and head
      v3 *t = &j[TRUNK[q]];
      t->y += (rag_support(*t) + POSE_HALF[TRUNK[q]] - t->y) * dr;
    }
    // ...and it runs BEFORE the limbs, not after: the arm drape clamps its wrist
    // target to shoulder reach, so with the shoulder still 0.84 m in the air the
    // hands were pulled back up to 0.47 instead of onto the ground.
    for (int i = 0; i < 2; i++) {
      float sd = i ? 1.0f : -1.0f;
      v3 out = v3_norm(v3_cross(axis, (v3){{0, 1, 0}}));
      v3 h = P->hip[i];
      // Each foot onto the surface under ITSELF. The last single-`gy` use in
      // the solve: a body settling across a platform lip had the leg that hung
      // over the void placed at the PLATFORM's height, i.e. a boot resting on
      // nothing 0.55 m up, which `ragx`'s `hang` column reads directly.
      float fy = rag_support(P->ankle[i]) + 0.085f;
      // Reach budget, the same one the gait uses: with the hip this low, the
      // heel can only be so far away horizontally. Spending it asymmetrically
      // (one leg nearly straight, one folded) is what keeps a corpse from
      // looking like it is doing sit-ups.
      // 0.62, not 0.78, on the trail leg. THE SILHOUETTE NEEDS A HOLE IN IT:
      // with both legs near-straight and parallel there is no negative space
      // anywhere in the body, and at 15 m a corpse with no holes in it reads as
      // a duffel bag rather than as a man. One deeply flexed knee is the
      // cheapest hole there is — it costs no geometry and it is also what a
      // dropped body actually does.
      float want = (ANIM_THIGH + ANIM_SHIN) * (i == lead ? 0.94f : 0.62f);
      float dy = h.y - fy;
      float hmax = sqrtf(fmaxf(0.010f, want * want - dy * dy));
      v3 hd = v3_sub(P->ankle[i], h);
      hd.y = 0.0f;
      hd = v3_add(hd, v3_scale(out, sd * 0.22f));  // heels roll apart
      if (v3_dot(hd, hd) < 1e-6f) hd = out;
      hd = v3_norm(hd);
      v3 lt = {{h.x + hd.x * hmax, fy, h.z + hd.z * hmax}};
      v3 nk = v3_lerp(P->ankle[i], lt, dr);
      v3 tv = v3_sub(P->toe[i], P->ankle[i]);
      kpole[i] = v3_add((v3){{0, 0.30f, 0}}, v3_scale(out, sd * 1.25f));
      P->knee[i] = ik_solve(h, nk, ANIM_THIGH, ANIM_SHIN, kpole[i]);
      P->ankle[i] = nk;
      P->toe[i] = v3_add(nk, tv);
      v3 s = P->sho[i];
      v3 wt = P->wri[i];
      wt.y = rag_support(P->wri[i]) + 0.065f;   // ...and each hand, likewise
      wt = v3_add(wt, v3_scale(out, sd * 0.26f));  // arms fall out to the sides
      // The lead-side arm ends past the head, the other across the belly —
      // symmetric arms are the "sassy corpse" read.
      if (i == lead) wt = v3_add(wt, v3_scale(axis, 0.44f));  // thrown out wide
      else wt = v3_lerp(wt, v3_add(P->pelvis, v3_scale(up, 0.04f)), 0.45f);
      v3 dv = v3_sub(wt, s);
      float dl = v3_len(dv);
      float lim = (ANIM_UARM + ANIM_FARM) * 0.94f;
      if (dl > lim) wt = v3_add(s, v3_scale(dv, lim / dl));
      v3 nw = v3_lerp(P->wri[i], wt, dr);
      epole[i] = v3_add((v3){{0, 0.4f, 0}}, v3_scale(out, sd * 1.0f));
      P->elb[i] = ik_solve(s, nw, ANIM_UARM, ANIM_FARM, epole[i]);
      P->wri[i] = nw;
    }
  }
  // Settle: rotating rigidly about the entity origin swings every
  // horizontally offset joint UPWARD, so a folded corpse ends up sitting in
  // mid-air with its boots a hand's width clear of its own shadow. Drop the
  // whole posed skeleton until its lowest joint rests on the surface, phased
  // in with the fall so a kill in mid-air sinks instead of snapping.
  // No part of a corpse may end up inside geometry. The entity collider is
  // clamped but the SKELETON is not, so a body toppling head-first at a wall
  // used to lie straight through it — from outside you saw a boot and a rifle
  // and nothing else. Raying every joint back to the body's own origin stops it
  // short of the first solid, so a corpse thrown into cover piles up against it
  // instead: the "slams into a wall and crumples" read, for free.
  // A JOINT IS NOT A SURFACE. The margin used to be 0.050 + 0.013*(k%6) — a
  // stagger whose only job was to keep several clamped joints off one plane —
  // which is a fair margin for a wrist and half of what a chest needs: the
  // carrier is 170 mm from the spine, so a chest joint stopped 63 mm short of a
  // wall still had a third of the torso inside it (measured: pen = 0.161 m of
  // geometry inside a solid). POSE_RAD is the mesh's own half-extent at each
  // joint, and it is the same number the floor guard below needs — a boot hangs
  // 85 mm under its ankle node and a helmet stands 115 mm off the head node, so
  // a guard that lifts JOINTS to the surface leaves the sole and the skull
  // under it (sink = -0.105).
  // The stagger survives inside it as the 4 mm term: coplanar same-facing faces
  // are what z-fights, and several joints clamped to one distance from one wall
  // is exactly that (a soak caught 5 such triangles).
  for (int k = 0; k < POSE_NPOS; k++)
    j[k] = foot_clamp_m(pos, j[k], POSE_RAD[k] + 0.004f * (float)(k % 5));
  // THE RAY CLAMP ONLY PROTECTS ALONG ONE LINE. It casts from the body's own
  // origin, so it cannot see a wall the body is lying ALONGSIDE, and it cannot
  // see a part that stands off its joint in a third direction — a helmet sits
  // 120 mm past the head node along the body's own spine, which is HORIZONTAL
  // once the body is on its side, so a head correctly stopped 119 mm short of a
  // face still had 99 mm of shell inside it (measured with ragx: 1421 vertices
  // in a solid on one tick of a body sailing past the end of a cover wall).
  // So: push every joint out of every solid it overlaps once its own mesh radius
  // is accounted for. There is no floor here — the ground is a plane and the
  // guard below owns it.
  // The mid joints are re-solved FIRST, from the ray-clamped ends, and the
  // push-out then runs over the whole skeleton: done the other way round the IK
  // put each elbow and knee back wherever the arm wanted it — which is inside
  // the wall the hand had just been pulled out of, and measured, that made
  // penetration WORSE than no push-out at all (0.077 -> 0.206 m).
  // ...and on the SAME POLES the stage that placed the end effectors used. Both
  // re-solves used to be written out with the flight poles, so the clamp pass
  // silently re-poled a DRAPED corpse: the knees came back up and every settled
  // body lay on its back doing sit-ups, which is the read the asymmetric leg
  // spend above exists to prevent.
  for (int i = 0; i < 2; i++) {
    P->elb[i] = ik_solve(P->sho[i], P->wri[i], ANIM_UARM, ANIM_FARM, epole[i]);
    P->knee[i] = ik_solve(P->hip[i], P->ankle[i], ANIM_THIGH, ANIM_SHIN, kpole[i]);
  }
  // Two passes, because pushing a joint out of one solid can push it into the
  // one abutting it — cover in this arena is built from touching boxes.
  for (int pass = 0; pass < 2; pass++)
  for (int k = 0; k < POSE_NPOS; k++) {
    float rad = POSE_RAD[k];
    BP_BOX(i, j[k].x - rad - 1.0f, j[k].z - rad - 1.0f, j[k].x + rad + 1.0f, j[k].z + rad + 1.0f) {
      const solid_t *sd = &g_solids[i];
      v3 q = j[k];
      if (q.x <= sd->min.x - rad || q.x >= sd->max.x + rad ||
          q.y <= sd->min.y - rad || q.y >= sd->max.y + rad ||
          q.z <= sd->min.z - rad || q.z >= sd->max.z + rad) continue;
      // Above the top face is RESTING, not trapped. Resolved on the shortest
      // axis a joint near the lip of a crate takes the sideways exit, which
      // walks the corpse off the thing it just landed on.
      // ...and only when the joint is really OVER the solid. Tested on the
      // EXPANDED box it also fired for a joint standing beside a crate and a
      // little above it, lifting it onto the crate's top plane instead of
      // letting it take the much shorter sideways exit — and pinning a limb
      // hanging over a lip at lip height, which is the float this pass is about.
      if (q.y >= sd->max.y && q.x > sd->min.x && q.x < sd->max.x &&
          q.z > sd->min.z && q.z < sd->max.z) {
        j[k].y = sd->max.y + rad;
        continue;
      }
      float dxl = q.x - (sd->min.x - rad), dxh = (sd->max.x + rad) - q.x;
      float dzl = q.z - (sd->min.z - rad), dzh = (sd->max.z + rad) - q.z;
      float dyh = (sd->max.y + rad) - q.y;   // never DOWN: that buries the body
      float m = fminf(fminf(dxl, dxh), fminf(fminf(dzl, dzh), dyh));
      if (m == dyh)      j[k].y = sd->max.y + rad;
      else if (m == dxl) j[k].x = sd->min.x - rad;
      else if (m == dxh) j[k].x = sd->max.x + rad;
      else if (m == dzl) j[k].z = sd->min.z - rad;
      else               j[k].z = sd->max.z + rad;
    }
  }
  // ...and a floor guard afterwards, because the drape and the wall clamp both
  // move joints and either can leave one under the surface. It only ever LIFTS:
  // lowering here is what the settle above is for, and doing it twice would
  // re-introduce the bug it was moved to fix.
  // Per-joint surface here too, and the body still moves RIGIDLY: the lift is
  // the largest any one joint needs to clear its own ground. A joint out over a
  // lower area asks for a negative lift and so never drives it, which is exactly
  // the "one leg hanging off the crate" case.
  float lo = 1e9f;
  for (int k = 0; k < POSE_NPOS; k++)
    lo = fminf(lo, j[k].y - POSE_HALF[k] - rag_support(j[k]));
  float set = (lo - 0.004f) * sfrac;
  if (set < 0.0f)
    for (int k = 0; k < POSE_NPOS; k++) j[k].y -= set;
  // The rifle is DROPPED, not welded to the chest: rotated rigidly with the
  // body it keeps its body-relative orientation and ends up standing straight
  // up out of a supine ribcage, held by nothing. Lay it flat on the ground
  // beside the trigger hand instead.
  v3 gfl = P->gun_f;
  gfl.y = 0.0f;
  if (v3_dot(gfl, gfl) > 1e-6f)
    P->gun_f = v3_norm(v3_lerp(P->gun_f, v3_norm(gfl), c));
  P->gun_u = v3_lerp(P->gun_u, (v3){{0, 1, 0}}, c);
  P->gun_u = v3_sub(P->gun_u, v3_scale(P->gun_f, v3_dot(P->gun_u, P->gun_f)));
  P->gun_u = v3_dot(P->gun_u, P->gun_u) > 1e-6f ? v3_norm(P->gun_u) : (v3){{0, 1, 0}};
  // A DROPPED RIFLE RESTS ON THE GROUND — on its magazine, which is what a
  // rifle set down actually stands on. `gy + 0.085` put the RECEIVER 85 mm up
  // and everything under it below the floor. g_gun_low is measured off the
  // model, so this cannot drift from the magazine it is clearing.
  // Zero means the model has not been measured yet — one draw, and only ever in
  // a session whose FIRST drawn figure is already dead (the harness can do that;
  // a played session draws living bots from frame one). Leaving the rifle where
  // the pose had it for that one frame is invisible; using zero would bury it.
  if (g_gun_low[wpn] > 0.0f) {
    // Against the surface under the RIFLE, the same per-joint reference the
    // body's own guard uses. Against `gy` the two disagreed by exactly the
    // body's float, so correcting that float drove the rifle underground.
    // BESIDE the hand, not under it. Laid on the wrist's own column the trigger
    // guard came out 21 mm inside the closed glove (`vmtrig`'s `guard`, which
    // must stay >= 0 — a wire in open air is the one thing nothing can hide
    // crossing a finger). Pushed out along the chest->wrist direction it lies
    // just clear of the fingers, which is also what a dropped rifle does.
    v3 away = v3_sub(P->wri[1], P->chest);
    away.y = 0.0f;
    float al = sqrtf(away.x * away.x + away.z * away.z);
    v3 lay = {{P->wri[1].x, P->wri[1].y, P->wri[1].z}};
    if (al > 1e-4f) {
      lay.x += away.x / al * 0.16f;
      lay.z += away.z / al * 0.16f;
    }
    lay.y = rag_support(lay) + g_gun_low[wpn] * GUN_GIRTH_3P + 0.006f;
    P->gun_o = v3_lerp(P->gun_o, lay, c);
  }
}

// Every band colour the figure builders below draw with, solved once per
// figure. Real kit is not one material at six brightnesses, so these name
// real materials that differ in HUE and ROUGHNESS as much as in value; the
// hit flash washes the whole set toward one colour before the gear tones are
// derived, so a flashed figure stays internally consistent.
typedef struct {
  v3 olive, boot, helm, web, rub, gm2;         // base materials
  v3 gear, belt, pad, pants, dust1;            // carrier, webbing, rubber, trouser
  v3 vest, sleeve, glove, bala;                // derived / balaclava
  v3 gnt;                                      // the glove's cuff / gauntlet
  v3 wst;                                      // the wrap's second (check) colour
  v3 arm;                                      // the operator's armband (friendly)
  int ins;                                     // insurgent build
} kit_t;

// THE OPPOSING FORCE IS NOT ONE MAN. Every bot wore the identical kit, so a
// crowd of eight read as clones. g_fig_var picks one of four carrier colours
// per entity, and they differ in HUE ONLY: each sits at the sand carrier's own
// luminance (0.571), so the light-carrier-on-dark-fatigues value step the 25 m
// silhouette depends on is untouched, and the red helmet — the hostile cue —
// never varies. 0 is the shipped sand kit (bot 0 keeps it). Set by the figure
// draw sites from the entity id, so a corpse keeps the colour it fell in.
static int g_fig_var;
// TEAM KIT: in a team game the side IS the uniform. -1 = no team (the palette
// decides as always); 0 ALPHA wears RED, 1 BRAVO wears BLUE — helmet, carrier
// and fatigues, the three biggest masses, so a side reads at any range and
// from behind. Set by the draw sites from the entity's side.
static int g_fig_team = -1;
// TWO FORCES, TWO SILHOUETTES. The player's side are OPERATORS (helmet,
// goggles, NVG, plate carrier, assault pack); the other side are INSURGENTS:
// a head wrap with a tail and an eye slit, a light chest rig over a civilian
// shirt, no helmet, no goggles, no pack. Every mode and map: an enemy is an
// insurgent (fig_ins_of). The difference is in the SHAPE, so it reads at range
// and in silhouette, and it never replaces the team colours a team game needs.
// Set by the draw sites; g_fig_ins_force (harness `figins`) pins it so the
// figure proofs can check the insurgent build too.
static int g_fig_ins;
static int g_fig_ins_force = -1;
// THE ARMORY'S GEAR ON THE BODY. The GLOVES + GAUNTLET colourway used to dye
// only the first-person meshes, so the shoulder cam (and the mirror of the same
// man in every other view of himself) showed the authored coyote glove whatever
// the player had picked. A GEAR_THEMES index for the figure being built — the
// LOCAL player's own body and corpse only, since a theme is a client setting
// and nobody else's is known here — or -1: the figure's authored kit. Set by
// the draw sites, like g_fig_team.
static int g_fig_gear = -1;
// ...and the PIECES that figure wears, the same way: a *_GEAR index, or -1 to
// follow the side it is on (the insurgent's wrap / rig / no pack, else the
// operator's helmet / plate carrier / assault pack).
static int g_fig_head = -1, g_fig_vest = -1, g_fig_pack = -1, g_fig_face = -1;
// The entity the figure being built belongs to (-1: none — a corpse, a proof).
// Read only by the sling (fig_sling), which needs the mode's weapon rules.
static int g_fig_ent = -1;
static int g_fig_free;   // fig_arms: bit i = hand i closes on nothing (weapon swap)
// fig_arms: bit i = hand i closes round the cylinder g_fig_grab_c/a/r[i] instead
// of the weapon's own grip (the weapon swap's hand on a stock)
static int g_fig_grab;
static v3 g_fig_grab_c[2], g_fig_grab_a[2];
static float g_fig_grab_r[2];
// THE DUAL BERETTAS in third person: 1 while the figure holds the pair, and its
// left hand then closes on the SECOND pistol, DUAL3P to the left of the first.
static int g_fig_dual;
#define DUAL3P 0.200f
// The local player's armory picks onto the figure about to be built (own = 1),
// or back to "follow the side" (own = 0).
static void fig_own_pieces(int own) {
  g_fig_head = own ? armory_idx(g_cfg.head_gear, HEAD_GEAR_COUNT) : -1;
  g_fig_vest = own ? armory_idx(g_cfg.vest_gear, VEST_GEAR_COUNT) : -1;
  g_fig_pack = own ? armory_idx(g_cfg.pack_gear, PACK_GEAR_COUNT) : -1;
  g_fig_face = own ? armory_idx(g_cfg.face_gear, FACE_GEAR_COUNT) : -1;
}
// THE INSURGENT IS FOUR MEN, AND THEY DIFFER IN SHAPE. g_fig_var picks the
// archetype (INS_RIFLEMAN .. INS_LEADER), from the entity id like the colour
// variants always were, so a corpse keeps the kit it fell in and the bot brain
// (bot_kind) reads the same number: the gunner who suppresses is the one with
// the belt across his chest. Hue variety alone read as clones at 25 m — a crowd
// of identical outlines in four shirts — so each type changes the OUTLINE:
//   RIFLEMAN  chest rig, wrap, AK
//   GUNNER    no rig, an ammo belt across the chest
//   SCOUT     no rig, light trainers (the one light value at the feet)
//   LEADER    chest rig with a radio and its whip antenna
enum { INS_RIFLEMAN, INS_GUNNER, INS_SCOUT, INS_LEADER };
#define FIG_INS (g_fig_ins_force >= 0 ? g_fig_ins_force : g_fig_ins)
// g_fig_var_force (harness `figvar`) pins the archetype, so figcheck can gate
// every insurgent build and not only the one bot 0 happens to be.
static int g_fig_var_force = -1;
static int g_fig_op_ent = -1;   // harness `figop`: this entity drawn as an operator
static int g_fig_sil;           // harness `figsil`: figures in flat black
static int g_fig_nade_kind;     // the grenade the figure holds: NADE_FRAG/SMOKE/INC
#define FIG_INS_KIND ((g_fig_var_force >= 0 ? g_fig_var_force : g_fig_var) & 3)
// An operator bot's headgear, from the entity id like its mask: two in seven
// carry the quad NVG flipped UP. Never DOWN on a bot — the tubes stand 50 mm
// further forward of the head box than the helmet does (`fighead`), so only a
// player who picks it in the armory wears it.
// harness `figgear H F`: pin every figure's headgear / face (-1 follows)
static int g_fig_head_force = -1, g_fig_face_force = -1;
#define FIG_HEAD (g_fig_head_force >= 0 ? g_fig_head_force : g_fig_head >= 0 ? g_fig_head : FIG_INS ? HEAD_WRAP                   \
                  : g_fig_ent >= 0 && (g_fig_ent * 5 + 3) % 7 < 2 ? HEAD_NVG_UP : HEAD_HELMET)
#define FIG_VEST (g_fig_vest >= 0 ? g_fig_vest : !FIG_INS ? VEST_CARRIER       \
                  : (FIG_INS_KIND == INS_GUNNER || FIG_INS_KIND == INS_SCOUT)   \
                  ? VEST_NONE : VEST_RIG)
#define FIG_PACK (g_fig_pack >= 0 ? g_fig_pack : FIG_INS ? PACK_NONE : PACK_ASSAULT)
// The face: the player's own pick, else picked from the entity id (like the
// insurgents' archetypes and the carrier colours: a corpse keeps it). v5: every
// one of the nine faces, one each per nine ids — consecutive ids (a squad) get
// different masks, so five squadmates are not five clones. The armory pick
// reaches the local body only; id 0 (the bot every figure proof films) keeps
// the shipped helmet and balaclava, so the proof baselines measure one kit; a protocol change would be needed to show
// another player's real one (see art_review/r8/REPORT.md).
static const int FIG_FACE_BY_ID[9] = {FACE_PLAIN, FACE_GOGGLES, FACE_GAS_TWIN, FACE_SKULL_HALF,
                                      FACE_SHEMAGH, FACE_RESP, FACE_PLATE, FACE_GAS_PANO, FACE_SKULL};
#define FIG_FACE (g_fig_face_force >= 0 ? g_fig_face_force : g_fig_face >= 0 ? g_fig_face : FIG_INS || g_fig_ent < 0 ? FACE_PLAIN \
                  : FIG_FACE_BY_ID[(g_fig_ent * 4) % 9])
// THE TUNIC IS DARK BECAUSE EVERY FLOOR IS LIGHT. All three world themes put
// the ground at display 0.55-0.63 and the desert maps' sand at the top of that,
// and the old shirts (khaki 0.44, brown 0.40, faded olive 0.37) sat one value
// step under it in the SAME warm hue family — an insurgent on Bazaar sand was
// camouflage, not a target. These sit at 0.18-0.27: a clear dark mass on any
// floor, still hue-varied so a crowd is four men, and still above the boots so
// the value ramp keeps its floor at the feet. The red wrap is the one peak.
static const float FIG_INS_SHIRT[4][3] = {
  {0.270f, 0.235f, 0.200f},   // rifleman: dark earth brown
  {0.200f, 0.210f, 0.260f},   // gunner: indigo
  {0.255f, 0.265f, 0.190f},   // scout: dark olive
  {0.180f, 0.178f, 0.185f},   // leader: black
};
// EVERY WRAP IS RED. One wrap in four used to be red and the others black,
// sand and olive — so three insurgents in four carried no hostile cue at all,
// and the sand one vanished into the sand maps. Red is the hostile colour
// (THREAT in the HUD, never worn by an operator), so the variety moves into the
// SECOND colour of the winding: a shemagh check against off-white or black.
static const float FIG_INS_WRAP[4][3] = {
  {0.720f, 0.170f, 0.130f},   // crimson
  {0.640f, 0.140f, 0.120f},   // brick red
  {0.780f, 0.210f, 0.140f},   // bright red
  {0.580f, 0.120f, 0.110f},   // maroon
};
static const float FIG_INS_WRAP2[4][3] = {
  {0.820f, 0.790f, 0.720f},   // off-white check
  {0.150f, 0.140f, 0.140f},   // black check
  {0.820f, 0.790f, 0.720f},
  {0.150f, 0.140f, 0.140f},
};
static const float FIG_CARRIER_VAR[4][3] = {
  {0.660f, 0.560f, 0.415f},   // sand (shipped)
  {0.520f, 0.600f, 0.440f},   // ranger green
  {0.640f, 0.560f, 0.440f},   // khaki (v5: was 0.72/0.55/0.36, which reads ORANGE in a
                              // warm sun; same luminance 0.57, saturation 0.50 -> 0.31)
  {0.570f, 0.575f, 0.560f},   // urban grey
};
static kit_t fig_kit(int palette, float flash01, v3 flash_col) {
  // Kit palette. What was here was two tones — one olive, one dark — scaled by a
  // constant per part, so helmet, carrier, pads, boots and gloves all measured
  // the same near-black and the whole figure read as one mass with a value ramp
  // painted on it. Real kit is not one material at six brightnesses: it is
  // ranger-green composite, coyote nylon, brown leather, black rubber and cotton
  // ripstop, and they differ in HUE and in ROUGHNESS as much as in value.
  // These are display-space albedos (the shader squares them into linear).
  // Kit albedo, measured against silhouette contrast rather than picked by eye.
  // These were lifted 30% on a review's advice to stop the figure crushing to
  // black; measuring afterwards showed what that cost — the gap between figure and
  // background at 25 m fell from 54 to 35, and 25 m is where most duels happen.
  // The contrast that review wanted belongs in the BACKGROUND (the aerial-
  // perspective term lifts the wall behind the man), not in the man. Held here at
  // the level where 25 m measures >= 50 again.
  v3 olive = {{0.362f, 0.384f, 0.288f}};   // ripstop uniform, olive drab
  v3 dark  = {{0.288f, 0.299f, 0.246f}};   // trouser, a shade greyer
  // Desaturated. With a 2.9-radiance warm sun a 0.70/0.61/0.54 skin renders the
  // sunlit forehead saturated orange while the chin — in the goggles' shadow, lit
  // only by the blue sky — renders pale pink, and the two read as two different
  // materials with a seam between them. Real skin is far less saturated than the
  // paint chip suggests.
  // (No skin material any more: the balaclava covered the face — its last
  // consumer — and a bare-skin albedo here would be a dead tone waiting for a
  // reader that no longer exists.)
  // Boots are the DARKEST thing on the figure — the value ramp is a contract
  // now (boots -> trousers -> carrier -> helmet, largest step at the chest
  // line), so the ground end of the ramp has to actually be its floor.
  v3 boot  = {{0.168f, 0.152f, 0.140f}};   // dark brown leather
  // 0.65x the sleeve. Helmet, collar, shoulder and sleeve all landed inside one
  // narrow olive band once the bare neck was covered, and at 12 m — which is
  // where duels happen, and where you aim at heads — the figure became one green
  // column from crown to armpit. A helmet is a different material from a
  // uniform and should carry a different VALUE, not just a different hue.
  // Held just ABOVE the sleeve, not below it. Dropping the helmet to 0.65x the
  // sleeve was the wrong direction and it was measured: the sleeve is the DARK
  // neighbour, so moving the helmet toward it collapsed the head/shoulder value
  // step at 12 m instead of restoring it. A helmet cover is a different, drier,
  // slightly lighter cloth than a uniform — which is also what it looks like.
  v3 helm  = {{0.428f, 0.452f, 0.348f}};   // ranger green composite shell (value peak)
  // The carrier is the LIGHT block. It used to sit a value step below the
  // uniform, which put the biggest mass on the body at the bottom of the value
  // ramp; the TF2 rule the redesign follows wants the largest value step at
  // the chest line, which means the carrier goes ABOVE the uniform, not below.
  // ...and it is a COOL light block now, not an orange one. The value contract
  // above is about LUMINANCE, and this holds it: 0.429 against the old 0.423,
  // so the carrier still sits 1.13x over the uniform and the 25 m silhouette
  // number is untouched. What changed is saturation, 0.39 -> 0.22. At the old
  // chroma the single biggest mass on the figure was a warm sand slab on a
  // green man — it read as surplus webbing rather than as a plate carrier, and
  // it pulled the eye off the head and the weapon, which are the two things a
  // shooter's silhouette is supposed to be about.
  v3 nyl   = {{0.452f, 0.430f, 0.352f}};   // wolf-grey nylon: carrier, pouches
  v3 web   = {{0.238f, 0.214f, 0.176f}};   // webbing / belt
  // Pads live in the trouser family now (max ~1.5 value steps off the sleeve):
  // near-black rubber made every joint the darkest accent on the model, and it
  // landed exactly on the roundest surfaces — action-figure articulation dots.
  v3 rub   = {{0.212f, 0.214f, 0.206f}};   // worn rubber knee & elbow pads
  v3 gm2   = {{0.078f, 0.082f, 0.098f}};   // lens glass, dark pupil
  // Balaclava: the face is COVERED. The bone-white face wedge was the
  // brightest value on the whole model — a skull mask up close, a headlamp
  // blob at range — and the value-ramp contract says the face is never the
  // peak. A dark neutral under goggles is the operator read, and it is hue-
  // neutral so both palettes keep their identity in the uniform, not the face.
  // +50%. At 0.212 this broke the ramp contract it was written under: measured
  // by recolour-and-diff, the face rendered at linear Y 0.0199 against boot
  // 0.0339 — 0.77 stops BELOW the surface this file calls the floor — and
  // helmet/face came out 12.09x (+3.60 stops), the largest value step anywhere
  // on the model, landing on the head. The goggle lens then had nothing to read
  // against: 0.0102 vs 0.0199 is display RGB 24 vs 35, so the "wide dark visor
  // with one wet gleam" that is supposed to carry the facing cue was invisible,
  // and so was the nose. At 0.318 the face sits between the boots and the
  // trousers, the helmet keeps the peak, and the visor becomes the dark accent
  // it was designed as. Still far under the old bone-white skin wedge (0.648)
  // that made the face the brightest thing on the figure.
  v3 bala  = {{0.318f, 0.309f, 0.282f}};
  // A BOT OPERATOR IS A FRIEND. Palette 0 used to be the opposing force (black
  // fatigues, crimson shell, red-lit goggles) from the days when every bot was
  // an enemy; since 0.7.3 every enemy is drawn as an insurgent and a palette-0
  // operator is a squadmate in the rescue or a body on the home backdrop — and
  // he was still wearing the hostile red helmet. Ranger kit now: green-grey
  // fatigues, the carrier variants, a ranger-green shell and cyan goggles (the
  // HUD's friendly SIGNAL), so red on a head means one thing only.
  olive = (v3){{0.330f, 0.350f, 0.330f}};
  dark  = (v3){{0.300f, 0.315f, 0.300f}};
  helm  = (v3){{0.430f, 0.455f, 0.360f}};
  { const float *cv = FIG_CARRIER_VAR[g_fig_var & 3];
    nyl = (v3){{cv[0], cv[1], cv[2]}}; }
  web   = (v3){{0.200f, 0.200f, 0.185f}};
  rub   = (v3){{0.230f, 0.235f, 0.225f}};
  // SMOKED GLASS (v5): near-black, lit only by its own gleam. The cyan lamp
  // that was here was the brightest thing on the figure, on the face.
  gm2   = (v3){{0.050f, 0.057f, 0.066f}};
  bala  = (v3){{0.250f, 0.260f, 0.250f}};
  if (palette == 1) {  // the player: the same kit in slate blue — the SAME
    // value ramp as the olive design (boots darkest, carrier light, helmet
    // peak), only the hue family changes.
    olive = (v3){{0.335f, 0.375f, 0.440f}};
    dark  = (v3){{0.290f, 0.320f, 0.380f}};
    // A light grey composite shell (v5): the value peak over the wolf-grey
    // carrier, without the blaze orange it used to wear (BODY_KITS SLATE).
    helm  = (v3){{0.660f, 0.660f, 0.620f}};
    web   = (v3){{0.150f, 0.160f, 0.190f}};
    boot  = (v3){{0.130f, 0.130f, 0.150f}};
    gm2   = (v3){{0.050f, 0.057f, 0.066f}};   // smoked glass
    // The three tones the palette used to leave behind, and leaving them behind
    // was not neutral — it was a different figure. On the bot the carrier is
    // separated from the uniform by HUE (coyote against olive: chromaticity
    // distance 0.070) and only 1.13x in value; palette 1 gave it the SAME blue
    // family, so the separation fell to 0.021 and the biggest mass on the body
    // stopped being an object at all. Meanwhile the neutral `rub` and `bala`
    // were carried over unchanged, which against a cool kit reads WARM: pad
    // vs trouser went 0.038 on the bot to 0.097 on the player — a hue break
    // landing on the roundest surfaces on the figure, which is the action-figure
    // ball joint this file already killed once at the shoulder.
    // Wolf grey carrier (chroma distance 0.075, value 1.155x — the bot's 0.070
    // and 1.135x), pads and balaclava pulled into the blue family.
    nyl   = (v3){{0.700f, 0.610f, 0.470f}};
    rub   = (v3){{0.160f, 0.168f, 0.190f}};
    bala  = (v3){{0.260f, 0.275f, 0.305f}};
  }
  v3 wst = v3_scale(helm, 0.80f);
  int ins = FIG_INS;
  if (ins) {   // the insurgent: dark tunic, drab rig, leather, a red head wrap
    int kind = FIG_INS_KIND;
    const float *sh = FIG_INS_SHIRT[kind], *wr = FIG_INS_WRAP[kind],
                *w2 = FIG_INS_WRAP2[kind];
    olive = (v3){{sh[0], sh[1], sh[2]}};
    // Loose trousers a shade off the tunic, never the same cloth: the hem of
    // the tunic is the silhouette's widest line and needs a value edge under it.
    dark  = v3_scale(olive, 0.86f);
    helm  = (v3){{wr[0], wr[1], wr[2]}};
    wst   = (v3){{w2[0], w2[1], w2[2]}};       // blended onto the final wrap below
    // The rig is the one MID value on the body (the operator's light carrier
    // plays the same role): faded olive webbing over the dark tunic.
    nyl   = (v3){{0.400f, 0.405f, 0.300f}};
    web   = (v3){{0.300f, 0.235f, 0.165f}};   // brown leather belt and bandolier
    boot  = kind == INS_SCOUT ? (v3){{0.560f, 0.540f, 0.500f}}   // worn trainers
                              : (v3){{0.170f, 0.150f, 0.130f}};
    rub   = dark;                             // no pads: the knee is trouser
    bala  = (v3){{0.090f, 0.085f, 0.085f}};   // the eye slit's shadow
  }
  if (g_fig_team == 0) {        // ALPHA: red
    olive = (v3){{0.335f, 0.320f, 0.330f}};
    dark  = (v3){{0.330f, 0.300f, 0.305f}};
    helm  = (v3){{0.820f, 0.160f, 0.120f}};
    nyl   = (v3){{0.720f, 0.330f, 0.270f}};
    web   = (v3){{0.235f, 0.200f, 0.200f}};
    rub   = (v3){{0.250f, 0.230f, 0.235f}};
    gm2   = (v3){{0.058f, 0.050f, 0.050f}};   // smoked glass: the RED is the masses
    bala  = (v3){{0.270f, 0.255f, 0.260f}};
  } else if (g_fig_team == 1) { // BRAVO: blue
    olive = (v3){{0.320f, 0.370f, 0.460f}};
    dark  = (v3){{0.275f, 0.315f, 0.395f}};
    helm  = (v3){{0.180f, 0.420f, 0.920f}};
    nyl   = (v3){{0.450f, 0.580f, 0.780f}};
    web   = (v3){{0.150f, 0.165f, 0.200f}};
    boot  = (v3){{0.130f, 0.130f, 0.150f}};
    rub   = (v3){{0.160f, 0.170f, 0.200f}};
    gm2   = (v3){{0.048f, 0.055f, 0.068f}};   // smoked glass: the BLUE is the masses
    bala  = (v3){{0.255f, 0.275f, 0.315f}};
  }
  // The check is woven INTO whatever the wrap ends up (a team's colour in a team
  // game), so the winding stays one cloth.
  if (ins) wst = v3_lerp(helm, wst, 0.55f);
  if (palette == 1 && g_fig_gear >= 0 && g_fig_gear < GEAR_THEME_COUNT) {
    // The armory's colourway on the whole kit — ALWAYS, team game or not.
    // This branch only ever runs for the LOCAL player's own body (the armory
    // pick is a client setting; nobody else's screen draws it), so painting
    // its three big masses in the team colour told nobody anything and threw
    // the player's outfit away the moment a team match started. His side is
    // on his armband instead (kt.arm below).
    const body_kit_t *bk = &BODY_KITS[g_fig_gear];
    #define BK(f) ((v3){{bk->f[0], bk->f[1], bk->f[2]}})
    olive = BK(uni);
    dark  = BK(pants);
    nyl   = BK(carrier);
    helm  = BK(helm);
    web  = BK(web);
    boot = BK(boot);
    rub  = BK(rub);
    bala = BK(bala);
    gm2  = BK(gog);
    #undef BK
  }
  if (flash01 > 0) {  // hit flash: every surface washes toward the flash color
    olive = v3_lerp(olive, flash_col, flash01);
    dark  = v3_lerp(dark, flash_col, flash01);
    boot  = v3_lerp(boot, flash_col, flash01);
    helm  = v3_lerp(helm, flash_col, flash01);
    nyl   = v3_lerp(nyl, flash_col, flash01);
    web   = v3_lerp(web, flash_col, flash01);
    rub   = v3_lerp(rub, flash_col, flash01);
  }

  // Band colors. The chains are colored band by band, so gear costs no extra
  // geometry — but every band now names a real material, and each one gets its
  // own roughness where it is drawn. At 15 m the silhouette is all the player
  // gets, so the value steps stay deliberate: darkest webbing, mid uniform,
  // lightest sleeves, with the coyote carrier reading warm against the olive.
  v3 gear = nyl;                      // plate carrier + pouches
  v3 belt = web;
  v3 pad = rub;                       // knee / elbow pads
  v3 pants = dark;
  // Everything below the knee has been walking through the arena, so it carries
  // the arena's colour. Two tones mixed toward the ground albedo — the cheapest
  // thing that stops a uniform reading as new-issue cloth, and it lifts the
  // darkest part of the figure out of the floor's own shadow.
  v3 dust1 = v3_lerp(dark, g_theme.ground, 0.20f);   // shin (the boot upper
                                                     // derives its own tint at
                                                     // the draw site)
  // The yoke band rides ABOVE the uniform value (it sits at the chest line,
  // where the ramp peaks), not below it.
  v3 vest = v3_scale(olive, 1.04f), sleeve = v3_scale(olive, 0.97f);
  // Light tan leather, DECOUPLED from the near-black boot: the lit hand must
  // sit >=1.15x OVER the sleeve (measured 0.82x under it, the x1.13 size buy
  // was invisible and the hands stayed dark blobs at the grip).
  // ONE GLOVE. The player's third-person glove was slate blue and his
  // first-person one was brown leather — the same man with different hands
  // depending on which camera was looking at him, and the most obvious of the
  // divergences this pass exists to remove. Leather in both, lifted from the
  // viewmodel's 0.340 so it still separates from the rifle at 20 m: measured
  // against the slate sleeve it sits at 0.86x in luminance and a full hue family
  // away, which is a leather glove on a blue uniform rather than a value step.
  // BLACK tactical gloves. Dark-on-light against the sleeve is a clean value
  // step; against the dark rifle the separation comes from ROUGHNESS
  // (MAT_GLOVE_R 0.72 vs the weapon's 0.30-0.55) and the hero rim, so the
  // tone stays a charcoal with a breath of warmth rather than a void black.
  v3 glove = palette == 1 ? (v3){{0.470f, 0.385f, 0.270f}}   // coyote tan
                          : (v3){{0.160f, 0.155f, 0.165f}};
  v3 gnt = v3_scale(glove, 0.88f);
  if (g_fig_gear >= 0 && g_fig_gear < GEAR_THEME_COUNT) {
    // The viewmodel glove's own colours: its fabric shell and the gauntlet's
    // hard plate. SLATE (row 0) is the identity row of GEAR_THEMES, so its
    // colours are the meshes' authored references (gear_dye's SLATE_R/HARD_R).
    static const float SLATE_FAB[3] = {0.204f, 0.224f, 0.268f},
                       SLATE_HARD[3] = {0.166f, 0.186f, 0.230f};
    const gear_theme_t *gt = &GEAR_THEMES[g_fig_gear];
    const float *f = g_fig_gear ? gt->fab : SLATE_FAB, *h = g_fig_gear ? gt->hard : SLATE_HARD;
    glove = (v3){{f[0], f[1], f[2]}};
    gnt   = (v3){{h[0], h[1], h[2]}};
  }
  if (flash01 > 0) {
    glove = v3_lerp(glove, flash_col, flash01);
    gnt   = v3_lerp(gnt, flash_col, flash01);
  }
  kit_t kt;
  kt.olive = olive;
  kt.boot = boot;
  kt.helm = helm;
  kt.web = web;
  kt.rub = rub;
  kt.gm2 = gm2;
  kt.gear = gear;
  kt.belt = belt;
  kt.pad = pad;
  kt.pants = pants;
  kt.dust1 = dust1;
  kt.vest = vest;
  kt.sleeve = sleeve;
  kt.glove = glove;
  kt.gnt = gnt;
  kt.bala = flash01 > 0 ? v3_lerp(bala, flash_col, flash01) : bala;
  kt.wst = flash01 > 0 ? v3_lerp(wst, flash_col, flash01) : wst;
  // The FRIENDLY ARMBAND: a cyan band round each upper arm of every operator,
  // in every outfit and every team. It is the SIGNAL colour the HUD gives
  // friends, on the one part of the body visible from every side, and it costs
  // no geometry (it recolours two bands of the deltoid pad). Insurgents never
  // wear it, so a figure with a cyan arm is never a target.
  // v5: DESATURATED BLUE-GREY, not lit-looking cyan (0.20, 0.70, 0.80, sat
  // 0.75). Saturation 0.19, luminance 0.55: a light cool band on a dark or
  // mid sleeve and a dark one on ARCTIC's white, never the insurgent's brown
  // webbing — measured by tools/fig_contrast.py and the 26_lineup capture.
  kt.arm = ins ? v3_scale(web, 0.92f) : (v3){{0.500f, 0.560f, 0.620f}};
  // The local body in a team game: the armband carries the side (the outfit
  // keeps the masses, see above), at the team carrier's colour.
  if (!ins && palette == 1 && g_fig_gear >= 0 && g_fig_team == 0) kt.arm = (v3){{0.720f, 0.330f, 0.270f}};
  if (!ins && palette == 1 && g_fig_gear >= 0 && g_fig_team == 1) kt.arm = (v3){{0.450f, 0.580f, 0.780f}};
  if (flash01 > 0) kt.arm = v3_lerp(kt.arm, flash_col, flash01);
  kt.ins = ins;
  if (g_fig_sil) {   // the silhouette test: shape alone must name the side
    v3 k0 = {{0.02f, 0.02f, 0.02f}};
    kt.olive = kt.boot = kt.helm = kt.web = kt.rub = kt.gm2 = k0;
    kt.gear = kt.belt = kt.pad = kt.pants = kt.dust1 = k0;
    kt.vest = kt.sleeve = kt.glove = kt.bala = kt.gnt = kt.wst = kt.arm = k0;
  }
  return kt;
}

// The torso chain's own cross-section at height T up the spine, interpolated
// exactly as fig_chain sweeps between its nodes: half-width rx, half-depth rz,
// the fore/aft offset F and the fraction of the spine twist. The table is
// fig_torso's node list; a piece of kit that has to ride OUTSIDE the shirt
// (the bandolier, the radio) asks here instead of guessing a radius.
#define FIG_TORSO_N 9
static const float FIG_TORSO_ST[FIG_TORSO_N][5] = {   // T, F, rx, rz, twist
  {-0.120f,  0.012f, 0.058f, 0.070f, 0.00f}, {-0.046f, -0.004f, 0.140f, 0.100f, 0.10f},
  { 0.020f, -0.002f, 0.148f, 0.096f, 0.19f},
  { 0.062f, -0.004f, 0.158f, 0.114f, 0.28f}, { 0.128f,  0.004f, 0.146f, 0.106f, 0.45f},
  { 0.205f,  0.010f, 0.158f, 0.122f, 0.66f}, { 0.285f,  0.016f, 0.172f, 0.136f, 0.86f},
  { 0.352f,  0.018f, 0.184f, 0.134f, 0.94f}, { 0.472f,  0.006f, 0.156f, 0.108f, 1.00f}};
static void fig_torso_at(float T, float *F, float *rx, float *rz, float *twf) {
  int i = 0;
  while (i < FIG_TORSO_N - 2 && T > FIG_TORSO_ST[i + 1][0]) i++;
  const float *a = FIG_TORSO_ST[i], *b = FIG_TORSO_ST[i + 1];
  float k = f_clamp((T - a[0]) / (b[0] - a[0]), 0.0f, 1.0f);
  *F = a[1] + (b[1] - a[1]) * k;
  *rx = a[2] + (b[2] - a[2]) * k;
  *rz = a[3] + (b[3] - a[3]) * k;
  *twf = a[4] + (b[4] - a[4]) * k;
}
// A point standing `off` metres proud of the torso's FRONT surface at height T
// and `x` across. The torso is a round profile now (an ellipse in rx/rz), so the
// depth there is rz * sqrt(1 - (x/rx)^2). On the far tiers it falls back to the
// chamfered octagon, whose depth is never less than the ellipse's, so a part
// placed off this formula only ever sits a hair deeper there.
static v3 fig_torso_front(const pose_t *P, v3 spine, v3 prt, v3 Rc, float tw,
                          float T, float x, float off, v3 *fw_out) {
  float F, rx, rz, twf;
  fig_torso_at(T, &F, &rx, &rz, &twf);
  v3 crt = v3_rot_axis(fig_perp(spine, prt), spine, twf * tw);
  v3 cfw = v3_norm(v3_cross(spine, crt));
  v3 cfw0 = v3_norm(v3_cross(spine, Rc));   // the torso's F is along the CHEST's forward
  float u = f_clamp(fabsf(x) / rx, 0.0f, 1.0f);
  float d = rz * sqrtf(1.0f - u * u);
  if (fw_out) *fw_out = cfw;
  return v3_add(P->pelvis, v3_add(v3_scale(spine, T), v3_add(v3_scale(crt, x),
                v3_add(v3_scale(cfw, d + off), v3_scale(cfw0, F)))));
}

// THE SEAT PANELS, standing (draw.c builds them per leg from the live hip
// joint; these are the same nodes at rest): T up the spine, L out to the side,
// F fore/aft, rx across, rz deep. hip_surf reads them so belt kit can sit ON
// the trousers rather than at a guessed radius.
#define FIG_SEAT_N 3
// THE BATTLE BELT: a band 46 mm tall standing proud of the waist and of the
// tops of both seat panels, which end inside it — the trousers' top edge is
// under the belt, as it is on a man.
#define FIG_BELT_T0 0.012f
#define FIG_BELT_T1 0.066f
#define FIG_BELT_F (-0.006f)
#define FIG_BELT_RX 0.172f
#define FIG_BELT_RZ 0.130f
static const float FIG_SEAT_ST[FIG_SEAT_N][5] = {   // T, L, F, rx, rz
  {-0.070f, 0.095f, -0.022f, 0.101f, 0.112f},
  {-0.020f, 0.072f, -0.020f, 0.112f, 0.100f},
  { 0.040f, 0.048f, -0.013f, 0.088f, 0.087f}};
// Farthest root t of the ray t*(c, s) on an ellipse centred (cx, cf), radii a, b;
// -1 when the ray misses it.
static float hip_ray_ellipse(float c, float s, float cx, float cf, float a, float b) {
  float ia = 1.0f / (a * a), ib = 1.0f / (b * b);
  float A = c * c * ia + s * s * ib, B = -2.0f * (c * cx * ia + s * cf * ib);
  float C = cx * cx * ia + cf * cf * ib - 1.0f;
  float D = B * B - 4.0f * A * C;
  if (D < 0.0f) return -1.0f;
  return (-B + sqrtf(D)) / (2.0f * A);
}
// How far the trouser/shirt surface stands from the spine at height T, in the
// direction phi round the hips (0 = the right hip, +pi/2 = straight ahead, as
// prt cos + pfw sin): the larger of the torso ring and either seat panel there.
static float hip_surf(float T, float phi) {
  float c = cosf(phi), s = sinf(phi);
  float F, rx, rz, twf;
  fig_torso_at(T, &F, &rx, &rz, &twf);
  float best = hip_ray_ellipse(c, s, 0.0f, F, rx, rz);
  if (T >= FIG_BELT_T0 && T <= FIG_BELT_T1)
    best = fmaxf(best, hip_ray_ellipse(c, s, 0.0f, FIG_BELT_F, FIG_BELT_RX, FIG_BELT_RZ));
  if (T >= FIG_SEAT_ST[0][0] && T <= FIG_SEAT_ST[FIG_SEAT_N - 1][0]) {
    int i = 0;
    while (i < FIG_SEAT_N - 2 && T > FIG_SEAT_ST[i + 1][0]) i++;
    const float *a = FIG_SEAT_ST[i], *b = FIG_SEAT_ST[i + 1];
    float k = f_clamp((T - a[0]) / (b[0] - a[0]), 0.0f, 1.0f);
    float L = a[1] + (b[1] - a[1]) * k, Fs = a[2] + (b[2] - a[2]) * k;
    float ax = a[3] + (b[3] - a[3]) * k, az = a[4] + (b[4] - a[4]) * k;
    for (int sd = -1; sd <= 1; sd += 2)
      best = fmaxf(best, hip_ray_ellipse(c, s, (float)sd * L, Fs, ax, az));
  }
  return best;
}

// The trunk: torso prism, plate carrier, shoulder straps, assault pack and
// antenna, and the two belt mag pouches. Called first, and it ESTABLISHES the
// figure's batch state — the ground-height AO ramp every builder below
// inherits, and cloth as the opening material.
FIG_INLINE void fig_torso(const pose_t *P, const kit_t *kt, v3 Rc, v3 spine,
                          v3 prt) {
  // A figure lit like the world it stands in: the same height ramp the cover
  // boxes get, so boots and knees sit in their own shadow while shoulders and
  // helmet catch the sky. As a material term (not baked into the albedo) it
  // dims the ambient without dimming the sun, which is what occlusion does.
  // Weak, because the sun shadow map now does what this used to fake. It only
  // survives as a hint that the ground steals sky from whatever is near it.
  ao_set(g_fig_y, 0.55f, 0.84f);
  mat_set(MAT_CLOTH_R, 0.0f);

  // Torso: ONE swept prism from crotch to shoulder yoke. The cross-section rolls
  // from the pelvis frame into the chest frame on the way up, so the spine twist
  // becomes a smooth twist of the MESH instead of two boxes shearing apart.
  float tw = atan2f(v3_dot(v3_cross(prt, Rc), spine), v3_dot(prt, Rc));
  // ROUND, k=16, smooth-shaded (was k=8 faceted — see below for the history).
  // The faceted torso read as a stack of planks against the sky: every band edge
  // was a crease, and the octagon's four diagonal arrises ran the whole height of
  // the body. A round profile with averaged normals turns the same node list into
  // a V-taper the light rolls over. Distance tiers below 16 still drop it to the
  // 12-gon / octagon, so the far-figure cost is what it was.
  // WAS FACETED, k=8: the redesign's stated art direction is a chunky flat-faceted
  // operator that rhymes with the arena's chamfered crates, and the torso is
  // where that decision is most visible. The chamfered octagon puts a real flat
  // plate on the chest and back where the 16-gon read as a vinyl tube; the
  // flat-shaded facet family also value-jitters per band, which is the crate
  // look. Halving the profile also halves what the biggest part of 21 figures
  // costs, which is what pays for the gear silhouette.
  // The SIDE view is the historically starved axis (every depth on this figure
  // was once measured short), so each node now carries its own fore/aft offset
  // along cfw: glutes push the pelvis mass BACK, the belly recedes, the chest
  // plate rides FORWARD-UP — an S-profile instead of a vertical stick.
  // Headroom: the collider is 2 * PLAYER_RADIUS = 0.70 m across and the widest
  // thing on the figure stays ~0.53 m (deltoid line), so nothing grows outside
  // the box the bullets are tested against.
  v3 cfw0 = v3_norm(v3_cross(spine, Rc));   // chest forward (the facing)
  #define TN(T, F, RX, RZ, COL, TW_, AO_) \
    {v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), v3_scale(cfw0, (F)))), \
     COL, COL, (RX), (RZ), (TW_) * tw, (AO_)}
  // THE CROTCH HANGS BETWEEN THE THIGHS, NOT DOWN THE SPINE. 12 cm "down the
  // spine" from the pelvis is between the legs only while the body is upright.
  // In a crouch, a forward-leaning run or a kneel the thighs swing forward and
  // up while the spine stays near vertical, and that node stayed where it was:
  // 17 cm below the hip joints and 6 cm under the (now level) thighs — a
  // sagging lump under the seat that read, from behind, as something in the
  // trousers. `figm` measured it (torso y0 349 mm against hips at 515).
  // The node is now anchored to the HIP JOINTS: the same distance below the
  // hip line as before, but measured ALONG THE THIGHS. Standing, the thighs
  // run down the spine and the node is where it always was; as the legs come
  // forward it rides forward between them and never drops beneath them.
  v3 crotch_p;
  {
    v3 old = v3_add(P->pelvis, v3_add(v3_scale(spine, -0.120f), v3_scale(cfw0, -0.008f)));
    v3 hm = v3_scale(v3_add(P->hip[0], P->hip[1]), 0.5f);
    float k = v3_dot(v3_sub(hm, old), spine);              // how far below the hip line
    v3 th0 = v3_sub(P->knee[0], P->hip[0]), th1 = v3_sub(P->knee[1], P->hip[1]);
    float l0 = v3_len(th0), l1 = v3_len(th1);
    crotch_p = old;
    if (k > 0.0f && l0 > 1e-4f && l1 > 1e-4f) {
      v3 td = v3_add(v3_scale(th0, 1.0f / l0), v3_scale(th1, 1.0f / l1));
      float tl = v3_len(td);
      if (tl > 1e-3f) {
        td = v3_scale(td, 1.0f / tl);
        // keep the old point's fore/aft and lateral offset from the hip line,
        // replace only its drop
        v3 rel = v3_sub(old, hm);
        v3 flat = v3_sub(rel, v3_scale(spine, v3_dot(rel, spine)));
        crotch_p = v3_add(v3_add(hm, flat), v3_scale(td, k));
      }
    }
  }
  // THE SEAT FOLLOWS THE HIP FLEXION. Upright, the thighs hang beside the
  // glutes ring and the three read together as hips. Flex the hips (crouch,
  // crouch-walk, a forward-leaning run) and the thighs swing forward out of
  // the rear view while the ring, square to a spine that now leans, tips its
  // back edge 9 cm DOWN: from behind, a 36 x 28 cm bowl hanging alone under
  // the belt. So as the hips flex the seat rings slim, rise and come forward
  // over the thighs; at zero flex every number is the one it always was.
  float hflex = 0.0f;
  {
    v3 th0 = v3_sub(P->knee[0], P->hip[0]), th1 = v3_sub(P->knee[1], P->hip[1]);
    float l0 = v3_len(th0), l1 = v3_len(th1);
    if (l0 > 1e-4f && l1 > 1e-4f) {
      v3 td = v3_add(v3_scale(th0, 1.0f / l0), v3_scale(th1, 1.0f / l1));
      float tl = v3_len(td);
      if (tl > 1e-3f) hflex = f_clamp(1.0f + v3_dot(spine, v3_scale(td, 1.0f / tl)), 0.0f, 1.0f);
    }
    hflex = f_clamp((hflex - 0.08f) / 0.80f, 0.0f, 1.0f);   // standing sway stays at 0
  }
  // THE SEAT IS ONE MASS. It used to be a 36 x 28 cm ring stood 2.6 cm under
  // the pelvis with a 22 cm crotch slab under it: from behind, a flat panel
  // with a square bottom edge hanging between the thighs, and the hip seats
  // (draw.c) bulging out under it as two balls. The glute ring now sits 2 cm
  // lower and a touch narrower so its underside rolls INTO the thighs, and the
  // crotch is tucked to an inseam (14 x 17 cm, 1 cm forward) — the seat ends
  // in a soft V between the legs instead of a shelf.
  crotch_p = v3_add(crotch_p, v3_scale(cfw0, 0.020f));
  // ...and NOT BELOW THE HIP LINE AT ALL. However it was dropped (down the
  // spine, then along the thighs), the torso's closing end hung under the seat
  // between the thighs, and in a running stride — one thigh forward, one back
  // — it showed from behind as a round lump between the legs. The seat panels
  // (draw.c) and the thighs close the seat and the inseam on their own; the
  // torso now ends tucked up under the pelvis, where nothing can see its end.
  // 6 cm under the pelvis node (the glute ring is at 4.6, so the two rings
  // never coincide — at +2 cm they did, and figcheck counted the z-fight) and
  // a quarter slimmer, so the end stays inside the seat and the thighs.
  // (Measured front, side, back, standing, crouched and in a running stride.)
  (void)crotch_p;
  const float cr_ = 0.75f;
  crotch_p = v3_add(P->pelvis, v3_add(v3_scale(spine, -0.060f), v3_scale(cfw0, 0.020f)));
  fnode_t t[FIG_TORSO_N] = {
    {crotch_p, kt->pants, kt->pants, (0.058f - 0.008f * hflex) * cr_, (0.070f - 0.016f * hflex) * cr_, 0.0f,
     0.70f - 0.25f * hflex},                                              // crotch
    TN(-0.046f + 0.034f * hflex, -0.004f + 0.008f * hflex, 0.140f - 0.010f * hflex,
       0.100f - 0.018f * hflex, kt->pants, 0.10f, 0.0f),                  // glutes: mass BACK
    // The seat-to-belt roll. Glutes to belt line lost 28 mm of depth in 88 mm,
    // which on a ring sweep is one steep band — the horizontal shelf across the
    // lower back and the seat. A node a touch FULLER than the straight line
    // between them makes that a curve.
    TN( 0.020f + 0.012f * hflex, -0.002f + 0.004f * hflex, 0.148f - 0.006f * hflex,
        0.096f - 0.006f * hflex, kt->pants, 0.19f, 0.0f),
    TN( 0.062f, -0.004f, 0.158f, 0.114f, kt->belt,  0.28f, 0.0f),   // belt line
    TN( 0.128f,  0.004f, 0.146f, 0.106f, kt->olive, 0.45f, 0.0f),   // waist, pulled in
    TN( 0.205f,  0.010f, 0.158f, 0.122f, kt->olive, 0.66f, 0.0f),   // lower ribcage
    TN( 0.285f,  0.016f, 0.172f, 0.136f, kt->olive, 0.86f, 0.0f),   // chest, fwd-up
    TN( 0.352f,  0.018f, 0.184f, 0.134f, kt->olive, 0.94f, 0.0f),   // upper chest
    TN( 0.472f,  0.006f, 0.156f, 0.108f, kt->vest,  1.00f, 0.0f)};  // shoulder yoke
  #undef TN
  probe("torso");
  // Damped jitter: full crate-jitter put a 1.35x light hoop mid-garment
  // (adjacent bands of ONE cloth suit), which reads as chain construction,
  // not as designed planes. The facet columns carry the low-poly read.
  g_fig_jit = 0.40f;
  fig_chain(t, FIG_TORSO_N, 16, 1, 1, prt);
  {
    v3 bk = v3_scale(kt->belt, 0.92f);
    #define BN(T, COL) {v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), v3_scale(cfw0, FIG_BELT_F))), \
                        COL, COL, FIG_BELT_RX, FIG_BELT_RZ, 0.22f * tw, 0.0f}
    fnode_t bt[2] = {BN(FIG_BELT_T0, bk), BN(FIG_BELT_T1, kt->belt)};
    #undef BN
    probe("belt");
    g_fig_jit = 0.0f;
    fig_chain(bt, 2, 16, 1, 1, prt);
    g_fig_jit = 0.40f;
  }
  // THE TUNIC: the insurgent's biggest outline change, and the one an operator
  // can never have. A long shirt worn OUTSIDE the trousers, split at the sides
  // (as a perahan is), so it is two panels — front and back — hanging from
  // under the belt to three quarters down the thigh. That flare below the belt
  // is the line a plate carrier and a fitted uniform never make, and it reads
  // in pure silhouette at 50 m where the head is four pixels.
  // Each panel hangs in front of (behind) whichever thigh is further forward
  // (back) at its hem, so a striding leg pushes the cloth instead of passing
  // through it; the panel's top node is buried in the torso below the belt and
  // leaves it on a steep line, so the join is a decisive crossing, never a
  // grazing one. Panels are 12 mm cloth: rx across the hips, rz the thickness.
  if (kt->ins) {
    v3 pfw0 = v3_norm(v3_cross(spine, prt));
    v3 hem[2], mid = {{0}};
    float fmax = -1.0f, fmin = 1.0f, umax = -1.0f, umin = 1.0f;
    for (int i = 0; i < 2; i++) {
      v3 th = v3_sub(P->knee[i], P->hip[i]);
      hem[i] = v3_add(P->hip[i], v3_scale(th, 0.30f));
      mid = v3_add(mid, v3_scale(hem[i], 0.5f));
      float f = v3_dot(v3_sub(hem[i], P->pelvis), pfw0);
      fmax = fmaxf(fmax, f);
      fmin = fminf(fmin, f);
      // ...and where the thigh is at the crotch line, for the middle node.
      float u = v3_dot(v3_sub(v3_add(P->hip[i], v3_scale(th, 0.30f)), P->pelvis), pfw0);
      umax = fmaxf(umax, u);
      umin = fminf(umin, u);
    }
    float drop = v3_dot(v3_sub(P->pelvis, mid), spine);   // pelvis -> hem, along the spine
    if (drop < 0.13f) drop = 0.13f;                       // a crouch folds it, never inverts it
    v3 tun = v3_scale(kt->olive, 0.96f), tdk = v3_scale(kt->olive, 0.84f);
    // ONE CLOSED SKIRT, not two panels. Split front/back panels read as a
    // plate and a scabbard from the side (edge-on, 12 mm of cloth hanging off a
    // hip is a blade). A closed hem that follows the legs reads as cloth from
    // every side: each station's fore/aft centre and depth are re-solved from
    // where the two thighs are, so a stride widens the skirt instead of a knee
    // passing through it. The thighs reach 0.185 across, the skirt 0.20-0.21.
    float cu = 0.5f * (umax + umin), du = 0.5f * (umax - umin);
    float ch = 0.5f * (fmax + fmin), dh = 0.5f * (fmax - fmin);
    #define TU(T, F, RX, RZ, COL, AO_) {v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), \
                                     v3_scale(pfw0, (F)))), COL, COL, (RX), (RZ), 0, (AO_)}
    // A SHIRT-TAIL, NOT A SACK. The hem used to hang three quarters down the
    // thigh with a CLOSED end cap whose depth was stretched to span both legs
    // in a stride: in every walk, run and crouch that was a capped bag between
    // the knees, which from behind read as something in the trousers. The
    // tunic now ends just below the crotch (30% down the thigh), where the
    // thighs swallow its cap, and its depth never reaches past the hips.
    (void)cu; (void)du;
    fnode_t tu[4] = {
      TU(0.058f, 0.000f, 0.138f, 0.090f, tun, 0.0f),               // buried under the belt
      // (wide and deep enough to hang OVER the seat panels: draw.c's seat
      // stands 0.19 m out at the hips and 0.15 m behind)
      TU(0.004f, -0.012f, 0.204f, 0.168f, tun, 0.0f),
      TU(-0.060f, 0.5f * ch - 0.008f, 0.210f, fminf(fmaxf(dh + 0.094f, 0.168f), 0.186f), tun, 0.0f),
      TU(-drop, ch, 0.186f, fminf(fmaxf(dh + 0.050f, 0.122f), 0.148f), tdk, 0.60f)};   // the hem
    #undef TU
    probe("tunic");
    g_fig_jit = 0.40f;
    fig_chain(tu, 4, 16, 1, 1, prt);
  }
  g_fig_jit = 1.0f;
  mat_set(MAT_GEAR_R, 0.0f);
  // Plate carrier: STANDOFF, not paint. The old wrap hugged the torso within
  // 6-14 mm everywhere, which reads as a colour change on the shirt; a carrier
  // is a box worn OVER a body, and the ~34 mm of front/back plate standoff is
  // most of what makes the silhouette read loaded from the side. Same k=8
  // profile family and same hint as the torso, so its flat chest plate is
  // parallel to the torso's own facet — strictly outside at every station
  // (rx >= +8 mm, rz >= +26 mm against the interpolated torso radii), never
  // crossing (the depth-order-seam class figcheck cannot see).
  #define CN(T, F, RX, RZ, TW_, AO_) \
    {v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), v3_scale(cfw0, (F)))), \
     kt->gear, kt->gear, (RX), (RZ), (TW_) * tw, (AO_)}
  fnode_t pc[4] = {
    CN(0.190f, 0.010f, 0.172f, 0.152f, 0.62f, 0.72f),
    CN(0.278f, 0.016f, 0.196f, 0.170f, 0.84f, 0.0f),
    CN(0.352f, 0.018f, 0.204f, 0.170f, 0.94f, 0.0f),
    CN(0.415f, 0.012f, 0.184f, 0.148f, 0.99f, 0.0f)};
  // The INSURGENT's chest rig: a low band round the ribs, no plates, so it
  // stands only 11-18 mm off the shirt (still strictly outside every torso
  // station, and on the torso's own twist) and stops below the collarbones.
  int vest = FIG_VEST;
  if (vest == VEST_RIG) {
    fnode_t rg[4] = {
      CN(0.196f, 0.010f, 0.168f, 0.138f, 0.64f, 0.72f),
      CN(0.250f, 0.014f, 0.178f, 0.146f, 0.78f, 0.0f),
      CN(0.300f, 0.016f, 0.186f, 0.152f, 0.88f, 0.0f),
      CN(0.322f, 0.016f, 0.184f, 0.146f, 0.91f, 0.0f)};
    memcpy(pc, rg, sizeof pc);
  }
  #undef CN
  // v5: THE CUMMERBUND AND THE MOLLE ROWS, as nodes OF the carrier rather
  // than straps laid on it: the plate carrier is one surface, and a webbing row
  // is that surface's own band in the webbing colour, so it wraps the sides and
  // the back and can never stand off or seam. Interpolated on the carrier's own
  // radii and twist so the outline is unchanged. Two rows low (the cummerbund,
  // which is what a side view reads), one high under the arms.
  fnode_t pcm[10];
  int npc = 4;
  if (vest == VEST_CARRIER && g_fig_k > 8) {
    static const float RT[6] = {0.212f, 0.226f, 0.244f, 0.258f, 0.300f, 0.312f};
    v3 row = v3_scale(kt->web, 1.25f);
    int n = 0;
    pcm[n++] = pc[0];
    for (int q = 0; q < 6; q++) {
      float T = RT[q];
      int j = T < 0.278f ? 0 : 1;
      static const float CT[4] = {0.190f, 0.278f, 0.352f, 0.415f};
      float k = (T - CT[j]) / (CT[j + 1] - CT[j]);
      fnode_t nd = pc[j];
      nd.p = v3_lerp(pc[j].p, pc[j + 1].p, k);
      nd.rx = pc[j].rx + (pc[j + 1].rx - pc[j].rx) * k;
      nd.rz = pc[j].rz + (pc[j + 1].rz - pc[j].rz) * k;
      nd.roll = pc[j].roll + (pc[j + 1].roll - pc[j].roll) * k;
      nd.ao = 0.0f;
      // the band ARRIVING at an odd node is the webbing row
      nd.col = nd.bcol = (q & 1) ? row : kt->gear;
      if (q == 4) pcm[n++] = pc[1];   // the original chest node, in order
      pcm[n++] = nd;
    }
    pcm[n++] = pc[2]; pcm[n++] = pc[3];
    npc = n;
  } else memcpy(pcm, pc, sizeof pc);
  if (vest != VEST_NONE) {
  probe("carrier");
  // SOFT BOX, k=16: the carrier keeps its flat plate faces and loses the razor
  // arrises (see FIG_PROF_SOFT). Every gear chain from here to the end of the
  // trunk is molded nylon and is built the same way; g_fig_soft is cleared
  // before anything that is not.
  g_fig_soft = 1;
  g_fig_jit = 0.45f;
  fig_chain(pcm, npc, 16, 1, 1, prt);
  g_fig_jit = 1.0f;
  // Front carrier placard: three compact rifle mag pouches on the lower plate
  // and a low-profile admin panel above them, rotated to the carrier's own
  // twist station so they ride flush with its front facet at every aim angle.
  {
    v3 pbase = fig_perp(spine, prt);
    v3 crt = v3_rot_axis(pbase, spine, 0.80f * tw);
    v3 cfw_p = v3_norm(v3_cross(spine, crt));
    v3 dk_web = v3_scale(kt->web, 0.92f);
    v3 pouch_c = v3_scale(kt->gear, 0.86f);
    float rig_in = vest == VEST_RIG ? -0.010f : 0.0f;   // the rig's face is lower than a plate's
    for (int mp_i = -1; mp_i <= 1; mp_i++) {
      float mx = (float)mp_i * 0.052f;
      #define MP_P(T, D) v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), \
                         v3_add(v3_scale(crt, mx), v3_scale(cfw_p, (D) + rig_in))))
      fnode_t fmp[4] = {
        {MP_P(0.204f, 0.166f), pouch_c, pouch_c, 0.018f, 0.016f, 0, 0.58f},
        {MP_P(0.220f, 0.174f), pouch_c, pouch_c, 0.023f, 0.022f, 0, 0.0f},
        {MP_P(0.276f, 0.176f), pouch_c, dk_web,  0.023f, 0.022f, 0, 0.0f},
        {MP_P(0.296f, 0.172f), dk_web,  dk_web,  0.019f, 0.017f, 0, 0.0f}};
      #undef MP_P
      probe("placard");
      g_fig_jit = 0.35f;
      fig_chain(fmp, 4, 16, 1, 1, crt);
      g_fig_jit = 1.0f;
      // v5: THE MAGAZINES, standing 14 mm out of each pouch's open top — the
      // thing that makes three bumps read as a triple mag placard. Narrower
      // than the pouch's top ring in both axes (15 x 9 against 19 x 17 mm), so
      // each one leaves the pouch through its cap, a decisive crossing. Dark
      // polymer. Not on the coarsest tiers, where they are under a pixel.
      if (g_fig_k > 8) {
        #define MG_P(T, D) v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), \
                           v3_add(v3_scale(crt, mx), v3_scale(cfw_p, (D) + rig_in))))
        v3 mgc = {{0.105f, 0.105f, 0.112f}};
        // Forward of the carrier face and LEANING OUT of it (15 mm forward over
        // 26 mm of rise): the pouches are two-thirds sunk in the carrier, so a
        // magazine at their depth lay along the plate and grazed it.
        fnode_t mg[3] = {
          {MG_P(0.284f, 0.184f), mgc, mgc, 0.0150f, 0.0100f, 0, 0.0f},
          {MG_P(0.302f, 0.192f), mgc, mgc, 0.0150f, 0.0100f, 0, 0.0f},
          {MG_P(0.310f, 0.196f), mgc, mgc, 0.0130f, 0.0085f, 0, 0.0f}};
        #undef MG_P
        probe("placardmag");
        mat_set(MAT_RUBBER_R, 0.0f);
        fig_chain(mg, 3, 8, 1, 1, crt);
        mat_set(MAT_GEAR_R, 0.0f);
      }
    }
    if (vest == VEST_RIG) goto no_admin;   // a rig has no admin panel up on a plate
    {
    v3 urt = v3_rot_axis(pbase, spine, 0.94f * tw);
    v3 ufw = v3_norm(v3_cross(spine, urt));
    #define AP_P(T, D) v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), v3_scale(ufw, (D))))
    v3 adm_c = v3_scale(kt->gear, 0.80f);
    fnode_t adm[3] = {
      {AP_P(0.322f, 0.176f), adm_c, adm_c, 0.052f, 0.015f, 0, 0.0f},
      {AP_P(0.350f, 0.180f), adm_c, adm_c, 0.058f, 0.018f, 0, 0.0f},
      {AP_P(0.376f, 0.174f), adm_c, adm_c, 0.050f, 0.014f, 0, 0.0f}};
    #undef AP_P
    probe("placard");
    g_fig_jit = 0.30f;
    fig_chain(adm, 3, 16, 1, 1, urt);
    g_fig_jit = 1.0f;
    }
  no_admin:;
    // THE CARRIER'S WORKING KIT (operators: a plate carrier only). Two frag
    // pouches stacked on its left front edge, their olive grenade tops showing;
    // a TOURNIQUET rolled on the upper right corner, black with its red tag; a
    // green chemlight taped on the upper left — lit (albedo over 1, the IR
    // strobe's emissive trick). Each rides the carrier's own twist station
    // (crt / cfw_p) at the depth of the carrier's face at that offset, so it
    // sits on the plate at every aim angle.
    // Close tiers only (g_fig_k > 12, inside ~6 m): at range a 14 mm tourniquet
    // is a pixel, and on the coarse carrier profile it would graze the plate.
    if (vest == VEST_CARRIER && !kt->ins && g_fig_k > 12) {
      v3 pch = v3_scale(kt->gear, 0.82f), frag = {{0.300f, 0.330f, 0.220f}};
      #define CF(X, T, D) v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), \
                          v3_add(v3_scale(crt, (X)), v3_scale(cfw_p, (D)))))
      for (int g = 0; g < 2; g++) {
        float T = 0.214f + 0.054f * (float)g, X = -0.118f, D = 0.166f;
        fnode_t fp[3] = {
          {CF(X, T - 0.022f, D), pch, pch, 0.0170f, 0.0170f, 0, 0.55f},
          {CF(X, T + 0.012f, D), pch, pch, 0.0200f, 0.0200f, 0, 0.0f},
          {CF(X, T + 0.020f, D), pch, pch, 0.0170f, 0.0170f, 0, 0.0f}};
        probe("fragpouch");
        g_fig_soft = 1;
        fig_chain(fp, 3, 12, 1, 1, crt);
        g_fig_soft = 0;
        fnode_t ft[2] = {
          {CF(X, T + 0.018f, D), frag, frag, 0.0150f, 0.0150f, 0, 0.0f},
          {CF(X, T + 0.038f, D), frag, frag, 0.0070f, 0.0070f, 0, 0.0f}};
        fig_chain(ft, 2, 10, 1, 1, crt);
      }
      // the tourniquet: a black roll across the corner, its red tag hanging
      // v5: the tag is plain tan webbing, not a red flag on a friendly chest.
      v3 tq = {{0.060f, 0.060f, 0.065f}}, red = {{0.340f, 0.310f, 0.250f}};
      fnode_t tqn[2] = {
        {CF(0.070f, 0.368f, 0.172f), tq, tq, 0.0140f, 0.0140f, 0, 0.0f},
        {CF(0.134f, 0.368f, 0.150f), tq, tq, 0.0140f, 0.0140f, 0, 0.0f}};
      probe("tourniquet");
      fig_chain(tqn, 2, 10, 1, 1, spine);
      fnode_t tg[2] = {
        {CF(0.084f, 0.360f, 0.184f), red, red, 0.0080f, 0.0035f, 0, 0.0f},
        {CF(0.084f, 0.330f, 0.184f), red, red, 0.0075f, 0.0030f, 0, 0.0f}};
      fig_chain(tg, 2, 8, 1, 1, crt);
      // the chemlight, UNLIT (v5): a dim olive stick, not a green lamp
      v3 cl = {{0.240f, 0.290f, 0.215f}};
      fnode_t cln[2] = {
        {CF(-0.110f, 0.356f, 0.168f), cl, cl, 0.0060f, 0.0060f, 0, 0.0f},
        {CF(-0.110f, 0.404f, 0.164f), cl, cl, 0.0060f, 0.0060f, 0, 0.0f}};
      probe("chemlight");
      fig_chain(cln, 2, 8, 1, 1, crt);
      #undef CF
    }
  }
  g_fig_soft = 0;
  }   // vest != VEST_NONE
  // THE GUNNER'S AMMUNITION BELT: a bandolier from the left shoulder to the
  // right hip across a bare shirt — the diagonal no operator's square carrier
  // has, readable from the front at any range the figure is more than a dot.
  // Bands alternate leather and brass, which is what a belt of rounds is.
  // 28 mm off the torso's own surface (fig_torso_front), so it never runs
  // parallel to the shirt; its two ends sink into the yoke and the belt line.
  if (kt->ins && FIG_INS_KIND == INS_GUNNER && vest == VEST_NONE) {
    v3 brass = {{0.640f, 0.500f, 0.240f}};
    fnode_t bd[6];
    for (int q = 0; q < 6; q++) {
      float u = (float)q / 5.0f;
      float T = 0.410f - 0.330f * u, x = -0.100f + 0.215f * u;
      float off = (q == 0 || q == 5) ? 0.004f : 0.026f;   // ends buried
      v3 fw;
      v3 p = fig_torso_front(P, spine, prt, Rc, tw, T, x, off, &fw);
      v3 c = (q & 1) ? brass : kt->web;
      bd[q] = (fnode_t){p, c, c, 0.011f, 0.030f, 0, 0.0f};
    }
    probe("bandolier");
    g_fig_jit = 0.30f;
    g_fig_soft = 1;
    fig_chain(bd, 6, 16, 1, 1, cfw0);
    g_fig_soft = 0;
    g_fig_jit = 1.0f;
  }
  // THE LEADER'S RADIO: a handset pouch on the left of his rig and a whip
  // antenna standing up past the shoulder. The only insurgent with an aerial,
  // so the one giving orders is the one a shooter should find first.
  if (kt->ins && FIG_INS_KIND == INS_LEADER && vest == VEST_RIG) {
    v3 rad = v3_scale(kt->web, 0.80f), fw;
    v3 r0 = fig_torso_front(P, spine, prt, Rc, tw, 0.215f, -0.090f, 0.010f, &fw);
    v3 r1 = fig_torso_front(P, spine, prt, Rc, tw, 0.255f, -0.090f, 0.034f, NULL);
    v3 r2 = fig_torso_front(P, spine, prt, Rc, tw, 0.305f, -0.090f, 0.034f, NULL);
    v3 r3 = fig_torso_front(P, spine, prt, Rc, tw, 0.322f, -0.090f, 0.016f, NULL);
    fnode_t rp[4] = {
      {r0, rad, rad, 0.024f, 0.016f, 0, 0.55f},
      {r1, rad, rad, 0.032f, 0.022f, 0, 0.0f},
      {r2, rad, rad, 0.032f, 0.022f, 0, 0.0f},
      {r3, rad, rad, 0.026f, 0.016f, 0, 0.0f}};
    v3 crt = v3_norm(v3_cross(fw, spine));
    probe("radio");
    g_fig_jit = 0.30f;
    g_fig_soft = 1;
    fig_chain(rp, 4, 16, 1, 1, crt);
    g_fig_soft = 0;
    g_fig_jit = 1.0f;
    v3 ab = v3_add(r2, v3_scale(fw, -0.006f));
    v3 out = v3_scale(crt, -1.0f);   // outboard, away from the face
    fnode_t an[4] = {
      {ab, kt->rub, kt->rub, 0.0090f, 0.0090f, 0, 0.0f},
      {v3_add(ab, v3_scale(spine, 0.060f)), kt->rub, kt->rub, 0.0070f, 0.0070f, 0, 0.0f},
      {v3_add(v3_add(ab, v3_add(v3_scale(spine, 0.220f), v3_scale(out, 0.040f))),
              v3_scale(P->ant_sway, 0.40f)), kt->rub, kt->rub, 0.0050f, 0.0040f, 0, 0.0f},
      {v3_add(v3_add(ab, v3_add(v3_scale(spine, 0.420f), v3_add(v3_scale(out, 0.090f),
              v3_scale(fw, -0.050f)))), P->ant_sway),
       kt->rub, kt->rub, 0.0030f, 0.0024f, 0, 0.0f}};
    probe("antenna");
    fig_chain(an, 4, 6, 1, 1, crt);
  }
  // Shoulder straps: the carrier is otherwise one black slab across the chest
  // with a hard top edge and nothing holding it up. Two short runs over each
  // trapezius break that mass and give the torso a visible harness.
  // (Not on the insurgent: they were sized to ride a plate carrier's top
  // edge, and without it they would sink into the shirt at a grazing angle.)
  if (vest == VEST_CARRIER) {
    v3 cf2 = v3_norm(v3_cross(spine, Rc));
    for (int e = 0; e < 2; e++) {
      float es = e ? 1.0f : -1.0f;
      fnode_t st[4] = {
        {v3_add(P->chest, v3_add(v3_scale(Rc, es * 0.060f),
           v3_add(v3_scale(spine, 0.016f), v3_scale(cf2, 0.112f)))),
         kt->gear, kt->gear, 0.034f, 0.019f, 0, 0.0f},
        // OUTBOARD, over the shoulder rather than over the throat. At 0.090
        // the strap's inner edge sat 52 mm off the midline and the collar's own
        // base ring reaches 76 — so the strap ran straight THROUGH the neck for
        // its whole length, nine crossing pairs of it, the second-worst join on
        // the figure. A harness passes over the trapezius, which is where these
        // two nodes are now.
        // 0.106 and a 33 mm half-width: the gap between the collar (76 mm) and
        // the deltoid's inner edge is narrow enough that a 38 mm strap has to
        // cross ONE of them, and a harness lying on a shoulder cap is what a
        // harness does — a harness through a throat is not.
        // v5: PADDED over the shoulder — the two top nodes are the strap's
        // foam pad, 6 mm thicker and a value step darker than the plate bags.
        {v3_add(P->chest, v3_add(v3_scale(Rc, es * 0.106f),
           v3_add(v3_scale(spine, 0.082f), v3_scale(cf2, 0.086f)))),
         kt->gear, v3_scale(kt->gear, 0.86f), 0.034f, 0.026f, 0, 0.0f},
        {v3_add(P->chest, v3_add(v3_scale(Rc, es * 0.112f),
           v3_add(v3_scale(spine, 0.092f), v3_scale(cf2, -0.010f)))),
         v3_scale(kt->gear, 0.86f), kt->gear, 0.034f, 0.027f, 0, 0.0f},
        {v3_add(P->chest, v3_add(v3_scale(Rc, es * 0.072f),
           v3_add(v3_scale(spine, 0.030f), v3_scale(cf2, -0.104f)))),
         kt->gear, kt->gear, 0.034f, 0.019f, 0, 0.0f}};
      probe("strap");
      g_fig_soft = 1;
      fig_chain(st, 4, 16, 1, 1, spine);
      g_fig_soft = 0;
    }
  }
  // Trapezius wedges: the head used to float on a long bare neck over a flat
  // shoulder line. Two short chains from the yoke out to each shoulder put the
  // neck-to-deltoid mass a real trunk has and visually shorten the neck ~30%.
  for (int e = 0; e < 2; e++) {
    float es = e ? 1.0f : -1.0f;
    fnode_t tp[3] = {
      {v3_add(P->chest, v3_add(v3_scale(spine, 0.085f), v3_scale(Rc, es * 0.040f))),
       kt->olive, kt->olive, 0.040f, 0.052f, 0, 0.0f},
      {v3_add(P->chest, v3_add(v3_scale(spine, 0.058f), v3_scale(Rc, es * 0.115f))),
       kt->olive, kt->olive, 0.036f, 0.048f, 0, 0.0f},
      {v3_add(P->chest, v3_add(v3_scale(spine, 0.042f), v3_scale(Rc, es * 0.172f))),
       kt->olive, kt->olive, 0.026f, 0.036f, 0, 0.0f}};
    probe("trap");
    // Round, so the neck-to-shoulder line is a slope the light rolls down rather
    // than three planks meeting the collar.
    fig_chain(tp, 3, 16, 1, 1, spine);
  }
  // Unified tactical assault pack on the carrier's back — EVERY element (main
  // compartment, top comms/bedroll housing, external rear pocket, twin side
  // sustainment pouches, vertical compression straps and whip antenna) is built
  // in the chest's own (Rc, spine, cfw) frame off P->chest, so the whole pack
  // moves as one rigid assembly with the upper torso instead of shearing
  // against pelvis-framed geometry.
  v3 cfw = v3_norm(v3_cross(spine, Rc));
  if (FIG_PACK == PACK_ASSAULT) {   // the insurgent travels light: no pack, no radio
    #define PP(X, Y, Z) v3_add(P->chest, v3_add(v3_scale(Rc, (X)), \
                        v3_add(v3_scale(spine, (Y)), v3_scale(cfw, (Z)))))
    v3 pack_col = v3_scale(kt->gear, 0.72f);
    v3 side_col = v3_scale(kt->gear, 0.88f);
    v3 top_col  = v3_scale(kt->web, 0.92f);
    // Main faceted compartment, front buried inside the carrier's backplate.
    fnode_t up[5] = {
      {PP(0.0f, -0.182f, -0.162f), pack_col, pack_col, 0.094f, 0.046f, 0, 0.55f},
      {PP(0.0f, -0.154f, -0.168f), pack_col, pack_col, 0.118f, 0.064f, 0, 0.0f},
      {PP(0.0f, -0.040f, -0.168f), pack_col, pack_col, 0.118f, 0.064f, 0, 0.0f},
      {PP(0.0f,  0.024f, -0.168f), pack_col, pack_col, 0.116f, 0.062f, 0, 0.0f},
      {PP(0.0f,  0.048f, -0.166f), pack_col, pack_col, 0.096f, 0.048f, 0, 0.0f}};
    probe("pack");
    g_fig_soft = 1;   // molded nylon: every pack chain is a soft box
    g_fig_jit = 0.35f;
    fig_chain(up, 5, 16, 1, 1, Rc);
    // Horizontal top comms / bedroll housing crowning the main compartment,
    // kept clear below the helmet nape in crouch and slide poses.
    fnode_t pt[4] = {
      {PP(-0.106f, 0.064f, -0.172f), top_col, top_col, 0.022f, 0.040f, 0, 0.0f},
      {PP(-0.088f, 0.064f, -0.172f), top_col, top_col, 0.030f, 0.054f, 0, 0.0f},
      {PP( 0.088f, 0.064f, -0.172f), top_col, top_col, 0.030f, 0.054f, 0, 0.0f},
      {PP( 0.106f, 0.064f, -0.172f), top_col, top_col, 0.022f, 0.040f, 0, 0.0f}};
    probe("pack");
    g_fig_jit = 0.30f;
    fig_chain(pt, 4, 16, 1, 1, spine);
    // Deliberate external lower utility pouch in carrier coyote, seated cleanly
    // into the main pack's rear face and standing 18 mm proud.
    fnode_t rp[4] = {
      {PP(0.0f, -0.148f, -0.220f), kt->gear, kt->gear, 0.068f, 0.018f, 0, 0.50f},
      {PP(0.0f, -0.128f, -0.224f), kt->gear, kt->gear, 0.082f, 0.026f, 0, 0.0f},
      {PP(0.0f, -0.032f, -0.224f), kt->gear, kt->gear, 0.082f, 0.026f, 0, 0.0f},
      {PP(0.0f, -0.012f, -0.220f), kt->gear, kt->gear, 0.068f, 0.018f, 0, 0.0f}};
    probe("pack");
    g_fig_jit = 0.30f;
    fig_chain(rp, 4, 16, 1, 1, Rc);
    // Twin side sustainment pouches on the pack's flanks (clear behind the
    // carrier's rear bevel, buried inboard into the main compartment).
    for (int sp_i = 0; sp_i < 2; sp_i++) {
      float sx = sp_i ? 1.0f : -1.0f;
      fnode_t sp2[4] = {
        {PP(sx * 0.118f, -0.150f, -0.188f), side_col, side_col, 0.018f, 0.022f, 0, 0.50f},
        {PP(sx * 0.118f, -0.130f, -0.188f), side_col, side_col, 0.026f, 0.030f, 0, 0.0f},
        {PP(sx * 0.118f, -0.026f, -0.188f), side_col, side_col, 0.026f, 0.030f, 0, 0.0f},
        {PP(sx * 0.118f, -0.006f, -0.188f), side_col, side_col, 0.018f, 0.022f, 0, 0.0f}};
      probe("pack");
      g_fig_jit = 0.30f;
      fig_chain(sp2, 4, 16, 1, 1, Rc);
    }
    g_fig_jit = 1.0f;
    // Two vertical compression straps hugging the external pouch and upper pack.
    mat_set(MAT_GEAR_R, 0.0f);
    for (int cs2 = 0; cs2 < 2; cs2++) {
      float ss = cs2 ? 1.0f : -1.0f;
      fnode_t vb[4] = {
        {PP(ss * 0.044f, -0.158f, -0.226f), kt->web, kt->web, 0.015f, 0.016f, 0, 0.0f},
        {PP(ss * 0.044f, -0.128f, -0.244f), kt->web, kt->web, 0.015f, 0.014f, 0, 0.0f},
        {PP(ss * 0.044f, -0.030f, -0.244f), kt->web, kt->web, 0.015f, 0.014f, 0, 0.0f},
        {PP(ss * 0.044f,  0.038f, -0.226f), kt->web, kt->web, 0.014f, 0.014f, 0, 0.0f}};
      probe("packstrap");
      fig_chain(vb, 4, 16, 1, 1, Rc);
    }
    g_fig_soft = 0;
    // Whip antenna seated in the strong-side top comms housing.
    {
      v3 ab = PP(0.076f, 0.088f, -0.172f);
      fnode_t an[4] = {
        {PP(0.076f, 0.078f, -0.172f), kt->rub, kt->rub, 0.0120f, 0.0120f, 0, 0.62f},
        {PP(0.076f, 0.104f, -0.172f), kt->rub, kt->rub, 0.0095f, 0.0095f, 0, 0.0f},
        {v3_add(v3_add(ab, v3_add(v3_scale(spine, 0.120f), v3_scale(cfw, -0.016f))),
         v3_scale(P->ant_sway, 0.40f)),
         kt->rub, kt->rub, 0.0064f, 0.0044f, 0, 0.0f},
        {v3_add(v3_add(ab, v3_add(v3_scale(spine, 0.248f), v3_scale(cfw, -0.062f))),
         P->ant_sway),
         kt->rub, kt->rub, 0.0034f, 0.0024f, 0, 0.0f}};
      probe("antenna");
      fig_chain(an, 4, 6, 1, 1, Rc);
    }
    #undef PP
  }
  // THE BELT KIT, SEATED ON THE BODY. Every pouch here used to hang at a fixed
  // offset from the pelvis, and the body under it has been re-cut twice since:
  // the dump pouch floated a centimetre off the seat, the IFAK sat on nothing,
  // and the two rifle pouches were tubes pointing straight FORWARD out of the
  // belt — end-on from the side they read as two hockey pucks. Now each one is
  // built against the surface it rides on (hip_surf: the waist and the two
  // seat panels, as draw.c cuts them), its back sunk a few millimetres into
  // it, and anything that hangs hangs FROM the belt: its top tucks under the
  // belt band and its body follows the curve of the seat down.
  v3 pfw = v3_norm(v3_cross(spine, prt));
  #define HK_DIR(PHI) v3_add(v3_scale(prt, cosf(PHI)), v3_scale(pfw, sinf(PHI)))
  #define HK_AT(PHI, T, OUT) v3_add(P->pelvis, v3_add(v3_scale(spine, (T)), \
                                    v3_scale(HK_DIR(PHI), hip_surf((T), (PHI)) + (OUT))))
  // Two double rifle-magazine pouches on the front of the belt, upright, a
  // darker flap over each; they also mark which way the hips face.
  for (int i = 0; i < 2; i++) {
    float phi = F_PI * 0.5f + (i ? -0.62f : 0.62f);      // front, 35 deg off centre
    v3 tg = v3_norm(v3_cross(spine, HK_DIR(phi)));      // along the belt
    v3 flap = v3_scale(kt->belt, 0.78f);
    fnode_t mp[4] = {
      {HK_AT(phi, -0.034f, 0.014f), kt->belt, kt->belt, 0.040f, 0.018f, 0, 0.55f},
      {HK_AT(phi, -0.022f, 0.016f), kt->belt, kt->belt, 0.044f, 0.020f, 0, 0.0f},
      {HK_AT(phi,  0.052f, 0.017f), kt->belt, flap,     0.044f, 0.021f, 0, 0.0f},
      {HK_AT(phi,  0.078f, 0.015f), flap, flap,         0.040f, 0.019f, 0, 0.0f}};
    probe("magpouch");
    g_fig_soft = 1;
    fig_chain(mp, 4, 8, 1, 1, tg);
    g_fig_soft = 0;
  }
  // THE BATTLE BELT'S KIT (operators): the IFAK at the back of the weak-side hip
  // with a subdued cross on its outer face, a DUMP POUCH hanging at the back of
  // the strong side, and a pistol magazine pouch on the weak-side hip.
  if (!kt->ins && g_fig_k > 8) {
    v3 pk = v3_scale(kt->gear, 0.82f);   // coyote kit on the dark belt
    // IFAK: a rolled pouch lying ALONG the belt, back-left, riding the belt band
    float iphi = -F_PI * 0.5f - 0.62f;
    v3 iout = HK_DIR(iphi);
    v3 iac = v3_norm(v3_cross(spine, iout));          // along the belt
    float ir = 0.030f;
    v3 ic = HK_AT(iphi, 0.052f, ir - 0.006f);
    fnode_t ik[3] = {
      {v3_add(ic, v3_scale(iac, -0.048f)), pk, pk, 0.026f, 0.024f, 0, 0.55f},
      {ic, pk, pk, ir, 0.029f, 0, 0.0f},
      {v3_add(ic, v3_scale(iac, 0.048f)), pk, pk, 0.026f, 0.024f, 0, 0.0f}};
    probe("ifak");
    g_fig_soft = 1;
    fig_chain(ik, 3, 12, 1, 1, iout);
    g_fig_soft = 0;
    // a belt loop over the roll at each end: the pouch is ON the belt
    for (int e = -1; e <= 1; e += 2) {
      v3 lc = v3_add(ic, v3_scale(iac, (float)e * 0.034f));
      fnode_t lp[2] = {
        {v3_sub(lc, v3_scale(iac, 0.006f)), kt->belt, kt->belt, ir + 0.003f, 0.032f, 0, 0.0f},
        {v3_add(lc, v3_scale(iac, 0.006f)), kt->belt, kt->belt, ir + 0.003f, 0.032f, 0, 0.0f}};
      fig_chain(lp, 2, 12, 1, 1, iout);
    }
    // a SUBDUED cross, a value step under the pouch (red is the hostile hue)
    v3 red = v3_scale(pk, 0.55f);
    v3 cface = v3_add(ic, v3_scale(iout, 0.0285f));
    for (int b = 0; b < 2; b++) {   // the cross: a bar along the belt, a bar up it
      v3 ax = b ? spine : iac;
      fnode_t cr[2] = {
        // the bars' caps at DIFFERENT depths: where they cross, coplanar caps z-fight
        {v3_sub(cface, v3_scale(iout, b ? 0.0040f : 0.0012f)), red, red, 0.0150f, 0.0050f, 0, 0.0f},
        {v3_add(cface, v3_scale(iout, b ? 0.0045f : 0.0018f)), red, red, 0.0150f, 0.0050f, 0, 0.0f}};
      fig_chain(cr, 2, 8, 1, 1, ax);
    }
    // the DUMP POUCH: a soft sack hung from the belt at the back of the strong
    // side. Its top is tucked behind the belt band; the body lies on the seat
    // and only its bottom swings (the antenna's 2nd-order follower).
    float dphi = -F_PI * 0.5f + 0.70f;
    v3 dtg = v3_norm(v3_cross(spine, HK_DIR(dphi)));
    v3 dcol = v3_scale(kt->gear, 0.70f);
    fnode_t dp[4] = {
      // HIGH ON THE BELT and short: below the belt line the seat moves with
      // the stride — the panel over the swinging leg swells out by two to
      // three centimetres — and a sack resting on it was crossed by it. Kept
      // above that, its bottom only just clears the top of the seat.
      {HK_AT(dphi,  0.074f, 0.010f), kt->belt, kt->belt, 0.036f, 0.014f, 0, 0.55f},  // the loop
      {HK_AT(dphi,  0.056f, 0.022f), dcol, dcol, 0.040f, 0.020f, 0, 0.0f},
      {v3_add(HK_AT(dphi, 0.024f, 0.030f), v3_scale(P->ant_sway, 0.05f)), dcol, dcol, 0.044f, 0.024f, 0, 0.0f},
      {v3_add(HK_AT(dphi, -0.006f, 0.030f), v3_scale(P->ant_sway, 0.10f)), dcol, dcol, 0.034f, 0.018f, 0, 0.0f}};
    probe("dumppouch");
    g_fig_soft = 1;
    fig_chain(dp, 4, 12, 1, 1, dtg);
    // the pistol magazine pouch, upright on the weak-side hip
    float pphi = F_PI + 0.30f;
    v3 ptg = v3_norm(v3_cross(spine, HK_DIR(pphi)));
    fnode_t ppm[3] = {
      {HK_AT(pphi, 0.000f, 0.011f), pk, pk, 0.018f, 0.012f, 0, 0.55f},
      {HK_AT(pphi, 0.012f, 0.013f), pk, pk, 0.020f, 0.014f, 0, 0.0f},
      {HK_AT(pphi, 0.074f, 0.012f), pk, pk, 0.018f, 0.012f, 0, 0.0f}};
    probe("pistolpouch");
    fig_chain(ppm, 3, 8, 1, 1, ptg);
    g_fig_soft = 0;
  }
  #undef HK_AT
  #undef HK_DIR
}

// Both arms, and the gloves closed round the grip cylinders the rifle
// publishes. Takes the weapon frame the CALLER solved: the gun draw after this
// reads the same frame and the two may not disagree about where the rifle is.
FIG_INLINE void fig_arms(const pose_t *P, const kit_t *kt, v3 Rc, v3 spine,
                         v3 g_org, v3 g_rt, v3 g_up, v3 g_fw, int wpn,
                         float brace_k, v3 brace_dir) {
  ao_set(g_fig_y, 0.55f, 0.84f);
  mat_set(MAT_CLOTH_R, 0.0f);
  // Arms: the chain reaches into the chest first, so the deltoid bevel closes
  // the shoulder instead of leaving a floating tube end at the seam.
  for (int i = 0; i < 2; i++) {
    float s = i ? 1.0f : -1.0f;
    // Six rings: a cylinder from shoulder to wrist is the single most obvious
    // "this is a mannequin" tell. Real mass distribution instead — deltoid cap at
    // the shoulder, bicep belly a third of the way down, a narrow elbow, the
    // forearm's flexor mass just past it, and a wrist half the elbow's width.
    v3 ua = v3_sub(P->elb[i], P->sho[i]);
    v3 fa = v3_sub(P->wri[i], P->elb[i]);
    // Split at the elbow for the same reason as the knee: a single chain through a
    // joint that flexes past 90 degrees has to pay for the fillet out of bone
    // length it does not have, and fig_chain pays by thinning the cross-section.
    fnode_t ar[4] = {
      // The upper arm starts AT the shoulder and runs straight to the elbow. It
      // used to begin 15 cm inboard inside the chest, which put a ~90 degree corner
      // at P->sho — and fig_chain pays for a corner it cannot fillet by thinning the
      // cross-section and opening one big flat bevel facet. Seen from outside THAT
      // facet is the square shoulder: a hard-edged plate where a deltoid should be
      // round. The ball below covers this chain's end cap, as at the knee and elbow.
      // v5: FOUR rings, a biceps belly. Three put the taper in one straight
      // line from shoulder to elbow — a cone, which is the constant tube the
      // review called out. The belly sits 30% down and swells in DEPTH (rz,
      // fore/aft of the arm), then the arm narrows hard into the elbow.
      {P->sho[i], kt->sleeve, kt->sleeve, 0.058f, 0.060f, 0, 0.74f},
      {v3_add(P->sho[i], v3_scale(ua, 0.30f)), kt->sleeve, kt->sleeve, 0.056f, 0.062f, 0, 0.0f},
      {v3_add(P->sho[i], v3_scale(ua, 0.68f)), kt->sleeve, kt->sleeve, 0.049f, 0.052f, 0, 0.0f},
      {P->elb[i], kt->sleeve, kt->sleeve, 0.044f, 0.046f, 0, 0.0f}};   // elbow
    // First bone runs chest-outward, so `spine` is the stable roll reference.
    // Round, k=16, smooth: the sleeve follows the torso out of the crate
    // language. The node radii already taper deltoid -> bicep -> elbow.
    probe("uarm");
    fig_chain(ar, 4, 16, 1, 1, spine);
    // The FOREARM is built after the hand, further down, because it needs the
    // palm's own axis — see fig_forearm. It used to be three plain nodes of
    // tapered sleeve here while the viewmodel had nine with a gauntlet; the
    // player's two views of his own arm were two different objects.
    {  // the elbow, and the pad on it, are one ball
      v3 hinge = v3_cross(ua, fa);
      if (v3_dot(hinge, hinge) < 1e-8f) hinge = spine;
      mat_set(MAT_RUBBER_R, 0.0f);
      probe("elbow");
      // The knee pad sits on `pants` and the elbow pad on `sleeve`, which is a
      // value step lighter — so one shared rubber puts the elbow further from
      // its host than the knee is from its own: measured, elbow/uarm 0.332 of
      // mean linear luminance against knee/thigh 0.464. Derive it from the arm
      // it is worn on instead, at the knee's own ratio.
      // v5: the joint is the SLEEVE's elbow (0.92, the crease in its own
      // shadow), not a 0.68 rubber ball — that dark sphere on the roundest
      // point of the arm was the action-figure joint the review saw. The PAD is
      // a separate moulded shell on the point of the elbow, below.
      fig_joint(P->elb[i], ua, v3_sub(P->wri[i], P->elb[i]),
                0.051f, 0.060f, 0.16f, 16, v3_scale(kt->sleeve, 0.92f));
      // THE ELBOW PAD (operators, close tiers): a soft-box shell 30 x 12 mm on
      // the OUTSIDE of the bend, its long axis along the bisector, rooted 6 mm
      // into the joint so it crosses decisively. Away from the fold; a straight
      // arm uses the IK pole's direction (down and out), which is where an
      // elbow points when it is not bent.
      if (!kt->ins && g_fig_k > 12) {
        v3 dua = v3_norm(ua), dfa = v3_norm(fa);
        v3 bis = v3_add(dua, dfa);
        bis = v3_dot(bis, bis) > 1e-6f ? v3_norm(bis) : dua;
        v3 out = v3_sub(dua, dfa);
        if (v3_dot(out, out) < 4e-3f) out = v3_add(v3_scale(Rc, s * 0.7f), v3_scale(spine, -0.8f));
        out = v3_sub(out, v3_scale(bis, v3_dot(out, bis)));
        out = v3_dot(out, out) > 1e-8f ? v3_norm(out) : v3_scale(spine, -1.0f);
        v3 lat = v3_norm(v3_cross(bis, out));
        v3 epc = v3_scale(kt->pad, 1.10f);
        v3 eb = v3_add(P->elb[i], v3_scale(out, 0.051f));
        fnode_t ep[3] = {
          {v3_add(eb, v3_scale(bis, -0.034f)), epc, epc, 0.024f, 0.010f, 0, 0.0f},
          {eb, epc, epc, 0.031f, 0.013f, 0, 0.0f},
          {v3_add(eb, v3_scale(bis, 0.034f)), epc, epc, 0.024f, 0.010f, 0, 0.0f}};
        probe("elbowpad");
        mat_set(MAT_RUBBER_R, 0.0f);
        g_fig_soft = 1;
        fig_chain(ep, 3, 16, 1, 1, lat);
        g_fig_soft = 0;
      }
      mat_set(MAT_CLOTH_R, 0.0f);
    }
    {  // The deltoid: a BALL, not a capped tube along the arm. As a chain its first
      // ring carried a flat end cap facing up and inboard, and that cap was plainly
      // visible from outside — a dark ellipse on the end of a cylinder, which is why
      // the shoulders read as barrels bolted on instead of armour over a joint. A
      // ball has no cap, is round from every angle, swallows the arm's own end cap,
      // and reaches inboard far enough (inner edge 0.106 from the spine, the yoke
      // reaches 0.156) to intersect the yoke, so there is no seam to close either.
      // >= 6 mm larger than the arm ring it swallows (0.067/0.069) in EVERY
      // direction, or the two surfaces come out tangent somewhere and z-fight.
      // Halved, and recoloured to the SLEEVE. It was an 84 mm coyote-brown
      // ellipsoid on a 380 mm frame — the figure's shoulder breadth was made out
      // of pads rather than bone: 548 mm overall against ANSUR's 509 bideltoid,
      // with the joints themselves below the 5th percentile. Moving 20 mm into
      // ANIM_SHO_HALF and taking 18 off the ball puts the mass where a deltoid
      // has it. The hue break mattered as much as the size: a hard coyote/olive
      // step landed exactly on the roundest surface on the body, which is what
      // made it read as a football pad. The armour above is still coyote, so it
      // is now a shell ON a shoulder instead of BEING the shoulder.
      mat_set(MAT_CLOTH_R, 0.0f);
      probe("sho_ball");
      // The shoulder's "parent bone" is the shoulder line itself — there is no
      // clavicle in this skeleton — so the deltoid's long axis comes out of the
      // line from the chest to the joint against the upper arm, which is where
      // a deltoid's mass actually lies.
      // Value, not just hue. Recolouring the ball from coyote to the sleeve
      // removed the hue break but left it LIGHTER than the sleeve band beside it
      // and the collar above it, so the eye landed on a pale sphere crowning a
      // brown pad — an action-figure ball joint. A shoulder is the top of a limb
      // and sits in its own occlusion: same cloth, 12% down in value.
      // TUCKED: the ball's centre sits 6 mm in toward the chest and 3 mm up the
      // spine from the joint (the joint itself does not move — this is where the
      // MESH is centred, not where the arm pivots), so the cap reads as the top
      // of the arm growing out of the clavicle line instead of a sphere bolted on
      // its side. The arm's first ring is a 16-gon now (circumradius 0.060
      // against the octagon's 0.065), which is exactly the margin the tuck
      // spends: the arm's end cap stays buried by the same >= 6 mm as before.
      v3 tuck = v3_sub(P->chest, P->sho[i]);
      tuck = v3_sub(tuck, v3_scale(spine, v3_dot(tuck, spine)));
      tuck = v3_dot(tuck, tuck) > 1e-8f ? v3_norm(tuck) : v3_scale(Rc, -s);
      v3 sc = v3_add(P->sho[i], v3_add(v3_scale(tuck, 0.006f), v3_scale(spine, 0.003f)));
      // v5: 70 mm along the limb (was 78) and the sleeve's own value — the
      // 0.88 step put a darker sphere at the top of every arm.
      fig_joint(sc, v3_sub(P->sho[i], P->chest), ua,
                0.066f, 0.070f, 0.12f, 16, kt->sleeve);
    }
    {  // Deltoid armour: a short pad riding the outside of the upper arm, strictly
      // outside it at every station (the arm runs 0.067 -> 0.057 there, this runs
      // 0.072 -> 0.069). Widening the silhouette at the SHOULDER is the strongest
      // "loaded" cue available, because the shoulder is where the eye reads body
      // mass — and it costs nothing in the skeleton or the hitbox.
      v3 uad = v3_norm(ua);
      mat_set(MAT_GEAR_R, 0.0f);
      // First ring 6.5 mm INSIDE the shoulder ball, second 6 mm outside it, so
      // the pad emerges through a decisive crossing rather than a near-tangent
      // one — a shallow crossing is a hairline seam whose depth order is decided
      // by rounding, and it is the one defect figcheck cannot see.
      // SLEEVE family, one step down — coyote here read as bare flesh at 20 m
      // (vest over a T-shirt), and a hue break on a curved mass's equator reads
      // as a ball-joint seam. Colour boundaries belong on geometric lines.
      v3 dcol = v3_scale(kt->sleeve, 0.94f);
      v3 strap_col = kt->arm;   // the friendly armband (insurgent: plain webbing)
      // Past 22 m the arm is a 6-gon whose corners stand out to its
      // circumradius: the pad goes 7% wider there or the arm's corners poke
      // through its flats along the whole band.
      float dk = g_fig_k <= 6 ? 1.07f : 1.0f;
      fnode_t sp[5] = {
        // Re-spaced for the elongated joint below: -0.035/+0.015 was authored
        // against a SPHERE, and against a mass that runs 84 mm down the arm the
        // pad emerged through a long shallow stretch of it instead of through a
        // decisive crossing — six grazing pairs on this join alone. Buried
        // deeper and emerging further out is the same rule the comment above
        // states, applied to the shape that is actually there now.
        // v5: the pad lies ON the arm (68-70 mm against the sleeve's 58-62,
        // was 76-78: it stood 16 mm off the deltoid and read as a separate
        // object), and the ARMBAND is 22 mm wide, not 37.
        {v3_add(P->sho[i], v3_scale(uad, -0.058f)), dcol, dcol, 0.044f * dk, 0.046f * dk, 0, 0.66f},
        {v3_add(P->sho[i], v3_scale(uad,  0.034f)), dcol, dcol, 0.069f * dk, 0.071f * dk, 0, 0.0f},
        {v3_add(P->sho[i], v3_scale(uad,  0.066f)), dcol, strap_col, 0.068f * dk, 0.070f * dk, 0, 0.0f},
        {v3_add(P->sho[i], v3_scale(uad,  0.088f)), strap_col, dcol, 0.066f * dk, 0.069f * dk, 0, 0.0f},
        {v3_add(P->sho[i], v3_scale(uad,  0.118f)), dcol, dcol, 0.055f * dk, 0.058f * dk, 0, 0.0f}};
      probe("deltoid");
      fig_chain(sp, 5, 16, 1, 1, spine);
      // v5: NO PAULDRON. A hard shell in the helmet's colour crowning each
      // shoulder 68 mm off the joint was what made the shoulders too wide and
      // read as armour bolted onto a mannequin; a plate carrier has none.
      // The carrier's shoulder straps carry pads instead (fig_torso).
      mat_set(MAT_CLOTH_R, 0.0f);
    }
    // Hand: the SAME wrap the viewmodel uses, closed round the SAME published
    // grip cylinder of the SAME rifle. The wrist stays where the two-bone IK put
    // it — that is the arm's business — and only the grip surface comes from the
    // weapon, so a figure whose reach is a little short gets a stretched palm
    // rather than a hand detached from its rifle.
    mat_set(MAT_GLOVE_R, 0.0f);
    probe("hand");
    {
      v3 hc, ha; float hr;
      if (i == 0 && wpn == WPN_PT && g_fig_dual)   // the left Beretta's grip
        gun_hold(wpn, GH_GRIP, v3_sub(g_org, v3_scale(g_rt, DUAL3P * GUN_GIRTH_3P)),
                 g_rt, g_up, g_fw, &hc, &ha, &hr);
      else
        gun_hold(wpn, i ? GH_GRIP : GH_FORE3, g_org, g_rt, g_up, g_fw, &hc, &ha, &hr);
      if ((i == 0 && (wpn == WPN_KN || wpn == WPN_GR)) || (g_fig_free >> i & 1)) {
        // the free hand closes on nothing — round an axis ACROSS the forearm.
        // (It used to close round the gun's right axis, which in the middle of
        // a weapon swap can lie along the forearm: the fingers then wrapped
        // round nothing in line with the arm and stood out as a claw.)
        v3 fdir = v3_norm(v3_sub(P->wri[i], P->elb[i]));
        v3 ax = v3_cross(fdir, spine);
        if (v3_dot(ax, ax) < 1e-4f) ax = v3_cross(fdir, Rc);
        ha = v3_norm(ax);
        hr = 0.013f;
        hc = v3_add(P->wri[i], v3_scale(fdir, 0.075f));
      }
      if (g_fig_grab >> i & 1) { hc = g_fig_grab_c[i]; ha = g_fig_grab_a[i]; hr = g_fig_grab_r[i]; }
      if (i == 0 && brace_k > 0.5f) {
        // Slide brace: a KNUCKLE-DRAG FIST. The fat-cylinder palm-flat
        // attempt wrapped the fingers ~90 deg around a deeply buried axis —
        // measured, the digits reached 0.16 m below the wrist and sank
        // through the floor. A fist round a small lateral axis keeps every
        // digit within ~3 cm of the wrist, so the knuckles kiss the ground
        // the wrist height in skel_solve puts them at, and drag there.
        v3 lat = v3_cross(brace_dir, (v3){{0, 1, 0}});
        ha = v3_dot(lat, lat) > 1e-6f ? v3_norm(lat) : Rc;
        hr = 0.013f;
        hc = v3_add(P->wri[i], v3_scale((v3){{0, 1, 0}}, -0.040f));
      }
      // The trigger hand gets the weapon's published blade, exactly as the
      // viewmodel does — the third-person body is the SAME man holding the SAME
      // rifle, and a finger that only finds the trigger in first person is the
      // divergence this whole pass exists to remove. (The slide-brace hand is
      // i == 0 and never receives the blade, so no brace guard is needed here.)
      // ...unless that hand is not on the grip at all (a weapon swap: closed
      // on nothing, or round the stock). Its trigger finger still reached for
      // the trigger then, wherever the rifle was — a 20 cm finger hanging off
      // a fist behind his neck.
      int off_grip = (g_fig_free | g_fig_grab) >> i & 1;
      const v3 *tp = i == 1 && !off_grip ? &g_gun_trig_p : NULL;
      v3 pd;
      fig_hand(P->wri[i], hc, ha, hr, s, kt->glove, i == 1 && !off_grip ? 1 : 2, tp, &pd);
      // ...and the SAME forearm the viewmodel builds, off the same palm axis.
      // The wrist rotations are the viewmodel's business — there the arm is free
      // and the hold decides where it goes — while here the elbow is already
      // solved by two-bone IK from the shoulder, so `back` is simply elbow-ward
      // and the chain is scaled to land on it.
      mat_set(MAT_CLOTH_R, 0.0f);
      probe("farm");
      {
        v3 back = v3_sub(P->elb[i], P->wri[i]);
        back = v3_dot(back, back) > 1e-8f ? v3_norm(back) : v3_scale(pd, -1.0f);
        v3 lat = v3_cross(back, spine);
        lat = v3_dot(lat, lat) > 1e-8f ? v3_norm(lat) : Rc;
        fig_forearm(P->wri[i], back, pd, lat, s, kt->sleeve,
                    kt->gnt, &P->elb[i], 16);
      }
    }
    mat_set(MAT_CLOTH_R, 0.0f);
  }
}

