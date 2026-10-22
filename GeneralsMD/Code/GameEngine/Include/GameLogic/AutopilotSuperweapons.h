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

// GeneralsX @feature Claude 22/10/2026 Autopilot superweapons (wallet and fortress): the player's identity is to defend, hoard cash and hoard superweapons;
// the one offensive act it takes is firing them. Every two seconds each ready area power (Particle Uplink Cannon, Nuclear Missile, Scud Storm and the
// general's strikes) is aimed at the spot where the enemy structures the player can SEE (or remembers, fogged) are worth the most, never at a spot with
// the player's own or an ally's structure in range, and only when the target is worth firing for. Nothing here chases units or moves an army.
// Owns: nothing. Depends on: AutopilotHost, AutopilotProfile.

#pragma once

#include "Common/GameCommon.h"
#include "GameLogic/AutopilotLog.h"

class AutopilotHost;
struct AutopilotProfile;

class AutopilotSuperweapons
{
public:
	AutopilotSuperweapons(AutopilotHost &host, const AutopilotProfile &profile) : m_host(host), m_profile(profile) {}

	/// Called every two seconds.
	void update();

private:
	/// The best spot for a power with blast radius 'radius' and its value, or false when nothing visible is worth it (or only friendly buildings are near).
	Bool chooseTarget(Real radius, Coord3D *pos, Real *value) const;

	AutopilotHost &m_host;
	const AutopilotProfile &m_profile;
	AutopilotLog::Throttle m_traceThrottle;
	AutopilotLog::Throttle m_chargeTrace;
};
