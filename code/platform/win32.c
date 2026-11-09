// code/platform/win32.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.

#define WGL_CONTEXT_MAJOR_VERSION_ARB    0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB    0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB     0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001

typedef HGLRC (WINAPI *pfn_wglCreateContextAttribsARB)(HDC, HGLRC, const int *);
typedef BOOL  (WINAPI *pfn_wglSwapIntervalEXT)(int);

// --- UDP socket backend, Winsock half(-lws2_32) ---
#include <winsock2.h>
#include <ws2tcpip.h>
// Winsock has to be up before ANY ws2_32 call, and getaddrinfo is one of them.
// This lived inside net_udp_open, which reads as "init before the first socket"
// and is wrong by one call: the client's first network operation is a RESOLVE,
// not a socket. start_connect resolves mp_host before anything is opened, so on
// Windows getaddrinfo returned WSANOTINITIALISED, net_resolve returned 0,
// start_connect returned 0 without touching g_cl, and QUICK JOIN sat at OFFLINE
// having sent not one packet — while every Linux client connected fine, because
// getaddrinfo needs no initialization there. One initializer, and every entry
// into this backend calls it.
static int net_wsa_init(void) {
  static int inited = 0;
  if (!inited) { WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 0; inited = 1; }
  return 1;
}
static int net_udp_open(int port) {
  if (!net_wsa_init()) return -1;
  SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == INVALID_SOCKET) return -1;
  u_long nb = 1;
  ioctlsocket(s, FIONBIO, &nb);                // non-blocking
  if (port > 0) {
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0) { closesocket(s); return -1; }
  }
  return (int)s;
}
static void net_udp_close(int fd) { if (fd >= 0) closesocket((SOCKET)fd); }
static int net_udp_send(int fd, uint32_t ip, uint16_t port, const void *b, int n) {
  struct sockaddr_in a = {0};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = ip;
  a.sin_port = htons(port);
  return sendto((SOCKET)fd, b, n, 0, (struct sockaddr *)&a, sizeof a);
}
static int net_udp_recv(int fd, uint32_t *ip, uint16_t *port, void *b, int cap) {
  struct sockaddr_in a = {0};
  int al = sizeof a;
  int r = recvfrom((SOCKET)fd, b, cap, 0, (struct sockaddr *)&a, &al);
  if (r == SOCKET_ERROR) return (WSAGetLastError() == WSAEWOULDBLOCK) ? 0 : -1;
  if (ip) *ip = a.sin_addr.s_addr;
  if (port) *port = ntohs(a.sin_port);
  return r;
}
static uint32_t net_resolve(const char *host) {
  if (!net_wsa_init()) return 0;             // getaddrinfo IS a ws2_32 call
  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) return 0;
  uint32_t out = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
  freeaddrinfo(res);
  return out;
}
static uint32_t net_now_ms(void) { return (uint32_t)GetTickCount(); }
static int net_udp_bcast_on(int fd) {
  BOOL on = TRUE;
  return setsockopt((SOCKET)fd, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof on) == 0;
}
static int net_bcast_targets(uint32_t *out, int max) {
  int n = 0;
  if (n < max) out[n++] = htonl(INADDR_BROADCAST);
  if (n < max) out[n++] = htonl(INADDR_LOOPBACK);
  return n;
}
static int net_local_addrs(uint32_t *out, int max) { (void)out; (void)max; return 0; }
static int host_spawn(char *const argv[]) { (void)argv; return 0; }   // HOST_SUPPORTED 0
static int host_alive(void) { return 0; }
static void host_kill(void) {}

// g_keys/g_pressed are indexed by VK code, which IS the shared KEY_* space —
// see the key layer above the frame step. Nothing here maps anything.
static float g_mouse_dx, g_mouse_dy;
static int   g_active, g_running = 1;
static HWND  g_hwnd;

static void win_clip_cursor(void) {
  RECT r;
  GetWindowRect(g_hwnd, &r);
  ClipCursor(&r);
}

static void win_set_cursor_visible(int visible) {
  static int shown = 1;
  if (visible == shown) return;
  shown = visible;
  ShowCursor(visible);
}

static pfn_wglSwapIntervalEXT g_swap_interval;

static void platform_vsync_changed(void) {
  if (g_swap_interval) g_swap_interval(g_cfg.vsync);
}

// --- Gamepad via XInput + DirectInput8 -------------------------------------
// Two adapters, tried in that order; both feed the shared mappers and neither
// writes g_pad on a failed read, so stale state cannot linger.
//   XInput      — every Xbox-protocol pad (USB, 2.4G dongles, Xbox Bluetooth).
//   DirectInput — the inbox path for everything else over Bluetooth/HID:
//                 8BitDo in D-input mode (the Ultimate 2 family's only
//                 Bluetooth protocol), DualShock, Switch Pro.
// GameInput v0 sat in front of this stack for two days in 2026-08 and was
// removed for cause: the inbox service only promotes devices with
// registry/redistributable mappings to gamepad readings, so a generic
// Bluetooth HID pad NEVER answers GetGamepadState — for Xbox-protocol pads it
// only duplicated XInput. WinMM's joystick facade is gone with it: its fixed
// axis guesses (dwUpos on an axis the 8BitDo does not report) were what
// pinned the look at full deflection when the mismatched-size DirectInput
// read failed and the game fell through.

