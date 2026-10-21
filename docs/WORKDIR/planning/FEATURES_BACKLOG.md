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

### Wallet Mode under attack - done (measured)
When enemy units are near the base (`isLocationSafe` on the base center), Wallet Mode ends its build-up, drops the reserve to $300, allows
up to 12 defenses (4 per type) and trains up to 16 defenders (strongest affordable combat unit, at most a quarter of the bank, at a
barracks / war factory); idle units defend themselves, nothing ever attacks. Measured solo, headless, 14 game-minutes, one enemy AI:
- vs **Medium**: Normal autopilot survives 0/12 (defeated 7.8-9.6 min); Wallet survives 8/12 (the 4 losses at 8.1, 8.6, 11.7, 12.8 min).
- vs **Hard**: nothing survives; Normal lasts ~10.5 min on average, Wallet ~11.8 min (was ~10.7 before the under-attack spending).
  Seen before the fix: a player died with $20k+ banked because its dozers were dead and it had no army.
`autopilot_scenarios.py --enemy E|M|H` runs this comparison (`lost=` column).
Round two (army cap, queue depth and the defense total all scale with the bank; several defenders per factory per cycle): vs Medium still
8/12 survive (the four losses now at 8.6, 10.8, 12.8, 13.5 min); vs Hard still nothing survives (9.7-12.6 min, ~11.4 avg), even with 172
defenders queued across 12 runs. The trace (`autopilot defenders none queued: ...`) shows why more spending does not help: under threat
the bank is only $600-1,100, because the extra defense structures (~$800 each) take the money before the defender trainer runs, and
the player has one production building. Beating a Hard AI needs more than reactive spending (tech, more factories, or a human), so
that is not being chased further.
Cross-machine: a 12-minute Linux + Windows match vs a Medium AI with both humans in Wallet Mode (real combat, 18 defenders queued on
each machine) stayed in sync on all 12 per-minute CRCs with identical final stats.

### Explore mode (autopilot level 5) - done (solo-verified, not yet across machines)
Like Normal, plus explorers that capture neutral buildings (oil derricks and the other tech buildings) for income. It buys the faction's
"Capture Building" upgrade at the barracks (every faction needs it; without it the capture special is "not ready"), trains up to two
capture-capable infantry (Ranger, Red Guard, Rebel), and sends each idle explorer to the nearest safe neutral building owned by the game's
neutral player. A building still in the shroud is first *scouted*: the explorer walks to it, which lifts the shroud, and the next cycle
orders the capture (the game's own capture special, via an AI group, same path as a player's click). A building an explorer fails to take
or reveal is skipped for five minutes. Nothing attacks an enemy. Measured on Death Valley, 12 game-minutes: 8-14 buildings captured
and ~$95-120k earned for 11 of 12 factions (Normal earns ~$25-31k there). On the standard Dark Night map there is nothing to capture,
so level 5 behaves like Normal. Hotkey cycle is now easy / normal / hard / wallet / explore.
Diagnosed with traces: first "not neutral" (the relationship toward an unowned building is not NEUTRAL: test the owner instead), then
"special not ready" (the upgrade), then "in the shroud" (scouting). Built for Challenge mode, which has many neutral buildings; not yet
tried on a Challenge map when this was written; see "Challenge maps from the harness" below.

