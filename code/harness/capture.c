// code/harness/capture.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
#ifndef EGL_PLATFORM_SURFACELESS_MESA
  #define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

static void egl_init(void) {
  typedef EGLDisplay (*pfn_getplat)(EGLenum, void *, const EGLint *);
  pfn_getplat get_platform = (pfn_getplat)(void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
  EGLDisplay dpy = EGL_NO_DISPLAY;
  if (get_platform)
    dpy = get_platform(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
  if (dpy == EGL_NO_DISPLAY)
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL))
    fatal("EGL: no display");
  eglBindAPI(EGL_OPENGL_API);
  EGLint cfg_attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
  EGLConfig cfg;
  EGLint ncfg = 0;
  eglChooseConfig(dpy, cfg_attr, &cfg, 1, &ncfg);
  EGLint ctx_attr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
                       EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                       EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, ncfg ? cfg : NULL, EGL_NO_CONTEXT, ctx_attr);
  if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx))
    fatal("EGL: context creation failed");
  gl_load_functions((gl_loader_fn)(void *)eglGetProcAddress);
}

static void fbo_init(int w, int h) {
  GLuint rb_col, rb_depth;
  glGenFramebuffers(1, &R.present_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, R.present_fbo);
  glGenRenderbuffers(1, &rb_col);
  glBindRenderbuffer(GL_RENDERBUFFER, rb_col);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb_col);
  glGenRenderbuffers(1, &rb_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, rb_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb_depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    fatal("FBO incomplete");
}

// PNG writer, no image library: the zlib stream inside IDAT uses deflate
// "stored" (uncompressed) blocks, so no compressor is needed — only CRC32
// and Adler32. Files are as large as raw RGB, fine for throwaway shots.

