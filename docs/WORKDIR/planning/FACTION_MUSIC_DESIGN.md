# Player-Supplied Faction Music (MP3 playlists with crossfade)

Status: **stages 1-4 implemented 2026-10-22** (folder per faction, shuffle, gapless-by-crossfade, `PlayerTemplate.MusicPlaylist`); stage 5's `playlists.ini` / `.m3u` and stage 6 are not built. Written 2026-10-22.

## How to use it (what is built)
1. Put audio files in `<user data>/Music/<playlist>/` (Linux: `~/.local/share/GeneralsX/GeneralsZH/Music/gla/`; the user data folder is the one that holds `Save/` and `Replays/`). `.mp3` is first-class; `.ogg .flac .wav .m4a .opus` also work (lowercase extensions).
2. The playlist is chosen per match from the local player: the template's `MusicPlaylist` INI value, else its name without `Faction` (`americaairforcegeneral`), else its side and base side (`america` also answers to `usa`, `china`, `gla`), else `default`; `observer` first for observers. Folder names are matched case-insensitively. No matching folder = retail music, unchanged.
3. In a match the retail `Game_*` tracks the map scripts ask for are replaced by the playlist (shuffled, no repeats until every song played once, 4 s equal-power crossfade, 2 s fade-in). Menus, load screens and the victory/defeat stingers stay retail; the stinger stops the playlist with a 1.5 s fade.
4. Use constant-bitrate MP3s (the length is read from the container, or from file size / bitrate when the container does not say); songs shorter than 10 s are played without overlap. The options-menu music volume and mute apply.
5. Log lines (stderr) start with `MUSIC playlist:` and show the chosen folder, every song start with the playlist clock, and each crossfade.
Testing without a sound card: `GENERALSX_HARNESS_AUDIO=1 ALSOFT_DRIVERS=null ./GeneralsXZH -headless -lanhost ... -lanHostTemplate FactionGLA` (the headless harness only pumps the audio manager when that variable is set).
Verified 2026-10-22 with 20 s test tones: songs start every 16.07 s (duration minus the overlap), no repeats, a faction with no folder logs `-> retail music`. Not verified by ear or on Windows/macOS/MiniAudio; the pause-menu clock stop is untested.

Scope: Zero Hour target (`GeneralsMD/`), shared audio code in `Core/`.

All file:line citations are against this repo at the time of writing.
Retail archives were inspected read-only (listing + `Music.ini` / script strings); nothing was extracted to disk.

---

## 1. What exists today (verified)

### 1.1 Retail music is already MP3
- `Data/Music.big` and `Data/MusicZH.big` hold `Data\Audio\Tracks\*.mp3`: `USA_01..11`, `CHI_01..11`, `GLA_01..11`, `C_*` (campaign), `End_*`, `Silence60.mp3`.
- `Data\INI\Music.ini` (in `INIZH.big`) defines `MusicTrack Game_USA_01 ... Game_CHI_10`, `Game_GLA_01..11`, `Load_USA/China/GLA`, `Game_Silent` (= `Silence60.mp3`), `End_*`, `Track1..3` (shell). Each is just `Filename = X.mp3`.
- Consequence: every audio backend already has an MP3 decode path. **No new decoder is required** (see 4).

### 1.2 How a track is parsed and located
- `INI::parseMusicTrackDefinition` creates an `AudioEventInfo` with `m_soundType = AT_Music` and registers the name in `AudioManager::m_musicTracks` (`Core/GameEngine/Source/Common/INI/INIAudioEventInfo.cpp:42-66`, `GameAudio.cpp:515-518`). The list is only used by the debug next/prev keys (`GameAudio.cpp:520-565`, `CommandXlat.cpp:5899,5909`).
- Path is built as `AudioRoot\MusicFolder\<Filename>` = `Data\Audio\Tracks\...` (`AudioEventRTS.cpp:762-782`, `AudioEventRTS::generateFilename` at `:314-329`; `MusicFolder` from `AudioSettings.ini:7`). There is no way to point an event at a file outside that folder today.
- `FileSystem::openFile` tries `TheLocalFileSystem` first and only then the `.big` archives (`Core/GameEngine/Source/Common/System/FileSystem.cpp:175-220`). So a loose `Data/Audio/Tracks/USA_01.mp3` shadows the archived one. That is a legitimate no-edit-to-`.big` override, but it is one file per retail slot, replaces retail for everyone using the install, and has no enumeration, crossfade or per-general selection.
- `LocalFileSystem::getFileListInDirectory` / `FileSystem::getFileListInDirectory` exist for enumerating a folder (`Core/GameEngine/Include/Common/FileSystem.h:155`, `LocalFileSystem.h:41`, impl `StdLocalFileSystem.h:46`).

