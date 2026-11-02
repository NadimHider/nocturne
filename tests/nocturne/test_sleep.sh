#!/usr/bin/env bash
# Exercise the H700 helper with fake sysfs and services, never real hardware.
set -euo pipefail
cd "$(dirname "$0")/../.."
repo=$PWD
real_sleep=$(command -v sleep)
export MOCK_REAL_SLEEP="$real_sleep"
mkdir -p tests/nocturne/build
scratch=$(mktemp -d "$PWD/tests/nocturne/build/sleep.XXXXXX")
case "$scratch" in "$PWD/tests/nocturne/build/"*) ;; *) exit 1 ;; esac
trap 'rm -rf "$scratch"' EXIT
mkdir -p "$scratch/bin" "$scratch/system/bin" "$scratch/system/etc/wifi" "$scratch/system/etc/bluetooth"
cp skeleton/SYSTEM/h700/bin/run_hooks.sh "$scratch/system/bin/run_hooks.sh"
chmod +x "$scratch/system/bin/run_hooks.sh"
cat > "$scratch/bin/alsactl" <<'EOF'
#!/bin/sh
echo "mixer:$3" >> "$MOCK_LOG"
if [ "$3" = store ]; then
    [ "${MOCK_STORE_FAIL:-0}" = 0 ] || exit 1
    echo fresh > "$2"
fi
EOF
cat > "$scratch/bin/pgrep" <<'EOF'
#!/bin/sh
[ "${MOCK_RADIOS:-0}" = 1 ]
EOF
cat > "$scratch/bin/sleep" <<'EOF'
#!/bin/sh
# Retry/wait delays are policy, not wall time, in these mock tests.
[ "${MOCK_REAL_WAIT:-0}" != 1 ] || exec "$MOCK_REAL_SLEEP" "$@"
exit 0
EOF
cat > "$scratch/system/bin/syncsettings.elf" <<'EOF'
#!/bin/sh
echo display:restore >> "$MOCK_LOG"
EOF
for radio in wifi bluetooth; do
    radio_script=wifi_init.sh
    [ "$radio" != bluetooth ] || radio_script=bt_init.sh
    cat > "$scratch/system/etc/$radio/$radio_script" <<'EOF'
#!/bin/sh
echo "radio:$1" >> "$MOCK_LOG"
if [ "$1" = start ]; then
    "$MOCK_REAL_SLEEP" 0.3
    echo radio:ready >> "$MOCK_LOG"
