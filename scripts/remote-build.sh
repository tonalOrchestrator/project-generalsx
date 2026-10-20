#!/usr/bin/env bash
# Build project-generalsx on the Windows box (ssh host "vadapuppy") and fetch log + artifacts.
# Usage: scripts/remote-build.sh [-p preset] [-c config] [-t target] [-D VAR=VAL ...] [--no-wip]
#   defaults: preset=win32, config=Release, target=(all)
# Uncommitted tracked changes are included via `git stash create` unless --no-wip.
set -euo pipefail

HOST=vadapuppy
PRESET=win32 CONFIG=Release TARGET="" WIP=1 EXTRA=""
while [[ $# -gt 0 ]]; do
  case $1 in
    -p) PRESET=$2; shift 2;;
    -c) CONFIG=$2; shift 2;;
    -t) TARGET=$2; shift 2;;
    -D) EXTRA="$EXTRA -D$2"; shift 2;;
    --no-wip) WIP=0; shift;;
    *) echo "unknown arg $1" >&2; exit 2;;
  esac
done

cd "$(git rev-parse --show-toplevel)"
SHA=$(git rev-parse HEAD)
if [[ $WIP == 1 ]]; then
  # Snapshot HEAD plus all working-tree changes to the source dirs, including NEW untracked files
  # (`git stash create` skips those). Uses a throwaway index so the real index is untouched.
  TMPIDX=$(mktemp -u)
  cp "$(git rev-parse --git-path index)" "$TMPIDX"
  GIT_INDEX_FILE=$TMPIDX git add -A -- Core Generals GeneralsMD Dependencies cmake scripts tools CMakeLists.txt CMakePresets.json 2>/dev/null || true
  WIPTREE=$(GIT_INDEX_FILE=$TMPIDX git write-tree)
  rm -f "$TMPIDX"
  if [[ $WIPTREE != $(git rev-parse HEAD^{tree}) ]]; then
    SHA=$(git commit-tree "$WIPTREE" -p HEAD -m "remote-build wip snapshot")
  fi
fi
SHORT=${SHA:0:10}
OUT=build-logs/$(date +%Y%m%d-%H%M%S)-$PRESET-$SHORT
mkdir -p "$OUT"

REMOTE_ROOT='C:/build'
echo ">> syncing $SHORT to $HOST" | tee "$OUT/build.log"
ssh "$HOST" "if (-not (Test-Path $REMOTE_ROOT/generalsx.git)) { New-Item -ItemType Directory -Force $REMOTE_ROOT | Out-Null; git init --bare $REMOTE_ROOT/generalsx.git | Out-Null }"
git push -q --force "$HOST:$REMOTE_ROOT/generalsx.git" "$SHA:refs/builds/$SHORT"

# Build script runs on the laptop (Windows PowerShell 5.1). It is copied over and run with -File so
# native-command stderr (git progress, cmake warnings) is not treated as a terminating error and the
# exit code survives ssh.
PS1=$(mktemp --suffix=.ps1)
cat > "$PS1" <<'PS'
$ErrorActionPreference = 'Continue'
$root = '@ROOT@'; $src = "$root/generalsx"; $sha = '@SHA@'
function Check($what) { if ($LASTEXITCODE) { Write-Host "FAILED: $what (exit $LASTEXITCODE)"; exit $LASTEXITCODE } }
if (-not (Test-Path "$src/.git")) { git clone -q "$root/generalsx.git" $src 2>&1 | Out-Host }
Set-Location $src
git fetch -q "$root/generalsx.git" "refs/builds/${sha}:refs/builds/${sha}" 2>&1 | Out-Host; Check 'git fetch'
git checkout -q -f --detach $sha 2>&1 | Out-Host; Check 'git checkout'

# Load the x86 MSVC environment (presets use Ninja with external arch).
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (-not (Test-Path $vswhere)) { Write-Host 'Visual Studio Build Tools not installed on the laptop'; exit 1 }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
cmd /c "`"$vs/VC/Auxiliary/Build/vcvarsall.bat`" x86 >nul && set" | ForEach-Object {
  if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
cmake --preset @PRESET@@EXTRA@ 2>&1 | Out-Host; Check 'cmake configure'
$a = @('--build', 'build/@PRESET@', '--config', '@CONFIG@')
if ('@TARGET@') { $a += @('--target', '@TARGET@') }
cmake @a 2>&1 | Out-Host; Check 'cmake build'
exit 0
PS
sed -i -e "s|@ROOT@|$REMOTE_ROOT|g" -e "s|@SHA@|$SHORT|g" -e "s|@PRESET@|$PRESET|g" \
  -e "s|@EXTRA@|$EXTRA|g" -e "s|@CONFIG@|$CONFIG|g" -e "s|@TARGET@|$TARGET|g" "$PS1"
scp -q "$PS1" "$HOST:$REMOTE_ROOT/build.ps1"
rm -f "$PS1"
ssh "$HOST" "powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $REMOTE_ROOT/build.ps1" 2>&1 | tee -a "$OUT/build.log"
RC=${PIPESTATUS[0]}
echo ">> build exit code: $RC" | tee -a "$OUT/build.log"

if [[ $RC == 0 ]]; then
  ssh "$HOST" "\$z='$REMOTE_ROOT/out/$SHORT.zip'; New-Item -ItemType Directory -Force $REMOTE_ROOT/out | Out-Null; Remove-Item \$z -ErrorAction Ignore; \$f = Get-ChildItem $REMOTE_ROOT/generalsx/build/$PRESET -Recurse -Include *.exe,*.dll,*.pdb | Where-Object { \$_.FullName -notmatch 'CMakeFiles' }; Compress-Archive -Path \$f.FullName -DestinationPath \$z"
  scp -q "$HOST:$REMOTE_ROOT/out/$SHORT.zip" "$OUT/artifacts.zip"
  echo ">> artifacts: $OUT/artifacts.zip"
fi
echo ">> log: $OUT/build.log"
exit "$RC"
