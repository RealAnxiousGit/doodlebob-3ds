#!/bin/bash
# Builds cinnamon-preprocessor.exe (Windows, static) from cinnamon-runner's preprocessor and copies it,
# with its game_profiles.cfg, to converter/bin/.
# Run from PowerShell: wsl -d Ubuntu --cd ~ -- bash -l "<work folder>/converter/build/build-preprocess-win.sh"
# PREPROCESSOR_DIR: the cinnamon-runner preprocessor source (default ~/dbv4/preprocessor).
set -e
P="${DOODLEBOB_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}"  # the DoodleBob 3DS work folder
SRC="${PREPROCESSOR_DIR:-$HOME/dbv4/preprocessor}"
B=~/dbpp-win4
mkdir -p $B
cd $B
cmake -S "$SRC" -B . -DCMAKE_TOOLCHAIN_FILE="$P/converter/build/mingw.cmake" \
  -DCMAKE_BUILD_TYPE=Release > cmake.log
nice -n 10 make -j2 cinnamon-preprocessor 2>&1 | grep -E 'error|undefined reference' || true
mkdir -p "$P/converter/bin"
rm -f "$P/converter/bin/n3ds-preprocess.exe"
cp cinnamon-preprocessor.exe "$P/converter/bin/"
cp "$SRC/game_profiles.cfg" "$P/converter/bin/"
mkdir -p "$P/converter/bin/licenses"
cp "$SRC/vendor/cinnamon/LICENSE" "$P/converter/bin/licenses/cinnamon-preprocessor-LICENSE.txt"  # AGPL-3.0
x86_64-w64-mingw32-objdump -p cinnamon-preprocessor.exe | grep 'DLL Name'
ls -la "$P/converter/bin/cinnamon-preprocessor.exe"
