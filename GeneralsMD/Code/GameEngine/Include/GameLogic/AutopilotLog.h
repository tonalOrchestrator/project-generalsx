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

// GeneralsX @refactor Claude 21/10/2026 One place for the autopilot's harness trace, replacing the copy-pasted
// "if (lanNetplayOptions().m_active) { printf(...); fflush(stdout); }" blocks. tools/netplay/autopilot_scenarios.py parses these lines,
// so the text passed to line() is part of the harness contract. Nothing here is read back by the game: it never affects logic.

#pragma once

#include "Common/GameCommon.h"

namespace AutopilotLog
{
	/// True in a LAN netplay harness run, the only time the autopilot prints anything.
	Bool enabled();

	/// printf plus fflush(stdout), only when enabled(). Callers write the whole line, newline included.
#if defined(__GNUC__)
	void line(const char *format, ...) __attribute__((format(printf, 1, 2)));
#else
	void line(const char *format, ...);
#endif

	/// True when enabled() and 'frame' is a multiple of 'interval' (a trace that fires once per interval of game time).
	Bool onFrameMultiple(UnsignedInt frame, UnsignedInt interval);

	/// "At most once per N frames" gate for a trace line; one instance per trace.
	class Throttle
	{
	public:
		/// True (and the gate re-arms) when enabled() and at least 'interval' frames passed since the last time it was true.
		Bool due(UnsignedInt frame, UnsignedInt interval);

	private:
		UnsignedInt m_lastFrame = 0;
	};
}
