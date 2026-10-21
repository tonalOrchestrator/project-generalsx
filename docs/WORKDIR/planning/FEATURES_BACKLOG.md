# Features and Backlog

What this fork (`tonalOrchestrator/project-generalsx`) adds on top of its base, and what is next.

**Lineage:** TheSuperHackers/GeneralsGameCode (upstream engine fixes) -> fbraz3/GeneralsX (Linux/macOS port: SDL3 +
DXVK + OpenAL) -> this fork. Everything below is *added here*; the port itself is not repeated.

Status key: **done** (built and verified), **wip** (in the tree, partly working), **idea** (not started).

---

## Added in this fork

### AI-vs-AI skirmish runner - done (2026-09-30)
`-aiskirmish <map>` runs a skirmish where every slot is an AI, headless (faster than real time, seed-deterministic) or
watchable. Per-player end-of-game stats as JSON, per-AI decision log (`-aiskirmishLog`).
Docs: `PLAN-023_AI_SKIRMISH_HARNESS.md`. Code: `Core/GameEngine/Source/Common/AISkirmishSimulation.cpp`.

### Windows MSVC build on a remote box - done (2026-10-20)
`scripts/remote-build.sh` snapshots the working tree (including new untracked source files), builds it with VS 2022 on the
Windows machine (ssh alias `vadapuppy`) and returns the log and artifacts. Needs
`-DSAGE_USE_DX8=ON -DRTS_BUILD_OPTION_SAGE_PATCH=OFF` (SAGE_USE_DX8 is undefined by default and silently selects the Linux DXVK
headers). MinGW compiles and links but cannot produce a runnable exe (needs the debug-only `d3dx8d.dll`; the SDK's static
`d3dx8.lib` is MSVC-only), so MSVC is the Windows toolchain. MinGW portability fixes are kept.

### LAN netplay harness (Linux host + Windows joiner) - done (2026-10-20)
`-lanhost <map>` / `-lanjoin <ip>`: a scripted LAN game (create, join, accept, start) with AI opponents, no menus.
`tools/netplay/run-netplay.sh` runs both machines (headless or windowed, `--watch` / `--linux-watch`), collects logs,
per-minute state CRCs, per-player stats and in-engine screenshots, and `compare.py` verdicts the run.
Verified: Linux and Windows stay in sync for headless+headless and windowed+windowed matches.
Docs: `tools/netplay/README.md`. Code: `Core/GameEngine/Source/Common/LanNetplayHarness.cpp`.

### Autopilot (build-only AI for a human player) - done (v1)
Hotkey Ctrl+Shift+A toggles, Ctrl+Shift+L cycles the level (Easy / Normal / Hard = the skirmish AI skill states). Goes through the
network command stream (`MSG_SET_AUTOPILOT`), so every machine flips at the same logic frame; verified in sync across
Linux/Windows with on, off, on-at-Hard. Attaches a skirmish AI to the player with army/attack logic disabled, adopts the
structures the player already owns, and shows "Autopilot: ON (Hard)" on screen.
Verified Linux+Windows in sync with on / off / on-at-Hard (6 game-minutes, two humans on autopilot at once): the autopilot bases
grew from 1 to 7-10 buildings and an income started (supply workers/trucks), and growth paused while it was off.
How it builds: a computer player gets its build order from the skirmish *scripts* (most build-list entries are marked
`AutomaticallyBuild = No`), a human has none, so `AISkirmishPlayer::autopilotQueueNextStructure` plays that role: cheapest missing
entry first, per-level cap on duplicates (Easy 1, Normal 2, Hard 3), power plants only when short of power, and missing
prerequisite buildings (the supply center/stash is not in the list; it can itself need a power plant) are queued recursively.

### Autopilot Wallet Mode (level 4) - done, solo-verified
Ctrl+Shift+L now cycles Easy / Normal / Hard / Wallet. Wallet = "defend myself, survive, hoard money": phase 1 builds the
foundation (supply center, power, and the prerequisites on the way to the faction's advanced tech building: strategy center /
propaganda center / palace), phase 2 only builds economy, power and up to 4 base defenses while $1,500 stays in the bank (defenses
that need a prerequisite building only when the bank is also $3,000 over their cost). Solo: ~$21k earned in 8 game-minutes with
spending stopped at the foundation cost.

### Autopilot economy and scenario matrix - done
- Supply centers are placed next to the nearest warehouse with cash (fan search 6-20 cells around it, base side first), the
  engine refuses a center too close to supplies and trucks ignore one with no supplies in reach. Trucks are sent only to
  build-list entries flagged as supply buildings, so autopilot flags the ones it adds (2/3/4 gatherers for Easy/Normal/Hard).
- Hard and Wallet add a second supply center at another unserved warehouse once there is money to spare.
- `tools/netplay/autopilot_scenarios.py` runs every (faction, level) combination (12 factions x 4 levels) headless on one
  machine with `-lanFast` and checks alive / supply center / income / base growth / placement failures; the 48-run matrix takes
  about two minutes. `-lanHostTemplate <Faction>` picks the human's faction, `-lanFast` skips the network frame pacing (a 4
  game-minute run takes 5 seconds, results identical to real time).
