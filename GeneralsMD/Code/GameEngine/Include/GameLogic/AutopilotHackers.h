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

// GeneralsX @feature Claude 21/10/2026 Autopilot hackers (wallet mode): China's Hacker hacks the Internet for cash by itself and faster inside an
// Internet Center, so a handful of hackers holed up in the Internet Center is steady income for a player who is hoarding. Trains hackers (while the
// bank stays comfortable) and sends them into the Internet Center. Owns: nothing. Depends on: AutopilotHost, AutopilotProfile, AutopilotUnits.

#pragma once

#include "Common/GameCommon.h"
#include "GameLogic/AutopilotLog.h"

class AutopilotHost;
struct AutopilotProfile;

class AutopilotHackers
{
public:
	AutopilotHackers(AutopilotHost &host, const AutopilotProfile &profile) : m_host(host), m_profile(profile) {}

	/// Called every two seconds.
	void update();

private:
	enum { MAX_HACKERS = 6 };

	AutopilotHost &m_host;
	const AutopilotProfile &m_profile;
	AutopilotLog::Throttle m_traceThrottle;
};
