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
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

#include "GameLogic/AutopilotExplorer.h"
#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"
#include "GameLogic/AutopilotUnits.h"

Bool AutopilotExplorer::isExplorer(const Object *unit) const
{
	if (!m_profile.m_explore)
		return FALSE;
	Int seen = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj && seen < EXPLORER_ROSTER; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == m_host.player() && !obj->isKindOf(KINDOF_STRUCTURE) && obj->hasSpecialPower(SPECIAL_INFANTRY_CAPTURE_BUILDING))
		{
			if (obj == unit)
				return TRUE;
			++seen;
		}
	return FALSE;
}

AutopilotExplorer::AutopilotExplorer(AutopilotHost &host, const AutopilotProfile &profile)
	: m_host(host), m_profile(profile)
{
	for (Int e = 0; e < MAX_EXPLORERS; ++e)
	{
		m_explorer[e] = INVALID_ID;
		m_target[e] = INVALID_ID;
		m_scouting[e] = FALSE;
	}
	for (Int b = 0; b < BLACKLIST_SIZE; ++b)
	{
		m_blacklist[b] = INVALID_ID;
		m_blacklistUntil[b] = 0;
	}
}

// GeneralsX @feature Claude 21/10/2026 Autopilot explore mode (level 5): normal autopilot plus explorers. An explorer is an infantry unit
// that can capture buildings (Ranger, Red Guard, Rebel). One is trained at a barracks, then every idle explorer is sent, with the game's
// own capture special power, to the nearest safe neutral capturable building (oil derricks and the other tech buildings); a building it
// fails to take (unreachable, destroyed first) is skipped for a while. Nothing here attacks an enemy.
void AutopilotExplorer::update()
{
	if (!m_profile.m_explore)
		return;
	Player *player = m_host.player();
	const UnsignedInt frame = TheGameLogic->getFrame();
	const Int wantedExplorers = EXPLORER_ROSTER;

	// The explorers we have, and forget errands whose explorer is idle again (done, or failed).
	Object *explorers[MAX_EXPLORERS];
	Int numExplorers = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj && numExplorers < wantedExplorers; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == player && !obj->isKindOf(KINDOF_STRUCTURE) && obj->hasSpecialPower(SPECIAL_INFANTRY_CAPTURE_BUILDING))
			explorers[numExplorers++] = obj;	// the first EXPLORER_ROSTER capture-capable units are the explorers; any others belong to the garrison squad

	forgetFinishedErrands(frame);

	// Infantry cannot capture anything until the player has bought the faction's "Capture Building" upgrade (the same player upgrade for
	// the Ranger, Red Guard and Rebel); the host finds a barracks queue for it and ignores a request it cannot place.
	static const char *const captureUpgradeName = "Upgrade_InfantryCaptureBuilding";
	const UpgradeTemplate *captureUpgrade = TheUpgradeCenter->findUpgrade(captureUpgradeName);
	if (captureUpgrade && !player->hasUpgradeComplete(captureUpgrade) && !player->hasUpgradeInProduction(captureUpgrade)
		&& TheUpgradeCenter->canAffordUpgrade(player, captureUpgrade))
	{
		m_host.requestUpgrade(captureUpgradeName);
		if (AutopilotLog::enabled() && player->hasUpgradeInProduction(captureUpgrade))
			AutopilotLog::line("NETPLAY autopilot explore: capture upgrade queued (frame %u)\n", frame);
	}

	if (m_statusThrottle.due(frame, 600))
		AutopilotLog::line("NETPLAY autopilot explore status: %d explorers, capture upgrade %s (frame %u)\n", numExplorers,
			!captureUpgrade ? "unknown" : (player->hasUpgradeComplete(captureUpgrade) ? "done" : (player->hasUpgradeInProduction(captureUpgrade) ? "in production" : "not started")), frame);

	// Train one when short.
	if (numExplorers < wantedExplorers)
		trainExplorer(frame);

	sendIdleExplorers(explorers, numExplorers, frame);
}

