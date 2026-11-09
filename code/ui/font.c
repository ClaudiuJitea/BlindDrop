// code/ui/font.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// "HARDLINE" — the UI typeface, procedural like everything else in this
// game. Original angular skeletons drawn for this project (the previous
// Hershey simplex strokes read rounded and playful): every stroke is a
// STRAIGHT segment — bowls are chamfered octagons, A is flat-topped, S is a
// zig — so the face is machined and futuristic rather than humanist. At GL
// init the skeletons are rendered into a single-channel distance atlas: each
// texel stores 1 - d/HF_DMAX where d is the distance to the nearest stroke
// segment in atlas pixels. The HUD shader thresholds that field per VERTEX
// (aMode), so the stroke weight — regular, bold, black — is picked at draw
// time out of ONE atlas in ONE draw call. CAPS-only by design (the brand
// voice); lowercase input folds to capitals, so rows 'a'..'z' are dead.
// Layout: [0]=vertex-pair count (pen-ups included), [1]=advance, then x,y
// pairs (y-up, baseline 0, cap height 21); -1,-1 lifts the pen.
// ---------------------------------------------------------------------------
static const int8_t HF_SIMPLEX[95][40] = {
  /*  32 ' ' */ {0,14},
  /*  33 '!' */ {5,10,5,21,5,7,-1,-1,5,1,5,1},
  /*  34 '"' */ {5,14,4,21,4,15,-1,-1,10,21,10,15},
  /*  35 '#' */ {11,19,6,21,6,0,-1,-1,11,21,11,0,-1,-1,2,14,15,14,-1,-1,2,7,15,7},
  /*  36 '$' */ {15,20,15,17,11,21,5,21,2,18,2,15,5,12,12,10,15,7,15,3,12,0,5,0,2,3,-1,-1,8,24,8,-3},
  /*  37 '%' */ {14,20,2,0,15,21,-1,-1,2,21,6,21,6,16,2,16,2,21,-1,-1,11,5,15,5,15,0,11,0,11,5},
  /*  38 '&' */ {13,21,15,0,5,10,3,12,3,18,6,21,9,21,12,18,12,15,3,7,3,3,6,0,10,0,15,5},
  /*  39 ''' */ {2,8,4,21,4,15},
  /*  40 '(' */ {4,12,10,25,6,20,6,-2,10,-7},
  /*  41 ')' */ {4,12,2,25,6,20,6,-2,2,-7},
  /*  42 '*' */ {8,16,8,20,8,12,-1,-1,4,18,12,14,-1,-1,4,14,12,18},
  /*  43 '+' */ {5,18,8,4,8,16,-1,-1,2,10,14,10},
  /*  44 ',' */ {2,10,5,2,3,-3},
  /*  45 '-' */ {2,16,3,10,13,10},
  /*  46 '.' */ {2,10,5,1,5,1},
  /*  47 '/' */ {2,16,3,-3,13,24},
  /*  48 '0' */ {12,20,2,4,2,17,6,21,11,21,15,17,15,4,11,0,6,0,2,4,-1,-1,7,6,10,15},
  /*  49 '1' */ {6,20,4,17,8,21,8,0,-1,-1,4,0,12,0},
  /*  50 '2' */ {7,20,2,17,6,21,11,21,15,17,15,12,2,0,15,0},
  /*  51 '3' */ {14,20,2,17,6,21,11,21,15,17,15,14,12,11,7,11,-1,-1,12,11,15,8,15,4,11,0,6,0,2,4},
  /*  52 '4' */ {4,20,12,0,12,21,3,6,16,6},
  /*  53 '5' */ {9,20,14,21,3,21,3,13,10,13,14,9,14,4,10,0,5,0,2,3},
  /*  54 '6' */ {11,20,13,21,7,21,2,16,2,4,6,0,11,0,15,4,15,7,11,11,6,11,2,7},
  /*  55 '7' */ {6,20,2,21,15,21,6,0,-1,-1,6,11,13,11},
  /*  56 '8' */ {18,20,6,11,3,14,3,18,6,21,11,21,14,18,14,14,11,11,6,11,-1,-1,6,11,2,7,2,3,5,0,12,0,15,3,15,7,11,11},
  /*  57 '9' */ {12,20,3,4,7,0,11,0,15,4,15,17,11,21,6,21,2,17,2,14,6,10,11,10,15,14},
  /*  58 ':' */ {5,10,5,2,5,2,-1,-1,5,13,5,13},
  /*  59 ';' */ {5,10,5,13,5,13,-1,-1,5,2,3,-3},
  /*  60 '<' */ {3,18,14,17,3,10,14,3},
  /*  61 '=' */ {5,18,2,13,14,13,-1,-1,2,7,14,7},
  /*  62 '>' */ {3,18,3,17,14,10,3,3},
  /*  63 '?' */ {10,18,2,18,5,21,10,21,13,18,13,14,9,10,9,7,-1,-1,9,1,9,1},
  /*  64 '@' */ {16,24,15,4,11,0,6,0,2,4,2,17,6,21,12,21,16,17,16,9,13,6,11,8,11,13,9,15,7,13,7,9,9,7},
  /*  65 'A' */ {7,20,2,0,6,21,11,21,15,0,-1,-1,4,7,13,7},
  /*  66 'B' */ {12,20,2,0,2,21,10,21,13,18,13,14,10,11,2,11,11,11,15,7,15,3,12,0,2,0},
  /*  67 'C' */ {8,20,15,17,11,21,6,21,2,17,2,4,6,0,11,0,15,4},
  /*  68 'D' */ {7,20,2,0,2,21,10,21,15,16,15,5,10,0,2,0},
  /*  69 'E' */ {7,19,15,0,2,0,2,21,15,21,-1,-1,2,11,12,11},
  /*  70 'F' */ {6,18,2,0,2,21,15,21,-1,-1,2,11,12,11},
  /*  71 'G' */ {10,21,15,17,11,21,6,21,2,17,2,4,6,0,11,0,15,4,15,9,9,9},
  /*  72 'H' */ {8,21,2,0,2,21,-1,-1,15,0,15,21,-1,-1,2,11,15,11},
  /*  73 'I' */ {2,9,4,0,4,21},
  /*  74 'J' */ {5,16,12,21,12,3,9,0,5,0,2,3},
  /*  75 'K' */ {8,20,2,0,2,21,-1,-1,15,21,2,9,-1,-1,6,12,15,0},
  /*  76 'L' */ {3,17,2,21,2,0,14,0},
  /*  77 'M' */ {5,24,2,0,2,21,10,7,18,21,18,0},
  /*  78 'N' */ {4,21,2,0,2,21,15,0,15,21},
  /*  79 'O' */ {9,21,2,4,2,17,6,21,11,21,15,17,15,4,11,0,6,0,2,4},
  /*  80 'P' */ {7,19,2,0,2,21,11,21,14,18,14,13,11,10,2,10},
  /*  81 'Q' */ {12,21,2,4,2,17,6,21,11,21,15,17,15,4,11,0,6,0,2,4,-1,-1,9,5,16,-2},
  /*  82 'R' */ {10,20,2,0,2,21,11,21,14,18,14,13,11,10,2,10,-1,-1,8,10,15,0},
  /*  83 'S' */ {12,20,15,17,11,21,5,21,2,18,2,15,5,12,12,10,15,7,15,3,12,0,5,0,2,3},
  /*  84 'T' */ {5,19,2,21,15,21,-1,-1,8,21,8,0},
  /*  85 'U' */ {6,21,2,21,2,4,6,0,11,0,15,4,15,21},
  /*  86 'V' */ {4,20,2,21,7,0,10,0,15,21},
  /*  87 'W' */ {5,24,2,21,6,0,10,13,14,0,18,21},
  /*  88 'X' */ {5,20,2,21,15,0,-1,-1,15,21,2,0},
  /*  89 'Y' */ {6,20,2,21,8,13,8,0,-1,-1,15,21,8,13},
  /*  90 'Z' */ {4,19,2,21,15,21,2,0,15,0},
  /*  91 '[' */ {4,12,9,25,4,25,4,-7,9,-7},
  /*  92 bslash */ {2,16,3,24,13,-3},
  /*  93 ']' */ {4,12,3,25,8,25,8,-7,3,-7},
  /*  94 '^' */ {3,16,4,15,8,21,12,15},
  /*  95 '_' */ {2,18,0,-4,16,-4},
  /*  96 '`' */ {2,8,3,22,6,18},
  /*  97 'a' */ {0,16},  // small caps: glyph_index maps to upper
  /*  98 'b' */ {0,16},  // small caps: glyph_index maps to upper
  /*  99 'c' */ {0,16},  // small caps: glyph_index maps to upper
  /* 100 'd' */ {0,16},  // small caps: glyph_index maps to upper
  /* 101 'e' */ {0,16},  // small caps: glyph_index maps to upper
  /* 102 'f' */ {0,16},  // small caps: glyph_index maps to upper
  /* 103 'g' */ {0,16},  // small caps: glyph_index maps to upper
  /* 104 'h' */ {0,16},  // small caps: glyph_index maps to upper
  /* 105 'i' */ {0,16},  // small caps: glyph_index maps to upper
  /* 106 'j' */ {0,16},  // small caps: glyph_index maps to upper
  /* 107 'k' */ {0,16},  // small caps: glyph_index maps to upper
  /* 108 'l' */ {0,16},  // small caps: glyph_index maps to upper
  /* 109 'm' */ {0,16},  // small caps: glyph_index maps to upper
  /* 110 'n' */ {0,16},  // small caps: glyph_index maps to upper
  /* 111 'o' */ {0,16},  // small caps: glyph_index maps to upper
  /* 112 'p' */ {0,16},  // small caps: glyph_index maps to upper
  /* 113 'q' */ {0,16},  // small caps: glyph_index maps to upper
  /* 114 'r' */ {0,16},  // small caps: glyph_index maps to upper
  /* 115 's' */ {0,16},  // small caps: glyph_index maps to upper
  /* 116 't' */ {0,16},  // small caps: glyph_index maps to upper
  /* 117 'u' */ {0,16},  // small caps: glyph_index maps to upper
  /* 118 'v' */ {0,16},  // small caps: glyph_index maps to upper
  /* 119 'w' */ {0,16},  // small caps: glyph_index maps to upper
  /* 120 'x' */ {0,16},  // small caps: glyph_index maps to upper
  /* 121 'y' */ {0,16},  // small caps: glyph_index maps to upper
  /* 122 'z' */ {0,16},  // small caps: glyph_index maps to upper
  /* 123 '{' */ {7,12,10,25,7,23,7,12,4,9,7,6,7,-5,10,-7},
  /* 124 '|' */ {2,8,4,25,4,-7},
  /* 125 '}' */ {7,12,2,25,5,23,5,12,8,9,5,6,5,-5,2,-7},
  /* 126 '~' */ {4,18,2,8,6,12,10,8,14,12},
};

