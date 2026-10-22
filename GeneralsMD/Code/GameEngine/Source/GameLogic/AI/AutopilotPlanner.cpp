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
#include "Common/ProductionPrerequisite.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/TerrainVisual.h"

#include "GameLogic/AutopilotTuning.h"
#include "GameLogic/AutopilotUnits.h"
#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotPlanner.h"
#include "GameLogic/AutopilotProfile.h"

AutopilotPlanner::AutopilotPlanner(AutopilotHost &host, const AutopilotProfile &profile, Links &links)
	: m_host(host), m_profile(profile), m_links(links), m_pending(nullptr), m_pendingFrame(0), m_ring(0)
{
}

// GeneralsX @feature Claude 20/10/2026 Autopilot: toggle build-only skirmish AI for a human player.
// A computer player gets its build order from the skirmish scripts, which queue structures one at a time through
// buildSpecificAIBuilding(). A human with autopilot has no such scripts, so this picks the next structure itself:
// walk the faction's build list in order and queue the first one that is missing, allowed and affordable right now.
// The autopilot level (the AI skill state) caps how many of each structure it will build; power plants are added
// only when the base is short of power. The host's base-building pass then places it with a dozer like any other build.
void AutopilotPlanner::update()
{
	if (!resolvePending())
		return;
	if (!m_host.structureSlotReady())
		return;

	Player *player = m_host.player();
	for (BuildListInfo *supplyInfo = player->getBuildList(); supplyInfo; supplyInfo = supplyInfo->getNext())
		if (supplyInfo->isSupplyBuilding())
			supplyInfo->setDesiredGatherers(m_links.desiredGatherers());	// follows the level if it changes

	// GeneralsX @feature Claude 20/10/2026 Autopilot wallet mode: defend, survive, hoard money.
	// Wallet mode builds only the economy (supply, cash generators, power) and a few base defenses, and never spends
	// into the reserve on defenses, so the rest of the money stays in the bank.
	Pass pass;
	pass.m_wallet = m_profile.m_wallet;
	const GameDifficulty difficulty = m_profile.m_difficulty;
	pass.m_cap = pass.m_wallet ? 1 : ((difficulty == DIFFICULTY_EASY) ? 1 : ((difficulty == DIFFICULTY_NORMAL) ? 2 : 3));
	pass.m_money = player->getMoney()->countMoney();
	pass.m_defense = m_links.defenseBudget(pass.m_money);	// under threat (enemy units near the base) the bank is there to be spent
	pass.m_defensesBuilt = 0;
	if (pass.m_wallet)
		for (BuildListInfo *d = player->getBuildList(); d; d = d->getNext())
		{
			const ThingTemplate *dt = TheThingFactory->findTemplate(d->getTemplateName());
			if (dt && dt->isKindOf(KINDOF_FS_BASE_DEFENSE) && TheGameLogic->findObjectByID(d->getObjectID()))
				++pass.m_defensesBuilt;
		}
	pass.m_underPowered = !player->getEnergy()->hasSufficientPower();
	if (m_profile.m_superweapons && !pass.m_underPowered && player->getEnergy()->getEnergySupplyRatio() < AutopilotTuning::SUPERWEAPON_POWER_HEADROOM)
	{
		// GeneralsX @feature Claude 22/10/2026 A superweapon is disabled the moment the base is short of power, and a plant at a time never catches up:
		// with one standing, keep some power in hand.
		for (Object *obj = TheGameLogic->getFirstObject(); obj && !pass.m_underPowered; obj = obj->getNextObject())
			if (obj->getControllingPlayer() == player && obj->isKindOf(KINDOF_FS_SUPERWEAPON) && obj->isKindOf(KINDOF_STRUCTURE) && !obj->isEffectivelyDead())
				pass.m_underPowered = TRUE;
	}
	pass.m_buildingUp = FALSE;

	if (pass.m_wallet)
	{
		if (queueSupplyCenterForIncome(pass.m_money))
			return;
		if (queueFoundation(pass))
			return;
	}

	if (!pass.m_buildingUp)
	{
		if (BuildListInfo *expansion = m_links.queueSupplyExpansion(pass.m_money))
		{
			track(expansion);
			return;
		}
	}

	if (m_profile.m_superweapons && !pass.m_buildingUp)
	{
		if (queueSuperweaponStructure(pass))
			return;
	}
	else if (m_saving)
	{
		m_saving = FALSE;	// the module was switched off, or the foundation needs building again
		m_links.setSavingTarget(0);
	}

	if (pass.m_wallet && !pass.m_buildingUp && queueExtraDefense(pass))
		return;

	queueCheapestCandidate(pass);
}

