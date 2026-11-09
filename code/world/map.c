// code/world/map.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// (Netcode wire layer moved below player_t — it needs PITCH_MAX/IN_COUNT.)

// ---------------------------------------------------------------------------
// Procedural map — seeded arena generation. AABB solids are the collision set
// AND the render mesh; themes color them.
// ---------------------------------------------------------------------------

// THE ARENA'S HALF-SIZE IS A PROPERTY OF THE MAP. 24 m (a 48 m square) for
// ARENA and the open handmade maps; the large tactical maps set their own in
// MAP_DEFS, and map_generate publishes it here per lobby before it lays a
// single box. Read through the accessor so a lobby that has not generated yet
// still answers the historical 24 rather than 0.
#define ARENA_HALF_DEF 24.0f
static float g_arena_half_all[SRV_LOBBIES];
static inline float arena_half(void) {
  float h = g_arena_half_all[g_lobby];
  return h > 0.0f ? h : ARENA_HALF_DEF;
}
#define ARENA_HALF (arena_half())
// STEP-UP, also per map. 0 on every map that existed before it (the collision
// has never auto-climbed, and their proofs and pacing are measured that way —
// see wall_rubble); the tactical maps have stairs, and a stair you must JUMP
// every 30 cm of is not a stair. A body walking into a box whose top is within
// this height of its feet, with headroom above it, is lifted onto it.
static float g_step_h_all[SRV_LOBBIES];
#define STEP_H (g_step_h_all[g_lobby])
// Raised from 96 with the wall-damage pass: a broken wall is up to four
// abutting pieces plus three rubble blocks where it used to be one box, and all
// of them are real cover that has to be in the collision set. int16 carries the
// solid index on the wire (net_ev_from_live) and the world hash is over the
// USED entries, so the ceiling itself is free — what it costs is 16 lobbies x
// 32 x 40 bytes of server memory.
#define MAX_SOLIDS   8192  // 160 suited the 48 m arena; a dressed 104 m town needs thousands

// `pen` = a round may cross this box if its budget stretches that far. Cover and
// crates are shootable-through material; the arena SHELL is not cover, it is the
// edge of the world, and a round leaving through it would fly forever.
typedef struct { v3 min, max, color; int pen; } solid_t;

static solid_t g_solids_all[SRV_LOBBIES][MAX_SOLIDS];
#define g_solids (g_solids_all[g_lobby])
static int     g_num_solids_all[SRV_LOBBIES];
#define g_num_solids (g_num_solids_all[g_lobby])

// LADDERS: a climbable strip on a wall face. The rails and rungs are ordinary
// solids (thin, flush on the wall); what makes it a ladder is this record,
// which the shared move code (player_move_tick) reads, so a client predicting
// a climb and the server simulating it run the same arithmetic on the same
// map-generated data. (x, z) is the foot on the wall face, (nx, nz) the face's
// outward normal, top the height you climb to (the parapet's crown).
#define MAX_LADDERS 16
typedef struct { float x, z, nx, nz, top; } ladder_t;
static ladder_t g_ladders_all[SRV_LOBBIES][MAX_LADDERS];
#define g_ladders (g_ladders_all[g_lobby])
static int      g_nladders_all[SRV_LOBBIES];
#define g_nladders (g_nladders_all[g_lobby])

// Every match start regenerates the arena from the next seed in a
// deterministic chain (harness runs stay reproducible from --seed; the
// Windows build seeds the chain from startup time). The renderer re-uploads
// the world VBO when flagged dirty.
static uint32_t g_map_seed_all[SRV_LOBBIES];
#define g_map_seed (g_map_seed_all[g_lobby])
static int      g_world_dirty_all[SRV_LOBBIES];
#define g_world_dirty (g_world_dirty_all[g_lobby])
// WHICH MAP. 0 is the procedural ARENA (a fresh layout per seed); 1.. are the
// handmade maps in MAP_DEFS. It is half of the world's identity — the seed
// still drives the tints, the wind and every RNG stream — so it travels with
// the seed everywhere the seed travels (ACCEPT, every snapshot) and both are
// adopted together. g_map_pick is the MAP setting that chose it (a map id, or
// MAP_ROTATE), latched at the last (re)start.
static int      g_map_id_all[SRV_LOBBIES];
#define g_map_id (g_map_id_all[g_lobby])
static int      g_map_pick_all[SRV_LOBBIES];
#define g_map_pick (g_map_pick_all[g_lobby])

static const v3 SPAWN_POS = {{0.0f, 0.0f, 18.0f}};

