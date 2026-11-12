#!/bin/bash
# Run Azahar inside Ubuntu for automated DoodleBob tests.
# Uses Fruit Slicer's extracted Azahar read-only, with DoodleBob's own config/save dirs.
# Usage: run-azahar-linux.sh <file.3dsx|file.cia> [--headless]
set -e
AZ=~/fs3ds-tools/squashfs-root/AppRun
if [ ! -x "$AZ" ]; then
  echo "Azahar is missing from ~/fs3ds-tools. Restore it from F: (F: must be connected):"
  echo "  bash \"/mnt/f/Fruit Slicer Dev/restore-wsl.sh\" 3ds"
  exit 1
fi
export XDG_DATA_HOME=~/doodlebob-3ds/azahar-home/data
export XDG_CONFIG_HOME=~/doodlebob-3ds/azahar-home/config
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME"
GAME="$1"; shift || true
if [ "$1" = "--headless" ]; then
  # Keep the window off the user's desktop (see Fruit Slicer notes).
  unset WAYLAND_DISPLAY
  export QT_QPA_PLATFORM=xcb
  exec xvfb-run -a -s "-screen 0 1280x800x24" "$AZ" "$GAME"
fi
exec "$AZ" "$GAME"
