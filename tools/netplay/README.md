# Netplay harness (Linux host + Windows joiner, AI opponents)

Headless two-machine LAN matches for testing cross-platform determinism. No UI, no screens.

## One command

```bash
tools/netplay/run-netplay.sh --map "Dark Night" --ai H,H --minutes 8
tools/netplay/run-netplay.sh --map "Tournament Desert" --ai H,H --observer --minutes 5
```

Linux hosts, the Windows box (ssh alias `vadapuppy`) joins, both run `-headless` through the real
network game loop. Output lands in `logs/netplay/<timestamp>/` (`host.log`, `join.log`, stats JSON) and
`compare.py` prints the verdict: per-minute state CRCs and per-player stats must match on both machines.

- `--ai` takes E/M/H per AI slot, optional `:TemplateName` (e.g. `H:FactionAmericaAirForce`)
- `--observer` makes both machines observers (the AIs fight each other, humans just watch)
- humans + AIs must fit the map's player count

## Engine flags (Core/GameEngine/Source/Common/LanNetplayHarness.cpp)

Host: `-lanhost <map> [-lanHumans N] [-lanSlots H,H] [-lanObserver] [-lanSeed N]`
Join: `-lanjoin <host ip>`
Both: `-headless -lanName X -lanIP a.b.c.d -lanMaxMinutes N -lanStats file.json`

Log lines: `NETPLAY status` (per game minute: frame + state CRC), `NETPLAY progress {json}` (per-player stats),
`NETPLAY_RESULT {json}` (final). The harness fails loudly on lobby timeout, game-start timeout and a
120 s network stall.

## Deploy loop

```bash
cmake --build build/linux64-deploy --target z_generals -j3 && cp build/linux64-deploy/GeneralsMD/GeneralsXZH ~/GeneralsX/GeneralsZH/
scripts/remote-build.sh -p win32 -t z_generals -D SAGE_USE_DX8=ON -D RTS_BUILD_OPTION_SAGE_PATCH=OFF
tools/netplay/deploy-win.sh        # newest artifacts.zip -> C:\GeneralsZH on the laptop
```

## Watching a match (windowed + screenshots)

```bash
tools/netplay/run-netplay.sh --map "Tournament Desert" --ai H,H --observer --minutes 3 --watch --linux-watch --shot-seconds 30
```

- `--watch` runs the Windows side windowed in the laptop's logged-in desktop session (scheduled task with /IT) and fetches the
  in-engine screenshots into `<logdir>/shots/`; `--linux-watch` does the same for the Linux host on this desktop
  (`<logdir>/shots-linux/`). The camera hops between AI bases (`--camera-seconds`).
- The first windowed run on the laptop pops a Windows Firewall prompt that someone has to click Allow once.
- **Do not mix modes.** headless+headless and windowed+windowed are in sync; headless on one side and windowed on the other
  desyncs from the first CRC checkpoint (all tested, Linux and Windows agree whenever the mode matches). The cause is
  a headless-only difference in the simulation (candidates: `createGhostObjectManager(m_headless)`, the multiplayer load screen
  path); not yet investigated. Use the same mode on both machines.

## Notes

- Network games run at real time: N game-minutes take about N real minutes.
- The Windows box joins (outbound first) because its Wi-Fi is the Public firewall profile.
- `linux-launch.sh` runs the windowed game inside Xvfb; software Vulkan ran out of memory on the 9 GB box, so it is unused.
