BlindDrop is an arena FPS in one C program. No engine and nothing to install: maps and weapons are generated in code, and the sound bank is built into the one binary.

## What's new in 0.8

- **The full arsenal:** Armory > Loadout now picks from **20 primaries** (M4A4, AK-47, FAMAS, M4A1-S, Galil AR, SG 553, AUG, MAC-10, MP9, MP7, MP5-SD, UMP-45, P90, PP-Bizon, Nova, XM1014, Sawed-Off, MAG-7, M249, Negev), **10 pistols** (P2000, Glock-18, Desert Eagle, USP-S, P250, Five-SeveN, Tec-9, CZ75-Auto, R8 Revolver, Dual Berettas) and **4 snipers** (AWP, SCAR-20, SSG 08, G3SG1), each with its own model, handling and recorded report. `B` switches fire mode where a gun has one.
- **Smoke and incendiary grenades:** the smoke (`6`) blooms into a cloud that fills the space it lands in; the incendiary (`7`) lays a spreading pool of fire. You start every life with **3 frags, 3 smokes and 3 incendiaries**, and the incendiary is now the same size as the smoke.
- **Supply stations** on every handmade map: medical stations give +50 health, ammunition crates restock your grenades. Each empties when used and restocks after 25 seconds; a lamp on its sign shows which.
- **Wounds you can feel:** blood lands on the side a round came from, the screen edge flashes red, and as your health drops the colour drains and the edges crust with blood in time with your heartbeat.
- **Maps rebuilt and dressed:**
  - **Stairs and walkways:** every one now has a proper handrail.
  - **Narrow streets:** covered rooms and stone arches span them in Citadel; pipe bridges in Harbor and Refinery.
  - **New props:** a forklift, oil tanks, sack pallets, benches and power cabinets in the yards and streets.
  - **Outpost:** a climbable HESCO sniper bastion and a barracks tent in each side lane.
  - **Bazaar:** a ladder to the second house's roof.
- **Better-looking props:** a new forklift, and black tyres on every vehicle (they had rendered pale grey).
- **Figures:** no more sagging seat on running or crouching soldiers; hips and thighs now join properly in a stride; heads move with the body instead of hanging still.
- **Face gear:** six new masks in Armory > Gear.

0.8 uses a new network protocol (22): everyone in a game needs 0.8.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