// --- SURFACE MATERIALS ----------------------------------------------------
// What a box is MADE of, for the world shader: brick courses, lime render,
// form-cast concrete, corrugated sheet. It is purely cosmetic, so it lives in
// a PARALLEL array rather than in solid_t: the solids are hashed byte for byte
// into the network world check (net_world_hash), and a renderer-only property
// has no business deciding whether a client may join.
//
// Builders do not pass it. A box's material is decided by its COLOUR, matched
// against a small table each map registers next to its palette: every palette
// colour reaches the solids either verbatim or through col_tint, which is a
// uniform scale, so "same chromaticity, brightness within the tint range" is
// an exact test for "built from that palette entry" with no builder threading
// a parameter through. g_mat_force overrides it for the few builders whose
// colour says nothing about the material (a container is any colour at all).
enum { SMAT_PAINT = 0, SMAT_STUCCO, SMAT_BRICK, SMAT_CONCRETE, SMAT_CLAD,
       SMAT_CONTAINER, SMAT_STONE, SMAT_WOOD,
       SMAT_COPPER,    // oxidised copper sheet: the mosque dome's verdigris
       SMAT_STEEL,     // welded steel plate and pipe: ring seams, brushed
       SMAT_HAZARD,    // safety-yellow paint over steel, chipped and striped
       SMAT_GLASS,     // a building's window: reflective glass, a room behind it
       SMAT_VGLASS,    // a vehicle's glass: tinted, a dark cabin behind it
       SMAT_LAMP,      // a lantern: lit at dusk and dawn
       SMAT_LED,       // a dot-matrix sign: always lit
       SMAT_COUNT };
// Steel answers a round with sparks, whatever its finish.
static int smat_metal(int m) {
  return m == SMAT_CLAD || m == SMAT_CONTAINER || m == SMAT_COPPER ||
         m == SMAT_STEEL || m == SMAT_HAZARD;
}
static uint8_t g_solid_mat_all[SRV_LOBBIES][MAX_SOLIDS];
#define g_solid_mat (g_solid_mat_all[g_lobby])
static int g_mat_force = -1;
static struct { v3 c; uint8_t m; } g_mat_tab[48];
static int g_mat_tab_n;
static void smat_reset(void) { g_mat_tab_n = 0; g_mat_force = -1; }
static void smat_reg(v3 c, int m) {
  if (g_mat_tab_n < (int)(sizeof g_mat_tab / sizeof g_mat_tab[0]))
    g_mat_tab[g_mat_tab_n++] = (typeof(g_mat_tab[0])){c, (uint8_t)m};
}
static int smat_classify(v3 c) {
  int best = SMAT_PAINT;
  float best_e = 1e9f;
  for (int i = 0; i < g_mat_tab_n; i++) {
    v3 k = g_mat_tab[i].c;
    float kk = k.x * k.x + k.y * k.y + k.z * k.z;
    if (kk < 1e-6f) continue;
    float r = (c.x * k.x + c.y * k.y + c.z * k.z) / kk;
    if (r < 0.55f || r > 1.12f) continue;      // the widest tint any builder uses
    float ex = c.x - r * k.x, ey = c.y - r * k.y, ez = c.z - r * k.z;
    float e = ex * ex + ey * ey + ez * ez;
    if (e < 2.5e-4f && e < best_e) { best_e = e; best = g_mat_tab[i].m; }
  }
  return best;
}

// RENDER PROPS. A vehicle's solids are its COLLISION — a block-out of boxes
// that the sim, the bots and the netcode all agree on and that must not move.
// What the player SEES is a detailed model built in the same footprint
// (mesh_props): the boxes are flagged hidden for the colour mesh, and the
// prop registry tells the renderer what to build where. Nothing here draws
// from g_rng, so registering a prop changes no map.
static uint8_t g_solid_hide_all[SRV_LOBBIES][MAX_SOLIDS];
#define g_solid_hide (g_solid_hide_all[g_lobby])
enum { PROP_TRUCK = 1, PROP_CAR, PROP_LAMP, PROP_HELI, PROP_PALM, PROP_BARREL };
typedef struct { int kind, ax, dir; float x, z; v3 col, col2; } prop_t;
#define MAX_PROPS 192
static prop_t g_props_all[SRV_LOBBIES][MAX_PROPS];
static int g_nprops_all[SRV_LOBBIES];
#define g_props (g_props_all[g_lobby])
#define g_nprops (g_nprops_all[g_lobby])
static void prop_add(int kind, float x, float z, int ax, int dir, v3 c, v3 c2) {
  if (g_nprops < MAX_PROPS) g_props[g_nprops++] = (prop_t){kind, ax, dir, x, z, c, c2};
}
static void solids_hide(int from) {
  for (int i = from; i < g_num_solids; i++) g_solid_hide[i] = 1;
}
static void map_add(v3 mn, v3 mx, v3 col) {
  if (g_num_solids >= MAX_SOLIDS) return;
  g_solid_hide[g_num_solids] = 0;
  g_solid_mat[g_num_solids] = (uint8_t)(g_mat_force >= 0 ? g_mat_force : smat_classify(col));
  g_solids[g_num_solids++] = (solid_t){mn, mx, col, 1};
}