// Icon strokes in the same format, addressed as string bytes \x01..\x05:
// chevron (list marker), AR and SR (killfeed weapon glyphs), headshot, and a
// middle dot. Same skeleton-distance treatment, so they inherit the stroke
// weight of the text they sit in.
static const int8_t HF_EXTRA[5][32] = {
  /* \x01 chevron  */ {3,14, 4,17,11,10,4,3},
  /* \x02 AR       */ {12,30, 1,7,4,10,28,10,-1,-1,9,10,8,5,-1,-1,14,10,13,4,-1,-1,21,10,21,14},
  /* \x03 SR       */ {15,31, 1,7,4,10,29,10,-1,-1,9,10,8,5,-1,-1,12,13,19,13,-1,-1,13,13,13,10,-1,-1,18,13,18,10},
  /* \x04 headshot */ {14,16,5,10,3,12,3,17,5,19,9,19,11,17,11,12,9,10,5,10,-1,-1,2,4,5,7,11,7,14,4},
  /* \x05 mid dot  */ {2,10, 5,9,5,9},
};

#define HF_ASCII   95
#define HF_ICONS   5
#define HF_GLYPHS  (HF_ASCII + HF_ICONS)   // stroke glyphs
#define HF_SOLID   HF_GLYPHS               // all-255 cell: solid fills
#define HF_SOFT    (HF_GLYPHS + 1)         // radial falloff cell: glow/shadow
#define HF_CELLS   (HF_GLYPHS + 2)
#define HF_COLS    16
#define HF_ROWS    ((HF_CELLS + HF_COLS - 1) / HF_COLS)
#define HF_CELL    72                      // atlas cell size, px
#define HF_TEX_W   (HF_COLS * HF_CELL)
#define HF_TEX_H   (HF_ROWS * HF_CELL)
#define HF_PX      1.6f                    // atlas px per font unit
#define HF_DMAX    10.0f                   // encoded distance range, atlas px
#define HF_BASE    50.0f                   // baseline row inside a cell
#define HF_MARG    10.0f                   // ink origin inset inside a cell
#define HF_CAP     21.0f                   // cap height in font units

// Text weights are SDF thresholds: 1 - stroke_radius_px/HF_DMAX. Lower =
// bolder. UIM_RAW bypasses the field (solid slot, soft glow slot).
#define TXT_REG    0.76f
#define TXT_BOLD   0.66f
#define TXT_BLACK  0.56f
#define UIM_RAW    2.0f

typedef struct {                 // per-glyph metrics, filled by font_bake
  float adv;                     // advance, font units (digits: tabular)
  float qx0, qy0, qx1, qy1;      // quad extents, font units (ink + spill)
  float u0, v0, u1, v1;          // atlas UVs of exactly that quad
} hf_gm_t;
static hf_gm_t g_hfm[HF_GLYPHS];
static float g_hf_solid_u, g_hf_solid_v;   // centre texel of the solid cell
static float g_hf_soft[4];                 // u0 v0 u1 v1 of the soft cell

static const int8_t *hf_glyph_data(int gi) {
  return gi < HF_ASCII ? HF_SIMPLEX[gi] : HF_EXTRA[gi - HF_ASCII];
}

// SQUARE-capped stroke distance: distance to the segment measured in its own
// frame with the cap planes cutting flat at the ends. Thresholded by the
// shader this yields hard, machined stroke terminals and mitred joints —
// the difference between a friendly rounded grotesk and the angular
// technical face this UI wants. Curves (O, S, C) stay curves: they are many
// short segments and the caps vanish into the polyline.
static float hf_seg_dist(float px, float py, float ax, float ay,
                         float bx, float by) {
  float dx = bx - ax, dy = by - ay;
  float l = sqrtf(dx * dx + dy * dy);
  if (l < 1e-6f) {  // a point stroke becomes a square dot
    float ex = fabsf(px - ax), ey = fabsf(py - ay);
    return fmaxf(ex, ey);
  }
  float ux = dx / l, uy = dy / l;
  float lx = (px - ax) * ux + (py - ay) * uy;   // along the stroke
  float ly = (px - ax) * -uy + (py - ay) * ux;  // across it
  float ox = fmaxf(fmaxf(-lx, lx - l), 0.0f);   // outside the cap planes
  // EUCLIDEAN distance with flat cap planes (a Chebyshev max() here warps
  // the field and the bold weights fray): end faces cut flat, only the four
  // cap corners keep a stroke-radius round — machined, not bullnosed.
  return sqrtf(ox * ox + ly * ly);
}

