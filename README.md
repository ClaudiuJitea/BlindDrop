<p align="center">
  <img src="media/hero.gif" alt="bullet-time headshot, slide under fire, jump the lens, fire at the camera" width="832">
</p>

<h1 align="center">skill-issue</h1>

<p align="center">
  <b>A free, private arena FPS. A new procedural map every match, two guns, 120 Hz servers.<br>
  No telemetry, no auto-updates, no accounts.</b>
</p>

<p align="center">
  <img src="media/fight.png" alt="assault rifle firefight on a procedural sand arena" width="49%">
  <img src="media/scope.png" alt="sniper scope on a bot returning fire" width="49%">
</p>

This is an independent, privacy-focused version of the original
[skill-issue by codehamr](https://codehamr.com). It runs on its own, with no
connection to the original project's servers or releases.

## The game

- **A new arena every match.** Map, weapons, characters and sounds are all generated from a seed at startup.
- **Two guns, everyone gets both.** A hitscan rifle and a bolt sniper. No loadouts or unlocks.
- **Movement you can get good at.** Air control, a slide that carries speed, and `Q`/`E` lean.
- **Bots that work solo.** They hear you, remember you and push.
- **One file.** Plain C with no engine, compiled to a single native executable.

## Privacy and network behaviour

The game contacts nothing on its own.

| | This version |
|---|---|
| Telemetry | Removed from client and server |
| Update check and self-replacement | Removed |
| Built-in server address | Removed |
| Singleplayer network traffic | None |
| Multiplayer network traffic | Only to the server you choose, over UDP |

Multiplayer traffic is not encrypted, and the server you join sees your IP address and player
name, so only join servers you trust.

If you ran an older version, delete its `config.cfg`. It may still hold the old server address.

## Build

Linux needs gcc 14 or newer, plus the EGL, GL and X11 development headers.

```sh
make build/game-x86_64     # Linux
make build/game.exe        # Windows, cross-compiled with MinGW-w64
./build/game-x86_64
```

Prebuilt binaries, when published, are on the
[releases page](https://github.com/ClaudiuJitea/skill-issue/releases) with a `SHA256SUMS` file.
Windows binaries are unsigned, so SmartScreen will warn you.

## Multiplayer

Anyone can host with a Linux build. The default UDP port is 31415.

```sh
./build/game-x86_64 --server
```

To join, either pass the address when you launch:

```sh
./build/game-x86_64 --connect 192.168.1.50
./build/game-x86_64 --connect example.com:31415
```

Or add it to `config.cfg` next to the binary, and **Quick Join** in the menu will use it:

```
mp_host 192.168.1.50
mp_port 31415
```

Up to 8 humans can play per arena, and bots fill the empty slots.

## License

Licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE). You can read, change and
share this software for noncommercial purposes. Commercial use is not permitted.

Required Notice: Copyright (c) 2026 codehamr (https://codehamr.com)

This version is a modified work based on that software. It is not affiliated with or endorsed by
codehamr.
