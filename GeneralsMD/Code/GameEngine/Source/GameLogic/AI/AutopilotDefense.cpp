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

#include "GameLogic/AutopilotTuning.h"
#include "GameLogic/AutopilotDefense.h"
#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"
#include "GameLogic/AutopilotUnits.h"

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
		m_threatUntilFrame = frame + AutopilotTuning::THREAT_MEMORY_SECONDS * LOGICFRAMES_PER_SECOND;
	return frame < m_threatUntilFrame;
}

AutopilotDefense::Budget AutopilotDefense::budget(UnsignedInt bank)
{
	Budget b;
	b.m_threatened = m_profile.m_wallet && underThreat();
	b.m_perTemplateCap = b.m_threatened ? AutopilotTuning::THREATENED_PER_KIND_CAP : AutopilotTuning::QUIET_PER_KIND_CAP;
	b.m_totalCap = b.m_threatened ? AutopilotTuning::THREATENED_TOTAL_CAP_BASE + (Int)(bank / AutopilotTuning::DEFENSE_CAP_BANK_STEP < AutopilotTuning::THREATENED_TOTAL_CAP_BONUS_MAX ? bank / AutopilotTuning::DEFENSE_CAP_BANK_STEP : AutopilotTuning::THREATENED_TOTAL_CAP_BONUS_MAX) : AutopilotTuning::QUIET_TOTAL_CAP;
	b.m_reserve = b.m_threatened ? AutopilotTuning::THREATENED_RESERVE : AutopilotTuning::QUIET_RESERVE;
	return b;
}

// GeneralsX @tweak Claude 21/10/2026 Units never spend the last DEFENSE_FUND of the bank: the base defenses (about $1,000 each) are what held the line
// against a Hard AI, and the defender trainer, running every two seconds, used to spend the bank down to $300 before the planner could afford one.
static const UnsignedInt DEFENSE_FUND = AutopilotTuning::DEFENSE_FUND;

// GeneralsX @feature Claude 22/10/2026 What units must leave in the bank: the defense fund, or the superweapon price while the planner is saving for one
// (and a small army already stands).
UnsignedInt AutopilotDefense::fundFor(Int army) const
{
	return (m_savingTarget > DEFENSE_FUND && army >= AutopilotTuning::SUPERWEAPON_SAVE_MIN_ARMY) ? m_savingTarget : DEFENSE_FUND;
}

// Wallet mode under attack: spend the bank on a few defenders. Every so often (called every two seconds) one affordable combat unit
// is queued at a barracks or war factory, up to a cap on the army; idle units defend themselves, so nothing here ever attacks.
void AutopilotDefense::trainDefenders()
{
	if (!m_profile.m_wallet || !underThreat())
		return;
	Player *player = m_host.player();
	UnsignedInt money = player->getMoney()->countMoney();
	const Int armyCap = AutopilotTuning::ARMY_CAP_BASE + (Int)(money / AutopilotTuning::ARMY_BANK_STEP < AutopilotTuning::ARMY_CAP_BONUS_MAX ? money / AutopilotTuning::ARMY_BANK_STEP : AutopilotTuning::ARMY_CAP_BONUS_MAX);	// a fuller bank buys a bigger defending army
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
				if (cost == 0 || cost + fundFor(army + queuedNow) > money || cost > money / 4 + 400 || cost <= bestCost)
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

// GeneralsX @feature Claude 21/10/2026 Defense Units module. Unlike trainDefenders (reactive, wallet only, "the strongest affordable unit"), this keeps
// a standing mixed force so an attack meets tanks, tank hunters and riflemen already at home: the three roles rotate, one unit per call, at a
// factory with an empty queue. Sized by the bank (6 units, +1 per $2,000, up to 20) and never below the defense fund, so structures still get
// bought first. Units idle at their factory and defend themselves; nothing here attacks.
void AutopilotDefense::trainStandingArmy()
{
	if (!m_profile.m_defenseUnits)
		return;
	Player *player = m_host.player();
	const UnsignedInt money = player->getMoney()->countMoney();
	const Int target = AutopilotTuning::STANDING_ARMY_BASE + (Int)(money / AutopilotTuning::STANDING_ARMY_BANK_STEP < AutopilotTuning::STANDING_ARMY_BONUS_MAX ? money / AutopilotTuning::STANDING_ARMY_BANK_STEP : AutopilotTuning::STANDING_ARMY_BONUS_MAX);
	Int army = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == player && !obj->isKindOf(KINDOF_STRUCTURE) && obj->isKindOf(KINDOF_CAN_ATTACK)
			&& !obj->isKindOf(KINDOF_HARVESTER) && !obj->isKindOf(KINDOF_DOZER))
			++army;
	if (army >= target)
	{
		if (m_armyTrace.due(TheGameLogic->getFrame(), 600))
			AutopilotLog::line("NETPLAY autopilot standing army: full, %d of %d (frame %u)\n", army, target, TheGameLogic->getFrame());
		return;
	}

	Int factoriesSeen = 0, factoriesBusy = 0;	// for the trace at the end
	for (Int attempt = 0; attempt < 3; ++attempt)	// the wanted role first, then the others
	{
		const Int role = (m_nextRole + attempt) % 3;
		for (Object *factory = TheGameLogic->getFirstObject(); factory; factory = factory->getNextObject())
		{
			if (factory->getControllingPlayer() != player || factory->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
				continue;
			if (!(role == 0 ? factory->isKindOf(KINDOF_FS_WARFACTORY) : factory->isKindOf(KINDOF_FS_BARRACKS)))
				continue;
			ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
			if (!pu)
				continue;
			++factoriesSeen;
			if (pu->getProductionCount() != 0)
			{
				++factoriesBusy;
				continue;
			}
			const ThingTemplate *pick = nullptr;
			UnsignedInt pickCost = 0;
			for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
			{
				if (t->isKindOf(KINDOF_STRUCTURE) || !t->isKindOf(KINDOF_CAN_ATTACK) || t->isKindOf(KINDOF_HARVESTER) || t->isKindOf(KINDOF_DOZER))
					continue;
				const Bool fits = role == 0 ? (t->isKindOf(KINDOF_VEHICLE) && !t->isKindOf(KINDOF_AIRCRAFT))
					: (role == 1 ? AutopilotUnits::isAntiVehicleInfantry(t) : AutopilotUnits::isRifleInfantry(t));
				if (!fits)
					continue;
				const UnsignedInt cost = t->calcCostToBuild(player);
				if (cost == 0 || cost + fundFor(army) > money || cost > money / 3 + AutopilotTuning::DEFENSE_UNIT_COST_SLACK || cost <= pickCost || TheBuildAssistant->canMakeUnit(factory, t) != CANMAKE_OK)
					continue;
				pick = t;
				pickCost = cost;
			}
			if (pick && pu->queueCreateUnit(pick, pu->requestUniqueUnitID()))
			{
				m_nextRole = (role + 1) % 3;
				AutopilotLog::line("NETPLAY autopilot standing army: %s (cost %u), army %d/%d (frame %u)\n", pick->getName().str(), pickCost, army, target, TheGameLogic->getFrame());
				AutopilotLog::feed(player, "Defense units: training a %s (%d of %d)", AutopilotLog::niceName(pick).str(), army + 1, target);
				return;
			}
		}
	}
	if (m_armyTrace.due(TheGameLogic->getFrame(), 600))
		AutopilotLog::line("NETPLAY autopilot standing army: none queued: army %d/%d, %d factories seen (%d busy), bank %u (frame %u)\n", army, target, factoriesSeen, factoriesBusy, money, TheGameLogic->getFrame());
}
