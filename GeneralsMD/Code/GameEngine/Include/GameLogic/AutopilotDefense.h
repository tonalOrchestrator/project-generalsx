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

// GeneralsX @refactor Claude 21/10/2026 Autopilot defense (wallet mode, level 4): is the base under threat, how many base defenses the
// planner may spend on, and training a few defenders while it is. Nothing here ever attacks.
// Owns: only the throttle of its trace line. Depends on: AutopilotHost, AutopilotProfile.

#pragma once

#include "Common/GameCommon.h"
#include "GameLogic/AutopilotLog.h"

class AutopilotHost;
struct AutopilotProfile;

class AutopilotDefense
{
public:
	/// What the structure planner may spend on base defenses right now.
	struct Budget
	{
		Bool m_threatened;				///< enemy units are near the base (always false outside wallet mode)
		Int m_perTemplateCap;			///< most defenses of one kind
		Int m_totalCap;					///< most defenses of all kinds together
		UnsignedInt m_reserve;			///< a defense is only started if this much stays in the bank
	};

	AutopilotDefense(AutopilotHost &host, const AutopilotProfile &profile) : m_host(host), m_profile(profile) {}

	/// Wallet mode only: enemy units near the base.
	Bool isThreatened();

	/// isThreatened(), held for 90 seconds after the last sighting.
	Bool underThreat();

	/// Under threat the bank is there to be spent (many more defenses, almost no reserve); otherwise a few defenses and a big reserve.
	Budget budget(UnsignedInt bank);

	/// Wallet mode under attack: queue combat units at barracks / war factories, up to a cap on the army. Called every two seconds.
	void trainDefenders();

	/// Defense Units module: keep a standing mixed force at the base (tank, anti-tank infantry, rifle infantry in turn), sized by the bank, without
	/// ever touching the defense fund. Called every two seconds; nothing here attacks.
	void trainStandingArmy();

private:
	AutopilotHost &m_host;
	Int m_nextRole = 0;					///< standing army: 0 tank, 1 anti-vehicle infantry, 2 rifle infantry, rotating
	const AutopilotProfile &m_profile;
	UnsignedInt m_threatUntilFrame = 0;	///< the threat flag stays on until this frame
	AutopilotLog::Throttle m_traceThrottle;
	AutopilotLog::Throttle m_armyTrace;	// the standing army has its own, or the defenders line starves it
};
