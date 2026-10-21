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

// AISkirmishPlayer.cpp
// Computerized opponent
// Author: Michael S. Booth, January 2002

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine


#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/LanNetplayOptions.h"
#include "Common/AISkirmishSimulation.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ActionManager.h"
#include "Common/BuildAssistant.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "Common/ThingTemplate.h"
#include "Common/WellKnownKeys.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/AISkirmishPlayer.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/RebuildHoleBehavior.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameClient/TerrainVisual.h"


#define USE_DOZER 1



///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

AISkirmishPlayer::AISkirmishPlayer( Player *p ) :	AIPlayer(p),
m_curFlankBaseDefense(0),
m_curFrontBaseDefense(0),
m_curFrontLeftDefenseAngle(0),
m_curFrontRightDefenseAngle(0),
m_curLeftFlankLeftDefenseAngle(0),
m_curLeftFlankRightDefenseAngle(0),
m_curRightFlankLeftDefenseAngle(0),
m_curRightFlankRightDefenseAngle(0),
m_frameToCheckEnemy(0),
m_currentEnemy(nullptr)

{
	m_frameLastBuildingBuilt = TheGameLogic->getFrame();
	p->setCanBuildUnits(true); // turn on ai production by default.
}

AISkirmishPlayer::~AISkirmishPlayer()
{
	clearTeamsInQueue();
}


/**
 * Build our base.
 */