// GeneralsX @feature Claude 22/10/2026 Most factions' build lists hold no superweapon structure (or only one the planner never reaches while it is spending on
// defenses), so a hoarder adds its own: the cheapest superweapon structure the dozer can build, once the bank covers its cost plus the defense fund and a
// cushion, or the prerequisite building it is waiting for. One at a time, up to the per-kind cap; the game's own limit still applies.
Bool AutopilotPlanner::queueSuperweaponStructure(const Pass &pass)
{
	Player *player = m_host.player();
	const UnsignedInt frame = TheGameLogic->getFrame();
	const Coord3D &baseCenter = m_host.baseCenter();
	Object *dozer = m_host.findDozerNear(&baseCenter);
	if (!dozer)
		return FALSE;

	// The cheapest superweapon structure this side can build (now, or once a prerequisite stands), whatever the bank holds.
	const ThingTemplate *pick = nullptr;
	UnsignedInt pickCost = 0;
	const ThingTemplate *blocked = nullptr;
	UnsignedInt blockedCost = 0;
	for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
	{
		if (!t->isKindOf(KINDOF_FS_SUPERWEAPON) || !t->isKindOf(KINDOF_STRUCTURE) || !AutopilotUnits::isSuperweaponStructure(t))
			continue;
		const UnsignedInt cost = t->calcCostToBuild(player);
		if (cost == 0)
			continue;
		const CanMakeType canMake = TheBuildAssistant->canMakeUnit(dozer, t);
		if (canMake == CANMAKE_OK)
		{
			if (!pick || cost < pickCost)
			{
				pick = t;
				pickCost = cost;
			}
		}
		else if (canMake == CANMAKE_NO_PREREQ && t->getDefaultOwningSide() == player->getSide() && (!blocked || cost < blockedCost))
		{
			blocked = t;
			blockedCost = cost;
		}
	}

	if (!pick && !blocked)
		return FALSE;
	if (pass.m_underPowered)
		return FALSE;	// a superweapon is disabled without power: the power plants (the candidate pass) come first, and no further one is added until the base has enough
	const ThingTemplate *want = pick ? pick : blocked;

	// How many of it the player owns or already has queued.
	Int have = 0;
	Bool allReady = TRUE;	// every one owned so far is finished and not disabled
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == player && obj->getTemplate()->isEquivalentTo(want))
		{
			++have;
			if (obj->isDisabled() || obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
				allReady = FALSE;
		}
	if (have > 0 && !allReady)
		return FALSE;	// the next one waits until the last is up and powered (three at once starved the base of power and every one was disabled)
	for (BuildListInfo *info = player->getBuildList(); info; info = info->getNext())
		if (info->getTemplateName() == want->getName() && info->getObjectID() == INVALID_ID && info->isPriorityBuild())
			return FALSE;	// one is already waiting for a dozer
	if (have >= AutopilotTuning::SUPERWEAPON_STRUCTURE_CAP)
	{
		m_saving = FALSE;
		return FALSE;
	}

	// A structure that waits on a prerequisite building (a strategy center, say) needs the bank to cover that too.
	const UnsignedInt price = (pick ? pickCost : blockedCost) + AutopilotTuning::SUPERWEAPON_BUILD_MARGIN + (pick ? 0 : AutopilotTuning::SUPERWEAPON_PREREQ_ALLOWANCE);
	if (pass.m_money < price)
	{
		// Save for it: once a few defenses stand, no further defenses (and fewer units) until the price is in the bank. A target that stays out of
		// reach for too long is given up for a while so the base is not starved.
		if (m_saving && frame - m_savingSinceFrame > (UnsignedInt)(AutopilotTuning::SUPERWEAPON_SAVE_MAX_SECONDS * LOGICFRAMES_PER_SECOND))
		{
			m_saving = FALSE;
			m_savingCooldownUntil = frame + AutopilotTuning::SUPERWEAPON_SAVE_COOLDOWN_SECONDS * LOGICFRAMES_PER_SECOND;
			AutopilotLog::line("NETPLAY autopilot superweapon saving gave up: %s costs %u, bank %u (frame %u)\n", want->getName().str(), price, pass.m_money, frame);
		}
		else if (!m_saving && frame >= m_savingCooldownUntil && pass.m_defensesBuilt >= AutopilotTuning::SUPERWEAPON_SAVE_AFTER_DEFENSES)
		{
			m_saving = TRUE;
			m_savingSinceFrame = frame;
			AutopilotLog::line("NETPLAY autopilot superweapon saving for %s: price %u, bank %u (frame %u)\n", want->getName().str(), price, pass.m_money, frame);
			AutopilotLog::feed(player, "Saving up for %s", AutopilotLog::niceName(want).str());
		}
		m_links.setSavingTarget(m_saving ? price : 0);
		return FALSE;
	}

	m_saving = FALSE;
	m_links.setSavingTarget(0);
	if (!pick)
		return queueMissingPrerequisite(blocked, pass.m_money, 0);
	const Real angle = (Real)(m_ring++ % 8) * (PI / 4);
	const Real radius = m_host.baseRadius() * 0.5f;	// well inside the defenses
	Coord3D pos = baseCenter;
	pos.x += WWMath::CosTrig(angle) * radius;
	pos.y += WWMath::SinTrig(angle) * radius;
	player->addToPriorityBuildList(pick->getName(), &pos, pick->getPlacementViewAngle());
	track(player->getBuildList());	// the new entry is the list head
	AutopilotLog::line("NETPLAY autopilot superweapon structure: %s at (%.0f,%.0f), %d owned (frame %u)\n", pick->getName().str(), pos.x, pos.y, have, frame);
	AutopilotLog::feed(player, "Building a superweapon: %s", AutopilotLog::niceName(pick).str());
	return TRUE;
}

