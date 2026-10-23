/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

// GeneralsX @refactor Claude 21/10/2026 The scripted-LAN test harness settings, kept out of GlobalData (which 150+ files include, so
// every added flag used to rebuild most of the game). Filled from the command line, read by the harness and the few places that
// adapt to it. See LanNetplayHarness.h for what the flags drive.

#pragma once

#include "Common/AsciiString.h"

struct LanNetplayOptions
{
	Bool m_active = FALSE;					///< run the scripted LAN host/join flow instead of the shell (-lanhost / -lanjoin)
	Bool m_host = FALSE;					///< host the game (otherwise join m_joinIP)
	AsciiString m_joinIP;					///< host address to join (dotted quad)
	AsciiString m_localIP;					///< force the local interface address (empty = first non-loopback)
	AsciiString m_map;						///< host only: map key or display-name substring
	AsciiString m_slots = "H,H";			///< host only: AI slots after the humans, e.g. "H,H" (E/M/H, optional ":Template")
	AsciiString m_name = "NetplayBot";		///< local player name
	AsciiString m_hostTemplate;				///< faction template for the host's own slot (empty = random)
	AsciiString m_statsFile;				///< if set, JSON stats are also written here
	Int m_humans = 2;						///< host only: number of human machines (including the host) to wait for
	Bool m_observer = FALSE;				///< the human machines observe instead of playing
	Int m_maxMinutes = 30;					///< safety cap in game minutes (0 = none)
	Int m_seed = -1;						///< host only: explicit game seed (-1 = clock)
	Bool m_fast = FALSE;					///< skip the network frame pacing (faster than real time; changes the wait, not the game)
	Int m_screenshotSeconds = 0;			///< windowed: in-engine PNG screenshot every N game seconds (0 = off)
	Int m_cameraSeconds = 45;				///< windowed: seconds between camera hops across AI bases (0 = only at start)
	Bool m_cameraLocal = FALSE;				///< windowed: the watch camera stays on the local player's base
	AsciiString m_autopilotSchedule;		///< "sec:level,sec:level": send autopilot level changes at those game seconds
	AsciiString m_cheatSchedule;			///< "sec:money:points,...": send cheat gifts (needs -cheats on every machine)
	AsciiString m_soloSchedule;				///< -file map: "sec:level,...": autopilot changes for the local player of a single-player map (Challenge mode); see AISkirmishSimulation::runSolo
	AsciiString m_soloTemplate;				///< -file Challenge map: the human's faction template (empty = the general whose map it is)
	Int m_soloSnapshotSeconds = 0;			///< -file map: print AUTOPILOT_SNAPSHOT (what the local player can see, JSON) every N game seconds (0 = off)
	Int m_soloMaxMinutes = 10;				///< -file map with -soloAutopilot: stop after this many game minutes and print the stats
	AsciiString m_directiveSchedule;		///< "sec:kind:objectId[:seconds],...": the local player sends an autopilot directive (MSG_AUTOPILOT_DIRECTIVE)
	AsciiString m_buildSchedule;			///< "sec:barracks,...": the local player orders a structure by hand (select dozer + construct)
};

// The one instance, filled by the command line parser.
LanNetplayOptions& lanNetplayOptions();
