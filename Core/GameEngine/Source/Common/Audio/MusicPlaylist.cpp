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

#include "Common/MusicPlaylist.h"

#include "Common/AudioEventInfo.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioRandomValue.h"
#include "Common/FileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace
{
	const Int CROSSFADE_MS = 4000;		///< length of the overlap between two songs
	const Int FADE_IN_MS = 2000;		///< the first song of a game
	const Int FADE_OUT_MS = 1500;		///< when the playlist is stopped
	const Int STOP_GRACE_MS = 250;		///< scripts stop one track and start the next on the same frame; a stop only counts if nothing follows
	const Int MAX_FILES = 200;
	const Real MIN_GAIN = 0.03f;		///< the audio manager culls events quieter than its minimum volume
	const Int MIN_TRACK_MS = 2 * CROSSFADE_MS + 2000;	///< shorter songs are played without overlap

	Real equalPower(Real t, Bool rising)
	{
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;
		return rising ? (Real)sin(t * 1.57079632679) : (Real)cos(t * 1.57079632679);
	}

	long long steadyMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	AsciiString lowered(const AsciiString &s)
	{
		AsciiString r = s;
		r.toLower();
		return r;
	}

	/// The name of the folder a file sits in ("<root>/usa/01.mp3" -> "usa"), compared case-insensitively.
	AsciiString parentFolder(const AsciiString &path)
	{
		std::string p(path.str());
		std::replace(p.begin(), p.end(), '\\', '/');
		const size_t last = p.rfind('/');
		if (last == std::string::npos)
			return AsciiString::TheEmptyString;
		const size_t prev = p.rfind('/', last == 0 ? 0 : last - 1);
		const std::string name = p.substr(prev == std::string::npos ? 0 : prev + 1, last - (prev == std::string::npos ? 0 : prev + 1));
		return lowered(AsciiString(name.c_str()));
	}
}

MusicPlaylistPlayer::MusicPlaylistPlayer()
	: m_scanned(FALSE), m_available(FALSE), m_active(FALSE), m_paused(FALSE), m_lastIndex(-1), m_trackCounter(0), m_failures(0),
	  m_haveCur(FALSE), m_haveNext(FALSE), m_fadeStartMs(0), m_fadeInStartMs(-1), m_fadeOutStartMs(-1), m_stopPending(FALSE), m_stopRequestedMs(0),
	  m_nowMs(0), m_lastTickMs(0)
{
	m_cur.handle = m_next.handle = AHSV_NoSound;
	m_cur.startMs = m_next.startMs = 0;
	m_cur.durationMs = m_next.durationMs = -1;
}

// The playlist folders to try, best first: the template's own playlist id, the template's name ("FactionAmericaAirForceGeneral" ->
// "americaairforcegeneral"), its side and base side ("america" also answers to "usa"), then "default".
AsciiString MusicPlaylistPlayer::pickPlaylistFolder(const std::vector<AsciiString> &files, std::vector<AsciiString> &chosen) const
{
	std::vector<AsciiString> candidates;
	Player *local = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
	const PlayerTemplate *tmpl = local ? local->getPlayerTemplate() : nullptr;
	if (tmpl)
	{
		if (tmpl->isObserver())
			candidates.push_back(AsciiString("observer"));
		if (!tmpl->getMusicPlaylist().isEmpty())
			candidates.push_back(lowered(tmpl->getMusicPlaylist()));
		AsciiString name = lowered(tmpl->getName());
		if (strncmp(name.str(), "faction", 7) == 0)
			name = AsciiString(name.str() + 7);
		candidates.push_back(name);
		const AsciiString sides[2] = { lowered(tmpl->getSide()), lowered(tmpl->getBaseSide()) };
		for (Int i = 0; i < 2; ++i)
		{
			if (sides[i].isEmpty())
				continue;
			candidates.push_back(sides[i]);
			if (strcmp(sides[i].str(), "america") == 0)
				candidates.push_back(AsciiString("usa"));
		}
	}
	candidates.push_back(AsciiString("default"));

	for (size_t c = 0; c < candidates.size(); ++c)
	{
		chosen.clear();
		for (size_t f = 0; f < files.size(); ++f)
			if (parentFolder(files[f]) == candidates[c])
				chosen.push_back(files[f]);
		if (!chosen.empty())
			return candidates[c];
	}
	return AsciiString::TheEmptyString;
}

