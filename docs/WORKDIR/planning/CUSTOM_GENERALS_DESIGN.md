# Custom generals ("General Wallet", "General Ben") - design

Status: design only. Nothing here is built, installed or run. Every engine statement below comes from reading the code at the
`file:line` given; nothing was tested in the running game. Retail archives were only listed/read (never copied).

Goal (Ben): personalised generals for nostalgia, with his own photos as portraits.
- **General Wallet** - Laser General flavour: hoard supply drops, defenses, superweapons, then blow through bases.
- **General Ben** - Infantry General flavour: Hackers, minigunner Chinook drops, Tank Hunters.

Their play style partly exists as the Autopilot AI (`AUTOPILOT_ARCHITECTURE.md`, `FEATURES_BACKLOG.md`): Wallet mode (`AM_WALLET`), Hackers
(`AM_HACKERS`), Defense Units (rotating tank / Tank Hunter / rifleman). What Autopilot does NOT do yet: build superweapons, attack, drop troops
(`AUTOPILOT_DIRECTIVES.md`: "nothing in the first cut attacks"; `AutopilotProfile.h` has no superweapon or attack module).

---------------------------------------------------------------------------------------------------------------------------------------------

## 1. Summary of the approach

Three tiers, each usable on its own:

| Tier | Content | Engine change | Works on |
|---|---|---|---|
| 1. Data pack | New PlayerTemplates (new `Side`), reskinned objects, command sets, sciences, AI build lists, images, loose files only | none | GeneralsX; the INI parts also parse in original ZH (same code lineage, untested) |
| 2. Small engine quality fixes | Portrait on the load screen without a Challenge persona; `ParentSide` fallback so a new side reuses Eva / AI build list / control bar of its parent; partial string table already supported | ~3 files, ~60 lines | GeneralsX only |
| 3. Personalities | PlayerTemplate field that picks an Autopilot module preset; new Autopilot modules (superweapon, attack wave, helicopter/Chinook drop) | Player + Autopilot* | GeneralsX only |

Nothing in the plan edits a retail `.big`. Everything is loose files (or, optionally, one extra `.big` that does not collide with retail names).

---------------------------------------------------------------------------------------------------------------------------------------------

## 2. How the engine loads data (findings)

### 2.1 Loose files beat archives, per file name
- `FileSystem::openFile` asks `TheLocalFileSystem` first and only then `TheArchiveFileSystem`: `Core/GameEngine/Source/Common/System/FileSystem.cpp:175-220`.
  `doesFileExist` is local-first too (`:226-281`).
- So a loose file with the exact retail name **replaces that whole file** (the archived copy is never read). `Data\INI\Object\LaserGeneral.ini` loose
  would hide all 750 KB of the archived one. A mod must never reuse a retail file name.
- `getFileListInDirectory` **merges** local and archive listings (`FileSystem.cpp:286-291`). This is the additive hook (see 2.2).
- Retail `.big` files are loaded from the working directory **recursively** (`StdBIGFileSystem.cpp:414`, `:651-653` pass `TRUE` for subdirectories),
  with `overwrite = FALSE`. Consequence: any `.big` dropped anywhere under the game directory is picked up. A mod folder must not contain stray `.big`s.
  (`Data\INI\INIZH.big` is skipped on purpose, `StdBIGFileSystem.cpp:660-664`.)
- `-mod <file.big|dir>` exists (`CommandLine.cpp:1306-1311`, `ArchiveFileSystem.cpp:213-237`) and loads with `overwrite = TRUE`
  (archive entry replaces archive entry by name). It is the heavier mechanism; Rise of the Reds already uses it on this box
  (`<mods dir>/RiseOfTheReds/!Rotr_*.big`). Not needed here.

