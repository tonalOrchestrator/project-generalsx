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

// GeneralsX @feature Claude 21/10/2026 Autopilot repair (wallet and explore modes): an idle dozer is sent to the most damaged building. A Hard AI
// wave leaves a damaged base behind; repairing is free (it only costs the dozer's time), so a base that heals between waves holds more of them.
// Owns: nothing. Depends on: AutopilotHost, AutopilotProfile.

#pragma once

#include "Common/GameCommon.h"
#include "GameLogic/AutopilotLog.h"

class AutopilotHost;
struct AutopilotProfile;

class AutopilotRepair
{
public:
	AutopilotRepair(AutopilotHost &host, const AutopilotProfile &profile) : m_host(host), m_profile(profile) {}

	/// Called every two seconds.
	void update();

private:
	AutopilotHost &m_host;
	const AutopilotProfile &m_profile;
	AutopilotLog::Throttle m_traceThrottle;
};
