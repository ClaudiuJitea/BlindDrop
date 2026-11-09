// code/core/platform_api.c — part of the game.c unity build: #included by code/game.c at a
// fixed position, never compiled on its own. See the file map there.
// Directory of the running executable, with trailing slash, so config.cfg
// lands next to game.exe no matter where it is started from.
static void exe_dir(char *out, size_t n) {
#ifdef _WIN32
  DWORD len = GetModuleFileNameA(NULL, out, (DWORD)n);
  while (len > 0 && out[len - 1] != '\\' && out[len - 1] != '/') len--;
  out[len] = '\0';
#else
  ssize_t len = readlink("/proc/self/exe", out, n - 1);
  if (len <= 0) { out[0] = '\0'; return; }
  out[len] = '\0';
  char *slash = strrchr(out, '/');
  if (slash) slash[1] = '\0';
#endif
}

#ifndef BUILD_VERSION
#define BUILD_VERSION "dev"     // Makefile default
#endif
#ifndef BUILD_COMMIT
#define BUILD_COMMIT "unknown"
#endif

// UDP socket backend: the ONLY networking below the platform
// split — POSIX in the Linux backend, Winsock in the Windows one. Everything
// above (encode/decode, the connection automaton, prediction) is shared. ip is
// a network-order s_addr; port is host-order. recv is non-blocking: it returns
// >0 bytes, 0 for "nothing waiting", -1 on a real error.
static int      net_udp_open(int port);       // 0 = ephemeral (client); -1 fail
static void     net_udp_close(int fd);
static int      net_udp_send(int fd, uint32_t ip, uint16_t port, const void *b, int n);
static int      net_udp_recv(int fd, uint32_t *ip, uint16_t *port, void *b, int cap);
static uint32_t net_resolve(const char *host);  // name/IP -> network-order s_addr, 0 fail
static uint32_t net_now_ms(void);               // monotonic ms for timeouts/RTT
// LAN discovery and HOST GAME, below the split for the same reason: a socket
// option, the interface list and a child process are all platform calls.
static int      net_udp_bcast_on(int fd);       // allow sends to broadcast addresses
static int      net_bcast_targets(uint32_t *out, int max);  // where a LAN scan goes
static int      net_local_addrs(uint32_t *out, int max);    // this machine's LAN IPs
static int      host_spawn(char *const argv[]); // start a --server child; 0 = failed
static int      host_alive(void);               // that child is still running
static void     host_kill(void);                // stop it (and reap it)
#ifdef _WIN32
#define HOST_SUPPORTED 0   // the Windows build has no --server mode
#else
#define HOST_SUPPORTED 1
#endif