void AISkirmishPlayer::processBaseBuilding()
{
	//
	// Refresh base buildings. Scan through list, if a building is missing,
	// rebuild it, unless it's rebuild count is zero.
	//
	if (m_readyToBuildStructure)
	{
		const ThingTemplate *bldgPlan=nullptr;
		BuildListInfo	*bldgInfo = nullptr;
		Bool isPriority = false;
		Object *bldg = nullptr;
		const ThingTemplate *powerPlan=nullptr;
		BuildListInfo	*powerInfo = nullptr;
		Bool isUnderPowered = !m_player->getEnergy()->hasSufficientPower();
		Bool powerUnderConstruction = false;
		for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
		{
			AsciiString name = info->getTemplateName();
			if (name.isEmpty()) continue;
			const ThingTemplate *curPlan = TheThingFactory->findTemplate( name );
			if (!curPlan) {
				DEBUG_LOG(("*** ERROR - Build list building '%s' doesn't exist.", name.str()));
				continue;
			}
			bldg = TheGameLogic->findObjectByID( info->getObjectID() );
			// check for hole.
			if (info->getObjectID() != INVALID_ID) {
				// used to have a building.
				Object *bldg = TheGameLogic->findObjectByID( info->getObjectID() );
				if (bldg==nullptr) {
					// got destroyed.
					ObjectID priorID;
					priorID = info->getObjectID();
					info->setObjectID(INVALID_ID);
					info->setObjectTimestamp(TheGameLogic->getFrame()+1);
					// Scan for a GLA hole.	KINDOF_REBUILD_HOLE
					Object *obj;
					for( obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() ) {
						if (!obj->isKindOf(KINDOF_REBUILD_HOLE)) continue;
						RebuildHoleBehaviorInterface *rhbi = RebuildHoleBehavior::getRebuildHoleBehaviorInterfaceFromObject( obj );
						if( rhbi ) {
							ObjectID spawnerID = rhbi->getSpawnerID();
							if (priorID == spawnerID) {
								DEBUG_LOG(("AI Found hole to rebuild %s", curPlan->getName().str()));
								info->setObjectID(obj->getID());
							}
						}
 					}
				}	else {
					if (bldg->getControllingPlayer() == m_player) {
						// Check for built or dozer missing.
						if( bldg->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
						{
							if (bldg->isKindOf(KINDOF_FS_POWER) && !bldg->isKindOf(KINDOF_CASH_GENERATOR))
							{
								powerUnderConstruction = true;
							}
							// make sure dozer is working on him.
							ObjectID builder = bldg->getBuilderID();
							Object* myDozer = TheGameLogic->findObjectByID(builder);

              if (myDozer && ( myDozer->getControllingPlayer() != m_player || myDozer->isDisabledByType( DISABLED_UNMANNED ) ) )
              {//I don't expect this dozer to work well with me.
                myDozer = nullptr;
                bldg->setBuilder( nullptr );
              }

							if (myDozer==nullptr) {
								DEBUG_LOG(("AI's Dozer got killed (or captured).  Find another dozer."));
								queueDozer();
 								myDozer = findDozer(bldg->getPosition());
								if (myDozer==nullptr || myDozer->getAI()==nullptr) {
									continue;
								}
								myDozer->getAI()->aiResumeConstruction(bldg, CMD_FROM_AI);
							}	else {
								// make sure he is building.
								myDozer->getAI()->aiResumeConstruction(bldg, CMD_FROM_AI);
							}
						}
					} else {
						// oops, got captured.
						info->setObjectID(INVALID_ID);
						info->setObjectTimestamp(TheGameLogic->getFrame()+1);
					}
				}
			}
			if (info->getObjectID()==INVALID_ID && info->getObjectTimestamp()>0) {
				// this object was built at some time, and got destroyed at or near objectTimestamp.
				// Wait a few seconds before initiating a rebuild.
				if (info->getObjectTimestamp()+TheAI->getAiData()->m_rebuildDelaySeconds*LOGICFRAMES_PER_SECOND > TheGameLogic->getFrame()) {
					continue;
				}	else {
					DEBUG_LOG(("Enabling rebuild for %s", info->getTemplateName().str()));
					info->setObjectTimestamp(0); // ready to build.
				}
			}
			if (bldg) {
				continue; // already built.
			}
			// Make sure it is safe to build here.
			if (!isLocationSafe(info->getLocation(), curPlan)) {
				continue;
			}
			if (info->isPriorityBuild()) {
				// Always take priority build, unless we already have priority build.
				if (!isPriority) {
					bldgPlan = curPlan;
					bldgInfo = info;
					isPriority = true;
				}
			}
			if (curPlan->isKindOf(KINDOF_FS_POWER)) {
				if (powerPlan==nullptr && !curPlan->isKindOf(KINDOF_CASH_GENERATOR)) {
					if (isUnderPowered || info->isAutomaticBuild()) {
						powerPlan = curPlan;
						powerInfo = info;
					}
				}
			}
			if (!info->isAutomaticBuild()) {
				continue; // marked to not build automatically.
			}
			Object *dozer = findDozer(info->getLocation());
			if (dozer==nullptr) {
				if (isUnderPowered) {
					queueDozer();
				}
				continue;
			}
			if (TheBuildAssistant->canMakeUnit(dozer, bldgPlan)!=CANMAKE_OK) {
				if (info->isBuildable()) {
					AsciiString bldgName = info->getTemplateName();
					bldgName.concat(" - Dozer unable to build - money or technology missing.");
					TheScriptEngine->AppendDebugMessage(bldgName, false);
				}
				continue;
			}
			// check if this building has any "rebuilds" left
			if (info->isBuildable())
			{
				if (bldgPlan == nullptr) {
					bldgPlan = curPlan;
					bldgInfo = info;
				}
			}
		}
		if (powerInfo && powerPlan && !powerPlan->isEquivalentTo(bldgPlan)) {
			if (!powerUnderConstruction) {
				bldgPlan = powerPlan;
				bldgInfo = powerInfo;
				DEBUG_LOG(("Forcing build of power plant."));
			}
		}
		if (bldgPlan && bldgInfo) {
#ifdef USE_DOZER
			// dozer-construct the building
			bldg = buildStructureWithDozer(bldgPlan, bldgInfo);
			if (m_autopilotOnly && lanNetplayOptions().m_active)
			{
				const Bool brokeFor = !bldg && m_player->getMoney()->countMoney() < bldgPlan->calcCostToBuild(m_player);
				if (bldg || !brokeFor || TheGameLogic->getFrame() % 600 == 0)	// waiting for money is normal: log it sparsely
				{
					printf("NETPLAY autopilot place %s priority=%d -> %s (frame %u)\n", bldgPlan->getName().str(), (Int)bldgInfo->isPriorityBuild(), bldg ? "placed" : (brokeFor ? "waiting for money" : "FAILED"), TheGameLogic->getFrame());
					fflush(stdout);
				}
				if (!bldg && !brokeFor)
					autopilotExplainStall(bldgInfo);
			}
			// store the object with the build order
			if (bldg)
			{
				bldgInfo->setObjectID( bldg->getID() );
				bldgInfo->decrementNumRebuilds();

				m_readyToBuildStructure = false;
				m_structureTimer = TheAI->getAiData()->m_structureSeconds*static_cast<float>(LOGICFRAMES_PER_SECOND);
				if (m_player->getMoney()->countMoney() < TheAI->getAiData()->m_resourcesPoor) {
					m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresPoorMod;
				}	else if (m_player->getMoney()->countMoney() > TheAI->getAiData()->m_resourcesWealthy) {
					m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresWealthyMod;
				}
				m_frameLastBuildingBuilt = TheGameLogic->getFrame();
				// only build one building per delay loop
			}

#else
			// force delay between rebuilds
			Int framesToBuild = bldgPlan->calcTimeToBuild(m_player);
			if (TheGameLogic->getFrame() - m_frameLastBuildingBuilt < framesToBuild)
			{
				m_buildDelay = framesToBuild - (TheGameLogic->getFrame() - m_frameLastBuildingBuilt);
				return;
			}	else {
				// building is missing, (re)build it
				// deduct money to build, if we have it
				Int cost = bldgPlan->calcCostToBuild( m_player );
				if (m_player->getMoney()->countMoney() >= cost)
				{
					// we have the money, deduct it
					m_player->getMoney()->withdraw( cost );

					// inst-construct the building
					bldg = buildStructureNow(bldgPlan, bldgInfo);
					// store the object with the build order
					if (bldg)
					{
						bldgInfo->setObjectID( bldg->getID() );
						bldgInfo->decrementNumRebuilds();

						m_readyToBuildStructure = false;
						m_structureTimer = TheAI->getAiData()->m_structureSeconds*LOGICFRAMES_PER_SECOND;
						if (m_player->getMoney()->countMoney() < TheAI->getAiData()->m_resourcesPoor) {
							m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresPoorMod;
						}	else if (m_player->getMoney()->countMoney() > TheAI->getAiData()->m_resourcesWealthy) {
							m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresWealthyMod;
						}
						m_frameLastBuildingBuilt = TheGameLogic->getFrame();
					}
				}
			}
#endif
		}
	}
}

/**
 * Invoked when a unit I am training comes into existence
 */
void AISkirmishPlayer::onUnitProduced( Object *factory, Object *unit )
{
	AIPlayer::onUnitProduced(factory, unit);
}

/**
 * Search the computer player's buildings for one that can build the given request
 * and start training the unit.
 * If busyOK is true, it will queue a unit even if one is building.  This lets
 * script invoked teams "push" to the front of the queue.
 */
Bool AISkirmishPlayer::startTraining( WorkOrder *order, Bool busyOK, AsciiString teamName)
{
	Object *factory = findFactory(order->m_thing, busyOK);
	if( factory )
	{
		ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
		if (pu && pu->queueCreateUnit( order->m_thing, pu->requestUniqueUnitID() )) {
			order->m_factoryID = factory->getID();
			AISkirmishSimulation::aiLog(m_player, "train %s at %s for team %s (%d/%d)", order->m_thing->getName().str(), factory->getTemplate() ? factory->getTemplate()->getName().str() : "?", teamName.str(), order->m_numCompleted + 1, order->m_numRequired);
			if (TheGlobalData->m_debugAI) {
				AsciiString teamStr = "Queuing ";
				teamStr.concat(order->m_thing->getName());
				teamStr.concat(" for ");
				teamStr.concat(teamName);
				TheScriptEngine->AppendDebugMessage(teamStr, false);
			}
			return true;
		}
	}

	return FALSE;

}


/**
 * Check if this team is buildable, doesn't exceed maximum limits, meets conditions, and isn't under construction.
 */
Bool AISkirmishPlayer::isAGoodIdeaToBuildTeam( TeamPrototype *proto )
{
	// Check condition.
	if (!proto->evaluateProductionCondition()) {
		return false;
	}
	// check build limit
	if (proto->countTeamInstances() >= proto->getTemplateInfo()->m_maxInstances){
		if (TheGlobalData->m_debugAI) {
			AsciiString str;
			str.format("Team %s not chosen - %d already exist.", proto->getName().str(), proto->countTeamInstances());
			TheScriptEngine->AppendDebugMessage(str, false);
		}
		return false;	// Max already built.
	}

	for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
	{
		TeamInQueue *team = iter.cur();
		if (team->m_team->getPrototype() == proto) {
			return false; // currently building one of these.
		}
	}
	Bool needMoney;
	if (!isPossibleToBuildTeam( proto, true, needMoney)) {
		if (TheGlobalData->m_debugAI) {
			AsciiString str;
			if (needMoney) {
				str.format("Team %s not chosen - Not enough money.", proto->getName().str());
			} else {
				str.format("Team %s not chosen - Factory/tech missing or busy.", proto->getName().str());
			}
			TheScriptEngine->AppendDebugMessage(str, false);
		}
		return false;
	}
	return true;
}

/**
 * See if any existing teams need reinforcements, and have higher priority.
 */
Bool AISkirmishPlayer::selectTeamToReinforce( Int minPriority )
{
	return AIPlayer::selectTeamToReinforce(minPriority);
}

/**
 * Determine the next team to build.  Return true if one was selected.
 */
Bool AISkirmishPlayer::selectTeamToBuild()
{
	return AIPlayer::selectTeamToBuild();
}

/**
	Build a specific building.
	*/
void AISkirmishPlayer::buildSpecificAIBuilding(const AsciiString &thingName)
{
	//
	Bool found = false;
	Bool foundUnbuilt = false;
	for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
	{
		if (info->getTemplateName()==thingName)
		{
			AsciiString name = info->getTemplateName();
			if (name.isEmpty()) continue;
			const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( name );
			if (!bldgPlan) {
				DEBUG_LOG(("*** ERROR - Build list building '%s' doesn't exist.", name.str()));
				continue;
			}
			Object *bldg = TheGameLogic->findObjectByID( info->getObjectID() );
			found = true;
			if (bldg) {
				continue; // already built.
			}
			if (info->isPriorityBuild()) {
				continue; // already marked for priority build.
			}
			foundUnbuilt = true;
			info->markPriorityBuild();
			break;
		}
	}
	if (foundUnbuilt) {
		m_buildDelay = 0;
		AsciiString buildingStr = "Queueing building '";
		buildingStr.concat(thingName);
		buildingStr.concat("' for construction.");
		TheScriptEngine->AppendDebugMessage(buildingStr, false);
	}	else if (found) {
		AsciiString buildingStr = "Warning - all instances of building '";
		buildingStr.concat(thingName);
		buildingStr.concat("' are already built or queued for build, not queueing.");
		TheScriptEngine->AppendDebugMessage(buildingStr, false);
	}	else {
		AsciiString buildingStr = "Error - could not find building '";
		buildingStr.concat(thingName);
		buildingStr.concat("' in the building template list.");
		TheScriptEngine->AppendDebugMessage(buildingStr, false);
	}
}



/**
	Gets the player index of my enemy.
	*/
Int AISkirmishPlayer::getMyEnemyPlayerIndex() {
	Int playerNdx;
	if (m_currentEnemy) {
		return m_currentEnemy->getPlayerIndex();
	}
	// For now, return first human player, as there should only be one. jba
	for (playerNdx=0; playerNdx<ThePlayerList->getPlayerCount(); playerNdx++) {
		if (ThePlayerList->getNthPlayer(playerNdx)->getPlayerType() == PLAYER_HUMAN) {
			break;
		}
	}
	return playerNdx;
}

/**
	Get the AI's enemy.  Recalc if it has been a while (5 seconds.)
*/
void AISkirmishPlayer::acquireEnemy()
{
	Player *bestEnemy = nullptr;
	Real bestDistanceSqr = HUGE_DIST*HUGE_DIST;

	if (m_currentEnemy) {
		Bool inBadShape = !m_currentEnemy->hasAnyUnits() || !m_currentEnemy->hasAnyBuildFacility();
		if (!inBadShape) return;
	}

	// look for the closest enemy.
	Int i;
	for (i=0; i<ThePlayerList->getPlayerCount(); i++) {
		Player *curPlayer = ThePlayerList->getNthPlayer(i);
		if (m_player->getRelationship(curPlayer->getDefaultTeam()) == ENEMIES) {
			if (curPlayer->hasAnyObjects()==false) continue; // not much of an enemy.
			// ok, we got an enemy;
			// If a player is out of units, or out of build facilities, we can lower his priority.
			Bool inBadShape = !curPlayer->hasAnyUnits() || !curPlayer->hasAnyBuildFacility();

			Coord3D enemyPos = m_baseCenter;
			Region2D bounds;
			getPlayerStructureBounds(&bounds, i);
			enemyPos.x = bounds.lo.x + bounds.width()/2;
			enemyPos.y = bounds.lo.y + bounds.height()/2;
			Real curDistSqr = sqr(enemyPos.x-m_baseCenter.x) + sqr(enemyPos.y-m_baseCenter.y);

			//Fudge for in bad shape.	 If an enemy is crippled, concentrate on the other ones.
			if (inBadShape) {
				curDistSqr = HUGE_DIST*HUGE_DIST*0.5f;
			}
			// See if other ai's are attacking this target.
			// We don't want the ai's to gang up on one enemy.
			Int k;
			for (k=0; k<ThePlayerList->getPlayerCount(); k++) {
				if (k==i) continue;  // don't count self.
				Player *somePlayer = ThePlayerList->getNthPlayer(k);
				if (somePlayer->isSkirmishAIPlayer() && (somePlayer->getCurrentEnemy()==curPlayer)) {
					// Some ai is already targeting this guy.  Add a distance penalty.
					curDistSqr += (500*500);
				}
				if (somePlayer->isSkirmishAIPlayer() && (somePlayer->getCurrentEnemy()==m_player)) {
					// he is attacking me.  So I will (gently) prefer to attack him.
					curDistSqr -= (25*25);
					if (curDistSqr<0) curDistSqr = 0;
				}
			}

			// Ai enemy - will take if we don't get a better offer.
			if (curDistSqr<bestDistanceSqr) {
				bestEnemy = curPlayer;
				bestDistanceSqr = curDistSqr;
			}
		}
	}
	if (bestEnemy!=nullptr && (bestEnemy!=m_currentEnemy)) {
		m_currentEnemy = bestEnemy;
		AISkirmishSimulation::aiLog(m_player, "targeting enemy %s#%d", bestEnemy->getSide().str(), bestEnemy->getMpStartIndex() + 1);
		AsciiString msg = TheNameKeyGenerator->keyToName(m_player->getPlayerNameKey());
		msg.concat(" acquiring target enemy player: ");
		msg.concat(TheNameKeyGenerator->keyToName(m_currentEnemy->getPlayerNameKey()));
		TheScriptEngine->AppendDebugMessage( msg, false);
	}
}



/**
	Get the AI's enemy.  Recalc if it has been a while (20 seconds.)
*/
Player *AISkirmishPlayer::getAiEnemy()
{
	if (TheGameLogic->getFrame()>=m_frameToCheckEnemy) {
		m_frameToCheckEnemy = TheGameLogic->getFrame() + 5*LOGICFRAMES_PER_SECOND;
		acquireEnemy();
	}
	return m_currentEnemy;
}

/**
	Build base defense structures on the front or flank of the base.
*/
void AISkirmishPlayer::buildAIBaseDefense(Bool flank)
{
	const AISideInfo *resInfo = TheAI->getAiData()->m_sideInfo;
	AsciiString defenseTemplateName;
	while (resInfo) {
		if (resInfo->m_side == m_player->getSide()) {
			defenseTemplateName = resInfo->m_baseDefenseStructure1;
			break;
		}
		resInfo = resInfo->m_next;
	}
	if (resInfo) {
		buildAIBaseDefenseStructure(resInfo->m_baseDefenseStructure1, flank);
	}
}

/**
	Build base defense structures on the front or flank of the base.
	Base defenses are placed as follows:
	m_baseCenter and m_baseRadius are calculated on map load.
	Defenses are placed along the this circle.
	Front defenses (!flank) are placed starting at the "Center" approach path.
	The first front defense is placed towards th Center path.  Number 2 is placed
	to the left of #1, #3 is placed to the right of #1, #4 is placed to the left of
	#2 and so on.  So it looks like:

												#1
									 #2 			#3
					#6  #4								 #5	  #7
		  #8																	#9

	The flank base defenses cover the "Flank" approach, and the "Backdoor" approach.
	They alternate between these two, so the first flank defense covers flank, and the second
	covers backdoor, and continue to alternate.  They cover the approach using the same
	pattern as front above.
	John A.

	*/
void AISkirmishPlayer::buildAIBaseDefenseStructure(const AsciiString &thingName, Bool flank)
{
	AISkirmishSimulation::aiLog(m_player, "base defense %s (%s)", thingName.str(), flank ? "flank/backdoor" : "center");
	const ThingTemplate *tTemplate = TheThingFactory->findTemplate(thingName);
	if (tTemplate==nullptr) {
		DEBUG_CRASH(("Couldn't find base defense structure '%s' for side %s", thingName.str(), m_player->getSide().str()));
		return;
	}
	do {
		AsciiString pathLabel;
		if (flank) {
			if (m_curFlankBaseDefense&1) {
				pathLabel.format("%s%d", SKIRMISH_FLANK, m_player->getMpStartIndex()+1);
			}	else {
				pathLabel.format("%s%d", SKIRMISH_BACKDOOR, m_player->getMpStartIndex()+1);
			}
		}	else {
			pathLabel.format("%s%d", SKIRMISH_CENTER, m_player->getMpStartIndex()+1);
		}

		Coord3D goalPos = m_baseCenter;
		Waypoint *way = TheTerrainLogic->getClosestWaypointOnPath( &goalPos, pathLabel );
		if (way) {
			goalPos = *way->getLocation();
		} else {
			if (flank) return;
			Region2D bounds;
			getPlayerStructureBounds(&bounds, getMyEnemyPlayerIndex());
			goalPos.x = bounds.lo.x + bounds.width()/2;
			goalPos.y = bounds.lo.y + bounds.height()/2;
		}
		Coord2D offset;
		offset.x = goalPos.x-m_baseCenter.x;
		offset.y = goalPos.y-m_baseCenter.y;
		offset.normalize();
		Real defenseDistance = m_baseRadius;
		defenseDistance += TheAI->getAiData()->m_skirmishBaseDefenseExtraDistance;
		offset.x *= defenseDistance;
		offset.y *= defenseDistance;

		Real structureRadius = tTemplate->getTemplateGeometryInfo().getBoundingCircleRadius();
		Real baseCircumference = 2*PI*defenseDistance;
		Real angleOffset = 2*PI*WWMath::Div_FixNaN(structureRadius*4, baseCircumference, 0.0f);

		Int selector;
		Real angle;
		if (flank) {
			selector = m_curFlankBaseDefense>>1;
			if (m_curFlankBaseDefense&1) {
				if (selector&1) {
					m_curLeftFlankRightDefenseAngle -= angleOffset;
					angle = m_curLeftFlankRightDefenseAngle;
				}	else {
					angle = m_curLeftFlankLeftDefenseAngle;
					m_curLeftFlankLeftDefenseAngle += angleOffset;
				}
			}	else {
				if (selector&1) {
					m_curRightFlankRightDefenseAngle -= angleOffset;
					angle = m_curRightFlankRightDefenseAngle;
				}	else {
					angle = m_curRightFlankLeftDefenseAngle;
					m_curRightFlankLeftDefenseAngle += angleOffset;
				}
			}

		} else {
			selector = m_curFrontBaseDefense;
			if (selector&1) {
				m_curFrontRightDefenseAngle -= angleOffset;
				angle = m_curFrontRightDefenseAngle;
			}	else {
				angle = m_curFrontLeftDefenseAngle;
				m_curFrontLeftDefenseAngle += angleOffset;
			}
		}

		if (angle > PI/3) break;
		Real s = WWMath::SinTrig(angle);
		Real c = WWMath::CosTrig(angle);

// TheSuperHackers @info helmutbuhler 21/04/2025 This debug mutates the code to become CRC incompatible
#if defined(RTS_DEBUG) || !RETAIL_COMPATIBLE_CRC
		DEBUG_LOG(("buildAIBaseDefenseStructure -- Angle is %f sin %f, cos %f", 180*angle/PI, s, c));
		DEBUG_LOG(("buildAIBaseDefenseStructure -- Offset is %f  %f, Final Position is %f, %f",
			offset.x, offset.y,
			offset.x*c - offset.y*s,
			offset.y*c + offset.x*s
			));
#endif
		Coord3D buildPos = m_baseCenter;
		buildPos.x += offset.x*c - offset.y*s;
		buildPos.y += offset.y*c + offset.x*s;

		/* See if we can build there. */
		Bool canBuild;
		Real placeAngle = tTemplate->getPlacementViewAngle();
		canBuild = LBC_OK == TheBuildAssistant->isLocationLegalToBuild(&buildPos, tTemplate, placeAngle,
			BuildAssistant::TERRAIN_RESTRICTIONS|BuildAssistant::NO_OBJECT_OVERLAP, nullptr, m_player);
		TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback, turn it off.  jba.
		if (flank) {
			m_curFlankBaseDefense++;
		} else {
			m_curFrontBaseDefense++;
		}
		if (canBuild) {
			m_player->addToPriorityBuildList(thingName, &buildPos, placeAngle);
			break;
		}
	}	while (true);

}


/**
	Checks bridges along a waypoint path.  If any are destroyed, sends a dozer to fix, and returns true.
	If there is no bridge problem, returns false.
	*/
Bool AISkirmishPlayer::checkBridges(Object *unit, Waypoint *way)
{
	Coord3D unitPos = *unit->getPosition();
	AIUpdateInterface *ai = unit->getAI();
	if (!ai) return false; // no ai
	const LocomotorSet& locoSet = ai->getLocomotorSet();
	Waypoint *curWay;
	for (curWay = way; curWay; curWay = curWay->getNext()) {
		if (TheAI->pathfinder()->clientSafeQuickDoesPathExist(locoSet, &unitPos, curWay->getLocation())) {
			continue;
		}
		ObjectID brokenBridge = INVALID_ID;
		if (TheAI->pathfinder()->findBrokenBridge(locoSet, &unitPos, curWay->getLocation(), &brokenBridge)) {
			repairStructure(brokenBridge);
			return true;
		}
	}
	return false;

}


/**
	Build a specific team.  If priorityBuild, put at front of queue with priority set.
	*/
void AISkirmishPlayer::buildSpecificAITeam( TeamPrototype *teamProto, Bool priorityBuild)
{
	AIPlayer::buildSpecificAITeam(teamProto, priorityBuild);
}


/**
	Recruit a specific team, within the specific radius of the home position.
	*/
void AISkirmishPlayer::recruitSpecificAITeam(TeamPrototype *teamProto, Real recruitRadius)
{
	if (recruitRadius < 1) recruitRadius = 99999.0f;
	//
	// Create "Team in queue" based on team population
	//
	if (teamProto)
	{
		if (teamProto->getIsSingleton()) {
			Team *singletonTeam = TheTeamFactory->findTeam( teamProto->getName() );
			if (singletonTeam && singletonTeam->hasAnyObjects()) {
				AsciiString teamStr = "Unable to recruit singleton team '";
				teamStr.concat("' because team already exists.");
				TheScriptEngine->AppendDebugMessage(teamStr, false);
				return;
			}
		}
		if (!teamProto->getTemplateInfo()->m_hasHomeLocation)
		{
			AsciiString teamStr = "Error : team '";
			teamStr.concat(teamProto->getName());
			teamStr.concat("' has no Home Position (or Origin).");
			TheScriptEngine->AppendDebugMessage(teamStr, false);
		}
		// create inactive team to place members into as they are built
		// when team is complete, the team is activated
		Team *theTeam = TheTeamFactory->createInactiveTeam( teamProto->getName() );
		AsciiString teamName = teamProto->getName();
		teamName.concat(" - Recruiting.");
		TheScriptEngine->AppendDebugMessage(teamName, false);
		const TCreateUnitsInfo *unitInfo = &teamProto->getTemplateInfo()->m_unitsInfo[0];
//		WorkOrder *orders = nullptr;
		Int i;
		Int unitsRecruited = 0;
		// Recruit.
		for( i=0; i<teamProto->getTemplateInfo()->m_numUnitsInfo; i++ )
		{
			const ThingTemplate *thing = TheThingFactory->findTemplate( unitInfo[i].unitThingName );
			if (thing)
			{
				int count = unitInfo[i].maxUnits;
				while (count>0) {
					Object *unit = theTeam->tryToRecruit(thing, &teamProto->getTemplateInfo()->m_homeLocation, recruitRadius);
					if (unit)
					{
						unitsRecruited++;

						AsciiString teamStr = "Team '";
						teamStr.concat(theTeam->getPrototype()->getName());
						teamStr.concat("' recruits ");
						teamStr.concat(thing->getName());
						teamStr.concat(" from team '");
						teamStr.concat(unit->getTeam()->getPrototype()->getName());
						teamStr.concat("'");
						TheScriptEngine->AppendDebugMessage(teamStr, false);

						unit->setTeam(theTeam);

						AIUpdateInterface *ai = unit->getAIUpdateInterface();
						if (ai)
						{
#ifdef DEBUG_LOGGING
							Coord3D pos = *unit->getPosition();
							Coord3D to = teamProto->getTemplateInfo()->m_homeLocation;
							DEBUG_LOG(("Moving unit from %f,%f to %f,%f", pos.x, pos.y , to.x, to.y ));
#endif
							ai->aiMoveToPosition( &teamProto->getTemplateInfo()->m_homeLocation, CMD_FROM_AI);
						}
					} else {
						break;
					}
					count--;
				}
			}
		}
		if (unitsRecruited>0)
		{
			/* We have something to build. */
			TeamInQueue *team = newInstance(TeamInQueue);
			// Put in front of queue.
			prependTo_TeamReadyQueue(team);
			team->m_priorityBuild = false;
			team->m_workOrders = nullptr;
			team->m_frameStarted = TheGameLogic->getFrame();
			team->m_team = theTeam;
			AsciiString teamName = teamProto->getName();
			teamName.concat(" - Finished recruiting.");
			TheScriptEngine->AppendDebugMessage(teamName, false);
		}	else {
			//disband.
			if (!theTeam->getPrototype()->getIsSingleton()) {
				deleteInstance(theTeam);
				theTeam = nullptr;
			}
			AsciiString teamName = teamProto->getName();
			teamName.concat(" - Recruited 0 units, disbanding.");
			TheScriptEngine->AppendDebugMessage(teamName, false);
		}
	}
}




/**
 * Train our teams.
 */
void AISkirmishPlayer::processTeamBuilding()
{
	// select a new team
	if (selectTeamToBuild()) {
		queueUnits();
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if it's time to build another base building.
 */
void AISkirmishPlayer::doBaseBuilding()
{
	if (m_player->getCanBuildBase()) {
		// See if we are ready to start trying a structure.
		if (!m_readyToBuildStructure) {
			m_structureTimer--;
			if (m_structureTimer<=0) {
				m_readyToBuildStructure = true;
				m_buildDelay = 0;
			}
			if (m_structureTimer > 3*LOGICFRAMES_PER_SECOND) {
				m_structureTimer = 3*LOGICFRAMES_PER_SECOND;
			}
		}
		// This timer is to keep from banging on the logic each frame.  If something interesting
		// happens, like a building is added or a unit finished, the timers are shortcut.
		m_buildDelay--;
		if (m_buildDelay<1) {
			if (m_readyToBuildStructure) {
				processBaseBuilding();
			}
			if (m_buildDelay<1) {	// processBaseBuilding may reset m_buildDelay.
				m_buildDelay = 2*LOGICFRAMES_PER_SECOND; // check again in 2 seconds.
			}
			// Note that this timer gets shortcut when a building is completed.
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if any ready teams have finished moving to the rally point.
 */
void AISkirmishPlayer::checkReadyTeams()
{
	AIPlayer::checkReadyTeams();
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if any queued teams have finished building, or have run out of time.
 */
void AISkirmishPlayer::checkQueuedTeams()
{
	AIPlayer::checkQueuedTeams();
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if it is time to start another ai team building.
 */
void AISkirmishPlayer::doTeamBuilding()
{
	// See if any teams are expired.
	if (m_player->getCanBuildUnits()) {
		// See if we are ready to start trying a team.
		if (!m_readyToBuildTeam) {
			m_teamTimer--;
			if (m_teamTimer<=0) {
				m_readyToBuildTeam = true;
				m_teamDelay = 0;
			}
			if (m_teamTimer > 3*LOGICFRAMES_PER_SECOND) {
				m_teamTimer = 3*LOGICFRAMES_PER_SECOND;
			}
		}

		// This timer is to keep from banging on the logic each frame.  If something interesting
		// happens, like a building is added or a unit finished, the timers are shortcut.
		m_teamDelay--;
		if (m_teamDelay<1) {
			queueUnits(); // update the queues.
			if (m_readyToBuildTeam) {
				processTeamBuilding();
			}
			m_teamDelay = 2*LOGICFRAMES_PER_SECOND; // check again in 5 seconds.
			// Note that this timer gets shortcut when a unit or building is completed.
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * Perform computer-controlled player AI
 */
// GeneralsX @feature Claude 20/10/2026 Autopilot: toggle build-only skirmish AI for a human player.
void AISkirmishPlayer::update()
{
	if (m_autopilotOnly)
	{
		// Autopilot: build-only AI for a human player: keep the base growing and the economy running, but no army
		// teams, no attack waves, no skills or upgrades (those stay the player's decisions).
		autopilotQueueNextStructure();
		doBaseBuilding();
		checkReadyTeams();
		checkQueuedTeams();
		m_teamDelay--;
		if (m_teamDelay < 1)
		{
			queueUnits();	// queues a supply truck when short, and trains any queued truck/dozer orders
			autopilotTrainDefenders();	// wallet mode only, and only under attack
			autopilotExplore();			// explore mode only
			m_teamDelay = 2*LOGICFRAMES_PER_SECOND;
		}
		return;
	}
	AIPlayer::update();
}

// GeneralsX @feature Claude 20/10/2026 Autopilot: toggle build-only skirmish AI for a human player.
// GeneralsX @feature Claude 20/10/2026 Autopilot: say why a queued structure was never placed, so a stalled build
// does not need a debugger session: the dozer, the money, the safety check and both legality checks the placement
// code runs.
void AISkirmishPlayer::autopilotExplainStall(BuildListInfo *info)
{
	if (!lanNetplayOptions().m_active)
		return;
	const ThingTemplate *plan = TheThingFactory->findTemplate(info->getTemplateName());
	if (!plan)
		return;
	Object *dozer = findDozer(info->getLocation());
	Coord3D pos = *info->getLocation();
	pos.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
	Int canMake = -1, legalEnemy = -1, legalTerrain = -1;
	if (dozer)
	{
		canMake = (Int)TheBuildAssistant->canMakeUnit(dozer, plan);
		legalEnemy = (Int)TheBuildAssistant->isLocationLegalToBuild(&pos, plan, info->getAngle(), BuildAssistant::NO_ENEMY_OBJECT_OVERLAP, dozer, m_player);
		legalTerrain = (Int)TheBuildAssistant->isLocationLegalToBuild(&pos, plan, info->getAngle(),
			BuildAssistant::CLEAR_PATH | BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, dozer, m_player);
		TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback
	}
	printf("NETPLAY autopilot stall %s at (%.0f,%.0f): dozer=%d canMake=%d money=%u cost=%u safe=%d legalEnemy=%d legalTerrain=%d ready=%d (frame %u)\n",
		info->getTemplateName().str(), pos.x, pos.y, dozer ? (Int)dozer->getID() : -1, canMake,
		m_player->getMoney()->countMoney(), plan->calcCostToBuild(m_player), (Int)isLocationSafe(info->getLocation(), plan),
		legalEnemy, legalTerrain, (Int)m_readyToBuildStructure, TheGameLogic->getFrame());
	printf("NETPLAY autopilot stall entry: priority=%d objectID=%d timestamp=%d rebuilds=%d buildable=%d canBuildBase=%d buildDelay=%d structureTimer=%d underPowered=%d\n",
		(Int)info->isPriorityBuild(), (Int)info->getObjectID(), (Int)info->getObjectTimestamp(), (Int)info->getNumRebuilds(), (Int)info->isBuildable(),
		(Int)m_player->getCanBuildBase(), (Int)m_buildDelay, (Int)m_structureTimer, (Int)!m_player->getEnergy()->hasSufficientPower());
	fflush(stdout);
}

// GeneralsX @feature Claude 20/10/2026 Autopilot: toggle build-only skirmish AI for a human player.
// A computer player gets its build order from the skirmish scripts, which queue structures one at a time through
// buildSpecificAIBuilding(). A human with autopilot has no such scripts, so this picks the next structure itself:
// walk the faction's build list in order and queue the first one that is missing, allowed and affordable right now.
// The autopilot level (the AI skill state) caps how many of each structure it will build; power plants are added
// only when the base is short of power. processBaseBuilding() then places it with a dozer like any other build.
void AISkirmishPlayer::autopilotQueueNextStructure()
{
	if (m_autopilotPending)
	{
		if (m_autopilotPending->getObjectID() != INVALID_ID)
			m_autopilotPending = nullptr;	// placed; processBaseBuilding() owns it from here
		else if (TheGameLogic->getFrame() - m_autopilotPendingFrame < 40 * LOGICFRAMES_PER_SECOND)
			return;	// still waiting for a dozer to start it
		else
		{
			// Gave up on it (no legal spot, no money, no dozer): drop the entry for good and plan something else.
			autopilotExplainStall(m_autopilotPending);
			m_autopilotPending->clearPriorityBuild();
			m_autopilotPending->setNumRebuilds(0);
			m_autopilotPending = nullptr;
		}
	}
	if (!m_readyToBuildStructure)
		return;

	for (BuildListInfo *supplyInfo = m_player->getBuildList(); supplyInfo; supplyInfo = supplyInfo->getNext())
		if (supplyInfo->isSupplyBuilding())
			supplyInfo->setDesiredGatherers(autopilotGatherers());	// follows the level if it changes

	// GeneralsX @feature Claude 20/10/2026 Autopilot wallet mode: defend, survive, hoard money.
	// Wallet mode builds only the economy (supply, cash generators, power) and a few base defenses, and never spends
	// into the reserve on defenses, so the rest of the money stays in the bank.
	const Bool wallet = m_autopilotWallet;
	const Int cap = wallet ? 1 : ((m_difficulty == DIFFICULTY_EASY) ? 1 : ((m_difficulty == DIFFICULTY_NORMAL) ? 2 : 3));
	// Under threat (enemy units near the base) the bank is there to be spent: many more defenses, almost no reserve.
	const Bool threatened = wallet && autopilotIsThreatened();
	const Int walletDefenseCap = threatened ? 6 : 2;				// per defense template
	const UnsignedInt bankNow = m_player->getMoney()->countMoney();
	const Int walletDefenseTotal = threatened ? 12 + (Int)(bankNow / 3000 < 12 ? bankNow / 3000 : 12) : 4;				// all defenses together
	const UnsignedInt walletReserve = threatened ? 300 : 1500;		// defenses are only started if this much stays in the bank
	Int defensesBuilt = 0;
	if (wallet)
		for (BuildListInfo *d = m_player->getBuildList(); d; d = d->getNext())
		{
			const ThingTemplate *dt = TheThingFactory->findTemplate(d->getTemplateName());
			if (dt && dt->isKindOf(KINDOF_FS_BASE_DEFENSE) && TheGameLogic->findObjectByID(d->getObjectID()))
				++defensesBuilt;
		}
	const Bool underPowered = !m_player->getEnergy()->hasSufficientPower();
	const UnsignedInt money = m_player->getMoney()->countMoney();

	Bool buildingUp = FALSE;	// wallet phase 1: build the foundation first; hoarding starts after it
	if (wallet)
	{
		// The supply center is not in the faction's build list; production buildings that need it are skipped in wallet
		// mode, so nothing would ever pull it in. Without income there is nothing to hoard: queue it through the
		// first list entry that lists it as a prerequisite.
		Bool hasSupply = FALSE;
		for (Object *obj = TheGameLogic->getFirstObject(); obj && !hasSupply; obj = obj->getNextObject())
			if (obj->getControllingPlayer() == m_player && (obj->isKindOf(KINDOF_FS_SUPPLY_CENTER) || obj->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)))
				hasSupply = TRUE;
		if (!hasSupply)
		{
			for (BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext())
			{
				const ThingTemplate *dependent = TheThingFactory->findTemplate(info->getTemplateName());
				if (dependent && (autopilotQueueMissingPrerequisite(dependent, money, 0, KINDOF_FS_SUPPLY_CENTER)
					|| autopilotQueueMissingPrerequisite(dependent, money, 0, KINDOF_FS_SUPPLY_DROPZONE)))
					return;
			}
		}

		// Hoarding only pays off once the foundation is up: until the faction's advanced tech building (strategy
		// center, propaganda center, palace) exists, spend on the way to it, prerequisites and power included.
		Bool hasAdvanced = FALSE;
		for (Object *obj = TheGameLogic->getFirstObject(); obj && !hasAdvanced; obj = obj->getNextObject())
			if (obj->getControllingPlayer() == m_player && obj->isKindOf(KINDOF_FS_ADVANCED_TECH))
				hasAdvanced = TRUE;
		BuildListInfo *advancedEntry = nullptr;
		if (!hasAdvanced)
			for (BuildListInfo *info = m_player->getBuildList(); info && !advancedEntry; info = info->getNext())
			{
				const ThingTemplate *at = TheThingFactory->findTemplate(info->getTemplateName());
				if (at && at->isKindOf(KINDOF_FS_ADVANCED_TECH) && info->isBuildable() && !TheGameLogic->findObjectByID(info->getObjectID()))
					advancedEntry = info;
			}
		buildingUp = (advancedEntry != nullptr) && !threatened;	// an attack ends the build-up: defend first
		if (buildingUp && !underPowered)
		{
			const ThingTemplate *advanced = TheThingFactory->findTemplate(advancedEntry->getTemplateName());
			if (Object *advDozer = findDozer(advancedEntry->getLocation()))
			{
				const CanMakeType advCanMake = TheBuildAssistant->canMakeUnit(advDozer, advanced);
				if (advCanMake == CANMAKE_OK)
				{
					if (isLocationSafe(advancedEntry->getLocation(), advanced))
					{
						advancedEntry->markPriorityBuild();
						m_autopilotPending = advancedEntry;
						m_autopilotPendingFrame = TheGameLogic->getFrame();
						m_buildDelay = 0;
						if (lanNetplayOptions().m_active)
						{
							printf("NETPLAY autopilot wallet foundation: %s (frame %u)\n", advanced->getName().str(), TheGameLogic->getFrame());
							fflush(stdout);
						}
						return;
					}
				}
				else if (advCanMake == CANMAKE_NO_PREREQ && autopilotQueueMissingPrerequisite(advanced, money, 0))
					return;
			}
			else
			{
				queueDozer();
				return;
			}
		}
	}

	if (!buildingUp && autopilotQueueSupplyExpansion(money))
		return;

	// Candidates: entries that are missing, allowed by the level cap and the power rule. Cheapest first, so the basic
	// economy and production come before tech and superweapons (which are first in some factions' lists).
	BuildListInfo *pick = nullptr;
	UnsignedInt pickCost = 0;
	BuildListInfo *blocked = nullptr;	// cheapest candidate that is only missing a prerequisite building
	UnsignedInt blockedCost = 0;
	Object *dozer = nullptr;

	// GeneralsX @feature Claude 20/10/2026 Count the structures the player really owns, so ones built by hand count towards the
	// per-level caps (the planner used to see only its own build-list entries and would build a duplicate).
	AsciiString ownedNames[96];
	Int ownedCounts[96];
	Int numOwned = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != m_player || !obj->isKindOf(KINDOF_STRUCTURE))
			continue;
		const AsciiString tmplName = obj->getTemplate()->getName();
		Int slot = 0;
		while (slot < numOwned && !(ownedNames[slot] == tmplName))
			++slot;
		if (slot == numOwned)
		{
			if (numOwned >= 96)
				continue;
			ownedNames[numOwned] = tmplName;
			ownedCounts[numOwned] = 0;
			++numOwned;
		}
		++ownedCounts[slot];
	}

	for (BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext())
	{
		const AsciiString name = info->getTemplateName();
		if (name.isEmpty() || !info->isBuildable())
			continue;
		const ThingTemplate *plan = TheThingFactory->findTemplate(name);
		if (!plan)
			continue;
		const Bool isDefense = plan->isKindOf(KINDOF_FS_BASE_DEFENSE);
		if (isDefense && (!wallet || buildingUp))
			continue;	// wallet defenses start after the foundation is up
		if (TheGameLogic->findObjectByID(info->getObjectID()))
			continue;	// this entry is built

		const Bool isPower = plan->isKindOf(KINDOF_FS_POWER) && !plan->isKindOf(KINDOF_CASH_GENERATOR);
		if (isPower != underPowered)
			continue;	// short of power: only power plants; otherwise no extra ones
		const Bool isEconomy = plan->isKindOf(KINDOF_FS_SUPPLY_CENTER) || plan->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)
			|| plan->isKindOf(KINDOF_CASH_GENERATOR);
		if (wallet && !isPower && !isEconomy && !isDefense)
			continue;	// wallet mode: no production, tech or superweapon buildings
		if (wallet && isDefense && defensesBuilt >= walletDefenseTotal)
			continue;

		Int have = 0;	// what the player owns of this structure, whoever built it
		for (Int o = 0; o < numOwned; ++o)
			if (ownedNames[o] == name)
				have = ownedCounts[o];
		if (!isPower && have >= (isDefense ? walletDefenseCap : cap))
			continue;

		if (!dozer)
			dozer = findDozer(info->getLocation());
		if (!dozer)
		{
			queueDozer();
			return;
		}
		const UnsignedInt cost = plan->calcCostToBuild(m_player);
		if (wallet && isDefense && money < cost + walletReserve)
			continue;	// hoarding: keep the reserve
		const CanMakeType canMake = TheBuildAssistant->canMakeUnit(dozer, plan);
		if (canMake == CANMAKE_OK)
		{
			if (isLocationSafe(info->getLocation(), plan) && (!pick || cost < pickCost))
			{
				pick = info;
				pickCost = cost;
			}
		}
		// Wallet defenses that need a prerequisite building (a barracks for the GLA tunnel network, say) are only worth the
		// extra spend once the bank is comfortably full.
		else if (canMake == CANMAKE_NO_PREREQ && !(wallet && isDefense && money < cost + walletReserve + (threatened ? 0 : 3000)) && (!blocked || cost < blockedCost))
		{
			blocked = info;
			blockedCost = cost;
		}
	}
	if (!pick && blocked)
	{
		// Nothing is buildable yet: queue the first missing prerequisite building of the cheapest blocked candidate.
		const ThingTemplate *plan = TheThingFactory->findTemplate(blocked->getTemplateName());
		if (plan && autopilotQueueMissingPrerequisite(plan, money, 0))
			return;
	}

	if (pick)
	{
		pick->markPriorityBuild();
		m_autopilotPending = pick;
		m_autopilotPendingFrame = TheGameLogic->getFrame();
		m_buildDelay = 0;
		if (lanNetplayOptions().m_active)
		{
			printf("NETPLAY autopilot queues %s cost=%u money=%u (frame %u)\n", pick->getTemplateName().str(), pickCost, money, TheGameLogic->getFrame());
			fflush(stdout);
		}
	}
}

// GeneralsX @feature Claude 20/10/2026 Autopilot wallet mode: enemy units near the base.
Bool AISkirmishPlayer::autopilotIsThreatened()
{
	const ThingTemplate *anchor = nullptr;
	for (Object *obj = TheGameLogic->getFirstObject(); obj && !anchor; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == m_player && obj->isKindOf(KINDOF_COMMANDCENTER))
			anchor = obj->getTemplate();
	return anchor && !isLocationSafe(&m_baseCenter, anchor);
}

// Wallet mode under attack: spend the bank on a few defenders. Every so often (called every two seconds) one affordable combat unit
// is queued at a barracks or war factory, up to a cap on the army; idle units defend themselves, so nothing here ever attacks.
void AISkirmishPlayer::autopilotTrainDefenders()
{
	if (!m_autopilotWallet || !autopilotIsThreatened())
		return;
	UnsignedInt money = m_player->getMoney()->countMoney();
	const Int armyCap = 16 + (Int)(money / 1500 < 40 ? money / 1500 : 40);	// a fuller bank buys a bigger defending army
	Int army = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == m_player && !obj->isKindOf(KINDOF_STRUCTURE) && obj->isKindOf(KINDOF_CAN_ATTACK)
			&& !obj->isKindOf(KINDOF_HARVESTER) && !obj->isKindOf(KINDOF_DOZER))
			++army;
	static UnsignedInt s_lastTraceFrame = 0;	// logging only
	Bool trace = FALSE;
	if (lanNetplayOptions().m_active && TheGameLogic->getFrame() >= s_lastTraceFrame + 300)	// every 10 s of game time
	{
		trace = TRUE;
		s_lastTraceFrame = TheGameLogic->getFrame();
	}
	if (army >= armyCap)
	{
		if (trace) { printf("NETPLAY autopilot defenders declined: army %d at cap %d (frame %u)\n", army, armyCap, TheGameLogic->getFrame()); fflush(stdout); }
		return;
	}

	Int queuedNow = 0;
	Int factories = 0, queueFull = 0, nothingToBuild = 0;
	for (Object *factory = TheGameLogic->getFirstObject(); factory; factory = factory->getNextObject())
	{
		if (factory->getControllingPlayer() != m_player || !(factory->isKindOf(KINDOF_FS_BARRACKS) || factory->isKindOf(KINDOF_FS_WARFACTORY)))
			continue;
		if (factory->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
		if (!pu)
			continue;
		++factories;
		if (pu->getProductionCount() >= 4)
			++queueFull;
		while (pu->getProductionCount() < 4 && queuedNow < 4 && army + queuedNow < armyCap)
		{
			// The strongest (most expensive) combat unit this factory can build that leaves a small reserve and is at most a quarter of the bank.
			const ThingTemplate *best = nullptr;
			UnsignedInt bestCost = 0;
			for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
			{
				if (t->isKindOf(KINDOF_STRUCTURE) || !t->isKindOf(KINDOF_CAN_ATTACK) || t->isKindOf(KINDOF_HARVESTER) || t->isKindOf(KINDOF_DOZER))
					continue;
				const UnsignedInt cost = t->calcCostToBuild(m_player);
				if (cost == 0 || cost + 300 > money || cost > money / 4 + 400 || cost <= bestCost)
					continue;
				if (TheBuildAssistant->canMakeUnit(factory, t) != CANMAKE_OK)
					continue;
				best = t;
				bestCost = cost;
			}
			if (!best || !pu->queueCreateUnit(best, pu->requestUniqueUnitID()))
			{
				if (!best)
					++nothingToBuild;
				break;	// nothing affordable or buildable here: next factory
			}
			money = money > bestCost ? money - bestCost : 0;
			++queuedNow;
			if (lanNetplayOptions().m_active)
			{
				printf("NETPLAY autopilot defender: %s (cost %u) at %s (frame %u)\n", best->getName().str(), bestCost, factory->getTemplate()->getName().str(), TheGameLogic->getFrame());
				fflush(stdout);
			}
		}	// while the factory has queue room
	}
	if (trace && queuedNow == 0)
	{
		printf("NETPLAY autopilot defenders none queued: army %d/%d, %d factories (%d queue full, %d nothing affordable), bank %u (frame %u)\n", army, armyCap, factories, queueFull, nothingToBuild, money, TheGameLogic->getFrame());
		fflush(stdout);
	}
}

// GeneralsX @feature Claude 21/10/2026 Autopilot explore mode (level 5): normal autopilot plus explorers. An explorer is an infantry unit
// that can capture buildings (Ranger, Red Guard, Rebel). One is trained at a barracks, then every idle explorer is sent, with the game's
// own capture special power, to the nearest safe neutral capturable building (oil derricks and the other tech buildings); a building it
// fails to take (unreachable, destroyed first) is skipped for a while. Nothing here attacks an enemy.
void AISkirmishPlayer::autopilotExplore()
{
	if (!m_autopilotExplore)
		return;
	const UnsignedInt frame = TheGameLogic->getFrame();
	const Int wantedExplorers = 2;

	// The explorers we have, and forget errands whose explorer is idle again (done, or failed).
	Object *explorers[8];
	Int numExplorers = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj && numExplorers < 8; obj = obj->getNextObject())
		if (obj->getControllingPlayer() == m_player && !obj->isKindOf(KINDOF_STRUCTURE) && obj->hasSpecialPower(SPECIAL_INFANTRY_CAPTURE_BUILDING))
			explorers[numExplorers++] = obj;

	for (Int e = 0; e < 8; ++e)
	{
		if (m_exploreExplorer[e] == INVALID_ID)
			continue;
		Object *who = TheGameLogic->findObjectByID(m_exploreExplorer[e]);
		AIUpdateInterface *whoAI = who ? who->getAIUpdateInterface() : nullptr;
		if (who && whoAI && !whoAI->isIdle())
			continue;	// still on its errand
		Object *target = TheGameLogic->findObjectByID(m_exploreTarget[e]);
		const Bool stillHidden = target && target->getShroudedStatus(m_player->getPlayerIndex()) != OBJECTSHROUD_CLEAR;
		if (who && target && target->getControllingPlayer() != m_player && (!m_exploreScouting[e] || stillHidden))
		{
			// Idle again but the building is not ours (a scouting walk that did not reveal it counts as a failure too): skip that
			// building for five minutes.
			for (Int b = 0; b < 16; ++b)
				if (m_exploreBlacklist[b] == INVALID_ID || m_exploreBlacklistUntil[b] <= frame)
				{
					m_exploreBlacklist[b] = m_exploreTarget[e];
					m_exploreBlacklistUntil[b] = frame + 300 * LOGICFRAMES_PER_SECOND;
					break;
				}
		}
		m_exploreExplorer[e] = INVALID_ID;
		m_exploreTarget[e] = INVALID_ID;
		m_exploreScouting[e] = FALSE;
	}

	// Infantry cannot capture anything until the player has bought the faction's "Capture Building" upgrade (the same player upgrade for
	// the Ranger, Red Guard and Rebel); buildUpgrade() finds a barracks queue for it and ignores a request it cannot place.
	static const char *const captureUpgradeName = "Upgrade_InfantryCaptureBuilding";
	const UpgradeTemplate *captureUpgrade = TheUpgradeCenter->findUpgrade(captureUpgradeName);
	if (captureUpgrade && !m_player->hasUpgradeComplete(captureUpgrade) && !m_player->hasUpgradeInProduction(captureUpgrade)
		&& TheUpgradeCenter->canAffordUpgrade(m_player, captureUpgrade))
	{
		buildUpgrade(captureUpgradeName);
		if (lanNetplayOptions().m_active && m_player->hasUpgradeInProduction(captureUpgrade))
		{
			printf("NETPLAY autopilot explore: capture upgrade queued (frame %u)\n", frame);
			fflush(stdout);
		}
	}

	static UnsignedInt s_lastStatusFrame = 0;	// logging only
	if (lanNetplayOptions().m_active && frame >= s_lastStatusFrame + 600)
	{
		s_lastStatusFrame = frame;
		printf("NETPLAY autopilot explore status: %d explorers, capture upgrade %s (frame %u)\n", numExplorers,
			!captureUpgrade ? "unknown" : (m_player->hasUpgradeComplete(captureUpgrade) ? "done" : (m_player->hasUpgradeInProduction(captureUpgrade) ? "in production" : "not started")), frame);
		fflush(stdout);
	}

	// Train one when short: the strongest affordable capture-capable infantry at a finished barracks with an empty queue.
	if (numExplorers < wantedExplorers)
	{
		const UnsignedInt money = m_player->getMoney()->countMoney();
		for (Object *barracks = TheGameLogic->getFirstObject(); barracks; barracks = barracks->getNextObject())
		{
			if (barracks->getControllingPlayer() != m_player || !barracks->isKindOf(KINDOF_FS_BARRACKS) || barracks->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
				continue;
			ProductionUpdateInterface *pu = barracks->getProductionUpdateInterface();
			if (!pu || pu->getProductionCount() != 0)
				continue;
			const ThingTemplate *pick = nullptr;
			for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
			{
				if (!t->isKindOf(KINDOF_INFANTRY) || !t->isKindOf(KINDOF_CAN_ATTACK))
					continue;
				AsciiString lower = t->getName();
				lower.toLower();
				if (!strstr(lower.str(), "ranger") && !strstr(lower.str(), "redguard") && !strstr(lower.str(), "rebel"))
					continue;
				if (t->calcCostToBuild(m_player) + 500 > money || TheBuildAssistant->canMakeUnit(barracks, t) != CANMAKE_OK)
					continue;
				pick = t;
				break;
			}
			if (pick && pu->queueCreateUnit(pick, pu->requestUniqueUnitID()))
			{
				if (lanNetplayOptions().m_active)
				{
					printf("NETPLAY autopilot explore: training %s (frame %u)\n", pick->getName().str(), frame);
					fflush(stdout);
				}
				break;
			}
		}
	}

	// Send every idle explorer to the nearest safe, neutral, capturable building that nobody is already heading for.
	for (Int i = 0; i < numExplorers; ++i)
	{
		Object *explorer = explorers[i];
		AIUpdateInterface *ai = explorer->getAIUpdateInterface();
		if (!ai || !ai->isIdle())
			continue;
		Int slot = -1;
		for (Int e = 0; e < 8 && slot < 0; ++e)
			if (m_exploreExplorer[e] == INVALID_ID)
				slot = e;
		if (slot < 0)
			break;

		Object *best = nullptr;
		Real bestDistSqr = 0;
		Object *hidden = nullptr;	// nearest neutral building still in the shroud: somewhere to scout when nothing can be captured yet
		Real hiddenDistSqr = 0;
		Int seen = 0, notNeutral = 0, takenOrSkipped = 0, cannotCapture = 0, unsafe = 0, shrouded = 0, notReady = 0;	// for the trace below
		for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (!obj->isKindOf(KINDOF_STRUCTURE) || !obj->isKindOf(KINDOF_CAPTURABLE) || obj->isKindOf(KINDOF_REBUILD_HOLE))
				continue;
			++seen;
			// Only buildings nobody owns: the game's neutral player (tech buildings, oil derricks, ...). The relationship toward such a
			// building is not NEUTRAL, so the owner is what is checked; a player's own, allied or enemy buildings are never targets.
			if (obj->getControllingPlayer() != ThePlayerList->getNeutralPlayer())
			{
				++notNeutral;
				continue;	// ours, an ally's or an enemy's (never attack an enemy)
			}
			Bool taken = FALSE;
			for (Int e = 0; e < 8 && !taken; ++e)
				taken = (m_exploreTarget[e] == obj->getID());
			for (Int b = 0; b < 16 && !taken; ++b)
				taken = (m_exploreBlacklist[b] == obj->getID() && m_exploreBlacklistUntil[b] > frame);
			if (taken)
			{
				++takenOrSkipped;
				continue;
			}
			if (!TheActionManager->canCaptureBuilding(explorer, obj, CMD_FROM_AI))
			{
				++cannotCapture;
				SpecialPowerModuleInterface *spi = explorer->findSpecialPowerModuleInterface(SPECIAL_INFANTRY_CAPTURE_BUILDING);
				if (spi && spi->getPercentReady() < 1.0f)
					++notReady;
				else if (obj->getShroudedStatus(m_player->getPlayerIndex()) != OBJECTSHROUD_CLEAR)
				{
					++shrouded;
					if (isLocationSafe(obj->getPosition(), obj->getTemplate()))
					{
						const Real hx = obj->getPosition()->x - explorer->getPosition()->x;
						const Real hy = obj->getPosition()->y - explorer->getPosition()->y;
						if (!hidden || hx * hx + hy * hy < hiddenDistSqr)
						{
							hidden = obj;
							hiddenDistSqr = hx * hx + hy * hy;
						}
					}
				}
				continue;
			}
			if (!isLocationSafe(obj->getPosition(), obj->getTemplate()))
			{
				++unsafe;
				continue;
			}
			const Real dx = obj->getPosition()->x - explorer->getPosition()->x;
			const Real dy = obj->getPosition()->y - explorer->getPosition()->y;
			const Real distSqr = dx * dx + dy * dy;
			if (!best || distSqr < bestDistSqr)
			{
				best = obj;
				bestDistSqr = distSqr;
			}
		}
		if (!best && hidden)
		{
			// Walk to the hidden building: seeing it lifts the shroud, and the next cycle can then order the capture.
			ai->aiMoveToPosition(hidden->getPosition(), CMD_FROM_AI);
			m_exploreExplorer[slot] = explorer->getID();
			m_exploreTarget[slot] = hidden->getID();
			m_exploreOrderFrame[slot] = frame;
			m_exploreScouting[slot] = TRUE;
			if (lanNetplayOptions().m_active)
			{
				printf("NETPLAY autopilot explore: %s -> scout %s at (%.0f,%.0f) (frame %u)\n", explorer->getTemplate()->getName().str(),
					hidden->getTemplate()->getName().str(), hidden->getPosition()->x, hidden->getPosition()->y, frame);
				fflush(stdout);
			}
			continue;
		}
		if (!best)
		{
			static UnsignedInt s_lastExploreTrace = 0;	// logging only
			if (lanNetplayOptions().m_active && frame >= s_lastExploreTrace + 600)
			{
				s_lastExploreTrace = frame;
				printf("NETPLAY autopilot explore: nothing to capture: %d capturable buildings seen (%d not neutral, %d taken/skipped, %d cannot capture [%d special not ready, %d in the shroud], %d unsafe) (frame %u)\n", seen, notNeutral, takenOrSkipped, cannotCapture, notReady, shrouded, unsafe, frame);
				fflush(stdout);
			}
			continue;
		}

		SpecialPowerModuleInterface *capture = explorer->findSpecialPowerModuleInterface(SPECIAL_INFANTRY_CAPTURE_BUILDING);
		if (!capture || !capture->getSpecialPowerTemplate())
			continue;
		AIGroupPtr group = TheAI->createGroup();
		if (!group)
			continue;
		group->add(explorer);
		group->groupDoSpecialPowerAtObject(capture->getSpecialPowerTemplate()->getID(), best, 0);
#if RETAIL_COMPATIBLE_AIGROUP
		TheAI->destroyGroup(group);
#else
		group->removeAll();
#endif
		m_exploreExplorer[slot] = explorer->getID();
		m_exploreScouting[slot] = FALSE;
		m_exploreTarget[slot] = best->getID();
		m_exploreOrderFrame[slot] = frame;
		if (lanNetplayOptions().m_active)
		{
			printf("NETPLAY autopilot explore: %s -> capture %s at (%.0f,%.0f) (frame %u)\n", explorer->getTemplate()->getName().str(),
				best->getTemplate()->getName().str(), best->getPosition()->x, best->getPosition()->y, frame);
			fflush(stdout);
		}
	}
}

// GeneralsX @feature Claude 20/10/2026 Autopilot: desired supply gatherers per level (Easy 2, Normal 3, Hard 4).
Int AISkirmishPlayer::autopilotGatherers() const
{
	return m_difficulty == DIFFICULTY_EASY ? 2 : (m_difficulty == DIFFICULTY_NORMAL ? 3 : 4);
}

// Finds a legal spot for a supply center next to 'warehouse': a supply center may not stand too close to a supply source,
// and the trucks only reach a center within SUPPLY_CENTER_CLOSE_DIST (20 cells) of one, so walk from the warehouse toward
// the base and take the first spot the placement code accepts. Returns false (and leaves 'out') if there is none.
Bool AISkirmishPlayer::autopilotFindSupplySpot(const ThingTemplate *need, Object *dozer, const Object *warehouse, Coord3D *out)
{
	Coord2D toBase;
	toBase.x = m_baseCenter.x - warehouse->getPosition()->x;
	toBase.y = m_baseCenter.y - warehouse->getPosition()->y;
	toBase.normalize();
	const Coord3D warehousePos = *warehouse->getPosition();
	const Real viewAngle = need->getPlacementViewAngle();
	// Closest spots first; at each distance try the side facing the base, then fan out around the warehouse (the straight
	// line to the base is often blocked by the warehouse's own supply piles or by terrain).
	static const Real fan[] = { 0.0f, PI / 4, -PI / 4, PI / 2, -PI / 2, 3 * PI / 4, -3 * PI / 4, PI };
	Bool found = FALSE;
	for (Int cells = 6; cells <= 20 && !found; ++cells)
	{
		for (Int f = 0; f < (Int)(sizeof(fan) / sizeof(fan[0])) && !found; ++f)
		{
			const Real c = WWMath::CosTrig(fan[f]);
			const Real s = WWMath::SinTrig(fan[f]);
			Coord3D candidate = warehousePos;
			candidate.x += (toBase.x * c - toBase.y * s) * cells * PATHFIND_CELL_SIZE_F;
			candidate.y += (toBase.x * s + toBase.y * c) * cells * PATHFIND_CELL_SIZE_F;
			candidate.z = TheTerrainLogic->getGroundHeight(candidate.x, candidate.y);
			if (TheBuildAssistant->isLocationLegalToBuild(&candidate, need, viewAngle, BuildAssistant::NO_ENEMY_OBJECT_OVERLAP, dozer, m_player) == LBC_OK &&
				TheBuildAssistant->isLocationLegalToBuild(&candidate, need, viewAngle,
					BuildAssistant::CLEAR_PATH | BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, dozer, m_player) == LBC_OK)
			{
				*out = candidate;
				found = TRUE;
			}
		}
	}
	TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback
	return found;
}

// A second supply center at another warehouse once the first one is running and there is money to spare. The template is
// whatever supply center the player already owns, so this works for every faction.
Bool AISkirmishPlayer::autopilotQueueSupplyExpansion(UnsignedInt money)
{
	const char *why = nullptr;
	auto decline = [&](const char *reason) {
		if (lanNetplayOptions().m_active && TheGameLogic->getFrame() % 600 == 0)
		{
			printf("NETPLAY autopilot supply expansion declined: %s (frame %u)\n", reason, TheGameLogic->getFrame());
			fflush(stdout);
		}
		return FALSE;
	};
	(void)why;
	const Int centerCap = (m_autopilotWallet || m_difficulty == DIFFICULTY_HARD) ? 2 : 1;
	const ThingTemplate *centerTemplate = nullptr;
	Int ownedCenters = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != m_player || !(obj->isKindOf(KINDOF_FS_SUPPLY_CENTER) || obj->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)))
			continue;
		++ownedCenters;
		if (!obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			centerTemplate = obj->getTemplate();
	}
	if (!centerTemplate)
		return decline("no finished supply center yet");
	if (ownedCenters >= centerCap)
		return decline("at the center cap");
	if (money < centerTemplate->calcCostToBuild(m_player) + 1500)
		return decline("not enough money");

	// The nearest warehouse (to the base) that still has cash, is not an enemy's and is not one of our centers' reach.
	const Real servedRange = 25 * PATHFIND_CELL_SIZE_F;
	const Object *warehouse = nullptr;
	Real bestDistSqr = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (!obj->isKindOf(KINDOF_STRUCTURE) || !obj->isKindOf(KINDOF_SUPPLY_SOURCE))
			continue;
		static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
		SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*)obj->findUpdateModule( key_warehouseUpdate );
		if (!warehouseModule || warehouseModule->getBoxesStored() * TheGlobalData->m_baseValuePerSupplyBox <= 0)
			continue;
		if (m_player->getRelationship(obj->getTeam()) == ENEMIES)
			continue;
		Bool served = FALSE;
		for (Object *center = TheGameLogic->getFirstObject(); center && !served; center = center->getNextObject())
			if (center->getControllingPlayer() == m_player && (center->isKindOf(KINDOF_FS_SUPPLY_CENTER) || center->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)))
			{
				const Real dx = center->getPosition()->x - obj->getPosition()->x;
				const Real dy = center->getPosition()->y - obj->getPosition()->y;
				served = dx * dx + dy * dy < servedRange * servedRange;
			}
		if (served)
			continue;
		const Real bx = obj->getPosition()->x - m_baseCenter.x;
		const Real by = obj->getPosition()->y - m_baseCenter.y;
		const Real distSqr = bx * bx + by * by;
		if (!warehouse || distSqr < bestDistSqr)
		{
			warehouse = obj;
			bestDistSqr = distSqr;
		}
	}
	if (!warehouse)
		return decline("no unserved warehouse with cash");

	Object *dozer = findDozer(&m_baseCenter);
	if (!dozer || TheBuildAssistant->canMakeUnit(dozer, centerTemplate) != CANMAKE_OK)
		return decline("no dozer or cannot build it");
	Coord3D pos;
	if (!autopilotFindSupplySpot(centerTemplate, dozer, warehouse, &pos))
		return decline("no legal spot near the warehouse");

	m_player->addToPriorityBuildList(centerTemplate->getName(), &pos, centerTemplate->getPlacementViewAngle());
	m_autopilotPending = m_player->getBuildList();	// the new entry is the list head
	autopilotMarkSupplyEntry(m_autopilotPending, centerTemplate);
	m_autopilotPendingFrame = TheGameLogic->getFrame();
	m_buildDelay = 0;
	if (lanNetplayOptions().m_active)
	{
		printf("NETPLAY autopilot supply expansion: %s at (%.0f,%.0f) (frame %u)\n", centerTemplate->getName().str(), pos.x, pos.y, TheGameLogic->getFrame());
		fflush(stdout);
	}
	return TRUE;
}

// GeneralsX @bugfix Claude 20/10/2026 The AI only sends supply trucks to build-list entries flagged as supply buildings with
// a desired gatherer count. An entry autopilot adds on its own has neither, so a supply center it built earned nothing.
void AISkirmishPlayer::autopilotMarkSupplyEntry(BuildListInfo *info, const ThingTemplate *tmpl)
{
	if (!info || !tmpl)
		return;
	if (tmpl->isKindOf(KINDOF_FS_SUPPLY_CENTER) || tmpl->isKindOf(KINDOF_FS_SUPPLY_DROPZONE))
	{
		info->setSupplyBuilding(TRUE);
		info->setDesiredGatherers(autopilotGatherers());
	}
}

// Queues the first prerequisite structure of 'plan' that the player lacks. Returns true if one was queued.
Bool AISkirmishPlayer::autopilotQueueMissingPrerequisite(const ThingTemplate *plan, UnsignedInt money, Int depth, KindOfType onlyKind)
{
	if (depth > 4)
		return FALSE;
	Object *dozer = findDozer(&m_baseCenter);
	if (!dozer)
		return FALSE;

	for (Int i = 0; i < plan->getPrereqCount(); ++i)
	{
		const ProductionPrerequisite *prereq = plan->getNthPrereq(i);
		if (prereq->isSatisfied(m_player))
			continue;
		for (Int j = 0; j < prereq->getUnitPrereqCount(); ++j)
		{
			const ThingTemplate *need = prereq->getUnitPrereqTemplate(j);
			if (!need || !need->isKindOf(KINDOF_STRUCTURE))
				continue;
			if (onlyKind != KINDOF_INVALID && !need->isKindOf(onlyKind))
				continue;

			// Already owned, or already queued/under way (any build-list entry for it that has not failed)?
			Bool have = false;
			for (Object *obj = TheGameLogic->getFirstObject(); obj && !have; obj = obj->getNextObject())
				if (obj->getControllingPlayer() == m_player && obj->getTemplate()->isEquivalentTo(need))
					have = true;
			for (BuildListInfo *info = m_player->getBuildList(); info && !have; info = info->getNext())
				if (info->getTemplateName() == need->getName() && info->isPriorityBuild() && info->getObjectID() == INVALID_ID)
					have = true;	// already queued and waiting for a dozer
			if (have)
				continue;

			// The prerequisite may itself be blocked (a supply center needs a power plant, say): go down a level first.
			if (TheBuildAssistant->canMakeUnit(dozer, need) == CANMAKE_NO_PREREQ)
			{
				if (autopilotQueueMissingPrerequisite(need, money, depth + 1))
					return TRUE;
				continue;
			}
			if (money < need->calcCostToBuild(m_player))
				return FALSE;	// cannot afford it yet; wait for money instead of queuing it

			// Prefer an unbuilt entry the faction's build list already has for it (the AI's own layout position).
			BuildListInfo *existing = nullptr;
			for (BuildListInfo *info = m_player->getBuildList(); info && !existing; info = info->getNext())
				if (info->getTemplateName() == need->getName() && info->isBuildable() && !info->isPriorityBuild() &&
					!TheGameLogic->findObjectByID(info->getObjectID()))
					existing = info;
			if (existing)
			{
				existing->markPriorityBuild();
				autopilotMarkSupplyEntry(existing, need);
				m_autopilotPending = existing;
				m_autopilotPendingFrame = TheGameLogic->getFrame();
				m_buildDelay = 0;
				if (lanNetplayOptions().m_active)
				{
					printf("NETPLAY autopilot queues prerequisite %s for %s (list entry, frame %u)\n", need->getName().str(), plan->getName().str(), TheGameLogic->getFrame());
					fflush(stdout);
				}
				return TRUE;
			}

			// Somewhere on the ring around the base; the dozer placement search adjusts it to a legal spot.
			const Real angle = (Real)(m_autopilotRing++ % 8) * (PI / 4);
			Coord3D pos = m_baseCenter;
			pos.x += WWMath::CosTrig(angle) * (m_baseRadius * 0.8f + 60.0f);
			pos.y += WWMath::SinTrig(angle) * (m_baseRadius * 0.8f + 60.0f);
			if (need->isKindOf(KINDOF_FS_SUPPLY_CENTER) || need->isKindOf(KINDOF_FS_SUPPLY_DROPZONE))
			{
				// A supply center is only useful next to supplies (the trucks ignore one with none in reach): put it by the
				// nearest warehouse that still has cash, on the side facing the base, as the AI's own supply placement does.
				if (const Object *warehouse = findSupplyCenter(1))
					autopilotFindSupplySpot(need, dozer, warehouse, &pos);
			}
			m_player->addToPriorityBuildList(need->getName(), &pos, need->getPlacementViewAngle());
			m_autopilotPending = m_player->getBuildList();	// the new entry is the list head
			autopilotMarkSupplyEntry(m_autopilotPending, need);
			m_autopilotPendingFrame = TheGameLogic->getFrame();
			m_buildDelay = 0;
			if (lanNetplayOptions().m_active)
			{
				printf("NETPLAY autopilot queues prerequisite %s for %s (frame %u)\n", need->getName().str(), plan->getName().str(), TheGameLogic->getFrame());
				fflush(stdout);
			}
			return TRUE;
		}
	}
	return FALSE;
}

void AISkirmishPlayer::adoptExistingBase()
{
	// A human player normally has no build list. Use this faction's skirmish build list, laid out around the
	// player's start position exactly like a computer player's.
	if (!m_player->getBuildList())
	{
		const AsciiString mySide = m_player->getSide();
		for (const AISideBuildList *build = TheAI->getAiData()->m_sideBuildLists; build; build = build->m_next)
		{
			if (build->m_side == mySide)
			{
				BuildListInfo *buildList = build->m_buildList->duplicate();
				adjustBuildList(buildList, false);	// keep the player's command center
				m_player->setBuildList(buildList);
				break;
			}
		}
	}

	// Claim structures the player already owns, one per build-list entry of the same template.
	for (BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext())
	{
		const AsciiString name = info->getTemplateName();
		if (name.isEmpty())
			continue;
		if (info->getObjectID() != INVALID_ID && TheGameLogic->findObjectByID(info->getObjectID()))
			continue;	// still linked to a live structure

		info->setObjectID(INVALID_ID);
		for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (obj->getControllingPlayer() != m_player || obj->getTemplate()->getName() != name)
				continue;
			Bool claimed = false;
			for (BuildListInfo *other = m_player->getBuildList(); other; other = other->getNext())
			{
				if (other != info && other->getObjectID() == obj->getID())
				{
					claimed = true;
					break;
				}
			}
			if (!claimed)
			{
				info->setObjectID(obj->getID());
				break;
			}
		}

		// Entries the player does not have yet get built once (newMap does the same for non-initial entries).
		if (info->getObjectID() == INVALID_ID)
			info->incrementNumRebuilds();
	}

	computeCenterAndRadiusOfBase(&m_baseCenter, &m_baseRadius);
	m_readyToBuildStructure = true;
	m_structureTimer = 1;
	m_buildDelay = 0;
	m_teamDelay = 0;
	m_autopilotPending = nullptr;
	m_autopilotPendingFrame = 0;
}