static void font_bake(uint8_t *atlas) {
  memset(atlas, 0, (size_t)HF_TEX_W * HF_TEX_H);
  const float spill = HF_DMAX / HF_PX;   // how far the field reaches, font units
  for (int gi = 0; gi < HF_GLYPHS; gi++) {
    const int8_t *d = hf_glyph_data(gi);
    int n = d[0];
    int cx = (gi % HF_COLS) * HF_CELL, cy = (gi / HF_COLS) * HF_CELL;
    hf_gm_t *m = &g_hfm[gi];
    memset(m, 0, sizeof *m);
    m->adv = (float)d[1];
    if (gi >= 16 && gi <= 25) m->adv = 22.0f;   // tabular digits '0'..'9'
    if (n == 0) continue;                       // space: advance only
    float ix0 = 127, iy0 = 127, ix1 = -127, iy1 = -127;
    for (int i = 0; i < n; i++) {
      float px = d[2 + 2 * i], py = d[3 + 2 * i];
      if (px == -1 && py == -1) continue;
      if (px < ix0) ix0 = px;
      if (px > ix1) ix1 = px;
      if (py < iy0) iy0 = py;
      if (py > iy1) iy1 = py;
    }
    m->qx0 = ix0 - spill; m->qx1 = ix1 + spill;
    m->qy0 = iy0 - spill; m->qy1 = iy1 + spill;
    int ax0 = cx + (int)floorf(HF_MARG + m->qx0 * HF_PX);
    int ax1 = cx + (int)ceilf(HF_MARG + m->qx1 * HF_PX);
    int ay0 = cy + (int)floorf(HF_BASE - m->qy1 * HF_PX);
    int ay1 = cy + (int)ceilf(HF_BASE - m->qy0 * HF_PX);
    if (ax0 < cx) ax0 = cx;
    if (ay0 < cy) ay0 = cy;
    if (ax1 > cx + HF_CELL) ax1 = cx + HF_CELL;
    if (ay1 > cy + HF_CELL) ay1 = cy + HF_CELL;
    for (int y = ay0; y < ay1; y++)
      for (int x = ax0; x < ax1; x++) {
        float fx = ((float)x + 0.5f - (float)cx - HF_MARG) / HF_PX;
        float fy = (HF_BASE - ((float)y + 0.5f - (float)cy)) / HF_PX;
        float dmin = 1e9f;
        float lx = 0, ly = 0;
        int pen = 0;
        for (int i = 0; i < n; i++) {
          float sx = d[2 + 2 * i], sy = d[3 + 2 * i];
          if (sx == -1 && sy == -1) { pen = 0; continue; }
          if (pen) {
            float sd = hf_seg_dist(fx, fy, lx, ly, sx, sy);
            if (sd < dmin) dmin = sd;
          }
          lx = sx; ly = sy; pen = 1;
        }
        float v = 1.0f - dmin * HF_PX / HF_DMAX;
        if (v > 0) atlas[y * HF_TEX_W + x] = (uint8_t)(v * 255.0f + 0.5f);
      }
    m->u0 = ((float)cx + HF_MARG + m->qx0 * HF_PX) / (float)HF_TEX_W;
    m->u1 = ((float)cx + HF_MARG + m->qx1 * HF_PX) / (float)HF_TEX_W;
    m->v0 = ((float)cy + HF_BASE - m->qy1 * HF_PX) / (float)HF_TEX_H;
    m->v1 = ((float)cy + HF_BASE - m->qy0 * HF_PX) / (float)HF_TEX_H;
  }
  {  // solid cell: every UI rect/line/disk samples its centre texel
    int cx = (HF_SOLID % HF_COLS) * HF_CELL, cy = (HF_SOLID / HF_COLS) * HF_CELL;
    for (int y = 0; y < HF_CELL; y++)
      for (int x = 0; x < HF_CELL; x++)
        atlas[(cy + y) * HF_TEX_W + cx + x] = 255;
    g_hf_solid_u = ((float)cx + HF_CELL * 0.5f) / (float)HF_TEX_W;
    g_hf_solid_v = ((float)cy + HF_CELL * 0.5f) / (float)HF_TEX_H;
  }
  {  // soft cell: quadratic radial falloff — one quad = one glow/shadow sprite
    int cx = (HF_SOFT % HF_COLS) * HF_CELL, cy = (HF_SOFT / HF_COLS) * HF_CELL;
    float half = HF_CELL * 0.5f, rmax = half - 1.0f;
    for (int y = 0; y < HF_CELL; y++)
      for (int x = 0; x < HF_CELL; x++) {
        float dx = (float)x + 0.5f - half, dy = (float)y + 0.5f - half;
        float v = 1.0f - sqrtf(dx * dx + dy * dy) / rmax;
        if (v < 0) v = 0;
        atlas[(cy + y) * HF_TEX_W + cx + x] = (uint8_t)(v * v * 255.0f + 0.5f);
      }
    g_hf_soft[0] = ((float)cx + 1.0f) / (float)HF_TEX_W;
    g_hf_soft[1] = ((float)cy + 1.0f) / (float)HF_TEX_H;
    g_hf_soft[2] = ((float)cx + HF_CELL - 1.0f) / (float)HF_TEX_W;
    g_hf_soft[3] = ((float)cy + HF_CELL - 1.0f) / (float)HF_TEX_H;
  }
}

static float g_scope_r, g_scope_cx, g_scope_cy;  // scope disc, derived per frame
// The AR's holographic window, projected per frame like the scope disc: its
// centre and the screen images of its two half-axes, in HUD units. hx/hy == 0
// means "no window this frame".
static float g_holo_cx, g_holo_cy, g_holo_ex[2], g_holo_ey[2];
static int g_holo_kind;   // 0 the EOTech (ring + dot), 1 the compact red dot (dot only)
static int g_hud_visible = 1;  // harness capture switch; menus remain visible
// One map over the whole 48 m arena: 4096 px gives ~1.3 cm texels, which is fine
// enough that a figure self-shadows its own arms.
#define SHADOW_TEX    4096
// Light-space half extent: the whole 48 m arena plus the height smear; on a
// large map the same map follows the camera and spans 64 m each way (2.9 cm
// texels), far enough that its edge sits inside the fog.
#define SHADOW_HALF   (ARENA_HALF > ARENA_HALF_DEF ? 64.0f : 37.0f)
#define SHADOW_DEPTH  96.0f
// World depth range. See the comment at the main projection: 1/near sets the
// depth resolution, and sub-millimetre nesting on a figure is only stable if the
// buffer can still separate it at across-the-arena distance.
#define ZNEAR         0.12f
// 140 fits the 48 m arena (and keeps its frames bit-identical); a 104 m map's
// diagonal alone is 147, so the large maps push it out. Depth precision is set by
// ZNEAR, so the far plane costs nothing measurable either way.
#define ZFAR          (ARENA_HALF > ARENA_HALF_DEF ? 240.0f : 140.0f)
#define SCOPE_OCU_Z   -0.052f // gun-space z of the ocular glass
#define SCOPE_OCU_R    0.0205f // ocular bell radius
#define SCOPE_TUBE_R   0.0142f // main tube radius
#define SCOPE_SEGS     96   // the disc rim must out-resolve the ocular's own
#define SCOPE_FOV_DEG 9.0f  // ~6x against a 55 deg hip view
// Where on the ADS ramp the scope picture starts to open. ONE number, TWO
// readers: the render gate that decides whether the scope FBO is filled at all,
// and the HUD disc's own radius ramp. Written twice they drift, and the way
// they drift is a pass that costs a whole extra scene and shows nothing.
#define SCOPE_ADS_OPEN 0.55f

