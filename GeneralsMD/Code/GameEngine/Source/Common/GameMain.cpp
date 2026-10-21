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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// GameMain.cpp
// The main entry point for the game
// Author: Michael S. Booth, April 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/FramePacer.h"
#include "Common/GameEngine.h"
#include "Common/ReplaySimulation.h"
#include "Common/AISkirmishSimulation.h"
#include "Common/LanNetplayHarness.h"
#include "Common/GlobalData.h"
#include "Common/LanNetplayOptions.h"

// GeneralsX @feature Claude 21/10/2026 defined in ChallengeMenu.cpp
extern Bool setupChallengeForMap( const AsciiString& mapName, const AsciiString& playerTemplateName );


/**
 * This is the entry point for the game system.
 */
Int GameMain()
{
	int exitcode = 0;
	// initialize the game engine using factory function
	TheFramePacer = new FramePacer();
	TheFramePacer->enableFramesPerSecondLimit(TRUE);
	TheGameEngine = CreateGameEngine();
	TheGameEngine->init();

	if (!TheGlobalData->m_simulateReplays.empty())
	{
		exitcode = ReplaySimulation::simulateReplays(TheGlobalData->m_simulateReplays, TheGlobalData->m_simulateReplayJobs);
	}
	else if (TheGlobalData->m_aiSkirmish)
	{
		// GeneralsX @feature Claude 30/09/2026 AI-vs-AI skirmish runner
		exitcode = AISkirmishSimulation::run();
	}
	else if (lanNetplayOptions().m_soloSchedule.isNotEmpty() && TheGlobalData->m_initialFile.isNotEmpty())
	{
		// GeneralsX @feature Claude 21/10/2026 Autopilot on a single-player map (Challenge mode)
		// The menu normally prepares the Challenge state (campaign, human slot); do it for the -file map.
		Bool challenge = setupChallengeForMap(TheGlobalData->m_initialFile, lanNetplayOptions().m_soloTemplate);
		printf("AISkirmishSolo: challenge setup %s\n", challenge ? "done" : "not a Challenge map, plain single-player start");
		exitcode = AISkirmishSimulation::runSolo();
	}
	else if (lanNetplayOptions().m_active)
	{
		// GeneralsX @feature Claude 20/10/2026 Headless LAN netplay harness
		exitcode = LanNetplayHarness::run();
	}
	else
	{
		// run it
		TheGameEngine->execute();
	}

	// since execute() returned, we are exiting the game
	delete TheFramePacer;
	TheFramePacer = nullptr;
	delete TheGameEngine;
	TheGameEngine = nullptr;

	return exitcode;
}

