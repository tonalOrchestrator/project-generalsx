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

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameClient/AutopilotPanel.h"
#include "GameClient/Color.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Mouse.h"
#include "GameLogic/AutopilotLog.h"
#include "GameLogic/AutopilotProfile.h"
#include "GameLogic/GameLogic.h"

namespace
{
	const Int PANEL_W = 232;		// in 800x600 pixels; scaled with the screen height
	const Int ROW_H = 16;
	const Int PAD = 6;
	const Int FEED_LINES = 4;

	struct ModuleRow { UnsignedInt bit; const char *label; };
	const ModuleRow s_rows[] =
	{
		{ AM_WALLET, "Wallet: defend, hoard cash" },
		{ AM_EXPLORE, "Explore: capture buildings" },
		{ AM_SUPERWEAPONS, "Superweapons: build + fire" },
		{ AM_SQUAD, "Squad: garrison buildings" },
		{ AM_HACKERS, "Hackers (China)" },
		{ AM_REPAIR, "Repair: dozer fixes base" },
		{ AM_DEFENSE_UNITS, "Defense units: standing army" },
	};
	const Int NUM_ROWS = (Int)(sizeof(s_rows) / sizeof(s_rows[0]));
	const char *const s_levelNames[] = { "OFF", "Easy", "Normal", "Hard", "Wallet", "Explore", "Fortress" };

	GameWindow *s_window = nullptr;
	DisplayString *s_text = nullptr;

	Real scale() { return TheDisplay ? (Real)TheDisplay->getHeight() / 600.0f : 1.0f; }
	Int px(Int v) { return (Int)(v * scale() + 0.5f); }

	Player *localHuman()
	{
		Player *p = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		return (p && p->getPlayerType() == PLAYER_HUMAN && !p->isPlayerObserver()) ? p : nullptr;
	}

	UnsignedInt currentModules(const Player *p)
	{
		const AutopilotProfile *profile = AutopilotProfile::forLevel(p->getAutopilotLevel());
		return profile ? profile->withModules(p->getAutopilotModules()).modules() : 0;
	}

	Int panelHeight(const Player *p)
	{
		if (p->getAutopilotLevel() <= 0)
			return px(ROW_H + 2 * PAD);
		return px(PAD + ROW_H + 4 + NUM_ROWS * ROW_H + 6 + FEED_LINES * (ROW_H - 2) + PAD);
	}

