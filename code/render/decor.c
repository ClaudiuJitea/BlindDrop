// code/render/decor.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ===========================================================================
// DECOR — the world mesh only. Never a solid, never a tick, never a bullet.
// ===========================================================================
// Everything here is ankle height ON PURPOSE. A knee-high visual rock a round
// passes through is a lie you can die to, and a free-standing mass between
// 300 and 800 mm reads as a crouched player at range whether it is one or not.
// So decor is 250 mm or under and it clusters where nobody stands: hard against
// the foot of a wall, in the concave corners, in the lee of the cover. The
// 0.3-1.8 m band is gameplay space and it stays empty.
//
// IT IS BUILT THROUGH THE SCENE PRIMITIVES AND COPIED. A tumbled stone is
// scene_box with a rotated basis, a grass blade is scene_poly3, a sand drift is
// scene_poly4 — all of it exists, and a second copy of that library written
// against g_mesh would be a second copy of a box. `world_upload` is the first
// thing render_frame does and scene_build is one of the last, so the scene
// buffer is free at exactly the moment this runs.
//
// ONE HALF IS GENERATED AND ROTATED. The arena is 180-degree rotationally
// symmetric and its cover is symmetric because symmetric cover is a fair duel;
// decor that is not would be a positional tell — one corner with a landmark the
// other has not got. But MIRRORING is wrong: a mirror flips chirality, and
// every item here has a wind direction in it. So the generator runs over the
// whole arena, keeps only what lands on the +z side, and emits each item twice,
// the second time rotated. Locally organic, globally fair, and the player can
// never see both halves at once.
static uint64_t g_rng_dec;

static float dr(float lo, float hi) { return rng_rangef(&g_rng_dec, lo, hi); }
static v3 dec_rot(v3 p, float sg) { return (v3){{p.x * sg, p.y, p.z * sg}}; }

// A tumbled stone. The TILT is the whole point: everything else in this arena
// is an axis-aligned box, so a few degrees off vertical is the single cue that
// separates "a thing that fell here" from "a thing that was built here". Under
// 60 mm it is sub-pixel past twenty metres and pure vertex cost, so the tiers
// start there.
static void dec_stone(v3 at, float rx, float ry, float rz, float yaw,
                      float tilt, v3 col, float sg) {
  // A STONE HAS NO ARRIS, and g_cham is already the primitive that says so —
  // the same flag the viewmodel turns on for its 0.7 mm broken edges, here at
  // 18% of the minor half-extent because a weathered stone is nearly round at
  // its corners. It costs 132 verts against 36, so only the tier that can
  // actually resolve one pays for it: below ~5 cm the bevel is sub-pixel at any
  // range a player is ever at.
  float mn3 = rx < ry ? (rx < rz ? rx : rz) : (ry < rz ? ry : rz);
  // 0.28 of the minor half-extent, not 0.18: at the smaller number the bevel
  // was a machined arris and every stone still read as a small crate. A stone
  // that has been in weather is nearly round at its corners, and the chamfer is
  // the only thing that says so on a shape that is otherwise a box.
  // The gate is a COST decision as much as a look one: a chamfered box is 132
  // verts against 36, the decor is drawn every frame in the world buffer, and
  // 30k vertices of pebble measured 2.26 ms/frame on llvmpipe. Only the tier
  // that fills more than a pixel or two of screen pays for a broken edge.
  float cham = mn3 > 0.072f ? mn3 * 0.30f : 0.0f;
  float save_cham = g_cham;
  g_cham = cham;
  float cy = cosf(yaw), sy = sinf(yaw), ct = cosf(tilt), st = sinf(tilt);
  v3 X = {{cy, 0.0f, -sy}}, Y = {{0.0f, 1.0f, 0.0f}}, Z = {{sy, 0.0f, cy}};
  v3 Xt = v3_add(v3_scale(X, ct), v3_scale(Y, st));
  v3 Yt = v3_sub(v3_scale(Y, ct), v3_scale(X, st));
  scene_box(dec_rot(at, sg), v3_scale(dec_rot(Xt, sg), rx),
            v3_scale(Yt, ry), v3_scale(dec_rot(Z, sg), rz), col);
  g_cham = save_cham;
}