// DirectInput 8, dynamically loaded. The data format is hand-declared like
// every other ABI in this file: dinput8.dll exports no data-format OBJECTS
// (MinGW ships them as static data in libdinput8.a, but that puts a new
// library on the link line for one 80-byte table), and the undocumented
// GetdfDIJoystick export returns the 80-byte DIJOYSTATE format, which
// GetDeviceState then rejects for any other buffer size (exact-size
// DIERR_INVALIDPARAM — the silent mismatch that killed this path before).
// Our format targets our own struct: the eight GUID-slotted axes in PH_
// order, one POV, 32 buttons.
typedef struct { LONG ax[PH_AXES]; DWORD pov; BYTE btn[32]; } win_di_state_t;
static_assert(sizeof(win_di_state_t) == 68, "DirectInput state layout");

// GUID_XAxis..GUID_Slider and GUID_POV by value (no dxguid import); the axis
// family differs only in Data1, and the array index IS the PH_ slot.
#define WIN_DI_GUID(d1) \
  {d1, 0xC9F3, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}}
static const GUID WIN_DI_AXIS_GUID[PH_AXES] = {
  WIN_DI_GUID(0xA36D02E0), WIN_DI_GUID(0xA36D02E1),   // X   Y
  WIN_DI_GUID(0xA36D02E2), WIN_DI_GUID(0xA36D02F4),   // Z   Rx
  WIN_DI_GUID(0xA36D02F5), WIN_DI_GUID(0xA36D02E3),   // Ry  Rz
  WIN_DI_GUID(0xA36D02E4), WIN_DI_GUID(0xA36D02E4),   // Slider x2
};
static const GUID WIN_DI_POV_GUID = WIN_DI_GUID(0xA36D02F2);
static const IID WIN_DI_IID_8W =
  {0xBF798031, 0x483A, 0x4DA2, {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00}};
typedef HRESULT (WINAPI *pfn_DirectInput8Create)(HINSTANCE, DWORD, REFIID,
                                                 LPVOID *, LPUNKNOWN);

static HMODULE g_dinput_module;
static LPDIRECTINPUT8W g_dinput;
static LPDIRECTINPUTDEVICE8W g_dinput_device;
static DIOBJECTDATAFORMAT g_di_odf[PH_AXES + 1 + 32];
static uint32_t g_di_axes;      // presence mask over PH_, from EnumObjects
static int      g_di_has_pov;
static uint16_t g_di_vid;       // keys the shared button-layout table
static int      g_di_live;      // a real input report has been seen
static int      g_di_fail;      // consecutive dead reads -> re-enumerate
static double   g_di_rescan;    // next EnumDevices while no device is open

static void win_di_format_init(void) {
  int n = 0;
  for (int i = 0; i < PH_AXES; i++)
    g_di_odf[n++] = (DIOBJECTDATAFORMAT){
      &WIN_DI_AXIS_GUID[i], (DWORD)offsetof(win_di_state_t, ax[i]),
      DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION};
  g_di_odf[n++] = (DIOBJECTDATAFORMAT){
    &WIN_DI_POV_GUID, (DWORD)offsetof(win_di_state_t, pov),
    DIDFT_POV | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0};
  for (int i = 0; i < 32; i++)
    g_di_odf[n++] = (DIOBJECTDATAFORMAT){
      NULL, (DWORD)offsetof(win_di_state_t, btn[i]),
      DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0};
}

// Present axes get an explicit symmetric DIPROP_RANGE so the pump's /32767 is
// exact by construction — DirectInput's default 0..65535 scale plus a
// first-sample "calibration" is what turned a late first Bluetooth report
// into a permanently pinned look axis. Windows' own per-axis dead zone is
// zeroed; the game runs its one radial deadzone in shared code.
static BOOL CALLBACK win_dinput_object(LPCDIDEVICEOBJECTINSTANCEW o, LPVOID ctx) {
  (void)ctx;
  if (o->dwType & DIDFT_POV) { g_di_has_pov = 1; return DIENUM_CONTINUE; }
  if (!(o->dwType & DIDFT_AXIS)) return DIENUM_CONTINUE;
  int ph = -1;
  for (int i = 0; i < PH_AXES; i++)
    if (!memcmp(&o->guidType, &WIN_DI_AXIS_GUID[i], sizeof(GUID))) { ph = i; break; }
  if (ph == PH_S0 && (g_di_axes & 1u << PH_S0)) ph = PH_S1;  // second slider
  if (ph < 0 || (g_di_axes & 1u << ph)) return DIENUM_CONTINUE;
  DIPROPRANGE range = {
    .diph = {sizeof range, sizeof range.diph, o->dwType, DIPH_BYID},
    .lMin = -32767, .lMax = 32767,
  };
  if (FAILED(g_dinput_device->lpVtbl->SetProperty(g_dinput_device, DIPROP_RANGE,
                                                  &range.diph)))
    return DIENUM_CONTINUE;   // axis refuses the range: treat as absent
  DIPROPDWORD dz = {
    .diph = {sizeof dz, sizeof dz.diph, o->dwType, DIPH_BYID}, .dwData = 0,
  };
  g_dinput_device->lpVtbl->SetProperty(g_dinput_device, DIPROP_DEADZONE, &dz.diph);
  g_di_axes |= 1u << ph;
  return DIENUM_CONTINUE;
}

static BOOL CALLBACK win_dinput_pick(LPCDIDEVICEINSTANCEW inst, LPVOID ctx) {
  (void)ctx;
  LPDIRECTINPUTDEVICE8W dev = NULL;
  if (FAILED(g_dinput->lpVtbl->CreateDevice(g_dinput, &inst->guidInstance,
                                            &dev, NULL)))
    return DIENUM_CONTINUE;
  g_dinput_device = dev;
  // A HID product GUID carries MAKELONG(vid, pid) in Data1 and literally
  // spells "PIDVID" in its tail; anything else gets the generic layout.
  g_di_vid = memcmp(&inst->guidProduct.Data4[2], "PIDVID", 6) == 0
           ? (uint16_t)inst->guidProduct.Data1 : 0;
  return DIENUM_STOP;
}

