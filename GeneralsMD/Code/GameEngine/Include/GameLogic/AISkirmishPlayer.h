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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// AISkirmishPlayer.h
// Computerized opponent
// Author: Michael S. Booth, January 2002

#pragma once

#include "Common/GameMemory.h"
#include "GameLogic/AIPlayer.h"

class BuildListInfo;
class SpecialPowerTemplate;


/**
 * The computer-controlled opponent.
 */
class AISkirmishPlayer : public AIPlayer
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AISkirmishPlayer, "AISkirmishPlayer"  )

public:	 // AISkirmish specific methods.

	AISkirmishPlayer( Player *p );							///< constructor
	virtual Bool computeSuperweaponTarget(const SpecialPowerTemplate *power, Coord3D *pos, Int playerNdx, Real weaponRadius) override; ///< Calculates best pos for weapon given radius.

public:	// AIPlayer interface methods.

	virtual void update() override;											///< simulates the behavior of a player

	virtual void newMap() override;											///< New map loaded call.

	virtual void adoptExistingBase() override;
	Bool autopilotQueueMissingPrerequisite(const ThingTemplate *plan, UnsignedInt money, Int depth, KindOfType onlyKind = KINDOF_INVALID);
	void autopilotExplore();	///< GeneralsX: explore mode: train a capture-capable infantry unit and send it to capture neutral buildings
	Bool autopilotIsThreatened();	///< GeneralsX: enemy units near the base
	void autopilotTrainDefenders();	///< GeneralsX: wallet mode under attack: queue a defender at a barracks / war factory
	Int autopilotGatherers() const;	///< GeneralsX: desired supply gatherers for the current level
	Bool autopilotFindSupplySpot(const ThingTemplate *need, Object *dozer, const Object *warehouse, Coord3D *out);
	Bool autopilotQueueSupplyExpansion(UnsignedInt money);	///< GeneralsX: a second supply center at another warehouse
	void autopilotMarkSupplyEntry(BuildListInfo *info, const ThingTemplate *tmpl);	///< GeneralsX: flag a supply center entry so trucks are sent to it
	void autopilotExplainStall(BuildListInfo *info);	///< GeneralsX: log why a queued structure was not placed (harness runs only)
	void autopilotQueueNextStructure();	///< GeneralsX: autopilot's build planner (the skirmish scripts do this for computer players)						///< GeneralsX: claim the human player's existing structures for the build list

	/// Invoked when a unit I am training comes into existence
	virtual void onUnitProduced( Object *factory, Object *unit ) override;

	virtual void buildSpecificAITeam(TeamPrototype *teamProto, Bool priorityBuild) override; ///< Builds this team immediately.

	virtual void buildSpecificAIBuilding(const AsciiString &thingName) override; ///< Builds this building as soon as possible.

	virtual void buildAIBaseDefense(Bool flank) override; ///< Builds base defense on front or flank of base.

	virtual void buildAIBaseDefenseStructure(const AsciiString &thingName, Bool flank) override; ///< Builds base defense on front or flank of base.

	virtual void recruitSpecificAITeam(TeamPrototype *teamProto, Real recruitRadius) override; ///< Builds this team immediately.

	virtual Bool isSkirmishAI() override {return true;}

	virtual Bool checkBridges(Object *unit, Waypoint *way) override;

	virtual Player *getAiEnemy() override;	///< Solo AI attacks based on scripting.  Only skirmish auto-acquires an enemy at this point.  jba.

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

	virtual void doBaseBuilding() override;
	virtual void checkReadyTeams() override;
	virtual void checkQueuedTeams() override;
	virtual void doTeamBuilding() override;
	virtual Object *findDozer(const Coord3D *pos) override;
	virtual void queueDozer() override;

protected:

	virtual Bool selectTeamToBuild() override;			///< determine the next team to build
	virtual Bool selectTeamToReinforce( Int minPriority ) override;			///< determine the next team to reinforce
	virtual Bool startTraining( WorkOrder *order, Bool busyOK, AsciiString teamName) override;	///< find a production building that can handle the order, and start building

	virtual Bool isAGoodIdeaToBuildTeam( TeamPrototype *proto ) override;		///< return true if team should be built
	virtual void processBaseBuilding() override;		///< do base-building behaviors
	virtual void processTeamBuilding() override;		///< do team-building behaviors

protected:
	void adjustBuildList(BuildListInfo *list, Bool destroyStartCommandCenter = true);
	Int getMyEnemyPlayerIndex();
	void acquireEnemy();

protected:
	Int m_curFrontBaseDefense; // First is 0.
	Int m_curFlankBaseDefense; // First is 0.
	Real m_curFrontLeftDefenseAngle;
	Real m_curFrontRightDefenseAngle;
	Real m_curLeftFlankLeftDefenseAngle;
	Real m_curLeftFlankRightDefenseAngle;
	Real m_curRightFlankLeftDefenseAngle;
	Real m_curRightFlankRightDefenseAngle;

	UnsignedInt m_frameToCheckEnemy;
	Player			*m_currentEnemy;


private:
	BuildListInfo *m_autopilotPending = nullptr;		///< GeneralsX: build-list entry the autopilot queued and is waiting on
	UnsignedInt m_autopilotPendingFrame = 0;
	ObjectID m_exploreExplorer[8] = {};				///< GeneralsX explore mode: explorer on each errand ...
	ObjectID m_exploreTarget[8] = {};				///< ... and the building it was sent to capture
	UnsignedInt m_exploreOrderFrame[8] = {};
	Bool m_exploreScouting[8] = {};					///< the errand is a walk to reveal the building, not a capture yet
	ObjectID m_exploreBlacklist[16] = {};			///< targets an explorer failed to capture (unreachable ...), skipped until the frame below
	UnsignedInt m_exploreBlacklistUntil[16] = {};
	Int m_autopilotRing = 0;							///< GeneralsX: next slot on the ring around the base for a prerequisite structure
};
