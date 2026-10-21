# GeneralsX (page text for the website)

Source: https://github.com/tonalOrchestrator/project-generalsx (branch `feat/netplay-autopilot`).
Everything below was measured, not planned. Figures are from automated test runs on 2026-10-20.

## What it is
A cross-platform port of Command & Conquer: Generals / Zero Hour for Linux and macOS (SDL3 + DXVK + OpenAL), maintained upstream by
fbraz3/GeneralsX. This fork adds Windows/Linux network play testing and an in-game AI helper. You need your own copy of the game;
no game data is included.

## Same match, two platforms
`pair-side-by-side.png` (or the two halves, `pair-linux-0200.png` and `pair-windows-0200.png`): one LAN match between a Linux
desktop and a Windows laptop, each machine controlling its own human player (USA on Linux, GLA on Windows) with Autopilot on, at
2:00 of game time. Both machines ran the same simulation: the game state checksum matched on every game-minute and the final
per-player statistics were identical.

## Autopilot
A build-only AI you can switch on for yourself in a networked game where you cannot pause (Ctrl+Shift+A; Ctrl+Shift+L cycles the
level). It builds your base and economy and never attacks, so it is a helper rather than a second player.
- **Easy / Normal / Hard** use the game's own skirmish AI skill levels: Hard builds more, earns more and adds a second supply center.
- **Wallet Mode** is "defend, survive, hoard money": it builds the foundation, then stops spending and keeps a cash reserve.
- Verified on all 12 factions and generals at all four levels (48 automated runs, about two minutes to run the whole set).
- **Explore Mode** sends capture-capable infantry to take neutral buildings (oil derricks, tech buildings) for income. On a map with 30+
  neutral buildings it captured 8 to 14 of them and earned roughly four times the money of plain Autopilot over 12 game-minutes.
- Under attack, Wallet Mode spends its bank on extra defenses and a few defenders. Measured over 14 game-minutes against one computer
  opponent: against a Medium AI, plain Autopilot survives in 0 of 12 runs and Wallet Mode in 12 of 12; against a Hard AI Wallet Mode now
  lasts about 13 game-minutes instead of 11 (a Hard AI still wins in the end).
- The switch goes through the network command stream, so every machine flips on the same game frame.

## Testing tools
- AI-vs-AI skirmish runner (headless or watchable), a scripted LAN match between two machines with automatic desync checking,
  and a scenario matrix that checks every faction and level in minutes. A 4 game-minute test runs in about 5 seconds.

## Honest limits
- Cheat mode (opt-in `-cheats`) is built but not yet tried in play.
- Autopilot never attacks. Only Wallet Mode trains a few defenders, and only when the base is under attack; it cannot hold off a Hard AI.
- Not yet tried over the internet or against players on the original retail game.
