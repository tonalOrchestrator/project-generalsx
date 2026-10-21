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

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/AutopilotProfile.h"

// GeneralsX @refactor Claude 21/10/2026 Index 0 is "off"; the rows after it are the levels in hotkey-cycle order.
static const AutopilotProfile s_profiles[] =
{
	{ 1, DIFFICULTY_EASY,   FALSE, FALSE, 0, FALSE, FALSE },	// easy
	{ 2, DIFFICULTY_NORMAL, FALSE, FALSE, 0, FALSE, FALSE },	// normal
	{ 3, DIFFICULTY_HARD,   FALSE, FALSE, 0, FALSE, FALSE },	// hard
	{ 4, DIFFICULTY_NORMAL, TRUE,  FALSE, 2, TRUE,  TRUE  },	// wallet: defend, survive, hoard money (+ garrison squad, hackers, repair)
	{ 5, DIFFICULTY_NORMAL, FALSE, TRUE,  2, FALSE, TRUE  },	// explore: capture neutral buildings for income (+ garrison squad, repair)
};

static const wchar_t *const s_banners[] =
{
	L"Autopilot: OFF",
	L"Autopilot: ON (Easy)",
	L"Autopilot: ON (Normal)",
	L"Autopilot: ON (Hard)",
	L"Autopilot: WALLET MODE (defend, survive, hoard)",
	L"Autopilot: EXPLORE (capture neutral buildings for income)",
};

const AutopilotProfile *AutopilotProfile::forLevel(Int level)
{
	return (level >= 1 && level <= maxLevel()) ? &s_profiles[level - 1] : nullptr;
}

Int AutopilotProfile::maxLevel()
{
	return (Int)(sizeof(s_profiles) / sizeof(s_profiles[0]));
}

const wchar_t *AutopilotProfile::banner(Int level)
{
	return s_banners[level < 0 ? 0 : (level > maxLevel() ? maxLevel() : level)];
}
