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

// GeneralsX @refactor Claude 21/10/2026 What each autopilot level means, in one place. Applying a level (Player::setAutopilot), restoring one
// from a saved game (Player::xfer), the on-screen banner and the hotkey cycle all read this table, so adding a level is one row here.

#pragma once

#include "Common/GameCommon.h"

/// GeneralsX @feature Claude 21/10/2026 The switchable parts of an autopilot, one bit each. A level starts with a default set of them
/// (AutopilotProfile::modules()); the player can then turn single ones on or off (MSG_SET_AUTOPILOT's second argument).
enum AutopilotModuleBit
{
	AM_WALLET        = 1 << 0,	///< foundation first, base defenses, defenders under attack, hoard money
	AM_DEFENSE_UNITS = 1 << 1,	///< a standing mixed force (tank, anti-tank infantry, rifle infantry) kept at the base
	AM_EXPLORE       = 1 << 2,	///< capture neutral buildings for income
	AM_SQUAD         = 1 << 3,	///< garrison squad in nearby buildings
	AM_HACKERS       = 1 << 4,	///< China: Hackers in the Internet Center
	AM_REPAIR        = 1 << 5,	///< an idle dozer repairs the most damaged building
	AM_SUPERWEAPONS  = 1 << 6,	///< fire ready superweapons / area powers at the best visible enemy structures
	AM_ALL           = (1 << 7) - 1
};
static const UnsignedInt AUTOPILOT_MODULES_DEFAULT = 0xFFFFFFFFu;	///< "no override": the level's own set

struct AutopilotProfile
{
	Int m_level;				///< 1..maxLevel() (0 means autopilot off and has no profile)
	GameDifficulty m_difficulty;///< the skirmish AI skill state the level borrows (build caps, gatherers, timers)
	Bool m_wallet;				///< defend, survive, hoard money: economy + defenses, defenders under attack
	Bool m_explore;				///< capture neutral buildings for income with capture-capable infantry
	Int m_standingSquad;		///< garrison squad: tank person + gun person kept holed up in buildings near the base (0 = none)
	Bool m_hackers;				///< train Hackers and hole them up in the Internet Center for income (China)
	Bool m_repair;				///< send an idle dozer to repair the most damaged building
	Bool m_defenseUnits;		///< keep a standing mixed force at the base (see AutopilotDefense::trainStandingArmy)
	Bool m_superweapons;		///< fire ready superweapons at the best visible enemy structures (AutopilotSuperweapons)

	/// The AM_* bits this profile has on.
	UnsignedInt modules() const;

	/// This profile with exactly the modules in 'mask' on (AUTOPILOT_MODULES_DEFAULT returns it unchanged).
	AutopilotProfile withModules(UnsignedInt mask) const;

	/// "wallet, explore, repair" for the on-screen message ("none" when empty).
	static void moduleList(UnsignedInt mask, wchar_t *out, Int outSize);

	/// The profile of a level, or nullptr for 0 (off) and anything out of range.
	static const AutopilotProfile *forLevel(Int level);

	/// The highest level; the hotkey cycles 1..maxLevel() and back.
	static Int maxLevel();

	/// The on-screen message when the local player switches to this level (0 = off).
	static const wchar_t *banner(Int level);
};
