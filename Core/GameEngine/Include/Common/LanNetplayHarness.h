/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// GeneralsX @feature Claude 20/10/2026 Headless LAN netplay harness.
//
// Drives the same LANAPI calls the Direct Connect / LAN game-options menus make, with no UI:
//   host:  -lanhost <map> [-lanHumans N] [-lanSlots H,H] [-lanObserver]
//   join:  -lanjoin <host ip>
// The host creates a direct-connect game, fills the AI slots from -lanSlots (E/M/H per slot,
// optional ":Template"), waits for N human machines, then starts. Every machine then runs the
// real network game loop (headless when -headless) until victory or -lanMaxMinutes, and prints a
// NETPLAY_RESULT JSON line (same per-player stats as the AI skirmish runner) plus periodic
// NETPLAY status lines, so a driver script can diff the machines.

#pragma once

class LanNetplayHarness
{
public:
	// Returns process exit code.
	static int run();

	// True while the harness owns the LAN flow; the LANAPI menu callbacks use this to skip UI work.
	static Bool isActive();

	// Called by the LANAPI callbacks in place of their UI handling (prints a NETPLAY line).
	static void onEvent(const char* what);
};
