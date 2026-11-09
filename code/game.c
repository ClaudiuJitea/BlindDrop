// game.c — "BlindDrop": minimalist low-poly tactical FPS prototype, Phase 1.
//
// One C23 translation unit. TWO platform backends selected at compile time
// (#ifdef _WIN32), and THREE mutually exclusive runtime modes selected at
// main() — play / headless harness (--do/--script) / --server:
//   Windows (game.exe) : borderless fullscreen popup window + WGL core 3.3.
//                        No display-mode change and no exclusive fullscreen,
//                        so no HDR mode-switch black screens and alt-tab is a
//                        plain DWM window switch. First frame is rendered
//                        *before* the window is shown (no startup flash).
//   Linux   (game)     : every runtime mode in one binary.
//                        No arguments = play: X11 window (borderless
//                        fullscreen via the EWMH hint), EGL core 3.3, raw
//                        mouse through XInput2, sound through ALSA — all of
//                        libXi and libasound reached by dlopen so a missing
//                        one degrades instead of failing to start. This is
//                        what `make deploy` scp's to the handheld.
//                        --do/--script = the headless CLI harness: EGL
//                        surfaceless on Mesa llvmpipe, renders into an FBO,
//                        driven by a tiny script language (see usage() below)
//                        that can press keys, move the mouse, dump
//                        screenshots (PNG) and print movement traces. The
//                        modes fork mutually exclusively at main(), which is
//                        what keeps every proof command deterministic.
//                        --server = dedicated sim + UDP, no GL/audio/window,
//                        one process serving up to SRV_LOBBIES arenas on ONE
//                        port. Live client<->server multiplayer over UDP:
//                        Quick Join, prediction, interpolation — all in this
//                        one binary.
//
// The frame loop itself belongs to NEITHER backend: app_start/app_frame/
// app_sync (just above the platform split) hold everything between "here is dt
// and the input state" and "swap the buffers", because a second windowed
// backend is exactly how a tick accumulator and a press-latch rule end up
// existing twice with one of the copies subtly wrong.
//
// Shared core: config (config.cfg next to the executable), seeded procedural
// map (AABB solids), fixed-timestep player sim (120 Hz) with render
// interpolation, GL 3.3 core renderer (one static VBO for the world, one
// dynamic VBO for the HUD), and the "GRATICULE" UI: a procedural SDF
// typeface (Hershey-simplex skeletons baked to a distance atlas at GL init)
// plus rounded-panel/capsule/ruler primitives, all in the one HUD batch.
//
// C23 gotcha (verified in-container): MinGW-w64 stddef.h lacks unreachable();
// constexpr/nullptr/typed enums work on both compilers. #embed needs GCC 15.
//
// THE SOURCE TREE: ONE TRANSLATION UNIT, MANY FILES (a "unity build").
// One TU is a deliberate constraint, not an accident: every function stays
// `static`, there are no headers to keep in sync, and the compiler sees the
// whole program exactly as it did when this was one 52k-line file. The parts
// live in code/<area>/*.c and are #included at the END of this header, in the
// one order C forces — a helper is defined above its first use. That makes the
// include list below the table of contents AND a hard rule:
//
//   * Never compile a part on its own, and never reorder the #include lines
//     casually — a part may use anything from the parts above it, and nothing
//     from the parts below it unless a forward declaration exists above.
//   * A new part goes where its first caller can see it; add its #include line
//     here and nowhere else (the Makefile picks up code/*/*.c by itself).
//   * The split itself moved no code: the parts concatenated in include order
//     are byte-for-byte the old game.c, and the binary was md5-identical.
//
//   core/      GL loader (GL_FUNC_LIST), math, PCG32 streams, config tables,
//              config I/O, the platform hooks the shared core calls down to
//   input/     gamepad vocabulary + processing (backends only FILL g_pad)
//   world/     procedural map, the solid index, handmade/tactical maps,
//              prefabs, the small maps' dressing kit
//   sim/       events, hitscan, player sim (120 Hz), locomotion, bots +
//              deathmatch, game modes, Hostage Rescue, ballistics, grid
//              pathing, the weapon (ONE gun_shoot for whoever pulls it)
//   anim/      skeleton locomotion, the hands' weapon-handling state
//   audio/     synthesis, the confirmation matrix, sound recipes, the embedded
//              samples (assets/sfx/pack.bin) that replace most of them, routing
//   net/       wire layer (hostile input, quantization), live connections
//   render/    renderer, atmospheres, HDR post chain, VFX, corpses, scene
//              batch, decor, town dressing, props, the per-frame passes
//   figure/    third-person bodies: skinning, weapon models (models.h),
//              figure builders, kit, carried weapons, figure draw
//   ui/        HARDLINE typeface, UI batch, settings menu, boot screen, home,
//              the Hostage Rescue HUD
//   app/       app_start/app_frame/app_sync: the loop shared by backends
//   harness/   (Linux) geometry checks, capture, audio analysis, the proof
//              commands, the script interpreter
//   platform/  linux.c (X11/EGL/XInput2/ALSA), server.c (dedicated server and
//              main), win32.c (WGL, raw input, WASAPI)
//
// Inside each part the old banner comments are unchanged and still searchable.
//
// The machine-checked proof commands live in the harness; ./build/game --help
// is their command reference, and each one's own comment at its implementation
// names the defect it exists to catch.