// GeneralsX @feature Claude 21/10/2026 The faction's build list holds only one or two base defenses, so a wallet player under attack (or sitting on a
// big surplus) was left with one defense and thousands unspent. Add further defenses on a ring around the base: rotate through the ones the
// dozer can build (so ground and air are both covered), skip a kind already at its cap, and spend only above the budget's reserve.
Bool AutopilotPlanner::queueExtraDefense(const Pass &pass)
{
	const Bool threatened = pass.m_defense.m_threatened;
	if (m_saving)
		return FALSE;	// the bank is being saved for a superweapon
	const UnsignedInt reserve = threatened ? pass.m_defense.m_reserve : AutopilotTuning::QUIET_EXTRA_DEFENSE_RESERVE;	// a quiet player only spends a real surplus
	const Bool trace = threatened && m_extraDefenseTrace.due(TheGameLogic->getFrame(), 600);	// every 20 s of game time while under attack
	if (pass.m_defensesBuilt >= pass.m_defense.m_totalCap || (!threatened && pass.m_money < AutopilotTuning::QUIET_EXTRA_DEFENSE_MIN_BANK))
	{
		if (trace)
			AutopilotLog::line("NETPLAY autopilot extra defense declined: %d defenses at the cap of %d, bank %u (frame %u)\n", pass.m_defensesBuilt, pass.m_defense.m_totalCap, pass.m_money, TheGameLogic->getFrame());
		return FALSE;
	}

	Player *player = m_host.player();
	const Coord3D &baseCenter = m_host.baseCenter();
	Object *dozer = m_host.findDozerNear(&baseCenter);
	if (!dozer)
	{
		m_host.requestDozer();
		return FALSE;
	}

	// The defenses the dozer can build now and afford, minus kinds already at the per-kind cap.
	const ThingTemplate *candidates[16];
	Int numCandidates = 0;
	Int defenseKinds = 0, tooDear = 0, cannotMake = 0, atKindCap = 0;	// for the trace
	for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t && numCandidates < 16; t = t->friend_getNextTemplate())
	{
		if (!t->isKindOf(KINDOF_FS_BASE_DEFENSE) || t->isKindOf(KINDOF_FS_FAKE))
			continue;
		++defenseKinds;
		const UnsignedInt cost = t->calcCostToBuild(player);
		if (cost == 0 || cost + reserve > pass.m_money)
		{
			++tooDear;
			continue;
		}
		if (TheBuildAssistant->canMakeUnit(dozer, t) != CANMAKE_OK)
		{
			++cannotMake;
			continue;
		}
		Int have = 0;
		for (BuildListInfo *info = player->getBuildList(); info; info = info->getNext())
			if (info->getTemplateName() == t->getName() && TheGameLogic->findObjectByID(info->getObjectID()))
				++have;
		if (have >= pass.m_defense.m_perTemplateCap)
		{
			++atKindCap;
			continue;
		}
		candidates[numCandidates++] = t;
	}
	if (numCandidates == 0)
	{
		if (trace)
			AutopilotLog::line("NETPLAY autopilot extra defense declined: %d defense kinds (%d too dear for a bank of %u, %d the dozer cannot build now, %d at the per-kind cap) (frame %u)\n", defenseKinds, tooDear, pass.m_money, cannotMake, atKindCap, TheGameLogic->getFrame());
		return FALSE;
	}

	const ThingTemplate *pick = candidates[m_defenseRotation++ % numCandidates];
	const Int slot = m_ring++;
	const Real angle = (Real)(slot % 8) * (PI / 4);
	const Real radius = m_host.baseRadius() * AutopilotTuning::DEFENSE_RING_RADIUS_FACTOR + AutopilotTuning::DEFENSE_RING_RADIUS_BASE + (Real)((slot / 8) % 3) * AutopilotTuning::DEFENSE_RING_RADIUS_STEP;	// ring outwards as the inner positions fill
	Coord3D pos = baseCenter;
	pos.x += WWMath::CosTrig(angle) * radius;
	pos.y += WWMath::SinTrig(angle) * radius;
	player->addToPriorityBuildList(pick->getName(), &pos, pick->getPlacementViewAngle());
	track(player->getBuildList());	// the new entry is the list head
	AutopilotLog::line("NETPLAY autopilot extra defense: %s at (%.0f,%.0f), %d defenses so far (frame %u)\n", pick->getName().str(), pos.x, pos.y, pass.m_defensesBuilt, TheGameLogic->getFrame());
	AutopilotLog::feed(m_host.player(), "Building an extra defense: %s", AutopilotLog::niceName(pick).str());
	return TRUE;
}

