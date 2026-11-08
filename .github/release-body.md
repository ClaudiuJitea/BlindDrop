BlindDrop is an arena FPS in one C file. No engine, no asset files: maps, weapons and sounds are all generated in code.

## What's new in 0.7.7

- **Your operator, your kit:** Armory > Gear now dresses the whole operator, not just the gloves. **OUTFIT** (slate, coyote, ranger, shadow, arctic) colours the fatigues, plate carrier, helmet, webbing, boots, pads, gloves and goggles; **HEADGEAR** is a helmet, cap, head wrap or bare; **VEST** a plate carrier, chest rig or none; **PACK** an assault pack or none. You see it in third person (`V`); team games keep your side's colours on top.
- **Weapons you carry:** in third person the carbine rides on your back, the sniper down your left side and the pistol in a drop-leg holster with leg straps. Switching weapons plays a stow-and-draw: the old weapon goes away, then the new one comes over the shoulder (or out of the holster) to the ready. Bots carry theirs too.
- **Foundry, Bazaar and Outpost rebuilt:** a steelworks yard with furnace stacks, an overhead crane, a railed gantry, roofed sheds, containers and a conveyor; a market quarter with real houses, striped canopies, stalls, palms and a fountain; a forward base with T-wall lanes, HESCO and sandbags, a watchtower, camo nets and container perches. Arena's walls get piers.
- **Cleaner rendering:** characters no longer get a glowing dashed outline on every facet edge in sunlight.
- **Hostage Rescue menu:** the MAP row shows COMPOUND (the mission's own map) instead of a picker that did nothing.

0.7.7 uses a new network protocol (15): everyone in a game needs 0.7.7.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
