# PLAN-023: Headless AI-vs-AI Skirmish Harness

**Date**: 2026-09-30
**Status**: Steps 1+2 DONE 2026-09-30 (`-aiskirmish`, `Core/GameEngine/Source/Common/AISkirmishSimulation.cpp`). Verified headless Hard-vs-Hard 1v1 on North America: 19 game-min in 23 s real, seed-deterministic (identical JSON on rerun). Step 3 (Python harness) not started. Watch mode (no `-headless`) verified 2026-09-30 after two engine fixes (below).
**Branch**: `main` (build on whatever the current Linux-runnable branch is)
**Concrete first target (2026-09-30)**: two **Hard** AI slots, **random general**, **1v1**, on a very large map, headless, seeded.
**Goal**: run a seeded Zero Hour skirmish with every slot AI-controlled, headless, faster than real time, and dump per-player end-of-game stats as JSON so AI tuning (difficulty, `AIData.ini`, `SkirmishScripts.scb`) can be diffed across runs.

---

## Why this is cheap

TheSuperHackers already shipped the hard part for their replay-compat CI, and this fork carries it:

| Existing piece | Where | Notes |
|---|---|---|
| `-headless` | `Core/GameEngine/Source/Common/CommandLine.cpp:1159` | no window, gfx, input, audio. Sets `TheGlobalData->m_headless`. |
| `-replay <file.rep>` / `-jobs N` | same file, `:1165`, `:1171` | plays replays, wildcards OK, fans across processes |
| `-fps N` | same file | logic frame cap; headless can run far above 30 |
| `-seed N` | same file | fixes RNG; skirmish AI is deterministic given seed + map + slots |
| `-stats` | same file `:1206` → `StatsCollector.cpp` | existing per-message stats writer, ends via `writeFileEnd()` in `GameLogicDispatch.cpp:268` |
| `Player::setPlayerType(PLAYER_COMPUTER, skirmish=true)` | `GeneralsMD/Code/GameEngine/Source/Common/RTS/Player.cpp:738`, called from `initFromDict` at `:865` | this is exactly the switch that makes a slot AI-driven and attaches an `AISkirmishPlayer` |
| `AISkirmishPlayer` | `GeneralsMD/.../GameLogic/AI/AISkirmishPlayer.cpp` | the whole skirmish brain; difficulty is `DIFFICULTY_EASY/NORMAL/HARD` (`GameCommon.h`) |
| Slot types | `Core/GameEngine/Include/GameNetwork/GameInfo.h:40` | `SLOT_EASY_AI`, `SLOT_MED_AI`, `SLOT_BRUTAL_AI`, `GameSlot::isAI()` |
| `ScoreKeeper` | `GeneralsMD/Code/GameEngine/Include/Common/ScoreKeeper.h:84-97` | per-player money earned/spent, units+buildings built/lost/destroyed, captures — only surfaced on the score screen today |
| Score screen suppression in headless | `Core/.../GameLogicDispatch.cpp:273` | already skips `ScoreScreen.wnd` when headless; this is where the JSON dump goes |

## Step 0 — zero-patch smoke test (do this first, no code)

1. Start a normal skirmish: you + N AI slots at chosen difficulties, on a map where the AIs meet each other before they reach you (or just wall in). Touch nothing. Let it play a few minutes, quit. Replays store only human commands, so an idle human's `.rep` is an AI-vs-AI seed.
2. `./run.sh -headless -replay <that>.rep -fps 1000` and confirm it runs to completion and exits 0. This validates that headless + Linux + this game data actually simulates a skirmish end to end. **If this fails, fix that before anything below.**
3. Known limitation of step 0: the game ends the moment the human base dies. That's what Step 1 removes.

## Step 1 — `-aiskirmish` flag (the real patch, ~1 session)

- Add `parseAISkirmish` in `CommandLine.cpp` → `TheWritableGlobalData->m_aiSkirmish = TRUE` (add the field to `GlobalData.h` next to `m_headless`).
- Find where the local human player is set up at skirmish game start: `Player::initFromDict` (`Player.cpp:782`) decides HUMAN vs COMPUTER from the sides dict; `PlayerList.cpp:192` forces the local player to `PLAYER_HUMAN`. With the flag set, instead call `setPlayerType(PLAYER_COMPUTER, true)` for the local slot so it gets an `AISkirmishPlayer` too. Pick the local slot's difficulty from a companion flag (`-aiskirmishDifficulty easy|normal|hard`, default normal).
- Verify victory handling: with no human player, confirm the game still ends when one side is eliminated (`VictoryConditions`), and that headless then reaches `clearGameData` → exits. If the engine assumes a local human for "you lost" detection, that's the yak to shave here — look at how observer/replay mode handles `ThePlayerList->getLocalPlayer()`.
- Determinism check: run the same seed twice, diff the stats dump. Must be identical before any tuning work means anything.