// Follows up on the structure queued earlier. Returns false while the planner must keep waiting for it.
Bool AutopilotPlanner::resolvePending()
{
	if (!m_pending)
		return TRUE;
	if (m_pending->getObjectID() != INVALID_ID)
		m_pending = nullptr;	// placed; the host's base-building pass owns it from here
	else if (TheGameLogic->getFrame() - m_pendingFrame < 40 * LOGICFRAMES_PER_SECOND)
		return FALSE;	// still waiting for a dozer to start it
	else
	{
		// Gave up on it (no legal spot, no money, no dozer): drop the entry for good and plan something else.
		explainStall(m_pending);
		m_pending->clearPriorityBuild();
		m_pending->setNumRebuilds(0);
		m_pending = nullptr;
	}
	return TRUE;
}

// Waits for 'info' (just queued) to be placed, and lets the base-building pass look at the build list right away.
void AutopilotPlanner::track(BuildListInfo *info)
{
	m_pending = info;
	m_pendingFrame = TheGameLogic->getFrame();
	m_host.kickBuildTimer();
}

// The supply center is not in the faction's build list; production buildings that need it are skipped in wallet
// mode, so nothing would ever pull it in. Without income there is nothing to hoard: queue it through the
// first list entry that lists it as a prerequisite. Returns true if one was queued.
Bool AutopilotPlanner::queueSupplyCenterForIncome(UnsignedInt money)
{
	Player *player = m_host.player();
	Bool hasSupply = FALSE;
	for (Object *obj = TheGameLogic->getFirstObject(); obj && !hasSupply; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == player && (obj->isKindOf(KINDOF_FS_SUPPLY_CENTER) || obj->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)))
			hasSupply = TRUE;
	if (hasSupply)
		return FALSE;
	for (BuildListInfo *info = player->getBuildList(); info; info = info->getNext())
	{
		const ThingTemplate *dependent = TheThingFactory->findTemplate(info->getTemplateName());
		if (dependent && (queueMissingPrerequisite(dependent, money, 0, KINDOF_FS_SUPPLY_CENTER)
			|| queueMissingPrerequisite(dependent, money, 0, KINDOF_FS_SUPPLY_DROPZONE)))
			return TRUE;
	}
	return FALSE;
}

