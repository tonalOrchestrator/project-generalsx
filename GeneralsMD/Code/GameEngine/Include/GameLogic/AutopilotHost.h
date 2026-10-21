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

// GeneralsX @refactor Claude 21/10/2026 Everything the autopilot needs FROM the AI player it is attached to, as a narrow interface.
// AISkirmishPlayer implements it with thin wrappers over its own (protected) base-building machinery; the autopilot modules see
// nothing else of the AI player, so they can be read, changed and tested without touching it.

#pragma once

#include "Common/AsciiString.h"
#include "Common/GameCommon.h"

class Object;
class Player;
class ThingTemplate;

class AutopilotHost
{
public:
	// ---- who and where ----
	virtual Player *player() const = 0;
	virtual const Coord3D &baseCenter() const = 0;	///< center of the player's build list
	virtual Real baseRadius() const = 0;
	virtual GameDifficulty difficulty() const = 0;	///< the AI skill state the autopilot level borrowed (build caps, gatherers)

	// ---- the AI player's builders and queries ----
	virtual Object *findDozerNear(const Coord3D *pos) = 0;
	virtual void requestDozer() = 0;					///< queue a new dozer/worker
	virtual Bool isSafeToBuildAt(const Coord3D *pos, const ThingTemplate *tmpl) = 0;	///< no enemy near that spot
	virtual Object *findSupplyWarehouse(Int minSupplies) = 0;	///< the AI's pick of supply source with at least that much cash left
	virtual void requestUpgrade(const AsciiString &upgradeName) = 0;	///< queue a player upgrade at a factory that can make it

	// ---- the structure build cadence ----
	virtual Bool structureSlotReady() const = 0;		///< the AI's build timer allows placing the next structure
	virtual Int buildDelayFrames() const = 0;		///< (trace only)
	virtual Int structureTimerFrames() const = 0;	///< (trace only)
	virtual void kickBuildTimer() = 0;				///< let the base-building pass look at the build list again right away

	// ---- the production side the autopilot lets the AI player keep running ----
	virtual void runBaseBuilding() = 0;				///< place queued structures, finish ready/queued teams (dozers, trucks)
	virtual Bool tickTeamTimer() = 0;				///< count the team timer down one frame; true once it ran out
	virtual void queueProduction() = 0;				///< queue supply trucks when short and start queued truck/dozer orders
	virtual void restartTeamTimer(Int frames) = 0;

protected:
	~AutopilotHost() = default;	// never deleted through this interface
};
