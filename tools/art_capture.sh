#!/bin/sh
# Fixed visual test set: the same scenes from the same cameras, every time.
#
#   tools/art_capture.sh <binary> <out-dir> [W H]
#
# Each point is its own headless run on a fresh default config and a fixed
# seed, so two builds can be compared image by image (tools/art_compare.py).
# ONLY=<regex> in the environment films only the points whose name matches.
# Writes <out-dir>/NN_name.png plus <out-dir>/budget.txt (scene vertices per
# point: the renderer's own load figure; GPU frame time needs real hardware).
set -eu
BIN="$(realpath "${1:?usage: art_capture.sh <binary> <out-dir> [W H]}")"
OUT="${2:?usage: art_capture.sh <binary> <out-dir> [W H]}"
W="${3:-1280}"; H="${4:-720}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
OUT="$(realpath "$OUT")"
[ -n "${ONLY:-}" ] || : > "$OUT/budget.txt"   # a filtered run appends

shoot() {   # name, script (shot path is appended)
  # ONLY=<regex> films just the matching points (iteration); unset films all.
  if [ -n "${ONLY:-}" ] && ! printf '%s' "$1" | grep -Eq "$ONLY"; then return 0; fi
  cfg="$(mktemp -u)"
  printf 'hints 0\n' > "$cfg"
  ( cd "$ROOT" && "$BIN" --seed 1337 --w "$W" --h "$H" --config "$cfg" \
      --do "$2; shot $OUT/$1.png; budget" ) 2>&1 |
    sed -n "s/^budget scene_verts=\([0-9]*\).*world_verts=\([0-9]*\).*world_decor=\([0-9]*\).*/$1 scene=\1 world=\2 decor=\3/p" >> "$OUT/budget.txt"
  rm -f "$cfg"
}