## Step 2 — stats dump

- In `GameLogicDispatch.cpp` right where headless skips the score screen (`:273`), when `m_headless`, iterate `ThePlayerList` and emit one JSON object per active player from `ScoreKeeper` getters (`:84-97`) plus player name, side/faction, difficulty, alive/dead, and the final frame count. Write to `-statsOut <path>` or stdout.
- Keep it flat and stable — this is what a Python script will diff.

## Step 3 — the harness (Python, outside the engine)

- `tools/ai-skirmish.py`: takes map, seed(s), slot spec (faction + difficulty per slot), runs N seeds in parallel (reuse the `-jobs` pattern or just `subprocess` with `-headless -aiskirmish -seed`), collects the JSON, prints a table: win rate per faction/difficulty, average game length, units lost ratio.
- Then the tuning loop: edit `AIData.ini` / `SkirmishScripts.scb` (they live inside the `.big` archives under `~/GeneralsX/GeneralsZH/Data/`; extract with a big-file tool or drop overrides in `Data/INI/` which the engine reads in preference), rerun the same seeds, diff.

## Open questions / risks

- Does the Linux build currently get through a full skirmish to the victory screen at all, non-headless? If skirmish still crashes mid-game on this branch, Step 0 will surface it and it becomes the prerequisite.
- The `-aiskirmish` flip must happen **after** `initFromDict` runs for the local player but before the first logic frame, or the AI's build list (`m_pBuildList`) won't be populated. `Player.cpp:855-870` is the region to read.
- `-fps` may be clamped somewhere in headless; check `GameEngine::execute` timing if it won't go above 30.
- Big-file override precedence needs confirming before relying on `Data/INI/` drop-ins for tuning.

## Not in scope

- Any GUI for this. It's a CLI harness for tuning and regression, nothing else.
- Generals (non-ZH) parity — do ZH first, backport later per CLAUDE.md.

## Usage (as built 2026-09-30)

```bash
cd ~/GeneralsX/GeneralsZH
LD_LIBRARY_PATH=. ./GeneralsXZH -headless -noaudio -aiskirmish "north america" \
  -aiskirmishSlots H,H -aiskirmishSeed 42 -aiskirmishMaxMinutes 60 -aiskirmishStats /tmp/r.json
```
- `-aiskirmish <map>`: map key or case-insensitive display-name substring (prints the multiplayer map list if not found).
- `-aiskirmishSlots E|M|H[:PlayerTemplateName],...` — default `H,H`, factions random unless given (e.g. `H:FactionGLAToxinGeneral,H:FactionChina`).
- `-aiskirmishSeed N` — use this, not `-seed` (GameData.ini `FixedSeed` clobbers `-seed` after the command line is parsed).
- Drop `-headless` to watch it in a window as the observer. The camera jumps to the first AI base 1 s in and hops between AI bases every `-aiskirmishCameraSeconds` (default 45; 0 = only at start). Mouse edge/RMB scrolling still works in between.
- Output: `AISKIRMISH_RESULT {json}` on stdout (+ file), one object per AI with ScoreKeeper totals, `victory`/`defeated`, `endFrame`.
- Engine changes besides the new file: `GameMain.cpp` hook, `Shell.cpp` bypass, `GlobalData` fields, `CommandLine.cpp` flags, `GameLogic::tryStartNewGame` skips the load screen in headless AI mode, `MultiPlayerLoadScreen::init` null-safe when there is no local slot (that was the first crash).

## Build gotcha on this Fedora box (2026-09-30)

`cmake --preset linux64-deploy` fails with `Targets not yet defined: ZLIB::ZLIBSTATIC`: Fedora's
`zlib-ng-compat-devel` ships `/usr/lib64/cmake/ZLIB/zlib-config.cmake`; FindZLIB (module, via vcpkg wrapper)
defines `ZLIB::ZLIB` first, then `PNGConfig.cmake`'s CONFIG-mode `find_dependency(ZLIB)` loads the system
config whose targets file demands both `ZLIB::ZLIB` and `ZLIB::ZLIBSTATIC`. `CMAKE_IGNORE_PATH` does not apply
to config search. Fix in use: a shim `build/zlib-shim/lib/cmake/ZLIB/ZLIBConfig.cmake` that reuses the existing
target, passed via `-DCMAKE_PREFIX_PATH=$PWD/build/zlib-shim`, and `-UZLIB_DIR` once to drop the cached system
path. Needs `VCPKG_ROOT=~/GeneralsX/vcpkg` exported. The shim lives under `build/` (untracked); consider
promoting it to `cmake/` + the preset if it keeps being needed.

