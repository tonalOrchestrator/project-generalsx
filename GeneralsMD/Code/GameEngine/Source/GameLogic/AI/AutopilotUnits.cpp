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

#include "Common/ThingTemplate.h"

#include "GameLogic/AutopilotUnits.h"

static Bool nameContainsAny(const ThingTemplate *t, const char *const *names, Int count)
{
	AsciiString lower = t->getName();
	lower.toLower();
	for (Int i = 0; i < count; ++i)
		if (strstr(lower.str(), names[i]))
			return TRUE;
	return FALSE;
}

#define NAME_LIST_ANY(t, list) nameContainsAny((t), (list), (Int)(sizeof(list) / sizeof((list)[0])))

// GeneralsX @refactor Claude 22/10/2026 The basic rifleman of each faction is also the unit that carries the capture special power: one list.
static const char *const s_basicRiflemen[] = { "ranger", "redguard", "rebel", "minigunner" };

Bool AutopilotUnits::isCaptureCapableInfantry(const ThingTemplate *t)
{
	return NAME_LIST_ANY(t, s_basicRiflemen);
}

Bool AutopilotUnits::isRifleInfantry(const ThingTemplate *t)
{
	return NAME_LIST_ANY(t, s_basicRiflemen);
}

Bool AutopilotUnits::isAntiVehicleInfantry(const ThingTemplate *t)
{
	static const char *const names[] = { "tankhunter", "missiledefender", "tunneldefender" };
	return NAME_LIST_ANY(t, names);
}

Bool AutopilotUnits::isHacker(const ThingTemplate *t)
{
	static const char *const names[] = { "hacker" };
	return NAME_LIST_ANY(t, names);
}

Bool AutopilotUnits::isSuperweaponStructure(const ThingTemplate *t)
{
	static const char *const names[] = { "particlecannon", "uplink", "nuclearmissile", "scudstorm" };
	return NAME_LIST_ANY(t, names);
}
