#!/usr/bin/env bash
# Run a headless two-machine LAN match with AI opponents: Linux hosts, the Windows box (ssh alias
# "vadapuppy") joins. Collects both logs + stats and compares them (desync check).
#
# Usage: tools/netplay/run-netplay.sh [--map NAME] [--ai H,H] [--observer] [--minutes N] [--seed N]
#   --map      map substring (default "Tournament Desert")   --ai    AI slots (E/M/H[:Template], comma separated)
#   --autopilot "10:2,70:0,140:3"  schedule autopilot level changes (game-second:level, 0 off 1 easy 2 normal 3 hard) on both machines
#   --fast     skip the real-time frame pacing on both machines (headless runs): a match takes minutes of game time in seconds-ish of
#              wall time, limited by the slower machine and the network; the game itself is unchanged
#   --build-schedule "130:barracks"  every human orders a structure by hand at that game-second (headless runs)
#   --camera-local  windowed runs: each machine's camera stays on its own base (two human players) instead of hopping between AIs
#   --cheats   pass -cheats to both machines (money / general-point hotkeys)
#   --cheat-schedule "30:5000:1"  gift money / general points from a test run (game-second:money:points; headless runs, needs --cheats)
#   --linux-watch the Linux host also runs windowed on this machine's desktop (not headless)
#   --observer both machines observe the AIs instead of playing   --minutes game-minute cap (default 10)
#   --watch    Windows side runs WINDOWED on the laptop's desktop (not headless) and takes in-engine screenshots every
#              --shot-seconds (default 30) game seconds; they are fetched into <logdir>/shots/
#   --linux-ip / --win-ip override the auto-detected LAN addresses
# Environment: LINUX_GAME_DIR (default ~/GeneralsX/GeneralsZH), WIN_GAME_DIR (default C:\GeneralsZH)
set -euo pipefail

MAP="Tournament Desert" AI="H,H" OBS="" MINUTES=10 SEED="" LINUX_IP="" WIN_IP="" WATCH=0 LWATCH=0 SHOT_SECS=30 CAM_SECS=30 AUTOP="" AUTOP_SCHED="" CHEATS="" CHEAT_SCHED="" CAMLOCAL="" BUILD_SCHED="" FAST=""
HOST=vadapuppy
LINUX_GAME_DIR=${LINUX_GAME_DIR:-$HOME/GeneralsX/GeneralsZH}
WIN_GAME_DIR=${WIN_GAME_DIR:-C:\\GeneralsZH}
while [[ $# -gt 0 ]]; do
  case $1 in
    --map) MAP=$2; shift 2;;
    --ai) AI=$2; shift 2;;
    --observer) OBS="-lanObserver"; shift;;
    --minutes) MINUTES=$2; shift 2;;
    --seed) SEED="-lanSeed $2"; shift 2;;
    --autopilot) AUTOP_SCHED=$2; AUTOP="-lanAutopilotSchedule $2"; shift 2;;
    --cheats) CHEATS="-cheats"; shift;;
    --fast) FAST="-lanFast"; shift;;
    --build-schedule) BUILD_SCHED=$2; shift 2;;
    --camera-local) CAMLOCAL="-lanCameraLocal -lanCameraSeconds 0"; shift;;
    --cheat-schedule) CHEAT_SCHED=$2; shift 2;;
    --watch) WATCH=1; shift;;
    --linux-watch) LWATCH=1; shift;;
    --shot-seconds) SHOT_SECS=$2; shift 2;;
    --camera-seconds) CAM_SECS=$2; shift 2;;
    --linux-ip) LINUX_IP=$2; shift 2;;
    --win-ip) WIN_IP=$2; shift 2;;
    *) echo "unknown arg $1" >&2; exit 2;;
  esac
