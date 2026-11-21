BlindDrop is an arena FPS in one C program. No engine and nothing to install: maps and weapons are generated in code, and the sound bank is built into the one binary.

## What's new in 0.8.9

- **Arena is a military training zone:** an 88 m camp with a concrete shoot house and a catwalk over it, a six-lane firing range with pop-up targets, an obstacle course, a range control tower and a container village.
- **Six maps grew:** Harbor, Refinery, Citadel, Foundry and Compound now span 140 m, and Outpost 80 m:
  - **Harbor:** a moored cargo ship you can board.
  - **Refinery:** LPG spheres and a tanker loading rack.
  - **Citadel:** a rampart you can walk along, bastions with cannons and a lower town.
  - **Foundry:** a rail yard and a coil warehouse.
  - **Compound:** anti-aircraft guns and a ruined village.
  - **Outpost:** a whole camp with a helipad, a motor pool, a fuel point and a satellite dish.
- **Climbing fixed:** ladders no longer knock you off at the top. All Outpost towers can be climbed, the Refinery tower stairs have headroom, and bots can now reach the Bazaar souk.
- **Pick one gun:** the Weapons setting is now ALL or any single gun + nades, such as AK-47 + NADES or DESERT EAGLE + NADES. Everyone, bots too, spawns with it. Servers take `--weapons all` or a gun key such as `sr0` (the AWP); `snipers` and `blades` still work.
- **Rebuilt guns:** a real banana magazine on the AK-47, and new R8 Revolver and Nova models.
- **Real hostages:** the three hostages are built like the operators: an older man, a young man and a woman, each in their own clothes. They wear bomb vests with a blinking detonator and kneel hooded with their hands tied. Once you cut them loose, the lamp turns green and they follow you.
- **Clearer wounds:** a dark red edge closes in as your health drops, the screen edge facing the shooter glows, and the hit marker points to them for a second.
- **More natural hands:** reshaped shoulders, a firing hand that sits behind the grip, and every finger on the grenade.

0.8.9 uses network protocol 25, so it can't play with older versions.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
