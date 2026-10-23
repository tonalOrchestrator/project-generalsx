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

// GeneralsX @refactor Claude 21/10/2026 The autopilot (levels 1-5, see AutopilotProfile): a build-only / defend / explore AI that
// drives a HUMAN player's base. It is attached to an AI player through the narrow AutopilotHost interface and owns one module per
// feature, wired together here and nowhere else:
//   AutopilotPlanner   which structure to build next (prerequisites, wallet build-up, stall explanation)
//   AutopilotEconomy   supply gatherers, supply center placement and expansion
//   AutopilotDefense   wallet mode: threat detection, base-defense budget, defender training
//   AutopilotExplorer  explore mode: capture upgrade, explorers, scouting
//   AutopilotSquad     wallet and explore modes: a tank person + a gun person garrisoned in buildings near the base
//   AutopilotHackers   wallet mode (China): hackers holed up in the Internet Center for income
//   AutopilotRepair    wallet and explore modes: an idle dozer repairs the most damaged building
// Applying a different level (setProfile) keeps every module's state; turning the autopilot off deletes this object.

#pragma once

#include "GameLogic/AutopilotDefense.h"
#include "GameLogic/AutopilotDirectives.h"
#include "GameLogic/AutopilotEconomy.h"
#include "GameLogic/AutopilotExplorer.h"
#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotHackers.h"
#include "GameLogic/AutopilotPlanner.h"
#include "GameLogic/AutopilotProfile.h"
#include "GameLogic/AutopilotRepair.h"
#include "GameLogic/AutopilotSquad.h"
#include "GameLogic/AutopilotSuperweapons.h"

class AIAutopilot : private AutopilotPlanner::Links, private AutopilotSquad::Links
{
public:
	AIAutopilot(AutopilotHost &host, AutopilotDriver &driver, const AutopilotProfile &profile);

	/// Switch level in place (the hotkey cycle): the modules read the new profile from now on.
	void setProfile(const AutopilotProfile &profile) { m_profile = profile; }

	/// Once per logic frame, in place of the AI player's own update(): plan the next structure, run the base-building and team passes,
	/// and every two seconds queue trucks, train defenders and send explorers.
	void update();

	/// A directive from MSG_AUTOPILOT_DIRECTIVE (kind AD_*, an object id, how long it holds), applied on the same logic frame on every machine.
	void applyDirective(Int kind, ObjectID id, UnsignedInt durationFrames);

	/// The AI player's base-building pass tried to place 'plan' for 'info' ('built' is null if it failed).
	void onStructureAttempt(const ThingTemplate *plan, BuildListInfo *info, const Object *built);

private:
	// AutopilotPlanner::Links: the planner's view of the sibling modules
	virtual AutopilotDefense::Budget defenseBudget(UnsignedInt bank) override { return m_defense.budget(bank); }
	virtual void setSavingTarget(UnsignedInt target) override { m_defense.setSavingTarget(target); }
	virtual Int desiredGatherers() const override { return m_economy.desiredGatherers(); }
	virtual BuildListInfo *queueSupplyExpansion(UnsignedInt money) override { return m_economy.queueSupplyExpansion(money); }
	virtual void markSupplyEntry(BuildListInfo *info, const ThingTemplate *tmpl) override { m_economy.markSupplyEntry(info, tmpl); }
	virtual Bool findSupplySpot(const ThingTemplate *need, Object *dozer, const Object *warehouse, Coord3D *out) override { return m_economy.findSupplySpot(need, dozer, warehouse, out); }

	// AutopilotSquad::Links: the squad must not touch the explorers
	virtual Bool isExplorer(const Object *unit) const override { return m_explorer.isExplorer(unit); }

	AutopilotHost &m_host;
	AutopilotDriver &m_driver;
	AutopilotProfile m_profile;	// declared before the modules: they hold a reference to it
	AutopilotDirectives m_directives;	// likewise
	AutopilotEconomy m_economy;
	AutopilotDefense m_defense;
	AutopilotExplorer m_explorer;
	AutopilotSquad m_squad;
	AutopilotHackers m_hackers;
	AutopilotRepair m_repair;
	AutopilotSuperweapons m_superweapons;
	AutopilotPlanner m_planner;
};