### 1.3 Who decides what plays in a match: the script engine, not C++
- Gameplay music is driven by map scripts. `SkirmishScripts.scb` (in `ScriptsZH.big`) contains per-faction script groups "USA Music", "China Music", "GLA Music", "Observer Music" with sub-scripts such as "USA Music Track 2 Start", "USA Music Player Built Particle Cannon", "GLA Music SCUD Silence", "... Lost Important Silence", "... Restart Cycle", and the track names `Game_USA_01..11` etc. (string scan of the .scb). `MultiplayerScripts.scb` only references `Game_Silent` and the `End_*_Failure` stingers.
- The action is `MUSIC_SET_TRACK` -> `ScriptActions::doMusicTrackChange(name, fadeout, fadein)` (`GeneralsMD/Code/GameEngine/Source/GameLogic/ScriptEngine/ScriptActions.cpp:3355-3370`, dispatch `:7044`). It issues `AHSV_StopTheMusic[Fade]`, builds an `AudioEventRTS`, sets `setShouldFade(fadein)`, `setPlayerIndex(localPlayerIndex)`, calls `TheAudio->addAudioEvent`, and stores `m_currentTrackName` in the script engine.
- `m_currentTrackName` is saved in save games (`ScriptEngine.cpp:9294`) and re-played on load (`:9331-9333`).
- Faction branching in those scripts uses the player's `getSide()` (`ScriptConditions.cpp:2194-2201`, "skirmish player is faction"), so the retail logic is per **side** (America/China/GLA), not per general.

### 1.4 AudioManager path for music
- `AudioManager::addAudioEvent` rejects music if `!isOn(AudioAffect_Music)` (`GameAudio.cpp:404-410`), culls if volume < `m_minVolume` (`:462-468`), then `m_music->addAudioEvent` for `AT_Music` (`:470-473`).
- `MusicManager` is a thin forwarder that queues `AR_Play` / `AR_Stop` requests (`Core/GameEngine/Source/Common/Audio/GameMusic.cpp:78-126`).
- `removeAudioEvent(AHSV_StopTheMusic | AHSV_StopTheMusicFade)` goes to `MusicManager::removeAudioEvent` (`GameAudio.cpp:569-573`).
- Volume: `setVolume(..., AudioAffect_Music)` writes `m_systemMusicVolume` / `m_scriptMusicVolume`, `m_musicVolume = script * system` (`GameAudio.cpp:704-711`). On/off: `m_musicOn` (`:684-686`, set from `TheGlobalData->m_musicOn` at `GameEngine.cpp:778`). Options menu music slider feeds `m_preferredMusicVolume` (`GameAudio.cpp:1137`). Pause/resume with the pause menu: `GameLogic::pauseGameMusic` -> `pauseAudio/resumeAudio(AudioAffect_Music)` (`GameLogic.cpp:4720-4733`). Alt-tab mute: `WinMain.cpp:496-501`.
- `AudioManager::UPDATE()` is called once per client loop from `GameEngine::update` (`GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp:1016`), on the main thread, before `TheGameClient->UPDATE()` and before logic. It is also pumped manually during load screens (`LoadScreen.cpp:1112,1165,1339`).

### 1.5 Backends
- Backend is picked at compile time in `SDL3GameEngine::createAudioManager` (`GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp:576-592`): MiniAudio if `SAGE_USE_MINIAUDIO`, else OpenAL.
- Presets: `linux64-deploy`, `linux64-openal`, `macos-openal` use OpenAL + FFmpeg; `macos-vulkan` and `linux64-miniaudio` use MiniAudio (`CMakePresets.json` ~208-303). So **both** backends ship and both must work.
- OpenAL music: `AT_Music` and `AT_Streaming` share one path. File opened via `TheFileSystem->openFile`, wrapped in `FFmpegFile`, frames pushed into an `OpenALAudioStream` (`Core/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioManager.cpp:745-862`). `FFmpegFile::open(File*)` uses custom AVIO so any readable `File` works (`FFmpegFile.cpp:50-89`), and FFmpeg probes by content, so MP3, OGG, FLAC, WAV, M4A all work.
- MiniAudio music: same idea, FFmpeg decode fed into miniaudio; the comment at `MiniAudioManager.cpp:351-353` says miniaudio's own decoders hang on MP3 via the VFS, so FFmpeg is deliberately used. Music goes to `m_musicGroup` (`:335`).
- Concurrency: nothing stops two music streams existing at once. OpenAL only kills a stream when `handleToKill` matches (`OpenALAudioManager.cpp:765-783`); otherwise a new `AR_Play` just adds another entry to `m_playingStreams`. The "stop the old music" is done by the script (`ScriptActions.cpp:3359-3361`).

