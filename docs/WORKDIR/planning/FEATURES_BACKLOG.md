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

### Autopilot (build-only AI for a human player) - wip
Hotkey Ctrl+Shift+A toggles, Ctrl+Shift+L cycles the level (Easy / Normal / Hard = the skirmish AI skill states). Goes through the
network command stream (`MSG_SET_AUTOPILOT`), so every machine flips at the same logic frame; verified in sync across
Linux/Windows with on, off, on-at-Hard. Attaches a skirmish AI to the player with army/attack logic disabled, adopts the
structures the player already owns, and shows "Autopilot: ON (Hard)" on screen.
**Not yet doing its job:** in test runs the autopilot-enabled humans did not place new structures (see backlog).

### Cheat mode - done, untested in play
`-cheats` (needed on every machine) enables Ctrl+Shift+M (+$10,000) and Ctrl+Shift+G (+1 general's point) through the
network command stream. A machine without `-cheats` skips the command, so the match desyncs visibly instead of anyone
cheating unnoticed.

---

## Findings worth keeping

- **Headless netplay needs `MSG_FRAME_TICK`.** The network layer only leaves PRE-GAME and closes logic frames when it sees
  the client's per-update frame tick. A headless loop that does not send it runs an independent simulation and silently
  drops every command it would send. The harness now sends it.
- **Headless vs windowed desync** (observed before the tick fix; the earlier headless runs were not true lockstep, so
  re-verify): windowed+windowed on Linux and Windows is in sync.
- `AISkirmishPlayer::adjustBuildList` destroys the player's start command center (an AI gets it rebuilt from its list).
  Autopilot adoption must not call it with the default flag.
- `macos_2p_vanilla_map.rep` fails at frame 0 with the same CRC on Linux and Windows (a replay/map issue, not platform).

---

## Backlog

### Autopilot
- [ ] Make the build assist actually place structures for a human (investigate why `doBaseBuilding` produces nothing: dozer
      selection, build-list placement around the human's layout, starting money).
- [ ] Supply trucks / dozers via the assist; decide on upgrades and general's skills (currently left to the player).
- [ ] The level should change behaviour (build speed / aggression), not just the difficulty flag.
- [ ] `getPlayerDifficulty()` returns the attached AI's difficulty: check nothing gameplay-visible changes for the human.
- [ ] Control-bar toggle button and an options entry instead of hotkeys only.
- [ ] Autopilot state in saved games (currently not saved).

### Netplay / determinism
- [ ] Re-run all headless netplay baselines now that the harness sends `MSG_FRAME_TICK` (true lockstep).
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
- [ ] Three older uncommitted fixes in the working tree (INI parse errors surfaced, recursive file listing fix, locomotor
      re-specification) still need review and their own commits.
- [ ] Push policy: `origin` is a public fork. Decide what goes public (no game data, no credentials, no personal paths).
