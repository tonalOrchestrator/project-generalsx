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

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/AIAutopilot.h"
#include "GameLogic/AutopilotHost.h"

AIAutopilot::AIAutopilot(AutopilotHost &host, const AutopilotProfile &profile)
	: m_host(host), m_profile(profile),
	  m_economy(host, m_profile), m_defense(host, m_profile), m_explorer(host, m_profile),
	  m_planner(host, m_profile, *this)
{
}

// GeneralsX @feature Claude 20/10/2026 Autopilot: toggle build-only skirmish AI for a human player.
void AIAutopilot::update()
{
	// Autopilot: build-only AI for a human player: keep the base growing and the economy running, but no army
	// teams, no attack waves, no skills or upgrades (those stay the player's decisions).
	m_planner.update();
	m_host.runBaseBuilding();
	if (m_host.tickTeamTimer())
	{
		m_host.queueProduction();	// queues a supply truck when short, and trains any queued truck/dozer orders
		m_defense.trainDefenders();	// wallet mode only, and only under attack
		m_explorer.update();		// explore mode only
		m_host.restartTeamTimer(2 * LOGICFRAMES_PER_SECOND);
	}
}

void AIAutopilot::onStructureAttempt(const ThingTemplate *plan, BuildListInfo *info, const Object *built)
{
	m_planner.reportPlacement(plan, info, built);
}
