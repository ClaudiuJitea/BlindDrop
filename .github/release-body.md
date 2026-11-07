BlindDrop is an arena FPS in one C file. No engine, no asset files: maps, weapons and sounds are all generated in code.

## What's new in 0.7

- **Ladders on Harbor:** five yellow ladders up the warehouse and harbour-office walls. Walk into one to climb, back off to go down, strafe to shift along it and jump to kick off. At the top you step through a gap in the parapet onto the roof, so the rooftops are now sniper positions.
- **Grenade hand rebuilt:** a straight wrist and forearm while holding and cooking, the spoon and pin on the grenade until you cook it, a leather thumb pad, cleaner glove seams and contact shadows between the hand and the grenade.

0.7 uses a new network protocol: everyone in a game needs 0.7.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