done

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$ROOT/logs/netplay/$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
[[ -z $LINUX_IP ]] && LINUX_IP=$(ip -4 -o addr show scope global | awk '{print $4}' | cut -d/ -f1 | grep '^192\.168\.' | head -1)
[[ -z $LINUX_IP ]] && { echo "cannot determine Linux LAN IP; use --linux-ip" >&2; exit 1; }
[[ -z $WIN_IP ]] && WIN_IP=$(ssh -o ConnectTimeout=8 "$HOST" '(Get-NetIPAddress -AddressFamily IPv4 -PrefixOrigin Dhcp,Manual | Where-Object { $_.IPAddress -like "192.168.*" } | Select-Object -First 1).IPAddress' | tr -d '\r')
echo ">> linux $LINUX_IP  windows $WIN_IP  map \"$MAP\"  ai $AI ${OBS:+observers}  cap ${MINUTES}min  logs $OUT"

cleanup() {
  [[ -n ${HOST_PID:-} ]] && kill "$HOST_PID" 2>/dev/null || true
  [[ $LWATCH == 1 ]] && pkill -x GeneralsXZH 2>/dev/null || true
  ssh -o ConnectTimeout=8 "$HOST" 'Stop-Process -Name GeneralsXZH -Force -ErrorAction SilentlyContinue' >/dev/null 2>&1 || true
}
trap cleanup EXIT

# --- host (Linux) ---
START_LOCAL=$(date +%s)
if [[ $LWATCH == 1 ]]; then
  # Windowed on this machine's real desktop via the project run script (Vulkan/audio workarounds included).
  "$ROOT/scripts/build/linux/run-linux-zh.sh" -win -xres 800 -yres 600 -lanhost "$MAP" -lanHumans 2 -lanSlots "$AI" $OBS $SEED \
    -lanMaxMinutes "$MINUTES" -lanName LinuxHost -lanIP "$LINUX_IP" -lanStats "$OUT/host-stats.json" \
    -lanScreenshotSeconds "$SHOT_SECS" -lanCameraSeconds "$CAM_SECS" $AUTOP $CHEATS $CAMLOCAL > "$OUT/host.log" 2>&1 &
else
  cd "$LINUX_GAME_DIR"
  LD_LIBRARY_PATH="$LINUX_GAME_DIR" ./GeneralsXZH -headless -lanhost "$MAP" -lanHumans 2 -lanSlots "$AI" $OBS $SEED \
    -lanMaxMinutes "$MINUTES" -lanName LinuxHost -lanIP "$LINUX_IP" -lanStats "$OUT/host-stats.json" $AUTOP $CHEATS ${CHEAT_SCHED:+-lanCheatSchedule $CHEAT_SCHED} ${BUILD_SCHED:+-lanBuildSchedule $BUILD_SCHED} $FAST > "$OUT/host.log" 2>&1 &
fi
HOST_PID=$!
for _ in $(seq 1 90); do grep -q "host lobby ready" "$OUT/host.log" 2>/dev/null && break; kill -0 $HOST_PID 2>/dev/null || break; sleep 1; done
grep -q "host lobby ready" "$OUT/host.log" || { echo "!! host never became ready"; tail -5 "$OUT/host.log"; exit 1; }
echo ">> host lobby ready"

