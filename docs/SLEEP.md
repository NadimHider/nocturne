# H700 sleep and wake

Nocturne keeps the existing two-stage behavior: the screen turns off first;
deep sleep begins after the configured suspend timeout. These changes do not
alter the timeout values or charging/USB policy.

If deep sleep fails, the common power handler restores the awake state instead
of escalating to power-off. Disabling the suspend timeout still yields CPU time
while the screen is off. Charging retries use a fresh elapsed-time origin to
avoid unsigned timer wraparound.

## Recovery improvements

- The H700 suspend helper waits for the preceding asynchronous radio restore
  before it detects and stops radios. This prevents a rapid second suspend
  from overlapping the first cycle's Wi-Fi or Bluetooth startup. Waiting is
  capped at 60 seconds; a busy worker cancels the new deep-sleep attempt before
  the helper changes hardware state. It never kills a PID read from a file.
- Display/LED, mixer, and radio restoration runs on normal exit, suspend
  failure, and catchable termination signals. Audio mixer restoration remains
  synchronous, before the frontend reopens its audio device; radio startup
  remains asynchronous so it does not delay input handling after wake.
- A failed mixer snapshot cannot cause an older cycle's snapshot to be restored.
- Suspend duration uses Linux uptime, which includes suspended time and does
  not jump when NTP adjusts the clock.
- There is no unnecessary retry delay after the final failed kernel attempt.

Sleep and resume hooks have a five-second limit per script. This uses the
firmware's optional `timeout` command with a kill signal so a hook cannot ignore
termination indefinitely. If `timeout` is unavailable, explicitly bounded hooks
are skipped with a diagnostic. Other hook categories keep their existing
execution behavior. Hook authors should avoid spawning detached background
work that outlives the hook.

The helper stores transient state in `/tmp/nocturne-power`. The
`NOCTURNE_POWER_RUNTIME_DIR` and `NOCTURNE_POWER_SYSFS_ROOT` environment overrides
are provided for host tests; installed systems use `/tmp/nocturne-power` and
`/sys` respectively.

## Validation

Run `bash tests/nocturne/test_sleep.sh` on a host with Bash and `timeout`. It
uses synthetic sysfs files and mock services, covering a successful cycle,
busy-worker cancellation before side effects, stale mixer prevention,
exhausted kernel retries, rapid successive radio restores, and a hung hook.
It does not access physical power controls.

Hardware validation is still required for repeated frontend and in-game
sleep/wake cycles, HDMI, audio, Wi-Fi/Bluetooth reconnection, brightness/volume,
LED restoration, failed suspend recovery, and overnight battery drain.
