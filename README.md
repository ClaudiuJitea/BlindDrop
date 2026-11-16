<p align="center">
  <img src="media/hero.jpg" alt="A Citadel street at dusk: a plastered room carried across the street on stone corbels between two houses, balconies and shutters either side" width="832">
</p>

<h1 align="center">BlindDrop</h1>

<p align="center">
  <b>A free, private arena FPS written from scratch in C.</b><br>
  Seven modes, eight maps, 34 weapons, team play with friends, bots that push, and no telemetry.
</p>

<p align="center">
  <a href="https://github.com/ClaudiuJitea/BlindDrop/releases/latest"><b>Download for Linux and Windows</b></a>
</p>

<p align="center">
  <img src="media/foundry_yard.jpg" alt="Foundry: a yellow forklift by the container stack in the steelworks yard under a storm sky" width="49%">
  <img src="media/bazaar_square.jpg" alt="Bazaar: the fountain square at golden hour, palms, striped canopies and a bench facing the fountain" width="49%">
  <img src="media/citadel_arch.jpg" alt="Citadel: a stone arch with a keystone spanning an alley between two houses at dusk" width="49%">
  <img src="media/refinery_stairs.jpg" alt="Refinery: the stairs up to the catwalk bridge over mid, with yellow handrails, under a golden haze" width="49%">
</p>

## The game

- **Play with friends:** one click hosts a game; everyone on your network sees it and joins with
  another. **Team Deathmatch** up to 9 v 9, ALPHA in red against BRAVO in blue, bots filling the gaps.
- **A hostage-rescue mission:** sneak into a guarded compound, free three hostages and walk them
  out to a helicopter, alone or as a squad of up to three with friends and bots.
