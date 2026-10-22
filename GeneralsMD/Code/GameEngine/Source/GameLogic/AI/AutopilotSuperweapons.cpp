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

#include "Common/Energy.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/SpecialPowerType.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"
#include "GameLogic/AutopilotSuperweapons.h"
#include "GameLogic/AutopilotTuning.h"

// The area powers worth aiming at a base. Support powers (paradrops, cash hack, spy satellite, repair, defector...) are left to the player.
// Each general's own variant of a power has its own type, so they are listed separately.
static const SpecialPowerType s_offensivePowers[] =
{
	SPECIAL_PARTICLE_UPLINK_CANNON, SUPW_SPECIAL_PARTICLE_UPLINK_CANNON, LAZR_SPECIAL_PARTICLE_UPLINK_CANNON,
	SPECIAL_NEUTRON_MISSILE, NUKE_SPECIAL_NEUTRON_MISSILE, SUPW_SPECIAL_NEUTRON_MISSILE,
	SPECIAL_SCUD_STORM,
	SPECIAL_DAISY_CUTTER, AIRF_SPECIAL_DAISY_CUTTER,
	SPECIAL_CARPET_BOMB, SPECIAL_CHINA_CARPET_BOMB, EARLY_SPECIAL_CHINA_CARPET_BOMB, AIRF_SPECIAL_CARPET_BOMB,
	SPECIAL_NAPALM_STRIKE, SPECIAL_ANTHRAX_BOMB, SPECIAL_ARTILLERY_BARRAGE,
	SPECIAL_A10_THUNDERBOLT_STRIKE, AIRF_SPECIAL_A10_THUNDERBOLT_STRIKE,
	SPECIAL_SPECTRE_GUNSHIP, AIRF_SPECIAL_SPECTRE_GUNSHIP,
	SUPR_SPECIAL_CRUISE_MISSILE, SPECIAL_BATTLESHIP_BOMBARDMENT,
};

namespace
{
	struct Candidate { Coord3D pos; Real cost; };
	const Int MAX_CANDIDATES = 128;
}

Bool AutopilotSuperweapons::chooseTarget(Real radius, Coord3D *pos, Real *value) const
{
	Player *player = m_host.player();
	const Int idx = player->getPlayerIndex();

	// Structures worth hitting (enemy, and seen at least once) and structures to keep clear of (ours, allies').
	Candidate targets[MAX_CANDIDATES];
	Int numTargets = 0;
	Candidate friends[MAX_CANDIDATES];
	Int numFriends = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (!obj->isKindOf(KINDOF_STRUCTURE) || obj->isEffectivelyDead() || obj->isKindOf(KINDOF_REBUILD_HOLE))
			continue;
		Player *owner = obj->getControllingPlayer();
		if (!owner)
			continue;
		if (owner == player || player->getRelationship(owner->getDefaultTeam()) == ALLIES)
		{
			if (numFriends < MAX_CANDIDATES)
			{
				friends[numFriends].pos = *obj->getPosition();
				friends[numFriends].cost = 0.0f;
				++numFriends;
			}
			continue;
		}
		if (player->getRelationship(owner->getDefaultTeam()) != ENEMIES || numTargets >= MAX_CANDIDATES)
			continue;
		const ObjectShroudStatus shroud = obj->getShroudedStatus(idx);
		if (shroud == OBJECTSHROUD_INVALID || shroud == OBJECTSHROUD_INVALID_BUT_PREVIOUS_VALID || shroud == OBJECTSHROUD_SHROUDED)
			continue;	// never seen: the player knows nothing about it
		Real cost = (Real)obj->getTemplate()->calcCostToBuild(owner);
		if (obj->isKindOf(KINDOF_COMMANDCENTER) || obj->isKindOf(KINDOF_FS_SUPERWEAPON))
			cost /= 10.0f;	// no area power destroys these, so they are poor targets (the engine's own targeting says the same)
		targets[numTargets].pos = *obj->getPosition();
		targets[numTargets].cost = cost;
		++numTargets;
	}

	const Real radSqr = radius * radius;
	Real best = -1.0f;
	Int bestIndex = -1;
	for (Int c = 0; c < numTargets; ++c)
	{
		const Coord3D &center = targets[c].pos;
		Bool clear = TRUE;
		for (Int f = 0; f < numFriends && clear; ++f)
		{
			const Real dx = center.x - friends[f].pos.x, dy = center.y - friends[f].pos.y;
			if (dx * dx + dy * dy < radSqr)
				clear = FALSE;
		}
		if (!clear)
			continue;
		Real sum = 0.0f;
		for (Int t = 0; t < numTargets; ++t)
		{
			const Real dx = center.x - targets[t].pos.x, dy = center.y - targets[t].pos.y;
			const Real distSqr = dx * dx + dy * dy;
			if (distSqr < radSqr)
				sum += targets[t].cost * (1.0f - WWMath::Sqrt(distSqr) / (2.0f * radius));	// 1.0 at the center, 0.5 at the edge
		}
		if (sum > best)
		{
			best = sum;
			bestIndex = c;
		}
	}
	if (bestIndex < 0 || best < AutopilotTuning::SUPERWEAPON_MIN_TARGET_VALUE)
		return FALSE;
	*pos = targets[bestIndex].pos;
	pos->z = TheTerrainLogic->getGroundHeight(pos->x, pos->y);
	*value = best;
	return TRUE;
}

