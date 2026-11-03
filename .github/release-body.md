BlindDrop is an arena FPS in one C file. No engine, no asset files: maps, weapons and sounds are all generated in code.

## What's new in 0.3

- **Five modes:** Deathmatch, Gun Game, Snipers Only, Knives + Nades and Last Man Standing (with spectating and round play).
- **Seven maps:** the procedural Arena, three handmade arenas (Foundry, Bazaar, Outpost), and three large tactical maps with A and B bomb sites — **Citadel** (desert old town with a climbable minaret), **Harbor** (warehouses, containers, a crane walkway and a lighthouse) and **Refinery** (tanks, pipe racks and a distillation column lookout). Rotate cycles through them all.
- **Smarter bots:** they throw grenades, rush with knives, climb stairs and find their way around the big maps.
- **Sniper:** hip-fire spread cut from 3.0 to 0.6 degrees.
- **Multiplayer:** servers take `--mode` and `--map`. The network protocol changed, so 0.3 clients and servers do not mix with older versions.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

Check your download with `sha256sum -c SHA256SUMS`.

A Windows build is not available yet.
