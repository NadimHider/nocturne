# UI and reliability review

The current changes keep the Settings app's comfortable default font size as
the primary text size throughout the UI. Font-size options remain global.
Settings and emulator options use measured row heights and separate columns;
game titles are drawn once, using the same name during static rendering,
selection animation, and scrolling. Text fitting respects UTF-8 boundaries and
destination capacity. Explicit description line breaks remain supported.

The palette catalog adds 15 everyday dark and 10 extra-dark Night options.
The existing live Left/Right palette selection and custom-color restoration
remain available. [The palette guide](PALETTES.md) describes the options.

H700 keeps screen-off first and deep sleep after the configured delay.
Failed suspend returns to the awake state; overlapping radio recovery cancels
a new suspend attempt before changing hardware. See [Sleep](SLEEP.md).

## Follow-up opportunities

These are investigation candidates, not implemented features or confirmed bugs:

| Priority | Area | Next useful investigation |
| --- | --- | --- |
| High | Device sleep | Measure repeated wake latency and overnight battery loss with radios, HDMI and different cores. |
| High | Saves | Verify recovery messaging and safe resume when a core cannot create or load a state. |
| Medium | Large libraries | Profile directory scanning, sorting and thumbnail loading with thousands of synthetic entries. |
| Medium | Library navigation | Evaluate search, favorites and faster alphabet/page navigation on the handheld controls. |
| Medium | Appearance | Consider palette favorites and a temporary preview/revert option alongside current live selection. |
| Medium | Diagnostics | Offer an on-device report of wake timings, failed restores and build version with sensitive values excluded. |

## Validation

`make test-nocturne` covers existing theme, switcher and preview-cache policies,
all shipped palette contrasts, real SDL_ttf font metrics and text fitting,
explicit/wrapped descriptions, and mocked suspend/recovery paths. The UI CI job
also compiles the frontend/Settings and captures all four font sizes using
the real desktop apps at 640x480 with a synthetic library.

Desktop captures and mocks do not verify physical display brightness, actual
suspend power consumption, audio hardware or kernel wake behavior. Those need
an RG40XX H device pass before a release.
