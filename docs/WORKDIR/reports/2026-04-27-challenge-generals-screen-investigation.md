# Challenge Generals Screen Bug Investigation Report

**Date**: 2026-04-27  
**Branch**: `bugfix/challenge-generalsscreen`  
**Status**: In Progress  

## Bug Description

When opening a "Challenge of the Generals" mission, the loading screen displays incorrectly:

**Expected behavior:**
- Portrait video of left general (animated)
- "VS" animation in center
- Portrait video of right general (animated)
- Text and UI elements showing general bios

**Actual behavior:**
- Generals portraits do NOT appear or display incorrectly
- "VS" animation loops incorrectly ("um tipo de loop uma dentro da outra")
- Animation appears to nest/multiply instead of playing sequentially

## Root Cause Analysis

### Investigation Summary

1. **Code Flow**
   - Main loading screen loop calls `activatePieces(frame)` for each frame of main video
   - `activatePieces()` triggers specific UI updates at frame checkpoints:
     - Frame 35 (`FRAME_PORTRAITS_START`): Should start portrait video playback
     - Frame 98 (`FRAME_VS_ANIM_START`): Should start VS overlay animation
   - `WindowVideoManager::update()` updates all active window videos each frame

2. **Potential Issues Found**
   - Window pointers (`m_portraitMovieLeft`, `m_portraitMovieRight`, `m_overlayVs`) may not be initialized correctly if layout file missing window IDs
   - Video files may not exist or have format issues
   - No NULL checks before calling `playMovie()` or `winHide()`
   - Duplicate calls to `m_overlayVsBackdrop->winHide(FALSE)` in `activatePiecesMinSpec()`
   - Video pause logic may cause unexpected looping

### Code Structure

**File**: `Core/GameEngine/Source/GameClient/GUI/LoadScreen.cpp`

- `ChallengeLoadScreen::init()` - Initializes all window pointers from layout script
- `ChallengeLoadScreen::activatePieces()` - Triggered for each frame in main loop; calls `playMovie()` at specific frames
- `ChallengeLoadScreen::activatePiecesMinSpec()` - Fallback for low-end systems; skips video and uses statics

**File**: `Core/GameEngine/Source/GameClient/GUI/WindowVideoManager.cpp`

- `WindowVideoManager::playMovie()` - Opens video stream and adds to playback queue
- `WindowVideoManager::update()` - Updates all active videos; pauses/stops when `frameIndex() == 0`

## Changes Made

### 1. Added Logging in `activatePieces()` - FRAME_PORTRAITS_START

Added NULL checks and logging:
```cpp
if (m_portraitMovieLeft && generalPlayer->getPortraitMovieLeftName().isNotEmpty())
{
    DEBUG_LOG(("ChallengeLoadScreen: Playing portrait movie LEFT: %s\n", ...));
    m_wndVideoManager->playMovie(...);
    m_portraitMovieLeft->winHide(FALSE);
}
else
{
    DEBUG_LOG(("ChallengeLoadScreen: WARNING - Portrait movie LEFT not available..."));
}
```

**Line**: 827-846 in LoadScreen.cpp

### 2. Added Logging in `activatePieces()` - FRAME_VS_ANIM_START

Added NULL check before `playMovie()` call:
```cpp
DEBUG_LOG(("ChallengeLoadScreen: FRAME_VS_ANIM_START - Playing VS overlay (vs=%p)\n", m_overlayVs));
m_overlayVsBackdrop->winHide(FALSE);
m_overlayVs->winHide(FALSE);
if (m_overlayVs)
{
    m_wndVideoManager->playMovie(m_overlayVs, "VSSmall", WINDOW_PLAY_MOVIE_SHOW_LAST_FRAME);
}
else
{
    DEBUG_LOG(("ChallengeLoadScreen: ERROR - m_overlayVs window is NULL\n"));
}
```

**Line**: 880-892 in LoadScreen.cpp

### 3. Added Logging in `activatePiecesMinSpec()`

Removed duplicate call and added NULL checks:
```cpp
// Removed: m_overlayVsBackdrop->winHide(FALSE);  // <- duplicate
m_overlayVs->winHide(FALSE);
DEBUG_LOG(("ChallengeLoadScreen::activatePiecesMinSpec: Playing VS overlay (vs=%p)\n", m_overlayVs));
if (m_overlayVs)
{
    m_wndVideoManager->playMovie(m_overlayVs, "VSSmall", WINDOW_PLAY_MOVIE_SHOW_LAST_FRAME);
}
else
{
    DEBUG_LOG(("ChallengeLoadScreen::activatePiecesMinSpec: ERROR - m_overlayVs window is NULL\n"));
}
```

**Line**: 957-967 in LoadScreen.cpp

### 4. Added Logging in `init()` - Window Initialization

Added logging when window pointers are retrieved:
```cpp
m_portraitMovieLeft = TheWindowManager->winGetWindowFromId(m_loadScreen, namekey);
DEBUG_LOG(("ChallengeLoadScreen::init: m_portraitMovieLeft = %p\n", m_portraitMovieLeft));

m_portraitMovieRight = TheWindowManager->winGetWindowFromId(m_loadScreen, namekey);
DEBUG_LOG(("ChallengeLoadScreen::init: m_portraitMovieRight = %p\n", m_portraitMovieRight));

// ... later in VS overlay initialization
DEBUG_LOG(("ChallengeLoadScreen::init: m_overlayVsBackdrop = %p, m_overlayVs = %p\n", m_overlayVsBackdrop, m_overlayVs));
```

**Line**: 984-1052 in LoadScreen.cpp

## Findings from Logging

The logging will help identify:

1. **Missing Windows**: If pointers are NULL, window IDs don't exist in layout file
2. **Missing Videos**: If `playMovie()` fails silently, video files may not exist
3. **Invalid Movie Names**: If portrait movie names are empty strings
4. **Execution Path**: Whether `activatePieces()` or `activatePiecesMinSpec()` is being used

## Next Steps

1. **Build and Test**
   - Compile with debug logging enabled
   - Run Challenge mission and check debug output
   - Verify window pointers are initialized correctly
   - Verify video streams are opened successfully

2. **Potential Fixes** (based on logging results)
   - If windows are NULL: Fix layout file to define missing window IDs
   - If videos fail to open: Check video file names and formats
   - If pause logic broken: Fix `WindowVideoManager::pauseMovie()` or `frameIndex()` calculation

3. **Long-term**
   - Add more robust error handling
   - Consider fallback behavior if videos unavailable
   - Add assertion/warnings for missing critical resources

## Files Modified

- `Core/GameEngine/Source/GameClient/GUI/LoadScreen.cpp`
  - Added logging to track window initialization
  - Added logging to track video playback
  - Added NULL checks before calling window methods
  - Fixed duplicate `winHide()` call in `activatePiecesMinSpec()`

## Commit Message Template

```
fix(loadscreen): add logging and NULL checks to Challenge Generals screen

Debug portrait and VS animations that fail to display in Challenge mission.
Added logging to identify missing window IDs or video streams.
Fixed duplicate winHide() call in min-spec path.

Addresses: Challenge Generals screen shows no portraits and VS animation loops incorrectly
```
