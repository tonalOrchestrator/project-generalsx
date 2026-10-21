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

// GeneralsX @feature Claude 20/10/2026 Headless LAN netplay harness (see header).

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/LanNetplayHarness.h"

#include "Common/AISkirmishSimulation.h"
#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/BuildAssistant.h"
#include "Common/RandomValue.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/GameClient.h"
#include "GameClient/MapUtil.h"
#include "GameClient/TerrainVisual.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/VictoryConditions.h"
#include "GameNetwork/GameInfo.h"
#include "GameNetwork/IPEnumeration.h"
#include "GameNetwork/LANAPI.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "GameNetwork/LANGameInfo.h"
#include "GameNetwork/NetworkInterface.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

//-------------------------------------------------------------------------------------------------
static UnsignedInt nowMs()
{
	return GetTickCount();
}

// A human ordering a structure by hand, the way the UI does it: select the dozer, then construct. 'kind' is the structure type
// ("barracks"); the template is the first one the dozer can build, the spot the first legal one around the command center.
static void orderManualBuild(const AsciiString& kind)
{
	Player* local = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
	auto skip = [](const char* why) { printf("NETPLAY manual build skipped: %s\n", why); fflush(stdout); };
	if (!local || local->isPlayerObserver())
		return skip("no active local player");
	Object* dozer = nullptr;
	Object* hq = nullptr;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != local)
			continue;
		if (!dozer && obj->isKindOf(KINDOF_DOZER))
			dozer = obj;
		if (!hq && obj->isKindOf(KINDOF_COMMANDCENTER))
			hq = obj;
	}
	if (!dozer || !hq)
		return skip(dozer ? "no command center" : "no dozer or worker");
	const KindOfType wanted = kind.compareNoCase("barracks") == 0 ? KINDOF_FS_BARRACKS : KINDOF_FS_POWER;
	const ThingTemplate* place = nullptr;
	for (const ThingTemplate* t = TheThingFactory->firstTemplate(); t && !place; t = t->friend_getNextTemplate())
		if (t->isKindOf(KINDOF_STRUCTURE) && t->isKindOf(wanted) && TheBuildAssistant->canMakeUnit(dozer, t) == CANMAKE_OK)
			place = t;
	if (!place)
		return skip("no buildable structure of that kind");
	const Coord3D* center = hq->getPosition();
	Coord3D spot = *center;
	Bool found = FALSE;
	for (Int ring = 12; ring <= 40 && !found; ring += 2)
		for (Int a = 0; a < 8 && !found; ++a)
		{
			const Real ang = a * (PI / 4);
			Coord3D c = *center;
			c.x += WWMath::CosTrig(ang) * ring * PATHFIND_CELL_SIZE_F;
			c.y += WWMath::SinTrig(ang) * ring * PATHFIND_CELL_SIZE_F;
			c.z = TheTerrainLogic->getGroundHeight(c.x, c.y);
			if (TheBuildAssistant->isLocationLegalToBuild(&c, place, place->getPlacementViewAngle(),
					BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH | BuildAssistant::NO_OBJECT_OVERLAP, dozer, nullptr) == LBC_OK)
			{
				spot = c;
				found = TRUE;
			}
		}
	TheTerrainVisual->removeAllBibs();
	if (!found)
		return skip("no legal spot near the command center");
	GameMessage* selectMsg = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	selectMsg->appendBooleanArgument(TRUE);
	selectMsg->appendObjectIDArgument(dozer->getID());
	GameMessage* buildMsg = TheMessageStream->appendMessage(GameMessage::MSG_DOZER_CONSTRUCT);
	buildMsg->appendIntegerArgument(place->getTemplateID());
	buildMsg->appendLocationArgument(spot);
	buildMsg->appendRealArgument(place->getPlacementViewAngle());
	printf("NETPLAY manual build ordered: %s at (%.0f,%.0f) with dozer %d\n", place->getName().str(), spot.x, spot.y, (int)dozer->getID());
	fflush(stdout);
}

static void sleepMs(UnsignedInt ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}

