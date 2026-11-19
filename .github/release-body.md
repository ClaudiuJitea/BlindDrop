BlindDrop is an arena FPS in one C program. No engine and nothing to install: maps and weapons are generated in code, and the sound bank is built into the one binary.

## What's new in 0.8.5

- **Rebuilt guns:** all 34 have new shaped bodies modelled on reference photos. The MAC-10, Tec-9, MP7 and P90 have their own grips and hand poses. The stock and butt pad stay on during reloads, and parts of the AK-47 that were only on one side are now on both.
- **Foundry, rebuilt:** new architecture, gates and back doors out of the main yard into the works, and stairs up to some of the big roofs.
- **Cleaner maps:** Citadel plays under a clear afternoon sky and Refinery on a crisp morning, with the brown haze gone; the other maps are tidied up too.
- **A forearm status screen:** your gauntlet shows health (green, then amber, then red), your magazine and your grenades as shapes, and flashes red when you're hit. Change the layout with `wrist_style` in `config.cfg`.

0.8.5 plays with 0.8 to 0.8.2 (same network protocol, 22).

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
