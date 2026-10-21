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

struct AutopilotProfile
{
	Int m_level;				///< 1..maxLevel() (0 means autopilot off and has no profile)
	GameDifficulty m_difficulty;///< the skirmish AI skill state the level borrows (build caps, gatherers, timers)
	Bool m_wallet;				///< defend, survive, hoard money: economy + defenses, defenders under attack
	Bool m_explore;				///< capture neutral buildings for income with capture-capable infantry
	Int m_standingSquad;		///< garrison squad: tank person + gun person kept holed up in buildings near the base (0 = none)
	Bool m_hackers;				///< train Hackers and hole them up in the Internet Center for income (China)
	Bool m_repair;				///< send an idle dozer to repair the most damaged building

	/// The profile of a level, or nullptr for 0 (off) and anything out of range.
	static const AutopilotProfile *forLevel(Int level);

	/// The highest level; the hotkey cycles 1..maxLevel() and back.
	static Int maxLevel();

	/// The on-screen message when the local player switches to this level (0 = off).
	static const wchar_t *banner(Int level);
};
