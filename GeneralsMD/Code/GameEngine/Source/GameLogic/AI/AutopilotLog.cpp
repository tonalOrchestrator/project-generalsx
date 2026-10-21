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

#include <stdarg.h>
#include <stdio.h>

#include "Common/LanNetplayOptions.h"
#include "GameLogic/AutopilotLog.h"

// GeneralsX @refactor Claude 21/10/2026 See AutopilotLog.h.
Bool AutopilotLog::enabled()
{
	return lanNetplayOptions().m_active;
}

void AutopilotLog::line(const char *format, ...)
{
	if (!enabled())
		return;
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	fflush(stdout);
}

Bool AutopilotLog::onFrameMultiple(UnsignedInt frame, UnsignedInt interval)
{
	return enabled() && frame % interval == 0;
}

Bool AutopilotLog::Throttle::due(UnsignedInt frame, UnsignedInt interval)
{
	if (!enabled() || frame < m_lastFrame + interval)
		return FALSE;
	m_lastFrame = frame;
	return TRUE;
}
