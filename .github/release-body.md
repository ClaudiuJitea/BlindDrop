BlindDrop is an arena FPS in one C program. No engine and nothing to install: maps and weapons are generated in code, and the sound bank is built into the one binary.

## What's new in 0.8.7

- **A much bigger Bazaar:** the market square is now the heart of an 84 m market quarter: a roofed souk hallway with paper lanterns and shop counters, alleys strung with washing, a market street under striped canopies, and tall houses all round.
- **Stairs and towers:** stone stairs to six roof terraces, a lantern tower in each corner and two pagoda towers to climb. Bots find their way round it.
- **New outfits:** Sahara, Viper, Nightshade and Abyss replace Coyote, Ranger and Shadow. Slate, Arctic and Reaper stay, and a saved Arctic or Reaper pick carries over.
- **A readable compass:** the forearm compass shows your heading in three big digits over a scrolling N / E / S / W strip.
- **Calmer weapon swaps:** the new weapon comes up in 0.55 s instead of 0.30 s.

0.8.7 uses network protocol 24, so it can't play with older versions.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