#define _DEFAULT_SOURCE  // readlink/strdup despite -std=c23 strict mode

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>   // SPSC sound-trigger ring, game thread -> audio thread
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #define DIRECTINPUT_VERSION 0x0800
  #include <dinput.h>
  #include <mmsystem.h>   // timeBeginPeriod for the frame limiter
  #include <mmreg.h>      // WAVEFORMATEXTENSIBLE
  #include <objbase.h>    // CoInitializeEx & friends (LEAN_AND_MEAN drops them)
  #include <mmdeviceapi.h>
  #include <audioclient.h>
  #include <GL/gl.h>
#else
  #include <sys/stat.h>
  #include <unistd.h>
  #include <GL/gl.h>
  #include <EGL/egl.h>
#endif

#ifndef APIENTRY
  #define APIENTRY
#endif

// Fatal errors must be visible even in the -mwindows build (no console).
static void fatal(const char *msg) {
#ifdef _WIN32
  MessageBoxA(NULL, msg, "BlindDrop", MB_ICONERROR);
#endif
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

#include "core/gl_loader.c"
#include "core/math.c"
#include "core/rng.c"
#include "core/config.c"
#include "input/gamepad.c"
#include "core/config_io.c"
#include "core/platform_api.c"
#include "world/map.c"
#include "world/solid_index.c"
#include "world/maps.c"
#include "world/prefabs.c"
#include "world/dressing.c"
#include "sim/events.c"
#include "sim/hitscan.c"
#include "audio/synth.c"
#include "audio/confirm.c"
#include "audio/sounds.c"
#include "audio/samples.c"
#include "audio/routing.c"
#include "sim/player.c"
#include "net/wire.c"
#include "sim/locomotion.c"
#include "anim/skeleton.c"
#include "sim/bots.c"
#include "sim/modes.c"
#include "sim/rescue.c"
#include "sim/ballistics.c"
#include "anim/hands.c"
#include "sim/nav.c"
#include "sim/rescue_guard.c"
#include "sim/rescue_mission.c"
#include "sim/weapon.c"
#include "net/conn.c"
#include "render/renderer.c"
#include "render/atmos.c"
#include "render/post.c"
#include "ui/font.c"
#include "ui/batch.c"
#include "ui/menu.c"
#include "ui/boot_menu.c"
#include "render/vfx.c"
#include "render/corpses.c"
#include "render/scene_batch.c"
#include "render/decor.c"
#include "render/town.c"
#include "render/props.c"
#include "figure/skin.c"
#include "figure/weapons.c"
#include "figure/builder.c"
#include "figure/draw.c"
#include "render/rescue_world.c"
#include "render/fx_pass.c"
#include "render/viewmodel.c"
#include "render/passes.c"
#include "ui/home.c"
#include "render/targets.c"
#include "ui/rescue_hud.c"
#include "render/frame.c"
#include "app/app.c"
// ===========================================================================
// Linux backend — native window (X11 + EGL) OR the headless CLI harness.
// ===========================================================================
#ifndef _WIN32
#include "harness/geometry_checks.c"
#include "harness/capture.c"
#include "harness/audio_analysis.c"
#include "harness/proofs.c"
#include "harness/script.c"
#include "platform/linux.c"
#include "platform/server.c"
// ===========================================================================
// Windows backend — borderless fullscreen popup + WGL 3.3 core, raw mouse
// input, config-driven binds, vsync via wglSwapIntervalEXT.
// ===========================================================================
#else  // _WIN32
#include "platform/win32.c"
#endif  // _WIN32
