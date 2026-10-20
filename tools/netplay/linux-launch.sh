#!/usr/bin/env bash
# Launch the Linux GeneralsX build windowed inside a private virtual X display (no real screen needed).
# Usage: linux-launch.sh [extra game args...]   env: NETPLAY_DISPLAY (default :99), NETPLAY_LOG
set -euo pipefail
DISP=${NETPLAY_DISPLAY:-:99}
GAME_DIR=${GAME_DIR:-$HOME/GeneralsX/GeneralsZH}
LOG=${NETPLAY_LOG:-$(dirname "$0")/../../logs/netplay-linux.log}
mkdir -p "$(dirname "$LOG")"

if ! xdpyinfo -display "$DISP" >/dev/null 2>&1; then
  Xvfb "$DISP" -screen 0 1024x768x24 >/tmp/netplay-xvfb.log 2>&1 &
  sleep 2
fi
cd "$GAME_DIR"
export DISPLAY=$DISP SDL_VIDEODRIVER=x11 DXVK_WSI_DRIVER=SDL3 LD_LIBRARY_PATH="$GAME_DIR" \
       VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
nohup ./GeneralsXZH -win -xres 800 -yres 600 "$@" >"$LOG" 2>&1 &
echo $! > /tmp/netplay-linux.pid
echo "started pid $(cat /tmp/netplay-linux.pid) on $DISP, log $LOG"
