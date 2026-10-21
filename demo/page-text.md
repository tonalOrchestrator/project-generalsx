# GeneralsX (page text for the website)

Source: https://github.com/tonalOrchestrator/project-generalsx (branch `feat/netplay-autopilot`).
Everything below was measured, not planned. Figures are from automated test runs on 2026-10-20.

## What it is
A cross-platform port of Command & Conquer: Generals / Zero Hour for Linux and macOS (SDL3 + DXVK + OpenAL), maintained upstream by
fbraz3/GeneralsX. This fork adds Windows/Linux network play testing and an in-game AI helper. You need your own copy of the game;
no game data is included.

## Same match, two platforms
A LAN match between a Windows laptop and a Linux desktop stays in lockstep: the game state checksum matched on every game-minute and
the final per-player statistics were identical on both machines, including with Autopilot switching modes and cheat gifts on both.
(Screenshots of a two-player match, one per machine, are being added to this folder.)

## Autopilot
A build-only AI you can switch on for yourself in a networked game where you cannot pause (Ctrl+Shift+A; Ctrl+Shift+L cycles the
level). It builds your base and economy and never attacks, so it is a helper rather than a second player.
- **Easy / Normal / Hard** use the game's own skirmish AI skill levels: Hard builds more, earns more and adds a second supply center.
- **Wallet Mode** is "defend, survive, hoard money": it builds the foundation, then stops spending and keeps a cash reserve.
- Verified on all 12 factions and generals at all four levels (48 automated runs, about two minutes to run the whole set).
- The switch goes through the network command stream, so every machine flips on the same game frame.

## Testing tools
- AI-vs-AI skirmish runner (headless or watchable), a scripted LAN match between two machines with automatic desync checking,
  and a scenario matrix that checks every faction and level in minutes. A 4 game-minute test runs in about 5 seconds.

## Honest limits
- Cheat mode (opt-in `-cheats`) is built but not yet tried in play.
- Autopilot does not build an army or defend with units; Wallet Mode's defenses are limited on some factions.
- Not yet tried over the internet or against players on the original retail game.