static void win_dinput_close(double now) {
  if (g_dinput_device) {
    g_dinput_device->lpVtbl->Unacquire(g_dinput_device);
    g_dinput_device->lpVtbl->Release(g_dinput_device);
  }
  g_dinput_device = NULL;
  g_di_axes = 0;
  g_di_has_pov = g_di_live = g_di_fail = 0;
  g_di_vid = 0;
  g_di_rescan = now + 2.0;
}

static void win_dinput_scan(double now) {
  if (!g_dinput || g_dinput_device) return;
  if (FAILED(g_dinput->lpVtbl->EnumDevices(g_dinput, DI8DEVCLASS_GAMECTRL,
                                           win_dinput_pick, NULL,
                                           DIEDFL_ATTACHEDONLY)) ||
      !g_dinput_device)
    return;
  DIDATAFORMAT fmt = {
    sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT), DIDF_ABSAXIS,
    sizeof(win_di_state_t), sizeof g_di_odf / sizeof g_di_odf[0], g_di_odf,
  };
  // Properties only apply while unacquired, hence the order; a device that
  // fails any step is closed and the 2 s rescan finds the next one.
  if (FAILED(g_dinput_device->lpVtbl->SetDataFormat(g_dinput_device, &fmt)) ||
      FAILED(g_dinput_device->lpVtbl->SetCooperativeLevel(
        g_dinput_device, g_hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)) ||
      FAILED(g_dinput_device->lpVtbl->EnumObjects(g_dinput_device,
        win_dinput_object, NULL, DIDFT_AXIS | DIDFT_POV)) ||
      FAILED(g_dinput_device->lpVtbl->Acquire(g_dinput_device)))
    win_dinput_close(now);
}

static int win_dinput_pump(pad_state_t *out, double now) {
  if (!g_dinput) return 0;
  if (!g_dinput_device) {
    // A Bluetooth pad usually connects AFTER launch; enumerate on the same
    // 2 s cadence as the XInput slot scan and the evdev node scan.
    if (now < g_di_rescan) return 0;
    g_di_rescan = now + 2.0;
    win_dinput_scan(now);
    if (!g_dinput_device) return 0;
  }
  win_di_state_t st;
  g_dinput_device->lpVtbl->Poll(g_dinput_device);  // DI_NOEFFECT on most pads
  HRESULT hr = g_dinput_device->lpVtbl->GetDeviceState(g_dinput_device,
                                                       sizeof st, &st);
  if (FAILED(hr)) {
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
      g_dinput_device->lpVtbl->Acquire(g_dinput_device);
    // A pad that stays dead through the re-Acquire was turned off; it
    // re-pairs as a NEW HID instance, so drop this one and re-enumerate
    // instead of retrying a stale instance GUID forever.
    if (++g_di_fail > 240) win_dinput_close(now);
    return 0;
  }
  g_di_fail = 0;
  // A freshly acquired Bluetooth device answers DI_OK with an all-zero
  // struct until its first input report lands (SDL #3816); latching that as
  // real state was the old first-sample-calibration bug. With the symmetric
  // range a zero AXIS is genuinely centred — the tell is the POV, whose
  // CENTRED value keeps 0xFFFF in the low word. Hold the pad "absent" until
  // any byte proves a report arrived. One real report is byte-identical to
  // the stale pattern — dpad held exactly north with everything else at
  // rest — so that single press is swallowed until release; accepted trade,
  // the alternative is buffered GetDeviceData plumbing.
  if (!g_di_live) {
    if (!g_di_has_pov) g_di_live = 1;   // povless: all-zero IS neutral
    const unsigned char *p = (const unsigned char *)&st;
    for (size_t i = 0; i < sizeof st && !g_di_live; i++) g_di_live = p[i] != 0;
    if (!g_di_live) return 0;
  }
  pad_hid_raw_t raw = {
    .connected = 1, .axes = g_di_axes, .vid = g_di_vid,
    .pov = g_di_has_pov && (st.pov & 0xFFFFu) != 0xFFFFu
         ? (int)(st.pov & 0xFFFFu) : -1,
  };
  for (int i = 0; i < PH_AXES; i++)   // absent slots stay 0 for the mapper
    if (g_di_axes & 1u << i)
      raw.ax[i] = f_clamp((float)st.ax[i] / 32767.0f, -1.0f, 1.0f);
  for (int i = 0; i < 32; i++)
    if (st.btn[i] & 0x80) raw.buttons |= 1u << i;
  pad_apply_hid(out, &raw);
  return 1;
}

static void win_dinput_load(double now) {
  // System32 only: a bare LoadLibrary also searches the exe folder, so a DLL
  // dropped beside a downloaded game would run as us. 0x800 is
  // LOAD_LIBRARY_SEARCH_SYSTEM32, spelled out to need no _WIN32_WINNT bump.
  HMODULE m = LoadLibraryExA("dinput8.dll", NULL, 0x00000800);
  if (!m) return;
  pfn_DirectInput8Create create = (pfn_DirectInput8Create)(void *)
    GetProcAddress(m, "DirectInput8Create");
  if (!create ||
      FAILED(create(GetModuleHandleA(NULL), DIRECTINPUT_VERSION, &WIN_DI_IID_8W,
                    (LPVOID *)&g_dinput, NULL)) || !g_dinput) {
    g_dinput = NULL;
    FreeLibrary(m);
    return;
  }
  g_dinput_module = m;
  win_di_format_init();
  win_dinput_scan(now);
}

static void win_dinput_shutdown(void) {
  win_dinput_close(0.0);
  if (g_dinput) g_dinput->lpVtbl->Release(g_dinput);
  if (g_dinput_module) FreeLibrary(g_dinput_module);
  g_dinput = NULL;
  g_dinput_module = NULL;
}