### 1.6 Crossfade today: does not exist
- Fade-out exists only as `AHSV_StopTheMusicFade`: the stream moves to `m_fadingAudio` and `processFadingList` linearly ramps gain over `m_fadeAudioFrames` (`OpenALAudioManager.cpp:2619-2668`, and `:1017-1040`). `TimeToFadeAudio = 2000` ms (`AudioSettings.ini:21`) is parsed as a duration into **logic frames** (`GameAudio.cpp:124`, `INI::parseDurationUnsignedInt`), but the counter ticks once per `update()` call, i.e. per client loop, so the real fade time depends on render FPS. Not suitable for a polished crossfade; use wall-clock.
- Fade-in is **not implemented**: `setShouldFade` is set by scripts and the load screen but nothing in `Core/GameEngineDevice` reads `getShouldFade` (grep over `Core`, `GeneralsMD/Code` finds only setters; getter defined at `AudioEventRTS.cpp:648`). `OpenALAudioManager::playStream` has an empty "Need to stop/fade out the old music here" stub (`:2958-2960`).
- Repeat/gapless: `hasMusicTrackCompleted` is a stub that returns FALSE on OpenAL (`:1504-1522`). When a stream ends it is simply released (`:2551-2563`). In retail, looping/rotation is done by scripts ("... Restart Cycle", timers), not by the audio device.

### 1.7 Gain control already available for streams
- `AudioManager::adjustVolumeOfPlayingAudio(eventName, newVolume)` is a pure virtual (`GameAudio.h:231`). OpenAL applies it to `m_playingStreams` too (`OpenALAudioManager.cpp:2242-2275`). **MiniAudio's version only loops `m_playingSounds`, not streams** (`MiniAudioManager.cpp:1126-1137`): a 5-line gap to fix there.
- Per-stream gain = `musicVolume * eventVolume * volumeShift`, recomputed on option changes (`OpenALAudioManager.cpp:1347-1351, 2881-2883`; Mini `:1449`). So the user's music slider/mute keep working for custom music for free, provided the custom tracks go through the same `AT_Music` path.

### 1.8 Who is the local player, and when
- `PlayerList::setLocalPlayer` -> `Player::becomingLocalPlayer` (`PlayerList.cpp:329-345`, `Player.cpp:1131`). Set during `PlayerList::newGame` (`PlayerList.cpp:~165-195`), before the first logic frame, so it is known by the time the first music script fires.
- `Player::getSide()` / `getBaseSide()` / `getPlayerTemplate()` (`Player.h:234-237`). `PlayerTemplate` has the INI fields `Side`, `BaseSide`, `IsObserver`, `LoadScreenMusic`, `ScoreScreenMusic`, ... (`PlayerTemplate.cpp:66-124`) and `getName()` returning the template key, e.g. a general's `FactionXxx` name (`PlayerTemplate.h:80`).
- The script action already feeds `ThePlayerList->getLocalPlayer()->getPlayerIndex()` into the event (`ScriptActions.cpp:3366`). So "the local player" is a first-class concept at audio time and no network knowledge is needed: in multiplayer every client plays its own local player's playlist.

### 1.9 Menu vs in-game
- Shell/menu music is `Track1..3` etc. started from shell code; loading screens use `PlayerTemplate::LoadScreenMusic` (`Load_USA`...) and the score screen uses `ScoreScreenMusic`; both call `AHSV_StopTheMusicFade` first (`LoadScreen.cpp:1273,1334`, `ScoreScreen.cpp:423,461`). In-game music is the `Game_*` tracks from the scripts above.

---

## 2. User story and folder layout

**Story**: "I generate songs with Suno for each faction, drop the MP3s into a folder named after the faction, start the game, and during a match I hear those songs, shuffled, crossfading into each other. Menus and cutscenes still use the retail music. Delete the folder and everything is retail again."

### 2.1 Location (no edits inside the install, works for read-only installs)
Search, in order, and **merge** (user dir wins on name collisions):
1. `<UserData>/Music/` = `TheGlobalData->getPath_UserData()` + `Music/` (`GlobalData.h:596`; on Linux this is the XDG data dir, `GlobalData.cpp:1396+`; on Windows it is the My Documents game folder). Per-user, never touches the install.
2. `<GameDir>/Music/` next to the executable (for portable installs / `.big`-free mods). Optional.