### 2.2 INI loading: `X.ini` plus an `X/` directory
Every subsystem calls `INI::loadFileDirectory("Data\\INI\\<Name>")` (via `SubsystemInterfaceList::initSubsystem`, `SubsystemInterface.cpp:180,187`,
always `INI_LOAD_OVERWRITE`):
1. `Data\INI\<Name>.ini` is loaded if it exists (local-first, so a loose copy would shadow the archive file) - `INI.cpp:194-247`.
2. then **all `*.ini` in `Data\INI\<Name>\`, recursively**, local + archive merged, sorted case-insensitively (`FilenameList` is a
   `std::set` with `less_than_nocase`, `FileSystem.h:66`); files directly in the directory first, sub-directories after (`INI.cpp:254-297`).

So the additive install point is a **new file inside a directory named after the INI**, e.g. `Data/INI/PlayerTemplate/CustomGenerals.ini`. Retail
`PlayerTemplate.ini` is read first, ours after it.

Where each INI type is loaded (all `Data\INI\<Name>` + `Default\<Name>` unless noted):

| Content | Loaded at | CRC'd into `m_iniCRC`? |
|---|---|---|
| PlayerTemplate | `GameEngine.cpp:655` | yes |
| Object (incl. `ObjectReskin`) | `GameEngine.cpp:685` | yes |
| Science, Upgrade, SpecialPower, Weapon, OCL, FXList, Armor, Locomotor, Crate, Multiplayer | `GameEngine.cpp:621-672,700,715` | yes |
| AIData (SideInfo, SkirmishBuildList) | `GameEngine.cpp:712` | yes |
| CommandButton, CommandSet | `ControlBar.cpp:1159-1163` | **no** |
| ControlBarScheme | `ControlBarScheme.cpp:1013-1014` | no |
| MappedImages (`TextureSize_512`, `HandCreated`) | `Image.cpp:246-280` | no |
| ChallengeMode | `ChallengeGenerals.cpp:58` | no |
| Eva | `Eva.cpp:257` | no |
| Voice, Speech | `GameAudio.cpp:227,230` | no |

`INI::readLine` feeds every line of the INIs that were given an Xfer into `m_iniCRC` (`INI.cpp:576-579`; total stored at `GameEngine.cpp:765`).

### 2.3 What a re-declared block does (this decides how a mod must be written)
| Block | Same name again | Evidence |
|---|---|---|
| `PlayerTemplate` | **updates the existing template in place**; a new name is appended to the end of the vector | `PlayerTemplate.cpp:387-408` |
| `AIData` `SideInfo` / `SkirmishBuildList` | new side is added; same side replaces | `AI.cpp:56,63-78,204-244,276-286` |
| `Science` | updates (OVERWRITE) | `Science.cpp:155-200` |
| `MappedImage` | new name creates; same name re-parses over the existing image | `INIMappedImage.cpp:54-90` |
| `Object` | `DEBUG_CRASH` duplicate, then overwritten (debug asserts; release silently replaces) | `ThingFactory.cpp:396-404` |
| `ObjectReskin <New> <From>` | copies `<From>` then applies the block; `<From>` must already be defined | `ThingFactory.cpp:410-422` |
| `CommandSet` | **throws `INI_INVALID_DATA`** (parse error) - cannot be overridden by name | `ControlBar.cpp:2144-2154` |
| unknown field in a block | `DEBUG_CRASH` only (ignored in release) | `INI.cpp:1626` |

Rules that follow: use **new names** for everything (Objects, CommandSets, CommandButtons, Sciences, MappedImages); never re-declare a CommandSet; a
PlayerTemplate may be re-declared if we want to patch a retail general in place (not the plan).

**Caveat found while reading, untested:** the local listing returns forward-slash paths (`StdLocalFileSystem.cpp:286-330`) while the archive listing has
backslashes, so the case-insensitive set will not de-duplicate a loose file that shares a name with an archived one; both entries would open the same
loose file and it would be parsed twice. Another reason to never reuse retail file names.

**Linux case rule:** the local directory scan uses the path exactly as given (`StdLocalFileSystem.cpp:286-300`, plain `directory_iterator`), unlike
file opens, which fall back to case-insensitive matching (`:60-170`). Directory names must therefore match the engine's spelling exactly:
`PlayerTemplate`, `Object`, `CommandSet`, `CommandButton`, `Science`, `AIData`, `ControlBarScheme`, `MappedImages/HandCreated`, `ChallengeMode`.

### 2.4 Where the loose root is
The loose root is the engine's working directory (`[ASSET_ROOT] Selected source=current-working-directory`, `StdBIGFileSystem.cpp:414-420`;
`run-linux-zh.sh` does `cd "${GAME_DIR}"`). On the dev box the `.big` files sit in `<game dir>/Data/` and the retail loose folder is
`<game dir>/Data/Data/` (Cursors, English/Movies, WaterPlane). `<game dir>/Window/Menus/ExtrasMenu.wnd` is an existing loose
file, so loose lookup works from `<game dir>/` as well. **The exact directory that `Data/INI/...` must live under needs one confirming
run (log line `loading ... into the directory tree`); see Open Questions.** Retail Windows installs: next to `generals.exe`.

### 2.5 Strings (display names)
- Names come from `generals.csf` (`DisplayName = INI:FactionX` goes through `parseAndTranslateLabel`, `INI.cpp:904-917`; lobby entries use `SIDE:<Side>`,
  `GUIUtil.cpp:271`). A missing label shows `MISSING: 'label'` (`GameText.cpp:1408`); it is not an error.
- The CSF is one file per language (`Data\English\generals.csf` in `EnglishZH.big`). A loose file of that name would replace the whole table.
- **GeneralsX already has the way out:** when the primary is a CSF it also loads the *archived* copy as a fallback (`GameText.cpp:389`, instance 1 skips
  the loose file), so a small loose `Data/English/generals.csf` that holds only our labels works and every other label falls back to retail. A loose
  `Data/Generals.str` must NOT be used: `.str` wins over `.csf` (`GameText.cpp:318`) and has no fallback.
- Original retail ZH has no such fallback, so on retail the names show as `MISSING: ...` unless the user merges their own CSF. Document it as a known limit.
- Writing a CSF needs a tiny tool (`tools/mkcsf.py` in the package, binary format is simple); not in the engine.

### 2.6 Images and portraits
- A portrait is a `MappedImage`: a named rectangle in a texture (`Texture`, `TextureWidth`, `TextureHeight`, `Coords = Left Top Right Bottom`,
  `Status`) - `Image.cpp:45-52`. Retail stores them as 512x512 or 1024x1024 **32-bit uncompressed TGA** (`SCGenChallengeSelect512_001.tga`, 1,048,620 bytes
  = 512*512*4 + 44 in `EnglishZH.big`). The texture loader uses a `.dds` if present, otherwise the `.tga` (`textureloader.cpp:249-253`), and
  `W3DFileSystem` retries the other extension (`W3DFileSystem.cpp` "Cross-extension fallback"), so shipping TGA only is fine. No DDS tooling needed.
- Texture lookup order (`W3DFileSystem.cpp:187-300`): `Data/<Language>/Art/Textures/`, then `Art/Textures/`, ... then **`<UserData>/Textures/`**
  (`USER_TGA_DIR_PATH`, `FileSystem.h:81`). MappedImage INIs are also read from **`<UserData>/INI/MappedImages/*.ini`** *before* the retail ones
  (`Image.cpp:252-271`), so that location can add new image names only (retail then re-parses same names over it).
  This is the engine's own "user can supply images" path, and it is where the photos should live (see 3, privacy).
- Where each portrait-ish PlayerTemplate field is actually used, and the size retail uses:

| Field | Used by | Retail size | Evidence |
|---|---|---|---|
| `LoadScreenImage` | Generals-only builds; **not** used by ZH's multiplayer load screen | 800x600 in a 1024 page | `LoadScreen.cpp:1293` is inside `#if RTS_GENERALS` |
| ZH load screen portrait | persona `BioPortraitLarge` from `ChallengeMode.ini` (`PNInfantryGen`) | ~332x327 | `LoadScreen.cpp:1299-1318` |
| `GeneralImage` | `PopupPlayerInfo.cpp:1072` (online profile popup, effectively dead) | 48x48 | |
| `SideIconImage` | `GameInfoWindow.cpp:196`, `ScoreScreen.cpp:1623,2229,2426` | 24x22 | |
| `EnabledImage` / `FlagWaterMark` | observer panel, `ControlBarObserver.cpp:265,378` | 22x24 / 96x160 | |
| `MedallionRegular/Hilite/Select` | Challenge menu buttons, `ChallengeMenu.cpp:155-165` | 40x40 | |
| `HeadWaterMark` | commented out in retail INI, "don't use" | | |

**Important finding:** the ZH multiplayer load screen gets its big portrait only from a Challenge persona (`getGeneralByTemplateName`), and for any template that
is not a persona and not one of the three base factions it hits `DEBUG_CRASH("Unexpected player template")` and shows **no portrait**
(`LoadScreen.cpp:1308-1318` and the twin at `:1573-1583`). So a Tier-1 general has no photo on the load screen unless it is given a persona (2.7) or the
engine falls back to `GeneralImage` (Tier 2, change A).

### 2.7 Challenge personas: two spare slots, with a catch
`ChallengeMode.ini` has 12 persona slots (`NUM_GENERALS (12)`, `ChallengeGenerals.h:39`; marked "ChallengeMenu.wnd dependent"). Slots 0-9 are the
nine playable generals plus Boss; **`GeneralPersona10` and `GeneralPersona11` are placeholders** (`StartsEnabled = no`, `Circle_Small03_Black`,
`Campaign = unimplemented`). A loose `Data/INI/ChallengeMode/CustomGenerals.ini` could fill them. Catch: the single-player Challenge menu shows every
enabled persona as a button (`ChallengeMenu.cpp:139-168`), and these have no campaign, so enabling them would put dead medallions in that menu. Hence
Tier-2 change A is preferred over using the slots.

### 2.8 Lobby, sides and what keys off the `Side` string
- The faction combo box skips any template whose `Side` was already listed (`GUIUtil.cpp:246,272-275`), so **a new general needs a new, unique `Side`**
  (same `Side` as a retail general = invisible in the lobby). Templates with an empty `StartingBuilding` are skipped too.
- Lookups keyed on `Player::getSide()`: AIData `SideInfo` (`AIPlayer.cpp:203`, `AISkirmishPlayer.cpp:578`), AIData `SkirmishBuildList`
  (`AIPlayer.cpp:2979`, `AISkirmishPlayer.cpp:984,1173`, used by Autopilot's `adoptExistingBase`, `:978`), Eva (`Eva.cpp:507-520`, no match = silent),
  control bar scheme (`ControlBarScheme.cpp:1079-1090,1150-1160`; no scheme falls back to Observer), `SIDE:<side>` label, map scripts
  (`ScriptConditions.cpp:2201`), `OCLUpdate.cpp:207`.
- `Object ... Side =` is mostly cosmetic: the one comparison with the player's side is `Player.cpp:1274` (`findNaturalCommandCenter`), which in ZH is only
  reached from an `RTS_GENERALS` block (`ControlBar.cpp:3705`). So cloned objects do not need the new `Side`.
- `BaseSide` (USA/China/GLA) is only used by `AcademyStats` for the base faction check.
- Skirmish lists, Eva and control bar schemes for a new side must be supplied, or the general plays but: Autopilot builds nothing (no build list),
  EVA is silent, control bar is the Observer one.
- Template **index** is what travels over the network and into saves/replays (`GameInfo.cpp:1330,1462` bounds-check against the template count).
  Our templates are appended after all retail ones (`PlayerTemplate.cpp:387-408`, retail file loaded first), so retail indices do not move.

### 2.9 Units, build lists, science tree: how a retail general is put together
Read from `PlayerTemplate.ini` (archive) for `FactionAmericaLaserGeneral` and `FactionChinaInfantryGeneral`:
- `StartingBuilding`/`StartingUnit0` name objects (`Lazr_AmericaCommandCenter`, `Lazr_AmericaVehicleDozer`).
- The build list is the command sets: the command center's `CommandSet = Lazr_AmericaCommandCenterCommandSet`, dozer/barracks sets the same way.
- Purchase tree: `PurchaseScienceCommandSetRank1/3/8` name CommandSets of `Command_PurchaseScience` buttons; `IntrinsicSciences` is the base science.
- Shortcut bar: `SpecialPowerShortcutCommandSet` + `SpecialPowerShortcutWinName` (a `.wnd`, reuse the faction's).
- Perks in data: `ProductionCostChange`, `ProductionTimeChange`, `ProductionVeterancyLevel`, `IntrinsicSciencePurchasePoints`, `StartMoney`
  (`PlayerTemplate.cpp:80-118`) - the cheap, pure-data way to give a general a unique edge.
- Infantry General already has minigunner paradrops (`ObjectCreationList.ini` payloads `Infa_ChinaInfantryMiniGunner` x5/10/8/7) and a Helix with
  `ChinookAIUpdate` (`InfantryGeneral.ini`, `Infa_ChinaVehicleHelix`); an American Chinook exists per general (`Lazr_AmericaVehicleChinook`). A
  "Chinook drop of minigunners" is therefore an OCL/command-button composition, probably pure data (to be confirmed in step 5).

### 2.10 Autopilot attachment
- Autopilot is human-only (`Player.cpp:806-814`), set by `MSG_SET_AUTOPILOT` (level + optional module mask), stored in the player and saved with it.
- `Player::applyAutopilotProfile` (`Player.cpp:794-800`) = `AutopilotProfile::forLevel(level)->withModules(mask)`; a level is a row in
  `AutopilotProfile.cpp` and modules are `AM_*` bits.
- Roles are recognised by **name fragments** in the template name (`AutopilotUnits.h`: `MiniGunner`, `TankHunter`, `Hacker`...), because a template
  cannot report abilities. **Naming rule for cloned units: keep the retail role name inside the new name** (e.g. `Ben_ChinaInfantryHacker`).
- Autopilot picks its structures from the side's `SkirmishBuildList` (`AISkirmishPlayer.cpp:978-992`) and reads command sets, so
  **CommandSet/CommandButton content must be identical on every machine** in a match, or Autopilot decisions can diverge and desync, even though the
  INI CRC does not cover those files (2.2).

---------------------------------------------------------------------------------------------------------------------------------------------

## 3. Install, removal, and what a mod touches

Package rule: **only new files, only new names, no retail text copied.** Derived data is generated on the user's machine by a build step that reads
the user's own archives (see 4). The repo is a public fork (`FEATURES_BACKLOG.md` push policy), so: no retail data and no personal photos are ever committed.

Everything is added; nothing is replaced. Files the mod creates (all relative to the loose root, 2.4):

```
Data/INI/PlayerTemplate/CustomGenerals.ini        2 new PlayerTemplates (+ never a retail name)
Data/INI/Object/CustomGenerals.ini                ObjectReskin deltas: command centers, dozers, a few units
Data/INI/CommandSet/CustomGenerals.ini            new CommandSets only
Data/INI/CommandButton/CustomGenerals.ini         new buttons
Data/INI/Science/CustomGenerals.ini               new sciences (if any)
Data/INI/AIData/CustomGenerals.ini                SideInfo + SkirmishBuildList for the two new sides
Data/INI/ControlBarScheme/CustomGenerals.ini      one scheme per side
Data/INI/MappedImages/HandCreated/ZZ_CustomGenerals.INI   image names -> texture rectangles
Data/English/generals.csf                         partial string table (GeneralsX only)
Art/Textures/CG_*.tga                              portrait pages (or <UserData>/Textures/, see below)
```
Install = copy the tree over the loose root. Remove = delete exactly the files in `MANIFEST.txt` (written by the installer, with sha256, so an update
or an uninstall never touches anything else). No `.big`, no `-mod`, no registry/settings edit. Starting the game without the files is the original
game again. Because the INI CRC covers PlayerTemplate/Object/AIData, a replay or save made with the mod needs the mod.

**Photos:** two options, the second is the default.
1. In the game tree (`Art/Textures/CG_*.tga`): simple, but then the package carries a face.
2. In the user data directory: `<UserData>/Textures/CG_*.tga` and `<UserData>/INI/MappedImages/CG_Portraits.ini` (2.6). The package ships only a
   neutral silhouette placeholder under the same names in the game tree; a user's photo in UserData cannot override a game-tree file (the game tree is
   searched first), so use **different image names** for the personal set and let the PlayerTemplate point at them, or ship no placeholder. Simplest
   rule: the placeholder lives in the package, the photo-backed image names live in UserData, and a one-line `GeneralImage`/persona edit chooses. A
   friend gets the silhouette; the owner's photos never leave `~/`.

Portrait formats (proposal): author a 1024x1024 32-bit TGA per general with regions: large portrait 332x327 (load screen), small 116x114,
48x48 (`GeneralImage`), 24x22 (side icon), 22x24 (observer). `tools/make_portraits.sh` (ImageMagick, local only) crops a square from the photo and
writes the page; sizes must be powers of two for the page, regions are free.

---------------------------------------------------------------------------------------------------------------------------------------------

## 4. Proposed directory layout (in the repo, outside `docs/`)

```
mods/CustomGenerals/                       (new top-level dir; no game data, no photos)
  README.md                                 install / remove / what it touches / compatibility table
  install.sh   install.ps1   uninstall.sh   uninstall.ps1      copy tree, write MANIFEST.txt; uninstall reads it
  pack/                                     the files that go into the game (layout of section 3)
    Data/INI/PlayerTemplate/CustomGenerals.ini
    Data/INI/Object/CustomGenerals.ini
    ...
    Data/INI/MappedImages/HandCreated/ZZ_CustomGenerals.INI
  strings/CustomGenerals.txt                label -> text source for the partial CSF
  tools/
    mkcsf.py                                strings -> Data/English/generals.csf (partial)
    make_portraits.sh                       photo(s) -> TGA page, reads ~/ only, writes to UserData
    derive_build_lists.py                   (only if no ParentSide, 6) reads the user's INIZH.big, writes AIData/CustomGenerals.ini locally
  portraits-src/                            .gitignored; the owner's photos
  placeholder/                              neutral silhouettes shipped in the package
```
The doc `docs/WORKDIR/planning/CUSTOM_GENERALS_DESIGN.md` stays here; add a `FEATURES_BACKLOG.md` entry when work starts.

---------------------------------------------------------------------------------------------------------------------------------------------

## 5. Multiplayer, `RETAIL_COMPATIBLE_CRC`, and what a friend must install

State of the fork: `RETAIL_COMPATIBLE_CRC` is `(0)` (`GameDefines.h:96-98`), so **this build is already not CRC-compatible with retail ZH 1.04**
(RandomValue, pathfinding, particle, `Network.cpp:693`, `GameLogic` guards). A GeneralsX host and a retail client cannot play together with or without
the mod (`FEATURES_BACKLOG.md:276-282`: untested even with the flag on). The mod adds nothing worse on that axis.

What the mod changes for people who DO run GeneralsX:
- **INI CRC differs.** PlayerTemplate, Object, AIData etc. feed `m_iniCRC` (2.2). It is printed in the version hash (`version.cpp:373`), stored in replays and
  saves (`Recorder.cpp:578`, `:1181`), and compared in the GameSpy lobby (`LobbyUtils.cpp:300,538`). The LAN join check is compiled out for ZH
  (`LANAPIhandlers.cpp:356-358`, `#if !RTS_ZEROHOUR`, "@todo Enable CRC checks"), so a mismatching friend is **not refused at join**; the first
  per-frame logic CRC compare (`GameLogic.cpp:2736-2768`) then reports the desync. Everyone in a match must have byte-identical mod files.
- **Slot index.** The lobby sends the template index; a client with fewer templates rejects it (`GameInfo.cpp:1330,1462`). A friend without the mod
  can therefore not be in a game where somebody picked a custom general.
- **Not covered by the CRC:** CommandSet, CommandButton, ControlBarScheme, MappedImages, strings, textures. A friend with different command sets is not
  detected, and with Autopilot on this can desync (2.10). Proposal: add a tiny `Data/INI/Object/...` (CRC'd) marker object or a version `Science` whose
  name carries the pack version, so that any version skew shows in `m_iniCRC`. Cheap, no engine change.
- **Random faction** picks from all templates, including ours (`PopulatePlayerTemplateComboBox` adds them), so a random slot only stays sane if every
  player has the pack.

Friend checklist (GeneralsX players): same GeneralsX build, retail Zero Hour data, copy `pack/` over their loose root (or run `install.sh/.ps1`),
optional own portrait. Original-retail players: they can load the INI tier (it is the same parser lineage; `PlayerTemplate` fields are all retail
fields), see `MISSING:` labels, and cannot play against a GeneralsX host anyway. Say so in the README; do not promise retail cross-play.

---------------------------------------------------------------------------------------------------------------------------------------------

## 6. Minimal engine changes vs pure data

### Pure data (no engine change)
- Two PlayerTemplates (new `Side`, `BaseSide = USA` / `China`, reuse a retail `StartingBuilding` or a reskinned copy).
- Perks: `ProductionCostChange/TimeChange/VeterancyLevel`, `IntrinsicSciencePurchasePoints`.
- New command sets/buttons/sciences, reskinned units via `ObjectReskin`.
- Images, `ControlBarScheme`, AIData `SideInfo`/`SkirmishBuildList` (full copy of a retail list, see change B for the alternative).
- Partial CSF (already supported by the GeneralsX fallback).

### Engine change list (each is small, annotated, and inert for retail-style games)
A. **Load-screen portrait fallback.** In both branches (`LoadScreen.cpp:1308-1318`, `:1573-1583`) replace the `DEBUG_CRASH("Unexpected player template")`
   with: use `pt->getGeneralImage()` when the template has no persona. Today that case is a debug assert and a blank portrait. No new INI field
   (`GeneralImage` is already parsed, `PlayerTemplate.cpp`). Not game logic; not CRC-visible. This is a bug-class fix (unhandled template), not a
   band-aid.
B. **`ParentSide`** (optional but removes ~250 lines of copied retail build list and keeps the pack free of retail text). New PlayerTemplate field
   `ParentSide = AmericaLaserGeneral`. At the lookups of 2.8 (AIPlayer 203/2979, AISkirmishPlayer 578/984/1173, Eva 507, ControlBarScheme 1083/1153) try the
   template's own `Side`, then `ParentSide`. Game-logic visible, but identical on every machine that has the pack (the field is in the CRC'd
   PlayerTemplate file).
C. **Autopilot personality** (Tier 3). New PlayerTemplate field `AutopilotPersonality = Wallet | Ben`. `Player::applyAutopilotProfile`
   (`Player.cpp:794`) uses the personality's module mask when the incoming mask is `AUTOPILOT_MODULES_DEFAULT`. A personality is one table row next to
   `s_profiles` in `AutopilotProfile.cpp` (module bits, banner text, which build-list extras). New modules follow the existing pattern (one module, one
   class, `AutopilotHost` interface only): `AM_SUPERWEAPON` (build the side's superweapon, fire it at the visible enemy structure cluster from the
   snapshot), `AM_ATTACK_WAVE` (after N minutes / on a bank threshold, send the standing army at the nearest enemy structure; the directive
   vocabulary in `AUTOPILOT_DIRECTIVES.md` is the place), `AM_AIR_DROP` (Chinook/Helix loaded with minigunners to a spot).
   All go through normal AI paths so every machine computes the same orders (`AUTOPILOT_ARCHITECTURE.md`).
D. (Optional, later) A Challenge-menu-free "bio" for the lobby tooltip: `ArmyTooltip`/`Features` already exist as PlayerTemplate fields, just strings.

Rules from the repo CLAUDE.md that apply to A-C: no `#ifdef __linux__` in `GeneralsMD/Code/GameEngine` or `Core/GameEngine` (none needed, all
three are portable); annotate each change `// GeneralsX @feature Claude DD/MM/YYYY ...` (or `@bugfix` for A); no empty stubs and no commented-out code;
do not mix refactors into these commits; conventional commits (`feat(custom-generals): ...`), dev-diary entry before committing; show the diff to Ben first
(global rule). Keep determinism: no wall clock, no libm in the new Autopilot modules (as in the architecture doc).

---------------------------------------------------------------------------------------------------------------------------------------------

## 7. Step-by-step plan: one reskinned general -> two generals with unique abilities

Each step ends in something checkable. "Smoke" = the existing headless harness (`-file <map> -soloAutopilot ... -soloTemplate <Template>`,
`tools/netplay/README.md`), which already takes `-soloTemplate`, so a new template can be exercised without the lobby.

0. **Confirm the loose root** (10 min). Put one harmless new file `Data/INI/Science/zz_probe.ini` (a new `Science SCIENCE_CG_Probe` block), start the game
   once, check the log for the file being loaded. Settles Open Question 1. Delete the probe.
1. **One reskinned general, Tier 1 only (Wallet-Laser).** `PlayerTemplate FactionBenWallet`: `Side = GeneralWallet`, `BaseSide = USA`,
   `PlayableSide = Yes`, copy every other field from `FactionAmericaLaserGeneral` (retail command center, dozer, science sets, shortcut bar). Add
   `AIData` `SideInfo` and `SkirmishBuildList GeneralWallet` copied from `AmericaLaserGeneral` (generated locally, not committed), a
   `ControlBarScheme GeneralWallet8x6` copied from `AmericaLaserGeneral8x6`. No partial CSF yet: expect `MISSING:` text.
   Check: general appears in the lobby combo, a skirmish starts, the control bar is the Laser one, Autopilot level 4 builds a base.
2. **Names.** `mkcsf.py` + `strings/CustomGenerals.txt` -> loose `Data/English/generals.csf` with `INI:FactionBenWallet`, `SIDE:GeneralWallet`,
   tooltip and features labels. Check: names show; every other label (menus) still retail (proves the fallback at `GameText.cpp:389`).
3. **Photo.** `make_portraits.sh` -> TGA page in `<UserData>/Textures/`, `MappedImage` entries in `<UserData>/INI/MappedImages/`, set `GeneralImage` and
   `SideIconImage`. Engine change A so the load screen shows it. Check: load screen, score screen icon, lobby icon list. Test with a placeholder silhouette
   first, then the photo.
4. **Second general (Ben-Infantry).** Same recipe from `FactionChinaInfantryGeneral` (`Side = GeneralBen`, `BaseSide = China`), own skirmish list and control
   bar scheme, own portrait. Check: both selectable at once, two humans on LAN (INI CRC identical on both machines, version hash equal in the log).
5. **Unique abilities, data only.** Wallet: `ProductionCostChange` on defenses/supply, `IntrinsicSciencePurchasePoints`, a reskinned command center
   (`ObjectReskin Wallet_AmericaCommandCenter Lazr_AmericaCommandCenter`) with a new CommandSet that adds the superweapon button earlier (pointing at the
   retail superweapon objects/powers), supply-drop button. Ben: reskinned barracks/war factory command sets with Hacker, Tank Hunter, a "Chinook
   minigunner drop" button (OCL + `Wallet_`/`Ben_` named Chinook object; keep role substrings for Autopilot). Check each with the harness, compare
   `summary.json` before/after as the architecture doc prescribes.
6. **Engine change B** (`ParentSide`) once the copied lists become a maintenance burden, delete the locally generated copies.
7. **Engine change C: personalities.** Add the field and the profile rows with the *existing* modules first (Wallet = `AM_WALLET|AM_DEFENSE_UNITS|AM_REPAIR`,
   Ben = `AM_HACKERS|AM_DEFENSE_UNITS|AM_SQUAD`): pure table work, scenario runner must show identical numbers for the old levels.
8. **New modules** one at a time, each with its own scenario: superweapon, attack wave, air drop. Linux+Windows `run-netplay.sh --fast` match to confirm sync
   (the module only runs for a player who switched Autopilot on).
9. **Package + docs.** Installer/uninstaller with manifest, README compatibility table, version marker object (section 5), `FEATURES_BACKLOG.md` entry,
   dev-diary entry. Dry-run on a clean copy of the game dir and on a second machine.

---------------------------------------------------------------------------------------------------------------------------------------------

## 8. Open questions for Ben

1. **Loose root.** I read that loose files resolve from the working directory, but the retail `.big` files here sit one level deeper
   (`<game dir>/Data/*.big`, loose retail data in `.../Data/Data/`). OK to run the step-0 probe to settle where `Data/INI/...` goes?
2. **Who plays with you?** Only GeneralsX users (then Tier 1+2+3 freely) or also people on the original game (then only Tier 1, `MISSING:` names, and never
   cross-play with a GeneralsX host)? This decides whether the engine changes are acceptable as "required".
3. **Do the generals need to be playable by humans in the lobby, as AI opponents, or both?** Both falls out of the same PlayerTemplate; AI opponents need the
   `SkirmishBuildList` to be good, humans do not.
4. **Wallet superweapons and "blow through bases":** the retail Laser General has no nuke/particle cannon. Do you want Wallet to be a Laser General
   that *also* gets the Super Weapon General's cannons (balance change) or keep Laser purity and let the AI be the "hoarder"? Same for the owner's
   Chinook drops: America has the Chinook, Infantry General has the Helix; which unit should carry the minigunners?
5. **Photos:** one photo per general, or several (lobby icon, load screen, victory)? Any restrictions on who may see them (determines whether the
   placeholder/`UserData` split in section 3 is mandatory)?
6. **Should Autopilot attack at all?** It currently never does (`AUTOPILOT_DIRECTIVES.md`); "blow through bases" is the first feature that would. Opt-in
   only (a new hotkey/level), or the default behavior of those two personalities?
7. **Names/lore:** display strings and bio text (lobby tooltip, features list). Draft yours, or should I write a first pass?
8. **Challenge menu / campaign:** skip (recommended: 2.7), or do you want real campaign personas later (needs `.wnd` and `Campaign.ini` work, 12 slots only)?
9. **Packaging for the public fork:** keep `mods/CustomGenerals` in this repo, or a separate repo so the public fork stays gameplay-neutral?

---------------------------------------------------------------------------------------------------------------------------------------------

## 9. Risks and things not verified

- Runtime behaviour of loose INI directories on Linux (exact-case rule, double listing of same-named files) is from reading only.
- `ObjectReskin` copying a CommandCenter with its module tags is the intended use (`ThingTemplate.cpp:111` note) but a cloned production building has
  not been tried here; some modules reference the object name (build lists, `PrerequisiteTech`).
- Binary CSF writer: format must be checked against `GameText.cpp` `parseCSF` before the tool is written.
- Original retail 1.04 behaviour is inferred from the shared code lineage; nothing was run against it.
- Autopilot module work (steps 7-8) has real design risk: attack logic is new territory for the engine's deterministic-AI constraints.