fi
EOF
done
chmod +x "$scratch/bin/"* "$scratch/system/bin/"* "$scratch/system/etc/"*/*
export PATH="$scratch/bin:$PATH"
export SYSTEM_PATH="$scratch/system" USERDATA_PATH="$scratch/userdata"
export MOCK_LOG="$scratch/actions.log"
export NOCTURNE_POWER_RUNTIME_DIR="$scratch/runtime"
export NOCTURNE_POWER_SYSFS_ROOT="$scratch/sys"
mkdir -p "$NOCTURNE_POWER_RUNTIME_DIR" "$scratch/sys/power" "$scratch/sys/class/power_supply/axp2202-battery"
led="$scratch/sys/class/power_supply/axp2202-battery/work_led"
mcu="$scratch/sys/class/power_supply/axp2202-battery/mcu_pwr"
echo 7 > "$led"
echo 7 > "$mcu"
echo 1 > "$scratch/sys/class/power_supply/axp2202-battery/hallkey"
: > "$scratch/sys/class/power_supply/axp2202-battery/os_sleep"
: > "$scratch/sys/power/state"
: > "$MOCK_LOG"

# A previous restore still running must abort before changing any hardware.
echo $$ > "$NOCTURNE_POWER_RUNTIME_DIR/resume.pid"
if sh skeleton/SYSTEM/h700/bin/suspend > "$scratch/busy.log" 2>&1; then
    echo 'Expected busy restore to cancel suspend' >&2; exit 1
fi
[ "$(cat "$led")" = 7 ]
[ ! -s "$MOCK_LOG" ]
grep -q 'Previous radio restore is still busy' "$scratch/busy.log"
echo invalid > "$NOCTURNE_POWER_RUNTIME_DIR/resume.pid"

# Successful cycle restores mixer before the caller reopens SDL audio.
sh skeleton/SYSTEM/h700/bin/suspend > "$scratch/success.log" 2>&1
[ "$(cat "$scratch/sys/power/state")" = mem ]
[ "$(cat "$led")" = 1 ]
[ "$(cat "$mcu")" = 1 ]
[ "$(sed -n '1p' "$MOCK_LOG")" = mixer:store ]
[ "$(sed -n '2p' "$MOCK_LOG")" = mixer:restore ]
[ "$(sed -n '3p' "$MOCK_LOG")" = display:restore ]

# Failed store cannot restore a stale mixer file from an earlier cycle.
echo invalid > "$NOCTURNE_POWER_RUNTIME_DIR/resume.pid"
echo old > "$NOCTURNE_POWER_RUNTIME_DIR/asound.state.pre"
: > "$MOCK_LOG"
MOCK_STORE_FAIL=1 sh skeleton/SYSTEM/h700/bin/suspend > "$scratch/store-failure.log" 2>&1
[ ! -e "$NOCTURNE_POWER_RUNTIME_DIR/asound.state.pre" ]
if grep -q mixer:restore "$MOCK_LOG"; then
    echo 'Restored stale mixer snapshot' >&2; exit 1
fi

# Exhausted kernel failures still restore display state, without a sixth delay.
echo invalid > "$NOCTURNE_POWER_RUNTIME_DIR/resume.pid"
rm "$scratch/sys/power/state"
mkdir "$scratch/sys/power/state"
echo 0 > "$led"
if sh skeleton/SYSTEM/h700/bin/suspend > "$scratch/failure.log" 2>&1; then
    echo 'Expected kernel failure to propagate' >&2; exit 1
fi
[ "$(cat "$led")" = 1 ]
[ "$(grep -c 'Deep sleep attempt' "$scratch/failure.log")" = 5 ]
[ "$(grep -c 'retrying in 3 seconds' "$scratch/failure.log")" = 4 ]

# Rapid repeated cycles finish old radio startup before beginning teardown.
rmdir "$scratch/sys/power/state"
: > "$scratch/sys/power/state"
: > "$MOCK_LOG"
echo invalid > "$NOCTURNE_POWER_RUNTIME_DIR/resume.pid"
export MOCK_RADIOS=1 MOCK_REAL_WAIT=1
sh skeleton/SYSTEM/h700/bin/suspend > "$scratch/radios-first.log" 2>&1
sh skeleton/SYSTEM/h700/bin/suspend > "$scratch/radios-second.log" 2>&1
awk '/radio:ready/ { ready++ } /mixer:store/ { cycles++; if (cycles == 2 && ready < 2) exit 1 }' "$MOCK_LOG"
resume_pid=$(cat "$NOCTURNE_POWER_RUNTIME_DIR/resume.pid")
for i in {1..100}; do
    kill -0 "$resume_pid" 2>/dev/null || break
    "$real_sleep" 0.1
done
[ "$(grep -c radio:ready "$MOCK_LOG")" = 4 ]
unset MOCK_RADIOS MOCK_REAL_WAIT

# Per-hook timeouts let the next hook run even when a script hangs.
mkdir -p "$USERDATA_PATH/.hooks/pre-sleep.d"
cat > "$USERDATA_PATH/.hooks/pre-sleep.d/01-hang.sh" <<'EOF'
#!/bin/sh
while :; do :; done
EOF
cat > "$USERDATA_PATH/.hooks/pre-sleep.d/02-after.sh" <<'EOF'
#!/bin/sh
echo hook:after >> "$MOCK_LOG"
EOF
chmod +x "$USERDATA_PATH/.hooks/pre-sleep.d/"*
sh skeleton/SYSTEM/h700/bin/run_hooks.sh pre-sleep.d --sync-only --timeout 1 > "$scratch/hooks.log" 2>&1
grep -q hook:after "$MOCK_LOG"
grep -q 'Hook failed or timed out' "$scratch/hooks.log"
if sh skeleton/SYSTEM/h700/bin/run_hooks.sh pre-sleep.d --timeout invalid; then
    echo 'Expected invalid timeout to fail' >&2; exit 1
fi
echo 'H700 sleep helper tests passed'
