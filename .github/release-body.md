BlindDrop is an arena FPS in one C file. No engine, no asset files: maps, weapons and sounds are all generated in code.

## What's new in 0.5

- **Hostage Rescue — Operation Nightfall:** a new single-player stealth mission. Three hostages are held in a guarded desert compound; free them (hold **R** beside one) and walk them out to the helicopter outside the north gate.
  - Guards patrol routes, hold posts and watch from four corner towers. Each one has a detection meter (an arc round your crosshair, `?` and `!` over his head) that fills faster when you are close, standing, moving or under a lamp.
  - A guard who spots you, or finds a body, radios the alarm unless you kill him first. The alarm sends every guard hunting and starts the hostages' execution clock.
  - A suppressed sniper rifle, a pistol and a knife; a knife strike on an unaware guard is a silent takedown.
  - Difficulty sets the guard count (10/14/18), how well they see, the radio delay and the execution timer. The debrief rates you Ghost, Shadow, Silent or Loud.
- **New map, Compound:** a walled stronghold at blue hour with an HQ, a hostage pen, a motor pool, watchtowers and a helicopter pad. It also plays as a normal map in every mode.
- **Vehicles and props rebuilt:** a utility helicopter, saloon cars with real wheel arches and glazing, cab-over trucks with rounded cabs and fenders, date palms with arching fronds, and ribbed steel drums, on every map.
- **Lighting and materials:** physically based shading, soft sun shadows, ambient occlusion and contact shadows, light shafts, bloom, motion blur, gentle eye adaptation, and lamps and windows that light up at dusk.
- **Weapons:** realistic magazine changes on the rifle and sniper (the empty one drops and stays on the floor, the hand fetches a fresh one from the pouch), springier sway and recoil, scope optics, and per-weapon hit markers.
- **Movement:** mantle and vault over low walls and crates.
- **Ballistics (optional):** in the Match tab, rounds can have travel time, drag, drop and ricochets. Off by default; offline or on a server started with `--ballistics`.

0.5 uses the same network protocol as 0.4, so 0.4 and 0.5 players can play together on every map except Compound. Hostage Rescue is offline only.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

Check your download with `sha256sum -c SHA256SUMS`.

A Windows build is not available yet.