// A dry grass tuft: five tapered blades from one root. Each blade is emitted
// TWICE with the same normal and opposite winding — a single-sided blade goes
// black as you walk round it, and a real two-sided one costs a whole extra ring
// of vertices for something 20 mm wide. Same normal on both faces also means a
// blade never flickers between two shadings at a grazing angle.
//
// The blades all lean the SAME way, into the arena's one wind. That common lean
// is the cheapest "this is a place with weather" cue there is, it costs nothing,
// and it is what makes a scatter of tufts read as a field rather than as a list.
static void dec_tuft(v3 at, float h, v3 wind, float lean, v3 col, float sg) {
  for (int b = 0; b < 5; b++) {
    float a = (float)b * 1.2566f + dr(-0.44f, 0.44f);
    float bh = h * dr(0.62f, 1.0f);
    float bw = dr(0.010f, 0.017f);
    v3 dir = {{cosf(a), 0.0f, sinf(a)}};
    v3 perp = {{-sinf(a), 0.0f, cosf(a)}};
    // Splay out from the root, then lean downwind. The splay is what gives the
    // tuft a silhouette; the lean is what gives the field a direction.
    v3 root = v3_add(at, v3_scale(dir, dr(0.0f, 0.022f)));
    v3 tip = v3_add(root, (v3){{0.0f, bh, 0.0f}});
    tip = v3_add(tip, v3_scale(dir, bh * dr(0.16f, 0.42f)));
    tip = v3_add(tip, v3_scale(wind, bh * lean));
    v3 b0 = v3_add(root, v3_scale(perp, bw));
    v3 b1 = v3_sub(root, v3_scale(perp, bw));
    // Up-and-outward, so the blade catches the sky the way a standing leaf does
    // rather than going flat-dark like a card lying on the ground.
    v3 n = v3_norm(v3_add((v3){{0.0f, 1.0f, 0.0f}}, v3_scale(perp, 0.35f)));
    v3 tint = v3_scale(col, dr(0.88f, 1.12f));
    v3 r0 = dec_rot(b0, sg), r1 = dec_rot(b1, sg), rt = dec_rot(tip, sg);
    v3 rn = dec_rot(n, sg);
    scene_poly3(r0, r1, rt, rn, tint);
    scene_poly3(r1, r0, rt, rn, tint);
  }
}

// A sand drift banked against a face. IT IS A LENS, NOT A SLAB. The first
// version was one quad from the wall out to a straight line at a fixed
// distance, closed with two vertical end triangles — which from any angle is a
// pale, hard-edged, axis-aligned RECTANGLE lying on the ground, and at the
// wall/floor line it put a 90-degree step into the join as if the wall were
// sunk into a ledge. That is a modelling error, not weather. Both the reach and
// the height now taper to zero at each end on a sine, so the thing has a plan
// outline a wind could have made and no edge anywhere that a straight line
// could be drawn along. Ankle height, so it is scenery a player walks through
// and never scenery a player trusts.
static void dec_drift(v3 a0, v3 a1, v3 o, float len, float hgt, v3 col,
                      float sg) {
  const int N = 7;
  v3 pt = {{0, 0, 0}}, pb = {{0, 0, 0}};
  for (int i = 0; i <= N; i++) {
    float u = (float)i / (float)N;
    float bell = powf(sinf(u * F_PI), 0.62f);
    v3 base = v3_lerp(a0, a1, u);
    v3 top = v3_add(base, (v3){{0.0f, hgt * bell, 0.0f}});
    v3 out = v3_add(base, v3_scale(o, len * bell));
    v3 rt = dec_rot(top, sg), rb = dec_rot(out, sg);
    if (i > 0) {
      v3 n = v3_norm(v3_cross(v3_sub(rb, pt), v3_sub(pb, pt)));
      if (n.y < 0.0f) n = v3_scale(n, -1.0f);
      scene_poly4(pt, rt, rb, pb, n, col);
    }
    pt = rt; pb = rb;
  }
}

