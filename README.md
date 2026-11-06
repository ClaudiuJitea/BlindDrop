<p align="center">
  <img src="media/hero.jpg" alt="The mosque courtyard and minaret on Citadel at dusk" width="832">
</p>

<h1 align="center">BlindDrop</h1>

<p align="center">
  <b>A free, private arena FPS in one C file.</b><br>
  Six modes, eight maps, bots that push, and no telemetry.
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

- **A hostage-rescue mission:** sneak into a guarded compound, free three hostages and walk them
  out to a helicopter.
- **Five weapons:** rifle, bolt sniper, sidearm, knife and frag grenades on `1`-`5`, with no loadouts or unlocks.
- **Real grenades:** cook, throw or lob them; they bounce, roll and are stopped by cover.
- **A knife that fights back:** slash chains, a guard, and parries that stagger.
- **Armory:** pick one of five knives (combat, tanto, dagger, kukri, talon), one of eleven weapon
  themes (camo, digital, tiger, fade, gilded and more) and a glove and gauntlet colourway. These
  choices only change how your weapons and gloves look in first person.
- **Movement:** air control, a slide that carries speed, and `Q`/`E` lean.
- **Bots:** they hear you, remember you, flank, throw grenades and find their way around the big maps.
- **A detailed world:** physically based materials, soft shadows, ambient occlusion, light shafts,
  bloom, lamps and lit windows at dusk, and vehicles, palms and props modelled with curved bodies.
- **Weapon handling:** realistic magazine changes (the old one drops, the hand fetches a fresh one
  from the pouch), aim-down-sights scopes, weapon sway and inertia, and a mantle over low walls.
- **No engine and no asset files:** maps, models and every sound are generated in code.

## Maps

| Map | |
|---|---|
| **Arena** | Procedural: a new layout and sky every match, so nobody starts with map knowledge. |
| **Foundry**, **Bazaar**, **Outpost** | Handmade 48 m arenas: a gantry and side halls, a market square with rooftops, and long sniper lanes. |
| **Citadel** | 104 m desert old town with A and B sites: a domed mosque, a climbable minaret, arched gates and rooftop stairs. |
| **Harbor** | 104 m waterfront: brick warehouses, stacked containers, a crane walkway over the yard and a lighthouse. |
| **Refinery** | 104 m industrial site: storage tanks, pipe racks over mid, a flare stack and a distillation-column lookout. |
| **Compound** | 104 m walled desert stronghold at blue hour: a two-storey HQ, a hostage pen, a motor pool, four corner watchtowers and a helicopter pad. It is the Hostage Rescue map and also plays as a normal map. |

**Rotate** moves to the next map every match.

## Modes

| Mode | |
|---|---|
| **Deathmatch** | First to the frag limit. |
| **Gun Game** | Two kills per weapon move you up the ladder; the first knife kill wins. |
| **Snipers Only** | The bolt sniper and nothing else. |
| **Knives + Nades** | The knife, with grenades that regrow. |
| **Last Man Standing** | No respawns; the last one alive takes the round, and you spectate once you're out. |
| **Hostage Rescue** | Operation Nightfall, a single-player stealth mission on Compound (details below). |

Pick the mode and map under **Singleplayer**. They apply on the next restart.

### Hostage Rescue

<p align="center">
  <img src="media/rescue.jpg" alt="Creeping up on the south gate, a sentry growing suspicious" width="49%">
  <img src="media/hostage.jpg" alt="Cutting a hostage's restraints in the pen" width="49%">
  <img src="media/extraction.jpg" alt="The freed hostages reaching the helicopter on the LZ" width="49%">
  <img src="media/compound.jpg" alt="A tower sniper watching over the compound at blue hour" width="49%">
</p>

Three hostages are held in the compound: in the pen, in the motor pool and on the HQ roof. Hold
**R** beside one to cut the restraints. Freed hostages follow you, and the mission ends when all
three reach the helicopter outside the north gate.

- **Loadout:** a suppressed sniper rifle, a pistol and a knife. Only guards within about 15 m hear
  a suppressed shot.
- **Detection:** guards patrol routes, hold posts, and watch from the four towers. A guard that can
  see you fills a meter, shown as an arc around your crosshair and a `?` over his head. The meter
  fills faster when you are close, standing, moving or under a lamp.
- **Spotted:** when the meter fills (`!`), the guard opens fire and radios the alarm unless you
  kill him first. A guard who finds a body radios it in too.
- **Alarm:** every guard hunts for you, and any hostage still tied up is executed when the timer
  runs out.
- **Takedowns:** a knife strike on a guard who is not fighting you is a silent kill.
- **Difficulty:** sets the number of guards (10, 14 or 18), how far and how fast they see, how long
  their radio call takes, and the execution timer (100, 75 or 55 s).
- **Debrief:** shows your time, kills, takedowns and headshots, and a rating: Ghost, Shadow, Silent
  or Loud.

Offline only.

## Download and run

Linux x86_64, glibc 2.38 or newer (Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch, SteamOS 3.7+):

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Check a download with `sha256sum -c SHA256SUMS`. If the frame rate is low, turn off **Post Effects**
in Settings > Video (it switches off SSAO, contact shadows, volumetric light, bloom and motion blur).

**Ballistics** in the Match tab makes rounds fly: they have a travel time, lose speed to drag, drop
under gravity and can ricochet off steel. It is off by default and only applies offline, or on a server
started with `--ballistics`.

## Multiplayer

Anyone can host; up to 8 players per arena, and bots fill the empty slots. The default port is UDP 31415.

```sh
./blinddrop-linux-amd64 --server --mode lms --map citadel   # host
./blinddrop-linux-amd64 --connect 192.168.1.50              # join
```

Modes: `dm gun sniper blades lms`. Maps: `arena foundry bazaar outpost citadel harbor refinery compound rotate`.
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

`make dist-linux RELEASE_VERSION=v0.5` builds the redistributable binary (needs Docker). The
first-person models come from `tools/build_models.py`, regenerated with
`blender -b --factory-startup -P tools/build_models.py`.

## Credits and license

BlindDrop is an independent modified version of
[skill-issue by codehamr](https://codehamr.com). It is not affiliated with or endorsed by codehamr.

It is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE). You can read, change
and share it for noncommercial purposes. Commercial use is not permitted.

Required Notice: Copyright (c) 2026 codehamr (https://codehamr.com)