// Forget errands whose explorer is idle again (done, or failed), blacklisting the building of a failed one.
void AutopilotExplorer::forgetFinishedErrands(UnsignedInt frame)
{
	Player *player = m_host.player();
	for (Int e = 0; e < MAX_EXPLORERS; ++e)
	{
		if (m_explorer[e] == INVALID_ID)
			continue;
		Object *who = TheGameLogic->findObjectByID(m_explorer[e]);
		AIUpdateInterface *whoAI = who ? who->getAIUpdateInterface() : nullptr;
		if (who && whoAI && !whoAI->isIdle())
			continue;	// still on its errand
		Object *target = TheGameLogic->findObjectByID(m_target[e]);
		const Bool stillHidden = target && target->getShroudedStatus(player->getPlayerIndex()) != OBJECTSHROUD_CLEAR;
		if (who && target && target->getControllingPlayer() != player && (!m_scouting[e] || stillHidden))
		{
			// Idle again but the building is not ours (a scouting walk that did not reveal it counts as a failure too): skip that
			// building for five minutes.
			for (Int b = 0; b < BLACKLIST_SIZE; ++b)
				if (m_blacklist[b] == INVALID_ID || m_blacklistUntil[b] <= frame)
				{
					m_blacklist[b] = m_target[e];
					m_blacklistUntil[b] = frame + 300 * LOGICFRAMES_PER_SECOND;
					break;
				}
		}
		m_explorer[e] = INVALID_ID;
		m_target[e] = INVALID_ID;
		m_scouting[e] = FALSE;
	}
}