//----------------------------------------------------------------------------------------------------------
/**
 * Adjusts the build list to match the starting position.
 */
void AISkirmishPlayer::adjustBuildList(BuildListInfo *list, Bool destroyStartCommandCenter)
{
	Bool foundStart = false;
	Coord3D startPos;

	// Find our command center location.
	Object *obj;
	for( obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() )
	{

		Player *owner = obj->getControllingPlayer();
		if (owner==m_player) {
			// See if it's a command center.
			if (obj->isKindOf(KINDOF_COMMANDCENTER)) {
				foundStart = true;
				startPos = *obj->getPosition();
				// GeneralsX @bugfix Claude 20/10/2026 Autopilot adopts a human's live base: keep the command center.
				// (A computer player's start structure is removed here and rebuilt right away from the build list.)
				if (destroyStartCommandCenter)
				{
					m_player->onStructureUndone(obj);
					TheAI->pathfinder()->removeObjectFromPathfindMap(obj);
					TheGameLogic->destroyObject(obj);
				}
				break;
			}
		}
	}
	if (!foundStart) {
		DEBUG_LOG(("Couldn't find starting command center for ai player."));
		return;
	}
	// Find the location of the command center in the build list.
	Bool foundInBuildList = false;
	Coord3D buildPos;
	BuildListInfo *cur = list;
	while (cur) {
		const ThingTemplate *tTemplate = TheThingFactory->findTemplate(cur->getTemplateName());
		if (tTemplate && tTemplate->isKindOf(KINDOF_COMMANDCENTER)) {
			foundInBuildList = true;
			buildPos = *cur->getLocation();
			cur->setInitiallyBuilt(true);
		}
		cur = cur->getNext();
	}
	Region3D bounds;
	TheTerrainLogic->getMaximumPathfindExtent(&bounds);
	/* calculate section of 3x3 grid:
		6 7 8
		3 4 5
		0 1 2 */

	Int gridIndex = 0;
	if (startPos.x > bounds.lo.x + bounds.width()/3) {
		gridIndex++;
	}
	if (startPos.x > bounds.lo.x + 2*bounds.width()/3) {
		gridIndex++;
	}

	if (startPos.y > bounds.lo.y + bounds.height()/3) {
		gridIndex+=3;
	}
	if (startPos.y > bounds.lo.y + 2*bounds.height()/3) {
		gridIndex+=3;
	}

	Real angle = 0;
	if (TheAI->getAiData()->m_rotateSkirmishBases) {
		switch (gridIndex) {
			case 0 : angle = 0; break;
			case 1 : angle = PI/4; break;// 45 degrees.
			case 2 : angle = PI/2; break; // 90 degrees;
			case 3 : angle = -PI/4; break; // -45 degrees.
			case 4 : angle = 0; break;
			case 5 : angle = 3*PI/4; break; // 135 degrees.
			case 6 : angle = -PI/2; break; // -90 degrees;
			case 7 : angle = -3*PI/4; break; // -135 degrees.
			case 8 : angle = PI; break; // 180 degrees.
		}
	}

	angle += 3*PI/4;

	Real s = WWMath::SinTrig(angle);
	Real c = WWMath::CosTrig(angle);

	cur = list;
	while (cur) {
		const ThingTemplate *tTemplate = TheThingFactory->findTemplate(list->getTemplateName());
		if (tTemplate && tTemplate->isKindOf(KINDOF_COMMANDCENTER)) {
			foundInBuildList = true;
			Coord3D curPos = *cur->getLocation();
			// Transform to new coords.
			curPos.x -= buildPos.x;
			curPos.y -= buildPos.y;
			Real newX = curPos.x*c - curPos.y*s;
			Real newY = curPos.y*c + curPos.x*s;
			curPos.x = newX + startPos.x;
			curPos.y = newY + startPos.y;
			cur->setLocation(curPos);
			cur->setAngle(cur->getAngle());
		}
		cur = cur->getNext();
	}

}



