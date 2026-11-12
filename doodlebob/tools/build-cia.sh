#!/bin/bash
# Packs the built runner (build/n3ds/cinnamon.elf) into dist/DoodleBob.cia with the HOME Menu icon, banner and banner sound.
# Needs cia/banner.png and cia/banner.wav first (tools/export-banner-art.csx, then tools/make_banner.py), and a runner
# build (tools/build-3ds.sh). The CIA holds only the runner: the game data still goes in sdmc:/3ds/DoodleBob.
# Uses makerom and bannertool from Fruit Slicer's ~/fs3ds-tools (read-only).
# Run from PowerShell: wsl -d Ubuntu --cd ~ -- bash -l "<work folder>/tools/build-cia.sh"
set -e
P="${DOODLEBOB_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"  # the DoodleBob 3DS work folder
T=~/fs3ds-tools
MAKEROM=$T/mr/makerom
BANNERTOOL=$T/bt/bannertool-1.2.3-linux/bannertool
for f in "$MAKEROM" "$BANNERTOOL" "$P/cia/banner.png" "$P/cia/banner.wav" ~/doodlebob-3ds/cinnamon/build/n3ds/cinnamon.elf; do
  [ -e "$f" ] || { echo "Missing $f"; exit 1; }
done
W=~/doodlebob-3ds/cia-work
mkdir -p $W
"$BANNERTOOL" makebanner -i "$P/cia/banner.png" -a "$P/cia/banner.wav" -o $W/banner.bnr
"$BANNERTOOL" makesmdh -s "DoodleBob 3DS" -l "DoodleBob and the Magic Pencil DX (personal port)" -p "Ethan Wake fangame" \
  -i ~/doodlebob-3ds/cinnamon/resources/3ds/meta/icon.png -o $W/icon.icn
"$MAKEROM" -f cia -o $W/DoodleBob.cia -target t -exefslogo -elf ~/doodlebob-3ds/cinnamon/build/n3ds/cinnamon.elf \
  -rsf "$P/tools/doodlebob.rsf" -icon $W/icon.icn -banner $W/banner.bnr -major 0 -minor 3 -micro 0
cp $W/DoodleBob.cia "$P/dist/DoodleBob.cia"
ls -la "$P/dist/DoodleBob.cia"
