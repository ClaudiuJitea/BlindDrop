<p align="center">
  <img src="media/hero.jpg" alt="The BlindDrop main menu over a sunlit arena, an operator in a plate carrier holding an assault rifle" width="832">
</p>

<h1 align="center">BlindDrop</h1>

<p align="center">
  <b>A free, private arena FPS written from scratch in C.</b><br>
  Four modes, eight maps, 34 weapons, team play with friends, bots that push, no telemetry.<br>
  <a href="https://github.com/ClaudiuJitea/BlindDrop/releases/latest"><b>Download for Linux and Windows</b></a>
</p>

<p align="center">
  <img src="media/foundry.jpg" alt="Foundry: the steelworks yard with a railed walkway, a crane gantry and the blast furnace, AK-47 in hand" width="49%">
  <img src="media/citadel.jpg" alt="Citadel: a sunlit old-town street with the minaret at its end, an AWP in hand" width="49%">
  <img src="media/bazaar.jpg" alt="Bazaar: the market square from a rooftop, canopies and palms in golden light, a pistol in hand" width="49%">
  <img src="media/refinery.jpg" alt="Refinery: a road under coloured pipe racks on a clear morning" width="49%">
</p>

## New in 0.8.6

- **Simpler modes:** four modes, plus **Teams** (on/off) and **Weapons** (all, snipers only, knives
  + nades) as settings. Any mix works, such as Team Elimination with snipers only.
- **Elimination with teams:** no respawns, last side standing takes the round.
- **Build your own Gun Game:** pick up to 10 guns in any order on the **Ladder** page. When you host,
  your ladder is the one every player climbs.
- **New forearm displays:** kills this life, health and ammo bars, a radar or a compass. Pick one
  under **Settings > HUD** or **Armory > Gear**.

0.8.6 uses network protocol 23, so it can't play with older versions.

<p align="center">
  <img src="media/modes.jpg" alt="The Singleplayer page with Deathmatch, Teams on and Snipers only; the same page in Gun Game with an 8-gun ladder; and the Gun Game Ladder page listing AK-47 to Knife" width="100%">
  <img src="media/wrist.jpg" alt="The four forearm displays: a kills counter, health and ammo bars, a radar and a compass" width="100%">
</p>

## The game

- **34 weapons, no unlocks:** 20 primaries, 10 pistols and 4 snipers, each with its own model,
  handling and recorded sound. Add frag, smoke and incendiary grenades and a knife.
- **Movement:** mantling, ladders, slides, `Q`/`E` leans, scopes and recoil you can learn.
- **Bots** hear and remember you, flank, throw grenades and use medical stations and ammo crates.
- **Your operator:** outfit, headgear, vest and pack under **Armory**. `V` toggles third person.
- **No engine, no asset files:** maps are generated in code, and the models and sounds are built
  into the one binary.

| Mode | |
|---|---|
| **Deathmatch** | Respawns, first to the frag limit. With Teams on: ALPHA against BRAVO, up to 9 v 9. |
| **Elimination** | No respawns: last one (or last side) standing takes the round. |
| **Gun Game** | A kill moves you up your own ladder of guns, and a kill with the last one wins. |
| **Hostage Rescue** | A stealth mission for a squad of one to three: free three hostages and reach the helicopter. |

**Maps:** Arena (new layout every match), Foundry, Bazaar, Outpost, Citadel, Harbor, Refinery and
Compound. **Rotate** changes map each match.

<p align="center">
  <img src="media/elimination.jpg" alt="Team Elimination with snipers only on Citadel: the round caption reads ROUND 1, 3 V 3, FIRST TO 3, SNIPERS ONLY" width="32%">
  <img src="media/gungame.jpg" alt="Gun Game on Foundry: the HUD shows the AK-47 on rung 1 of 8" width="32%">
  <img src="media/rescue.jpg" alt="Hostage Rescue on Compound: a guard in the sniper scope at dusk" width="32%">
</p>

## Download and run

**Linux** x86_64 with glibc 2.38+ (Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch, SteamOS 3.7+):

```sh
chmod +x ./blinddrop-linux-amd64 && ./blinddrop-linux-amd64
```

**Windows** 10 or 11, 64-bit: run `blinddrop-windows-amd64.exe`. It isn't code-signed, so
SmartScreen may ask you to confirm (More info > Run anyway). Windows can play and join, but hosting
needs Linux. Check any download with `sha256sum -c SHA256SUMS`.

If the frame rate is low, turn off **Post Effects** in Settings > Video.

## Multiplayer

**Host a game** starts a server on your PC (Linux only), **Games on your network** lists LAN games,
and **Join by address** takes `host:port` (forward UDP 31415 for internet play). Up to 8 players per
arena, with bots filling the empty places.

```sh
./blinddrop-linux-amd64 --server --mode elim --teams 1 --weapons snipers --team-size 5
./blinddrop-linux-amd64 --server --mode gun --ladder ar1,ar15,pt2,sr2,kn0
./blinddrop-linux-amd64 --connect 192.168.1.50
```

Modes: `dm elim gun rescue`. Weapons: `all snipers blades`. Maps: `arena foundry bazaar outpost
citadel harbor refinery compound rotate`. A `--ladder` rung is a slot (`ar` primary, `pt` pistol,
`sr` sniper, `gr` grenade, `kn` knife) plus its place in the Armory list, counting from 0. The
menu is easier.

## Privacy

No telemetry, no update checks and no built-in servers. The game only uses the network when you join
or host, and only with the server you choose. That traffic isn't encrypted, and a server sees your
IP address and player name, so only join servers you trust.

## Build from source

Needs gcc 14+, make, and the EGL, GL and X11 development headers:

```sh
make build/game-x86_64 && ./build/game-x86_64
```

`code/game.c` `#include`s the parts under `code/` in a fixed order (a unity build). The meshes in
`code/models.h` are baked with `blender -b --factory-startup -P tools/build_models.py`, and the world
props with `tools/build_props.py`.

## Credits and license

BlindDrop is an independent modified version of
[skill-issue by codehamr](https://codehamr.com). It is not affiliated with or endorsed by codehamr.
It is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE): you can read, change and
share it for noncommercial purposes, but commercial use is not permitted.

Required Notice: Copyright (c) 2026 codehamr (https://codehamr.com)