### 2.2 Layout
```
Music/
  playlists.ini            optional; never required
  usa/            *.mp3    all USA generals unless they have their own folder
  china/          *.mp3
  gla/            *.mp3
  usa-airforce/   *.mp3    optional per-general override (folder = playlist id)
  observer/       *.mp3    optional
  default/        *.mp3    optional catch-all when nothing else matches
```
- Folder name = **playlist id**, matched case-insensitively (we do our own enumeration/compare so Linux case-sensitivity does not matter).
- Resolution order for the local player (first existing, non-empty folder wins):
  1. `PlayerTemplate.MusicPlaylist` (new INI field, see 3.2), so a custom general can say `MusicPlaylist = shadow-ops` and ship nothing else.
  2. Lower-cased `PlayerTemplate::getName()` with the leading `Faction` stripped (`FactionAmericaAirForceGeneral` -> `americaairforcegeneral`).
  3. Lower-cased `Side` (`America` -> `america`; alias map `america=usa`, `china`, `gla`).
  4. Lower-cased `BaseSide`.
  5. `default/`.
  6. Nothing found: retail behaviour, bit-for-bit.
- Observer (`IsObserver`) resolves to `observer/` then `default/`.

### 2.3 Ordering and options
Default with no INI: shuffle, no immediate repeats, full-playlist-before-repeat, crossfade 4 s, gain 1.0. Tracks ordered by filename when `Order = Sequential`.

Optional tiny `Music/playlists.ini`, using the engine's existing INI parser, one block per id:
```
MusicPlaylist usa
  Order      = Shuffle        ; Shuffle | Sequential
  Crossfade  = 4000           ; ms; 0 = hard cut/gapless
  Volume     = 90%            ; extra trim on top of the options-menu slider
  Scope      = InGame         ; InGame | InGameAndMenus (stage 6)
End
```
An `.m3u`/`.m3u8` inside a folder, if present, replaces the directory listing as the order (paths relative to the folder; `#`-lines ignored). This is the "order file" for people who want a fixed running order; it is parsed with ~20 lines and no dependency.

Accepted extensions: `.mp3` first-class; `.ogg .flac .wav .m4a .opus` come for free from FFmpeg (4.1) but are documented as best-effort.

---

## 3. Engine change vs pure-data

### 3.1 Why pure data is not enough
Pure-data options and their limits:
- Loose `Data/Audio/Tracks/*.mp3` shadows (1.2): one file per fixed slot, global to the install, no per-general choice, no enumeration, no shuffle/crossfade, and the user would have to rename files to `USA_01.mp3`. It also violates "drop MP3s into a folder per faction".
- Overriding `Music.ini` with loose files (`INI_LOAD_OVERWRITE`, `GameAudio.cpp:220-221`): can repoint `Game_USA_01` etc., but paths are still confined to `Data\Audio\Tracks` and the retail scripts still pick the slot, so still no crossfade/shuffle across an arbitrary file count.
- Editing `SkirmishScripts.scb`: the script is inside a retail `.big` and would also have to ship per mod; out of scope by requirement.

So a small engine layer is required. Everything below keeps `GameEngine/` free of platform ifdefs and puts the decoder/device work where it already lives.

### 3.2 Components

**A. `Core/GameEngine/.../Common/Audio/MusicPlaylist.{h,cpp}` (platform-neutral, new, about 300 lines)**
- `MusicPlaylistLibrary`: scans the roots from 2.1 using `TheLocalFileSystem->getFileListInDirectory`, builds `id -> {files, options}`, parses `playlists.ini` / `.m3u`. Lazy: scan at first need and again on `reset()`; never at engine init (keeps startup fast; desktop indexers (Tracker and the like) can choke on huge music dirs, so no recursive scans, cap e.g. 200 files/playlist).
- `MusicPlaylistPlayer` (owned by `AudioManager`, next to `m_music`, `GameAudio.cpp:235`): the state machine (current/next track, timers, shuffle bag, crossfade envelope). It only talks to the rest of the engine through public `AudioManager` calls: `addAudioEvent`, `removeAudioEvent`, `adjustVolumeOfPlayingAudio`, plus a duration query (3.3). No device types.
- Random: use `GameAudioRandomValue` (`AudioRandomValue.h:35-39`), the client-side audio RNG. Never `GameLogicRandomValue`.
- Playlist id resolution uses `ThePlayerList->getLocalPlayer()`, `getPlayerTemplate()`, `getSide()` (1.8). Header already reachable from `Core` audio code? `GameAudio.cpp` includes game-logic headers elsewhere; if layering complains, inject the id with one setter `AudioManager::setMusicPlaylistId(AsciiString)` called from game code (e.g. where the local player is established), keeping `Core` free of `PlayerList` includes.

