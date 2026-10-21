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

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"

#include "GameLogic/AutopilotDefense.h"
#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"

// GeneralsX @feature Claude 20/10/2026 Autopilot wallet mode: enemy units near the base.
Bool AutopilotDefense::isThreatened()
{
	Player *player = m_host.player();
	const ThingTemplate *anchor = nullptr;
	for (Object *obj = TheGameLogic->getFirstObject(); obj && !anchor; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == player && obj->isKindOf(KINDOF_COMMANDCENTER))
			anchor = obj->getTemplate();
	return anchor && !m_host.isSafeToBuildAt(&m_host.baseCenter(), anchor);
}

// GeneralsX @tweak Claude 21/10/2026 The threat flag stays on for 90 seconds after enemy units were last seen near the base: a wave that steps back
// out of range for a moment is still coming back, and flipping the budget on and off stopped the spending in between.
Bool AutopilotDefense::underThreat()
{
	const UnsignedInt frame = TheGameLogic->getFrame();
	if (isThreatened())
		m_threatUntilFrame = frame + 90 * LOGICFRAMES_PER_SECOND;
	return frame < m_threatUntilFrame;
}

AutopilotDefense::Budget AutopilotDefense::budget(UnsignedInt bank)
{
	Budget b;
	b.m_threatened = m_profile.m_wallet && underThreat();
	b.m_perTemplateCap = b.m_threatened ? 16 : 3;
	b.m_totalCap = b.m_threatened ? 24 + (Int)(bank / 2000 < 24 ? bank / 2000 : 24) : 6;
	b.m_reserve = b.m_threatened ? 200 : 1500;
	return b;
}

// Wallet mode under attack: spend the bank on a few defenders. Every so often (called every two seconds) one affordable combat unit
// is queued at a barracks or war factory, up to a cap on the army; idle units defend themselves, so nothing here ever attacks.
void AutopilotDefense::trainDefenders()
{
	if (!m_profile.m_wallet || !underThreat())
		return;
	Player *player = m_host.player();
	UnsignedInt money = player->getMoney()->countMoney();
	const Int armyCap = 16 + (Int)(money / 1500 < 40 ? money / 1500 : 40);	// a fuller bank buys a bigger defending army
	Int army = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == player && !obj->isKindOf(KINDOF_STRUCTURE) && obj->isKindOf(KINDOF_CAN_ATTACK)
			&& !obj->isKindOf(KINDOF_HARVESTER) && !obj->isKindOf(KINDOF_DOZER))
			++army;
	const Bool trace = m_traceThrottle.due(TheGameLogic->getFrame(), 300);	// every 10 s of game time
	if (army >= armyCap)
	{
		if (trace)
			AutopilotLog::line("NETPLAY autopilot defenders declined: army %d at cap %d (frame %u)\n", army, armyCap, TheGameLogic->getFrame());
		return;
	}

	Int queuedNow = 0;
	Int factories = 0, queueFull = 0, nothingToBuild = 0;
	for (Object *factory = TheGameLogic->getFirstObject(); factory; factory = factory->getNextObject())
	{
		if (factory->getControllingPlayer() != player || !(factory->isKindOf(KINDOF_FS_BARRACKS) || factory->isKindOf(KINDOF_FS_WARFACTORY)))
			continue;
		if (factory->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
		if (!pu)
			continue;
		++factories;
		if (pu->getProductionCount() >= 4)
			++queueFull;
		while (pu->getProductionCount() < 4 && queuedNow < 4 && army + queuedNow < armyCap)
		{
			// The strongest (most expensive) combat unit this factory can build that leaves a small reserve and is at most a quarter of the bank.
			const ThingTemplate *best = nullptr;
			UnsignedInt bestCost = 0;
			for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
			{
				if (t->isKindOf(KINDOF_STRUCTURE) || !t->isKindOf(KINDOF_CAN_ATTACK) || t->isKindOf(KINDOF_HARVESTER) || t->isKindOf(KINDOF_DOZER))
					continue;
				const UnsignedInt cost = t->calcCostToBuild(player);
				if (cost == 0 || cost + 300 > money || cost > money / 4 + 400 || cost <= bestCost)
					continue;
				if (TheBuildAssistant->canMakeUnit(factory, t) != CANMAKE_OK)
					continue;
				best = t;
				bestCost = cost;
			}
			if (!best || !pu->queueCreateUnit(best, pu->requestUniqueUnitID()))
			{
				if (!best)
					++nothingToBuild;
				break;	// nothing affordable or buildable here: next factory
			}
			money = money > bestCost ? money - bestCost : 0;
			++queuedNow;
			AutopilotLog::line("NETPLAY autopilot defender: %s (cost %u) at %s (frame %u)\n", best->getName().str(), bestCost, factory->getTemplate()->getName().str(), TheGameLogic->getFrame());
		}	// while the factory has queue room
	}
	if (trace && queuedNow == 0)
		AutopilotLog::line("NETPLAY autopilot defenders none queued: army %d/%d, %d factories (%d queue full, %d nothing affordable), bank %u (frame %u)\n", army, armyCap, factories, queueFull, nothingToBuild, money, TheGameLogic->getFrame());
}
