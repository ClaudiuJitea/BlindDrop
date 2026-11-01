Arena FPS in one C file. No engine, no assets, everything procedural.

This build contains no telemetry, no update check and no built-in server. It only opens a network connection when you join or host a multiplayer game.

**Windows:** download `skill-issue-windows-amd64.exe`. It is unsigned, so SmartScreen will warn you: choose **More info**, then **Run anyway**.

**Linux:** download `skill-issue-linux-amd64`, then:

```sh
chmod +x ./skill-issue-linux-amd64
./skill-issue-linux-amd64
```

Needs glibc 2.38 or newer: Ubuntu 23.10+, Debian 13+, Fedora 39+, Arch or SteamOS 3.7+.

Check your download with `sha256sum -c SHA256SUMS --ignore-missing`.
