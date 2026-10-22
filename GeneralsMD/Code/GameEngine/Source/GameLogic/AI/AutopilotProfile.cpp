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
	{ 1, DIFFICULTY_EASY,   FALSE, FALSE, 0, FALSE, FALSE, FALSE },	// easy
	{ 2, DIFFICULTY_NORMAL, FALSE, FALSE, 0, FALSE, FALSE, FALSE },	// normal
	{ 3, DIFFICULTY_HARD,   FALSE, FALSE, 0, FALSE, FALSE, FALSE },	// hard
	{ 4, DIFFICULTY_NORMAL, TRUE,  FALSE, 2, TRUE,  TRUE, FALSE },	// wallet: defend, survive, hoard money (+ garrison squad, hackers, repair)
	{ 5, DIFFICULTY_NORMAL, FALSE, TRUE,  2, FALSE, TRUE, FALSE },	// explore: capture neutral buildings for income (+ garrison squad, repair)
	{ 6, DIFFICULTY_NORMAL, TRUE,  TRUE,  2, TRUE,  TRUE,  FALSE },	// fortress: wallet + explore (captured income feeds the defense spending)
};

static const wchar_t *const s_banners[] =
{
	L"Autopilot: OFF",
	L"Autopilot: ON (Easy)",
	L"Autopilot: ON (Normal)",
	L"Autopilot: ON (Hard)",
	L"Autopilot: WALLET MODE (defend, survive, hoard)",
	L"Autopilot: EXPLORE (capture neutral buildings for income)",
	L"Autopilot: FORTRESS (wallet + explore: capture income, defend, survive)",
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

UnsignedInt AutopilotProfile::modules() const
{
	return (m_wallet ? AM_WALLET : 0) | (m_defenseUnits ? AM_DEFENSE_UNITS : 0) | (m_explore ? AM_EXPLORE : 0)
		| (m_standingSquad > 0 ? AM_SQUAD : 0) | (m_hackers ? AM_HACKERS : 0) | (m_repair ? AM_REPAIR : 0);
}

AutopilotProfile AutopilotProfile::withModules(UnsignedInt mask) const
{
	AutopilotProfile p = *this;
	if (mask == AUTOPILOT_MODULES_DEFAULT)
		return p;
	p.m_wallet = (mask & AM_WALLET) != 0;
	p.m_defenseUnits = (mask & AM_DEFENSE_UNITS) != 0;
	p.m_explore = (mask & AM_EXPLORE) != 0;
	p.m_standingSquad = (mask & AM_SQUAD) ? (m_standingSquad > 0 ? m_standingSquad : 2) : 0;
	p.m_hackers = (mask & AM_HACKERS) != 0;
	p.m_repair = (mask & AM_REPAIR) != 0;
	return p;
}

void AutopilotProfile::moduleList(UnsignedInt mask, wchar_t *out, Int outSize)
{
	static const struct { UnsignedInt bit; const wchar_t *name; } names[] =
	{
		{ AM_WALLET, L"wallet" }, { AM_DEFENSE_UNITS, L"defense units" }, { AM_EXPLORE, L"explore" },
		{ AM_SQUAD, L"squad" }, { AM_HACKERS, L"hackers" }, { AM_REPAIR, L"repair" },
	};
	out[0] = 0;
	Int len = 0;
	for (Int i = 0; i < (Int)(sizeof(names) / sizeof(names[0])); ++i)
	{
		if (!(mask & names[i].bit))
			continue;
		const wchar_t *name = names[i].name;
		if (len > 0 && len + 2 < outSize) { out[len++] = L','; out[len++] = L' '; }
		while (*name && len + 1 < outSize)
			out[len++] = *name++;
		out[len] = 0;
	}
	if (len == 0)
	{
		const wchar_t *none = L"none";
		while (*none && len + 1 < outSize)
			out[len++] = *none++;
		out[len] = 0;
	}
}
