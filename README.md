<p align="center">
  <img src="media/hero.jpg" alt="The mosque courtyard and minaret on Citadel at dusk" width="832">
</p>

<h1 align="center">BlindDrop</h1>

<p align="center">
  <b>A free, private arena FPS in one C file.</b><br>
  Five modes, seven maps, bots that push, and no telemetry.
</p>

<p align="center">
  <a href="https://github.com/ClaudiuJitea/BlindDrop/releases/latest"><b>Download for Linux</b></a>
</p>

<p align="center">
  <img src="media/sniper.jpg" alt="Sniper lookout on top of Citadel's minaret" width="49%">
  <img src="media/harbor.jpg" alt="The waterfront dock on Harbor" width="49%">
  <img src="media/refinery.jpg" alt="Pipe racks over mid on Refinery" width="49%">
  <img src="media/citadel.jpg" alt="The market street through mid on Citadel" width="49%">
</p>

## The game

- **Five weapons:** rifle, bolt sniper, sidearm, knife and frag grenades on `1`-`5`, with no loadouts or unlocks.
- **Real grenades:** cook, throw or lob them; they bounce, roll and are stopped by cover.
- **A knife that fights back:** slash chains, a guard, and parries that stagger.
- **Movement:** air control, a slide that carries speed, and `Q`/`E` lean.
- **Bots:** they hear you, remember you, flank, throw grenades and find their way around the big maps.
- **No engine and no asset files:** maps, models and every sound are generated in code.

## Maps

| Map | |
|---|---|
| **Arena** | Procedural: a new layout and sky every match, so nobody starts with map knowledge. |
| **Foundry**, **Bazaar**, **Outpost** | Handmade 48 m arenas: a gantry and side halls, a market square with rooftops, and long sniper lanes. |
| **Citadel** | 104 m desert old town with A and B sites: a domed mosque, a climbable minaret, arched gates and rooftop stairs. |
| **Harbor** | 104 m waterfront: brick warehouses, stacked containers, a crane walkway over the yard and a lighthouse. |
| **Refinery** | 104 m industrial site: storage tanks, pipe racks over mid, a flare stack and a distillation-column lookout. |

**Rotate** moves to the next map every match.

## Modes

| Mode | |
|---|---|
| **Deathmatch** | First to the frag limit. |
| **Gun Game** | Two kills per weapon move you up the ladder; the first knife kill wins. |
| **Snipers Only** | The bolt sniper and nothing else. |
| **Knives + Nades** | The knife, with grenades that regrow. |
| **Last Man Standing** | No respawns; the last one alive takes the round, and you spectate once you're out. |

Pick the mode and map under **Singleplayer**. They apply on the next restart.

## Download and run

Linux x86_64, glibc 2.38 or newer (Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch, SteamOS 3.7+):

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Check a download with `sha256sum -c SHA256SUMS`. If the frame rate is low, turn off **Post Effects**
in Settings > Video.

## Multiplayer

Anyone can host; up to 8 players per arena, and bots fill the empty slots. The default port is UDP 31415.

```sh
./blinddrop-linux-amd64 --server --mode lms --map citadel   # host
./blinddrop-linux-amd64 --connect 192.168.1.50              # join
```

Modes: `dm gun sniper blades lms`. Maps: `arena foundry bazaar outpost citadel harbor refinery rotate`.
Set `mp_host` in `config.cfg` to use **Quick Join** from the menu.

## Privacy

No telemetry, no update checks and no built-in servers. The game only uses the network when you join
or host, and only to the server you choose. That traffic is not encrypted, and a server sees your IP
address and player name, so only join servers you trust.

## Build from source

Needs gcc 14+, make, and the EGL, GL and X11 development headers:

```sh
make build/game-x86_64
./build/game-x86_64
```

`make dist-linux RELEASE_VERSION=v0.3` builds the redistributable binary (needs Docker). The
first-person models come from `tools/build_models.py`, regenerated with
`blender -b --factory-startup -P tools/build_models.py`.

## Credits and license

BlindDrop is an independent modified version of
[skill-issue by codehamr](https://codehamr.com). It is not affiliated with or endorsed by codehamr.

It is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE). You can read, change
and share it for noncommercial purposes. Commercial use is not permitted.

Required Notice: Copyright (c) 2026 codehamr (https://codehamr.com)