// One dressing site: a clump, never a single object. Real ground detail is
// deposited by a process, and a process leaves piles — one parent with a few
// smaller children falling away from it. A uniform scatter has a characteristic
// nearest-neighbour distance and the eye reads that as a lattice with jitter,
// which is the one thing that cannot look natural.
static void dec_clump(v3 at, v3 o, v3 wind, const theme_t *th, v3 host,
                      int grass, float sg) {
  // A stone is the GROUND it broke off, not the cover it sits beside: at 0.42
  // toward the cover tone it came out a saturated red-brown slab on beige sand,
  // which is the loudest thing a 6 cm object can be. Pulled most of the way back
  // to the floor's own hue and then desaturated, so what separates it is value
  // and silhouette rather than colour.
  // ...AND IT IS MADE OF WHAT IT FELL OFF. A grey rock at the foot of a red
  // wall on tan sand is three materials in one square metre and none of them
  // share anything, which is most of why an arena reads as effects stacked on a
  // greybox rather than as a place. A third of the host's own colour is enough:
  // the piece still reads as loose ground material, and it belongs to the thing
  // it is lying against.
  v3 stone = v3_lerp(v3_lerp(th->ground, th->cover, 0.20f), host, 0.34f);
  float sl = stone.x * 0.30f + stone.y * 0.59f + stone.z * 0.11f;
  stone = v3_lerp(stone, (v3){{sl, sl, sl}}, 0.45f);
  stone = v3_scale(stone, dr(0.80f, 0.98f));
  int n = 1 + (int)(rng_next(&g_rng_dec) % 3u);
  // FEWER SITES, BIGGER PILES. Sites spaced along every face at a metre and
  // dressed at half of them gave an evenly spaced ROW of pebbles down the foot
  // of each crate line — the exact lattice-with-jitter read that uniform scatter
  // always produces. A quarter as many sites, each with real mass, reads as
  // debris that collected somewhere.
  float rmax = dr(0.062f, 0.112f);
  for (int i = 0; i < n; i++) {
    // Each child is a fraction of the one before it, thrown a little further
    // out. Strongly skewed small: a uniform size range reads as machine output.
    float f = powf(0.66f, (float)i) * dr(0.85f, 1.15f);
    float r = rmax * f;
    if (r < 0.028f) continue;
    v3 p = v3_add(at, v3_scale(o, dr(-0.05f, 0.34f)));
    v3 side = {{-o.z, 0.0f, o.x}};
    p = v3_add(p, v3_scale(side, dr(-0.36f, 0.36f) * (1.0f + (float)i * 0.5f)));
    p.y = r * dr(0.55f, 0.80f);   // half-buried, never balanced on a point
    // BULK, not a slab. At 0.55-0.85 of its width in height every stone came
    // out a flat plate lying on the sand and read as a plank; a stone that has
    // tumbled to rest is nearly as tall as it is wide.
    dec_stone(p, r * dr(0.85f, 1.35f), r * dr(0.72f, 1.05f), r * dr(0.85f, 1.25f),
              dr(0.0f, 6.283f), dr(-0.20f, 0.20f),
              v3_scale(stone, dr(0.92f, 1.08f)), sg);
  }
  if (!grass) return;
  int g = 1 + (int)(rng_next(&g_rng_dec) % 2u);
  // Dead dry grass, and it must stay UNDER the floor in value: a tuft brighter
  // than the sand it stands in is the brightest thing in the lower third of the
  // frame, which is exactly where a player's eye is looking for a prone figure.
  v3 dry = v3_lerp(th->ground, (v3){{0.34f, 0.31f, 0.17f}}, 0.62f);
  for (int i = 0; i < g; i++) {
    v3 p = v3_add(at, v3_scale(o, dr(0.02f, 0.30f)));
    v3 side = {{-o.z, 0.0f, o.x}};
    p = v3_add(p, v3_scale(side, dr(-0.42f, 0.42f)));
    p.y = 0.0f;
    // Log-uniform biased small: many short tufts, a few tall ones.
    float u = dr(0.0f, 1.0f);
    float h = 0.085f * powf(0.235f / 0.085f, u * u);
    dec_tuft(p, h, wind, dr(0.16f, 0.34f), v3_scale(dry, dr(0.90f, 1.10f)), sg);
  }
}