- **34 weapons, no unlocks:** pick a primary, a pistol and a sniper under **Armory > Loadout**. Twenty
  primaries (M4A4, AK-47, FAMAS, M4A1-S, Galil AR, SG 553, AUG, seven SMGs, four shotguns, the M249
  and the Negev), ten pistols (P2000 to the R8 Revolver and Dual Berettas) and four snipers (AWP,
  SCAR-20, SSG 08, G3SG1), each with its own model, damage, magazine, rate, recoil and recorded
  report. `B` switches fire mode where a gun has one (a burst, the revolver's fanned hammer).
- **Three grenades, three of each per life:** frags on `5` cook, bounce and roll; the **smoke** on `6`
  pops where it comes to rest and blooms into a volumetric cloud that fills the space it lands in,
  never through a wall, blocking sight but not bullets for 18 seconds; the **incendiary** on `7`
  goes off where it hits the floor and lays a pool of fire that spreads over the ground.
- **Supplies on every handmade map:** medical stations give +50 health and ammunition crates
  restock every grenade. A used station empties and restocks after 25 seconds; the lamp on its sign
  tells you which (lit, or dim red).
- **You feel every hit:** blood lands on the side the round came from, the screen edge flashes red,
  and as your health drops the colour drains, the edges close in and the frame crusts with blood in
  time with your heartbeat.
- **Weapon handling:** realistic magazine changes, scopes, sway and inertia, a short, fast tracer
  on every shot (one pale colour for both sides), a mantle over low walls and ladders to climb.
- **Movement:** air control, a slide that carries speed, and `Q`/`E` lean.
- **Bots** hear you, remember you, flank, throw grenades and find their way around the big maps.
- **Two forces you can tell apart by shape:** you and your side are operators (helmet, goggles,
  plate carrier, pack, a cyan armband and helmet strobe); whoever you're fighting are insurgents
  (red head wrap, dark knee-length tunic, loose trousers, AK), on every map and in every mode.
  Insurgents come in four outlines: rifleman, gunner with an ammo belt, scout in trainers, and a
  leader with a radio. Each one fights to match its kit, and hostile rifles sound like AKs. Team
  games keep their red and blue on top.
- **Third person (`V`):** your operator wears the outfit you pick, carries the rifles he isn't using
  on his back and the pistol in a thigh holster, and stows one weapon and draws the next when you switch.
- **Armory:** five knives and eleven weapon themes; five outfit colourways with a choice of headgear,
  vest and pack for your operator; the weapon's field of view and a lower hip position.
- **Match summary and career:** your headshots and best run each match, lifetime totals in `config.cfg`.
- **No engine and no asset files:** maps are generated in code, and the models and the recorded
  sound bank are compiled into the one binary, with physically based materials, soft shadows,
  ambient occlusion, light shafts and lit windows at dusk.

## Supplies, wounds and the world

<p align="center">
  <img src="media/supply_med.jpg" alt="A medical station: a white cabinet on steel posts, door open, a red trauma bag and a first-aid box on the shelves, a green-cross sign with a lit green lamp above" width="49%">
  <img src="media/supply_ammo.jpg" alt="An ammunition crate: an olive crate with its lid propped open and grenades standing in the foam, a yellow hazard sign with a grenade on a post beside it" width="49%">
  <img src="media/wounds.jpg" alt="First person at 22 health: blood smeared round the edges of the screen and a dark red vignette closing in" width="49%">
  <img src="media/tracers.jpg" alt="An operator firing down a Compound street at blue hour: the muzzle flash and a short pale tracer streaking away" width="49%">
  <img src="media/sedan.jpg" alt="Citadel: an orange early-90s four-door saloon parked by a wall at dusk, framed glass, recessed headlamps, black rubbing strips and alloy wheels" width="49%">
  <img src="media/pickup.jpg" alt="Citadel: a white single-cab pickup at sunset, a chrome-barred grille, round headlamps and amber indicators, door mirrors and alloy wheels" width="49%">
  <img src="media/outpost_bastion.jpg" alt="Outpost: a HESCO sniper bastion with a ladder and a handrail, an ammunition crate in front of it" width="49%">
  <img src="media/truck.jpg" alt="Compound: a cab-over container truck in orange with a white side stripe, chrome grille bars, mirrors on arms and amber roof lamps" width="49%">
  <img src="media/harbor_forklift.jpg" alt="Harbor: a yellow counterbalance forklift with its LPG bottle and forks down beside the container yard" width="49%">
</p>

Walk up to a **medical station** (a white cabinet under a green cross) when you're hurt, or an
**ammunition crate** (under a yellow sign with a grenade) when you're short of grenades: you take
what it holds only if you need it, it empties, and it restocks 25 seconds later. Bots use them too
when they pass one. Every stair and raised walkway has a handrail; narrow streets are spanned by
covered rooms and stone arches in the old towns and pipe bridges at the docks and the refinery; the
yards and streets have saloon cars and pickups, forklifts, container trucks, IBC tanks, sack pallets,
benches and power cabinets, and none of them sits inside another; Outpost has a
climbable HESCO bastion and barracks tents in each side lane, and Bazaar's second house a ladder to
its roof.

## Your operator

<p align="center">
  <img src="media/outfits.jpg" alt="Five operators side by side: slate with helmet and plate carrier, coyote with a cap and chest rig, ranger with a head wrap, shadow bare-headed with no vest, arctic with a helmet" width="49%">
  <img src="media/carry.jpg" alt="An operator from behind with the pistol drawn: the sniper down the left of the pack, the carbine on the right, the pistol holster strapped to the thigh" width="49%">
  <img src="media/swap.jpg" alt="Five frames of a weapon switch: the sniper swung up and stowed on the left side, then the carbine pulled over the right shoulder to the ready" width="49%">
  <img src="media/armory.jpg" alt="The Armory page: knife, weapon theme, outfit, headgear, vest and pack" width="49%">
</p>

<p align="center">
  <img src="media/grenade.jpg" alt="First person in the Bazaar market at golden hour, an olive frag grenade held upright in the gloved right hand" width="49%">
  <img src="media/grenade3p.jpg" alt="The shoulder camera behind an operator holding a grenade up beside his helmet, elbow down and out, ready to throw" width="49%">
  <img src="media/throw.jpg" alt="Five frames of an overhand grenade throw: the hold, cooking it by the ear, the arm drawn back behind the head, the release out front and the follow-through across the body" width="49%">
  <img src="media/loadouts.jpg" alt="The new loadouts in first person: the AK-47 and the FAMAS, the Glock-18 and the Desert Eagle" width="49%">
</p>

Pick your look under **Armory > Gear**: **OUTFIT** (slate, coyote, ranger, shadow, arctic) colours the
whole kit, from fatigues and plate carrier to helmet, gloves and goggles; **HEADGEAR** is a helmet,
cap, head wrap or bare; **FACE** a balaclava, skull mask, gas mask, respirator, goggles, face plate
or shemagh; **VEST** a plate carrier, chest rig or none; **PACK** an assault pack or none.
Press `V` for the shoulder camera. The carbine rides on your back, the sniper down your left side and
the pistol in a drop-leg holster; switching weapons puts the old one away and draws the new one,
over the shoulder for a rifle and from the holster for the pistol. It is all looks: nothing here
changes how a weapon handles, and in team games your side's colours stay on top.

## Maps and modes

| Map | |
|---|---|
| **Arena** | Procedural: a new layout and sky every match. |
| **Foundry** | Handmade 48 m steelworks yard: furnace stacks, a railed gantry, an overhead crane, roofed sheds with oil tanks, containers, a forklift and a conveyor. |
| **Bazaar** | Handmade 48 m market quarter: houses with two climbable roofs, striped canopies over the streets, stalls, palms and a fountain square with benches. |
| **Outpost** | Handmade 48 m forward base: T-wall lanes, HESCO and sandbags, a watchtower, a HESCO sniper bastion and a barracks tent each side, long sniper lines. |
| **Citadel** | 104 m desert old town: a domed mosque, a climbable minaret, arched gates, rooms and arches across the alleys, railed rooftop stairs. |
| **Harbor** | 104 m waterfront: brick warehouses, stacked containers, a crane walkway, pipe bridges, a lighthouse and ladders up to the rooftops. |
| **Refinery** | 104 m industrial site: storage tanks, pipe racks over mid, a railed catwalk bridge, a flare stack and a column lookout. |
| **Compound** | 104 m walled stronghold at blue hour, the Hostage Rescue map; also a normal map. |

| Mode | |
|---|---|
| **Deathmatch** | First to the frag limit. |
| **Team Deathmatch** | ALPHA (red) against BRAVO (blue), 1 v 1 up to 9 v 9; first side to the frag limit. |
| **Gun Game** | Two kills per weapon move you up the ladder; the first knife kill wins. |
| **Snipers Only** | The bolt sniper and nothing else. |
| **Knives + Nades** | The knife, with grenades that regrow. |
| **Last Man Standing** | No respawns; the last one alive takes the round. |
| **Hostage Rescue** | Operation Nightfall, a stealth mission on Compound for a squad of one to three, players and bots. |

Pick them under **Singleplayer** (they apply on the next restart); **Rotate** changes map every
match. A game you host has its own mode and map under **Multiplayer > Host settings**.

### Hostage Rescue

<p align="center">
  <img src="media/rescue.jpg" alt="A guard at the pen in the sniper scope, the operator still undetected" width="49%">
  <img src="media/hostage.jpg" alt="The operator with the three freed hostages at the landing zone" width="49%">
  <img src="media/extraction.jpg" alt="The faded-olive utility helicopter on its pad at blue hour: glazed nose, the cabin door slid open, a green navigation light, skids and tapered rotor blades" width="49%">
  <img src="media/compound.jpg" alt="The compound at blue hour from above the HQ" width="49%">
</p>

Three hostages are held in the pen, the motor pool and on the HQ roof. Hold **R** beside one to
cut the restraints, then lead all three to the helicopter outside the north gate. You carry a
suppressed sniper rifle (heard within about 15 m), a pistol and a knife.

Guards patrol, hold posts and watch from four towers. One who sees you fills a meter (an arc round
your crosshair, `?` over his head), faster when you are close, standing, moving or under a lamp. At
`!` he fires and radios the alarm unless you kill him first, and a guard who finds a body radios it
too. The alarm sends everyone hunting and starts the hostages' execution clock. A knife strike on a
guard who isn't fighting you is a silent takedown. Difficulty sets the guards (10, 14 or 18), their
sight, the radio delay and the execution timer (100, 75 or 55 s); the debrief rates you Ghost,
Shadow, Silent or Loud.

**The squad.** **SQUAD** (Singleplayer, or Host settings) is one to three operators. Players take
the places first and bots fill the rest, stepping aside as friends join. Every operator past the
first brings two more guards, up to 18: a squad of three on Easy faces 14, on Normal or Hard 18.
Squad bots stay a few metres behind the nearest player, crouch when you crouch, watch your flanks
and cut a hostage loose when you're both beside one. They hold fire until a guard is onto the
squad, the alarm is up or a player opens fire. Freed hostages follow whoever cut them free (a bot
hands them on to the nearest player). The mission fails only when the whole squad is down;
a fallen operator watches the others carry on.

## Download and run

**Linux** x86_64 with glibc 2.38+ (Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch, SteamOS 3.7+):

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

**Windows** 10 or 11, 64-bit: run `blinddrop-windows-amd64.exe`. It is not code-signed, so
SmartScreen may ask you to confirm (More info > Run anyway). Windows can play and join; hosting
needs Linux. Check any download with `sha256sum -c SHA256SUMS`.

Low frame rate? Turn off **Post Effects** in Settings > Video. Settings > HUD has **Perf Stats**,
crosshair and HUD options, and **Enemy Markers** (a shape over enemies you can see, for players who
can't rely on colour). **Ballistics** in the Match tab gives rounds travel time, drag, drop and
ricochets; it is off by default and applies offline or on a server started with `--ballistics`.

## Multiplayer

<p align="center">
  <img src="media/teams.jpg" alt="Two ALPHA riflemen in red pushing up a Citadel street past a fallen soldier" width="49%">
  <img src="media/host.jpg" alt="The Host settings page: Team Deathmatch on Foundry, 5 v 5, bots fill the teams, friendly fire off" width="49%">
</p>

Up to 8 players per arena, with bots in the empty slots. Open **MULTIPLAYER**:

- **Host a game** starts a server on your PC and puts you in it; it shuts down when you leave.
  **Host settings** picks its mode, map, bots, difficulty and frag limit. Linux only.
  **Hostage Rescue** hosts the mission as a co-op game: up to three players, bots in the free places.
- **Games on your network** lists every game on your LAN. Click one to join.
- **Join by address** takes an IP or `host:port`. Over the internet the host forwards UDP 31415.

In **Team Deathmatch** the host sets the team size (1 v 1 to 9 v 9), whether bots fill the empty
places (they step aside as players join) and friendly fire (off by default). Joining players pick
ALPHA or BRAVO on the **CHOOSE TEAM** page, and the **TEAM** tile switches sides mid-match.
Teammates carry a marker in their team colour, and the kill feed and scoreboard colour every name
by team. Offline, Singleplayer has Team Deathmatch too: you and bot teammates against a bot team.

A dedicated server works from the command line:

```sh
./blinddrop-linux-amd64 --server --mode tdm --team-size 5 --team-fill 1 --friendly-fire 0
./blinddrop-linux-amd64 --server --mode lms --map citadel --name "friday night"
./blinddrop-linux-amd64 --server --mode rescue --squad 3 --skill 1
./blinddrop-linux-amd64 --connect 192.168.1.50
```

Modes: `dm tdm gun sniper blades lms rescue`. Maps: `arena foundry bazaar outpost citadel harbor refinery compound rotate`.

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

The source is split by area under `code/` (`app`, `core`, `platform`, `net`, `sim`, `world`,
`render`, `figure`, `anim`, `audio`, `ui`, `input` and the headless test `harness`), about 70
files in all, and `code/game.c` `#include`s them in a fixed order so the compiler still builds the
whole game as one translation unit (a "unity build"): every function stays `static`, and there are
no headers to keep in sync. The include list in `code/game.c` is the table of contents; a part may
only use what is included above it. `code/models.h` holds the baked meshes generated by the Blender
scripts in `tools/`.

`make dist-linux RELEASE_VERSION=v0.8.2` builds the redistributable binary (needs Docker). The
first-person models come from `tools/build_models.py`, regenerated with
`blender -b --factory-startup -P tools/build_models.py`; the world props (vehicles, street furniture,
supply stations) are in `tools/props_models.py` and rebuild on their own with
`blender -b --factory-startup -P tools/build_props.py -- PROP_FORKLIFT ...`.

## Credits and license

BlindDrop is an independent modified version of
[skill-issue by codehamr](https://codehamr.com). It is not affiliated with or endorsed by codehamr.

It is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE). You can read, change
and share it for noncommercial purposes. Commercial use is not permitted.

Required Notice: Copyright (c) 2026 codehamr (https://codehamr.com)