typedef struct {
  int    width, height;
  GLuint prog_world, prog_hud, prog_sky, prog_depth;
  GLint  u_vp, u_eye, u_detail, u_emis, u_screen, u_tex, u_img;
  GLint  u_lightvp, u_smtexel, u_shadow, u_smstep, u_sm, u_dvp;
  GLint  u_vm, u_fill, u_rim, u_muzc, u_muzl, u_muzp;
  GLint  u_mdlo, u_mdlx, u_mdlz;           // the weapon's own frame
  GLint  u_surf;                           // the floor's theme treatment
  GLuint sm_fbo, sm_tex;             // sun shadow map (depth only)
  mat4   light_vp;
  GLuint scope_fbo, scope_tex, scope_depth;  // picture-in-picture scope target
  int    scope_size;
  GLint  u_sky_fwd, u_sky_right, u_sky_up, u_sky_tan;
  GLuint vao_world, vbo_world, vao_hud, vbo_hud, vao_sky, font_tex;
  GLuint vao_scene, vbo_scene;   // per-frame dynamic 3D batch (bot, VFX, gun)
  int    world_verts;
  // THE SHADOW PASS DRAWS A PREFIX, NOT THE WHOLE BUFFER. The solids come first
  // and the decor after, and only the solids cast: a shadow map is 4096 over
  // 62 m, i.e. 15 mm a texel, and a 22 mm grass blade at that resolution is not
  // a shadow, it is one texel of noise flickering under the figure standing on
  // it. The decor's own contact darkening is baked into the floor beneath it
  // instead, which is both cheaper and steadier.
  int    world_cast_verts, world_decor_verts;
  GLuint present_fbo;               // frame destination: 0 on Windows, harness FBO on Linux
  GLuint msaa_fbo, msaa_rb[2];      // multisampled scene target, msaa_fbo 0 = MSAA off
  int    msaa_cfg, msaa_w, msaa_h;  // setting/size the MSAA target was built for
  // THE HDR CHAIN (see POST_FS). scene_* is the single-sample RGBA16F frame
  // every lit pass ends in; z_*[0] is the world depth copied before the
  // viewmodel pass, z_*[1] the viewmodel's own depth after it.
  GLuint scene_fbo, scene_tex, scene_zrb;
  GLuint z_fbo[2], z_tex[2];
  GLuint ao_fbo, ao_tex, vol_fbo, vol_tex;
  GLuint bl_fbo[5], bl_tex[5];
  GLuint lum_fbo, lum_tex, ad_fbo[2], ad_tex[2];
  int    ad_i, ad_frame, lum_levels, bl_w[5], bl_h[5];
  int    post_w, post_h, frame_no, vm_drawn, have_prev;
  mat4   prev_vp, cur_vp, inv_vp;
  float  tan_x, tan_y, ads_now, vm_vel[2];
  v3     cam_fwd, cam_rgt, cam_up;
  GLuint sm_raw;                    // sampler: the shadow map WITHOUT compare (PCSS)
  GLuint prog_post, prog_lum, prog_adapt, prog_ao, prog_vol, prog_bdown, prog_bup;
  atm_loc_t atm_loc[2];              // world, sky
  GLuint blur_fbo[2], blur_tex[2];  // quarter-res ping-pong: the frosted menu
  GLuint prog_blur;                 // dual-Kawase tap (see ui_menu_backdrop)
  GLint  u_blur_tex, u_blur_px;
  int    blur_w, blur_h;            // size the blur chain was built for
} renderer_t;

static renderer_t R;

static GLuint gl_compile(GLenum type, const char *src) {
  GLuint sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, NULL);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetShaderInfoLog(sh, sizeof log, NULL, log);
    fatal(log);
  }
  return sh;
}

static GLuint gl_program(const char *vs, const char *fs) {
  GLuint p = glCreateProgram();
  GLuint v = gl_compile(GL_VERTEX_SHADER, vs), f = gl_compile(GL_FRAGMENT_SHADER, fs);
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetProgramInfoLog(p, sizeof log, NULL, log);
    fatal(log);
  }
  glDeleteShader(v);
  glDeleteShader(f);
  return p;
}

// World mesh: 36 verts per solid + 6 for the ground quad, 12 floats each
// (position, normal, albedo, material).
#define VERT_FLOATS 12

// --- surface state: what the next vertices are MADE OF ---------------------
// Roughness and metalness belong to a PART, not to a vertex, and threading them
// through every scene_box/scene_quad/fig_chain call would touch a hundred call
// sites to say "still steel". So they are batch state, set once before a part is
// emitted, exactly like a GL bind.
// Ambient occlusion is the opposite — it varies per vertex — but it is always
// the same function of height above whatever the part is standing on, so it is
// state too: a base height, a rate and a floor. ao_off() means "fully open sky",
// which is what a viewmodel in front of your face has.
static float g_mat_rough = 0.72f, g_mat_metal = 0.0f;
static float g_ao_base = 0.0f, g_ao_rate = 0.0f, g_ao_min = 1.0f;

static void mat_set(float rough, float metal) {
  g_mat_rough = rough;
  g_mat_metal = metal;
}
static void ao_set(float base, float rate, float min_v) {
  g_ao_base = base;
  g_ao_rate = rate;
  g_ao_min = min_v;
}
static void ao_off(void) { g_ao_min = 1.0f; g_ao_rate = 0.0f; }

// The world's own occlusion ramp for a solid: concentrated in the bottom half
// metre, because spread over 1.7 m it is an 8% wash with nothing at the contact,
// which is the one place occlusion shows. Named because it has a SECOND reader —
// an impact crater has to darken with the face it is stamped in, and a copy of
// the number there would be a copy of the world's shading.
#define WORLD_AO_RATE (1.0f / 0.55f)
#define WORLD_AO_MIN  0.55f
// ...and the two finishes the world pass gives its own surfaces, for the same
// reason: a decal whose roughness does not match its host has a silhouette at
// GRAZING angles, which is exactly where a wall you are running past is seen.
#define WORLD_SOLID_R  0.72f   // cover, crates, the shell

// Cavity term, set per band by fig_chain from fnode_t.ao.
static float g_ao_mul = 1.0f;

// Named finishes, so "gun metal" means the same thing on the bot's rifle, on the
// viewmodel and on the corpse's dropped weapon. Roughness is what the eye reads
// as material: cloth has no highlight at all, polymer has a broad one, and
// phosphated steel has a tight one that moves with the light.
#define MAT_CLOTH_R    0.88f   // cotton ripstop: no highlight, only sheen
#define MAT_GEAR_R     0.72f   // coyote nylon: carrier, pouches, webbing
#define MAT_HELM_R     0.42f   // composite shell: broad semi-gloss
#define MAT_RUBBER_R   0.90f   // knee / elbow pads
#define MAT_LEATHER_R  0.50f   // boots
// A GLOVE is not a boot. At 0.50 the sky reflection carried a dark leather's
// whole highlight, so every hand facet turned toward the sky rendered blue-grey
// — measured on the turntable, the back of the support hand read as slate, not
// as brown, and a hand the colour of the sleeve is a hand that has stopped being
// a separate object. Nomex/goatskin is a dull surface.
#define MAT_GLOVE_R    0.72f
// Glass-filled nylon furniture is not semi-gloss. At 0.55 the broad sheen on a
// stock's up-facing surfaces made the POLYMER the brightest thing on the weapon
// — brighter than the parkerized receiver beside it, which is backwards: a metal
// has no diffuse and therefore collects less total light than a dielectric of
// the same darkness, so the two only sort correctly if the polymer's own sheen
// is as weak as the real material's.
#define MAT_POLY_R     0.62f   // weapon furniture: grips, handguards, stocks
// A METAL HAS NO DIFFUSE, so its roughness is not a taste setting, it is the
// only thing that decides whether the form is modelled at all. At 0.30 the GGX
// lobe is narrow enough to land the whole sun on ONE facet of a receiver and
// leave every other facet lit by the environment term alone — which is what a
// black plastic casting looks like. Widened until the highlight WRAPS: the sun
// now travels across the receiver's flats and its chamfers as the weapon turns,
// and the micro-structure has a lobe wide enough to break up.
#define MAT_PARK_R     0.44f   // parkerized receiver: matte metal
#define MAT_PARK_M     0.90f
// The other side of the same argument: a turned barrel is NOT a mirror, but it
// is the bright family, and 0.17 put it close enough to one that the glossy
// environment lobe collapsed to a point nothing could catch. 0.24 keeps the
// glint tight enough to read as a different material from the receiver beside
// it and wide enough that a turning mark has something to modulate.
#define MAT_STEEL_R    0.24f   // barrel, bolt, turrets: bright metal
#define MAT_STEEL_M    1.00f
#define MAT_GLASS_R    0.05f

