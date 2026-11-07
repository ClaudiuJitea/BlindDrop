BlindDrop is an arena FPS in one C file. No engine, no asset files: maps, weapons and sounds are all generated in code.

## What's new in 0.7.3

- **Hostage Rescue co-op:** Operation Nightfall is now a squad mission for one to three operators. Host it from Multiplayer > Host settings (or `--server --mode rescue --squad 3`) and friends join the squad; bots take the places nobody holds and step aside as players join. Squad bots follow the nearest player, crouch when you crouch, cover your flanks, help cut hostages loose and hold fire until the squad is compromised. Every extra operator brings two more guards (up to 18). The mission only fails when the whole squad is down, and a fallen operator watches the rest carry on.
- **Squad setting offline too:** Singleplayer > Hostage Rescue has a SQUAD row: go in solo, or with one or two bot operators.
- **Two forces you can tell apart:** you and your side are operators (helmet, goggles, plate carrier, pack); whoever you fight are insurgents (striped head wrap with a tail, a scarf over the face, a light chest rig over a civilian shirt, no armour). Every map and every mode; team games keep their red and blue on top.
- **Steadier automatic fire:** the rifle and pistol no longer shake the view side to side at full rate; the bolt sniper keeps its full kick.

0.7.3 uses a new network protocol (14): everyone in a game needs 0.7.3.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