//----------------------------------------------------------------------------------------------------------
/**
 * Find any things that build stuff & add them to the build list.  Then build any initially built
 * buildings.
 */
void AISkirmishPlayer::newMap()
{

	/* Get our proper build list. */
	AsciiString mySide = m_player->getSide();
	DEBUG_LOG(("AI Player side is %s", mySide.str()));
	const AISideBuildList *build = TheAI->getAiData()->m_sideBuildLists;
	while (build) {
		if (build->m_side == mySide) {
			BuildListInfo *buildList = build->m_buildList->duplicate();
			adjustBuildList(buildList); // adjust to  our start position.
			m_player->setBuildList(buildList);
			computeCenterAndRadiusOfBase(&m_baseCenter, &m_baseRadius);
			break;
		}
		build = build->m_next;
	}
	DEBUG_ASSERTLOG(build!=nullptr, ("Couldn't find build list for skirmish player."));

	// Build any with the initially built flag.
	for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
	{
		AsciiString name = info->getTemplateName();
		if (name.isEmpty()) continue;
		const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( name );
		if (!bldgPlan) {
			DEBUG_LOG(("*** ERROR - Build list building '%s' doesn't exist.", name.str()));
			continue;
		}
		if (info->isInitiallyBuilt()) {
			buildStructureNow(bldgPlan, info);
		} else {
			info->incrementNumRebuilds(); // the initial build in the normal build list consumes a rebuild, so add one.
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * Queues up a dozer.
 */
void AISkirmishPlayer::queueDozer()
{
	AIPlayer::queueDozer();
}

//----------------------------------------------------------------------------------------------------------
/**
 * Finds a dozer that isn't building or collecting resources.
 */
Object * AISkirmishPlayer::findDozer( const Coord3D *pos )
{
	return AIPlayer::findDozer(pos);
}


//----------------------------------------------------------------------------------------------------------
/**
 * Find a good spot to fire a superweapon.
 */
Bool AISkirmishPlayer::computeSuperweaponTarget(const SpecialPowerTemplate *power, Coord3D *retPos, Int playerNdx, Real weaponRadius)
{

	Region2D bounds;
	getPlayerStructureBounds(&bounds, playerNdx);

	if( power->getSpecialPowerType() == SPECIAL_CLUSTER_MINES || power->getSpecialPowerType() == NUKE_SPECIAL_CLUSTER_MINES )
	{
		// hackus brutus - mine the entrances to our base.
		AsciiString pathLabel;
		Int mode = GameLogicRandomValue(0, 2);
		if (mode==1) {
				pathLabel.format("%s%d", SKIRMISH_FLANK, m_player->getMpStartIndex()+1);
		}	else if (mode==2) {
				pathLabel.format("%s%d", SKIRMISH_BACKDOOR, m_player->getMpStartIndex()+1);
		}	else {
			pathLabel.format("%s%d", SKIRMISH_CENTER, m_player->getMpStartIndex()+1);
		}

		Coord3D goalPos = m_baseCenter;
		Waypoint *way = TheTerrainLogic->getClosestWaypointOnPath( &goalPos, pathLabel );
		if (way) {
			goalPos = *way->getLocation();
		} else {
			Region2D bounds;
			getPlayerStructureBounds(&bounds, getMyEnemyPlayerIndex());
			goalPos.x = bounds.lo.x + bounds.width()/2;
			goalPos.y = bounds.lo.y + bounds.height()/2;
		}
		Coord2D offset;
		offset.x = goalPos.x-m_baseCenter.x;
		offset.y = goalPos.y-m_baseCenter.y;
		offset.normalize();
		offset.x *= m_baseRadius;
		offset.y *= m_baseRadius;
		*retPos = m_baseCenter;
		retPos->x += offset.x;
		retPos->y += offset.y;
		retPos->z = TheTerrainLogic->getGroundHeight(retPos->x, retPos->y);
		return TRUE;
	}

	return AIPlayer::computeSuperweaponTarget(power, retPos, playerNdx, weaponRadius);

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AISkirmishPlayer::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info;
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void AISkirmishPlayer::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// xfer base class info
	AIPlayer::xfer( xfer );

	// front base defense
	xfer->xferInt( &m_curFrontBaseDefense );

	// flank base defense
	xfer->xferInt( &m_curFlankBaseDefense );

	// front left defense angle
	xfer->xferReal( &m_curFrontLeftDefenseAngle );

	// front right defense angle
	xfer->xferReal( &m_curFrontRightDefenseAngle );

	// left flank left defense angle
	xfer->xferReal( &m_curLeftFlankLeftDefenseAngle );

	// left flank right defense angle
	xfer->xferReal( &m_curLeftFlankRightDefenseAngle );

	// right flank left defense angle
	xfer->xferReal( &m_curRightFlankLeftDefenseAngle );

	// right flank right defense angle
	xfer->xferReal( &m_curRightFlankRightDefenseAngle );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AISkirmishPlayer::loadPostProcess()
{

}