// The ramp itself, as a pure function. It exists separately because mark_stamp
// runs inside the SIM (ev_push -> vfx_on_event) and has to ASK what the world's
// occlusion is at a height without moving the renderer's batch state to do it —
// this file's architecture note is explicit that the sim's only channel to
// presentation is the event ring.
static float ao_ramp(float y, float base, float rate, float min_v) {
  if (min_v >= 1.0f) return 1.0f;
  float t = (y - base) * rate;
  t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
  return min_v + (1.0f - min_v) * t;
}

static float ao_at(float y) {
  return ao_ramp(y, g_ao_base, g_ao_rate, g_ao_min);
}

// --- THE FLOOR IS THE LARGEST SURFACE IN THE FRAME, and it was one quad ----
// Six vertices carrying one colour over 48x48 m: every centimetre of it had
// the same albedo, the same roughness and the same occlusion, and the only
// thing that ever varied across it was a 2 m checker. A fragment shader cannot
// fix that, because the variation that makes a floor read as GROUND is a
// function of where the walls are — sand piles in the lee of an obstacle,
// traffic packs the open lanes, and the metre nearest a wall is always dirtier
// and darker than the middle of a lane. That is a per-VERTEX quantity: it costs
// zero fragment ALU, it is baked once when the arena changes, and it writes
// into the three channels a vertex already has.
//
// A 1 m grid over the arena. Deliberately not finer: the contact skirt is the
// only term with a length scale under a metre, and the decor's own rubble sits
// exactly there and carries the crisp end of that read. 2304 quads = 13824
// verts against the solids' ~1200 — the floor becomes the bulk of the world
// mesh, which is the correct proportion for a surface that is half the screen.
// The CELL COUNT is the authored number and the step is derived from it: a
// float division is not an integer constant expression, so authoring the step
// makes g_mesh's size variably modified and the array stops being a file-scope
// object gcc can size.
// 32, i.e. a 1.5 m cell. The shortest length scale anything in the bake has is
// the 1.3 m contact skirt, so 1.5 m still carries every term it has to carry —
// and the grid is drawn twice a frame at 1280x720 on a CPU rasteriser, where
// 13824 vertices of flat floor measured 1.45 ms/frame against 6144.
// THE CELL SIZE IS KEPT, NOT THE CELL COUNT: 32 across the 48 m arena, scaled
// with the map, so a 104 m map's floor carries the same 1.5 m skirt detail
// instead of stretching 32 cells over twice the ground. GROUND_CELLS_MAX sizes
// the arrays; ground_cells() is the live count.
#define GROUND_CELLS_MAX 80
static int ground_cells(void) {
  int n = (int)(32.0f * ARENA_HALF / ARENA_HALF_DEF + 0.5f);
  return n < 32 ? 32 : n > GROUND_CELLS_MAX ? GROUND_CELLS_MAX : n;
}
#define GROUND_CELLS (ground_cells())
#define GROUND_STEP  (2.0f * ARENA_HALF / (float)GROUND_CELLS)
#define GROUND_VERTS (GROUND_CELLS_MAX * GROUND_CELLS_MAX * 6)

// What the floor is MADE OF at one point, as three independent scalars. Kept as
// a struct rather than three parallel loops because all three want the same
// scan over the solids and that scan is the whole cost of the bake.
typedef struct {
  float drift;    // wind-blown sand banked against an obstacle
  float compact;  // open ground, packed by traffic: darker, less loose
  float skirt;    // the metre nearest a wall: contact darkening
} gsurf_t;

// Smooth value noise in C, matching the shader's own construction so a term
// baked here and a term added there cannot disagree about what a metre is.
// The arguments are CELL INDICES — small integers — and the mixing happens in
// uint32, where overflow is defined. The first version scaled the coordinate by
// 6.7e8 and cast that to int32: past a cell index of four the product left the
// signed range, which is undefined behaviour and which -O3 -ffast-math is
// entitled to turn into anything at all. The visible symptom was that every
// noise term in the bake went flat over most of the arena — the lane mask, the
// patchiness, all of it — so the floor had no large shapes in it and the whole
// three-to-eight-metre band the eye reads a floor at was empty.
static float fhash2(float x, float z) {
  uint32_t h = (uint32_t)(int32_t)x * 374761393u +
               (uint32_t)(int32_t)z * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return (float)((h ^ (h >> 16)) >> 8) * (1.0f / 16777216.0f);
}
static float fnoise2(float x, float z) {
  float ix = floorf(x), iz = floorf(z), fx = x - ix, fz = z - iz;
  fx = fx * fx * (3.0f - 2.0f * fx);
  fz = fz * fz * (3.0f - 2.0f * fz);
  float a = fhash2(ix, iz),         b = fhash2(ix + 1.0f, iz);
  float c = fhash2(ix, iz + 1.0f),  d = fhash2(ix + 1.0f, iz + 1.0f);
  return (a + (b - a) * fx) + ((c - a) + (a - b + d - c) * fx) * fz;
}

// How far UPWIND of this point the given footprint is, or -1 if the wind does
// not pass over it on its way here. A 2D slab test along the wind, which is the
// same construction shadow_foot makes for the sun one boundary out: a drift is
// a shadow cast by a wall, and what casts it is the wind.
static float wind_shadow(const solid_t *s, float x, float z, v3 w, float sgn) {
  float dx = w.x * sgn, dz = w.z * sgn;
  float t0 = -1e9f, t1 = 1e9f;
  if (fabsf(dx) < 1e-6f) {
    if (x < s->min.x || x > s->max.x) return -1.0f;
  } else {
    float a = (s->min.x - x) / dx, b = (s->max.x - x) / dx;
    if (a > b) { float t = a; a = b; b = t; }
    if (a > t0) t0 = a;
    if (b < t1) t1 = b;
  }
  if (fabsf(dz) < 1e-6f) {
    if (z < s->min.z || z > s->max.z) return -1.0f;
  } else {
    float a = (s->min.z - z) / dz, b = (s->max.z - z) / dz;
    if (a > b) { float t = a; a = b; b = t; }
    if (a > t0) t0 = a;
    if (b < t1) t1 = b;
  }
  if (t1 < t0 || t1 < 0.0f) return -1.0f;
  return t0 > 0.0f ? t0 : 0.0f;
}

