<p align="center">
  <img src="media/hero.png" alt="BlindDrop main menu over a live arena at golden hour" width="832">
</p>

<h1 align="center">BlindDrop</h1>

<p align="center">
  <b>New arena. No map knowledge. Just aim and movement.</b><br>
  A free, private arena FPS with procedural maps and no telemetry.
</p>

<p align="center">
  <a href="https://github.com/ClaudiuJitea/BlindDrop/releases/latest"><b>Download for Linux</b></a>
</p>

<p align="center">
  <img src="media/fight.png" alt="Fight in an arena under the Ember Dusk sky" width="49%">
  <img src="media/arena.png" alt="Arena at Golden Hour with mountains on the horizon" width="49%">
</p>

## The game

- **Dropped in blind.** On the Arena map, every match generates a new layout, along with its weapons and characters, so nobody starts with map knowledge. Every sound in the game is synthesized from code.
- **Five skies.** Every arena drops into High Noon, Golden Hour, Ember Dusk, Cold Dawn or Stormfront, with its own light, clouds and color grade. Everyone in a match sees the same one.
- **Five weapons, everyone gets all of them.** A hitscan rifle, a bolt sniper, a sidearm, a combat knife and two frag grenades on keys `1`-`5`. No loadouts, unlocks or attachments. (Some modes below narrow this.)
- **Seven maps.** **Arena** is the procedural one, built fresh every match. Three are handmade, each with its own look and sky. **Foundry** has a gantry you can stand on or run under, plus two side halls. **Bazaar** is a fountain square in close quarters, with market stalls and two rooftops you can climb. **Outpost** is a sniper map with three long lanes, gated bunkers and container perches. Three larger tactical maps (104 m, two-sided with A and B bomb sites) are built as real places, with houses that have windows, doors, shutters, balconies, awnings, cornices and rooftop parapets. **Citadel** is a desert old town with a domed mosque and a minaret you can climb, arched gates, a market street, palms, and rooftops reached by outside stairs. **Harbor** is a waterfront of brick warehouses and stacked shipping containers, with a gantry crane whose walkway crosses the container yard and a lighthouse to climb. **Refinery** has storage tanks, a pipe rack over mid, a flare stack, a cooling tower and a distillation column with a lookout deck. You walk up stairs on these maps, and bots route around their streets. **Rotate** moves to the next map every match. Choose under **Singleplayer**.
- **Five modes.** Pick one under **Singleplayer**; it applies on the next restart.
  - **Deathmatch:** first to the frag limit.
  - **Gun Game:** two kills per weapon move you up the ladder (rifle, sidearm, sniper, grenades, knife). The first knife kill wins. Getting knifed or killing yourself drops you a rung.
  - **Snipers Only:** the bolt sniper and nothing else.
  - **Knives + Nades:** the knife, plus grenades that regrow one every 5 s.
  - **Last Man Standing:** no respawns. The last player alive takes the round, and you spectate once you're out. Once two remain or a round passes 45 s, bots are pointed at their nearest rival, and a round still going at 3 minutes is a draw.
- **Grenades that behave.** Hold fire to pull the pin and cook, release to throw, and hold aim as you release to lob it underhand. While you hold it, an arc shows where it will land and a countdown shows the fuse. It bounces and rolls for real, kills inside about 4.5 m, wounds out to 9 m, and cover stops it. Cook it too long and it goes off in your hand.
- **A knife that fights back.** Left mouse chains two slashes into a thrust. Right mouse raises a guard that blocks frontal cuts, and a guard raised just in time parries and staggers the attacker.
- **Built to be seen.** Enemies wear charcoal with crimson helmets, players wear slate with blaze orange, and everyone carries field kit. In first person you get a sculpted two-tone carbine with a holographic sight, mesh tactical gloves and an armored forearm gauntlet.
- **Movement you can get good at.** Air control, a slide that carries speed, and `Q`/`E` lean.
- **Bots that make solo worthwhile.** They hear you, remember you and push.
- **One small executable.** Plain C with no engine and no asset files.

## Download and run

BlindDrop currently ships as a Linux x86_64 binary. It needs glibc 2.38 or newer: Ubuntu 23.10+,
Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Each release includes a `SHA256SUMS` file, which you can check with `sha256sum -c SHA256SUMS`.

## Graphics options

Settings > Video has anti-aliasing and **Post Effects**, which covers bloom, color grading and the
vignette. If frame rate is a problem, try turning Post Effects off first. You can also set
`post_fx 0` in `config.cfg`.

## Privacy

BlindDrop contacts nothing on its own.

| | |
|---|---|
| Telemetry | None |
| Update checks | None |
| Built-in servers | None |
| Singleplayer network traffic | None |
| Multiplayer network traffic | Only to the server you choose, over UDP |

Multiplayer traffic is not encrypted, and any server you join sees your IP address and player
name. Only join servers you trust.

## Multiplayer

Anyone can host from a Linux build. The default port is UDP 31415.

```sh
./blinddrop-linux-amd64 --server
```

To join, pass the address when you launch:

```sh
./blinddrop-linux-amd64 --connect 192.168.1.50
./blinddrop-linux-amd64 --connect example.com:31415
```

Or add it to `config.cfg` next to the binary, and **Quick Join** in the menu will use it:

```
mp_host 192.168.1.50
mp_port 31415
```

Up to 8 people can play in each arena, and bots fill the empty slots.

The server picks the mode and the map. Deathmatch on Arena is the default:

```sh
./blinddrop-linux-amd64 --server --mode gun        # dm | gun | sniper | blades | lms
./blinddrop-linux-amd64 --server --mode lms --rounds 5
./blinddrop-linux-amd64 --server --map rotate      # arena | foundry | bazaar | outpost | citadel | harbor | refinery | rotate
```

## Build from source

You need gcc 14 or newer, make, and the EGL, GL and X11 development headers.

```sh
make build/game-x86_64
./build/game-x86_64
```

To build the same redistributable binary as a release, which needs Docker:

```sh
make dist-linux RELEASE_VERSION=v0.1.0
```

The first-person rifle, gloves and forearm armor are sculpted by `tools/build_models.py` and
compiled in through `code/models.h`. To change them, edit that script and regenerate the header
with Blender:

```sh
blender -b --factory-startup -P tools/build_models.py
```

## Credits and license

BlindDrop is an independent modified version of
[skill-issue by codehamr](https://codehamr.com). It is not affiliated with or endorsed by codehamr.

It is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE). You can read, change
and share it for noncommercial purposes. Commercial use is not permitted.

Required Notice: Copyright (c) 2026 codehamr (https://codehamr.com)