	void sendLevel(Int level, UnsignedInt modules)
	{
		GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_SET_AUTOPILOT);
		msg->appendIntegerArgument(level);
		if (modules != 0xFFFFFFFFu)
			msg->appendIntegerArgument((Int)modules);
	}

	void drawText(const char *text, Int x, Int y, Color color)
	{
		if (!s_text)
		{
			s_text = TheDisplayStringManager->newDisplayString();
			// Arial 11: at 10 (and 10 bold) some glyphs render with a stray line above them in this engine's font cache; 11 and up are clean.
			s_text->setFont(TheFontLibrary->getFont("Arial", 11, FALSE));
		}
		UnicodeString line;
		line.translate(AsciiString(text));
		s_text->setText(line);
		s_text->draw(x, y, color, GameMakeColor(0, 0, 0, 255));
	}

	void drawPanel(GameWindow *window, WinInstanceData *)
	{
		Player *p = localHuman();
		if (!p || !TheDisplay)
			return;
		Int x, y, w, h;
		window->winGetScreenPosition(&x, &y);
		window->winGetSize(&w, &h);
		TheDisplay->drawFillRect(x, y, w, h, GameMakeColor(8, 12, 24, 205));
		TheDisplay->drawOpenRect(x, y, w, h, 1.0f, GameMakeColor(120, 160, 220, 255));

		const Int level = p->getAutopilotLevel();
		const Int tx = x + px(PAD);
		Int ty = y + px(PAD);
		char title[96];
		snprintf(title, sizeof(title), level > 0 ? "AUTOPILOT: %s  (click)" : "AUTOPILOT: OFF  (click: on)", s_levelNames[level < 0 ? 0 : (level > 6 ? 6 : level)]);
		drawText(title, tx, ty, level > 0 ? GameMakeColor(120, 255, 150, 255) : GameMakeColor(255, 200, 90, 255));
		if (level <= 0)
			return;

		ty += px(ROW_H + 4);
		const UnsignedInt modules = currentModules(p);
		for (Int i = 0; i < NUM_ROWS; ++i, ty += px(ROW_H))
		{
			const Bool on = (modules & s_rows[i].bit) != 0;
			const Int box = px(9);
			TheDisplay->drawOpenRect(tx, ty + px(2), box, box, 1.0f, GameMakeColor(200, 210, 230, 255));
			if (on)
				TheDisplay->drawFillRect(tx + 2, ty + px(2) + 2, box - 3, box - 3, GameMakeColor(120, 255, 150, 255));
			drawText(s_rows[i].label, tx + box + px(6), ty, on ? GameMakeColor(235, 240, 250, 255) : GameMakeColor(130, 140, 160, 255));
		}

		ty += px(6);
		TheDisplay->drawLine(tx, ty - px(3), x + w - px(PAD), ty - px(3), 1.0f, GameMakeColor(80, 100, 140, 255));
		for (Int i = 0; i < FEED_LINES; ++i, ty += px(ROW_H - 2))
		{
			AsciiString line;
			if (AutopilotLog::recentFeed(i, &line))
				drawText(line.str(), tx, ty, i == 0 ? GameMakeColor(255, 235, 140, 255) : GameMakeColor(170, 180, 200, 255));
		}
	}

	WindowMsgHandledType panelInput(GameWindow *, UnsignedInt msg, WindowMsgData, WindowMsgData)
	{
		if (msg == GWM_LEFT_UP && TheMouse)
		{
			const MouseIO *io = TheMouse->getMouseStatus();
			AutopilotPanel::click(io->pos.x, io->pos.y);
			return MSG_HANDLED;
		}
		return (msg == GWM_LEFT_DOWN || msg == GWM_RIGHT_DOWN || msg == GWM_RIGHT_UP) ? MSG_HANDLED : MSG_IGNORED;
	}

	WindowMsgHandledType panelSystem(GameWindow *, UnsignedInt, WindowMsgData, WindowMsgData)
	{
		return MSG_IGNORED;
	}
}

Bool AutopilotPanel::click(Int x, Int y)
{
	Player *p = localHuman();
	if (!p || !s_window)
		return FALSE;
	Int wx, wy, ww, wh;
	s_window->winGetScreenPosition(&wx, &wy);
	s_window->winGetSize(&ww, &wh);
	if (x < wx || x >= wx + ww || y < wy || y >= wy + wh)
		return FALSE;

	const Int level = p->getAutopilotLevel();
	const Int rel = y - wy - px(PAD);
	if (level <= 0 || rel < px(ROW_H + 4))
	{
		// title: off -> Fortress (the strongest), otherwise cycle up and wrap to off
		sendLevel(level <= 0 ? 6 : (level + 1) % 7, 0xFFFFFFFFu);
		return TRUE;
	}
	const Int row = (rel - px(ROW_H + 4)) / px(ROW_H);
	if (row >= 0 && row < NUM_ROWS)
	{
		sendLevel(level, currentModules(p) ^ s_rows[row].bit);
		return TRUE;
	}
	return FALSE;
}

void AutopilotPanel::update()
{
	Player *p = (TheGameLogic && TheGameLogic->isInGame() && !TheGlobalData->m_headless) ? localHuman() : nullptr;
	if (!p)
	{
		if (s_window)
			s_window->winHide(TRUE);
		return;
	}
	if (s_window && TheWindowManager->winGetWindowFromId(nullptr, s_window->winGetWindowId()) != s_window)
		s_window = nullptr;	// the match's teardown removed it
	if (!s_window)
	{
		s_window = TheWindowManager->winCreate(nullptr, WIN_STATUS_ENABLED | WIN_STATUS_NO_FOCUS, px(8), px(58), px(PANEL_W), panelHeight(p), panelSystem);
		if (!s_window)
			return;
		s_window->winSetInputFunc(panelInput);
		s_window->winSetDrawFunc(drawPanel);
	}
	s_window->winHide(FALSE);
	Int w, h;
	s_window->winGetSize(&w, &h);
	if (h != panelHeight(p))
		s_window->winSetSize(px(PANEL_W), panelHeight(p));
}

void AutopilotPanel::reset()
{
	s_window = nullptr;
}
