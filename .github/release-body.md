BlindDrop is an arena FPS in one C file. No engine, no asset files: maps, weapons and sounds are all generated in code.

## What's new in 0.4

- **Each gun fires its own round:** the AR keeps its quick, thin tracer, the sniper's is faster and white-hot and leaves a heat trail hanging in the air for half a second, and the pistol's is slower, shorter and warmer.
- **Ricochets:** rounds that hit a wall or the floor at a shallow angle can skip off it.
- **Muzzle flashes per weapon:** a narrow forward flash on the AR, a big sideways blast from the sniper's muzzle brake, a round fireball on the pistol. Each gun also leaves its own muzzle smoke.
- **Impacts match the surface:** metal throws hot sparks, stone and brick throw cooler chips, wood barely sparks, and every non-metal hit kicks up dust in the colour of what it hit.
- **Building materials:** walls now render as brick, plaster, poured concrete, corrugated sheet, shipping containers, stone and timber instead of flat colour, and hard floors are cut into slabs.

0.4 uses the same network protocol as 0.3, so 0.3 and 0.4 players can play together.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

Check your download with `sha256sum -c SHA256SUMS`.

A Windows build is not available yet.
