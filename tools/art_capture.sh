#!/bin/sh
# Fixed visual test set: the same scenes from the same cameras, every time.
#
#   tools/art_capture.sh <binary> <out-dir> [W H]
#
# Each point is its own headless run on a fresh default config and a fixed
# seed, so two builds can be compared image by image (tools/art_compare.py).
# Writes <out-dir>/NN_name.png plus <out-dir>/budget.txt (scene vertices per
# point: the renderer's own load figure; GPU frame time needs real hardware).
set -eu
BIN="$(realpath "${1:?usage: art_capture.sh <binary> <out-dir> [W H]}")"
OUT="${2:?usage: art_capture.sh <binary> <out-dir> [W H]}"
W="${3:-1280}"; H="${4:-720}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
OUT="$(realpath "$OUT")"
: > "$OUT/budget.txt"

shoot() {   # name, script (shot path is appended)
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
echo "captured $(ls "$OUT"/*.png | wc -l) points into $OUT"
