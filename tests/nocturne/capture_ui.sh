#!/usr/bin/env bash
# Render the real desktop apps against a synthetic SD card in the CI runner.
set -euo pipefail
cd "$(dirname "$0")/../.."
export SDCARD_PATH=/var/tmp/nextui/sdcard
export SYSTEM_PATH="$SDCARD_PATH/.system/desktop"
export USERDATA_PATH="$SDCARD_PATH/.userdata/desktop"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LD_LIBRARY_PATH=/var/tmp/nextui/lib
export SDL_AUDIODRIVER=dummy
export LIBGL_ALWAYS_SOFTWARE=1
mkdir -p "$USERDATA_PATH" "$SHARED_USERDATA_PATH" "$SYSTEM_PATH/paks" \
  "$SDCARD_PATH/.system/res" "$SDCARD_PATH/Roms/Game Boy (GB)" \
  "$SDCARD_PATH/Emus/desktop/GB.pak" tests/nocturne/build/previews
cp -R skeleton/SYSTEM/res/. "$SDCARD_PATH/.system/res/"
printf '#!/bin/sh\nexit 1\n' > "$SDCARD_PATH/Emus/desktop/GB.pak/launch.sh"
chmod +x "$SDCARD_PATH/Emus/desktop/GB.pak/launch.sh"
touch "$SDCARD_PATH/Roms/Game Boy (GB)/A synthetic sample game.gb"
touch "$SDCARD_PATH/Roms/Game Boy (GB)/A very long synthetic game name for scrolling.gb"

ui_pid=
stop_ui() {
  if [ -n "$ui_pid" ]; then
    kill "$ui_pid" 2>/dev/null || true
    wait "$ui_pid" 2>/dev/null || true
    ui_pid=
  fi
}
trap stop_ui EXIT
for size in 0 1 2 3; do
  for app in nextui settings; do
    printf 'fontSize=%s\nfontStyle=0\nscreenTimeout=0\nshowMenuAnimations=0\n' "$size" \
      > "$SHARED_USERDATA_PATH/minuisettings.txt"
    "workspace/all/$app/build/desktop/$app.elf" \
      > "tests/nocturne/build/previews/$app-$size.log" 2>&1 &
    ui_pid=$!
    sleep 2
    kill -0 "$ui_pid"
    scrot "tests/nocturne/build/previews/$app-$size.png"
    if [ "$app" = settings ]; then
      window=$(xdotool search --onlyvisible --pid "$ui_pid" | head -n 1)
      xdotool windowfocus --sync "$window"
      xdotool key --clearmodifiers s
      sleep 1
      scrot "tests/nocturne/build/previews/appearance-$size.png"
    fi
    stop_ui
  done
done
