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
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"

#include "GameLogic/AutopilotTuning.h"
#include "GameLogic/AutopilotHackers.h"
#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"
#include "GameLogic/AutopilotUnits.h"

void AutopilotHackers::update()
{
	if (!m_profile.m_hackers)
		return;
	Player *player = m_host.player();

	Object *internetCenter = nullptr;
	Int hackers = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != player)
			continue;
		if (!internetCenter && obj->isKindOf(KINDOF_FS_INTERNET_CENTER) && !obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			internetCenter = obj;
		else if (!obj->isKindOf(KINDOF_STRUCTURE) && AutopilotUnits::isHacker(obj->getTemplate()))
			++hackers;
	}
	const Bool trace = m_traceThrottle.due(TheGameLogic->getFrame(), 600);	// every 20 s of game time
	if (!internetCenter)
	{
		if (trace)
			AutopilotLog::line("NETPLAY autopilot hackers: waiting for an Internet Center (frame %u)\n", TheGameLogic->getFrame());
		return;	// the hackers' home comes first
	}

	// Train one when short, at a barracks with an empty queue, only while the bank stays comfortable (this is a hoarder).
	Int barracksSeen = 0, queueBusy = 0, noHackerOption = 0;	// for the trace below
	const UnsignedInt money = player->getMoney()->countMoney();
	if (hackers < MAX_HACKERS)
	{
		for (Object *barracks = TheGameLogic->getFirstObject(); barracks; barracks = barracks->getNextObject())
		{
			if (barracks->getControllingPlayer() != player || !barracks->isKindOf(KINDOF_FS_BARRACKS) || barracks->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
				continue;
			ProductionUpdateInterface *pu = barracks->getProductionUpdateInterface();
			if (!pu)
				continue;
			++barracksSeen;
			if (pu->getProductionCount() != 0)
			{
				++queueBusy;
				continue;
			}
			const ThingTemplate *pick = nullptr;
			for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t && !pick; t = t->friend_getNextTemplate())
				if (t->isKindOf(KINDOF_INFANTRY) && AutopilotUnits::isHacker(t) && t->calcCostToBuild(player) + AutopilotTuning::HACKER_MARGIN <= money
					&& TheBuildAssistant->canMakeUnit(barracks, t) == CANMAKE_OK)
					pick = t;
			if (!pick)
				++noHackerOption;
			if (pick && pu->queueCreateUnit(pick, pu->requestUniqueUnitID()))
			{
				AutopilotLog::line("NETPLAY autopilot hackers: training %s (frame %u)\n", pick->getName().str(), TheGameLogic->getFrame());
				break;
			}
		}
	}

	if (trace)
		AutopilotLog::line("NETPLAY autopilot hackers status: %d hackers, bank %u, %d barracks (%d queue busy, %d cannot make a hacker now) (frame %u)\n", hackers, money, barracksSeen, queueBusy, noHackerOption, TheGameLogic->getFrame());

	// Every idle hacker outside goes into the Internet Center (the game decides whether there is room).
	Int orders = 0;
	for (Object *hacker = TheGameLogic->getFirstObject(); hacker && orders < 3; hacker = hacker->getNextObject())
	{
		if (hacker->getControllingPlayer() != player || hacker->isKindOf(KINDOF_STRUCTURE) || hacker->isContained() || !AutopilotUnits::isHacker(hacker->getTemplate()))
			continue;
		AIUpdateInterface *ai = hacker->getAIUpdateInterface();
		if (!ai || !ai->isIdle() || !TheActionManager->canEnterObject(hacker, internetCenter, CMD_FROM_AI, CHECK_CAPACITY))
			continue;
		ai->aiEnter(internetCenter, CMD_FROM_AI);
		++orders;
		AutopilotLog::line("NETPLAY autopilot hackers: %s -> enter the Internet Center (frame %u)\n", hacker->getTemplate()->getName().str(), TheGameLogic->getFrame());
	}
}
