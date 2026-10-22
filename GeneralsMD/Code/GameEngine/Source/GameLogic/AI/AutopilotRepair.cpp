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

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

#include "GameLogic/AutopilotTuning.h"
#include "GameLogic/AutopilotHost.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"
#include "GameLogic/AutopilotRepair.h"

void AutopilotRepair::update()
{
	if (!m_profile.m_repair)
		return;
	Player *player = m_host.player();

	// The most damaged finished building (health below 70%).
	Object *worst = nullptr;
	Real worstRatio = AutopilotTuning::REPAIR_BELOW_HEALTH;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != player || !obj->isKindOf(KINDOF_STRUCTURE) || obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION)
			|| obj->isEffectivelyDead())
			continue;
		BodyModuleInterface *body = obj->getBodyModule();
		if (!body || body->getMaxHealth() <= 0.0f)
			continue;
		const Real ratio = body->getHealth() / body->getMaxHealth();
		if (ratio < worstRatio)
		{
			worst = obj;
			worstRatio = ratio;
		}
	}
	if (!worst)
		return;

	// An idle dozer or worker (one busy building or repairing is left alone; the planner's orders override this one when it needs the dozer).
	Object *dozer = m_host.findDozerNear(worst->getPosition());
	AIUpdateInterface *ai = dozer ? dozer->getAIUpdateInterface() : nullptr;
	if (!ai || !ai->isIdle())
		return;
	ai->aiRepair(worst, CMD_FROM_AI);
	if (m_traceThrottle.due(TheGameLogic->getFrame(), 300))
		AutopilotLog::line("NETPLAY autopilot repair: %s at %d%% (frame %u)\n", worst->getTemplate()->getName().str(), (Int)(worstRatio * 100), TheGameLogic->getFrame());
}