### Challenge maps from the harness - done (2026-10-21)
`-file Maps\\<Name>.map -soloAutopilot "5:5" -soloMinutes 12 [-soloTemplate FactionChinaInfantryGeneral]` runs a single-player map headless with the
human on Autopilot and prints the stats (docs: `tools/netplay/README.md`). `-file` itself was dead in release builds (inside the `RTS_DEBUG` block of
the command-line table). A Challenge map additionally needs the state the Challenge menu sets up (campaign, `TheChallengeGameInfo`, human slot),
otherwise the map's `ThePlayer` owns nothing and its lose script fires on frame 0; `setupChallengeForMap` does that.
Explore + garrison squad as China Infantry General, 12 game-minutes, Normal difficulty (the map can end the game earlier):
| map | ended | capture / garrison orders | earned | tech captured |
|---|---|---|---|---|
| Air General | map, 11:07 | 5 / 3 | $47.7k | 6 |
| Chem General | map, 11:05 | 0 / 10 | $13.5k | 1 |
| Demolition General | cap | 4 / 7 | $53.4k | 4 |
| Laser General | cap | 2 / 7 | $38.3k | 5 |
| Super Weapons General | map, 6:37 | 2 / 1 | $11.8k | 2 |
| Tank General | map, 11:37 | 4 / 17 | $15.6k | 3 |
| China Boss / Nuke General / Stealth | map, 1:25 / 0:07 / 0:07 | - | - | - (map scripts end the game; not investigated) |
Findings: (1) Challenge maps give their capturable buildings to the *civilian* player (relationship neutral), not the game's neutral player, so Explore
captured nothing on most maps; it now targets those too (Laser $15k -> $38k, Demolition 0 -> 4 captures, Death Valley identical). (2) Most capturable
buildings belong to the opposing general (22 of 31 on Chem), which Explore never touches, so income stays modest. (3) The squad does garrison
(houses, a convenience store, the Palace); on every map some 10 candidate buildings "refuse" (not yet understood; likely the enemy's or already full).
(4) The human, who only builds and defends, is wiped out by the Challenge AI on most maps after 6-11 game-minutes; that is the Wallet-vs-Hard story again.

### Defending against a Hard AI (Wallet mode) - improved, not solved
Goal from Ben: a 1v1 setup that can actually defend against a Hard opponent. Measured solo, headless, 16 game-minute cap, all 12 factions vs one
Hard AI (median survival; none survived 16 minutes until the extra defenses):
- before today's defence work: 11.4 min (9.8-12.1), ~8 kills / 60 losses per game.
- sticky threat flag (90 s) + bigger caps: 11.3 min - **no change**: the faction build lists hold only 1-2 defenses, so the caps never applied.
- **extra defenses beyond the build list** (`AutopilotPlanner::queueExtraDefense`: rotate through the defenses the dozer can build, ring around
  the base, spend a surplus even before an attack): 12.2 min, one run survived all 16; then a lower gate ($5k) and higher caps: **12.9 min**
  (10.9-14.9), ~14 kills per game. The USA still builds only 6-10 (one defense type, cause not yet found); GLA and China 20-30+.
- **units leave a $1,200 defense fund** (the defender trainer spent the bank down to $300 every two seconds, so the planner could never afford the
  ~$1,000 defenses; under attack both the USA and GLA had a bank of only $250-600): **13.4 min** (11.3-16.0), one run survived the full 16, ~16 kills /
  44 losses per game, the USA now builds 12-20 extra defenses (was 6-10). Lesson: units and structures draw on one bank, so the one that matters has to
  be protected, and a decline trace (`extra defense declined: ... too dear for a bank of N`) found it in one run.
- **dozer repair** (`AutopilotRepair`: an idle dozer is sent to the most damaged building below 70% health, wallet and explore): median 13.1 min
  (11.9-16.0), ~18 kills / 45 losses - **no measurable change in the median** (noise is about +-0.3), a slightly better worst run. Kept because the
  healing is free and the Medium / matrix / Explore numbers did not move; it is not what decides a Hard game.
- Against a **Medium** AI: Wallet survivors **12/12** (plain Autopilot 0/12; Wallet was 8/12 this morning).
Findings that mattered: units die piecemeal (60 losses / 8 kills), the bank sat unspent ($14k at death) and the only defense was the one the build
list names. Not yet tried: repair of damaged structures, keeping defenders grouped at the defenses, an anti-vehicle mix tuned to the Hard AI's waves.

### Garrison squad and Hackers (Wallet and Explore modes) - done (solo-verified)
Both requested by Ben. **Garrison squad** (wallet and explore): keeps a standing pair, alternating a "tank person" (Tank Hunter, Missile
Defender, GLA RPG trooper) and a "gun person" (Ranger, Red Guard, Rebel, Minigunner), trained at a barracks while the bank is comfortable and
sent into the nearest building that can hold troops within reach of the base (neutral buildings count; the game decides who may enter);
the explorers are exempt (the first two capture-capable units). **Hackers** (wallet): the planner builds the Internet Center and a barracks,
then China trains up to 6 Hackers and sends them into the Internet Center, where the game data says they hack faster. Measured solo:
- vs a Medium AI, 14 game-minutes: Wallet survivors **8/12 -> 10/12**.
- China Wallet income per game-minute rises from ~$3k (minute 4) to ~$5.8-6.7k by minutes 8-10 with the hackers in; over only 8 minutes
  Wallet earns slightly less ($22.9k vs $24.8k on average) because the barracks, hackers and squad cost money up front.
- Garrison orders happen on maps with enterable buildings within ~1,500 units of the base (Golden Oasis tents, Bitter Winter towers, Death
  Valley watchtowers, Hostile Dawn bunkers); Leipzig Lowlands has none.
Open: the USA squad made no garrison orders on Death Valley in Wallet mode (not yet understood); squad units are not sent to captured
buildings specifically; supply drops (USA) are already covered by the supply logic.

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
      is $3,000 over their cost). [x] Defending units and more defenses under attack (see Wallet Mode under attack). [~] Defend against Hard: median 11.4 -> 13.4 game-minutes (see "Defending against a Hard AI"); a Hard AI still wins in the end.