**B. Hook points in shared code (3 small edits)**
1. `AudioManager::addAudioEvent` (`GameAudio.cpp:470`): in the `AT_Music` branch, ask `m_playlistPlayer->shouldSuppress(eventName)`. True only when a playlist is active **and** the event is a retail in-game gameplay track (name begins `Game_`, which are exactly the tracks the skirmish scripts rotate, see 1.1/1.3). `Game_Silent` is also suppressed so scripted "silence" events do not cut the playlist (open question 8.2). `Load_*`, `End_*`, `Track*`, score screen and campaign tracks are never suppressed in the default scope.
2. `AudioManager::removeAudioEvent(AHSV_StopTheMusic[Fade])` (`GameAudio.cpp:569-573`): also tell the player to fade out/stop, so victory/defeat stingers (`End_*`) and the load-screen stop work exactly as today.
3. A start trigger: the first suppressed `Game_*` request after game start is the trigger to start the playlist. This piggybacks on the retail "Start Music Timer" scripts, so **no C++ needs to know about "game started"** and it automatically honours observers, replays and skirmish/multiplayer map conditions. If no script ever asks (custom map without music scripts), add stage 6's explicit `onGameStart()` hook.

**C. `PlayerTemplate` field (1 line)**: add `{ "MusicPlaylist", INI::parseAsciiString, nullptr, offsetof(PlayerTemplate, m_musicPlaylist) }` beside `LoadScreenMusic` (`PlayerTemplate.cpp:102`) plus member + getter in `PlayerTemplate.h`. This is the "each general carries a playlist id" hook for the parallel custom-generals design. Absent -> fallback chain in 2.2. Note this is data-only, parsed from INI the same way as all other template fields and not part of any checksum-sensitive logic (see 5).

**D. Device code (`Core/GameEngineDevice`)**: only small, backend-local tweaks:
- Both managers: allow an `AudioEventInfo` whose filename is a full path. Cleanest: a bool `AudioEventInfo::m_filenameIsFullPath` honoured in `AudioEventRTS::generateFilename` (`AudioEventRTS.cpp:323-329`, Core, no platform code) so it skips `generateFilenamePrefix`. The player registers per-track infos with `TheAudio->newAudioEventInfo(...)` under unique names `__playlist_<id>_<n>` (not via `parseMusicTrackDefinition`, so the debug next/prev list is not polluted).
- MiniAudio: make `adjustVolumeOfPlayingAudio` also walk the stream list (1.7).
- Both: expose track length in ms. Add `virtual Int getMusicLengthMs(const AsciiString& path)` on `AudioManager` with default `-1`; OpenAL/Mini implement via `FFmpegFile` duration (container duration, `avformat` field `fmt_ctx->duration`, `FFmpegFile.cpp:347` already reads it for video) or Mini's existing length probe (`MiniAudioManager.cpp:1326-1342`). Do it once per track at selection time.
- Neither backend needs a new decoder, a new thread, or any platform `#ifdef` in `GameEngine/`.

### 3.3 Smallest viable alternative (if we want even less)
Skip the generic `MusicPlaylistPlayer` abstraction and implement stage 1-3 entirely in `OpenALAudioManager`/`MiniAudioManager`. Rejected: duplicates the logic twice and ties the feature to backends; the Core-level driver above is smaller overall.

---

## 4. Decoder and crossfade

### 4.1 Decoder: reuse FFmpeg (already in tree)
- Both audio backends decode music through `FFmpegFile` (1.5). MP3 works today (retail is MP3). No new dependency, no install-time transcode, no touching `.big` files.
- **Licensing**: FFmpeg's libav* are LGPL-2.1+ when built without GPL options. Link the system/shared libraries (already how `RTS_BUILD_OPTION_FFMPEG` is wired: `pkg_check_modules` in `Core/GameEngineDevice/CMakeLists.txt` ~308) and do not static-link a `--enable-gpl` build. MP3 decoding itself needs no patent licence since the 2017 expiry.
- If FFmpeg is ever dropped from the audio path, the fallbacks in order of preference: **dr_mp3** (public domain / MIT-0 single header, same family as miniaudio) or **minimp3** (CC0), **mpg123** (LGPL-2.1, shared lib only). Install-time transcode to OGG is *not* recommended: it adds a user step, a lossy generation, and breaks "just drop MP3s".
- miniaudio itself is public domain / MIT-0, so the Mini backend adds no licensing constraint.

