# Handoff: GeneralsX netplay + Autopilot (as of 2026-10-22)

Start here when resuming. Written so the long Claude sessions behind this work can be archived without losing anything.

## State
- Branch `feat/netplay-autopilot` on Ben's PUBLIC fork `tonalOrchestrator/project-generalsx`. Everything is committed and pushed through `495819555`
  (level 6 Fortress), plus the docs commits of 2026-10-22 (local until Ben says push; see "Working rules").
- The installed game binary `~/GeneralsX/GeneralsZH/GeneralsXZH` equals the build of `495819555`. The Windows laptop (`ssh vadapuppy`) had the exe of
  `474f8fb01` last time it was deployed: rebuild + `tools/netplay/deploy-win.sh` before any Linux + Windows sync check.
- Autopilot (levels 1-6: Easy, Normal, Hard, Wallet, Explore, Fortress; modules: planner, economy, defense, defense units, explorer, squad, hackers, repair,
  snapshot, activity feed) is documented in `AUTOPILOT_ARCHITECTURE.md`; results and open items in `FEATURES_BACKLOG.md`; the next-layer design (directives, UI,
  outside planner) in `AUTOPILOT_DIRECTIVES.md`; the day-by-day story in `docs/DEV_BLOG/2026-10-DIARY.md`.

## The sessions (all Ben's, titles as shown in the Claude app)
1. **Netplay + Autopilot session (2026-10-20 to 21):** two-machine netplay rig, Windows build, Autopilot levels 1-5, Wallet/Explore, garrison squad, hackers, repair, the
   module split, scenario runner, demo assets, website `demo/`, friend package. Its project notes: memory `project_netplay_rig.md`.
2. **"GeneralsX autopilot Challenge map" (2026-10-21 to 22, find it by title in the Claude app's session list):** `-file` Challenge maps from the
   harness, Explore on civilian-owned buildings, snapshot, activity feed, module toggles, Defense Units, level 6 Fortress. Interrupted by the shutdown while investigating
   why Fortress ends early on the Super Weapons General and China Boss maps.
3. **"Fix backslash in Linux screenshot filenames":** finished; its fix is commit `8f17f1f93` on branch `claude/sharp-boyd-94d548` (worktree
   `.claude/worktrees/modest-chatterjee-23ebcf`). NOT applied to `feat/netplay-autopilot` yet: cherry-pick it (and drop the harness workaround in
   `tools/netplay/run-netplay.sh`) when convenient.

## Where the Challenge-map work stopped
Recovered results (also in the backlog): Fortress + Defense Units vs Hard on Death Valley = 10/12 survive (no gain over Fortress alone); Wallet on Golden Oasis =
1/11 survive, median 13.9 min (the run was cut off at 11 of 12). Next steps in order: (1) re-run the Golden Oasis comparison for Wallet and Fortress,
(2) run the Super Weapons General Challenge map with `-soloSnapshot` and see what ends it (the doc commit `b101ef90e` already explains part of it), (3) then the
directive layer (`capture` / `garrison` / `defend`) from `AUTOPILOT_DIRECTIVES.md`, then a control-bar panel.

## Other open items
Backlog "Autopilot", "Netplay / determinism", "Tooling" sections. Notable: a save made with Autopilot on has never been loaded by hand; a hand-played Challenge
map; replacing lost supply trucks; `-cheats` missing on one machine should desync visibly (untested); trim the ~75 s of fixed setup per two-machine match.

## Working rules Ben set (keep following them)
- Do not push without his OK, except at the end of a feature after: a scan of the new commits for secrets / paths / game data / binaries, and a Linux + Windows
  `run-netplay.sh --fast` sync check. Origin is public. Show diffs / ask before committing unless told to commit.
- Weekly Claude allowance: the cap Ben sets each day (it was 40% on 2026-10-20, 50% on 2026-10-21); check `get_usage` after each step and stop at the cap.
- Distribution (the friend package, Drive) is for later and needs his explicit OK. The laptop's Windows Update policy is his to change: only give him the PowerShell.
- No game data in the repo or any zip. Never `pkill -f`; never run two game processes at once on one machine.
- Gameplay changes are judged by survival against `--enemy M|H` and Explore income; structure changes must leave the scenario baselines identical.

## Quick commands
```bash
cmake --build build/linux64-deploy --target z_generals -j 6 && cp build/linux64-deploy/GeneralsMD/GeneralsXZH ~/GeneralsX/GeneralsZH/   # the runner uses the INSTALLED binary
tools/netplay/autopilot_scenarios.py --levels 1 2 3 4 5 6 --minutes 8                  # standard matrix (add --enemy M|H --map "Death Valley" --minutes 14 for survival)
scripts/remote-build.sh -p win32 -t z_generals -D SAGE_USE_DX8=ON -D RTS_BUILD_OPTION_SAGE_PATCH=OFF && tools/netplay/deploy-win.sh
tools/netplay/run-netplay.sh --map "Death Valley" --ai H --minutes 14 --fast --host-template FactionChinaInfantryGeneral --autopilot "10:6"
```

## Artifacts outside the repo (not in git)
- `~/generalsx-friend-package/GeneralsX-Windows-474f8fb017.zip`: Windows package, built and parked, NOT shared (see backlog).
- `~/generalsx-site-assets/`: video, poster, screenshot pair and page text for the Cloudflare site (copies of `demo/`).
