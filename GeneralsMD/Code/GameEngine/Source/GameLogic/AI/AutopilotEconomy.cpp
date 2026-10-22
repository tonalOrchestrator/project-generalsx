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
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/TerrainVisual.h"

#include "GameLogic/AutopilotTuning.h"
#include "GameLogic/AutopilotEconomy.h"
#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"

// GeneralsX @feature Claude 20/10/2026 Autopilot: desired supply gatherers per level (Easy 2, Normal 3, Hard 4).
Int AutopilotEconomy::desiredGatherers() const
{
	const GameDifficulty difficulty = m_profile.m_difficulty;
	return difficulty == DIFFICULTY_EASY ? 2 : (difficulty == DIFFICULTY_NORMAL ? 3 : 4);
}

// Finds a legal spot for a supply center next to 'warehouse': a supply center may not stand too close to a supply source,
// and the trucks only reach a center within SUPPLY_CENTER_CLOSE_DIST (20 cells) of one, so walk from the warehouse toward
// the base and take the first spot the placement code accepts. Returns false (and leaves 'out') if there is none.
Bool AutopilotEconomy::findSupplySpot(const ThingTemplate *need, Object *dozer, const Object *warehouse, Coord3D *out)
{
	Player *player = m_host.player();
	const Coord3D &baseCenter = m_host.baseCenter();
	Coord2D toBase;
	toBase.x = baseCenter.x - warehouse->getPosition()->x;
	toBase.y = baseCenter.y - warehouse->getPosition()->y;
	toBase.normalize();
	const Coord3D warehousePos = *warehouse->getPosition();
	const Real viewAngle = need->getPlacementViewAngle();
	// Closest spots first; at each distance try the side facing the base, then fan out around the warehouse (the straight
	// line to the base is often blocked by the warehouse's own supply piles or by terrain).
	static const Real fan[] = { 0.0f, PI / 4, -PI / 4, PI / 2, -PI / 2, 3 * PI / 4, -3 * PI / 4, PI };
	Bool found = FALSE;
	for (Int cells = 6; cells <= 20 && !found; ++cells)
	{
		for (Int f = 0; f < (Int)(sizeof(fan) / sizeof(fan[0])) && !found; ++f)
		{
			const Real c = WWMath::CosTrig(fan[f]);
			const Real s = WWMath::SinTrig(fan[f]);
			Coord3D candidate = warehousePos;
			candidate.x += (toBase.x * c - toBase.y * s) * cells * PATHFIND_CELL_SIZE_F;
			candidate.y += (toBase.x * s + toBase.y * c) * cells * PATHFIND_CELL_SIZE_F;
			candidate.z = TheTerrainLogic->getGroundHeight(candidate.x, candidate.y);
			if (TheBuildAssistant->isLocationLegalToBuild(&candidate, need, viewAngle, BuildAssistant::NO_ENEMY_OBJECT_OVERLAP, dozer, player) == LBC_OK &&
				TheBuildAssistant->isLocationLegalToBuild(&candidate, need, viewAngle,
					BuildAssistant::CLEAR_PATH | BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, dozer, player) == LBC_OK)
			{
				*out = candidate;
				found = TRUE;
			}
		}
	}
	TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback
	return found;
}