### 4.2 Crossfade: driver-level, wall-clock, equal-power
- Mechanism: for a transition at time T (track duration minus `Crossfade`), the player (a) calls `addAudioEvent(next)` with initial volume 0, (b) each `AudioManager::update()` computes `t = clamp((now - T) / fadeMs, 0, 1)` using a monotonic clock (not frame counts, to avoid the FPS-dependent behaviour noted in 1.6), and sets `outgoing = cos(t*pi/2)`, `incoming = sin(t*pi/2)` via `adjustVolumeOfPlayingAudio` (1.7), (c) at `t = 1` calls `removeAudioEvent(outgoingHandle)` and starts picking the next track. Equal-power avoids the 3 dB dip of a linear crossfade.
- Both streams exist concurrently, which the backends already permit (1.5). `StreamCount = 3` in `AudioSettings.ini:17` is a hint, not a hard cap on `m_playingStreams`; verify `doesViolateLimit` (`GameAudio.h:226`) does not reject the second music stream (open question 8.1). Two music streams + speech stays within 3 in the worst case.
- Per-frame gain stepping from `update()` is ~60 Hz, which is smooth enough for a 2-6 s fade (OpenAL `AL_GAIN` is applied immediately; the existing fade uses the same technique).
- Fade-in at game start and fade-out on `AHSV_StopTheMusicFade` use the same ramp, replacing the frame-based `m_fadeAudioFrames` for playlist streams only (retail streams keep current behaviour).
- Track length: needed to schedule the transition (3.2 D). If the duration is unknown or implausible (<`2*Crossfade`), fall back to a simple end-of-stream start with no overlap. Suno's MP3s should be re-encoded CBR with a Xing header (6.x) so the container duration is exact.
- Pause/mute: pausing the game with music paused (`GameLogic.cpp:4720-4733`) pauses the streams; the player must also freeze its timer (use "audio clock" that only advances while `isOn(AudioAffect_Music)` and not paused) or crossfades fire early after unpausing. Alt-tab mute (`WinMain.cpp:496-501`) is similar.

---

## 5. Determinism and netplay

Hard rules, and why this design satisfies them:
1. **Music is client-only state.** The player lives in `AudioManager`, which is updated from `GameEngine::update` inside the client section (`GameEngine.cpp:1016`), before `TheGameClient->UPDATE()`; that section is wrapped in `VERIFY_CRC` (`GameEngine.cpp:1009`, `CRCDebug.h:87`), which in debug builds asserts that client code did not alter the logic CRC. A good built-in guard for this feature.
2. **No logic RNG, no logic state writes.** Shuffle uses `GameAudioRandomValue` (separate audio RNG). The playlist player reads `ThePlayerList` only (read-only getters).
3. **No new data in checksums.** The Zero Hour CRC covers INI and map content sent to peers; `playlists.ini` and `Music/` are never loaded through `INI::load*` paths that feed the game CRC (it uses its own small parser, 2.3), and `PlayerTemplate.MusicPlaylist` is an unused-by-logic string. Caveat: **adding a field to `PlayerTemplate` changes nothing in logic**, but if a mod's `PlayerTemplate` INI is part of the exe/INI CRC (it is, as INI content), a custom general's INI must be identical across peers anyway, which the custom-generals design already requires. The playlist *folder contents* never need to match between peers, and it is OK (indeed expected) for each player to hear different music.
4. **Script-visible audio state must stay constant.** `ScriptConditions::MUSIC_TRACK_HAS_COMPLETED` (`ScriptConditions.cpp:2687`) calls `TheAudio->hasMusicTrackCompleted`, which on OpenAL is a stub returning FALSE (1.6) and for Mini returns a constant too. Do **not** make it reflect real playlist/playback state; that would let a script branch (in logic) on what a client is hearing and desync multiplayer. Add a comment and a unit test pinning this.
5. **Suppression must be decided from client-side facts only.** Suppress based on the event name and local config, never on logic state. The retail script still runs identically on every client (it still issues `MUSIC_SET_TRACK`, still updates `m_currentTrackName`, still saves into save games), the audio manager just substitutes what is rendered. Saved games therefore stay compatible: loading re-issues the retail track name (`ScriptEngine.cpp:9331-9333`) and the player re-starts the playlist.
6. **Replays**: music is not recorded; a replay played on a machine with the playlists simply hears them. Same for observers.
7. **Errors never propagate to logic**: missing/unreadable file, decode failure, FFmpeg open failure -> skip to the next track; zero playable tracks -> fall through to retail (`return` before suppression). Bound all enumeration (file count, path length) so a hostile folder cannot stall a frame.
8. **Performance**: file open + FFmpeg probe happens on selection (stage 3 can pre-open the next track a few seconds early). Probing on the main thread at track change is ~ms; the existing code already does this synchronously (`OpenALAudioManager.cpp:792-800`).