// XInput is reached by LoadLibrary so the build keeps zero new link-time
// dependencies and a machine without either controller DLL simply has no pad.
// The struct is ABI from xinput.h, hand-declared like the XI2 and WASAPI ones.
typedef struct {
  WORD  wButtons;
  BYTE  bLeftTrigger, bRightTrigger;
  SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} win_xi_gamepad_t;
typedef struct { DWORD dwPacketNumber; win_xi_gamepad_t Gamepad; } win_xi_state_t;
static_assert(sizeof(win_xi_state_t) == 16, "XINPUT_STATE ABI");
static_assert(sizeof(win_xi_gamepad_t) == sizeof(pad_xinput_raw_t), "XINPUT raw sample ABI");
typedef DWORD (WINAPI *pfn_XInputGetState)(DWORD, win_xi_state_t *);
static pfn_XInputGetState g_xi_get_state;
static int      g_xi_slot = -1;
static double   g_xi_rescan;
static unsigned g_xi_live;      // bit s = slot s answered on the last scan

static void win_pad_load(void) {
  win_dinput_load(0.0);
  HMODULE m = LoadLibraryExA("xinput1_4.dll", NULL, 0x00000800);   // Win8+
  if (!m) m = LoadLibraryExA("xinput9_1_0.dll", NULL, 0x00000800); // OS-shipped fallback
  if (m) g_xi_get_state = (pfn_XInputGetState)(void *)GetProcAddress(m, "XInputGetState");
}

// IS ANYTHING BEING PRESSED ON THIS PAD. Levels, not edges — which is all
// XInput offers and all the hand-over below needs, since it only ever asks "is
// the pad I am reading idle while another one is not". Half a thumb's travel is
// well past any drift, so a pad on a table cannot answer yes.
static int win_xi_busy(const win_xi_gamepad_t *g) {
  const SHORT DZ = 16384;
  return g->wButtons != 0 || g->bLeftTrigger > 128 || g->bRightTrigger > 128 ||
         g->sThumbLX > DZ || g->sThumbLX < -DZ ||
         g->sThumbLY > DZ || g->sThumbLY < -DZ ||
         g->sThumbRX > DZ || g->sThumbRX < -DZ ||
         g->sThumbRY > DZ || g->sThumbRY < -DZ;
}

// THE SAME DEFECT THE EVDEV PUMP HAD, MIRRORED: this used to latch the FIRST
// slot that answered and keep it for the session, so a machine with two pads
// connected — a handheld's built-in controls plus an 8BitDo on its 2.4 GHz
// dongle, the exact case measured on the deploy device — played whichever one
// Windows happened to enumerate first and ignored the one in the player's
// hands. "First slot" and "last event minor" are the same guess wearing
// different clothes, and neither knows anything about hands.
//
// XInput makes the fix smaller than the Linux one, because a slot IS a physical
// pad (there are no virtual twins to thrash between) and every read is a whole
// state rather than an event stream. So: keep reading the active slot, and only
// while it is IDLE look for another slot that is not. A pad being used is never
// interrupted, and picking up the other pad takes one press.
//
// NOTE: compile-checked only — this container cannot run the Windows build.
static int win_xinput_pump(pad_state_t *out, double now) {
  if (!g_xi_get_state) return 0;
  win_xi_state_t st;
  // Empty XInput slots are expensive to poll (a device enumeration inside the
  // call), so which slots EXIST is refreshed on a 2 s timer rather than every
  // frame. Reading a slot that does exist is cheap, which is what makes the
  // per-frame hand-over search below affordable.
  if (now >= g_xi_rescan) {
    g_xi_rescan = now + 2.0;
    g_xi_live = 0;
    for (DWORD i = 0; i < 4; i++)
      if (g_xi_get_state(i, &st) == 0 /* ERROR_SUCCESS */) g_xi_live |= 1u << i;
    if (g_xi_slot >= 0 && !(g_xi_live & (1u << g_xi_slot))) g_xi_slot = -1;
    if (g_xi_slot < 0)
      for (int i = 0; i < 4; i++)
        if (g_xi_live & (1u << i)) { g_xi_slot = i; break; }
  }
  if (g_xi_slot < 0) return 0;
  if (g_xi_get_state((DWORD)g_xi_slot, &st) != 0) {
    g_xi_slot = -1;
    g_xi_live = 0;
    g_xi_rescan = now + 2.0;
    return 0;
  }
  // HAND-OVER: only while the active pad is idle, and only to a slot that is
  // not. Gating on idle is what keeps a pad in use from being taken away
  // mid-turn, and it is also what keeps the extra reads off the hot path.
  if (!win_xi_busy(&st.Gamepad)) {
    for (int i = 0; i < 4; i++) {
      if (i == g_xi_slot || !(g_xi_live & (1u << i))) continue;
      win_xi_state_t o;
      if (g_xi_get_state((DWORD)i, &o) != 0) { g_xi_live &= ~(1u << i); continue; }
      if (win_xi_busy(&o.Gamepad)) { g_xi_slot = i; st = o; break; }
    }
  }
  const win_xi_gamepad_t *g = &st.Gamepad;
  // SHORT is -32768..32767; pad_apply_xinput clamps the one-step negative
  // overshoot. XInput Y is already up-positive, matching the shared contract.
  pad_apply_xinput(out, &(pad_xinput_raw_t){
    .buttons = g->wButtons, .lt = g->bLeftTrigger, .rt = g->bRightTrigger,
    .lx = g->sThumbLX, .ly = g->sThumbLY,
    .rx = g->sThumbRX, .ry = g->sThumbRY,
  });
  return 1;
}