FP="hud off; bots 3; botfreeze on; restart; wait 60"
shoot 01_menu_home     "home on; home 8; wait 30"
shoot 02_fp_rifle      "$FP; weapon ar; wait 60"
shoot 03_fp_sniper     "$FP; weapon sr; wait 60"
shoot 04_fp_pistol     "$FP; weapon pt; wait 60"
shoot 05_fp_knife      "$FP; weapon kn; +ads; wait 60"
shoot 06_fp_grenade    "$FP; weapon gr; wait 60"
# Map views stand above the player's own spawn, facing the centre the way the
# spawn itself does. `cam` takes RADIANS: yaw = atan2(-x, z), positive pitch up.
shoot 07_ext_bazaar    "hud off; usemap bazaar; restart; wait 30; botfreeze on; cam 12.27 4 12.15 -0.79 -0.17"
shoot 08_ext_citadel   "hud off; usemap citadel; restart; wait 30; botfreeze on; cam 21.02 5 36.57 -0.52 -0.14"
shoot 09_int_foundry   "hud off; usemap foundry; restart; wait 30; botfreeze on; cam 20.71 1.7 -5.43 -1.83 0"
shoot 10_harbor_wide   "hud off; usemap harbor; restart; wait 30; botfreeze on; cam -29.48 6 2.73 1.48 -0.14"
shoot 11_refinery      "hud off; usemap refinery; restart; wait 30; botfreeze on; cam -29.48 3 2.73 1.48 -0.07"
shoot 12_combat        "hud off; bots 8; skill hard; restart; wait 900; cam 3 4 20 0 -0.14"
# One bot, puppeted to a known spot, filmed at 3 m and at 40 m.
shoot 13_char_close    "hud off; bots 1; restart; wait 30; puppet on; puppet warp 0 -20; puppet face 180; wait 60; cam 0 1.5 -17 0 -0.05"
shoot 14_char_far      "hud off; bots 1; restart; wait 30; puppet on; puppet warp 8 -20; puppet face 180; wait 60; cam 8 1.7 20 0 0"
shoot 15_hud_combat    "bots 3; skill hard; restart; wait 240; aimbot; +fire; wait 12"
shoot 16_death         "bots 1; botfreeze on; restart; wait 60; hurt 100; wait 30"
shoot 19_damage        "bots 1; botfreeze on; restart; wait 60; hurt 60; wait 20"
shoot 17_menu_settings "menu; nav down; uiframe; nav down; uiframe; nav down; uiframe; nav down; uiframe; nav ok; uiframe; uiframe"
shoot 18_menu_armory   "menu; nav down; uiframe; nav down; uiframe; nav down; uiframe; nav ok; uiframe; uiframe"
# Round 2 (2026-11-12): the points the first set did not cover.
# A frag thrown down the open lane, filmed from the thrower's eyes: the flash
# frame (2 ticks after detonation) and the smoke 26 ticks later.
NADE="hud off; bots 1; botfreeze on; restart; wait 60; look 2700 0; weapon gr; wait 60; +fire; wait 50; -fire"
shoot 20_blast_flash   "$NADE; wait 427"
shoot 21_blast_smoke   "$NADE; wait 451"
shoot 22_rescue        "hud off; mode rescue; restart; wait 120"
shoot 23_citadel_street "hud off; usemap citadel; restart; wait 30; botfreeze on; cam 0 10 45 0 -0.18"
shoot 24_citadel_alley  "hud off; usemap citadel; restart; wait 30; botfreeze on; cam 0 3 20 0 0"
shoot 25_hud_full      "bots 3; botfreeze on; restart; wait 120"
# Round 3 (2026-11-13): the two forces. An operator (figop 1: bot 0 in the
# friendly kit) beside the four insurgent archetypes (rifleman, gunner, scout,
# leader), close; the same row in flat black at ~18 m (shape alone must name
# the side); the row at 25 m on the four map families (an insurgent must read
# against sand, stone, yard concrete and fogged brick); a team game; and the
# hostage, who must read as neither force.
LINE="hud off; bots 5; restart; wait 30; botfreeze on; figop 1"
shoot 26_lineup        "usemap bazaar; $LINE; lineup 0.5 -20 1.2 150; wait 60; cam 2.9 1.3 -16.2 0 -0.05"
shoot 27_silhouette    "usemap bazaar; $LINE; lineup 0.5 -20 1.2 150; wait 60; figsil on; cam 2.9 1.5 -2 0 -0.01"
# (Bazaar's stalls block every 25 m line onto open ground: its point is the 18 m
# view the silhouette uses, between the canopy posts.)
shoot 28_ins18_bazaar  "usemap bazaar; $LINE; lineup 0.5 -20 1.2 150; warp -14 0 14; wait 60; cam 2.9 1.5 -2 0 -0.01"
shoot 29_ins25_citadel "usemap citadel; $LINE; lineup -2.4 0 1.2 180; wait 60; cam 0 1.7 25 0 -0.02"
shoot 30_ins25_foundry "usemap foundry; $LINE; lineup -2.4 -10 1.2 180; wait 60; cam 0 1.7 14 0 -0.02"
shoot 31_ins25_harbor  "usemap harbor; $LINE; lineup -2.4 0 1.2 180; wait 60; cam 0 1.7 25 0 -0.02"
shoot 32_tdm           "mode tdm; bots 7; skill hard; restart; wait 600"
shoot 33_hostage       "hud off; mode rescue; restart; wait 120; botfreeze on; cam -24.5 1.6 -21 -0.62 -0.15"
# The smoke grenade (2026-11-13): a cloud on open sand at full bloom, and the
# same cloud from inside it.
SMK="usemap bazaar; bots 3; restart; wait 30; botfreeze on; smoke 2 0 -14; wait 600"
shoot 34_smoke         "hud off; $SMK; cam 2 1.6 -4 0 -0.05"
shoot 35_smoke_inside  "$SMK; warp 2 0 -12.5; aim 2 1.4 -20; wait 3"
# Round 5 (2026-11-14): sky visibility. The operator and the four insurgent
# archetypes under the foundry shed roof (~6-8 m): a covered floor must not
# swallow a dark tunic, the read the sky-occlusion floor (SV_MIN) protects.
shoot 36_ins_shade_foundry "usemap foundry; $LINE; lineup 14.2 -4 1.2 180; wait 60; cam 16.6 1.6 -9 3.14159 -0.05"
# The same frag 0.58 s on (point 21 is the fireball at 0.22 s): the dust and
# soot plume, which is the part of a blast a player reads at range.
shoot 37_blast_plume   "$NADE; wait 520"
echo "captured $(ls "$OUT"/*.png | wc -l) points into $OUT"