// One point of floor against every solid. O(ground x solids) once per arena —
// 2304 x ~50, i.e. free at the only moment it runs, and the reason the whole
// thing is a bake rather than a shader: a fragment cannot afford to ask where
// the walls are, and this term is ABOUT where the walls are.
static gsurf_t ground_sample(float x, float z) {
  gsurf_t g = {0.0f, 0.0f, 0.0f};
  v3 w = wind_dir();
  float near_d = 1e9f;
  for (int i = 0; i < g_num_solids; i++) {
    const solid_t *s = &g_solids[i];
    if (s->max.y < 0.30f) continue;         // a step crate banks nothing
    float dx = fmaxf(fmaxf(s->min.x - x, x - s->max.x), 0.0f);
    float dz = fmaxf(fmaxf(s->min.z - z, z - s->max.z), 0.0f);
    float d = sqrtf(dx * dx + dz * dz);
    if (d < near_d) near_d = d;
    if (d > 9.0f) continue;
    // A DRIFT IS A SHADOW, NOT A HALO. The first version scored the distance to
    // the footprint and weighted it by which face the point sat off, and the
    // result was an isotropic ring around every solid in the arena — every wall
    // banked sand on all four sides equally, which says nothing about wind and
    // reads as a glow. What deposits material is the air being in the lee, so
    // the test is whether the wind reaches this point THROUGH the solid. Two
    // lengths, because a windward ramp is short and steep and a lee tail is long.
    float lee = wind_shadow(s, x, z, w, -1.0f);
    if (lee >= 0.0f) {
      float amt = expf(-lee / 3.4f);
      if (amt > g.drift) g.drift = amt;
    }
    float wnd = wind_shadow(s, x, z, w, 1.0f);
    if (wnd >= 0.0f) {
      float amt = expf(-wnd / 1.15f) * 0.45f;
      if (amt > g.drift) g.drift = amt;
    }
  }
  // Traffic packs LANES, not acreage. The first version was openness alone and
  // openness is nearly the whole arena — past five metres from cover every
  // point scored 1, so the entire floor came out packed, every ripple in the
  // map was gated off by it and the sand went back to being a plane. What packs
  // ground is people walking the same line, so it is openness TIMES a broad
  // lane mask: about a third of the open floor, in runs ten metres across.
  float open01 = (near_d - 1.6f) * (1.0f / 3.4f);
  open01 = open01 < 0.0f ? 0.0f : open01 > 1.0f ? 1.0f : open01;
  float lane = fnoise2(x * 0.088f, z * 0.088f) * 0.7f +
               fnoise2(x * 0.21f, z * 0.21f) * 0.3f;
  lane = (lane - 0.44f) * 3.6f;
  lane = lane < 0.0f ? 0.0f : lane > 1.0f ? 1.0f : lane;
  g.compact = open01 * lane;
  g.skirt = expf(-near_d / 1.30f);
  // Drift and traffic are the same sand arguing over the same ground: a lane
  // that is walked flat cannot also be a dune. Traffic wins, which is why the
  // drifts survive only where nobody goes.
  g.drift *= 1.0f - 0.75f * g.compact;
  return g;
}

// The floor's own occlusion, asked for by NAME because mark_stamp has to stamp
// an impact crater into ground whose occlusion it did not build. A copy of the
// number there would be a copy of the world's shading, and the two would drift.
static float ground_skirt_ao(float x, float z) {
  return 1.0f - 0.34f * ground_sample(x, z).skirt;
}

// Sized from MEASURED demand, not a guess: the handmade towns dress far more
// than the 24 m arenas (citadel 290,250 decor verts, refinery 197,334, harbor
// 177,864, outpost 17,196). The old 49,152 cut all three towns' tails off —
// the dropped geometry was street rubble and scatter, built last, so it read as
// bare ground rather than as an error. 384 Ki leaves ~35% over the citadel.
// Decor does not cast (it lands after world_cast_verts), so the cost is the
// main pass and ~16 MB of BSS. ci-proofs gates world_drops = 0 on every map.
// RAISED AGAIN to the staging buffer's own size (SCENE_MAX_VERTS: decor is
// built in the scene buffer and copied, so nothing larger could ever arrive)
// when the town dressing (dec_town: cornices, window frames, bars, louvers,
// flanges, cables) put the citadel at ~420k.
#define DECOR_MAX_VERTS 524288
#define PROPS_MAX_VERTS 262144  // detailed vehicles and lanterns (mesh_props)
static float g_mesh[(MAX_SOLIDS * 36 + GROUND_VERTS + DECOR_MAX_VERTS + PROPS_MAX_VERTS) * VERT_FLOATS];
static int   g_mesh_floats;
static long  g_mesh_drops;

// The raw pusher. mesh_push below is the batch-state form every solid uses; the
// floor needs its three channels PER VERTEX, which is the whole point of baking
// them, so it writes through here instead of through the batch state.
static void mesh_vert(v3 p, v3 n, v3 c, float rough, float metal, float ao) {
  if (g_mesh_floats + VERT_FLOATS >
      (int)(sizeof g_mesh / sizeof g_mesh[0])) {
    // Unlike scene_push this used to write blind, because the array was sized
    // exactly for a full solid list and could not overflow. It can now.
    if (!g_mesh_drops++)
      fprintf(stderr, "mesh: world vertex budget exhausted — geometry dropped\n");
    return;
  }
  float *v = g_mesh + g_mesh_floats;
  v[0] = p.x; v[1] = p.y; v[2] = p.z;
  v[3] = n.x; v[4] = n.y; v[5] = n.z;
  v[6] = c.x; v[7] = c.y; v[8] = c.z;
  v[9] = rough; v[10] = metal; v[11] = ao;
  g_mesh_floats += VERT_FLOATS;
}

// The surface material rides in the AO channel's INTEGER part: AO is in [0, 1],
// so ao + 4 * material decodes exactly with one floor() in WORLD_FS, and the
// vertex format, the shadow pass and every other draw stay as they are. Only
// the static world writes it; the scene stream (figures, VFX, the viewmodel)
// always carries material 0, and the shader decodes it only under uDetail.
static int g_mesh_mat;
static int g_props_verts;   // what mesh_props built (printed by `budget`)
static void mesh_push(v3 p, v3 n, v3 c) {
  mesh_vert(p, n, c, g_mat_rough, g_mat_metal, ao_at(p.y) + 4.0f * (float)g_mesh_mat);
}

static void mesh_quad(v3 a, v3 b, v3 c, v3 d, v3 n, v3 col) {
  mesh_push(a, n, col); mesh_push(b, n, col); mesh_push(c, n, col);
  mesh_push(a, n, col); mesh_push(c, n, col); mesh_push(d, n, col);
}

// One baked corner of the floor grid. Everything the bake decides lands in the
// three channels a vertex already carries, so the fragment shader pays nothing
// for any of it:
//   albedo    drift lightens (loose dry sand scatters more), traffic darkens
//             (packed ground is a darker ground), skirt darkens further
//   roughness loose sand is rougher than a packed lane
//   AO        the contact skirt, which is the term that was missing entirely —
//             the wall/floor join was a hard line with no occlusion anywhere on
//             it, and that join is the single most artificial edge in a box
//             arena.
//
// AND IT IS LOW-PASSED BEFORE IT IS EMITTED. Three values per vertex on a 1.5 m
// grid, linearly interpolated across two triangles, show their own lattice the
// moment adjacent vertices disagree by more than the eye's threshold: the quad
// diagonal becomes a visible crease and the whole floor carries a rectangular
// mesh. One 3x3 pass over the grid removes exactly the vertex-to-vertex
// component that the interpolation cannot hide, and it costs nothing at runtime
// because the whole thing is a bake.
typedef struct { float tint, rough, ao; } gvert_t;
static gvert_t g_gbake[(GROUND_CELLS_MAX + 1) * (GROUND_CELLS_MAX + 1)];