# --- joiner (Windows) ---
WIN_UNIX=${WIN_GAME_DIR//\\//}
if [[ $WATCH == 1 ]]; then
  # Windowed, inside the logged-in desktop session (a scheduled task with /IT), so it really draws on the laptop's screen.
  START_TS=$(ssh "$HOST" 'Get-Date -Format o' | tr -d '\r')
  BAT=$(mktemp --suffix=.bat)
  printf '@echo off\r\ncd /d %s\r\nGeneralsXZH.exe -win -xres 800 -yres 600 -lanjoin %s -lanName WinJoin -lanIP %s -lanMaxMinutes %s -lanScreenshotSeconds %s -lanCameraSeconds %s %s %s %s -lanStats %s\\netplay-stats.json > %s\\netplay-join.log 2>&1\r\n' \
    "$WIN_GAME_DIR" "$LINUX_IP" "$WIN_IP" "$MINUTES" "$SHOT_SECS" "$CAM_SECS" "$OBS $CAMLOCAL" "$AUTOP" "$CHEATS" "$WIN_GAME_DIR" "$WIN_GAME_DIR" > "$BAT"
  scp -q "$BAT" "$HOST:$WIN_UNIX/watch-join.bat"; rm -f "$BAT"
  ssh "$HOST" "Remove-Item $WIN_GAME_DIR\\netplay-join.log -ErrorAction SilentlyContinue; schtasks /Create /TN netplaywatch /SC ONCE /ST 00:00 /RU genserver /IT /RL HIGHEST /F /TR $WIN_GAME_DIR\\watch-join.bat; schtasks /Run /TN netplaywatch" > "$OUT/win-exit.txt" 2>&1
  (
    sleep 8
    while ssh -o ConnectTimeout=8 "$HOST" 'if (Get-Process GeneralsXZH -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }' >/dev/null 2>&1; do sleep 10; done
  ) &
  WIN_SSH=$!
else
  ssh "$HOST" "cd $WIN_GAME_DIR; \$p = Start-Process .\\GeneralsXZH.exe -ArgumentList '-headless','-lanjoin','$LINUX_IP','-lanName','WinJoin','-lanIP','$WIN_IP','-lanMaxMinutes','$MINUTES','-lanStats','$WIN_GAME_DIR\\netplay-stats.json' ${OBS:+,'-lanObserver'} ${AUTOP_SCHED:+,'-lanAutopilotSchedule','$AUTOP_SCHED'} ${CHEATS:+,'-cheats'} ${CHEAT_SCHED:+,'-lanCheatSchedule','$CHEAT_SCHED'} ${BUILD_SCHED:+,'-lanBuildSchedule','$BUILD_SCHED'} ${FAST:+,'-lanFast'} -PassThru -NoNewWindow -RedirectStandardOutput netplay-join.log -RedirectStandardError netplay-join.err; \$p.WaitForExit(); \$p.WaitForExit(); 'win exit ' + \$p.ExitCode" > "$OUT/win-exit.txt" 2>&1 &
  WIN_SSH=$!
fi

wait $HOST_PID || true; HOST_RC=$?
wait $WIN_SSH || true
scp -q "$HOST:$WIN_UNIX/netplay-join.log" "$OUT/join.log" || true
if [[ $WATCH == 1 ]]; then
  mkdir -p "$OUT/shots"
  ssh "$HOST" "Get-ChildItem $WIN_GAME_DIR,\$env:USERPROFILE\\Documents -Recurse -Include *.png -ErrorAction SilentlyContinue | Where-Object { \$_.LastWriteTime -gt [datetime]'$START_TS' } | ForEach-Object { \$_.FullName }" | tr -d '\r' | while read -r f; do
    [[ -n $f ]] && scp -q "$HOST:${f//\\//}" "$OUT/shots/" 2>/dev/null || true
  done
  echo ">> screenshots: $(ls "$OUT/shots" 2>/dev/null | wc -l) in $OUT/shots"
fi
scp -q "$HOST:$WIN_UNIX/netplay-stats.json" "$OUT/join-stats.json" 2>/dev/null || true
if [[ $LWATCH == 1 ]]; then
  mkdir -p "$OUT/shots-linux"
  # The Linux engine writes these to <data dir>/Screenshots/sshot_*.png. Builds from before the separator fix wrote a
  # literal "Screenshots\sshot_*.png" in the data dir root, so keep matching that too (depth 2 covers both).
  find "$HOME/.local/share/GeneralsX/GeneralsZH" -maxdepth 2 -name '*sshot_*.png' -newermt "@$START_LOCAL" 2>/dev/null | while read -r f; do
    cp "$f" "$OUT/shots-linux/$(basename "${f//\\//}")"
  done
  echo ">> linux screenshots: $(ls "$OUT/shots-linux" 2>/dev/null | wc -l) in $OUT/shots-linux"
fi
echo ">> host exit $(grep -c . /dev/null)$(true) ; windows: $(tr -d '\r' < "$OUT/win-exit.txt" | tail -1)"
python3 "$ROOT/tools/netplay/compare.py" "$OUT" || exit $?
