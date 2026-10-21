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

// GeneralsX @refactor Claude 21/10/2026 Autopilot structure planner: picks the next structure to build (the skirmish scripts do this for
// computer players), chains in missing prerequisite buildings, runs wallet mode's build-up phase, and explains stalled placements.
// A computer player gets its build order from scripts; a human with autopilot has none, so this walks the faction's build list.
// Owns: the one structure it is waiting on (pending) and the ring position for prerequisite structures.
// Depends on: AutopilotHost, AutopilotProfile, and the sibling features it consults through AutopilotPlanner::Links.

#pragma once

#include "Common/GameCommon.h"
#include "Common/KindOf.h"
#include "GameLogic/AutopilotDefense.h"

class AutopilotHost;
class BuildListInfo;
class Object;
class ThingTemplate;
struct AutopilotProfile;

class AutopilotPlanner
{
public:
	/// What the planner asks of the other autopilot features. The facade (AIAutopilot) wires these to the modules that own them, so the
	/// planner never sees them directly.
	class Links
	{
	public:
		virtual AutopilotDefense::Budget defenseBudget(UnsignedInt bank) = 0;
		virtual Int desiredGatherers() const = 0;
		virtual BuildListInfo *queueSupplyExpansion(UnsignedInt money) = 0;
		virtual void markSupplyEntry(BuildListInfo *info, const ThingTemplate *tmpl) = 0;
		virtual Bool findSupplySpot(const ThingTemplate *need, Object *dozer, const Object *warehouse, Coord3D *out) = 0;

	protected:
		~Links() = default;
	};

	AutopilotPlanner(AutopilotHost &host, const AutopilotProfile &profile, Links &links);

	/// Every frame: follow up on the structure queued earlier, and when the AI's build timer allows, queue the next one.
	void update();

	/// After the base-building pass tried to place 'plan' for 'info' ('built' is null when it failed): the placement trace, and the
	/// stall explanation when it failed for a reason other than money.
	void reportPlacement(const ThingTemplate *plan, BuildListInfo *info, const Object *built);

private:
	/// The facts one planning pass works from.
	struct Pass
	{
		Bool m_wallet;
		Bool m_underPowered;
		Bool m_buildingUp;							///< wallet phase 1: build the foundation first; hoarding starts after it
		UnsignedInt m_money;
		Int m_cap;									///< most of one structure the level builds
		AutopilotDefense::Budget m_defense;
		Int m_defensesBuilt;
	};

	Bool resolvePending();
	void track(BuildListInfo *info);
	Bool queueSupplyCenterForIncome(UnsignedInt money);
	Bool queueFoundation(Pass &pass);
	void queueCheapestCandidate(const Pass &pass);
	Bool queueMissingPrerequisite(const ThingTemplate *plan, UnsignedInt money, Int depth, KindOfType onlyKind = KINDOF_INVALID);
	void explainStall(BuildListInfo *info);

	AutopilotHost &m_host;
	const AutopilotProfile &m_profile;
	Links &m_links;

	BuildListInfo *m_pending;			///< build-list entry the planner queued and is waiting on
	UnsignedInt m_pendingFrame;
	Int m_ring;							///< next slot on the ring around the base for a prerequisite structure
};
