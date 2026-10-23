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

// GeneralsX @feature Claude 22/10/2026 Directives: small standing orders for the autopilot, each sent as one network command (MSG_AUTOPILOT_DIRECTIVE) so every
// machine stores it on the same logic frame. A directive is an INPUT to a module's choice, never an order of its own: the explorer prefers the building
// it names, and everything else about carrying it out (safety, the capture upgrade, scouting a shrouded target) is the module's normal path. It expires.
// First cut: capture (a building by object id) and clear. See docs/WORKDIR/planning/AUTOPILOT_DIRECTIVES.md.

#pragma once

#include "Common/GameCommon.h"

enum AutopilotDirectiveKind
{
	AD_NONE = 0,
	AD_CAPTURE = 1,		///< prefer capturing the building with this object id
	AD_CLEAR = 9		///< drop every standing directive
};

class AutopilotDirectives
{
public:
	AutopilotDirectives() { for (Int i = 0; i < SLOTS; ++i) m_slots[i].kind = AD_NONE; }

	/// Store (or, for AD_CLEAR, drop) a directive that holds until 'expireFrame'. A new one of the same kind replaces the old.
	void apply(Int kind, ObjectID id, UnsignedInt expireFrame)
	{
		if (kind == AD_CLEAR)
		{
			for (Int i = 0; i < SLOTS; ++i) m_slots[i].kind = AD_NONE;
			return;
		}
		Int free = -1;
		for (Int i = 0; i < SLOTS; ++i)
		{
			if (m_slots[i].kind == kind) { free = i; break; }
			if (m_slots[i].kind == AD_NONE && free < 0) free = i;
		}
		if (free < 0) free = 0;
		m_slots[free].kind = kind;
		m_slots[free].id = id;
		m_slots[free].expires = expireFrame;
	}

	/// The building a capture directive names, or INVALID_ID.
	ObjectID captureTarget(UnsignedInt frame) const
	{
		for (Int i = 0; i < SLOTS; ++i)
			if (m_slots[i].kind == AD_CAPTURE && frame < m_slots[i].expires)
				return m_slots[i].id;
		return INVALID_ID;
	}

private:
	enum { SLOTS = 8 };
	struct Slot { Int kind; ObjectID id; UnsignedInt expires; };
	Slot m_slots[SLOTS];
};
