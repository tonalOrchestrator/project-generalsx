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

// GeneralsX @feature Claude 22/10/2026 The autopilot control panel: a small always-on-top window in the corner of the screen that shows the local player's
// autopilot level, one check box per module, and the last lines of the activity feed. Clicking the title cycles the level; clicking a module sends the same
// network command (MSG_SET_AUTOPILOT with a module set) the hotkeys use, so every machine applies it on the same logic frame. Client-only display state.

#pragma once

#include "Common/GameCommon.h"

namespace AutopilotPanel
{
	/// Once per client frame: creates the panel when a human player is in a match, hides it otherwise.
	void update();

	/// A click at screen position (x, y), as the window's input callback or a test harness delivers it. True if it hit a control.
	Bool click(Int x, Int y);

	/// Forget the window (the window system tore it down with the match).
	void reset();
}
