# Root Cause Analysis: Challenge Generals Screen Bug

**Report Date**: 2026-04-27  
**Bug Branch**: `bugfix/challenge-generalsscreen`  
**Likely Culprit Commit**: `c0ce9f0` (2026-04-23)  

## Commit c0ce9f0 Analysis

### Commit Metadata
- **Hash**: c0ce9f00d2adc2aaf3b29fc4775a85e9259ab1b7
- **Title**: fix(audio-openal): restore briefing video audio (#100)
- **Related Issue**: #38
- **Date**: 2026-04-23 (4 days BEFORE Challenge bug report)

### Changes to ChallengeLoadScreen

The commit introduced **two key changes** to `ChallengeLoadScreen::init()`:

#### Change 1: NULL-check for m_videoStream (line ~962)
```cpp
m_videoStream = TheVideoPlayer->open( TheCampaignManager->getCurrentMission()->m_movieLabel );
if (m_videoStream == nullptr)  // <-- ADDED
{
    return;
}
```

**Impact**: Harmless safety improvement. Exits early if video fails to load.

#### Change 2: Added TheVideoPlayer->update() in blocking loop (line ~1090)
```cpp
// Inside the while(m_videoStream->frameIndex() < m_videoStream->frameCount() - 1) loop:
m_videoStream->frameDecompress();
m_videoStream->frameRender(m_videoBuffer);
m_videoStream->frameNext();

// ADDED:
if (TheVideoPlayer)
    TheVideoPlayer->update();  // <-- CRITICAL CHANGE

if(m_videoBuffer)
    m_loadScreen->winGetInstanceData()->setVideoBuffer(m_videoBuffer);
```

**Stated Purpose**: Fix OpenAL audio underruns by ensuring `TheVideoPlayer->update()` is called in the blocking loop (which never returns to main game loop).

**Potential Issue**: This call happens **BEFORE** `m_wndVideoManager->update()` in the frame order!

### Current Code Order (After c0ce9f0)

In the ChallengeLoadScreen::init() blocking loop:

```cpp
while (m_videoStream->frameIndex() < m_videoStream->frameCount() - 1) {
    // ... decompress/render main video ...
    m_videoStream->frameNext();
    
    // 1. NEW: TheVideoPlayer->update() - updates FFmpeg streams
    if (TheVideoPlayer)
        TheVideoPlayer->update();
    
    // 2. Set main video buffer to window
    if(m_videoBuffer)
        m_loadScreen->winGetInstanceData()->setVideoBuffer(m_videoBuffer);
    
    // 3. Update GUI/Windows
    TheWindowManager->update();
    
    // 4. Call activatePieces() - adds portrait/VS videos to WindowVideoManager
    activatePieces(frame, generalPlayer, generalOpponent);
    
    // 5. Update ALL window videos - SHOULD update portraits/VS
    m_wndVideoManager->update();
    
    // 6. Render
    TheDisplay->draw();
    
    // 7. Audio
    TheAudio->update();
}
```

### Hypothesis: Resource Contention

Three video streams are competing for resources:

1. **Main background video** (m_videoStream)
   - Being actively decompressed and rendered every frame
   - TheVideoPlayer->update() now called explicitly
   
2. **Portrait videos** (m_portraitMovieLeft, m_portraitMovieRight)
   - Added to WindowVideoManager at frame 35
   - Updated only via m_wndVideoManager->update()
   
3. **VS overlay video** (m_overlayVs)
   - Added to WindowVideoManager at frame 98
   - Updated only via m_wndVideoManager->update()

**Potential Problem**:
- If `TheVideoPlayer->update()` (step 1) now dominates processing time
- OR if FFmpegVideoStream changes in c0ce9f0 altered buffer management
- Then `m_wndVideoManager->update()` (step 5) might:
  - Not get enough CPU time to decompress portrait/VS video frames
  - Lose frame buffering due to changed FFmpeg behavior
  - Not advance video index, causing infinite loop (matching user's "loop dentro da outra")

### Supporting Evidence from c0ce9f0

The commit also changed:
- `FFmpegVideoPlayer.cpp` (~70 lines modified)
- `OpenALAudioStream.cpp` (~81 lines modified)

These changes to video buffer management and audio queueing could have inadvertently:
- Changed how video decompression buffers are allocated
- Altered timing of buffer availability
- Changed how window videos share resources with main video

## Investigation Strategy

### Phase 1: Confirm NULL Pointers or Failed Initialization
- Add logging to verify `m_portraitMovieLeft/Right` and `m_overlayVs` are not NULL
- Add logging to verify `TheVideoPlayer->open()` succeeds for portrait movies
- Status: **COMPLETED** (Commit 3940f2d03)

### Phase 2: Verify Video Playback Progresses
- Add logging to `WindowVideoManager::update()` to trace portrait/VS video frame advancement
- Check if videos get to `frameIndex() == 0` (which triggers pause)
- Verify portrait/VS videos actually enter `m_playingVideos` map

### Phase 3: Attribute to Commit c0ce9f0 Effects
Based on Phase 2 results:
- If videos never enter playback → resource starvation or timing issue
- If videos pause immediately → `frameIndex()` calculation broken
- If videos loop infinitely → pause logic broken

### Phase 4: Apply Targeted Fix
Options:
1. **Revert c0ce9f0**: Simplest but loses OpenAL audio fix
2. **Reorder operations**: Move TheVideoPlayer->update() AFTER m_wndVideoManager->update()
3. **Decouple updates**: Call TheVideoPlayer->update() less frequently
4. **Fix WindowVideoManager**: Ensure it doesn't starve in competitive scenario

## Recommendation

**DO NOT revert c0ce9f0** - it fixes a real audio issue (#38).

Instead:
1. Complete Phase 1 logging (already done)
2. Build debug and run to get Phase 2 data
3. Based on results, apply surgical fix to ChallengeLoadScreen update order
4. Test that both audio AND video work

Most likely fix: **Move `m_wndVideoManager->update()` BEFORE `TheVideoPlayer->update()`** to ensure window videos get processed first, then top up the main video stream.
