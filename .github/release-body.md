BlindDrop is an arena FPS in one C program. No engine and nothing to install: maps and weapons are generated in code, and the sound bank is built into the one binary.

## What's new in 0.8.6

- **Simpler modes:** four modes (Deathmatch, Elimination, Gun Game, Hostage Rescue), plus **Teams** (on/off) and **Weapons** (all, snipers only, knives + nades) as settings. Any mix works, such as Team Elimination with snipers only. Old settings carry over.
- **Elimination with teams:** no respawns, last side standing takes the round.
- **Build your own Gun Game:** pick up to 10 guns in any order on the Ladder page. When you host, your ladder is the one every player climbs (`--ladder` on a dedicated server).
- **New forearm displays:** kills this life, health and ammo bars, a radar or a compass. Pick one under Settings > HUD or Armory > Gear.
- Ballistics moved to Host Settings and Dev > Assist.

0.8.6 uses network protocol 23, so it can't play with older versions.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
