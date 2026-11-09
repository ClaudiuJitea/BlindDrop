// code/core/gl_loader.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// ---------------------------------------------------------------------------
// GL 3.3 core loader — only what we use beyond the GL 1.1 headers.
// Pointer names match the spec names; nothing here collides with the 1.1
// prototypes in either platform's GL/gl.h (glActiveTexture is avoided: we
// only ever use texture unit 0, which is active by default).
// ---------------------------------------------------------------------------

#ifdef _WIN32  // Linux Mesa gl.h pulls in glext.h which already has these
#include <stddef.h>
typedef char      GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef uint64_t  GLuint64;
typedef struct __GLsync *GLsync;
#endif

#define GL_FRAGMENT_SHADER      0x8B30
#define GL_VERTEX_SHADER        0x8B31
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_ARRAY_BUFFER         0x8892
#define GL_STATIC_DRAW          0x88E4
#define GL_STREAM_DRAW          0x88E0
#define GL_FRAMEBUFFER          0x8D40
#define GL_RENDERBUFFER         0x8D41
#define GL_COLOR_ATTACHMENT0    0x8CE0
#define GL_DEPTH_ATTACHMENT     0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_DEPTH_COMPONENT24    0x81A6
#define GL_READ_FRAMEBUFFER     0x8CA8
#define GL_DRAW_FRAMEBUFFER     0x8CA9
#define GL_MAX_SAMPLES          0x8D57
#define GL_CLAMP_TO_EDGE        0x812F
#define GL_R8                   0x8229
#define GL_RED_CORE             0x1903
#define GL_DEPTH_COMPONENT      0x1902
#define GL_CLAMP_TO_BORDER      0x812D
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_SYNC_FLUSH_COMMANDS_BIT    0x00000001
// The HDR pipeline (scene_update / post_pass). Guarded: Mesa's glext.h already
// defines these on Linux, MinGW's 1.1 gl.h does not.
#ifndef GL_RGBA16F
#define GL_RGBA16F              0x881A
#endif
#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT           0x140B
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0             0x84C0
#endif
#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL    0x813D
#endif
#ifndef GL_RG16F
#define GL_RG16F                0x822F
#endif
#ifndef GL_RG
#define GL_RG                   0x8227
#endif
#ifndef GL_R16F
#define GL_R16F                 0x822D
#endif

