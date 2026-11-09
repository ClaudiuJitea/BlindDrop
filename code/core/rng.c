// code/core/rng.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// PCG32 — deterministic map generation from a seed.
// ---------------------------------------------------------------------------

// Independent streams so gunplay and the bot never disturb map generation —
// or each other: movement traces stay byte-identical to pre-gunplay builds,
// and spray patterns reproduce regardless of what the bot wandered.
// --- WHICH ARENA THIS TICK BELONGS TO(rebuilt 2026-08-18) ---
// ONE process serves SRV_LOBBIES independent arenas over ONE UDP port. Every
// piece of world state below gains a leading [SRV_LOBBIES] dimension and the
// OLD NAME BECOMES A MACRO onto the current row, so not one of the ~2000 call
// sites changed — which is the whole reason this is a small diff in a 32k-line
// file. Switching arenas is `g_lobby = n`: O(1), no copy, no memcpy of 176 KB.
//
// Singleplayer, the harness and a connected client only ever run lobby 0, and
// every static initializer below is replicated per lobby, so lobby 0 behaves
// byte-for-byte as it did before this existed. The byte gate is what proves it.
//
// What is NOT here: tracers, particles, decals, hit-stop and the damage
// indicator. They are pure presentation — written by the sim, never read back
// by it — so 16 server lobbies sharing them costs nothing observable. The
// ragdoll handover triple (g_player_rag/g_player_corpse_q/g_corpses) IS here:
// ent_damage writes it and player_present_tick consumes it, i.e. it crosses
// the sim boundary.
#define SRV_LOBBIES 16
static int g_lobby;   // 0..SRV_LOBBIES-1; always 0 off the dedicated server

static uint64_t g_rng_all[SRV_LOBBIES];      // map generation
#define g_rng (g_rng_all[g_lobby])
// PER HUMAN (P1-46, sfx_alloc's reseed-from-id idiom): one shared stream is a
// determinism promise that silently dies with the second human — and the
// prediction cannot suppress a draw someone else's shot consumed. [0] is the
// historical stream, byte-identical in singleplayer.
static uint64_t g_rng_gun_all[SRV_LOBBIES][8 /* == MAX_HUMANS, later */];
#define g_rng_gun (g_rng_gun_all[g_lobby])
static uint64_t g_rng_bot_all[SRV_LOBBIES];  // wander targets + respawn spots
#define g_rng_bot (g_rng_bot_all[g_lobby])

static uint32_t rng_next(uint64_t *s) {
  uint64_t old = *s;
  *s = old * 6364136223846793005ULL + 1442695040888963407ULL;
  uint32_t xs = (uint32_t)(((old >> 18) ^ old) >> 27);
  uint32_t rot = (uint32_t)(old >> 59);
  return (xs >> rot) | (xs << ((32 - rot) & 31));
}

static uint32_t rng_u32(void) { return rng_next(&g_rng); }

static float rng_rangef(uint64_t *s, float lo, float hi) {
  return lo + (hi - lo) * ((float)(rng_next(s) >> 8) / 16777216.0f);
}

static float rng_range(float lo, float hi) { return rng_rangef(&g_rng, lo, hi); }

