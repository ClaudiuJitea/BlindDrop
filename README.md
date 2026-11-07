<p align="center">
  <img src="media/hero.jpg" alt="The mosque courtyard and minaret on Citadel at dusk" width="832">
</p>

<h1 align="center">BlindDrop</h1>

<p align="center">
  <b>A free, private arena FPS in one C file.</b><br>
  Seven modes, eight maps, team play with friends, bots that push, and no telemetry.
</p>

<p align="center">
  <a href="https://github.com/ClaudiuJitea/BlindDrop/releases/latest"><b>Download for Linux and Windows</b></a>
</p>

<p align="center">
  <img src="media/sniper.jpg" alt="Sniper lookout on top of Citadel's minaret" width="49%">
  <img src="media/harbor.jpg" alt="The waterfront dock on Harbor" width="49%">
  <img src="media/refinery.jpg" alt="Pipe racks over mid on Refinery" width="49%">
  <img src="media/citadel.jpg" alt="The market street through mid on Citadel" width="49%">
</p>

## The game

- **Play with friends:** one click hosts a game, and everyone on your network sees it listed and
  joins with another click. **Team Deathmatch** up to 9 v 9: ALPHA in red, BRAVO in blue, players
  choose their side and bots fill the empty places.
- **A hostage-rescue mission:** sneak into a guarded compound, free three hostages and walk them
  out to a helicopter.
- **Five weapons:** rifle, bolt sniper, sidearm, knife and frag grenades on `1`-`5`, with no loadouts or unlocks.
- **Real grenades:** cook, throw or lob them; they bounce, roll and are stopped by cover.
- **A knife that fights back:** slash chains, a guard, and parries that stagger.
- **Match summary and career:** the end screen adds your own match (headshots, best run without
  dying) and lifetime totals saved in `config.cfg`.
- **Armory:** pick one of five knives (combat, tanto, dagger, kukri, talon), one of eleven weapon
  themes (camo, digital, tiger, fade, gilded and more) and a glove and gauntlet colourway. These
  choices only change how your weapons and gloves look in first person. The same page sets the
  weapon's field of view and a lower hip position that clears the middle of the screen.
- **Movement:** air control, a slide that carries speed, and `Q`/`E` lean.
- **Bots:** they hear you, remember you, flank, throw grenades and find their way around the big maps.
- **A detailed world:** physically based materials, soft shadows, ambient occlusion, light shafts,
  bloom, lamps and lit windows at dusk, and vehicles, palms and props modelled with curved bodies.
- **Tracers:** every gun draws its rounds from the muzzle to the impact, and the barrel lines up
  with the crosshair from the hip.
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
| **Team Deathmatch** | ALPHA (red) against BRAVO (blue), 1 v 1 up to 9 v 9; the first side to the frag limit wins. |
| **Hostage Rescue** | Operation Nightfall, a single-player stealth mission on Compound (details below). |

Pick the mode and map under **Singleplayer**. They apply on the next restart. A game you host has
its own mode and map under **Multiplayer > Host settings**.

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

Windows 10 or 11 (64-bit): run `blinddrop-windows-amd64.exe`. It is not code-signed, so SmartScreen
may ask you to confirm (More info > Run anyway). Windows players can play offline and join games;
hosting needs Linux.

Check a download with `sha256sum -c SHA256SUMS`. If the frame rate is low, turn off **Post Effects**
in Settings > Video (it switches off SSAO, contact shadows, volumetric light, bloom and motion blur).
Settings > HUD has **Perf Stats** (frame, CPU and GPU milliseconds on screen), plus crosshair style,
colour and size, HUD scale and opacity, and optional **Enemy Markers** (a shape over enemies you can
see, for players who can't rely on the red helmet).

**Ballistics** in the Match tab makes rounds fly: they have a travel time, lose speed to drag, drop
under gravity and can ricochet off steel. It is off by default and only applies offline, or on a server
started with `--ballistics`.

## Multiplayer

Anyone can host; up to 8 players per arena, and bots fill the empty slots. The default port is UDP 31415.

<p align="center">
  <img src="media/teams.jpg" alt="An ALPHA rifleman in red pushing past two downed BRAVO soldiers in blue on Citadel" width="49%">
  <img src="media/host.jpg" alt="The Host settings page: Team Deathmatch, 5 v 5, bots fill the teams, friendly fire off" width="49%">
</p>

From the menu, open **MULTIPLAYER**:

- **Host a game** starts a server on your PC and puts you in it. **Host settings** (the row under it)
  picks that game's mode, map, bots, difficulty and frag limit, separately from SINGLEPLAYER.
  Leaving the match or closing the game shuts it down. Hosting needs the Linux build; Windows
  players can join.
- **Games on your network** lists every game on your LAN, refreshed every second. Click one to join.
- **Join by address** takes an IP (or `host:port`) for games outside your network. The address is
  remembered. Over the internet the host has to forward UDP 31415 on their router.

A dedicated server works from the command line too:

```sh
./blinddrop-linux-amd64 --server --mode lms --map citadel --name "friday night"   # host
./blinddrop-linux-amd64 --connect 192.168.1.50                                    # join
```

**Team Deathmatch** (`tdm`) puts everyone on ALPHA or BRAVO; the first side to the frag limit wins.
The host sets **TEAM SIZE** (1v1 to 9v9), **FILL TEAMS WITH BOTS** and **FRIENDLY FIRE** (off by
default) in MULTIPLAYER > HOST SETTINGS. With filling on, bots take every place no player holds and
step aside as players join. Joining players get the **CHOOSE TEAM** page (a full side refuses), and
the **TEAM** tile in the menu switches sides mid-match. ALPHA wears red and BRAVO blue; teammates
carry a marker in their team colour over their heads, and names in the kill feed and scoreboard
take their team's colour. Offline, SINGLEPLAYER has Team Deathmatch too, with just TEAM SIZE and
YOUR TEAM: bots fill both sides and friendly fire is off.

```sh
./blinddrop-linux-amd64 --server --mode tdm --team-size 5 --team-fill 1 --friendly-fire 0   # 5v5, bots fill
```

Modes: `dm gun sniper blades lms tdm`. Maps: `arena foundry bazaar outpost citadel harbor refinery compound rotate`.

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