#define GL_FUNC_LIST(X) \
  X(GLuint, glCreateShader, (GLenum)) \
  X(void,   glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
  X(void,   glCompileShader, (GLuint)) \
  X(void,   glGetShaderiv, (GLuint, GLenum, GLint *)) \
  X(void,   glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
  X(GLuint, glCreateProgram, (void)) \
  X(void,   glAttachShader, (GLuint, GLuint)) \
  X(void,   glLinkProgram, (GLuint)) \
  X(void,   glGetProgramiv, (GLuint, GLenum, GLint *)) \
  X(void,   glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
  X(void,   glDeleteShader, (GLuint)) \
  X(void,   glUseProgram, (GLuint)) \
  X(GLint,  glGetUniformLocation, (GLuint, const GLchar *)) \
  X(void,   glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat *)) \
  X(void,   glUniform1i, (GLint, GLint)) \
  X(void,   glUniform1f, (GLint, GLfloat)) \
  X(void,   glUniform2f, (GLint, GLfloat, GLfloat)) \
  X(void,   glUniform3f, (GLint, GLfloat, GLfloat, GLfloat)) \
  X(void,   glUniform3fv, (GLint, GLsizei, const GLfloat *)) \
  X(void,   glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
  X(void,   glGenVertexArrays, (GLsizei, GLuint *)) \
  X(void,   glBindVertexArray, (GLuint)) \
  X(void,   glGenBuffers, (GLsizei, GLuint *)) \
  X(void,   glBindBuffer, (GLenum, GLuint)) \
  X(void,   glBufferData, (GLenum, GLsizeiptr, const void *, GLenum)) \
  X(void,   glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
  X(void,   glEnableVertexAttribArray, (GLuint)) \
  X(void,   glGenFramebuffers, (GLsizei, GLuint *)) \
  X(void,   glBindFramebuffer, (GLenum, GLuint)) \
  X(void,   glGenRenderbuffers, (GLsizei, GLuint *)) \
  X(void,   glBindRenderbuffer, (GLenum, GLuint)) \
  X(void,   glRenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei)) \
  X(void,   glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) \
  X(void,   glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
  X(GLenum, glCheckFramebufferStatus, (GLenum)) \
  X(void,   glDeleteFramebuffers, (GLsizei, const GLuint *)) \
  X(void,   glDeleteRenderbuffers, (GLsizei, const GLuint *)) \
  X(void,   glRenderbufferStorageMultisample, (GLenum, GLsizei, GLenum, GLsizei, GLsizei)) \
  X(void,   glBlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
  X(GLsync, glFenceSync, (GLenum, GLbitfield)) \
  X(GLenum, glClientWaitSync, (GLsync, GLbitfield, GLuint64)) \
  X(void,   glDeleteSync, (GLsync)) \
  X(void,   glGenerateMipmap, (GLenum)) \
  X(void,   glGenSamplers, (GLsizei, GLuint *)) \
  X(void,   glBindSampler, (GLuint, GLuint)) \
  X(void,   glSamplerParameteri, (GLuint, GLenum, GLint)) \
  X(void,   glUniformMatrix3fv, (GLint, GLsizei, GLboolean, const GLfloat *)) \
  X(void,   glUniform4fv, (GLint, GLsizei, const GLfloat *))

#define X(ret, name, args) typedef ret (APIENTRY *pfn_##name) args; static pfn_##name name;
GL_FUNC_LIST(X)
#undef X

typedef void *(*gl_loader_fn)(const char *);

// glActiveTexture is GL 1.3, so Mesa's gl.h already PROTOTYPES it and a static
// pointer of the same name would collide. It is loaded under its own name. The
// HDR post pipeline is the only multi-texture consumer; everything else still
// lives on unit 0, and every multi-unit call site hands unit 0 back.
typedef void (APIENTRY *pfn_active_texture)(GLenum);
static pfn_active_texture gl_active_texture;
#define GL_TIME_ELAPSED_Q          0x88BF
#define GL_QUERY_RESULT_Q          0x8866
#define GL_QUERY_RESULT_AVAILABLE_Q 0x8867
typedef void (APIENTRY *pfn_gen_queries)(GLsizei, GLuint *);
typedef void (APIENTRY *pfn_begin_query)(GLenum, GLuint);
typedef void (APIENTRY *pfn_end_query)(GLenum);
typedef void (APIENTRY *pfn_query_iv)(GLuint, GLenum, GLint *);
typedef void (APIENTRY *pfn_query_u64)(GLuint, GLenum, GLuint64 *);
static pfn_gen_queries gl_gen_queries;
static pfn_begin_query gl_begin_query;
static pfn_end_query gl_end_query;
static pfn_query_iv gl_query_iv;
static pfn_query_u64 gl_query_u64;
static void gl_load_functions(gl_loader_fn load) {
#define X(ret, name, args) name = (pfn_##name)load(#name); \
  if (!name) fatal("GL 3.3 function missing: " #name);
  GL_FUNC_LIST(X)
#undef X
  gl_active_texture = (pfn_active_texture)load("glActiveTexture");
  if (!gl_active_texture) fatal("GL 3.3 function missing: glActiveTexture");
  // Timer queries: core in 3.3, but loaded OPTIONALLY — they only feed the
  // PERF overlay, and a driver that lacks them must still run the game.
  gl_gen_queries = (pfn_gen_queries)load("glGenQueries");
  gl_begin_query = (pfn_begin_query)load("glBeginQuery");
  gl_end_query = (pfn_end_query)load("glEndQuery");
  gl_query_iv = (pfn_query_iv)load("glGetQueryObjectiv");
  gl_query_u64 = (pfn_query_u64)load("glGetQueryObjectui64v");
}
// Bind `tex` on texture unit `unit`, then return to unit 0.
static void tex_on(int unit, GLuint tex) {
  gl_active_texture(GL_TEXTURE0 + (GLenum)unit);
  glBindTexture(GL_TEXTURE_2D, tex);
  gl_active_texture(GL_TEXTURE0);
}