---

## 6. Staged plan

Each stage is shippable, behind the existence of `Music/`, and leaves retail behaviour unchanged when no folder is present.

| Stage | Goal | Work | Verify |
|---|---|---|---|
| 0 | Prove the pipe | No code. Hand-place a loose `Data/Audio/Tracks/USA_01.mp3` (local-FS shadowing, 1.2) and confirm OpenAL + Mini play a non-retail MP3 in a skirmish. | Both presets, volume slider, mute, pause. |
| 1 | **Play one MP3 in game** | `m_filenameIsFullPath` flag + `generateFilename` branch; `AudioManager::playPlaylistTrack(path)` test hook; env/ini switch `Music/test.mp3` starts at first suppressed `Game_*` event; suppress `Game_*`. | One USA match: custom track plays instead of `Game_USA_*`; `End_*` still plays on defeat; volume/mute honoured. |
| 2 | Folder per faction | `MusicPlaylistLibrary`: enumerate `Music/<id>/`, id resolution from local player (2.2, steps 2-5), no PlayerTemplate change yet; sequential, hard cut, loop. | USA/China/GLA each get their own folder; observer; empty folder -> retail. |
| 3 | Shuffle + gapless | Shuffle bag w/ no-repeat, `getMusicLengthMs`, start next on end (no overlap), robust skip on bad files. | Long match with 6+ tracks; deleted file mid-session. |
| 4 | **Crossfade** | Equal-power wall-clock envelope (4.2), `adjustVolumeOfPlayingAudio` for streams on Mini, pause/unpause-safe clock, fade-in at start, fade-out on stop. | Both backends; low-FPS test (crossfade length stays correct); pause during a fade. |
| 5 | Config + generals | `playlists.ini` (+ `.m3u`), `PlayerTemplate.MusicPlaylist` + fallback chain, per-general folders, options (`Order`, `Crossfade`, `Volume`). | Custom general with `MusicPlaylist = x`; case-insensitive folder names on Linux. |
| 6 | Polish | Optional `Scope = InGameAndMenus` (replace `Track*`/`Load_*`), in-game "now playing" line via `TheInGameUI->message`, next/prev debug keys wired to the player, options-menu "Custom music: on/off" check box, explicit `onGameStart()`. | Menus + loading screen behaviour; stays off by default. |

Do not combine with Stage 0 any C&C Generals base (non-ZH) work; `Generals/` shares the `Core` code so it comes along automatically, but is not a test target.

---

## 7. Suno track-set production checklist

