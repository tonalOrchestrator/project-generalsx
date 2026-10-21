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
#include "GameLogic/AutopilotHost.h"

class AIAutopilot;
class BuildListInfo;
class SpecialPowerTemplate;


/**
 * The computer-controlled opponent.
 */
// GeneralsX @refactor Claude 21/10/2026 Also the AutopilotHost of the autopilot attached to a human player (see AIAutopilot).
class AISkirmishPlayer : public AIPlayer, private AutopilotHost
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AISkirmishPlayer, "AISkirmishPlayer"  )

public:	 // AISkirmish specific methods.

	AISkirmishPlayer( Player *p );							///< constructor
	virtual Bool computeSuperweaponTarget(const SpecialPowerTemplate *power, Coord3D *pos, Int playerNdx, Real weaponRadius) override; ///< Calculates best pos for weapon given radius.

public:	// AIPlayer interface methods.

	virtual void update() override;											///< simulates the behavior of a player

	virtual void newMap() override;											///< New map loaded call.

	virtual void adoptExistingBase() override;
	virtual void attachAutopilot(const AutopilotProfile &profile) override;	///< GeneralsX: start (or re-level) the autopilot on this AI player

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
	// GeneralsX @refactor Claude 21/10/2026 AutopilotHost: what the autopilot may use of this AI player, nothing more.
	virtual Player *player() const override { return m_player; }
	virtual const Coord3D &baseCenter() const override { return m_baseCenter; }
	virtual Real baseRadius() const override { return m_baseRadius; }
	virtual GameDifficulty difficulty() const override { return m_difficulty; }
	virtual Object *findDozerNear(const Coord3D *pos) override { return findDozer(pos); }
	virtual void requestDozer() override { queueDozer(); }
	virtual Bool isSafeToBuildAt(const Coord3D *pos, const ThingTemplate *tmpl) override { return isLocationSafe(pos, tmpl); }
	virtual Object *findSupplyWarehouse(Int minSupplies) override { return findSupplyCenter(minSupplies); }
	virtual void requestUpgrade(const AsciiString &upgradeName) override { buildUpgrade(upgradeName); }
	virtual Bool structureSlotReady() const override { return m_readyToBuildStructure; }
	virtual Int buildDelayFrames() const override { return m_buildDelay; }
	virtual Int structureTimerFrames() const override { return m_structureTimer; }
	virtual void kickBuildTimer() override { m_buildDelay = 0; }
	virtual void runBaseBuilding() override { doBaseBuilding(); checkReadyTeams(); checkQueuedTeams(); }
	virtual Bool tickTeamTimer() override { --m_teamDelay; return m_teamDelay < 1; }
	virtual void queueProduction() override { queueUnits(); }
	virtual void restartTeamTimer(Int frames) override { m_teamDelay = frames; }

	AIAutopilot *m_autopilot = nullptr;		///< GeneralsX: the autopilot driving this (human) player's base, or null (not saved, not in the CRC)
};
