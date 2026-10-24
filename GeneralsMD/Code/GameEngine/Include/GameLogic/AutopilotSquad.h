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

// GeneralsX @feature Claude 21/10/2026 Autopilot garrison squad (wallet and explore modes): a small standing team of one "tank person" (anti-vehicle
// infantry) and one "gun person" (rifle infantry), alternating, trained at a barracks and sent to hole up in buildings near the base. Maps are
// full of buildings that infantry can garrison, and garrisoned infantry are hard to dislodge. Any other idle combat infantry the autopilot
// trained (the wallet defenders) is garrisoned too. Nothing here ever attacks.
// Owns: which role is trained next, the trace throttle. Depends on: AutopilotHost, AutopilotProfile, AutopilotUnits.

#pragma once

#include "Common/GameCommon.h"
#include "GameLogic/AutopilotLog.h"

class AutopilotHost;
class AutopilotDirectives;
class Object;
struct AutopilotProfile;

class AutopilotSquad
{
public:
	/// What the squad must not touch: the explorers (the facade answers from the explorer module).
	class Links
	{
	public:
		virtual Bool isExplorer(const Object *unit) const = 0;
	protected:
		~Links() = default;
	};

	AutopilotSquad(AutopilotHost &host, const AutopilotProfile &profile, const AutopilotDirectives &directives, Links &links) : m_host(host), m_profile(profile), m_directives(directives), m_links(links) {}

	/// Called every two seconds: top the squad up to the profile's standing size, then garrison the idle ones.
	void update();

private:
	void train();
	void garrison();
	Bool isSquadUnit(const Object *unit) const;

	AutopilotHost &m_host;
	const AutopilotProfile &m_profile;
	const AutopilotDirectives &m_directives;
	Links &m_links;
	Bool m_nextAntiVehicle = TRUE;	///< the next trained member is the tank person (else the gun person)
	AutopilotLog::Throttle m_traceThrottle;
};
