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

#include "Common/Energy.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

#include "GameLogic/AutopilotSnapshot.h"

namespace
{
	const Int MAX_LISTED = 80;	// per list, nearest-to-nothing is not defined; the cap just keeps one snapshot readable

	void addObject(AsciiString &out, Int &count, const Object *obj, const char *extra)
	{
		if (count >= MAX_LISTED)
			return;
		const BodyModuleInterface *body = obj->getBodyModule();
		const Int health = body && body->getMaxHealth() > 0.0f ? (Int)(100.0f * body->getHealth() / body->getMaxHealth()) : 100;
		AsciiString one;
		one.format("%s{\"id\":%d,\"t\":\"%s\",\"x\":%d,\"y\":%d,\"hp\":%d%s}", count ? "," : "", (Int)obj->getID(), obj->getTemplate()->getName().str(),
			(Int)obj->getPosition()->x, (Int)obj->getPosition()->y, health, extra);
		out.concat(one);
		++count;
	}
}

AsciiString AutopilotSnapshot::json(Player *player)
{
	if (!player || !TheGameLogic || !ThePlayerList)
		return AsciiString::TheEmptyString;

	AsciiString own, enemy, capturable, ownUnits, enemyUnits;
	Int nOwn = 0, nEnemy = 0, nCap = 0;
	struct Tally { const ThingTemplate *tmpl; Int n; };
	Tally mine[40] = {}, theirs[40] = {};
	Int nMine = 0, nTheirs = 0;

	const Int idx = player->getPlayerIndex();
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->isEffectivelyDead() || obj->isKindOf(KINDOF_REBUILD_HOLE))
			continue;
		Player *owner = obj->getControllingPlayer();
		const Bool structure = obj->isKindOf(KINDOF_STRUCTURE);
		const Bool mineOwned = owner == player;
		const ObjectShroudStatus shroud = mineOwned ? OBJECTSHROUD_CLEAR : obj->getShroudedStatus(idx);
		if (shroud == OBJECTSHROUD_INVALID || shroud == OBJECTSHROUD_INVALID_BUT_PREVIOUS_VALID || shroud == OBJECTSHROUD_SHROUDED)
			continue;	// never seen, or black shroud: the player knows nothing about it
		const Bool fogged = shroud == OBJECTSHROUD_FOGGED;

		if (mineOwned)
		{
			if (structure)
				addObject(own, nOwn, obj, obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION) ? ",\"building\":true" : "");
			else
			{
				Tally *list = mine; Int &n = nMine;
				Int i = 0;
				while (i < n && list[i].tmpl != obj->getTemplate()) ++i;
				if (i == n && n < 40) { list[n].tmpl = obj->getTemplate(); list[n].n = 0; ++n; }
				if (i < n) ++list[i].n;
			}
			continue;
		}
		if (!owner)
			continue;
		const Bool neutralish = owner == ThePlayerList->getNeutralPlayer() || player->getRelationship(owner->getDefaultTeam()) == NEUTRAL;
		if (structure && neutralish && obj->isKindOf(KINDOF_CAPTURABLE))
		{
			addObject(capturable, nCap, obj, fogged ? ",\"fogged\":true" : "");
			continue;
		}
		if (neutralish || player->getRelationship(owner->getDefaultTeam()) != ENEMIES)
			continue;	// allies and civilians' non-capturable props are not interesting
		if (structure)
			addObject(enemy, nEnemy, obj, fogged ? ",\"fogged\":true" : "");
		else if (!fogged)
		{
			Tally *list = theirs; Int &n = nTheirs;
			Int i = 0;
			while (i < n && list[i].tmpl != obj->getTemplate()) ++i;
			if (i == n && n < 40) { list[n].tmpl = obj->getTemplate(); list[n].n = 0; ++n; }
			if (i < n) ++list[i].n;
		}
	}

	AsciiString units, foes;
	for (Int i = 0; i < nMine; ++i)
	{
		AsciiString one;
		one.format("%s\"%s\":%d", i ? "," : "", mine[i].tmpl->getName().str(), mine[i].n);
		units.concat(one);
	}
	for (Int i = 0; i < nTheirs; ++i)
	{
		AsciiString one;
		one.format("%s\"%s\":%d", i ? "," : "", theirs[i].tmpl->getName().str(), theirs[i].n);
		foes.concat(one);
	}

	// format() truncates at about 2 KB, so the long lists are concatenated, not formatted.
	AsciiString result, head;
	head.format("{\"frame\":%u,\"player\":\"%s\",\"money\":%u,\"powerRatio\":%.2f,\"autopilot\":%d,\"structures\":[",
		TheGameLogic->getFrame(), player->getSide().str(), player->getMoney()->countMoney(), player->getEnergy()->getEnergySupplyRatio(), player->getAutopilotLevel());
	result.concat(head);
	result.concat(own);
	result.concat("],\"units\":{");
	result.concat(units);
	result.concat("},\"capturable\":[");
	result.concat(capturable);
	result.concat("],\"enemyStructures\":[");
	result.concat(enemy);
	result.concat("],\"enemyUnitsVisible\":{");
	result.concat(foes);
	result.concat("}}");
	return result;
}