// The strongest affordable capture-capable infantry at a finished barracks with an empty queue.
void AutopilotExplorer::trainExplorer(UnsignedInt frame)
{
	Player *player = m_host.player();
	const UnsignedInt money = player->getMoney()->countMoney();
	for (Object *barracks = TheGameLogic->getFirstObject(); barracks; barracks = barracks->getNextObject())
	{
		if (barracks->getControllingPlayer() != player || !barracks->isKindOf(KINDOF_FS_BARRACKS) || barracks->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		ProductionUpdateInterface *pu = barracks->getProductionUpdateInterface();
		if (!pu || pu->getProductionCount() != 0)
			continue;
		const ThingTemplate *pick = nullptr;
		for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
		{
			if (!t->isKindOf(KINDOF_INFANTRY) || !t->isKindOf(KINDOF_CAN_ATTACK))
				continue;
			if (!AutopilotUnits::isCaptureCapableInfantry(t))
				continue;
			if (t->calcCostToBuild(player) + 500 > money || TheBuildAssistant->canMakeUnit(barracks, t) != CANMAKE_OK)
				continue;
			pick = t;
			break;
		}
		if (pick && pu->queueCreateUnit(pick, pu->requestUniqueUnitID()))
		{
			AutopilotLog::line("NETPLAY autopilot explore: training %s (frame %u)\n", pick->getName().str(), frame);
			break;
		}
	}
}

// Send every idle explorer to the nearest safe, neutral, capturable building that nobody is already heading for.
void AutopilotExplorer::sendIdleExplorers(Object *const *explorers, Int numExplorers, UnsignedInt frame)
{
	Player *player = m_host.player();
	for (Int i = 0; i < numExplorers; ++i)
	{
		Object *explorer = explorers[i];
		AIUpdateInterface *ai = explorer->getAIUpdateInterface();
		if (!ai || !ai->isIdle())
			continue;
		Int slot = -1;
		for (Int e = 0; e < MAX_EXPLORERS && slot < 0; ++e)
			if (m_explorer[e] == INVALID_ID)
				slot = e;
		if (slot < 0)
			break;

		Object *best = nullptr;
		Real bestDistSqr = 0;
		Object *hidden = nullptr;	// nearest neutral building still in the shroud: somewhere to scout when nothing can be captured yet
		Real hiddenDistSqr = 0;
		Int seen = 0, notNeutral = 0, takenOrSkipped = 0, cannotCapture = 0, unsafe = 0, shrouded = 0, notReady = 0;	// for the trace below
		for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (!obj->isKindOf(KINDOF_STRUCTURE) || !obj->isKindOf(KINDOF_CAPTURABLE) || obj->isKindOf(KINDOF_REBUILD_HOLE))
				continue;
			++seen;
			// Only buildings nobody owns: the game's neutral player (tech buildings, oil derricks, ...). The relationship toward such a
			// building is not NEUTRAL, so the owner is what is checked; a player's own, allied or enemy buildings are never targets.
			if (obj->getControllingPlayer() != ThePlayerList->getNeutralPlayer())
			{
				++notNeutral;
				continue;	// ours, an ally's or an enemy's (never attack an enemy)
			}
			Bool taken = FALSE;
			for (Int e = 0; e < MAX_EXPLORERS && !taken; ++e)
				taken = (m_target[e] == obj->getID());
			for (Int b = 0; b < BLACKLIST_SIZE && !taken; ++b)
				taken = (m_blacklist[b] == obj->getID() && m_blacklistUntil[b] > frame);
			if (taken)
			{
				++takenOrSkipped;
				continue;
			}
			if (!TheActionManager->canCaptureBuilding(explorer, obj, CMD_FROM_AI))
			{
				++cannotCapture;
				SpecialPowerModuleInterface *spi = explorer->findSpecialPowerModuleInterface(SPECIAL_INFANTRY_CAPTURE_BUILDING);
				if (spi && spi->getPercentReady() < 1.0f)
					++notReady;
				else if (obj->getShroudedStatus(player->getPlayerIndex()) != OBJECTSHROUD_CLEAR)
				{
					++shrouded;
					if (m_host.isSafeToBuildAt(obj->getPosition(), obj->getTemplate()))
					{
						const Real hx = obj->getPosition()->x - explorer->getPosition()->x;
						const Real hy = obj->getPosition()->y - explorer->getPosition()->y;
						if (!hidden || hx * hx + hy * hy < hiddenDistSqr)
						{
							hidden = obj;
							hiddenDistSqr = hx * hx + hy * hy;
						}
					}
				}
				continue;
			}
			if (!m_host.isSafeToBuildAt(obj->getPosition(), obj->getTemplate()))
			{
				++unsafe;
				continue;
			}
			const Real dx = obj->getPosition()->x - explorer->getPosition()->x;
			const Real dy = obj->getPosition()->y - explorer->getPosition()->y;
			const Real distSqr = dx * dx + dy * dy;
			if (!best || distSqr < bestDistSqr)
			{
				best = obj;
				bestDistSqr = distSqr;
			}
		}
		if (!best && hidden)
		{
			// Walk to the hidden building: seeing it lifts the shroud, and the next cycle can then order the capture.
			ai->aiMoveToPosition(hidden->getPosition(), CMD_FROM_AI);
			m_explorer[slot] = explorer->getID();
			m_target[slot] = hidden->getID();
			m_scouting[slot] = TRUE;
			AutopilotLog::line("NETPLAY autopilot explore: %s -> scout %s at (%.0f,%.0f) (frame %u)\n", explorer->getTemplate()->getName().str(),
				hidden->getTemplate()->getName().str(), hidden->getPosition()->x, hidden->getPosition()->y, frame);
			continue;
		}
		if (!best)
		{
			if (m_nothingToCaptureThrottle.due(frame, 600))
				AutopilotLog::line("NETPLAY autopilot explore: nothing to capture: %d capturable buildings seen (%d not neutral, %d taken/skipped, %d cannot capture [%d special not ready, %d in the shroud], %d unsafe) (frame %u)\n", seen, notNeutral, takenOrSkipped, cannotCapture, notReady, shrouded, unsafe, frame);
			continue;
		}

		SpecialPowerModuleInterface *capture = explorer->findSpecialPowerModuleInterface(SPECIAL_INFANTRY_CAPTURE_BUILDING);
		if (!capture || !capture->getSpecialPowerTemplate())
			continue;
		AIGroupPtr group = TheAI->createGroup();
		if (!group)
			continue;
		group->add(explorer);
		group->groupDoSpecialPowerAtObject(capture->getSpecialPowerTemplate()->getID(), best, 0);
#if RETAIL_COMPATIBLE_AIGROUP
		TheAI->destroyGroup(group);
#else
		group->removeAll();
#endif
		m_explorer[slot] = explorer->getID();
		m_scouting[slot] = FALSE;
		m_target[slot] = best->getID();
		AutopilotLog::line("NETPLAY autopilot explore: %s -> capture %s at (%.0f,%.0f) (frame %u)\n", explorer->getTemplate()->getName().str(),
			best->getTemplate()->getName().str(), best->getPosition()->x, best->getPosition()->y, frame);
	}
}
