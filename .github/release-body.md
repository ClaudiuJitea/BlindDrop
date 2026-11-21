BlindDrop is an arena FPS in one C program. No engine and nothing to install: maps and weapons are generated in code, and the sound bank is built into the one binary.

## What's new in 0.9

- **Power slides:** a slide now hits the ground with a dust burst, drags a dust wake and gravel behind the boot and knee, and throws sparks off the glove. The camera banks into the carve, the gun cants in at the hip, and a fast slide streaks wind past the edges of the screen.
- **A natural lean:** the hips shift into the peek over the loaded knee and the upper body bends about 17 degrees instead of snapping over 31, so the view rolls about 11 degrees. The peek reaches as far as before.
- **Smoother weapon swaps:** each gun is drawn on a curve from the holster or sling and settles with a little weight. The support hand comes up to catch it, and in third person the shoulders and torso turn into the reach.
- **Real magazine changes:** every magazine slides out along its own magwell and the hand stays on it. Pistols swap magazines at the belt pouch, and the tactical reload moves without pausing.
- **Proper grips on the MP9 and MP7:** the support hand closes round the vertical foregrip.
- **Dual Berettas:** the slides are whole again, with the open-top slot of a real 92.
- **Compound:** stone-capped perimeter walls with buttresses and wire, a fortified gatehouse, sandbag emplacements, sandstone outcrops, roads and a helipad.

0.9 uses network protocol 26, so it can't play with older versions.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
