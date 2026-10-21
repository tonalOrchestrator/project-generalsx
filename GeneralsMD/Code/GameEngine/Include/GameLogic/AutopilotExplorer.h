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

// GeneralsX @refactor Claude 21/10/2026 Autopilot explore mode (level 5): normal autopilot plus explorers. An explorer is an infantry unit
// that can capture buildings (Ranger, Red Guard, Rebel). Owns: the explorers' errands and the blacklist of buildings they failed to take.
// Depends on: AutopilotHost, AutopilotProfile.

#pragma once

#include "Common/GameCommon.h"
#include "Common/GameType.h"
#include "GameLogic/AutopilotLog.h"

class AutopilotHost;
class Object;
struct AutopilotProfile;

class AutopilotExplorer
{
public:
	AutopilotExplorer(AutopilotHost &host, const AutopilotProfile &profile);

	/// Called every two seconds. Buys the capture upgrade, trains an explorer when short, and sends every idle explorer to capture
	/// (or scout) the nearest safe neutral building. A building it fails to take is skipped for a while. Nothing here attacks an enemy.
	void update();

	/// True for the explorers: the first EXPLORER_ROSTER capture-capable units. The garrison squad leaves them alone.
	Bool isExplorer(const Object *unit) const;

private:
	enum { MAX_EXPLORERS = 8, BLACKLIST_SIZE = 16, EXPLORER_ROSTER = 2 };

	void forgetFinishedErrands(UnsignedInt frame);
	void trainExplorer(UnsignedInt frame);
	void sendIdleExplorers(Object *const *explorers, Int numExplorers, UnsignedInt frame);

	AutopilotHost &m_host;
	const AutopilotProfile &m_profile;

	ObjectID m_explorer[MAX_EXPLORERS];				///< explorer on each errand ...
	ObjectID m_target[MAX_EXPLORERS];				///< ... and the building it was sent to capture
	Bool m_scouting[MAX_EXPLORERS];					///< the errand is a walk to reveal the building, not a capture yet
	ObjectID m_blacklist[BLACKLIST_SIZE];			///< targets an explorer failed to capture (unreachable ...), skipped until the frame below
	UnsignedInt m_blacklistUntil[BLACKLIST_SIZE];

	AutopilotLog::Throttle m_statusThrottle;		///< trace only
	AutopilotLog::Throttle m_nothingToCaptureThrottle;
};