static SlotState difficultyCharToSlotState(char c)
{
	switch (toupper((unsigned char)c))
	{
		case 'E': return SLOT_EASY_AI;
		case 'M':
		case 'N': return SLOT_MED_AI;
		default:  return SLOT_BRUTAL_AI;
	}
}

static void log(const char* fmt, ...)
{
	char buf[8192];	// progress lines carry a full per-player stats JSON
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	printf("NETPLAY %s\n", buf);
	fflush(stdout);
}

//-------------------------------------------------------------------------------------------------
Bool LanNetplayHarness::isActive()
{
	return TheGlobalData && TheGlobalData->m_lanHarness;
}

void LanNetplayHarness::onEvent(const char* what)
{
	log("event %s", what);
}

//-------------------------------------------------------------------------------------------------
// Prefer a 192.168.x.x address, then any non-loopback / non-link-local one.
static UnsignedInt pickLocalIP()
{
	IPEnumeration ips;
	EnumeratedIP* list = ips.getAddresses();
	UnsignedInt best = 0;
	for (EnumeratedIP* e = list; e; e = e->getNext())
	{
		const UnsignedInt ip = e->getIP();
		const UnsignedInt a = (ip >> 24) & 0xff;
		const UnsignedInt b = (ip >> 16) & 0xff;
		if (a == 127 || (a == 169 && b == 254) || a == 0)
			continue;
		if (a == 192 && b == 168)
			return ip;
		if (best == 0)
			best = ip;
	}
	return best;
}

static UnsignedInt parseDottedQuad(const char* s)
{
	int a, b, c, d;
	if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
		return 0;
	return ((UnsignedInt)a << 24) | ((UnsignedInt)b << 16) | ((UnsignedInt)c << 8) | (UnsignedInt)d;
}

//-------------------------------------------------------------------------------------------------
static Int countHumans(LANGameInfo* g)
{
	Int n = 0;
	for (Int i = 0; i < MAX_SLOTS; ++i)
		if (g->getSlot(i)->isHuman())
			++n;
	return n;
}

static Bool allHumansHaveMap(LANGameInfo* g)
{
	for (Int i = 0; i < MAX_SLOTS; ++i)
		if (g->getSlot(i)->isHuman() && !g->getSlot(i)->hasMap())
			return FALSE;
	return TRUE;
}

static Bool allHumansAccepted(LANGameInfo* g)
{
	for (Int i = 0; i < MAX_SLOTS; ++i)
		if (g->getSlot(i)->isHuman() && !g->getSlot(i)->isAccepted())
			return FALSE;
	return TRUE;
}

