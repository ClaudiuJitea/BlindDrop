BlindDrop is an arena FPS in one C program. No engine and nothing to install: maps and weapons are generated in code, and the sound bank is built into the one binary.

## What's new in 0.7.8

- **Loadouts:** Armory > Loadout picks your rifle (**M4 carbine**, **AK-47** or **FAMAS**) and your pistol (**duty pistol**, **Glock-18** or **Desert Eagle**). Each one has its own model, damage, magazine, fire rate and recoil, plus its own gunshot. Your choice applies right away if you haven't fired yet, otherwise on your next spawn.
- **Recorded sounds:** gunshots, footsteps, impacts, the knife, grenades and the mission sounds now use recorded audio instead of synthesized tones.
- **Kills and deaths:** a kill now sounds like a round hitting a man in body armour, with four takes for body kills and three for headshots, and never the same take twice in a row. Your own death has its own sound (a hit to the chest, a gasp, ringing ears). You now always hear the enemy you killed go down.
- **Reloads:** new recordings for the carbine, sniper and pistol (magazine out, magazine in, bolt or slide, plus gear handling), each landing exactly on the reload animation.
- **Grenades:**
  - **First person:** a new olive frag held further into view.
  - **Overhand throw:** your arm draws back, whips through, releases at full stretch and follows through; it takes 0.83 s, up from 0.47 s.
  - **Underhand lob:** it swings back past the hip first.
  - **In third person:** you hold the grenade up beside your head with your other arm reaching forward, and you throw with your whole body: shoulders turning, chest leaning into it, arm finishing across your body.
- **Third-person rifle carry:** when you look up or down, the rifle and the arms now move together. Before, the rifle pointed at your feet and your elbow stuck out sideways.

0.7.8 uses a new network protocol (16): everyone in a game needs 0.7.8.

This build has no telemetry, no update check and no built-in server. It only uses the network when you join or host a multiplayer game.

**Linux:** download `blinddrop-linux-amd64`, then:

```sh
chmod +x ./blinddrop-linux-amd64
./blinddrop-linux-amd64
```

Needs an x86_64 system with glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

**Windows:** download `blinddrop-windows-amd64.exe` and run it (64-bit Windows 10 or 11). It is not code-signed, so SmartScreen may ask you to confirm: More info > Run anyway.

Check your download with `sha256sum -c SHA256SUMS`.