Bool MusicPlaylistPlayer::prepare()
{
	if (m_scanned)
		return m_available;
	if (m_root.isEmpty() || !TheLocalFileSystem || !ThePlayerList || !ThePlayerList->getLocalPlayer())
		return FALSE;	// not ready yet; try again on the next request
	m_scanned = TRUE;
	m_available = FALSE;
	m_files.clear();
	m_bag.clear();
	m_lastIndex = -1;

	static const char *const patterns[] = { "*.mp3", "*.ogg", "*.flac", "*.wav", "*.m4a", "*.opus" };
	std::vector<AsciiString> all;
	for (size_t p = 0; p < sizeof(patterns) / sizeof(patterns[0]); ++p)
	{
		FilenameList found;
		TheLocalFileSystem->getFileListInDirectory(AsciiString::TheEmptyString, m_root, AsciiString(patterns[p]), found, TRUE);
		for (FilenameListIter it = found.begin(); it != found.end() && (Int)all.size() < MAX_FILES * 4; ++it)
			all.push_back(*it);
	}
	std::sort(all.begin(), all.end(), [](const AsciiString &a, const AsciiString &b) { return strcmp(a.str(), b.str()) < 0; });

	m_folder = pickPlaylistFolder(all, m_files);
	if (m_files.size() > (size_t)MAX_FILES)
		m_files.resize(MAX_FILES);
	m_available = !m_files.empty();
	fprintf(stderr, "MUSIC playlist: root '%s', %d audio files found, folder '%s' (%d songs) %s\n", m_root.str(), (Int)all.size(),
		m_folder.isEmpty() ? "-" : m_folder.str(), (Int)m_files.size(), m_available ? "-> custom music" : "-> retail music");
	return m_available;
}

Int MusicPlaylistPlayer::nextFileIndex()
{
	const Int n = (Int)m_files.size();
	if (m_bag.empty())
	{
		for (Int i = 0; i < n; ++i)
			m_bag.push_back(i);
		for (Int i = n - 1; i > 0; --i)
			std::swap(m_bag[i], m_bag[GameAudioRandomValue(0, i)]);
		if (n > 1 && m_bag.back() == m_lastIndex)
			std::swap(m_bag.back(), m_bag.front());	// never the same song twice in a row across two rounds
	}
	const Int idx = m_bag.back();
	m_bag.pop_back();
	m_lastIndex = idx;
	return idx;
}

Bool MusicPlaylistPlayer::startTrack(Playing &out, Real firstGain)
{
	const Int idx = nextFileIndex();
	AsciiString name;
	name.format("__playlist_%d_%d", idx, (m_trackCounter++) & 1);	// the same song twice at once (one-song playlist) needs two names
	AudioEventInfo *info = TheAudio->findAudioEventInfo(name);
	if (!info)
		info = TheAudio->newAudioEventInfo(name);
	if (!info)
		return FALSE;
	if (AudioEventInfo *defaults = TheAudio->findAudioEventInfo("DefaultMusicTrack"))
		*info = *defaults;
	info->m_audioName = name;
	info->m_soundType = AT_Music;
	info->m_filename = m_files[idx];
	info->m_filenameIsFullPath = TRUE;

	AudioEventRTS event(name);
	event.setVolume(firstGain);
	event.setShouldFade(false);
	const AudioHandle handle = TheAudio->addAudioEvent(&event);
	if (handle < AHSV_FirstHandle)
	{
		fprintf(stderr, "MUSIC playlist: could not start '%s' (handle %d)\n", m_files[idx].str(), (Int)handle);
		return FALSE;
	}
	out.name = name;
	out.handle = handle;
	out.startMs = m_nowMs;
	out.durationMs = TheAudio->getMusicDurationMs(m_files[idx]);
	fprintf(stderr, "MUSIC playlist: [%d ms] playing '%s' (%d ms)\n", m_nowMs, m_files[idx].str(), out.durationMs);
	return TRUE;
}

void MusicPlaylistPlayer::stopTrack(Playing &p)
{
	if (p.handle >= AHSV_FirstHandle)
		TheAudio->removeAudioEvent(p.handle);
	p.handle = AHSV_NoSound;
}

void MusicPlaylistPlayer::setGain(Playing &p, Real gain)
{
	if (p.handle >= AHSV_FirstHandle)
		TheAudio->adjustVolumeOfPlayingAudio(p.name, gain < MIN_GAIN ? MIN_GAIN : gain);
}

Bool MusicPlaylistPlayer::shouldSuppress(const AsciiString &eventName)
{
	if (strncmp(eventName.str(), "Game_", 5) != 0)
		return FALSE;	// menus, load screens and the victory/defeat stingers stay retail
	if (!prepare())
		return FALSE;
	m_stopPending = FALSE;
	if (m_fadeOutStartMs >= 0)
	{
		m_fadeOutStartMs = -1;	// a new retail track while stopping: keep going
		if (m_haveCur)
			setGain(m_cur, 1.0f);
	}
	m_active = TRUE;
	return TRUE;
}

