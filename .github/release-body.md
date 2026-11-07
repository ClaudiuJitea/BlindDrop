BlindDrop is an arena FPS in one C file. No engine, no asset files: maps, weapons and sounds are all generated in code.

## What's new in 0.6

- **Host a game in one click:** MULTIPLAYER > HOST A GAME starts a server on your PC and puts you in it. Everyone on your network sees it under **Games on your network** and joins with a click; friends outside your network join by address (the host forwards UDP 31415). **Host settings** picks the hosted game's mode, map, bots, difficulty and frag limit, separately from Singleplayer.
- **Team Deathmatch:** ALPHA against BRAVO, 1 v 1 up to 9 v 9; the first side to the frag limit wins.
  - Players who join choose their side on the **CHOOSE TEAM** page (a full side refuses), and the **TEAM** tile switches sides mid-match.
  - The host chooses whether bots fill the empty places (they step aside as players join) and whether friendly fire is on (off by default).
  - ALPHA wears red and BRAVO blue, on bots, players, bodies and your own arms. Teammates carry a marker and name in their team colour, and the kill feed and scoreboard colour every name by team.
  - Also offline in Singleplayer: you and bot teammates against a bot team.
- **Tracers:** every gun draws its rounds from the muzzle to the impact, including while spraying and strafing, and the barrel lines up with the crosshair from the hip.
- **Grenade pin pull:** a straight wrist and a hooked finger through the ring.
- **Windows build:** `blinddrop-windows-amd64.exe`. Windows players can play offline and join games; hosting needs the Linux build.

0.6 uses a new network protocol: everyone in a game needs 0.6.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