// Fill the host's game the way the options menu would after the host picks a map and AI opponents.
static Bool configureHostGame(LANGameInfo* g, const AsciiString& mapName)
{
	const MapMetaData* md = TheMapCache->findMap(mapName);
	if (!md)
		return FALSE;

	g->setMap(mapName);
	g->setMapCRC(md->m_CRC);
	g->setMapSize(md->m_filesize);
	g->getSlot(0)->setMapAvailability(TRUE);

	const Int mapPlayers = md->m_numPlayers;
	const Int humans = TheGlobalData->m_lanHumans > 0 ? TheGlobalData->m_lanHumans : 1;

	// Slot 0: the host machine.
	LANGameSlot* host = g->getLANSlot(0);
	Int hostTemplate = PLAYERTEMPLATE_RANDOM;
	if (TheGlobalData->m_lanHostTemplate.isNotEmpty())
	{
		hostTemplate = ThePlayerTemplateStore->getTemplateNumByName(TheGlobalData->m_lanHostTemplate);
		if (hostTemplate < 0)
			hostTemplate = PLAYERTEMPLATE_RANDOM;
	}
	host->setPlayerTemplate(TheGlobalData->m_lanObserver ? PLAYERTEMPLATE_OBSERVER : hostTemplate);
	host->setColor(-1);
	host->setStartPos(-1);
	host->setTeamNumber(-1);

	Int slotNum = 1;
	// Reserve open slots for the joining machines.
	for (Int h = 1; h < humans && slotNum < MAX_SLOTS; ++h, ++slotNum)
		g->getLANSlot(slotNum)->setState(SLOT_OPEN);

	// AI opponents.
	AsciiString spec = TheGlobalData->m_lanSlots;
	AsciiString token;
	Int aiCount = 0;
	while (spec.nextToken(&token, ",") && slotNum < MAX_SLOTS)
	{
		LANGameSlot* s = g->getLANSlot(slotNum);
		s->setState(difficultyCharToSlotState(token.getCharAt(0)));
		s->setColor(-1);
		s->setStartPos(-1);
		s->setTeamNumber(-1);
		Int tmpl = PLAYERTEMPLATE_RANDOM;
		const char* colon = strchr(token.str(), ':');
		if (colon && *(colon + 1))
		{
			tmpl = ThePlayerTemplateStore->getTemplateNumByName(AsciiString(colon + 1));
			if (tmpl < 0)
				tmpl = PLAYERTEMPLATE_RANDOM;
		}
		s->setPlayerTemplate(tmpl);
		++slotNum;
		++aiCount;
	}
	for (; slotNum < MAX_SLOTS; ++slotNum)
		g->getLANSlot(slotNum)->setState(SLOT_CLOSED);

	Int seed = (Int)GetTickCount();
	if (TheGlobalData->m_lanSeed >= 0)
		seed = TheGlobalData->m_lanSeed;
	g->setSeed(seed);

	log("host configured map=\"%s\" mapPlayers=%d humans=%d ai=%d observer=%d seed=%d", mapName.str(), mapPlayers, humans, aiCount,
		TheGlobalData->m_lanObserver ? 1 : 0, seed);
	if (humans + aiCount > mapPlayers && !TheGlobalData->m_lanObserver)
		log("warning: humans+ai (%d) exceeds the map's %d start positions", humans + aiCount, mapPlayers);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
int LanNetplayHarness::run()
{
	const Bool host = TheGlobalData->m_lanHost;
	const Bool headless = TheGlobalData->m_headless;
	const UnsignedInt startMs = nowMs();

	AsciiString mapName;
	if (host)
	{
		mapName = AISkirmishSimulation::resolveMapName(TheGlobalData->m_lanMap);
		if (mapName.isEmpty())
		{
			log("error map \"%s\" not found", TheGlobalData->m_lanMap.str());
			return 1;
		}
	}

	// --- bring up LANAPI the way the Direct Connect screen does ---
	if (TheLAN)
	{
		delete TheLAN;
		TheLAN = nullptr;
	}
	TheLAN = NEW LANAPI();
	TheLAN->init();

	UnsignedInt localIP = TheGlobalData->m_lanLocalIP.isNotEmpty() ? parseDottedQuad(TheGlobalData->m_lanLocalIP.str()) : pickLocalIP();
	if (localIP == 0)
	{
		log("error no usable local IPv4 address (use -lanIP)");
		return 1;
	}
	TheLAN->SetLocalIP(localIP);
	TheLAN->RequestLobbyLeave(TRUE);

	UnicodeString name;
	name.translate(TheGlobalData->m_lanName);
	name.truncateTo(g_lanPlayerNameLength);
	TheLAN->RequestSetName(name);

	log("role=%s local=%d.%d.%d.%d name=%s headless=%d", host ? "host" : "join", PRINTF_IP_AS_4_INTS(localIP),
		TheGlobalData->m_lanName.str(), headless ? 1 : 0);

	TheMapCache->updateCache();

	enum Phase { PH_CREATE, PH_WAIT_CREATE, PH_LOBBY, PH_JOIN, PH_WAIT_JOIN, PH_JOINED, PH_PLAYING, PH_DONE };
	Phase phase = host ? PH_CREATE : PH_JOIN;
	UnsignedInt phaseSince = nowMs();
	UnsignedInt lastResend = 0;
	Bool startRequested = FALSE;
	Bool joinOptionsSent = FALSE;
	Bool timedOut = FALSE;
	Bool sawGame = FALSE;
	UnsignedInt lastFrameSeen = 0;
	UnsignedInt lastFrameMs = 0;
	UnsignedInt lastStatusFrame = 0;
	Int seed = 0;
	AsciiString mapForProgress = mapName;

	// -lanAutopilotSchedule "10:2,70:0,140:2": game-second:level pairs, sent as MSG_SET_AUTOPILOT through the
	// message stream (same path as the hotkey), so they travel the network command stream and stay in sync.
	struct AutopilotStep { UnsignedInt frame; Int level; };
	struct BuildStep { UnsignedInt frame; AsciiString kind; };
	BuildStep buildSteps[16];
	Int numBuildSteps = 0;
	Int nextBuildStep = 0;
	{
		AsciiString spec = TheGlobalData->m_lanBuildSchedule;
		AsciiString token;
		while (spec.nextToken(&token, ",") && numBuildSteps < 16)
		{
			const char* colon = strchr(token.str(), ':');
			if (colon && *(colon + 1))
			{
				buildSteps[numBuildSteps].frame = (UnsignedInt)atoi(token.str()) * LOGICFRAMES_PER_SECOND;
				buildSteps[numBuildSteps].kind = AsciiString(colon + 1);
				++numBuildSteps;
			}
		}
	}
	struct CheatStep { UnsignedInt frame; Int money; Int points; };
	CheatStep cheatSteps[16];
	Int numCheatSteps = 0;
	Int nextCheatStep = 0;
	{
		AsciiString spec = TheGlobalData->m_lanCheatSchedule;
		AsciiString token;
		while (spec.nextToken(&token, ",") && numCheatSteps < 16)
		{
			int sec = 0, money = 0, points = 0;
			if (sscanf(token.str(), "%d:%d:%d", &sec, &money, &points) == 3)
			{
				cheatSteps[numCheatSteps].frame = (UnsignedInt)sec * LOGICFRAMES_PER_SECOND;
				cheatSteps[numCheatSteps].money = money;
				cheatSteps[numCheatSteps].points = points;
				++numCheatSteps;
			}
		}
	}
	AutopilotStep autopilotSteps[16];
	Int numAutopilotSteps = 0;
	Int nextAutopilotStep = 0;
	{
		AsciiString spec = TheGlobalData->m_lanAutopilotSchedule;
		AsciiString token;
		while (spec.nextToken(&token, ",") && numAutopilotSteps < 16)
		{
			int sec = 0, lvl = 0;
			if (sscanf(token.str(), "%d:%d", &sec, &lvl) == 2)
			{
				autopilotSteps[numAutopilotSteps].frame = (UnsignedInt)sec * LOGICFRAMES_PER_SECOND;
				autopilotSteps[numAutopilotSteps].level = lvl;
				++numAutopilotSteps;
			}
		}
	}
	const UnsignedInt lobbyTimeoutMs = 120000;
	const UnsignedInt maxFrames = (UnsignedInt)TheGlobalData->m_lanMaxMinutes * 60u * LOGICFRAMES_PER_SECOND;

	for (;;)
	{
		if (TheGameEngine->getQuitting())
			break;

		const UnsignedInt t = nowMs();

		if (phase != PH_PLAYING)
		{
			TheLAN->update();

			// Lobby diagnostics every 5s: who is in which slot and what each side knows.
			static UnsignedInt s_lastLobbyDiag = 0;
			if (t - s_lastLobbyDiag > 5000)
			{
				s_lastLobbyDiag = t;
				LANGameInfo* dg = TheLAN->GetMyGame();
				if (dg)
				{
					AsciiString slots;
					for (Int i = 0; i < MAX_SLOTS; ++i)
					{
						GameSlot* sl = dg->getSlot(i);
						if (sl->isOccupied() || sl->isHuman())
						{
							AsciiString one;
							one.format(" [%d %s%s%s%s]", i, sl->isHuman() ? "H" : "AI", sl->hasMap() ? " map" : " nomap", sl->isAccepted() ? " acc" : " noacc",
								sl->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER ? " obs" : "");
							slots.concat(one);
						}
					}
					const MapMetaData* dmd = dg->getMap().isNotEmpty() ? TheMapCache->findMap(dg->getMap()) : nullptr;
					log("lobby phase=%d local=%d map=\"%s\" found=%d crc=%08X%s", (int)phase, dg->getLocalSlotNum(), dg->getMap().str(), dmd ? 1 : 0,
						dg->getMapCRC(), slots.str());
				}
				else
				{
					log("lobby phase=%d no game yet", (int)phase);
				}
			}

			if (t - phaseSince > lobbyTimeoutMs && phase != PH_DONE)
			{
				log("error lobby timeout in phase %d", (int)phase);
				return 2;
			}
		}

		switch (phase)
		{
			case PH_CREATE:
				TheLAN->RequestGameCreate(UnicodeString::TheEmptyString, TRUE);
				phase = PH_WAIT_CREATE;
				phaseSince = t;
				break;

			case PH_WAIT_CREATE:
			{
				LANGameInfo* g = TheLAN->GetMyGame();
				if (g && TheLAN->AmIHost())
				{
					if (!configureHostGame(g, mapName))
					{
						log("error could not configure map");
						return 1;
					}
					seed = g->getSeed();
					g->resetAccepted();
					TheLAN->RequestGameOptions(GenerateGameOptionsString(), TRUE);
					log("host lobby ready, waiting for %d human machine(s)", TheGlobalData->m_lanHumans);
					phase = PH_LOBBY;
					phaseSince = t;
				}
				break;
			}

			case PH_LOBBY:
			{
				LANGameInfo* g = TheLAN->GetMyGame();
				if (!g)
				{
					log("error host game vanished");
					return 2;
				}
				if (countHumans(g) >= TheGlobalData->m_lanHumans && allHumansHaveMap(g))
				{
					if (allHumansAccepted(g))
					{
						if (!startRequested)
						{
							for (Int i = 0; i < MAX_SLOTS; ++i)
								if (g->getLANSlot(i)->isOpen())
									g->getLANSlot(i)->setState(SLOT_CLOSED);
							TheLAN->RequestGameOptions(GenerateGameOptionsString(), TRUE);
							log("all %d humans ready, starting", countHumans(g));
							TheLAN->RequestGameStart();
							startRequested = TRUE;
							phase = PH_PLAYING;
							phaseSince = t;
						}
					}
					else if (!g->getSlot(0)->isAccepted() && t - lastResend > 1000)
					{
						// Tell the joiners, and mark ourselves: the menu path learns its own accept from its own
						// broadcast looping back, which the directed-send fan-out no longer delivers to the sender.
						TheLAN->RequestAccept();
						g->getSlot(0)->setAccept();
						lastResend = t;
					}
				}
				break;
			}

			case PH_JOIN:
			{
				UnsignedInt ip = parseDottedQuad(TheGlobalData->m_lanJoinIP.str());
				if (ip == 0)
				{
					log("error bad -lanjoin address \"%s\"", TheGlobalData->m_lanJoinIP.str());
					return 1;
				}
				log("joining %d.%d.%d.%d", PRINTF_IP_AS_4_INTS(ip));
				TheLAN->RequestGameJoinDirectConnect(ip);
				phase = PH_WAIT_JOIN;
				phaseSince = t;
				lastResend = t;
				break;
			}

			case PH_WAIT_JOIN:
			{
				LANGameInfo* g = TheLAN->GetMyGame();
				if (g && g->getLocalSlotNum() >= 0)
				{
					log("joined slot %d", g->getLocalSlotNum());
					phase = PH_JOINED;
					phaseSince = t;
				}
				else if (t - lastResend > 6000)
				{
					// the join request or reply was lost (or the host was not up yet): ask again
					UnsignedInt ip = parseDottedQuad(TheGlobalData->m_lanJoinIP.str());
					TheLAN->RequestGameJoinDirectConnect(ip);
					lastResend = t;
				}
				break;
			}

			case PH_JOINED:
			{
				LANGameInfo* g = TheLAN->GetMyGame();
				if (!g || g->getLocalSlotNum() < 0)
					break;
				if (g->isGameInProgress())
				{
					phase = PH_PLAYING;
					phaseSince = t;
					break;
				}
				if (!joinOptionsSent)
				{
					AsciiString o;
					o.format("PlayerTemplate=%d", TheGlobalData->m_lanObserver ? (Int)PLAYERTEMPLATE_OBSERVER : (Int)PLAYERTEMPLATE_RANDOM);
					TheLAN->RequestGameOptions(o, TRUE);
					o.format("Color=%d", -1);
					TheLAN->RequestGameOptions(o, TRUE);
					o.format("NAT=%d", FirewallHelperClass::FIREWALL_TYPE_SIMPLE);
					TheLAN->RequestGameOptions(o, TRUE);
					joinOptionsSent = TRUE;
				}
				// Does this machine have the host's map? Then say so and accept (repeat until the host sees it).
				const MapMetaData* md = g->getMap().isNotEmpty() ? TheMapCache->findMap(g->getMap()) : nullptr;
				GameSlot* me = g->getSlot(g->getLocalSlotNum());
				if (md && !me->hasMap())
				{
					me->setMapAvailability(TRUE);
					if (md->m_CRC != g->getMapCRC())
						log("warning map CRC differs: local %08X host %08X", md->m_CRC, g->getMapCRC());
				}
				if (md && t - lastResend > 1500)
				{
					TheLAN->RequestHasMap();
					if (!me->isAccepted())
						TheLAN->RequestAccept();
					lastResend = t;
				}
				break;
			}

			case PH_PLAYING:
				break;

			case PH_DONE:
				break;
		}

		if (phase == PH_DONE)
			break;

		if (phase == PH_PLAYING || (TheGameLogic && TheGameLogic->isInGame()))
		{
			// Same order as GameEngine::update(). Headless: the minimal subset (no rendering). Windowed: the
			// real engine update, which also draws, so the watch camera and in-engine screenshots work.
			if (headless)
			{
				// GameClient::update() appends this every client update; Network::processCommand() relies on it to
				// leave NETLOCALSTATUS_PREGAME and to close each logic frame (frame-info packets, lockstep). Without it
				// a headless instance runs its own simulation and silently drops every command it would send.
				if (TheNetwork)
				{
					GameMessage* tick = TheMessageStream->appendMessage(GameMessage::MSG_FRAME_TICK);
					tick->appendTimestampArgument(TheGameClient->getFrame());
				}
				TheMessageStream->propagateMessages();
				if (TheNetwork)
					TheNetwork->UPDATE();
			}
			else
			{
				TheGameEngine->serviceWindowsOS();
				TheGameEngine->update();
			}

			if (TheGameLogic->isInGame())
			{
				phase = PH_PLAYING;
				sawGame = TRUE;
				if (mapForProgress.isEmpty() && TheLAN && TheLAN->GetMyGame())
					mapForProgress = TheLAN->GetMyGame()->getMap();
				if (!seed && TheLAN && TheLAN->GetMyGame())
					seed = TheLAN->GetMyGame()->getSeed();

				const Bool ready = !headless || !TheNetwork || TheNetwork->isFrameDataReady();
				if (ready)
				{
					if (headless)
					{
						TheGameClient->updateHeadless();
						TheGameLogic->UPDATE();
					}

					const UnsignedInt frame = TheGameLogic->getFrame();
					while (nextBuildStep < numBuildSteps && frame >= buildSteps[nextBuildStep].frame)
					{
						orderManualBuild(buildSteps[nextBuildStep].kind);
						++nextBuildStep;
					}
					while (nextCheatStep < numCheatSteps && frame >= cheatSteps[nextCheatStep].frame)
					{
						Player* local = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
						if (local && !local->isPlayerObserver())
						{
							GameMessage* cheatMsg = TheMessageStream->appendMessage(GameMessage::MSG_CHEAT_GIVE);
							cheatMsg->appendIntegerArgument(cheatSteps[nextCheatStep].money);
							cheatMsg->appendIntegerArgument(cheatSteps[nextCheatStep].points);
							log("cheat request money=%d points=%d at frame %u", cheatSteps[nextCheatStep].money, cheatSteps[nextCheatStep].points, frame);
						}
						++nextCheatStep;
					}
					while (nextAutopilotStep < numAutopilotSteps && frame >= autopilotSteps[nextAutopilotStep].frame)
					{
						Player* local = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
						if (local && !local->isPlayerObserver())
						{
							GameMessage* autopilotMsg = TheMessageStream->appendMessage(GameMessage::MSG_SET_AUTOPILOT);
							autopilotMsg->appendIntegerArgument(autopilotSteps[nextAutopilotStep].level);
							log("autopilot request level=%d at frame %u", autopilotSteps[nextAutopilotStep].level, frame);
						}
						++nextAutopilotStep;
					}
					if (frame != lastFrameSeen)
					{
						lastFrameSeen = frame;
						lastFrameMs = t;
					}
					if (frame != 0 && frame % (60u * LOGICFRAMES_PER_SECOND) == 0 && frame != lastStatusFrame)
					{
						lastStatusFrame = frame;
						const UnsignedInt gs = frame / LOGICFRAMES_PER_SECOND;
						log("status frame=%u game=%02u:%02u real=%us crc=%08X", frame, gs / 60, gs % 60, (t - startMs) / 1000,
							TheGameLogic->getCRC(CRC_RECALC));
						// Full per-player stats each game minute: a timeline of the match that can be diffed across machines.
						AsciiString snap = AISkirmishSimulation::buildStatsJson(mapForProgress, seed, FALSE, t - startMs);
						if (snap.isNotEmpty())
							log("progress %s", snap.str());
					}
					if (TheVictoryConditions && TheVictoryConditions->getEndFrame() != 0)
						break;
					if (maxFrames != 0 && frame >= maxFrames)
					{
						timedOut = TRUE;
						break;
					}
					if (!headless)
						sleepMs(1);
				}
				else
				{
					if (lastFrameMs != 0 && t - lastFrameMs > 120000)
					{
						log("error network stalled: no logic frame for 120s at frame %u", lastFrameSeen);
						return 5;
					}
					sleepMs(1);
				}
			}
			else if (sawGame)
			{
				break;	// game cleared itself (disconnect / quit)
			}
			else
			{
				// Game start requested but not running yet: the logic update is what consumes MSG_NEW_GAME and
				// performs the load, so it must keep running before isInGame() turns true (windowed: update() above).
				if (headless)
				{
					TheGameClient->updateHeadless();
					TheGameLogic->UPDATE();
				}
				if (t - phaseSince > 180000)
				{
					log("error game did not start within 180s of the start request");
					return 4;
				}
				sleepMs(1);
			}
		}
		else
		{
			sleepMs(5);
		}
	}

	AsciiString mapForStats = mapName;
	if (mapForStats.isEmpty() && TheLAN && TheLAN->GetMyGame())
		mapForStats = TheLAN->GetMyGame()->getMap();
	if (!seed && TheLAN && TheLAN->GetMyGame())
		seed = TheLAN->GetMyGame()->getSeed();

	AsciiString json = AISkirmishSimulation::buildStatsJson(mapForStats, seed, timedOut, nowMs() - startMs);
	if (json.isNotEmpty())
	{
		printf("NETPLAY_RESULT %s\n", json.str());
		fflush(stdout);
		if (TheGlobalData->m_lanStatsFile.isNotEmpty())
		{
			FILE* f = fopen(TheGlobalData->m_lanStatsFile.str(), "w");
			if (f)
			{
				fprintf(f, "%s\n", json.str());
				fclose(f);
			}
		}
	}
	else
	{
		log("no game stats (game never started)");
	}
	return sawGame ? 0 : 3;
}