// Hoarding only pays off once the foundation is up: until the faction's advanced tech building (strategy
// center, propaganda center, palace) exists, spend on the way to it, prerequisites and power included.
// Sets pass.m_buildingUp. Returns true if it queued something (or a dozer) and this pass is over.
Bool AutopilotPlanner::queueFoundation(Pass &pass)
{
	Player *player = m_host.player();
	Bool hasAdvanced = FALSE;
	for (Object *obj = TheGameLogic->getFirstObject(); obj && !hasAdvanced; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == player && obj->isKindOf(KINDOF_FS_ADVANCED_TECH))
			hasAdvanced = TRUE;
	BuildListInfo *advancedEntry = nullptr;
	if (!hasAdvanced)
		for (BuildListInfo *info = player->getBuildList(); info && !advancedEntry; info = info->getNext())
		{
			const ThingTemplate *at = TheThingFactory->findTemplate(info->getTemplateName());
			if (at && at->isKindOf(KINDOF_FS_ADVANCED_TECH) && info->isBuildable() && !TheGameLogic->findObjectByID(info->getObjectID()))
				advancedEntry = info;
		}
	pass.m_buildingUp = (advancedEntry != nullptr) && !pass.m_defense.m_threatened;	// an attack ends the build-up: defend first
	if (pass.m_buildingUp && !pass.m_underPowered)
	{
		const ThingTemplate *advanced = TheThingFactory->findTemplate(advancedEntry->getTemplateName());
		if (Object *advDozer = m_host.findDozerNear(advancedEntry->getLocation()))
		{
			const CanMakeType advCanMake = TheBuildAssistant->canMakeUnit(advDozer, advanced);
			if (advCanMake == CANMAKE_OK)
			{
				if (m_host.isSafeToBuildAt(advancedEntry->getLocation(), advanced))
				{
					advancedEntry->markPriorityBuild();
					track(advancedEntry);
					AutopilotLog::line("NETPLAY autopilot wallet foundation: %s (frame %u)\n", advanced->getName().str(), TheGameLogic->getFrame());
					return TRUE;
				}
			}
			else if (advCanMake == CANMAKE_NO_PREREQ && queueMissingPrerequisite(advanced, pass.m_money, 0))
				return TRUE;
		}
		else
		{
			m_host.requestDozer();
			return TRUE;
		}
	}
	return FALSE;
}

// GeneralsX @feature Claude 22/10/2026 The build list holds a fixed number of power plants, and once they all stand a base that is still short of power (a
// superweapon draws a lot, and a disabled one never charges) would wait for nothing. Add a plant of the faction's own kind on the ring around the base.
Bool AutopilotPlanner::queueExtraPowerPlant(UnsignedInt money)
{
	Player *player = m_host.player();
	for (BuildListInfo *info = player->getBuildList(); info; info = info->getNext())
		if (info->getObjectID() == INVALID_ID && info->isPriorityBuild())
			return FALSE;	// something is already waiting for a dozer
	const Coord3D &baseCenter = m_host.baseCenter();
	Object *dozer = m_host.findDozerNear(&baseCenter);
	if (!dozer)
		return FALSE;
	const ThingTemplate *pick = nullptr;
	UnsignedInt pickCost = 0;
	for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
	{
		if (!t->isKindOf(KINDOF_FS_POWER) || t->isKindOf(KINDOF_CASH_GENERATOR) || !t->isKindOf(KINDOF_STRUCTURE) || t->getDefaultOwningSide() != player->getSide())
			continue;
		const UnsignedInt cost = t->calcCostToBuild(player);
		if (cost == 0 || cost > money || TheBuildAssistant->canMakeUnit(dozer, t) != CANMAKE_OK)
			continue;
		if (!pick || cost < pickCost)
		{
			pick = t;
			pickCost = cost;
		}
	}
	if (!pick)
		return FALSE;
	const Real angle = (Real)(m_ring++ % 8) * (PI / 4);
	const Real radius = m_host.baseRadius() * 0.6f;
	Coord3D pos = baseCenter;
	pos.x += WWMath::CosTrig(angle) * radius;
	pos.y += WWMath::SinTrig(angle) * radius;
	player->addToPriorityBuildList(pick->getName(), &pos, pick->getPlacementViewAngle());
	track(player->getBuildList());
	AutopilotLog::line("NETPLAY autopilot extra power plant: %s at (%.0f,%.0f), bank %u (frame %u)\n", pick->getName().str(), pos.x, pos.y, money, TheGameLogic->getFrame());
	AutopilotLog::feed(player, "Extra power plant: %s", AutopilotLog::niceName(pick).str());
	return TRUE;
}

