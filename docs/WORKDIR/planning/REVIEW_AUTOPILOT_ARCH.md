# Autopilot architecture review (pre-directive, pre-personality)

Scope: branch `feat/netplay-autopilot`. Paths below are relative to `GeneralsMD/Code/GameEngine/` (`Source/GameLogic/AI/` for the .cpp files,
`Include/GameLogic/` for headers). Line numbers are from the tree as read on 2026-10-22; where I only know the function, I name the function.
This is a critique, not a restatement of `AUTOPILOT_ARCHITECTURE.md`. Nothing here was run.

## 0. Verdict

The module split is a good *file* split and a weak *dependency* split. `AutopilotHost` is narrow only toward the AI player; toward the rest of
the engine every module reaches into `TheGameLogic`, `TheThingFactory`, `TheBuildAssistant`, `TheActionManager` directly and re-scans the whole object
list. The real coupling is three implicit shared resources that no module owns and the facade only arbitrates by call order:
**units, factory queues, money**. Directives will fight over exactly those. Fix the resources first.

## 1. Where the split leaks

### 1.1 Host surface: wrong shape, not wrong width
- `AutopilotHost.h` mixes two audiences. Module-facing: `player`, `baseCenter`, `baseRadius`, `findDozerNear`, `requestDozer`, `isSafeToBuildAt`,
  `findSupplyWarehouse`, `requestUpgrade`. Facade-only driver hooks: `runBaseBuilding`, `tickTeamTimer`, `queueProduction`, `restartTeamTimer`,
  `kickBuildTimer`, and trace-only `buildDelayFrames` / `structureTimerFrames` (used from `AutopilotPlanner::explainStall`). Modules can call the driver
  hooks today (nothing prevents `m_host.restartTeamTimer`). Split into `AutopilotHost` (queries/requests) and `AutopilotDriver` (only `AIAutopilot` sees it).
- `difficulty()` is a leak of the *borrowed AI skill* into module logic: `AutopilotEconomy::desiredGatherers` (Easy 2/Normal 3/Hard 4),
  `AutopilotEconomy::queueSupplyExpansion` (`centerCap`, `m_wallet || DIFFICULTY_HARD`), `AutopilotPlanner::update` (`pass.m_cap`, planner:~67). The profile
  already holds `m_difficulty`; these are profile numbers disguised as an enum switch. Personality data cannot override them today.
- The host is *too narrow* in the opposite direction: it hides AI-player internals but not the world. Every module has its own copy of "scan all objects
  for mine/not-structure/can-attack/not-harvester/not-dozer". Result: O(objects) scans per module per 2 s, and **`AutopilotSquad::garrison` is
  O(units x buildings) with `m_links.isExplorer` (itself an O(objects) scan, Explorer.cpp:45-58) inside `isSquadUnit` (Squad.cpp:39-46), called for every object
  in `train()` (Squad.cpp:61-63) and `garrison()` (107)** - effectively O(n^2) per call.

### 1.2 Shared mutable state (the real coupling)
1. **Unit ownership is implicit and recomputed.** Explorers = "the first `EXPLORER_ROSTER` capture-capable units in object-list order", computed twice with
   duplicated loops (`AutopilotExplorer::isExplorer` Explorer.cpp:45-58 vs `update` 135-137). Squad = "infantry matching the rifle/anti-vehicle name lists minus
   explorers minus hackers". A Ranger is simultaneously a valid explorer candidate, a squad unit (rifle list == capture list, see 1.4) and a standing-army
   unit (`trainStandingArmy` counts every non-structure CAN_ATTACK). Which job it gets depends on object-list position. A `garrison <id>` or `capture <id>`
   directive has no way to say "this unit is mine".
2. **Factory queues are first-come.** Barracks are fed by `trainExplorer`, `Squad::train`, `Hackers::update`, `Defense::trainDefenders`,
   `Defense::trainStandingArmy`, each testing `getProductionCount() == 0` (or `< 4` for defenders) in the fixed order of `AIAutopilot::update` (AIAutopilot.cpp:41-47).
   That order *is* the priority scheme and is undocumented; changing it changes every baseline.
