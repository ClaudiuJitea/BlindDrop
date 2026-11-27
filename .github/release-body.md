BlindDrop is an arena FPS in one C program. No engine and nothing to install: maps and weapons are generated in code, and the sound bank is built into the one binary.

## What's new in 0.9.1

- **Dropped guns:** press **G** (DROP / PICK UP, rebindable) to throw the gun in your hands, or to take the one under your crosshair. It goes into its own slot and your gun from that slot drops where the other one was, so an AWP replaces your sniper and an AK-47 replaces your rifle. Anyone who dies drops the gun they were holding. Walk over a gun to pick it up when that slot is empty. Guns clatter as they land, stay on the floor for 30 seconds and then fade away. The HUD shows what G will do. Rifles, snipers and pistols can be dropped; the knife and grenades can't. Gun Game has no drops.

0.9.1 uses network protocol 27, so it can't play with older versions.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