### 7.1 Naming and structure
- One folder per playlist id (`usa`, `china`, `gla`, or a general's id).
- Files: `NN-short-title.mp3`, ASCII, lowercase, no spaces if possible (`01-iron-skies.mp3`). Number gives the `Sequential` order; ignored when shuffling.
- 6-12 tracks per faction is a good set (retail has 9-11 per side). Mix tempos: roughly 40% slow/atmospheric build-up, 40% mid-tempo, 20% high-energy.
- Instrumental only (tick instrumental in Suno, or `--instrumental` in the organizer). Vocals fight unit voice lines (EVA, unit speech sit in the same mix).
- 3-5 min per track. Fewer, longer tracks give fewer transitions.

### 7.2 Prompt style seeds (starting points, tune to taste)
- USA: "cinematic orchestral rock, snare march, brass, driving guitar, heroic, instrumental" / "post-grunge/alt-rock instrumental, big drums, wide guitars" for the "So Cold" feel.
- China: "cinematic Chinese orchestral, guzheng, erhu, taiko, bamboo flute, epic, instrumental".
- GLA: "Middle Eastern desert percussion, oud, duduk, tense, minor-key, cinematic, instrumental".
- Avoid: artist names in prompts (Suno rejects and it is a copyright smell); avoid long silence; ask for "no fade-out intro, constant energy" so the crossfade does not stack on a fade.

### 7.3 Audio prep (export -> game-ready)
1. Download from Suno. Note the existing organizer only fetches **unlocked** songs as `m4a` (`src/commands/backup.js:1-14` in the Suno organizer repo); locked songs must be unlocked/downloaded through Suno's UI. Do not route around the download gate.
2. Normalise loudness to a common target with ffmpeg `loudnorm` (EBU R128): integrated **-16 LUFS** (so the game's SFX/speech still win at the default `DefaultMusicVolume = 55%`, `AudioSettings.ini:32`), true peak <= -1.5 dBTP, LRA 11. Use the two-pass form for accuracy. Same target for every track so a shuffle is never jarring.
3. Trim leading/trailing silence (`silenceremove`, threshold -50 dB), keep ~0.3 s head room. Crossfade handles joins; do not bake fades in.
4. Encode: stereo, 44.1 kHz, **CBR 192-256 kbps**, LAME, with the Xing/Info header (ffmpeg default) so duration is exact for scheduling.
5. Tag nothing relevant; strip cover art (`-vn`) to keep files lean.
6. Sanity: `ffprobe -v error -show_entries format=duration,bit_rate -of default=nw=1 <file>`; reject tracks <60 s or <2x the configured crossfade.

A tiny shell loop (lives in the user's tools, not in the engine) is enough:
```
ffmpeg -i in.m4a -vn -af loudnorm=I=-16:TP=-1.5:LRA=11 -ar 44100 -ac 2 -c:a libmp3lame -b:a 224k out.mp3
```
(For best results, run `loudnorm` in the two-pass `measured_*` form.)

### 7.4 Existing tooling worth reusing
Checked the existing Suno organizer tool:
- `generate-set <songs.json> --style "..." --playlist "name" --instrumental` (`src/commands/generateSet.js`) generates N distinct songs with one shared style into a playlist. Ideal for "one faction = one style + one playlist".
- `mirror` / `playlists` keep a SQLite mirror; `backup` downloads unlocked audio (m4a) and metadata. A small new command `export-playlist <name> --to <dir>` that runs the 7.3 ffmpeg line would turn a Suno playlist straight into a game folder. Not in the repo today.
- Known issues to respect: name-based playlist create-or-find can create duplicates (memory note; check `BACKLOG.md` first); `audio_url` is `/api/forbidden` for every clip and downloads are per-song paywalled.
- Nothing in either repo normalises loudness or encodes MP3 today (grep for `loudnorm|ffmpeg|lufs` in `src/` and `scripts/` returns nothing).
- No existing music/playlist code in `project-generalsx/docs` beyond `docs/WORKDIR/phases/PHASE02_LINUX_AUDIO.md` (historic OpenAL bring-up notes).

---

## 8. Open questions and risks (to resolve in Stage 1 before coding further)

1. **Concurrent music streams**: confirm `doesViolateLimit` / `StreamCount` do not drop a second `AT_Music` stream on either backend. (Code reading suggests no cap in `m_playingStreams`; needs a runtime test.)
2. **Script "silence" events**: retail scripts play `Game_Silent` (60 s of silence) on events like nuke launch / lost building ("... Music Silence", "... Lost Important Silence"). Suppressing them means the playlist keeps playing through dramatic moments; not suppressing them would cut the playlist to silence then resume. Proposed: suppress `Game_Silent`, but **duck** the playlist (-12 dB for a few seconds) as a nicety in Stage 6.
3. **Absolute paths through `TheFileSystem`**: `StdLocalFileSystem::openFile` was not verified to accept absolute / user-data paths on all platforms (installs use relative-to-CWD paths today). Stage 1 must test with the XDG user data dir and `~`; fallback is to copy no data, only construct paths relative to a registered local root.
4. **Layering**: whether `Core/GameEngine/.../GameAudio.cpp` may include `PlayerList.h` (it lives under `GeneralsMD/`). If not, use the `setMusicPlaylistId` injection described in 3.2 A.
5. **Mixed generals**: retail scripts branch on `Side`, so a custom general with its own side would play no retail music; the playlist then supplies all of it. Acceptable and desirable.
6. **Disk/IO**: many large MP3 on slow NAS or SMB mounts could stall the main thread on open. Cap per-playlist count, pre-open next track early (stage 3), and measure.
7. **Fade-in for retail**: since `setShouldFade` is unimplemented (1.6), retail music has never faded in. Out of scope here; the new envelope could later be reused to fix it for retail streams too.
8. **MiniAudio/ macOS parity**: Mini's `adjustVolumeOfPlayingAudio` gap (1.7) is the one known device delta. Everything else is common.
9. Licensing of user-supplied music is the user's responsibility; Suno output usage terms apply. Do not ship any Suno tracks in the repo or in release artifacts.