// Slicing-by-8: tables 1-7 are derived from table 0, eight bytes advance per
// iteration, and the 32-bit word is assembled from bytes (endian-neutral, no
// aliasing question). Bit-identical to the byte-at-a-time form, ~5x faster
// over the 2.8 MB IDAT a 720p frame produces.
static uint32_t png_crc(uint32_t crc, const uint8_t *buf, size_t len) {
  static uint32_t t[8][256];
  if (!t[0][1]) {
    for (uint32_t n = 0; n < 256; n++) {
      uint32_t c = n;
      for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      t[0][n] = c;
    }
    for (int s = 1; s < 8; s++)
      for (uint32_t n = 0; n < 256; n++)
        t[s][n] = t[0][t[s - 1][n] & 0xFF] ^ (t[s - 1][n] >> 8);
  }
  size_t i = 0;
  for (; i + 8 <= len; i += 8) {
    uint32_t lo = crc ^ ((uint32_t)buf[i] | (uint32_t)buf[i + 1] << 8 |
                         (uint32_t)buf[i + 2] << 16 | (uint32_t)buf[i + 3] << 24);
    crc = t[7][lo & 0xFF] ^ t[6][(lo >> 8) & 0xFF] ^ t[5][(lo >> 16) & 0xFF] ^
          t[4][lo >> 24] ^ t[3][buf[i + 4]] ^ t[2][buf[i + 5]] ^
          t[1][buf[i + 6]] ^ t[0][buf[i + 7]];
  }
  for (; i < len; i++)
    crc = t[0][(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
  return crc;
}

static void png_chunk(FILE *f, const char type[4], const uint8_t *data, uint32_t len) {
  uint8_t hdr[8] = {len >> 24, len >> 16, len >> 8, len,
                    type[0], type[1], type[2], type[3]};
  fwrite(hdr, 1, 8, f);
  if (len) fwrite(data, 1, len, f);
  uint32_t crc = png_crc(png_crc(0xFFFFFFFFu, hdr + 4, 4), data, len) ^ 0xFFFFFFFFu;
  uint8_t cb[4] = {crc >> 24, crc >> 16, crc >> 8, crc};
  fwrite(cb, 1, 4, f);
}

// Append one framebuffer image as top-down RGB24. Unlike the PNG writer this
// is a streaming capture primitive, so every allocation, GL read and row write
// is checked and reported to its caller.
static int write_rgb_frame(FILE *f, int w, int h) {
  if (!f || w <= 0 || h <= 0 || (size_t)w > SIZE_MAX / 3 ||
      (size_t)h > SIZE_MAX / ((size_t)w * 3)) return 0;
  size_t row = (size_t)w * 3;
  uint8_t *pix = malloc(row * (size_t)h);
  if (!pix) return 0;

  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  while (glGetError() != GL_NO_ERROR) {}
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pix);
  int ok = glGetError() == GL_NO_ERROR;
  for (int y = h - 1; ok && y >= 0; y--)
    if (fwrite(pix + (size_t)y * row, 1, row, f) != row) ok = 0;
  if (ferror(f)) ok = 0;
  free(pix);
  return ok;
}

static void write_png(const char *path, int w, int h) {
  size_t row = (size_t)w * 3;
  uint8_t *pix = malloc(row * (size_t)h);
  if (!pix) fatal("out of memory");
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pix);

  // Scanline stream: filter byte 0 + RGB per row, top-down (GL is bottom-up).
  size_t raw_len = (size_t)h * (row + 1);
  uint8_t *raw = malloc(raw_len);
  if (!raw) fatal("out of memory");
  for (int y = 0; y < h; y++) {
    uint8_t *dst = raw + (size_t)y * (row + 1);
    *dst = 0;
    memcpy(dst + 1, pix + (size_t)(h - 1 - y) * row, row);
  }
  free(pix);

  // zlib wrapper: header, stored blocks of <=65535 bytes, Adler32 of raw.
  size_t nblk = (raw_len + 65534) / 65535;
  if (!nblk) nblk = 1;
  uint8_t *z = malloc(2 + nblk * 5 + raw_len + 4);
  if (!z) fatal("out of memory");
  size_t zn = 0, off = 0;
  z[zn++] = 0x78; z[zn++] = 0x01;
  do {
    size_t blk = raw_len - off > 65535 ? 65535 : raw_len - off;
    z[zn++] = off + blk == raw_len;                 // BFINAL, BTYPE=00
    z[zn++] = blk & 0xFF;  z[zn++] = (blk >> 8) & 0xFF;
    z[zn++] = ~blk & 0xFF; z[zn++] = (~blk >> 8) & 0xFF;
    memcpy(z + zn, raw + off, blk);
    zn += blk;
    off += blk;
  } while (off < raw_len);
  // Adler32, deferred modulo (zlib's NMAX=5552: the largest block for which b
  // cannot overflow uint32) — arithmetically identical to the per-byte form
  // and ~9x faster over the 2.8 MB a 720p frame produces.
  uint32_t a = 1, b = 0;
  for (size_t i = 0; i < raw_len;) {
    size_t n = raw_len - i;
    if (n > 5552) n = 5552;
    for (size_t k = 0; k < n; k++) { a += raw[i + k]; b += a; }
    a %= 65521u;
    b %= 65521u;
    i += n;
  }
  free(raw);
  uint32_t adler = (b << 16) | a;
  z[zn++] = adler >> 24; z[zn++] = adler >> 16; z[zn++] = adler >> 8; z[zn++] = adler;

  FILE *f = fopen(path, "wb");
  if (!f) { fprintf(stderr, "cannot write %s\n", path); free(z); return; }
  static const uint8_t sig[8] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);
  uint8_t ihdr[13] = {(uint32_t)w >> 24, w >> 16, w >> 8, w,
                      (uint32_t)h >> 24, h >> 16, h >> 8, h,
                      8, 2, 0, 0, 0};                // 8-bit RGB, no interlace
  png_chunk(f, "IHDR", ihdr, 13);
  png_chunk(f, "IDAT", z, (uint32_t)zn);
  png_chunk(f, "IEND", NULL, 0);
  free(z);
  // The "shot" line is the success contract for scripted playtests — it must
  // not appear if any buffered write failed (disk full, quota, I/O error).
  int err = ferror(f);
  if (fclose(f) != 0) err = 1;
  if (err) fprintf(stderr, "cannot write %s\n", path);
  else     printf("shot %s\n", path);
}

