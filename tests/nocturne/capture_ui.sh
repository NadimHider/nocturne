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
    # This is a disposable fixture process. Avoid its device power-off path.
    kill -KILL "$ui_pid" 2>/dev/null || true
    wait "$ui_pid" 2>/dev/null || true
    ui_pid=
  fi
}
trap stop_ui EXIT
capture_ready_frame() {
  local destination=$1
  # Mesa may take longer to paint its first frame on a cold CI runner.
  # Wait for the actual selection pill instead of accepting a black capture.
  for attempt in $(seq 1 40); do
    kill -0 "$ui_pid"
    scrot -o "$destination"
    if /usr/bin/python3 -c 'from PIL import Image; import sys; image=Image.open(sys.argv[1]).convert("RGB"); sys.exit(0 if image.crop((10,20,300,80)).getextrema()[1][1] > 80 else 1)' "$destination"; then
      return
    fi
    sleep 0.25
  done
  echo "UI did not paint a ready frame: $destination" >&2
  return 1
}
for size in 0 1 2 3; do
  for app in nextui settings; do
    printf 'fontSize=%s\nfontStyle=0\nscreenTimeout=0\nshowMenuAnimations=0\n' "$size" \
      > "$SHARED_USERDATA_PATH/minuisettings.txt"
    "workspace/all/$app/build/desktop/$app.elf" \
      > "tests/nocturne/build/previews/$app-$size.log" 2>&1 &
    ui_pid=$!
    capture_ready_frame "tests/nocturne/build/previews/$app-$size.png"
    window=$(xdotool search --onlyvisible --pid "$ui_pid" | head -n 1)
    xdotool windowfocus --sync "$window"
    xdotool key --clearmodifiers s
    sleep 1
    if [ "$app" = settings ]; then
      scrot "tests/nocturne/build/previews/appearance-$size.png"
    else
      scrot "tests/nocturne/build/previews/game-list-$size.png"
    fi
    stop_ui
  done
done
