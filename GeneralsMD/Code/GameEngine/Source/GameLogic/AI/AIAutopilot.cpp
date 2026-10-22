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

AIAutopilot::AIAutopilot(AutopilotHost &host, AutopilotDriver &driver, const AutopilotProfile &profile)
	: m_host(host), m_driver(driver), m_profile(profile),
	  m_economy(host, m_profile), m_defense(host, m_profile), m_explorer(host, m_profile),
	  m_squad(host, m_profile, *this), m_hackers(host, m_profile), m_repair(host, m_profile), m_superweapons(host, m_profile),
	  m_planner(host, m_profile, *this)
{
}

// GeneralsX @feature Claude 20/10/2026 Autopilot: toggle build-only skirmish AI for a human player.
void AIAutopilot::update()
{
	// Autopilot: build-only AI for a human player: keep the base growing and the economy running, but no army
	// teams, no attack waves, no skills or upgrades (those stay the player's decisions).
	m_planner.update();
	m_driver.runBaseBuilding();
	if (m_driver.tickTeamTimer())
	{
		m_driver.queueProduction();	// queues a supply truck when short, and trains any queued truck/dozer orders
		m_defense.trainDefenders();	// wallet mode only, and only under attack
		m_defense.trainStandingArmy();	// Defense Units module: a standing mixed force
		m_explorer.update();		// explore mode only
		m_squad.update();			// wallet and explore modes: a standing tank person + gun person, garrisoned
		m_hackers.update();			// wallet mode: hackers in the Internet Center
		m_repair.update();			// wallet and explore modes: repair the most damaged building
		m_superweapons.update();	// wallet and fortress: fire ready superweapons at the best visible enemy structures
		m_driver.restartTeamTimer(2 * LOGICFRAMES_PER_SECOND);
	}
}

void AIAutopilot::onStructureAttempt(const ThingTemplate *plan, BuildListInfo *info, const Object *built)
{
	m_planner.reportPlacement(plan, info, built);
}
