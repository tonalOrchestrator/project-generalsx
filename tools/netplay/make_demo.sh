#!/usr/bin/env bash
# Autopilot demo video: a visible solo game where Autopilot is ON, then OFF while the "player" orders a Barracks by hand, then ON
# again, captured as in-engine screenshots (no screen recording, nothing touches the mouse) and stitched into a captioned mp4.
#
#   tools/netplay/make_demo.sh [outdir]          (default outdir: ./demo-out)
#
# Needs: the game running from LINUX_GAME_DIR (default ~/GeneralsX/GeneralsZH), a desktop session (the game opens a window),
# ImageMagick (magick) and ffmpeg. Takes about 5 minutes (real time). Caption times follow the schedule below.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$ROOT/demo-out}; mkdir -p "$OUT/frames" "$OUT/cap"
FONT=${FONT:-$(find /usr/share/fonts -name '*.ttf' | grep -i bold | head -1)}
SHOTS=$HOME/.local/share/GeneralsX/GeneralsZH
STAMP=$OUT/.start; touch "$STAMP"

# on at 10 s, off at 100 s, a hand-ordered barracks at 110 s, on again at 190 s; a screenshot every 4 s, camera on the human base
"$ROOT/scripts/build/linux/run-linux-zh.sh" -win -xres 800 -yres 600 -lanhost "Dark Night" -lanHumans 1 -lanSlots E -lanSeed 7 \
  -lanHostTemplate FactionGLA -lanMaxMinutes 5 -lanAutopilotSchedule "10:2,100:0,190:2" -lanBuildSchedule "110:barracks" \
  -lanScreenshotSeconds 4 -lanCameraSeconds 0 -lanCameraLocal > "$OUT/run.log" 2>&1

i=0
find "$SHOTS" -maxdepth 2 -name '*sshot_*.png' -newer "$STAMP" | sort | while read -r f; do   # the file name has a literal backslash
  i=$((i+1)); cp "$f" "$OUT/frames/f$(printf %03d $i).png"
done
n=$(ls "$OUT/frames" | wc -l)
for i in $(seq 1 "$n"); do
  t=$((4*i)); txt=""; col="#66ff88"
  if   [ $t -ge 190 ]; then txt="AUTOPILOT ON (Normal) - it carries on from there"
  elif [ $t -ge 100 ]; then txt="AUTOPILOT OFF - you are in control"; col="#ffcc44"
  elif [ $t -ge 10  ]; then txt="AUTOPILOT ON (Normal) - the AI builds the base"; fi
  o=$(printf "$OUT/cap/c%03d.png" $i)
  magick "$OUT/frames/$(printf f%03d $i).png" -resize 1200x900 -fill "rgba(0,0,0,0.6)" -draw "rectangle 0,0 1200,64" \
    -font "$FONT" -pointsize 30 -fill "$col" -annotate +22+43 "$txt" "$o"
  [ $t -ge 110 ] && [ $t -lt 150 ] && magick "$o" -fill "rgba(0,0,0,0.7)" -draw "rectangle 300,812 900,872" -font "$FONT" \
    -pointsize 30 -fill white -annotate +322+852 "You order a Barracks by hand" "$o"
done
ffmpeg -hide_banner -loglevel error -y -framerate 5 -i "$OUT/cap/c%03d.png" -c:v libx264 -pix_fmt yuv420p -crf 20 "$OUT/autopilot-demo.mp4"
echo "wrote $OUT/autopilot-demo.mp4"