// Candidates: entries that are missing, allowed by the level cap and the power rule. Cheapest first, so the basic
// economy and production come before tech and superweapons (which are first in some factions' lists).
void AutopilotPlanner::queueCheapestCandidate(const Pass &pass)
{
	Player *player = m_host.player();
	const Bool wallet = pass.m_wallet;
	const Bool buildingUp = pass.m_buildingUp;
	const Bool threatened = pass.m_defense.m_threatened;
	const UnsignedInt money = pass.m_money;
	const UnsignedInt walletReserve = pass.m_defense.m_reserve;

	BuildListInfo *pick = nullptr;
	UnsignedInt pickCost = 0;
	BuildListInfo *blocked = nullptr;	// cheapest candidate that is only missing a prerequisite building
	UnsignedInt blockedCost = 0;
	Object *dozer = nullptr;

	// GeneralsX @feature Claude 20/10/2026 Count the structures the player really owns, so ones built by hand count towards the
	// per-level caps (the planner used to see only its own build-list entries and would build a duplicate).
	AsciiString ownedNames[96];
	Int ownedCounts[96];
	Int numOwned = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != player || !obj->isKindOf(KINDOF_STRUCTURE))
			continue;
		const AsciiString tmplName = obj->getTemplate()->getName();
		Int slot = 0;
		while (slot < numOwned && !(ownedNames[slot] == tmplName))
			++slot;
		if (slot == numOwned)
		{
			if (numOwned >= 96)
				continue;
			ownedNames[numOwned] = tmplName;
			ownedCounts[numOwned] = 0;
			++numOwned;
		}
		++ownedCounts[slot];
	}

	for (BuildListInfo *info = player->getBuildList(); info; info = info->getNext())
	{
		const AsciiString name = info->getTemplateName();
		if (name.isEmpty() || !info->isBuildable())
			continue;
		const ThingTemplate *plan = TheThingFactory->findTemplate(name);
		if (!plan)
			continue;
		const Bool isDefense = plan->isKindOf(KINDOF_FS_BASE_DEFENSE);
		if (isDefense && (!wallet || buildingUp || m_saving))
			continue;	// wallet defenses start after the foundation is up (and wait while the bank is saved for a superweapon)
		if (TheGameLogic->findObjectByID(info->getObjectID()))
			continue;	// this entry is built

		const Bool isPower = plan->isKindOf(KINDOF_FS_POWER) && !plan->isKindOf(KINDOF_CASH_GENERATOR);
		if (isPower != pass.m_underPowered)
			continue;	// short of power: only power plants; otherwise no extra ones
		const Bool isEconomy = plan->isKindOf(KINDOF_FS_SUPPLY_CENTER) || plan->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)
			|| plan->isKindOf(KINDOF_CASH_GENERATOR) || (m_profile.m_hackers && plan->isKindOf(KINDOF_FS_INTERNET_CENTER))
			|| ((m_profile.m_hackers || m_profile.m_standingSquad > 0 || m_profile.m_defenseUnits) && plan->isKindOf(KINDOF_FS_BARRACKS))	// where hackers and the squad are trained
			|| (m_profile.m_defenseUnits && plan->isKindOf(KINDOF_FS_WARFACTORY));	// and the standing army's tanks
		// GeneralsX @feature Claude 22/10/2026 A player with the superweapons module hoards them: once the foundation is up and the bank has a comfortable
		// surplus (and no attack is going on) the faction's superweapon structure is built, up to the game's own limit.
		const Bool isSuperweapon = m_profile.m_superweapons && plan->isKindOf(KINDOF_FS_SUPERWEAPON);
		if (wallet && !isPower && !isEconomy && !isDefense && !isSuperweapon)
			continue;	// wallet mode: no production or tech buildings (superweapons only with that module)
		if (isSuperweapon && buildingUp)
			continue;
		if (wallet && isDefense && pass.m_defensesBuilt >= pass.m_defense.m_totalCap)
			continue;

		Int have = 0;	// what the player owns of this structure, whoever built it
		for (Int o = 0; o < numOwned; ++o)
			if (ownedNames[o] == name)
				have = ownedCounts[o];
		if (!isPower && have >= (isDefense ? pass.m_defense.m_perTemplateCap : (isSuperweapon ? AutopilotTuning::SUPERWEAPON_STRUCTURE_CAP : pass.m_cap)))
			continue;

		if (!dozer)
			dozer = m_host.findDozerNear(info->getLocation());
		if (!dozer)
		{
			m_host.requestDozer();
			return;
		}
		const UnsignedInt cost = plan->calcCostToBuild(player);
		if (wallet && isDefense && money < cost + walletReserve)
			continue;	// hoarding: keep the reserve
		if (isSuperweapon && money < cost + AutopilotTuning::SUPERWEAPON_BUILD_MARGIN)
			continue;	// the defense fund and a cushion stay in the bank
		const CanMakeType canMake = TheBuildAssistant->canMakeUnit(dozer, plan);
		if (canMake == CANMAKE_OK)
		{
			if (m_host.isSafeToBuildAt(info->getLocation(), plan) && (!pick || cost < pickCost))
			{
				pick = info;
				pickCost = cost;
			}
		}
		// Wallet defenses that need a prerequisite building (a barracks for the GLA tunnel network, say) are only worth the
		// extra spend once the bank is comfortably full.
		else if (canMake == CANMAKE_NO_PREREQ && !(wallet && isDefense && money < cost + walletReserve + (threatened ? 0 : AutopilotTuning::QUIET_PREREQ_DEFENSE_MARGIN)) && (!blocked || cost < blockedCost))
		{
			blocked = info;
			blockedCost = cost;
		}
	}
	if (!pick && blocked)
	{
		// Nothing is buildable yet: queue the first missing prerequisite building of the cheapest blocked candidate.
		const ThingTemplate *plan = TheThingFactory->findTemplate(blocked->getTemplateName());
		if (plan && queueMissingPrerequisite(plan, money, 0))
			return;
	}

	if (!pick && !blocked && pass.m_underPowered && m_profile.m_superweapons && queueExtraPowerPlant(money))
		return;

	if (pick)
	{
		pick->markPriorityBuild();
		track(pick);
		AutopilotLog::line("NETPLAY autopilot queues %s cost=%u money=%u (frame %u)\n", pick->getTemplateName().str(), pickCost, money, TheGameLogic->getFrame());
		AutopilotLog::feed(m_host.player(), "Next structure: %s ($%u)", AutopilotLog::niceName(TheThingFactory->findTemplate(pick->getTemplateName())).str(), pickCost);
	}
}

