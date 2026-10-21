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

#include "Common/ActionManager.h"
#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"

#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"
#include "GameLogic/AutopilotSquad.h"
#include "GameLogic/AutopilotUnits.h"

// Combat infantry the squad looks after: not a hacker or explorer (they have jobs), not a Black Lotus, not inside something already.
Bool AutopilotSquad::isSquadUnit(const Object *unit) const
{
	if (unit->getControllingPlayer() != m_host.player() || unit->isKindOf(KINDOF_STRUCTURE) || !unit->isKindOf(KINDOF_INFANTRY) || !unit->isKindOf(KINDOF_CAN_ATTACK))
		return FALSE;
	if (AutopilotUnits::isHacker(unit->getTemplate()) || m_links.isExplorer(unit))
		return FALSE;
	return AutopilotUnits::isRifleInfantry(unit->getTemplate()) || AutopilotUnits::isAntiVehicleInfantry(unit->getTemplate());
}

void AutopilotSquad::update()
{
	if (m_profile.m_standingSquad <= 0)
		return;
	train();
	garrison();
}

// One squad member per call at an empty-queued barracks, alternating tank person / gun person, only while the bank stays comfortable.
void AutopilotSquad::train()
{
	Player *player = m_host.player();
	Int members = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		if (isSquadUnit(obj))
			++members;
	if (members >= m_profile.m_standingSquad)
		return;

	const UnsignedInt money = player->getMoney()->countMoney();
	for (Object *barracks = TheGameLogic->getFirstObject(); barracks; barracks = barracks->getNextObject())
	{
		if (barracks->getControllingPlayer() != player || !barracks->isKindOf(KINDOF_FS_BARRACKS) || barracks->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		ProductionUpdateInterface *pu = barracks->getProductionUpdateInterface();
		if (!pu || pu->getProductionCount() != 0)
			continue;
		const ThingTemplate *pick = nullptr;
		for (Int attempt = 0; attempt < 2 && !pick; ++attempt)	// the wanted role first, then the other one
		{
			const Bool wantAntiVehicle = (attempt == 0) ? m_nextAntiVehicle : !m_nextAntiVehicle;
			UnsignedInt pickCost = 0;
			for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
			{
				if (!(wantAntiVehicle ? AutopilotUnits::isAntiVehicleInfantry(t) : AutopilotUnits::isRifleInfantry(t)) || !t->isKindOf(KINDOF_INFANTRY))
					continue;
				const UnsignedInt cost = t->calcCostToBuild(player);
				if (cost == 0 || cost + 1500 > money || cost <= pickCost || TheBuildAssistant->canMakeUnit(barracks, t) != CANMAKE_OK)
					continue;
				pick = t;
				pickCost = cost;
			}
		}
		if (pick && pu->queueCreateUnit(pick, pu->requestUniqueUnitID()))
		{
			m_nextAntiVehicle = !m_nextAntiVehicle;
			AutopilotLog::line("NETPLAY autopilot squad: training %s (frame %u)\n", pick->getName().str(), TheGameLogic->getFrame());
			return;
		}
	}
}

// Send idle squad infantry into the nearest garrisonable building within reach of the base (neutral buildings count; the game decides
// who may enter and whether there is room). A few orders per call.
void AutopilotSquad::garrison()
{
	const Coord3D &center = m_host.baseCenter();
	const Real reach = m_host.baseRadius() + 1500.0f;	// neutral buildings are rarely right at the base: about a quarter of a typical map
	Int orders = 0;
	for (Object *unit = TheGameLogic->getFirstObject(); unit && orders < 3; unit = unit->getNextObject())
	{
		if (!isSquadUnit(unit) || unit->isContained())
			continue;
		AIUpdateInterface *ai = unit->getAIUpdateInterface();
		if (!ai || !ai->isIdle())
			continue;

		Object *best = nullptr;
		Real bestDistSqr = 0;
		Int withContain = 0, inReach = 0, refused = 0, refusedEnemy = 0, refusedFull = 0, refusedShroud = 0;
		Object *hidden = nullptr;	// nearest garrison building still in the shroud: somewhere to scout when nothing can be entered yet
		Real hiddenDistSqr = 0;	// for the trace below
		for (Object *b = TheGameLogic->getFirstObject(); b; b = b->getNextObject())
		{
			if (!b->isKindOf(KINDOF_STRUCTURE) || !b->getContain())
				continue;
			++withContain;
			const Real cx = b->getPosition()->x - center.x, cy = b->getPosition()->y - center.y;
			if (cx * cx + cy * cy > reach * reach)
				continue;
			++inReach;
			if (!TheActionManager->canEnterObject(unit, b, CMD_FROM_AI, CHECK_CAPACITY))
			{
				++refused;
				// Why: an enemy's building, one that is full, or something else (still under construction, wrong unit kind, ...).
				if (b->getShroudedStatus(m_host.player()->getPlayerIndex()) != OBJECTSHROUD_CLEAR)
				{
					++refusedShroud;
					const Real hx = b->getPosition()->x - unit->getPosition()->x, hy = b->getPosition()->y - unit->getPosition()->y;
					if (!hidden || hx * hx + hy * hy < hiddenDistSqr)
					{
						hidden = b;
						hiddenDistSqr = hx * hx + hy * hy;
					}
				}
				else if (b->getControllingPlayer() && m_host.player()->getRelationship(b->getControllingPlayer()->getDefaultTeam()) == ENEMIES)
					++refusedEnemy;
				else if (TheActionManager->canEnterObject(unit, b, CMD_FROM_AI, DONT_CHECK_CAPACITY))
					++refusedFull;
				continue;
			}
			const Real dx = b->getPosition()->x - unit->getPosition()->x, dy = b->getPosition()->y - unit->getPosition()->y;
			const Real distSqr = dx * dx + dy * dy;
			if (!best || distSqr < bestDistSqr)
			{
				best = b;
				bestDistSqr = distSqr;
			}
		}
		if (!best && hidden)
		{
			// GeneralsX @feature Claude 21/10/2026 Walk to it: seeing the building lifts the shroud, and the next cycle can order the entry.
			ai->aiMoveToPosition(hidden->getPosition(), CMD_FROM_AI);
			++orders;
			AutopilotLog::line("NETPLAY autopilot squad: %s -> scout %s (frame %u)\n", unit->getTemplate()->getName().str(), hidden->getTemplate()->getName().str(), TheGameLogic->getFrame());
			continue;
		}
		if (!best && m_traceThrottle.due(TheGameLogic->getFrame(), 600))
			AutopilotLog::line("NETPLAY autopilot squad: nothing to garrison for %s: %d buildings can hold troops, %d within reach of the base, %d of those refuse [%d in the shroud, %d enemy-owned, %d full] (frame %u)\n",
				unit->getTemplate()->getName().str(), withContain, inReach, refused, refusedShroud, refusedEnemy, refusedFull, TheGameLogic->getFrame());
		if (best)
		{
			ai->aiEnter(best, CMD_FROM_AI);
			++orders;
			AutopilotLog::line("NETPLAY autopilot squad: %s -> garrison %s (frame %u)\n", unit->getTemplate()->getName().str(), best->getTemplate()->getName().str(), TheGameLogic->getFrame());
		}
	}
}
