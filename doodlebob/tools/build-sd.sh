#!/bin/bash
# Builds the runner, then the 3DS asset folder (sdmc:/3ds/DoodleBob) from game-data/3ds/data.win.
# Make game-data/3ds/data.win first with tools/patch-data.csx (see its header) whenever sprite-replacements/ changes.
# Run from PowerShell: wsl -d Ubuntu --cd ~ -- bash -l "<work folder>/tools/build-sd.sh"
# Output: ~/doodlebob-3ds/sd/3ds/DoodleBob (existing saves and the debuglog flag there are kept).
set -e
P="${DOODLEBOB_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"  # the DoodleBob 3DS work folder
DW="$P/game-data/3ds/data.win"
[ -f "$DW" ] || { echo "Missing $DW: run tools/patch-data.csx first"; exit 1; }
export DEVKITPRO=/opt/devkitpro PATH=/opt/devkitpro/tools/bin:$PATH
df -h / | tail -1
bash "$P/tools/build-3ds.sh"
cd ~/doodlebob-3ds/cinnamon
OUT=~/doodlebob-3ds/sd/3ds/DoodleBob
mkdir -p $OUT
nice -n 19 ./build/preprocess/n3ds-preprocess "$DW" $OUT --tex3ds $(which tex3ds) > ~/doodlebob-3ds/prep.log 2>&1
grep -E 'Wrote 3DS|Failed' ~/doodlebob-3ds/prep.log | head
cp "$DW" $OUT/data.win
du -sh $OUT