static void ground_bake(void) {
  float H = ARENA_HALF;
  int N = GROUND_CELLS + 1;
  for (int iz = 0; iz < N; iz++)
    for (int ix = 0; ix < N; ix++) {
      float x = -H + (float)ix * GROUND_STEP, z = -H + (float)iz * GROUND_STEP;
      gsurf_t g = ground_sample(x, z);
      float t = 1.0f + 0.33f * g.drift - 0.11f * g.compact - 0.15f * g.skirt;
      t *= 0.965f + 0.07f * fnoise2(x * 0.11f, z * 0.11f);
      float r = WORLD_GROUND_R + 0.13f * g.drift - 0.08f * g.compact;
      g_gbake[iz * N + ix] = (gvert_t){t, r, 1.0f - 0.34f * g.skirt};
    }
  static gvert_t tmp[(GROUND_CELLS_MAX + 1) * (GROUND_CELLS_MAX + 1)];
  memcpy(tmp, g_gbake, sizeof tmp);
  for (int iz = 0; iz < N; iz++)
    for (int ix = 0; ix < N; ix++) {
      float st = 0.0f, sr = 0.0f, sa = 0.0f, w = 0.0f;
      for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
          int jx = ix + dx, jz = iz + dz;
          if (jx < 0 || jz < 0 || jx >= N || jz >= N) continue;
          float k = (dx == 0 && dz == 0) ? 4.0f : (dx == 0 || dz == 0) ? 2.0f : 1.0f;
          const gvert_t *q = &tmp[jz * N + jx];
          st += q->tint * k; sr += q->rough * k; sa += q->ao * k; w += k;
        }
      g_gbake[iz * N + ix] = (gvert_t){st / w, sr / w, sa / w};
    }
}

static void mesh_ground_vert(int ix, int iz, v3 base) {
  const gvert_t *g = &g_gbake[iz * (GROUND_CELLS + 1) + ix];
  v3 c = {{base.x * g->tint, base.y * g->tint, base.z * g->tint}};
  float r = g->rough < 0.05f ? 0.05f : g->rough > 1.0f ? 1.0f : g->rough;
  mesh_vert((v3){{-ARENA_HALF + (float)ix * GROUND_STEP, 0.0f,
                  -ARENA_HALF + (float)iz * GROUND_STEP}},
            (v3){{0, 1, 0}}, c, r, 0.0f, g->ao);
}

// Defined far below, next to the scene primitives it is built out of: decor is
// tumbled boxes, tapered blades and sloped wedges, and every one of those
// already exists as a scene_* primitive. Building a second copy of that library
// against g_mesh so it could live up here would be two copies of a box.
static void mesh_decor(void);
static void mesh_props(void);

static void mesh_build(void) {
  g_mesh_floats = 0;
  v3 ground_col = g_theme.ground;
  ao_off();
  mat_set(WORLD_GROUND_R, 0.0f);  // dusty concrete: a wide, weak sheen at grazing
  // Cover surfaces darken toward the floor: the old bake multiplied it into the
  // albedo, which also dimmed the sun; as a material term it dims the AMBIENT,
  // which is what an occluded surface actually loses.
  mat_set(WORLD_SOLID_R, 0.0f);
  // Concentrated in the bottom half metre. Spread linearly over 1.7 m it was an
  // 8% wash with nothing at the contact, which is the one place occlusion shows.
  ao_set(0.0f, WORLD_AO_RATE, WORLD_AO_MIN);
  for (int i = 0; i < g_num_solids; i++) {
    if (g_solid_hide[i]) continue;          // a prop draws it (mesh_props)
    const solid_t *s = &g_solids[i];
    v3 mn = s->min, mx = s->max, c = s->color;
    g_mesh_mat = g_solid_mat[i] < SMAT_COUNT ? g_solid_mat[i] : SMAT_PAINT;
    // +x -x +y -y +z -z faces, wound counter-clockwise from outside.
    mesh_quad((v3){{mx.x,mn.y,mn.z}},(v3){{mx.x,mx.y,mn.z}},(v3){{mx.x,mx.y,mx.z}},(v3){{mx.x,mn.y,mx.z}},(v3){{1,0,0}},c);
    mesh_quad((v3){{mn.x,mn.y,mx.z}},(v3){{mn.x,mx.y,mx.z}},(v3){{mn.x,mx.y,mn.z}},(v3){{mn.x,mn.y,mn.z}},(v3){{-1,0,0}},c);
    mesh_quad((v3){{mn.x,mx.y,mn.z}},(v3){{mn.x,mx.y,mx.z}},(v3){{mx.x,mx.y,mx.z}},(v3){{mx.x,mx.y,mn.z}},(v3){{0,1,0}},c);
    mesh_quad((v3){{mn.x,mn.y,mx.z}},(v3){{mn.x,mn.y,mn.z}},(v3){{mx.x,mn.y,mn.z}},(v3){{mx.x,mn.y,mx.z}},(v3){{0,-1,0}},c);
    mesh_quad((v3){{mn.x,mn.y,mx.z}},(v3){{mx.x,mn.y,mx.z}},(v3){{mx.x,mx.y,mx.z}},(v3){{mn.x,mx.y,mx.z}},(v3){{0,0,1}},c);
    mesh_quad((v3){{mx.x,mn.y,mn.z}},(v3){{mn.x,mn.y,mn.z}},(v3){{mn.x,mx.y,mn.z}},(v3){{mx.x,mx.y,mn.z}},(v3){{0,0,-1}},c);
  }
  g_mesh_mat = SMAT_PAINT;
  ao_off();
  mesh_props();   // the detailed vehicles and lanterns: they CAST, so they go here
  // EVERYTHING THAT CASTS IS NOW BEHIND US. The floor cannot: it is the lowest
  // surface in the world and there is nothing under it to shadow, so its 4608
  // triangles were being rasterised into a 4096-square depth map every frame to
  // decide the shadowing of nothing at all. The decor cannot either, and for a
  // different reason — at 15 mm a texel a 22 mm grass blade is not a shadow, it
  // is one texel of noise flickering under whoever is standing on it.
  R.world_cast_verts = g_mesh_floats / VERT_FLOATS;
  mat_set(WORLD_GROUND_R, 0.0f);
  ao_off();
  ground_bake();
  const int gc = GROUND_CELLS;
  for (int iz = 0; iz < gc; iz++)
    for (int ix = 0; ix < gc; ix++) {
      mesh_ground_vert(ix,     iz,     ground_col);
      mesh_ground_vert(ix,     iz + 1, ground_col);
      mesh_ground_vert(ix + 1, iz + 1, ground_col);
      mesh_ground_vert(ix,     iz,     ground_col);
      mesh_ground_vert(ix + 1, iz + 1, ground_col);
      mesh_ground_vert(ix + 1, iz,     ground_col);
    }
  int decor0 = g_mesh_floats;
  mesh_decor();
  R.world_decor_verts = (g_mesh_floats - decor0) / VERT_FLOATS;
}

// The world VBO is static until a fresh map arrives (new match = new arena);
// the sim then flags g_world_dirty and the next frame re-uploads once.
static void world_upload(void) {
  mesh_build();
  R.world_verts = g_mesh_floats / VERT_FLOATS;
  if (R.world_cast_verts > R.world_verts) R.world_cast_verts = R.world_verts;
  glBindBuffer(GL_ARRAY_BUFFER, R.vbo_world);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(g_mesh_floats * sizeof(float)), g_mesh, GL_STATIC_DRAW);
}