// Queues the first prerequisite structure of 'plan' that the player lacks. Returns true if one was queued.
Bool AutopilotPlanner::queueMissingPrerequisite(const ThingTemplate *plan, UnsignedInt money, Int depth, KindOfType onlyKind)
{
	if (depth > 4)
		return FALSE;
	Player *player = m_host.player();
	const Coord3D &baseCenter = m_host.baseCenter();
	Object *dozer = m_host.findDozerNear(&baseCenter);
	if (!dozer)
		return FALSE;

	for (Int i = 0; i < plan->getPrereqCount(); ++i)
	{
		const ProductionPrerequisite *prereq = plan->getNthPrereq(i);
		if (prereq->isSatisfied(player))
			continue;
		for (Int j = 0; j < prereq->getUnitPrereqCount(); ++j)
		{
			const ThingTemplate *need = prereq->getUnitPrereqTemplate(j);
			if (!need || !need->isKindOf(KINDOF_STRUCTURE))
				continue;
			if (onlyKind != KINDOF_INVALID && !need->isKindOf(onlyKind))
				continue;

			// Already owned, or already queued/under way (any build-list entry for it that has not failed)?
			Bool have = false;
			for (Object *obj = TheGameLogic->getFirstObject(); obj && !have; obj = obj->getNextObject())
				if (obj->getControllingPlayer() == player && obj->getTemplate()->isEquivalentTo(need))
					have = true;
			for (BuildListInfo *info = player->getBuildList(); info && !have; info = info->getNext())
				if (info->getTemplateName() == need->getName() && info->isPriorityBuild() && info->getObjectID() == INVALID_ID)
					have = true;	// already queued and waiting for a dozer
			if (have)
				continue;

			// The prerequisite may itself be blocked (a supply center needs a power plant, say): go down a level first.
			if (TheBuildAssistant->canMakeUnit(dozer, need) == CANMAKE_NO_PREREQ)
			{
				if (queueMissingPrerequisite(need, money, depth + 1))
					return TRUE;
				continue;
			}
			if (money < need->calcCostToBuild(player))
				return FALSE;	// cannot afford it yet; wait for money instead of queuing it

			// Prefer an unbuilt entry the faction's build list already has for it (the AI's own layout position).
			BuildListInfo *existing = nullptr;
			for (BuildListInfo *info = player->getBuildList(); info && !existing; info = info->getNext())
				if (info->getTemplateName() == need->getName() && info->isBuildable() && !info->isPriorityBuild() &&
					!TheGameLogic->findObjectByID(info->getObjectID()))
					existing = info;
			if (existing)
			{
				existing->markPriorityBuild();
				m_links.markSupplyEntry(existing, need);
				track(existing);
				AutopilotLog::line("NETPLAY autopilot queues prerequisite %s for %s (list entry, frame %u)\n", need->getName().str(), plan->getName().str(), TheGameLogic->getFrame());
				return TRUE;
			}

			// Somewhere on the ring around the base; the dozer placement search adjusts it to a legal spot.
			const Real angle = (Real)(m_ring++ % 8) * (PI / 4);
			Coord3D pos = baseCenter;
			pos.x += WWMath::CosTrig(angle) * (m_host.baseRadius() * 0.8f + 60.0f);
			pos.y += WWMath::SinTrig(angle) * (m_host.baseRadius() * 0.8f + 60.0f);
			if (need->isKindOf(KINDOF_FS_SUPPLY_CENTER) || need->isKindOf(KINDOF_FS_SUPPLY_DROPZONE))
			{
				// A supply center is only useful next to supplies (the trucks ignore one with none in reach): put it by the
				// nearest warehouse that still has cash, on the side facing the base, as the AI's own supply placement does.
				if (const Object *warehouse = m_host.findSupplyWarehouse(1))
					m_links.findSupplySpot(need, dozer, warehouse, &pos);
			}
			player->addToPriorityBuildList(need->getName(), &pos, need->getPlacementViewAngle());
			BuildListInfo *queued = player->getBuildList();	// the new entry is the list head
			m_links.markSupplyEntry(queued, need);
			track(queued);
			AutopilotLog::line("NETPLAY autopilot queues prerequisite %s for %s (frame %u)\n", need->getName().str(), plan->getName().str(), TheGameLogic->getFrame());
			return TRUE;
		}
	}
	return FALSE;
}

