/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

// GeneralsX @feature Claude 30/09/2026
// AI-vs-AI skirmish runner. Fills TheSkirmishGameInfo with AI-only slots (no human
// slot; the engine's always-present ReplayObserver becomes the local player), starts a
// normal GAME_SKIRMISH, and in -headless mode steps the logic until VictoryConditions
// reports the game over, then dumps per-player ScoreKeeper stats as JSON.
// Without -headless the normal engine loop runs so the match can be watched.

#pragma once

class AISkirmishSimulation
{
public:
	// Returns process exit code.
	static int run();

	// Windowed (watch) mode only: called every logic frame from GameLogic::update. Puts the
	// observer camera on the first AI base at game start and hops between AI bases periodically.
	static void updateWatchCamera();

private:
	static AsciiString resolveMapName(const AsciiString& wanted);
	static void dumpStats(const AsciiString& mapName, Int seed, Bool timedOut, UnsignedInt realTimeMs);
};
