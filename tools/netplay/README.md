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

`run-netplay.sh --fast` skips the real-time frame pacing on both machines (headless runs): the game phase of a match is ~3.6x shorter,
and the machines still have to agree on every checksum. Scheduled commands land on a frame chosen from live timing, so two runs with
the same seed can differ by a couple of frames' worth of results; use the solo scenario matrix for exact before/after comparisons.

## Autopilot scenario matrix

```bash
tools/netplay/autopilot_scenarios.py                                    # 12 factions x levels 2 and 4, 4 game-minutes
tools/netplay/autopilot_scenarios.py --levels 1 2 3 4 --minutes 8       # full matrix (about two minutes)
tools/netplay/autopilot_scenarios.py --factions FactionGLA --levels 4   # one scenario
tools/netplay/autopilot_scenarios.py --schedule "5:1,6:0,7:2,8:4,9:3,10:0"  # custom schedule / stress test (all factions)
```

One human on Autopilot vs one Easy AI, headless, one machine, `-lanFast` (no network pacing). Per scenario it checks the player
is alive, the faction really was the one requested, placements did not fail, the supply center went up, income started and the
base grew. Logs and `summary.json` land in `logs/autopilot-scenarios/<timestamp>/`. Extra engine flags: `-lanHostTemplate
<Faction>` (the host's faction, e.g. `FactionAmericaAirForceGeneral`), `-lanFast`, `-lanCheatSchedule "sec:money:points,..."` (with `-cheats`), `-lanBuildSchedule "sec:barracks,..."` (the local player orders a
structure by hand), `-lanCameraLocal` (camera stays on the local base). `tools/netplay/make_demo.sh` builds the Autopilot demo video.

## Challenge maps and single-player maps (no network)

```bash
cd ~/GeneralsX/GeneralsZH
LD_LIBRARY_PATH=$PWD ./GeneralsXZH -headless -file 'Maps\GC_TankGeneral.map' -soloAutopilot "5:5" -soloMinutes 12 \
    -soloTemplate FactionChinaInfantryGeneral
```

- `-file Maps\<Name>.map` (short form; the engine expands it), `-soloAutopilot "sec:level,..."` turns the human's Autopilot on at those game
  seconds, `-soloMinutes` caps the run (default 10), `-soloTemplate` picks the human's faction (default: the general whose campaign
  starts on that map). Challenge maps: `GC_AirGeneral GC_ChemGeneral GC_ChinaBoss GC_DemolitionGeneral GC_LaserGeneral GC_NukeGeneral
  GC_Stealth GC_SuperWeaponsGeneral GC_TankGeneral`.
- Prints `AISKIRMISH_RESULT {json}` (per-player stats) and the usual `NETPLAY autopilot ...` traces. If the map's own script ends the game
  first, the last snapshot (every 5 game seconds) is printed and the run says so.
- The map may be any mission of a general's campaign; the Challenge setup finds it. A non-Challenge map just starts as plain single-player.
- `-soloSnapshot <seconds>` prints `AUTOPILOT_SNAPSHOT {json}` every N game seconds: what the local player can see (fog respected), see `docs/WORKDIR/planning/AUTOPILOT_DIRECTIVES.md`.
- `compare_summaries.py <before-dir> <after-dir>` diffs two `autopilot_scenarios.py` runs (exit 0 = identical; copy the build to the install dir first).
- Autopilot schedules take an optional module mask: `"sec:level:mask"` (`-lanAutopilotSchedule`, `-soloAutopilot`), bits `1` wallet, `2` defense units, `4` explore, `8` squad, `16` hackers, `32` repair. In game, Ctrl+Shift+D toggles Defense Units on the current level.