// GeneralsX @feature Claude 20/10/2026 Autopilot: say why a queued structure was never placed, so a stalled build
// does not need a debugger session: the dozer, the money, the safety check and both legality checks the placement
// code runs.
void AutopilotPlanner::explainStall(BuildListInfo *info)
{
	if (!AutopilotLog::enabled())
		return;
	Player *player = m_host.player();
	const ThingTemplate *plan = TheThingFactory->findTemplate(info->getTemplateName());
	if (!plan)
		return;
	Object *dozer = m_host.findDozerNear(info->getLocation());
	Coord3D pos = *info->getLocation();
	pos.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
	Int canMake = -1, legalEnemy = -1, legalTerrain = -1;
	if (dozer)
	{
		canMake = (Int)TheBuildAssistant->canMakeUnit(dozer, plan);
		legalEnemy = (Int)TheBuildAssistant->isLocationLegalToBuild(&pos, plan, info->getAngle(), BuildAssistant::NO_ENEMY_OBJECT_OVERLAP, dozer, player);
		legalTerrain = (Int)TheBuildAssistant->isLocationLegalToBuild(&pos, plan, info->getAngle(),
			BuildAssistant::CLEAR_PATH | BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, dozer, player);
		TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback
	}
	AutopilotLog::line("NETPLAY autopilot stall %s at (%.0f,%.0f): dozer=%d canMake=%d money=%u cost=%u safe=%d legalEnemy=%d legalTerrain=%d ready=%d (frame %u)\n",
		info->getTemplateName().str(), pos.x, pos.y, dozer ? (Int)dozer->getID() : -1, canMake,
		player->getMoney()->countMoney(), plan->calcCostToBuild(player), (Int)m_host.isSafeToBuildAt(info->getLocation(), plan),
		legalEnemy, legalTerrain, (Int)m_host.structureSlotReady(), TheGameLogic->getFrame());
	AutopilotLog::line("NETPLAY autopilot stall entry: priority=%d objectID=%d timestamp=%d rebuilds=%d buildable=%d canBuildBase=%d buildDelay=%d structureTimer=%d underPowered=%d\n",
		(Int)info->isPriorityBuild(), (Int)info->getObjectID(), (Int)info->getObjectTimestamp(), (Int)info->getNumRebuilds(), (Int)info->isBuildable(),
		(Int)player->getCanBuildBase(), (Int)m_host.buildDelayFrames(), (Int)m_host.structureTimerFrames(), (Int)!player->getEnergy()->hasSufficientPower());
}

// After the host's base-building pass tried to place 'plan': waiting for money is normal (logged sparsely); any other failure
// gets the stall explanation.
void AutopilotPlanner::reportPlacement(const ThingTemplate *plan, BuildListInfo *info, const Object *built)
{
	if (!AutopilotLog::enabled())
		return;
	Player *player = m_host.player();
	const Bool brokeFor = !built && player->getMoney()->countMoney() < plan->calcCostToBuild(player);
	if (built || !brokeFor || AutopilotLog::onFrameMultiple(TheGameLogic->getFrame(), 600))
		AutopilotLog::line("NETPLAY autopilot place %s priority=%d -> %s (frame %u)\n", plan->getName().str(), (Int)info->isPriorityBuild(), built ? "placed" : (brokeFor ? "waiting for money" : "FAILED"), TheGameLogic->getFrame());
	if (!built && !brokeFor)
		explainStall(info);
}