## Engine bugs found while making watch mode work (2026-09-30)

1. **Screen-edge scroll runs with a phantom cursor at (0,0).** `Mouse` emits `MSG_RAW_MOUSE_POSITION` every
   frame whether or not it ever saw the pointer; `LookAtTranslator::m_currentPos` starts at (0,0), which is
   inside the top-left edge-scroll zone, so the camera is dragged into the map corner every frame. On Wayland a
   window created with the pointer elsewhere gets neither ENTER nor LEAVE, so `Mouse::isCursorInside()` (default
   true) never flips. Fix: `SDL3Mouse::init` seeds the inside/outside state from `SDL_GetMouseFocus()`, and
   `LookAtXlat.cpp` skips `SCROLL_SCREENEDGE` while `!TheMouse->isCursorInside()`. This affects any windowed
   launch, not just AI skirmish - worth its own upstream PR.
2. **`MultiPlayerLoadScreen::init` dereferences `getSlot(getLocalSlotNum())` unguarded** - null when there is no
   human slot. Null-safed (treats as observer); headless AI mode also skips the load screen entirely.
3. **Black window in watch mode: `m_breakTheMovie`.** `Intro::doPostIntro()` sets `TheGlobalData->m_breakTheMovie`, and
   `W3DDisplay::draw` skips `WW3D::Begin_Render` entirely while it is set (`WW3D::FrameCount` stayed 0). It is
   normally cleared by `MainMenuInit` or `ShellGameLoadScreen::init`, neither of which runs when the shell is
   bypassed and the multiplayer load screen is used. `AISkirmishSimulation::updateWatchCamera` clears it once
   in-game. Found by `gdb -p` + `p WW3D::FrameCount` and stepping `W3DDisplay::draw` to the skipped branch.
   Note: in-engine screenshots (`-aiskirmishScreenshotSeconds`) copy the DXVK back buffer after present and
   are reliable only once rendering actually happens.
4. `-seed` is clobbered by `GameData.ini FixedSeed` after command-line parsing (pre-existing; `-aiskirmishSeed`
   added as the reliable path).

## Per-AI decision log (2026-09-30)

`-aiskirmishLog 1` (default in AI-skirmish mode; `0` = off) prints one line per AI decision, tagged `<side>#<slot>`
because both AIs are named "Hard Army". `-aiskirmishLogFile <path>` also writes it to a file.

```
AI[00:33] GLAToxinGeneral#2      build Chem_GLAStingerSite with dozer at 1160,2831 ($900)
AI[00:44] ChinaTankGeneral#1     base defense Tank_ChinaGattlingCannon (center)
AI[00:56] GLAToxinGeneral#2      upgrade Upgrade_InfantryCaptureBuilding queued at Chem_GLABarracks
AI[01:00] ChinaTankGeneral#1     -- status: $14650  units built 6 lost 0 killed 0 | buildings built 3 lost 0 destroyed 0
```

Event kinds and where they hook (all `AISkirmishSimulation::aiLog`, no-op outside this mode):
`build … (instant|with dozer)` `AIPlayer::buildStructureNow/buildStructureWithDozer` · `completed` `AIPlayer::onStructureProduced` ·
`train X at F for team T (n/m)` `AIPlayer::startTraining` + `AISkirmishPlayer::startTraining` · `unit ready` `AIPlayer::onUnitProduced` ·
`upgrade … queued` `AIPlayer::buildUpgrade` · `upgrade complete` `Player::onUpgradeCompleted` (computer players only) ·
`team … queued/launched` `AIPlayer::buildSpecificAITeam` / `checkReadyTeams` · `base defense` `AISkirmishPlayer::buildAIBaseDefenseStructure` ·
`targeting enemy` `AISkirmishPlayer::acquireEnemy` · `-- status` once per game minute from `AISkirmishSimulation::perFrame`.
Team names come straight from `SkirmishScripts.scb` (e.g. "China Replace Dozer. 30"), so this doubles as a trace of which
script teams the AI is actually picking - the tuning signal for `AIData.ini` / `SkirmishScripts.scb` work.
