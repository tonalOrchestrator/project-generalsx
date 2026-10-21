# Autopilot architecture

The autopilot is an AI that runs beside a human player (levels 1-5, hotkeys Ctrl+Shift+A / Ctrl+Shift+L). It is built from small modules
with a narrow interface to the AI player, so each feature can be read, changed and tested on its own.

```
Player::setAutopilot(level)            AutopilotProfile::forLevel(level)   <- one table: skill, wallet, explore, banner
        |                                        |
        v                                        v
AISkirmishPlayer::attachAutopilot(profile) --> AIAutopilot  (facade: owns the modules, runs update() each frame)
   (implements AutopilotHost)                   |-- AutopilotPlanner   structures: build order, prerequisites, wallet build-up, stall explanation
                                                |-- AutopilotEconomy   supply: spot search, second supply center, gatherers
                                                |-- AutopilotDefense   threat check, defense budget, defender training (wallet mode)
                                                |-- AutopilotExplorer  capture upgrade, explorers, scouting, blacklist (explore mode)
                                                |-- AutopilotSquad     tank person + gun person trained and garrisoned in nearby buildings (wallet, explore)
                                                |-- AutopilotHackers   Hackers trained and sent into the Internet Center (wallet, China)
                                                `-- AutopilotLog       harness trace helpers (throttled lines the scenario runner parses)
```

- `AutopilotUnits` names the unit roles (capture-capable, rifle, anti-vehicle, hacker) in one place; a template cannot report what a unit can
  do, so roles are recognised by the unit name inside the template name.
- `AutopilotHost` is everything the modules may ask of the AI player (player, base center, dozer, safe-to-build, supply warehouse, build
  timer, the production tick). `AISkirmishPlayer` implements it with one-line wrappers; the modules never touch its internals and nothing
  uses `friend`.
- Each module owns its own state. Switching level in place keeps that state; turning autopilot off deletes the facade.
- Settings that depend on the level live in `AutopilotProfile` only. Adding a level = one row there (+ a banner string).
- Autopilot orders only go through the normal AI paths (dozer construction, production queues, the capture special power), so every machine
  computes the same orders from the same game state. Nothing here uses wall-clock time or libm.

## Verifying a change
The scenario runner is deterministic, so a refactor must leave every number identical:

```bash
tools/netplay/autopilot_scenarios.py --levels 1 2 3 4 5 --minutes 8                   # 60 runs
tools/netplay/autopilot_scenarios.py --levels 4 --enemy M --minutes 14                # wallet vs a Medium AI
tools/netplay/autopilot_scenarios.py --levels 5 --map "Death Valley" --minutes 12     # explore with captures
```

Keep a `summary.json` of each from before the change and compare the tuple (name, earned, spent, buildings, lost, units, finished, alive).
Gameplay changes are judged the same way (survival against `--enemy M|H`, captures and income on Death Valley), then a Linux + Windows
`tools/netplay/run-netplay.sh --fast` match confirms the machines still agree on every checksum.

Related: `FEATURES_BACKLOG.md` (what each level does, measured results and open items), `tools/netplay/README.md` (the tooling).