static void renderer_init(int width, int height) {
  R.width = width;
  R.height = height;
  R.scope_size = height * 2 / 3;
  if (R.scope_size < 768) R.scope_size = 768;
  if (R.scope_size > 1536) R.scope_size = 1536;
  R.prog_world = gl_program(WORLD_VS, WORLD_FS);
  R.prog_hud = gl_program(HUD_VS, HUD_FS);
  R.prog_sky = gl_program(SKY_VS, SKY_FS);
  R.prog_depth = gl_program(DEPTH_VS, DEPTH_FS);
  R.prog_post = gl_program(BLUR_VS, POST_FS);
  R.prog_lum = gl_program(BLUR_VS, LUM_FS);
  R.prog_adapt = gl_program(BLUR_VS, ADAPT_FS);
  R.prog_ao = gl_program(BLUR_VS, AO_FS);
  R.prog_vol = gl_program(BLUR_VS, VOL_FS);
  R.prog_bdown = gl_program(BLUR_VS, BDOWN_FS);
  R.prog_bup = gl_program(BLUR_VS, BUP_FS);
  glGenSamplers(1, &R.sm_raw);
  glSamplerParameteri(R.sm_raw, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glSamplerParameteri(R.sm_raw, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glSamplerParameteri(R.sm_raw, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(R.sm_raw, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(R.sm_raw, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  {
    GLuint pr[2] = {R.prog_world, R.prog_sky};
    for (int i = 0; i < 2; i++) {
      atm_loc_t *l = &R.atm_loc[i];
      l->sun = glGetUniformLocation(pr[i], "SUN_DIR");
      l->col = glGetUniformLocation(pr[i], "SUN_COL");
      l->hor = glGetUniformLocation(pr[i], "SKY_HOR");
      l->top = glGetUniformLocation(pr[i], "SKY_TOP");
      l->gnd = glGetUniformLocation(pr[i], "SKY_GND");
      l->glow = glGetUniformLocation(pr[i], "uGlow");
      l->exp = glGetUniformLocation(pr[i], "EXPOSURE");
      l->amb = glGetUniformLocation(pr[i], "uAmb");
      l->fog = glGetUniformLocation(pr[i], "uFog");
      l->cloud = glGetUniformLocation(pr[i], "uCloud");
    }
  }
  R.u_dvp = glGetUniformLocation(R.prog_depth, "uVP");
  R.u_lightvp = glGetUniformLocation(R.prog_world, "uLightVP");
  R.u_smtexel = glGetUniformLocation(R.prog_world, "uSmTexel");
  R.u_shadow = glGetUniformLocation(R.prog_world, "uShadow");
  R.u_smstep = glGetUniformLocation(R.prog_world, "uSmStep");
  R.u_sm = glGetUniformLocation(R.prog_world, "uSm");
  R.u_vm = glGetUniformLocation(R.prog_world, "uVm");
  R.u_muzc = glGetUniformLocation(R.prog_world, "uMuzC");
  R.u_muzl = glGetUniformLocation(R.prog_world, "uMuzL");
  R.u_muzp = glGetUniformLocation(R.prog_world, "uMuzP");
  R.u_fill = glGetUniformLocation(R.prog_world, "uFill");
  R.u_rim = glGetUniformLocation(R.prog_world, "uRim");
  R.u_mdlo = glGetUniformLocation(R.prog_world, "uMdlO");
  R.u_mdlx = glGetUniformLocation(R.prog_world, "uMdlX");
  R.u_mdlz = glGetUniformLocation(R.prog_world, "uMdlZ");
  R.u_vp = glGetUniformLocation(R.prog_world, "uVP");
  R.u_eye = glGetUniformLocation(R.prog_world, "uEye");
  R.u_detail = glGetUniformLocation(R.prog_world, "uDetail");
  R.u_surf = glGetUniformLocation(R.prog_world, "uSurf");
  R.u_emis = glGetUniformLocation(R.prog_world, "uEmis");
  R.u_screen = glGetUniformLocation(R.prog_hud, "uScreen");
  R.u_tex = glGetUniformLocation(R.prog_hud, "uTex");
  R.u_img = glGetUniformLocation(R.prog_hud, "uImg");
  R.prog_blur = gl_program(BLUR_VS, BLUR_FS);
  R.u_blur_tex = glGetUniformLocation(R.prog_blur, "uTex");
  R.u_blur_px = glGetUniformLocation(R.prog_blur, "uPix");
  R.u_sky_fwd = glGetUniformLocation(R.prog_sky, "uFwd");
  R.u_sky_right = glGetUniformLocation(R.prog_sky, "uRight");
  R.u_sky_up = glGetUniformLocation(R.prog_sky, "uUp");
  R.u_sky_tan = glGetUniformLocation(R.prog_sky, "uTan");
  glGenVertexArrays(1, &R.vao_sky);  // attribute-less: verts from gl_VertexID

  glGenVertexArrays(1, &R.vao_world);
  glGenBuffers(1, &R.vbo_world);
  glBindVertexArray(R.vao_world);
  world_upload();
  GLsizei stride = VERT_FLOATS * sizeof(float);
  for (int pass = 0; pass < 2; pass++) {
    if (pass) {  // the per-frame dynamic batch: same layout, own VAO/VBO
      glGenVertexArrays(1, &R.vao_scene);
      glGenBuffers(1, &R.vbo_scene);
      glBindVertexArray(R.vao_scene);
      glBindBuffer(GL_ARRAY_BUFFER, R.vbo_scene);
    }
    for (int a = 0; a < 4; a++) {
      glVertexAttribPointer((GLuint)a, 3, GL_FLOAT, GL_FALSE, stride,
                            (void *)(size_t)(3 * a * sizeof(float)));
      glEnableVertexAttribArray((GLuint)a);
    }
  }

  glGenVertexArrays(1, &R.vao_hud);
  glGenBuffers(1, &R.vbo_hud);
  glBindVertexArray(R.vao_hud);
  glBindBuffer(GL_ARRAY_BUFFER, R.vbo_hud);
  GLsizei hs = 9 * sizeof(float);  // x y u v r g b a mode
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, hs, (void *)0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, hs, (void *)(2 * sizeof(float)));
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, hs, (void *)(4 * sizeof(float)));
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, hs, (void *)(8 * sizeof(float)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);

  // The distance atlas is baked on the CPU at startup (~half a megabyte of
  // BSS, written once). LINEAR filtering is what makes the field usable: the
  // shader thresholds the bilinear-interpolated distance, not texel steps.
  static uint8_t hf_atlas[HF_TEX_H][HF_TEX_W];
  font_bake(&hf_atlas[0][0]);
  glGenTextures(1, &R.font_tex);
  glBindTexture(GL_TEXTURE_2D, R.font_tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, HF_TEX_W, HF_TEX_H, 0, GL_RED_CORE, GL_UNSIGNED_BYTE, hf_atlas);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Sun shadow map. The arena is 48 m across and bounded, so ONE map covering
  // all of it beats any cascade scheme here: no split seams, no scrolling
  // flicker, and every shadow in the level is present at every distance.
  // Depth-compare is done by the sampler (sampler2DShadow), which makes each of
  // the shader's sixteen taps bilinear for free.
  glGenTextures(1, &R.sm_tex);
  glBindTexture(GL_TEXTURE_2D, R.sm_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_TEX, SHADOW_TEX, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  {  // border = "nothing in front of the light", so outside the map is fully lit
    const float lit[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, lit);
  }
  glGenFramebuffers(1, &R.sm_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, R.sm_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, R.sm_tex, 0);
  glDrawBuffer(GL_NONE);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    R.sm_fbo = 0;   // unshadowed game rather than no game
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Scope target: the magnified view is rendered here once per frame while the
  // sniper is shouldered, then shown as a disc inside the ocular. Square, because
  // the disc is round and the aspect must not stretch inside it.
  glGenTextures(1, &R.scope_tex);
  glBindTexture(GL_TEXTURE_2D, R.scope_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, R.scope_size, R.scope_size, 0, GL_RGBA,
               GL_HALF_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenRenderbuffers(1, &R.scope_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, R.scope_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                        R.scope_size, R.scope_size);
  glGenFramebuffers(1, &R.scope_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, R.scope_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         R.scope_tex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                            R.scope_depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    R.scope_fbo = 0;  // no scope picture rather than no game
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, R.font_tex);

  glViewport(0, 0, width, height);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glClearColor(0.71f, 0.73f, 0.75f, 1.0f);  // ~= tonemap(sky_env) at the horizon
}

static int glyph_index(char ch) {
  unsigned char c = (unsigned char)ch;
  if (c >= 'a' && c <= 'z') c -= 32;           // CAPS-only brand voice
  if (c >= 32 && c < 127) return (int)c - 32;  // printable ASCII
  if (c >= 1 && c <= HF_ICONS) return HF_ASCII + (int)c - 1;  // \x01.. icons
  return -1;
}

