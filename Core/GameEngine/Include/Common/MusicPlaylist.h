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

// GeneralsX @feature Claude 22/10/2026 Player-supplied music. If the player has put audio files under <user data>/Music/<playlist>/ the in-game
// tracks the skirmish scripts ask for (Game_USA_01 ...) are replaced by those files: shuffled, with an equal-power crossfade between songs. The retail
// scripts keep running unchanged (menus, load screens, the victory and defeat stingers stay retail); with no folder nothing changes at all.
// Client-only: nothing here touches game logic, the logic RNG or a checksum. See docs/WORKDIR/planning/FACTION_MUSIC_DESIGN.md.

#pragma once

#include "Common/AsciiString.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/GameAudio.h"

#include <vector>

class MusicPlaylistPlayer
{
public:
	MusicPlaylistPlayer();

	/// The folder holding the playlist folders (with a trailing slash), e.g. "<user data>/Music/". Empty = feature off.
	void setRoot(const AsciiString &root) { m_root = root; m_scanned = FALSE; }

	/// Called by AudioManager::addAudioEvent for every music event. True = swallow it: it is a retail in-game track and a playlist is playing instead.
	Bool shouldSuppress(const AsciiString &eventName);

	/// Called when a music stop is requested (AHSV_StopTheMusic[Fade]). Scripts stop then start a track on the same frame; the playlist only
	/// ends if no retail in-game track follows (a stinger, a load screen).
	void onStopMusic();

	/// The game paused or resumed its music (the clock that times the crossfades stops while it is paused).
	void setMusicPaused(Bool paused) { m_paused = paused; }

	/// Once per client update.
	void update();

	/// A new game or a return to the menus: forget everything and stop.
	void reset();

private:
	struct Playing
	{
		AsciiString name;		///< unique audio event name of this play of a track
		AudioHandle handle;
		Int startMs;			///< playlist clock when it started
		Int durationMs;			///< -1 if unknown
	};

	Bool prepare();							///< scan once and pick the playlist for the local player; false if there is nothing to play
	AsciiString pickPlaylistFolder(const std::vector<AsciiString> &files, std::vector<AsciiString> &chosen) const;
	Bool startTrack(Playing &out, Real firstGain);
	Int nextFileIndex();
	void stopTrack(Playing &p);
	void setGain(Playing &p, Real gain);

	AsciiString m_root;
	Bool m_scanned;
	Bool m_available;						///< a playlist with at least one file exists for this player
	Bool m_active;							///< the playlist is (supposed to be) playing
	Bool m_paused;
	AsciiString m_folder;					///< the playlist chosen (for the log)
	std::vector<AsciiString> m_files;
	std::vector<Int> m_bag;					///< shuffle bag: indexes still to play this round
	Int m_lastIndex;
	Int m_trackCounter;
	Int m_failures;							///< tracks in a row that failed to play

	Playing m_cur;
	Playing m_next;
	Bool m_haveCur;
	Bool m_haveNext;					///< a crossfade is running
	Int m_fadeStartMs;
	Int m_fadeInStartMs;				///< the first track fades in; -1 when not
	Int m_fadeOutStartMs;				///< stopping: fading the current track out; -1 when not
	Bool m_stopPending;
	Int m_stopRequestedMs;

	Int m_nowMs;						///< playlist clock: advances with real time, but not while the music is paused
	long long m_lastTickMs;
};
