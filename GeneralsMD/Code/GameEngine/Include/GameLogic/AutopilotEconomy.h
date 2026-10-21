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

// GeneralsX @refactor Claude 21/10/2026 Autopilot economy: how many trucks a supply center gets, where a second supply center
// goes, and the flag that makes the AI send trucks to a supply center the autopilot added on its own.
// Owns: nothing persistent (a queued expansion is tracked by the planner). Depends on: AutopilotHost, AutopilotProfile.

#pragma once

#include "Common/GameCommon.h"

class AutopilotHost;
class BuildListInfo;
class Object;
class ThingTemplate;
struct AutopilotProfile;

class AutopilotEconomy
{
public:
	AutopilotEconomy(AutopilotHost &host, const AutopilotProfile &profile) : m_host(host), m_profile(profile) {}

	/// Desired supply gatherers for the current level (Easy 2, Normal 3, Hard 4).
	Int desiredGatherers() const;

	/// Finds a legal spot for a supply center next to 'warehouse' (see the .cpp). Returns false, leaving 'out', if there is none.
	Bool findSupplySpot(const ThingTemplate *need, Object *dozer, const Object *warehouse, Coord3D *out);

	/// A second supply center at another warehouse once the first one is running and there is money to spare. Returns the build-list
	/// entry it queued (the planner then waits for it), or nullptr when it declined.
	BuildListInfo *queueSupplyExpansion(UnsignedInt money);

	/// Flags a supply center entry so the AI sends trucks to it.
	void markSupplyEntry(BuildListInfo *info, const ThingTemplate *tmpl);

private:
	AutopilotHost &m_host;
	const AutopilotProfile &m_profile;
};
