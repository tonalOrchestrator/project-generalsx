#!/usr/bin/env bash
# Copy GeneralsXZH.exe from a remote-build artifacts.zip (default: the newest under build-logs/) to the Windows box.
set -euo pipefail
cd "$(dirname "$0")/../.."
HOST=vadapuppy
ZIP=${1:-$(ls -t build-logs/*/artifacts.zip | head -1)}
TMP=$(mktemp -d)
unzip -q -o "$ZIP" GeneralsXZH.exe -d "$TMP"
ssh "$HOST" 'Stop-Process -Name GeneralsXZH -Force -ErrorAction SilentlyContinue; Start-Sleep 1'
scp -q "$TMP/GeneralsXZH.exe" "$HOST:C:/GeneralsZH/GeneralsXZH.exe"
rm -rf "$TMP"
echo "deployed $(basename "$(dirname "$ZIP")") to $HOST:C:/GeneralsZH"