3. **Money has five private reserve rules**: defender `DEFENSE_FUND` 1200, squad `cost + 1500`, explorer `cost + 500`, hackers `cost + 2500`, extra defense reserve
   2500 / 5000 trigger / 3000 prereq slack (planner:~110-112, ~372), budget reserves 200/1500. No component can ask "what may I spend", so a directive
   cannot be funded or protected.
4. **Dozer**: Repair vs Planner: "the planner's orders override this one" (Repair.cpp comment) - works by accident of ordering/idle checks.
5. `isThreatened()` (Defense.cpp) is the only threat sensor and is hard-wired to `baseCenter`; `defend(x,y,r)` has no sensor to hang on.

### 1.3 Modules that know each other / know the profile
- `AutopilotPlanner.h` includes `AutopilotDefense.h` just for `Defense::Budget`. Move `Budget` to its own header; otherwise Defense cannot change without
  recompiling/reading the planner.
- **Planner encodes other modules' needs** (planner:~335-338): `isEconomy` includes Internet Center "if `m_profile.m_hackers`", Barracks "if hackers || squad ||
  defenseUnits", WarFactory "if defenseUnits". Add a module and you must edit the planner. This is the single clearest Open/Closed violation and the thing a
  personality table must replace (see 3).
- `Squad::Links::isExplorer` couples squad to explorer for one bit of information that belongs in a unit-claim ledger (2.2).
- Modules read `m_profile` booleans (`m_wallet`, `m_explore`, `m_hackers`...) rather than being constructed enabled/disabled; `isExplorer` returns FALSE when
  `!m_explore` while `m_explorer[]` keeps stale slots (state survives toggles - noted as a feature in the arch doc, but it means a disabled module's claims linger).

### 1.4 The `m_links` pattern
- Works, and cheap: private inheritance of two tiny interfaces in `AIAutopilot` (AIAutopilot.h) avoids `friend`. But `Planner::Links` has 5 methods of
  which 4 are pure forwards to `AutopilotEconomy` (`desiredGatherers`, `queueSupplyExpansion`, `markSupplyEntry`, `findSupplySpot`) - i.e. the planner depends on
  Economy by another name. Honest fix: planner takes `AutopilotEconomy &` (or a 2-method `SupplyPlanner` interface). Keep `Links` only where the dependency is a
  *different shape* (e.g. `defenseBudget`, which should become a data struct computed once per tick and passed in, removing the callback).
- The pattern does not scale to directives: each new cross-module question becomes another virtual on `AIAutopilot`. Use a shared context object
  (`AutopilotContext { host, profile, ledger, directives, roster }`) constructed once; modules take `AutopilotContext &`.
- `AIAutopilot` copies the profile by value (`m_profile`), modules hold references to it; `setProfile` assigns in place. Safe only because the object is
  never moved. Pin it (`= delete` copy/move) so a later refactor cannot silently dangle.

### 1.5 Role knowledge duplicated, with divergent tie-breaks
- `AutopilotUnits.cpp`: `isCaptureCapableInfantry` and `isRifleInfantry` have **identical** name lists (`ranger, redguard, rebel, minigunner`). Two functions, one
  list. Matching is by lowercase *substring* of the template name, so `ranger` also hits `Lazr_`/`AirF_`/`SupW_` Ranger variants *and* any future mod unit containing
  the string; for a faction personality (General Ben) this is not fine-grained enough.
- "Pick the best affordable template at an idle factory" is written five times with five tie-breaks: Explorer takes the **first** match in template-list order
  (`break`, Explorer.cpp:209-219, though its comment says "strongest"); Squad and Defense take the most expensive with `cost <= pickCost` skipping ties (first wins);
  Hackers takes the first. All depend on `TheThingFactory->firstTemplate()` order (see 4.3).
- `army` counting loop duplicated verbatim in `trainDefenders` and `trainStandingArmy`.

### 1.6 Profile, modules and level in saved games
- `Player::xfer` (Player.cpp:4317-4335) writes `level + 256*(mask+1)` as one Int when "AI present and human". Consequences:
  - Level = **array index** into `s_profiles` (`forLevel(level)` returns `&s_profiles[level-1]`). Inserting or reordering a row re-meanings every old save and
    every recorded command. Rows are effectively append-only forever, unstated anywhere.
  - Packing overflows an Int at mask >= 2^23; fine for 6 bits but the encoding is a trap for a personality system.
  - The sentinel `0xFFFFFFFF` ("level's own set") would collide with a real all-ones mask.
  - `withModules` is lossy: `AM_SQUAD` set on a profile with `m_standingSquad == 0` silently becomes 2; the mask cannot express squad size, difficulty or hackers' cap.
  - The packed Int is written inside the unversioned `if (aiPlayerPresent && human)` branch; extending it needs a version bump at the Player level.
- **No module state is saved**: `m_pending` (a raw `BuildListInfo *`!), `m_ring`, `m_defenseRotation`, `m_nextRole`, `m_nextAntiVehicle`, `m_threatUntilFrame`, explorer slots,
  blacklist, scout counts. `AISkirmishPlayer.h:144` says "not saved, not in the CRC"; `AISkirmishPlayer::crc` is empty (AISkirmishPlayer.cpp:1278-1281).
  A loaded game therefore does not continue as the uninterrupted game would (handoff: "a save made with Autopilot on has never been loaded by hand"). Not a
  netplay desync (every machine resets alike), but any *standing directive* kept in module state would be silently dropped on load. `m_pending` could dangle if the
  build list is rebuilt (`adoptExistingBase` / `setBuildList`).
- `AIAutopilot` is `new`ed and `delete`d by hand in `AISkirmishPlayer` (ctor/dtor at :94, :1047). OK, but then it is not owned by anything the engine can xfer.

## 2. What must change BEFORE directives

### 2.1 Rule: a directive is an input to a module's *selection*, never an order source
If `capture`/`garrison`/`defend` call `aiEnter` / `groupDoSpecialPowerAtObject` themselves, there are two control paths that disagree on unit claims, money
and queues. So every module must expose a pure **decide** step that directives can bias, and a single **act** step:

- `Explorer::chooseTarget(explorer, directives) -> Decision{obj, scouting, reasonCounts}` out of the 150-line `sendIdleExplorers` (Explorer.cpp:229-373), which today mixes
  scanning, trace counters, ordering and slot bookkeeping. Counters (`seen`, `notNeutral`, ...) become the `reason` the feed needs anyway.
- `Squad::chooseBuilding(unit, directives)` out of `garrison()` (Squad.cpp:102-176); same for `Defense`.
- Only `act` touches `ai->...`. This is also what makes order logging/digesting (4.4) one function.

### 2.2 Unit-claim ledger (new, small)
`AutopilotLedger`: fixed array `{ObjectID unit; uint8 owner(module/directive); UnsignedInt untilFrame;}` (e.g. 64 slots, no heap, no hash iteration). API:
`tryClaim(unit, owner, ttl)`, `release`, `ownerOf`. Explorer/Squad/Hackers claim instead of re-deriving roster membership; `Links::isExplorer` disappears.
This removes the object-list-order dependency of roster membership and gives `garrison <id>` something to claim.

### 2.3 Module/capability preconditions
- `capture` is impossible while `AM_EXPLORE` is off: `Explorer::update` returns at once (Explorer.cpp:126) and the capture-upgrade request lives after that return.
  Decide now: either (a) a directive is **rejected with a reason** when its module is off (recommended: explicit, no hidden enabling), or (b) it grants a lease.
  Do not pick "silently ignored" - the DIRECTIVES doc already forbids it.
- `garrison` needs a unit role and the squad size cap (`m_standingSquad`) must not block a directed garrison; `defend` needs a sensor (`threatAt(pos, r)`) and an
  *order* (rally = `aiMoveToPosition`), which is a new order kind: put it in Defense behind the same decide/act split, nowhere else.

### 2.4 Persistence must come first
Directives are authoritative simulation input; a save/load that drops them is a gameplay change. Version `AISkirmishPlayer::xfer` (it has a `version` field,
:1288-1297) and save: personality id, mask, directive store, ledger. Pointer-free (store `ObjectID`s and a template name for `m_pending`, not `BuildListInfo*`).
Put the directive store in `crc()` (it is input, unlike module scratch state).

### 2.5 Minimal data model
```cpp
enum DirectiveKind : UnsignedByte { DK_NONE, DK_CAPTURE, DK_GARRISON, DK_DEFEND };
enum DirectiveStatus : UnsignedByte { DS_PENDING, DS_ACTIVE, DS_DONE, DS_FAILED, DS_EXPIRED, DS_REJECTED };
enum DirectiveReason : UnsignedByte { /* NONE, MODULE_OFF, NO_SUCH_OBJECT, NOT_NEUTRAL, NOT_CAPTURABLE, NO_CAPTURER, SHROUDED,
                                         UNSAFE, NOT_GARRISONABLE, FULL, NO_PATH, STORE_FULL, TTL_EXPIRED, SUPERSEDED */ };
struct Directive {                // 24 bytes, POD, fixed array of 8 in AutopilotDirectives
    UnsignedInt seq;              // assigned at apply time, monotonically increasing; the only ordering key
    UnsignedInt issuedFrame;      // = TheGameLogic->getFrame() when the MSG is applied (same frame on every machine)
    UnsignedInt expiresFrame;     // = issuedFrame + ttlFrames; ttl comes in the message, clamped [10 s, 10 min]
    ObjectID    target;           // capture/garrison; INVALID_ID for defend
    Int         x, y, radius;     // defend; integer world units (no Real in the wire format)
    ObjectID    unit;             // optional: garrison this specific unit; else role
    UnsignedByte kind, status, reason, role;
};
```
- **Wire**: one `MSG_AUTOPILOT_DIRECTIVE` (kind, target, x, y, radius, ttl, role, unit), mirroring `MSG_SET_AUTOPILOT` dispatch
  (`Core/GameEngine/Source/GameLogic/System/GameLogicDispatch.cpp:530-550`): applied in logic, on every machine, same frame. The *sender's* frame is not in the
  message; expiry is computed at apply time. `clear` = kind DK_NONE.
- **Store**: `AutopilotDirectives` owned by `AIAutopilot`; renew = same (kind,target) overwrites in place and extends `expiresFrame`; full store rejects with `STORE_FULL`
  (never evicts silently). Per update (every 2 s, not every frame) sweep: expire, drop DONE/FAILED/REJECTED after they were reported once.
- **Validation** at apply time (object exists, not ours for capture, relationship, module on) so `REJECTED+reason` is produced deterministically on all machines.
- **Feedback**: single function `AutopilotLog::directive(seq, kind, status, reason)` -> `AutopilotLog::feed` (local player only, existing path) and a trace
  line `NETPLAY directive seq=.. kind=.. status=.. reason=.. (frame N)`. Status *transitions* are the only emit points (not per tick), so the feed cannot spam.
  Distinct prefix `NETPLAY directive` so the scenario runner's regexes and the trace digest (4.4) ignore it.
- Memory/ids: no `AsciiString` or pointers in the store (they are CRC/xfer-hostile).

## 3. Personality as data

### 3.1 What is wrong with today's table
`AutopilotProfile` is six booleans + one int + a borrowed `GameDifficulty`; the banner lives in a *parallel* array (`s_banners`) that must stay index-aligned (the
`banner()` clamp hides a mismatch). Wallet is a pure turtle: `queueCheapestCandidate` excludes production/tech/superweapons in wallet mode (planner:~339-340) and there
is no superweapon module (AM comment: "superweapons not yet"). So **"General Wallet" as described (hoard, defenses, superweapons, then break out) needs two modules
that do not exist (Superweapon, Assault) - it is not just a new row.** Same for "General Ben": Hackers and Tank Hunters exist as roles; "minigunner Chinook drops" needs a
Transport/Drop module. Nothing attacks by design (docs); an Assault module is an explicit decision, as `AUTOPILOT_DIRECTIVES.md` already says. Do not smuggle it into Defense.

### 3.2 Proposal: `AutopilotPersonality` (const table, later INI)
```cpp
struct AutopilotPersonality {
    UnsignedShort id;                 // STABLE, never reused, never reordered. Levels 1..6 keep ids 1..6; named generals take 100+.
    const char   *key;                // "Wallet", "Ben" - lookup for harness/UI; never sent on the wire
    const wchar_t *banner;            // folded in; kills the parallel array
    UnsignedInt   sideMask;           // factions it is valid for (Ben: China; Wallet: America Laser). Mismatch => feed message, not silent.
    GameDifficulty skill;             // build caps / timers borrowed from the AI
    UnsignedInt   modules;            // default AM_* set
    AutopilotTuning tune;             // every literal now buried in the .cpp files (reserves, caps, army sizing, roster, hacker max, threat hold, blacklist time)
    const BuildWant *buildWants;      // ordered: {kindOf | name, phase, cap, minBank, priority}
    const RoleSet   *roles;           // role -> ordered template names (+KindOf guard) for this faction
    const Phase     *phases;          // ordered: {name, enterWhen(bank>=, structure present, frame>=), spendPolicy HOARD|SPEND, activeModules, wants}
};
```
- `BuildWant` replaces planner:~335-338 (`isEconomy` hard-coded by module) and the "cheapest first" rule: planner iterates wants in priority order within the current phase.
  Default personalities set priorities so that levels 1-6 reproduce today's behavior (cheapest-first = a mode flag `PICK_CHEAPEST`, not a rewrite).
- `RoleSet` replaces `AutopilotUnits`' substring lists: roles `CAPTURER, RIFLE, ANTI_VEHICLE, HACKER, TANK, TRANSPORT, GARRISON_FILLER, SUPERWEAPON`. Matching: exact template name list first
  (faction prefixes enumerated), KindOf guard second; substring only as the legacy fallback for levels 1-6 so baselines stay identical. Modules ask `roles.is(ROLE, tmpl)` / `roles.best(ROLE, factory, policy)`.
  General Ben = `RIFLE{Minigunner}`, `ANTI_VEHICLE{TankHunter}`, `HACKER{Hacker}`, `TRANSPORT{Chinook}`. General Wallet = different `Phase` list, same module code.
- `Phase` is how "hoard, then spend, then blow through" is expressed: phase 1 foundation (build-up), 2 hoard (`HOARD`, defenses to cap), 3 superweapon(s), 4 breakthrough (enables the Assault
  module, which will not exist until Ben says yes). Phase transitions are pure functions of sim state (bank, structures, frame) - deterministic.
- **Wire/save**: send/save `personality.id` (+ mask). Never an index, never a name hash computed from a string table that could differ between builds. Table in code first; INI later is
  safe only because INI CRC is checked at lobby time (verify that this port still enforces it before moving the table to INI).
- Modules take `const AutopilotPersonality &` (or `.tune`) instead of `AutopilotProfile` bools; `modules` stays the on/off layer so the toggle UI and `withModules` keep working
  (fix `withModules` to carry `tune.squadSize` instead of the magic `2`).

## 4. Determinism risks and test plan

### 4.1 What is already right
No wall clock, no `rand`, no libm trig in autopilot code: grep of `Autopilot*.cpp`/`AIAutopilot.cpp` shows only `WWMath::CosTrig/SinTrig` (table-based, Economy `findSupplySpot` and planner ring) and
integer/float arithmetic; random is not used. Frame comes from `TheGameLogic->getFrame()`. Logging and the feed are output-only. Decisions iterate `getFirstObject()` lists
and linked `BuildListInfo`s - both are sim-ordered.

### 4.2 Float on Win32
`targetWeight` multiplies squared distances by 1.0/16.0 and compares; repair compares `health/max` to `0.7f`; ring radius `baseRadius()*0.8f + ...`. If the Windows build ever uses x87
or contracts FMA, ties/threshold cases can differ from Linux SSE. Confirm the Win32 build is SSE2 `/fp:precise` (the existing "IN SYNC for 10 game-minutes vs Hard" says it is at least
good enough, not proof). Cheap hardening: compare in integer (`Int(dx)*Int(dx)+...`) or add the object ID as the final tie-break everywhere.
`Coord2D::normalize()` in `AutopilotEconomy::findSupplySpot` uses `sqrt` (correctly rounded under IEEE, so fine) - verify it is not routed through a libm `pow`/`hypot`.

### 4.3 Order dependence that is deterministic *today by luck of the data*
- `TheThingFactory->firstTemplate()` order = INI load order. Explorer/Hackers take the first match; Squad/Defense take first-of-equal-max. If the Linux port enumerates INI files in
  `readdir` order and Windows in sorted order (or an upstream sync reorders), the pick differs between machines with *identical* inputs. Every pick must tie-break on template name
  (or ID). This is a behavior change only where ties exist - do it as its own step and expect (and inspect) baseline deltas.
- Object-list order determines explorer roster membership (1.2.1); same on all machines if the sim is in sync, but it makes behavior hypersensitive to unrelated changes
  (e.g. a unit spawned earlier). The ledger removes this dependence.
- `ownedNames[96]` silently drops structures beyond 96 distinct templates (planner:~296-298): harmless now, a quiet bug for big mods.

### 4.4 Test plan
Existing check: `compare_summaries.py` compares (earned, spent, buildings, lost, units, alive) per scenario - it exits non-zero on any difference, which is the right bar, but it is
**coarse**: two different order sequences can produce the same six totals, and it covers one machine only.

Before any refactor:
0. **Self-check**: run the same build twice, compare. If it differs, the baseline is flaky and nothing below means anything.
1. Capture `before` dirs for: `--levels 1 2 3 4 5 6 --minutes 8`; `--levels 4 6 --enemy M --minutes 14`; `--levels 5 6 --map "Death Valley" --minutes 12`; plus a `--schedule` level-switch run. The
   arch doc says 1-5 / 60 runs and the handoff says 1-6: the documented matrix is stale; level 6 and **module-mask (setModules) runs are not in any documented baseline** and there is no
   scenario that loads a save.
2. **Add a trace digest** to `autopilot_scenarios.py` `summary.json`: hash of all `NETPLAY autopilot ... (frame N)` lines except the throttled "status"/"nothing to..." ones (they are
   frame-deterministic too, include them, but exclude prefix `NETPLAY directive`). `compare_summaries.py` includes `trace_digest` in `KEYS`. A pure refactor must keep the digest; this catches
   re-ordered orders that totals hide. (Tooling change; not made here.)
3. **Order digest in the engine**: the act step (2.1) updates a running FNV over `(frame, kind, unitId, targetId)` and prints it every N seconds under `AutopilotLog::enabled()`. Run
   `run-netplay.sh --fast` Linux+Windows, diff the digest lines per frame: a desync is then localized to the first diverging order instead of "checksum mismatch minutes later".
4. **Save/load**: one scenario saves at minute 4 with autopilot on, loads, and finishes; compare against an uninterrupted run once xfer of module state exists (steps 7); until then the load is
   expected to differ, so the test only asserts "loads, autopilot alive, no crash".
5. **Rule for trace code**: nothing inside `if (AutopilotLog::enabled())` may mutate sim state. Today `explainStall` calls `isLocationLegalToBuild` + `removeAllBibs`, and the harness enables logging while retail play does not; keep it
   query-only, and verify one scenario with logging forced off still yields identical summary numbers.
6. After the directive layer: scripted-directive scenarios (a `-lanDirectiveSchedule "sec:kind:id"` mirroring `-lanAutopilotSchedule`, `LanNetplayHarness.cpp:359,685`) and the same Linux/Windows digest compare.
   Note the headless sim also issues `MSG_SET_AUTOPILOT` (`Core/GameEngine/Source/Common/AISkirmishSimulation.cpp:616`): directives must work there too.

## 5. Ranked refactor steps (small, independently verifiable)

"Verify" = the matrix in 4.4 identical (digest included once it exists) unless stated. Risk is for the baseline, not for effort.

| # | Step | Verify | Risk |
|---|---|---|---|
| 0 | Baseline hardening: self-check run, stale-matrix fix, trace digest, mask + level-switch scenarios | self-check identical | none (tools only) |
| 1 | `AutopilotUnits`: one table, `isCaptureCapableInfantry`/`isRifleInfantry` share it; same names | identical | very low |
| 2 | Hoist every literal (1200, 1500, 500, 2500, 5000, 3000, 6/2000/20, 16/1500/40, 90 s, 300 s, roster, hacker max) into `AutopilotTuning` filled with today's values | identical | low |
| 3 | `explorerRoster()` single function used by `isExplorer` and `update` (Explorer.cpp:45-58 / 135-137) | identical | low |
| 4 | Move `Defense::Budget` to its own header; drop `AutopilotPlanner.h` -> `AutopilotDefense.h` include | build only | very low |
| 5 | Replace `m_host.difficulty()` reads (`desiredGatherers`, `centerCap`, `pass.m_cap`) by profile fields holding the same numbers | identical | low |
| 6 | Split `AutopilotHost` into Host (module-facing) and Driver (facade-only) | build only | very low |
| 7 | Persistence: stable personality id, versioned `AISkirmishPlayer::xfer` of module state, pointer-free `m_pending`, old saves still read (legacy `level + 256*(mask+1)` branch kept) | old-save load + save/load scenario; normal scenarios identical | medium (save format) |
| 8 | Shared `countArmy` and `bestAffordable(factory, filter, TieBreak)`; migrate ONE call site per commit, each with its original tie-break as an explicit enum value | identical per site | medium (five variants) |
| 9 | Planner `isEconomy` -> per-module `wantsStructure()` / data `BuildWant`; add template-name final tie-break as a separate commit | identical; tie-break commit: inspect deltas | medium |
| 10 | Pick/act split in Explorer then Squad (`Decision` struct carries the trace counters); order digest in act | identical + digest | medium |
| 11 | Unit-claim ledger replacing `Links::isExplorer`; delete `Squad::Links` | identical (roster semantics preserved: claim = same units) | medium |
| 12 | `AutopilotPersonality` table: fold `s_banners`, ids 1-6 as rows, `sideMask`, `tune`; `AutopilotProfile` becomes a thin alias | identical | low-medium |
| 13 | Directive store + `MSG_AUTOPILOT_DIRECTIVE` + feed; `capture` first (smallest: a target override in `chooseTarget`), then `garrison`, then `defend` | scripted-directive scenarios + Linux/Windows digest | medium-high |
| 14 | New modules (Superweapon, Transport/Drop, Assault) and the named generals as rows/phases | new scenarios only; levels 1-6 must stay identical | high (new behavior; Assault is an explicit product decision) |

## 6. Leave alone
- **The `AutopilotHost` idea itself** and the "no `friend`" rule: it is the part of the design that works. Reshape (step 6), do not widen.
- **`AIAutopilot::update` call order and the 2-second cadence** (AIAutopilot.cpp:37-48): it is the implicit arbiter; touch it only deliberately, as a gameplay change judged by survival/income.
- **Log line formats** that `autopilot_scenarios.py` parses (`place ... priority=1 -> placed`, the `NETPLAY_RESULT` / player JSON lines, the throttled traces) and the `AutopilotLog` throttle
  semantics. New output uses new prefixes.
- **`AutopilotSnapshot`** (read-only, `%.2f` is output only) and **`AutopilotRepair`** (67 lines, no coupling worth paying for).
- **`AutopilotEconomy::findSupplySpot`** geometry (table trig, ordered fan search): correct and deterministic; only its constants move.
- **The legacy save read path** (`level + 256*(mask+1)`, `0xFFFFFFFF` sentinel): keep reading it forever; just stop writing it.
- **The CRC-empty stance for module scratch state** (only directives and ledger enter the CRC).
- **Do not move the personality table to INI** until the INI-CRC lobby check is confirmed on both platforms; code table first.
