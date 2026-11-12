#!/bin/bash
# Builds n3ds-preprocess.exe (Windows, static) from the runner source and copies it to converter/bin/.
# Run from PowerShell: wsl -d Ubuntu --cd ~ -- bash -l "<work folder>/converter/build/build-preprocess-win.sh"
set -e
P="${DOODLEBOB_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}"  # the DoodleBob 3DS work folder
B=~/dbpp-win
mkdir -p $B
cd $B
cmake -S ~/doodlebob-3ds/cinnamon/tools/n3ds-preprocess -B . -DCMAKE_TOOLCHAIN_FILE="$P/converter/build/mingw.cmake" \
  -DCMAKE_BUILD_TYPE=Release > cmake.log
nice -n 10 make -j2 2>&1 | grep -E 'error|undefined reference' || true
mkdir -p "$P/converter/bin"
cp n3ds-preprocess.exe "$P/converter/bin/"
x86_64-w64-mingw32-objdump -p n3ds-preprocess.exe | grep 'DLL Name'
ls -la "$P/converter/bin/n3ds-preprocess.exe"
