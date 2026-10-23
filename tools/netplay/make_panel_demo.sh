#!/usr/bin/env bash
# Autopilot control-panel demo: a visible solo game (Death Valley, USA) where Autopilot goes on at Fortress, a "capture this building" directive is sent,
# the Superweapons check box is clicked off and on, and the title is clicked to turn Autopilot off and on again. In-engine screenshots (no screen
# recording; the clicks come from -lanPanelClickSchedule through the panel's own click handler) are captioned and stitched into an mp4, plus a panel close-up.
#
#   tools/netplay/make_panel_demo.sh [outdir]          (default outdir: ./demo-out/panel)
#
# Needs a desktop session (the game opens a window; sound is muted through OpenAL's null device), ImageMagick (magick) and ffmpeg. About 5 minutes.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$ROOT/demo-out/panel}; mkdir -p "$OUT/frames" "$OUT/cap"; rm -f "$OUT"/frames/* "$OUT"/cap/*
FONT=${FONT:-$(find /usr/share/fonts -name '*.ttf' | grep -i bold | head -1)}
SHOTS=$HOME/.local/share/GeneralsX/GeneralsZH
STAMP=$OUT/.start; touch "$STAMP"

# on at 5 s (Fortress); capture directive for derrick #296 at 40 s; Superweapons off at 80 s, on at 110 s; title click: off at 140 s, Fortress at 170 s.
# Panel at 800x600 (x=8, y=58, 232 wide): the title row is at y~64, module rows start at y=84 and are 16 px apart (Superweapons is the third: y~123).
ALSOFT_DRIVERS=null "$ROOT/scripts/build/linux/run-linux-zh.sh" -win -xres 800 -yres 600 -lanhost "Death Valley" -lanHumans 1 -lanSlots E -lanSeed 7 \
  -lanHostTemplate FactionAmerica -lanMaxMinutes 4 -lanAutopilotSchedule "5:6" -lanDirectiveSchedule "40:1:296:300" \
  -lanPanelClickSchedule "80:30:123,110:30:123,140:60:68,170:60:68" \
  -lanScreenshotSeconds 4 -lanCameraSeconds 0 -lanCameraLocal > "$OUT/run.log" 2>&1

i=0
find "$SHOTS" -maxdepth 2 -name '*sshot_*.png' -newer "$STAMP" | sort | while read -r f; do   # the file name has a literal backslash
  i=$((i+1)); cp "$f" "$OUT/frames/f$(printf %03d $i).png"
done
n=$(ls "$OUT/frames" | wc -l)
first=$(grep -a -m1 "autopilot applied\|autopilot request" "$OUT/run.log" >/dev/null && echo 0)
for i in $(seq 1 "$n"); do
  t=$((4*i)); txt="The panel: Autopilot level, modules, activity feed"; col="#aaccff"
  if   [ $t -ge 172 ]; then txt="Click the title again: Autopilot is back on (Fortress)"; col="#66ff88"
  elif [ $t -ge 142 ]; then txt="Click the title: Autopilot is OFF - the panel shrinks"; col="#ffcc44"
  elif [ $t -ge 112 ]; then txt="Click Superweapons again: it is on (builds + fires)"; col="#66ff88"
  elif [ $t -ge 82  ]; then txt="Click Superweapons: unchecked, the module is off"; col="#ffcc44"
  elif [ $t -ge 42  ]; then txt="Directive sent: capture that oil derrick first (see the feed)"; col="#ffee88"
  elif [ $t -ge 8   ]; then txt="Autopilot ON (Fortress): modules are checked, the feed says what it does"; col="#66ff88"; fi
  o=$(printf "$OUT/cap/c%03d.png" $i)
  magick "$OUT/frames/$(printf f%03d $i).png" -resize 1200x900 -fill "rgba(0,0,0,0.6)" -draw "rectangle 0,0 1200,64" \
    -font "$FONT" -pointsize 30 -fill "$col" -annotate +22+43 "$txt" "$o"
done
ffmpeg -hide_banner -loglevel error -y -framerate 4 -i "$OUT/cap/c%03d.png" -c:v libx264 -pix_fmt yuv420p -crf 20 "$OUT/autopilot-panel-demo.mp4"
# close-ups of the panel at three moments (crop the 800x600 frame, enlarge 3x)
for pair in "11:on" "23:superweapons-off" "37:autopilot-off"; do
  k=${pair%%:*}; name=${pair##*:}
  [ "$k" -le "$n" ] && magick "$OUT/frames/$(printf f%03d "$k").png" -crop 252x232+2+54 +repage -filter point -resize 300% "$OUT/panel-$name.png"
done
echo "wrote $OUT/autopilot-panel-demo.mp4 and $OUT/panel-*.png ($n frames)"
