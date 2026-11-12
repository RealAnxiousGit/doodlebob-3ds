#!/bin/bash
# Build the Cinnamon 3DS runner (doodlebob-3ds branch) and the host asset preprocessor.
# Run from PowerShell: wsl -d Ubuntu --cd ~ -- bash -l "<work folder>/tools/build-3ds.sh"
# Low priority, 2 jobs: the laptop usually runs a 60% CPU cap.
set -e
export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
export PATH=$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH
export CMAKE_BUILD_PARALLEL_LEVEL=2
[ -d $DEVKITARM ] || { echo "devkitPro missing: bash \"/mnt/f/Fruit Slicer Dev/restore-wsl.sh\" 3ds"; exit 1; }
cd ~/doodlebob-3ds/cinnamon
LOG=~/doodlebob-3ds/build.log
nice -n 19 cmake -S . -B build/n3ds -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake \
  -DPLATFORM=n3ds -DCMAKE_BUILD_TYPE=Release > $LOG 2>&1
nice -n 19 cmake --build build/n3ds >> $LOG 2>&1 || { grep -m20 -E " error|Error " $LOG; exit 1; }
nice -n 19 cmake -S tools/n3ds-preprocess -B build/preprocess -DCMAKE_BUILD_TYPE=Release >> $LOG 2>&1
nice -n 19 cmake --build build/preprocess >> $LOG 2>&1
DIST="${DOODLEBOB_DIR:-$(cd "$(dirname "$0")/.." && pwd)}/dist"
mkdir -p "$DIST" && cp build/n3ds/cinnamon.3dsx "$DIST/"
echo "Built: build/n3ds/cinnamon.3dsx (copied to dist), build/preprocess/n3ds-preprocess"