- [x] Second supply center on GLA factions (fan search around the warehouse).
- [ ] Replace lost trucks/workers explicitly and scale it with the level (today the AI's gatherer logic does it).
- [x] Manually built structures count towards the caps (the planner counts what the player owns). A dozer with a build task pending
      is already skipped by `findDozer`, so a dozer the human ordered to build is left alone.
- [x] Cheat mode across two machines with `-cheats` on both: gifts land on the same frame with identical money/points, in sync.
- [ ] One machine without `-cheats` should desync visibly (untested), and the hotkeys by hand.
- [ ] Re-adopt manual buildings on toggle-on (done for entries with the same template; extra structures the player built that are
      not in the list are ignored).
- [ ] Supply trucks / dozers via the assist; decide on upgrades and general's skills (currently left to the player).
- [ ] Levels now differ in duplicate cap, gatherers and second supply center; make them change build pace and tech choice too.
- [ ] `getPlayerDifficulty()` returns the attached AI's difficulty: check nothing gameplay-visible changes for the human.
- [ ] Control-bar toggle button and an options entry instead of hotkeys only.
- [~] Saved games: a save made with Autopilot on used to be unloadable (the load found "AI present" for a human player). `Player::xfer`
      now saves the autopilot level for that case and re-creates the autopilot AI on load; old saves are unchanged. NOT yet tried by
      actually saving and loading a skirmish (the harness has no save/load path): test by hand, then tick this.
- [x] Rapid on/off/level toggling (15 changes per run, all 12 factions) leaves the player alive and building: no crash.

- [x] Explore for China Infantry General: he has no Red Guard (his barracks builds Minigunner, Tank Hunter, Hacker, Black Lotus), so the explorer
      is the Minigunner; fixed, 8 captured and ~$104k earned on Death Valley (was 0 and ~$29k).
- [ ] Explore: explorers do not run from enemies; the Challenge-map runs (headless) are done, a hand-played one is not.
- [ ] Squad: find out why ~10 nearby garrisonable buildings refuse on every Challenge map; GC_ChinaBoss / GC_Nuke / GC_Stealth end within seconds with Autopilot on.

### Netplay / determinism
- [x] Cross-machine check of the placement/economy/cheat changes (Linux + Windows, Autopilot wallet/normal/hard + cheat gifts): in sync.
- [x] Headless netplay now runs true lockstep (frame tick): every Linux + Windows match since 2026-10-20 evening (autopilot modes, cheats,
      hand orders) is in sync. [ ] Re-run the old pre-fix baselines for the record now that the harness sends `MSG_FRAME_TICK` (true lockstep).
- [x] Commands from a human player across machines: `-lanBuildSchedule` sends a real select + construct command; each machine's
      human ordered a barracks by hand (Autopilot off at 100 s, on again at 190 s) in a Linux + Windows match: in sync on all 6
      per-minute CRCs, identical final stats. Note: with Autopilot on, a human's build click is placed with the AI's relaxed rules.
- [ ] Windows exe packaged for a remote friend (exe + DLLs + launcher .bat, no game data), shared by drive link.
- [x] Retail compatibility, answer: `RETAIL_COMPATIBLE_CRC` is `(0)` by default (`Core/GameEngine/Include/Common/GameDefines.h`), so
      neither the Linux nor the Windows build is expected to be CRC-compatible with retail Zero Hour 1.04 and a match against the
      original game would desync or be refused. Linux <-> Windows builds of THIS fork agree with each other (every match above).
      The autopilot code only changes anything for a player who has switched Autopilot on, so it would not break CRC compatibility
      for ordinary games if that switch were turned on later (it is a compile definition guarded by `#ifndef`).
      [ ] Try a build with `RETAIL_COMPATIBLE_CRC=1` against a real retail client if retail cross-play is ever wanted.
- [ ] Netplay over the internet (Tailscale) between two homes.

### Tooling
- [x] `run-netplay.sh --fast` (skip the frame pacing on both machines): the game phase of a 4 game-minute Linux + Windows match took 68 s instead
      of 247 s (3.6x), both runs IN SYNC. Two runs with the same seed end with slightly different numbers because the network picks the
      frame a scheduled command (autopilot switch, cheat, hand order) lands on from live timing (frame 304 vs 306): consistent across the two
      machines, not a determinism bug. [ ] ~75 s of fixed setup and teardown per match remain (lobby, map load, ssh, log fetch): trim.
- Windows build is not the bottleneck: it is incremental (~1 min for a sim change) on the 16-thread laptop.
- [ ] Linux screenshots are written with a literal backslash in the file name (task spawned; driver works around it).
- [x] Demo video: `tools/netplay/make_demo.sh` runs a visible solo game (Autopilot on, off, a hand-ordered Barracks via the real
      select + construct commands, on again) and stitches in-engine screenshots into a captioned mp4. [ ] A real screen recording with
      mouse clicks on the Windows laptop (ffmpeg gdigrab + SendInput) is still optional and would take over its desktop.
- [ ] Remove the Linux software-Vulkan Xvfb path (`tools/netplay/linux-launch.sh`) or make it work.

### Housekeeping
- [x] Three older fixes (INI parse errors surfaced, recursive file listing fix, locomotor re-specification) committed separately.
- [ ] Push policy: `origin` is a public fork. Decide what goes public (no game data, no credentials, no personal paths).
