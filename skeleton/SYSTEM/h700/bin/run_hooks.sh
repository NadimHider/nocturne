#!/bin/sh
# run_hooks.sh - shared hook runner for NextUI
# Usage: run_hooks.sh <dir-name> [--sync-only] [--timeout seconds]
#
# dir-name:   directory name under $USERDATA_PATH/.hooks/ (e.g. pre-launch.d, boot.d)
# --sync-only: force all scripts to run synchronously
#
# By default, scripts run in the background. Scripts ending in .sync.sh
# always run synchronously. All background scripts are waited on before exit.

DIR_NAME="${1:-}"
SYNC_ONLY=
HOOK_TIMEOUT=
[ -n "$DIR_NAME" ] || exit 0
shift
while [ "$#" -gt 0 ]; do
	case "$1" in
		--sync-only) SYNC_ONLY=1 ;;
		--timeout)
			shift
			HOOK_TIMEOUT="${1:-}"
			case "$HOOK_TIMEOUT" in ''|*[!0-9]*|0) exit 2 ;; esac
			;;
		*) exit 2 ;;
	esac
	shift
done

# Sleep hooks must never keep the panel off indefinitely. On firmware without
# timeout, skip only explicitly bounded hooks rather than run them unbounded.
if [ -n "$HOOK_TIMEOUT" ] && ! command -v timeout >/dev/null 2>&1; then
	echo "Skipping $DIR_NAME: bounded hook execution needs timeout" >&2
	exit 0
fi

run_hook() {
	if [ -n "$HOOK_TIMEOUT" ]; then
		timeout -s KILL "$HOOK_TIMEOUT" "$1"
	else
		"$1"
	fi
}

: "${SDCARD_PATH:=/mnt/SDCARD}"
: "${PLATFORM:=h700}"
: "${USERDATA_PATH:=$SDCARD_PATH/.userdata/$PLATFORM}"

HOOK_DIR="$USERDATA_PATH/.hooks/$DIR_NAME"
[ -d "$HOOK_DIR" ] || exit 0

case "$DIR_NAME" in
	pre-*)  export HOOK_PHASE="pre" ;;
	post-*) export HOOK_PHASE="post" ;;
	boot*)  export HOOK_PHASE="boot" ;;
esac
export HOOK_CATEGORY="$DIR_NAME"

for script in "$HOOK_DIR"/*.sh; do
	[ -f "$script" ] || continue
	if [ -n "$SYNC_ONLY" ] || echo "$script" | grep -q '\.sync\.sh$'; then
		run_hook "$script" > /dev/null 2>&1 || echo "Hook failed or timed out: $script" >&2
	else
		run_hook "$script" > /dev/null 2>&1 &
	fi
done
wait