void MusicPlaylistPlayer::onStopMusic()
{
	if (!m_active || m_stopPending)
		return;
	m_stopPending = TRUE;
	m_stopRequestedMs = (Int)(steadyMs() & 0x3fffffff);
}

void MusicPlaylistPlayer::reset()
{
	if (m_haveCur) stopTrack(m_cur);
	if (m_haveNext) stopTrack(m_next);
	m_haveCur = m_haveNext = FALSE;
	m_active = FALSE;
	m_stopPending = FALSE;
	m_fadeInStartMs = m_fadeOutStartMs = -1;
	m_scanned = FALSE;	// the next game may have another local player
	m_available = FALSE;
	m_failures = 0;
}

void MusicPlaylistPlayer::update()
{
	const long long real = steadyMs();
	long long dt = m_lastTickMs ? real - m_lastTickMs : 0;
	m_lastTickMs = real;
	if (dt < 0) dt = 0;
	if (dt > 250) dt = 250;	// a stall (loading, a breakpoint) must not skip songs
	if (!m_paused)
		m_nowMs += (Int)dt;
	if (!m_active && m_fadeOutStartMs < 0)
		return;

	if (m_stopPending && ((Int)(real & 0x3fffffff) - m_stopRequestedMs) >= STOP_GRACE_MS)
	{
		m_stopPending = FALSE;
		m_fadeOutStartMs = m_nowMs;
		m_active = FALSE;
		fprintf(stderr, "MUSIC playlist: stopped by the game (a stinger or a screen change); fading out\n");
	}

	if (m_fadeOutStartMs >= 0)
	{
		const Real t = (Real)(m_nowMs - m_fadeOutStartMs) / FADE_OUT_MS;
		const Real g = equalPower(t, FALSE);
		if (m_haveCur) setGain(m_cur, g);
		if (m_haveNext) setGain(m_next, g);
		if (t >= 1.0f)
		{
			if (m_haveCur) stopTrack(m_cur);
			if (m_haveNext) stopTrack(m_next);
			m_haveCur = m_haveNext = FALSE;
			m_fadeOutStartMs = -1;
		}
		return;
	}

	if (!m_haveCur)
	{
		if (!startTrack(m_cur, MIN_GAIN))
		{
			if (++m_failures >= (Int)m_files.size())
			{
				fprintf(stderr, "MUSIC playlist: no song could be played; back to retail music\n");
				m_active = FALSE;
				m_available = FALSE;
			}
			return;
		}
		m_haveCur = TRUE;
		m_fadeInStartMs = m_nowMs;
		return;
	}

	if (m_fadeInStartMs >= 0)
	{
		const Real t = (Real)(m_nowMs - m_fadeInStartMs) / FADE_IN_MS;
		setGain(m_cur, equalPower(t, TRUE));
		if (t >= 1.0f)
			m_fadeInStartMs = -1;
	}

	if (m_haveNext)
	{
		const Real t = (Real)(m_nowMs - m_fadeStartMs) / CROSSFADE_MS;
		setGain(m_cur, equalPower(t, FALSE));
		setGain(m_next, equalPower(t, TRUE));
		if (t >= 1.0f)
		{
			stopTrack(m_cur);
			m_cur = m_next;
			m_haveNext = FALSE;
			m_failures = 0;
			setGain(m_cur, 1.0f);
		}
		return;
	}

	const Int age = m_nowMs - m_cur.startMs;
	if (m_cur.durationMs >= MIN_TRACK_MS && age >= m_cur.durationMs - CROSSFADE_MS)
	{
		if (startTrack(m_next, MIN_GAIN))
		{
			m_haveNext = TRUE;
			m_fadeStartMs = m_nowMs;
			fprintf(stderr, "MUSIC playlist: [%d ms] crossfading (%d ms)\n", m_nowMs, CROSSFADE_MS);
		}
		else
		{
			++m_failures;
			m_cur.durationMs = -1;	// do not retry every frame: fall back to waiting for it to end
		}
		return;
	}

	// No usable length (or a short song), or the song died early: when it is no longer playing, start the next one without overlap.
	if (age > 2000 && !TheAudio->isCurrentlyPlaying(m_cur.handle))
	{
		stopTrack(m_cur);
		m_haveCur = FALSE;
	}
}