void AutopilotSuperweapons::update()
{
	if (!m_profile.m_superweapons)
		return;
	Player *player = m_host.player();
	const UnsignedInt frame = TheGameLogic->getFrame();

	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != player || obj->isEffectivelyDead() || obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		for (Int i = 0; i < (Int)(sizeof(s_offensivePowers) / sizeof(s_offensivePowers[0])); ++i)
		{
			if (!obj->hasSpecialPower(s_offensivePowers[i]))
				continue;
			SpecialPowerModuleInterface *mod = obj->findSpecialPowerModuleInterface(s_offensivePowers[i]);
			if (!mod || mod->isScriptOnly())
				continue;
			if (obj->isDisabled())
			{
				if (m_chargeTrace.due(frame, 1800))
					AutopilotLog::line("NETPLAY autopilot superweapon: %s is disabled (underpowered %d, emp %d, hacked %d, power ratio %.2f) (frame %u)\n", mod->getPowerName().str(), (Int)obj->isDisabledByType(DISABLED_UNDERPOWERED), (Int)obj->isDisabledByType(DISABLED_EMP), (Int)obj->isDisabledByType(DISABLED_HACKED), player->getEnergy()->getEnergySupplyRatio(), frame);
				continue;
			}
			if (!mod->isReady())
			{
				if (m_chargeTrace.due(frame, 1800))	// every minute of game time
					AutopilotLog::line("NETPLAY autopilot superweapon: %s charging, %d%% (frame %u)\n", mod->getPowerName().str(), (Int)(100.0f * mod->getPercentReady()), frame);
				continue;
			}
			const ScienceType science = mod->getRequiredScience();
			if (science != SCIENCE_INVALID && !player->hasScience(science))
				continue;	// a general's power the player has not bought; buying sciences stays the player's decision
			const SpecialPowerTemplate *power = mod->getSpecialPowerTemplate();
			if (!power)
				continue;
			Real radius = AutopilotTuning::SUPERWEAPON_MIN_RADIUS;
			if (power->getRadiusCursorRadius() > radius)
				radius = power->getRadiusCursorRadius();

			Coord3D target;
			Real value = 0.0f;
			if (!chooseTarget(radius, &target, &value))
			{
				if (m_traceThrottle.due(frame, 600))
					AutopilotLog::line("NETPLAY autopilot superweapon: %s is ready, no target worth it yet (frame %u)\n", mod->getPowerName().str(), frame);
				continue;
			}
			mod->doSpecialPowerAtLocation(&target, INVALID_ANGLE, COMMAND_FIRED_BY_SCRIPT);
			AutopilotLog::line("NETPLAY autopilot superweapon: fired %s at (%.0f,%.0f), target value %.0f (frame %u)\n", mod->getPowerName().str(), target.x, target.y, value, frame);
			AutopilotLog::feed(player, "Firing %s at the enemy base", mod->getPowerName().str());
		}
	}
}
