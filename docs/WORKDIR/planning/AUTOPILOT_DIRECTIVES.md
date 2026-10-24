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
| `setModules` (built) | `AM_*` bit set: wallet, defenseUnits, explore, squad, hackers, repair (superweapons not yet) | optional 2nd argument of `MSG_SET_AUTOPILOT`; `AutopilotProfile::withModules` |
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

## Built so far (2026-10-22)
`MSG_AUTOPILOT_DIRECTIVE` (arg0 kind `AD_*`, arg1 object id, arg2 seconds it holds; one network command applied on the same logic frame everywhere) -> `Player::applyAutopilotDirective`
-> `AIAutopilot::applyDirective` -> `AutopilotDirectives` (8 slots, expiring). Kinds: `AD_CAPTURE` (the explorer weights the named building almost to zero distance, in both its capture
and scout paths, so it is chosen first when eligible; nothing else about carrying it out changes) `AD_GARRISON` (2; the squad scouts the named building if it is in the shroud, then garrisons it, ahead of its usual nearest-building choice), `AD_DEFEND` (3; the squad picks garrison buildings nearest the named object instead of nearest the base centre) and `AD_CLEAR`. `scout` etc. follow the same shape. Squad traces now print `#id`, so a building's id can be read from a run (Death Valley, Explore, China: `-lanDirectiveSchedule "20:2:256:400"` sends the squad to bunker #256 instead of watchtower #228). Test: `-lanDirectiveSchedule "30:1:302:300"`
(Death Valley, one human + one AI: object ids 295-304 are the oil derricks and refineries; the explorer trace prints `#id`).

## In-game panel (built 2026-10-22)
`AutopilotPanel` (client-only, `Source/GameClient/GUI/AutopilotPanel.cpp`): one custom-drawn window in the top-left corner of a match for a human player. Title row: the
level (`AUTOPILOT: Fortress`), click to cycle the level (off -> Fortress, then up and round to off); one check box per module (Wallet, Explore, Superweapons, Squad, Hackers, Repair, Defense
units), click to toggle: it sends `MSG_SET_AUTOPILOT` with the module set, the same command the hotkeys use, so every machine applies it on the same frame; below it the last four
activity-feed lines (`AutopilotLog::recentFeed`, which also shows each directive). It shrinks to a one-line "AUTOPILOT: OFF (click: on)" when off. Text is Arial 11: at 10 and 10 bold some glyphs
render with a stray line above them in this engine's font cache. Test without a mouse: `-lanPanelClickSchedule "sec:x:y,..."` delivers clicks to the panel's click handler;
`tools/netplay/make_panel_demo.sh` records the whole flow (screenshots every 4 s, captioned mp4 plus panel close-ups, output in the git-ignored `demo-out/panel/`). Not built: click-to-direct
(select a building, "capture this"), a text box for plain-language orders, other directive kinds.

## Order of work
Snapshot (done) -> activity feed (done) -> `setModules` toggles + defense-units module (done) -> `capture` / `garrison` / `defend` directives ->
control-bar panel -> outside planner bridge.
