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

// GeneralsX @feature Claude 21/10/2026 What the player can see, as one JSON object: the input an outside planner (a person, a script, a language
// model) needs to suggest directives. Read-only: it never changes the game, and it only reports what the player's own shroud lets them see
// (visible objects fully; buildings seen earlier and now fogged flagged "fogged"; nothing in the black shroud).
// Owns: nothing. Depends on: nothing from the autopilot (it works for any player).

#pragma once

#include "Common/AsciiString.h"

class Player;

namespace AutopilotSnapshot
{
	/// JSON for 'player': money, power, own structures and unit counts, visible enemy structures and unit groups, capturable neutral/civilian buildings.
	AsciiString json(Player *player);
}