// A second supply center at another warehouse once the first one is running and there is money to spare. The template is
// whatever supply center the player already owns, so this works for every faction.
BuildListInfo *AutopilotEconomy::queueSupplyExpansion(UnsignedInt money)
{
	Player *player = m_host.player();
	const Coord3D &baseCenter = m_host.baseCenter();
	auto decline = [&](const char *reason) -> BuildListInfo * {
		if (AutopilotLog::onFrameMultiple(TheGameLogic->getFrame(), 600))
			AutopilotLog::line("NETPLAY autopilot supply expansion declined: %s (frame %u)\n", reason, TheGameLogic->getFrame());
		return nullptr;
	};
	const Int centerCap = (m_profile.m_wallet || m_profile.m_difficulty == DIFFICULTY_HARD) ? 2 : 1;
	const ThingTemplate *centerTemplate = nullptr;
	Int ownedCenters = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != player || !(obj->isKindOf(KINDOF_FS_SUPPLY_CENTER) || obj->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)))
			continue;
		++ownedCenters;
		if (!obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			centerTemplate = obj->getTemplate();
	}
	if (!centerTemplate)
		return decline("no finished supply center yet");
	if (ownedCenters >= centerCap)
		return decline("at the center cap");
	if (money < centerTemplate->calcCostToBuild(player) + AutopilotTuning::SECOND_CENTER_MARGIN)
		return decline("not enough money");

	// The nearest warehouse (to the base) that still has cash, is not an enemy's and is not one of our centers' reach.
	const Real servedRange = 25 * PATHFIND_CELL_SIZE_F;
	const Object *warehouse = nullptr;
	Real bestDistSqr = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (!obj->isKindOf(KINDOF_STRUCTURE) || !obj->isKindOf(KINDOF_SUPPLY_SOURCE))
			continue;
		static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
		SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*)obj->findUpdateModule( key_warehouseUpdate );
		if (!warehouseModule || warehouseModule->getBoxesStored() * TheGlobalData->m_baseValuePerSupplyBox <= 0)
			continue;
		if (player->getRelationship(obj->getTeam()) == ENEMIES)
			continue;
		Bool served = FALSE;
		for (Object *center = TheGameLogic->getFirstObject(); center && !served; center = center->getNextObject())
			if (center->getControllingPlayer() == player && (center->isKindOf(KINDOF_FS_SUPPLY_CENTER) || center->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)))
			{
				const Real dx = center->getPosition()->x - obj->getPosition()->x;
				const Real dy = center->getPosition()->y - obj->getPosition()->y;
				served = dx * dx + dy * dy < servedRange * servedRange;
			}
		if (served)
			continue;
		const Real bx = obj->getPosition()->x - baseCenter.x;
		const Real by = obj->getPosition()->y - baseCenter.y;
		const Real distSqr = bx * bx + by * by;
		if (!warehouse || distSqr < bestDistSqr)
		{
			warehouse = obj;
			bestDistSqr = distSqr;
		}
	}
	if (!warehouse)
		return decline("no unserved warehouse with cash");

	Object *dozer = m_host.findDozerNear(&baseCenter);
	if (!dozer || TheBuildAssistant->canMakeUnit(dozer, centerTemplate) != CANMAKE_OK)
		return decline("no dozer or cannot build it");
	Coord3D pos;
	if (!findSupplySpot(centerTemplate, dozer, warehouse, &pos))
		return decline("no legal spot near the warehouse");

	player->addToPriorityBuildList(centerTemplate->getName(), &pos, centerTemplate->getPlacementViewAngle());
	BuildListInfo *queued = player->getBuildList();	// the new entry is the list head
	markSupplyEntry(queued, centerTemplate);
	AutopilotLog::line("NETPLAY autopilot supply expansion: %s at (%.0f,%.0f) (frame %u)\n", centerTemplate->getName().str(), pos.x, pos.y, TheGameLogic->getFrame());
	return queued;
}

// GeneralsX @bugfix Claude 20/10/2026 The AI only sends supply trucks to build-list entries flagged as supply buildings with
// a desired gatherer count. An entry autopilot adds on its own has neither, so a supply center it built earned nothing.
void AutopilotEconomy::markSupplyEntry(BuildListInfo *info, const ThingTemplate *tmpl)
{
	if (!info || !tmpl)
		return;
	if (tmpl->isKindOf(KINDOF_FS_SUPPLY_CENTER) || tmpl->isKindOf(KINDOF_FS_SUPPLY_DROPZONE))
	{
		info->setSupplyBuilding(TRUE);
		info->setDesiredGatherers(desiredGatherers());
	}
}
