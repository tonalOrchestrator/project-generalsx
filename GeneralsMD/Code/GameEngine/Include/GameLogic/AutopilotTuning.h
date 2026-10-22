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

// GeneralsX @refactor Claude 22/10/2026 Every number that tunes the autopilot's behaviour, in one place and with a name, so a general's
// personality can later override them and a gameplay experiment changes one line. The values are exactly the ones the modules used inline.

#pragma once

#include "Common/GameCommon.h"

namespace AutopilotTuning
{
	// Money: how much must be left in the bank after a purchase of each kind (cost + margin <= bank).
	static const UnsignedInt DEFENSE_FUND = 1200;			///< units never dip into this; the planner needs it to afford a defense
	static const UnsignedInt SQUAD_UNIT_MARGIN = 1500;		///< garrison squad units
	static const UnsignedInt HACKER_MARGIN = 2500;			///< Hackers
	static const UnsignedInt EXPLORER_UNIT_MARGIN = 500;	///< capture-capable explorers
	static const UnsignedInt SECOND_CENTER_MARGIN = 1500;	///< a second supply center
	static const UnsignedInt DEFENSE_UNIT_COST_SLACK = 300;	///< a standing-army unit may cost at most a third of the bank plus this

	// Defense budget (AutopilotDefense::budget): quiet versus under attack.
	static const Int THREAT_MEMORY_SECONDS = 90;			///< the threat flag stays on this long after enemies were last near the base
	static const Int QUIET_PER_KIND_CAP = 3;
	static const Int THREATENED_PER_KIND_CAP = 16;
	static const Int QUIET_TOTAL_CAP = 6;
	static const Int THREATENED_TOTAL_CAP_BASE = 24;		///< plus one per DEFENSE_CAP_BANK_STEP in the bank, at most THREATENED_TOTAL_CAP_BONUS_MAX
	static const UnsignedInt DEFENSE_CAP_BANK_STEP = 2000;
	static const UnsignedInt THREATENED_TOTAL_CAP_BONUS_MAX = 24;
	static const UnsignedInt QUIET_RESERVE = 1500;
	static const UnsignedInt THREATENED_RESERVE = 200;

	// Defending army (wallet defenders) and the standing army (Defense Units module).
	static const Int ARMY_CAP_BASE = 16;					///< plus one per ARMY_BANK_STEP in the bank, at most ARMY_CAP_BONUS_MAX
	static const UnsignedInt ARMY_BANK_STEP = 1500;
	static const UnsignedInt ARMY_CAP_BONUS_MAX = 40;
	static const Int STANDING_ARMY_BASE = 6;				///< plus one per STANDING_ARMY_BANK_STEP, at most STANDING_ARMY_BONUS_MAX
	static const UnsignedInt STANDING_ARMY_BANK_STEP = 2000;
	static const UnsignedInt STANDING_ARMY_BONUS_MAX = 14;

	// Extra defenses beyond the faction's build list (AutopilotPlanner::queueExtraDefense).
	static const UnsignedInt QUIET_EXTRA_DEFENSE_RESERVE = 2500;	///< a quiet player only spends a real surplus
	static const UnsignedInt QUIET_EXTRA_DEFENSE_MIN_BANK = 5000;
	static const UnsignedInt QUIET_PREREQ_DEFENSE_MARGIN = 3000;	///< a defense that needs a prerequisite building waits for this much extra
	static const Real DEFENSE_RING_RADIUS_FACTOR = 0.8f;			///< of the base radius
	static const Real DEFENSE_RING_RADIUS_BASE = 120.0f;
	static const Real DEFENSE_RING_RADIUS_STEP = 90.0f;			///< per ring outwards (three rings)

	// Squad and repair.
	static const Real SQUAD_REACH_BEYOND_BASE = 1500.0f;			///< neutral buildings are rarely at the base: about a quarter of a typical map
	static const Real REPAIR_BELOW_HEALTH = 0.7f;				///< a finished building under this fraction of its health gets repaired

	// Superweapons (AutopilotSuperweapons).
	static const Real SUPERWEAPON_MIN_TARGET_VALUE = 2000.0f;	///< summed (distance-weighted) build cost of the structures in range; below this the power waits
	static const Real SUPERWEAPON_MIN_RADIUS = 50.0f;
	static const Real SUPERWEAPON_POWER_HEADROOM = 1.25f;			///< with a superweapon standing, build power plants until supply is this multiple of demand
	static const UnsignedInt SUPERWEAPON_PREREQ_ALLOWANCE = 2500;	///< added to the price of a superweapon that waits on a prerequisite building
	static const Int SUPERWEAPON_SAVE_AFTER_DEFENSES = 4;			///< base defenses the player has before it starts saving for a superweapon
	static const Int SUPERWEAPON_SAVE_MIN_ARMY = 8;				///< while saving, units may still spend down to the fund until this many exist
	static const Int SUPERWEAPON_SAVE_MAX_SECONDS = 300;			///< give up saving after this long without reaching the price ...
	static const Int SUPERWEAPON_SAVE_COOLDOWN_SECONDS = 120;		///< ... and defend normally for this long
	static const Int SUPERWEAPON_STRUCTURE_CAP = 3;				///< most superweapon structures of one kind to build (the game's own limit may be lower)
	static const UnsignedInt SUPERWEAPON_BUILD_MARGIN = 2700;	///< left in the bank after starting one: the defense fund plus a cushion

	// Explorers.
	static const Int CAPTURE_BLACKLIST_SECONDS = 300;				///< a building the explorer failed to take is skipped this long
}
