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

// GeneralsX @refactor Claude 22/10/2026 What the structure planner may spend on base defenses, in its own header so the planner does not
// depend on the defense module that computes it (AutopilotDefense::budget fills it in).

#pragma once

#include "Common/GameCommon.h"

struct AutopilotDefenseBudget
{
	Bool m_threatened;				///< enemy units are near the base (always false outside wallet mode)
	Int m_perTemplateCap;			///< most defenses of one kind
	Int m_totalCap;					///< most defenses of all kinds together
	UnsignedInt m_reserve;			///< a defense is only started if this much stays in the bank
};