- Self-diagnosing logs (harness runs): `NETPLAY autopilot place ... placed|waiting for money|FAILED`, a stall explanation with
  dozer / money / legality codes, and a reason whenever the supply expansion declines.

### Cheat mode - done, verified solo (not yet by hand in play)
`-cheats` (needed on every machine) enables Ctrl+Shift+M (+$10,000) and Ctrl+Shift+G (+1 general's point) through the
network command stream. A machine without `-cheats` skips the command, so the match desyncs visibly instead of anyone
cheating unnoticed. `-lanCheatSchedule "sec:money:points,..."` triggers it from a test run: verified that `-cheats` applies the
money and general's points (frame request + 4) and that without the flag the request is ignored.

---

## Findings worth keeping

- **Headless netplay needs `MSG_FRAME_TICK`.** The network layer only leaves PRE-GAME and closes logic frames when it sees
  the client's per-update frame tick. A headless loop that does not send it runs an independent simulation and silently
  drops every command it would send. The harness now sends it.
- **Headless vs windowed desync** (observed before the tick fix; the earlier headless runs were not true lockstep, so
  re-verify): windowed+windowed on Linux and Windows is in sync.
- `AISkirmishPlayer::adjustBuildList` destroys the player's start command center (an AI gets it rebuilt from its list).
  Autopilot adoption must not call it with the default flag.
- **Placement for a human player is stricter than for the AI** in three places (all relaxed for a human with autopilot on):
  `BuildAssistant::buildObjectNow` (units in the way), `DozerAIUpdate::construct` / `WorkerAIUpdate::construct`
  (`SHROUD_REVEALED`, path and overlap checks). The shroud check made every placement on unexplored ground fail silently; the
  first symptom was the supply stash being retried for four minutes.
- The AI only sends supply trucks to build-list entries flagged `isSupplyBuilding` with a desired gatherer count.
- `LBC_TOO_CLOSE_TO_SUPPLIES`: a supply center must not stand close to a supply source.
- Why autopilot built nothing at first: (1) that command-center destruction, (2) human players have no skirmish scripts and
  almost every build-list entry is script-triggered, (3) prerequisite buildings (supply center/stash) are not in the list.
- `macos_2p_vanilla_map.rep` fails at frame 0 with the same CRC on Linux and Windows (a replay/map issue, not platform).

---

## Backlog

### Autopilot
- [x] Prerequisite retries: caused by the human-only placement checks above, fixed.
- [x] Wallet defenses: USA builds a Patriot battery, China speaker towers, GLA tunnel networks (prerequisite defenses once the bank
      is $3,000 over their cost). [ ] Add defending units and scale the number of defenses with the bank.
- [x] Second supply center on GLA factions (fan search around the warehouse).
- [ ] Replace lost trucks/workers explicitly and scale it with the level (today the AI's gatherer logic does it).
- [x] Manually built structures count towards the caps (the planner counts what the player owns). A dozer with a build task pending
      is already skipped by `findDozer`, so a dozer the human ordered to build is left alone.
- [ ] Cheat mode across two machines (one without `-cheats` should desync visibly) and by hand with the hotkeys.
- [ ] Re-adopt manual buildings on toggle-on (done for entries with the same template; extra structures the player built that are
      not in the list are ignored).
- [ ] Supply trucks / dozers via the assist; decide on upgrades and general's skills (currently left to the player).
- [ ] Levels now differ in duplicate cap, gatherers and second supply center; make them change build pace and tech choice too.
- [ ] `getPlayerDifficulty()` returns the attached AI's difficulty: check nothing gameplay-visible changes for the human.
- [ ] Control-bar toggle button and an options entry instead of hotkeys only.
- [ ] Autopilot state in saved games (currently not saved).

### Netplay / determinism
- [ ] Cross-machine check of the placement/economy changes (running 2026-10-20); re-run all headless netplay baselines now that the harness sends `MSG_FRAME_TICK` (true lockstep).
- [ ] Commands from a human player (real orders) across machines, not just idle + autopilot.
- [ ] Windows exe packaged for a remote friend (exe + DLLs + launcher .bat, no game data), shared by drive link.
- [ ] Retail compatibility: confirm whether `RETAIL_COMPATIBLE_CRC` is on in the Windows build and what that means for
      cross-play with the original game.
- [ ] Netplay over the internet (Tailscale) between two homes.

### Tooling
- [ ] Linux screenshots are written with a literal backslash in the file name (task spawned; driver works around it).
- [ ] Record the demo video (Windows screen capture via ffmpeg) and drive manual clicks with scripted input.
- [ ] Remove the Linux software-Vulkan Xvfb path (`tools/netplay/linux-launch.sh`) or make it work.

### Housekeeping
- [x] Three older fixes (INI parse errors surfaced, recursive file listing fix, locomotor re-specification) committed separately.
- [ ] Push policy: `origin` is a public fork. Decide what goes public (no game data, no credentials, no personal paths).
