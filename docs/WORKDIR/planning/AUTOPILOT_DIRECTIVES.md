# Autopilot directives (design, not yet built)

Goal: let the player (or an outside planner such as a script or a language model) steer Autopilot with a few high-level orders, while the
engine keeps doing the work the same way on every machine. Today Autopilot has five fixed levels; this note is the layer above them.

## Pieces

1. **Snapshot (built):** `AutopilotSnapshot::json(player)` -> one JSON object of what the player can see: money, power, own structures
   (id, template, position, health), own unit counts, capturable neutral/civilian buildings, visible enemy structures and unit counts. It uses the
   player's own shroud: black shroud is omitted, remembered buildings carry `"fogged":true`. Print it from a harness run with
   `-file <map> -soloAutopilot ... -soloSnapshot <seconds>` (`AUTOPILOT_SNAPSHOT {json}` lines).
2. **Directives (next):** small orders the engine already knows how to carry out. Each is one network command (like `MSG_SET_AUTOPILOT`), so all
   machines apply it on the same logic frame; the simulation never calls an outside program.
3. **Planner (outside the engine):** reads snapshots, writes directives. Can be a person with a menu, a rule script, or a language model; the engine
   cannot tell which.
4. **Activity feed (next):** each directive and each notable Autopilot action ("capturing TechOilDerrick", "queued 2 Tank Hunters for defense")
   printed to the console and to the in-game message line (`TheInGameUI->messageNoFormat`, the same line that shows the Autopilot banner).

## Directive vocabulary (first cut)

| Directive | Arguments | Carried out by (existing code) |
|---|---|---|
| `setModules` | bit set: economy, defenseStructures, defenseUnits, explore, squad, hackers, repair, superweapons | `AutopilotProfile` flags (replaces the single level) |
| `capture` | object id | `AutopilotExplorer` (target override) |
| `garrison` | object id (+ unit role) | `AutopilotSquad` |
| `defend` | x, y, radius | `AutopilotDefense` (defenders rally there) |
| `build` | template, count, optional x, y | `AutopilotPlanner` queue |
| `reserve` | money | defense fund / reserve logic |
| `scout` | x, y | an explorer walks there |
| `clear` | none | drop all standing directives |

Rules: directives expire (a frame count) unless renewed; an impossible directive is reported in the feed, never silently ignored; nothing in the
first cut attacks (an attack module would be a separate, explicit decision).

## In-game UI

No Autopilot control exists besides the hotkeys. Options, smallest first: (a) the message line already used for the banner, as the activity feed;
(b) a control-bar panel (a `.wnd` layout) with the module check boxes and a feed list; (c) click-to-direct (select a building, choose "capture
this" / "garrison here") that sends the same directives. A text box for plain-language orders belongs to the outside planner, not the engine.

## Order of work
Snapshot (done) -> activity feed -> `setModules` toggles + defense-units module -> `capture` / `garrison` / `defend` directives ->
control-bar panel -> outside planner bridge.