// One pad, first source that answers wins. Each pump fully rebuilds the
// state through the shared mappers on success and leaves g_pad untouched on
// failure, so a dead read can never freeze a stale button.
static void win_pad_pump(double now) {
  if (win_xinput_pump(&g_pad, now)) return;
  if (win_dinput_pump(&g_pad, now)) return;
  memset(&g_pad, 0, sizeof g_pad);
}

// --- WASAPI output: shared mode, event-driven, ~20 ms buffer. -------------
// COM in plain C: GUIDs defined by value (no -luuid), vtbl calls throughout.
// Every failure path degrades to a silent game — audio must never crash it.

static const CLSID CLSID_MMDevEnum_ =
  {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const IID IID_IMMDevEnum_ =
  {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const IID IID_IAudioClient_ =
  {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
static const IID IID_IAudioRenderClient_ =
  {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};

// _Atomic, not volatile: the main thread writes it while the WASAPI thread
// reads it, and under ISO C volatile is no synchronization at all — the Linux
// half (and g_trig_w/r, and cfg.vol) already use the atomic idiom.
static _Atomic LONG g_audio_run = 1;
static HANDLE       g_audio_h;     // joined on quit, see WinMain's teardown

// A lost stream used to be permanent. Endpoint REMOVAL, a format change, or
// another app taking the endpoint in exclusive mode all make GetCurrentPadding
// or GetBuffer fail, and the loop below simply broke out — silencing the game
// for the rest of the session with no way back short of a restart. It now
// rebuilds the client a bounded number of times.
// KNOWN GAP: a DEFAULT-device change while the old endpoint stays alive
// (headphones plugged in beside speakers) does NOT fail the old stream —
// shared-mode WASAPI clients are not auto-rerouted, so audio stays on the old
// device until it errors. Catching that needs an IMMNotificationClient
// (OnDefaultDeviceChanged -> flag -> break-to-rebuild), which is COM plumbing
// this container cannot exercise; documented rather than implemented blind.
//
// The distinction that matters: an ACQUISITION failure (no render endpoint, a
// non-float mix format) is permanent and must still give up on the first try —
// retrying that would re-enter CoCreateInstance every 400 ms forever on a machine
// with no sound card. Only a stream that genuinely STARTED and then broke is
// worth rebuilding, which is exactly what `started` distinguishes.
#define AUDIO_RETRY_MAX 8
#define AUDIO_RETRY_MS  400
#define AUDIO_RETRY_SLICE_MS 20   // how often the back-off re-reads g_audio_run

static DWORD WINAPI audio_thread(LPVOID param) {
  (void)param;
  // The apartment is initialised ONCE and torn down once — it must not be
  // recycled per attempt.
  if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) return 0;
  int attempt = 0;
retry: ;
  IMMDeviceEnumerator *en = NULL;
  IMMDevice *dev = NULL;
  IAudioClient *ac = NULL;
  IAudioRenderClient *rc = NULL;
  WAVEFORMATEX *fmt = NULL;
  HANDLE ev = NULL;
  int started = 0;
  if (FAILED(CoCreateInstance(&CLSID_MMDevEnum_, NULL, CLSCTX_ALL,
                              &IID_IMMDevEnum_, (void **)&en))) goto done;
  if (FAILED(en->lpVtbl->GetDefaultAudioEndpoint(en, eRender, eConsole, &dev))) goto done;
  if (FAILED(dev->lpVtbl->Activate(dev, &IID_IAudioClient_, CLSCTX_ALL, NULL,
                                   (void **)&ac))) goto done;
  if (FAILED(ac->lpVtbl->GetMixFormat(ac, &fmt))) goto done;
  // The shared-mode mix format is 32-bit float on Win10/11; bail silently on
  // anything exotic rather than converting.
  int is_float = fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                 (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                  ((const WAVEFORMATEXTENSIBLE *)fmt)->SubFormat.Data1 == 3);
  if (!is_float || fmt->nChannels < 1) goto done;
  g_dev_rate = (float)SFX_HZ / (float)fmt->nSamplesPerSec;
  if (FAILED(ac->lpVtbl->Initialize(ac, AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                    200000 /* 20 ms */, 0, fmt, NULL))) goto done;
  ev = CreateEventA(NULL, FALSE, FALSE, NULL);
  if (!ev || FAILED(ac->lpVtbl->SetEventHandle(ac, ev))) goto done;
  UINT32 bufsz = 0;
  if (FAILED(ac->lpVtbl->GetBufferSize(ac, &bufsz))) goto done;
  if (FAILED(ac->lpVtbl->GetService(ac, &IID_IAudioRenderClient_, (void **)&rc))) goto done;
  if (FAILED(ac->lpVtbl->Start(ac))) goto done;
  started = 1;
  while (atomic_load_explicit(&g_audio_run, memory_order_relaxed)) {
    if (WaitForSingleObject(ev, 200) != WAIT_OBJECT_0) continue;
    UINT32 pad = 0;
    if (FAILED(ac->lpVtbl->GetCurrentPadding(ac, &pad))) break;
    UINT32 n = bufsz - pad;
    if (!n) continue;
    BYTE *out = NULL;
    if (FAILED(rc->lpVtbl->GetBuffer(rc, n, &out))) break;
    audio_mix((float *)out, n, fmt->nChannels);
    rc->lpVtbl->ReleaseBuffer(rc, n, 0);
  }
done:
  if (started) ac->lpVtbl->Stop(ac);
  if (rc) rc->lpVtbl->Release(rc);
  if (ac) ac->lpVtbl->Release(ac);
  if (dev) dev->lpVtbl->Release(dev);
  if (en) en->lpVtbl->Release(en);
  if (fmt) CoTaskMemFree(fmt);
  if (ev) CloseHandle(ev);
  // Rebuild only a stream that was running and broke — never a device that was
  // never there. Stale triggers are dropped first: the ring holds scheduled
  // delays measured against a stream that no longer exists, so replaying them
  // would open the new one with a burst of ancient sounds.
  // Sliced like the ALSA twin's: one long Sleep spends half of WinMain's 800 ms
  // join before the first COM call, and then re-enters device activation the
  // teardown already asked this thread to leave.
  if (atomic_load_explicit(&g_audio_run, memory_order_relaxed) && started &&
      ++attempt <= AUDIO_RETRY_MAX) {
    int ok = 1;
    for (int slept = 0; slept < AUDIO_RETRY_MS; slept += AUDIO_RETRY_SLICE_MS) {
      if (!atomic_load_explicit(&g_audio_run, memory_order_relaxed)) { ok = 0; break; }
      Sleep(AUDIO_RETRY_SLICE_MS);
    }
    if (ok && atomic_load_explicit(&g_audio_run, memory_order_relaxed)) {
      atomic_store(&g_trig_r, atomic_load(&g_trig_w));
      goto retry;
    }
  }
  CoUninitialize();
  return 0;
}

// Menu open: free the cursor (still hidden — the UI draws its own crosshair)
// and seed the UI cursor from the real one. Menu close: re-clip. Both ways,
// all input state is cleared so nothing sticks across the transition.
// ONE copy of "clear all keyboard/mouse input" — it runs on menu toggle AND on
// focus loss, and the two hand-written twins had already drifted (the focus
// path forgot g_ui_mdown, so an alt-tab with LMB held on a slider left the
// button latched and the slider dragging by pure cursor motion). The Linux
// backend's nat_input_clear is this function's sibling.
static void win_input_clear(void) {
  memset(g_keys, 0, sizeof g_keys);
  memset(g_pressed, 0, sizeof g_pressed);
  g_ui_mdown = 0;
  g_ui_drag = 0;
  g_mouse_dx = g_mouse_dy = 0;
}

static void platform_menu_changed(void) {
  if (g_menu_open) {
    ClipCursor(NULL);
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(g_hwnd, &pt);
    g_ui_cx = (float)pt.x;
    g_ui_cy = (float)pt.y;
  } else if (g_active) {
    win_clip_cursor();
  }
  win_input_clear();
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_INPUT: {
    RAWINPUT raw;
    UINT size = sizeof raw;
    if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
        raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
      g_mouse_dx += (float)raw.data.mouse.lLastX;
      g_mouse_dy += (float)raw.data.mouse.lLastY;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);  // MSDN: let the system clean up
  }
  case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: {
    WPARAM vk = wp;
    if (vk == VK_SHIFT)
      vk = MapVirtualKeyA(((UINT)lp >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX);
    else if (vk == VK_CONTROL)
      vk = (lp & (1 << 24)) ? VK_RCONTROL : VK_LCONTROL;
    int down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
    // The PRESS EDGE only — lParam bit 30 is the key's PREVIOUS state, so it is
    // set on an auto-repeat. Acting on repeats toggles the menu at the repeat
    // rate and writes config.cfg on every close, i.e. thirty file writes a
    // second for as long as the key is held. (Same guard as the X11 backend's
    // g_esc_held; that one needs a flag because detectable auto-repeat sends no
    // release to key off.)
    if (down && wp == VK_ESCAPE) { if (!(lp & (1 << 30))) ui_on_escape(); return 0; }
    if (down && UI_TEXT_EDIT && msg == WM_KEYDOWN) {  // a text field has focus
      if (vk == VK_BACK) ui_name_key("back");
      else if (vk == VK_RETURN) ui_name_key("enter");
      else if (vk == VK_OEM_PERIOD || vk == VK_DECIMAL) ui_name_key(".");
      else if (vk == VK_OEM_1) ui_name_key(":");
      else if (vk == VK_OEM_MINUS || vk == VK_SUBTRACT) ui_name_key("-");
      else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        char t[2] = {(char)('0' + (vk - VK_NUMPAD0)), 0};
        ui_name_key(t);
      } else {
        const char *tok = token_from_key((int)vk);
        if (tok && !tok[1]) ui_name_key(tok);
      }
      return 0;
    }
    if (down && g_ui_rebind >= 0 && msg == WM_KEYDOWN) {
      const char *tok = token_from_key((int)vk);
      if (tok) ui_assign_bind(tok);          // same cancel rule as the X11 half
      else if (vk == VK_RETURN || vk == VK_BACK) g_ui_rebind = -1;
      return 0;
    }
    // Arrow keys + Enter drive the menu like the pad — see the X11 backend's
    // nat_key for why this matters on the handheld. Repeats are wanted.
    if (down && g_menu_open) {
      // PgUp/PgDn switch tabs — same rationale as the X11 twin.
      int nav = vk == VK_UP    ? NAV_UP
              : vk == VK_DOWN  ? NAV_DOWN
              : vk == VK_LEFT  ? NAV_LEFT
              : vk == VK_RIGHT ? NAV_RIGHT
              : vk == VK_PRIOR ? NAV_TABL
              : vk == VK_NEXT  ? NAV_TABR
              : vk == VK_RETURN ? NAV_OK : -1;
      if (nav >= 0) { g_nav_kbd = 1; nav_push(nav); return 0; }
    }
    if (vk < 256) {
      g_keys[vk] = down;
      if (down && !g_menu_open) g_pressed[vk] = 1;
    }
    if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) return DefWindowProcA(hwnd, msg, wp, lp);
    return 0;
  }
  case WM_LBUTTONDOWN:
    if (g_menu_open) {
      if (g_ui_rebind >= 0) ui_assign_bind("mouse1");
      else g_ui_mdown = 1;
      return 0;
    }
    g_keys[KEY_MOUSE1] = 1; g_pressed[KEY_MOUSE1] = 1; return 0;
  case WM_LBUTTONUP:
    g_ui_mdown = 0; g_keys[KEY_MOUSE1] = 0; return 0;
  case WM_RBUTTONDOWN:
    if (g_menu_open) { if (g_ui_rebind >= 0) ui_assign_bind("mouse2"); return 0; }
    g_keys[KEY_MOUSE2] = 1; g_pressed[KEY_MOUSE2] = 1; return 0;
  case WM_RBUTTONUP:   g_keys[KEY_MOUSE2] = 0; return 0;
  case WM_MBUTTONDOWN:
    if (g_menu_open) { if (g_ui_rebind >= 0) ui_assign_bind("mouse3"); return 0; }
    g_keys[KEY_MOUSE3] = 1; g_pressed[KEY_MOUSE3] = 1; return 0;
  case WM_MBUTTONUP:   g_keys[KEY_MOUSE3] = 0; return 0;
  case WM_XBUTTONDOWN: {  // thumb buttons; MSDN wants TRUE returned when handled
    int k = HIWORD(wp) == XBUTTON1 ? KEY_MOUSE4 : KEY_MOUSE5;
    if (g_menu_open) {
      if (g_ui_rebind >= 0) ui_assign_bind(k == KEY_MOUSE4 ? "mouse4" : "mouse5");
      return TRUE;
    }
    g_keys[k] = 1; g_pressed[k] = 1; return TRUE;
  }
  case WM_XBUTTONUP:
    g_keys[HIWORD(wp) == XBUTTON1 ? KEY_MOUSE4 : KEY_MOUSE5] = 0; return TRUE;
  case WM_MOUSEWHEEL: {
    // The wheel cycles the weapon (see frame_input_keys). WHEEL_DELTA is one
    // detent; a free-spinning wheel can deliver several in one message and a
    // high-resolution one delivers fractions, so the notches are COUNTED and
    // the count is collapsed to a single latch — the alternative is a swap per
    // sub-detent report, i.e. a rifle that flickers between identities under a
    // flick. Latch only, no g_keys: a notch has no level to hold.
    int notch = (short)HIWORD(wp) / WHEEL_DELTA;
    if (notch && g_active && !g_menu_open)
      g_pressed[notch > 0 ? KEY_MWUP : KEY_MWDN] = 1;
    return 0;
  }
  case WM_MOUSEMOVE:
    g_ui_cx = (float)(short)LOWORD(lp);
    g_ui_cy = (float)(short)HIWORD(lp);
    return 0;
  case WM_MOUSEACTIVATE:
    return MA_ACTIVATEANDEAT;  // the click that refocuses the game must not fire
  case WM_ACTIVATE:
    // HIWORD(wp) is the minimized flag: activation of a still-minimized
    // window must not clip the cursor to its off-screen placement rect.
    g_active = LOWORD(wp) != WA_INACTIVE && !HIWORD(wp);
    if (g_active) {
      if (!g_menu_open) win_clip_cursor();
      win_set_cursor_visible(0);
      // XInput is a level poll, so a button held across the alt-tab (or the
      // press that refocused the game) re-appears fully pressed on the first
      // active frame — eat everything still down, like MA_ACTIVATEANDEAT.
      win_pad_pump(0.0);  // refresh levels first; 0.0 skips only the rescan
      pad_eat_held();
    } else {
      // Fast, glitch-free alt-tab: release the cursor and clear all input so
      // no key sticks while the game is in the background. Pad levels stay —
      // the focus-gain eat above is what neutralises them.
      ClipCursor(NULL);
      win_set_cursor_visible(1);
      win_input_clear();
    }
    return 0;
  case WM_DISPLAYCHANGE: {
    // Desktop resolution changed while running: keep the borderless popup
    // matched to the desktop, or the "fullscreen" illusion breaks. Same +1 px
    // as at startup to stay out of fullscreen optimizations (HDR handshake).
    int w = LOWORD(lp), h = HIWORD(lp) + 1;
    if (w > 0 && h > 0 && (w != R.width || h != R.height)) {
      SetWindowPos(hwnd, NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
      R.width = w;
      R.height = h;
      glViewport(0, 0, w, h);
      if (g_active) win_clip_cursor();
    }
    return 0;
  }
  case WM_SETCURSOR:
    if (g_active && LOWORD(lp) == HTCLIENT) { SetCursor(NULL); return TRUE; }
    break;
  case WM_SYSCOMMAND:
    if ((wp & 0xFFF0) == SC_KEYMENU) return 0;  // no menu beep on Alt
    break;
  case WM_ERASEBKGND:
    return 1;
  case WM_CLOSE:
    if (g_menu_open) config_write(g_cfg_path, &g_cfg);  // Alt+F4 mid-edit: keep changes
    g_running = 0;
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(hwnd, msg, wp, lp);
}


int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show) {
  (void)prev; (void)cmdline; (void)show;

  // Per-monitor DPI awareness so the popup covers real pixels (dynamic call:
  // present on Win10 1703+, and we must not fail to start without it).
  HMODULE user32 = GetModuleHandleA("user32.dll");
  typedef BOOL (WINAPI *pfn_dpi)(HANDLE);
  pfn_dpi set_dpi = (pfn_dpi)(void *)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
  if (set_dpi) set_dpi((HANDLE)-4 /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */);

  char dir[480];
  exe_dir(dir, sizeof dir);
  snprintf(g_cfg_path, sizeof g_cfg_path, "%sconfig.cfg", dir);
  config_load_or_create(g_cfg_path, &g_cfg);

  WNDCLASSA wc = {
    .style = CS_OWNDC,
    .lpfnWndProc = wndproc,
    .hInstance = inst,
    .hCursor = LoadCursorA(NULL, IDC_ARROW),
    .hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH),
    .lpszClassName = "blinddrop",
  };
  RegisterClassA(&wc);

  // Borderless fullscreen: a plain popup at desktop resolution. No display
  // mode change, no exclusive fullscreen — this is what avoids HDR mode-switch
  // black screens and makes alt-tab instant. The +1 px height is deliberate:
  // a popup matching the screen EXACTLY is promoted to fullscreen
  // optimizations (independent flip), which performs the HDR handshake with
  // its multi-second black screen; one off-screen pixel row keeps DWM in
  // plain composition. (Learned the hard way on an HDR display.)
  int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN) + 1;
  g_hwnd = CreateWindowExA(0, wc.lpszClassName, "BlindDrop", WS_POPUP,
                           0, 0, sw, sh, NULL, NULL, inst, NULL);
  HDC dc = GetDC(g_hwnd);

  PIXELFORMATDESCRIPTOR pfd = {
    .nSize = sizeof pfd,
    .nVersion = 1,
    .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    .iPixelType = PFD_TYPE_RGBA,
    .cColorBits = 32,
    .cDepthBits = 24,
  };
  SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
  HGLRC dummy = wglCreateContext(dc);
  wglMakeCurrent(dc, dummy);
  pfn_wglCreateContextAttribsARB create_ctx =
      (pfn_wglCreateContextAttribsARB)(void *)wglGetProcAddress("wglCreateContextAttribsARB");
  HGLRC ctx = NULL;
  if (create_ctx) {
    int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                     WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
    ctx = create_ctx(dc, NULL, attribs);
  }
  if (ctx) {
    wglMakeCurrent(dc, ctx);
    wglDeleteContext(dummy);
  } else {
    ctx = dummy;  // ancient driver: try the legacy context and hope for 3.3
  }
  gl_load_functions((gl_loader_fn)(void *)wglGetProcAddress);
  g_swap_interval = (pfn_wglSwapIntervalEXT)(void *)wglGetProcAddress("wglSwapIntervalEXT");
  if (g_swap_interval) g_swap_interval(g_cfg.vsync);
  timeBeginPeriod(1);  // 1 ms Sleep granularity for the frame limiter
  win_pad_load();

  // Singleplayer contract: every launch gets a fresh procedural arena.
  LARGE_INTEGER seed_qpc;
  QueryPerformanceCounter(&seed_qpc);
  player_t *player_s = &g_players[0];   // human 0 IS the local player (P1-1)
  app_start(player_s, sw, sh,
            (uint32_t)seed_qpc.QuadPart ^
            ((uint32_t)(seed_qpc.QuadPart >> 32) * 2654435761u) ^
            GetCurrentProcessId());
  g_audio_h = CreateThread(NULL, 0, audio_thread, NULL, 0, NULL);
  ui_open_menu();      // boot into the menu over the live arena, as on Linux
  home_enter(local_player());   // ...as the BOOT SCREEN, as on Linux
  boot_connect_kick();   // --connect, same as Linux

  RAWINPUTDEVICE rid = {.usUsagePage = 1, .usUsage = 2, .hwndTarget = g_hwnd};
  RegisterRawInputDevices(&rid, 1, sizeof rid);

  // First frame before the window appears: no black/white startup flash.
  render_frame(player_s, 1.0f, 0, NULL);
  SwapBuffers(dc);
  ShowWindow(g_hwnd, SW_SHOW);
  SetForegroundWindow(g_hwnd);
  SetFocus(g_hwnd);

  app_t app = {0};
  LARGE_INTEGER freq, now, last;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&last);

  while (g_running) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) g_running = 0;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    if (!g_active) {  // alt-tabbed away: no sim, no rendering, no GPU burn
      Sleep(5);
      QueryPerformanceCounter(&last);
      app.acc = 0;
      continue;
    }

    QueryPerformanceCounter(&now);
    double dt = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
    last = now;

    win_pad_pump((double)now.QuadPart / (double)freq.QuadPart);
    if (app_pump(&app, player_s, dt, g_mouse_dx, g_mouse_dy)) g_running = 0;
    g_mouse_dx = g_mouse_dy = 0;

    SwapBuffers(dc);
    app_sync(&app);

    // Frame limiter: sleep the coarse part, spin the last 2 ms for accuracy.
    if (g_cfg.fps_cap > 0) {
      double target = 1.0 / g_cfg.fps_cap;
      LARGE_INTEGER t;
      for (;;) {
        QueryPerformanceCounter(&t);
        double el = (double)(t.QuadPart - last.QuadPart) / (double)freq.QuadPart;
        if (el >= target) break;
        if (target - el > 0.002) Sleep(1);
        else YieldProcessor();
      }
    }
  }

  atomic_store(&g_audio_run, 0);  // the audio thread winds down while the window closes
  DestroyWindow(g_hwnd);
  // JOIN, and only after DestroyWindow. The process used to exit while the audio
  // thread might still be inside a WASAPI/COM call, which is a teardown race the
  // loader resolves by luck. Joining BEFORE closing the window would throw away
  // the overlap the line above is for and leave the window on screen for the
  // whole wait, so the order matters. The NULL guard keeps this file's rule that
  // any audio init failure is a silent game and never a crash.
  if (g_audio_h) {
    WaitForSingleObject(g_audio_h, 800);
    CloseHandle(g_audio_h);
  }
  win_dinput_shutdown();
  return 0;
}

