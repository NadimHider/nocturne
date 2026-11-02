# Configuration

## Shared typography and themes

Comfortable preserves the Settings app's default 10-point body size (20 physical
pixels at the H700 UI scale). The library, Settings labels and values, and
emulator options share the same body size: Compact 9, Comfortable 10, Large 12,
Extra Large 14. Hints and descriptions use a smaller proportional role. Settings
and emulator rows grow to fit font metrics, and narrow columns keep labels and
values separate. Font family and normal/bold choices remain available.

See [Palettes](PALETTES.md) for all 35 dark themes and [Sleep](SLEEP.md) for
delayed deep sleep and recovery behavior.

Nocturne stores frontend settings in `.userdata/shared/minuisettings.txt`. The Settings app writes this file. Manual edits are supported, but the frontend should be closed while the file is changed.

## Settings app

| Section | Options |
| --- | --- |
| Appearance | Font, font style, font size, color palette, semantic colors, game art radius and width, recents, collections, game art, tools, folder names, clock, battery percentage, animations, transitions, quick switcher interface, game switcher curtain, input prompt style, game switcher scaling |
| Display | Brightness, color temperature, contrast, saturation, exposure, white point correction, red, green, and blue gain |
| System | Volume, screen timeout, suspend timeout, haptics, default view, 24-hour time, clock, NTP, time zone, save format, save state format, extracted filenames, keep awake over USB |
| In-Game | Save, load, screenshot, volume, and display adjustment notifications, notification duration, RetroAchievements |
| Network | Wi-Fi, diagnostics |
| Bluetooth | Pairing, diagnostics, sample rate limit |
| About | Build version, platform, stock OS version, BusyBox version |

## Settings file

Path:

```text
.userdata/shared/minuisettings.txt
```

Common keys:

| Key | Values | Default |
| --- | --- | --- |
| `palette` | Palette name | `Nocturne` |
| `defaultView` | `0` game list, `1` Game Switcher, `2` Quick Menu | `0` |
| `font` | Font filename in `.system/res` | `DepartureMono-Regular.otf` |
| `fontStyle` | `0` normal, `1` bold | `0` |
| `fontSize` | `0` Compact, `1` Comfortable, `2` Large, `3` Extra Large | `1` |
| `menuanim` | `0` off, `1` on | `1` |
| `menutransitions` | `0` off, `1` snappy, `2` comfortable | `1` |
| `recents` | `0` hidden, `1` shown | `1` |
| `collections` | `0` hidden, `1` shown | `1` |
| `gameart` | `0` hidden, `1` shown | `1` |
| `tools` | `0` hidden, `1` shown | `1` |
| `showclock` | `0` hidden, `1` shown | `0` |
| `clock24h` | `0` 12-hour, `1` 24-hour | `1` |
| `batteryperc` | `0` hidden, `1` shown | `0` |
| `radius` | Thumbnail radius, unscaled | `20` |
| `wifi` | `0` off, `1` on | `0` |
| `bluetooth` | `0` off, `1` on | `0` |
| `screentimeout` | Seconds before the screen turns off | `60` |
| `suspendTimeout` | Seconds before suspend after screen off | `30` |
| `keepAwakeWhenUSB` | `0` off, `1` on | `0` |
| `haptics` | `0` off, `1` on | `0` |

The file also stores the seven semantic color values (`color1` through `color7`) for the active palette.

## Startup view

`defaultView` controls the first screen after boot:

| Value | View |
| --- | --- |
| `0` | Game-system list |
| `1` | Game Switcher |
| `2` | Quick Menu |

Nocturne defaults to `0`. Persisted values outside the valid range are clamped when the frontend starts.

## Palettes

Nocturne includes ten dark palettes:

| Palette | Character |
| --- | --- |
| Nocturne | Neutral dark surfaces with a cool selection accent |
| Ember | Warm ember accent on near-black brown surfaces |
| Iris | Violet accent on deep indigo surfaces |
| Tidal | Teal accent on blue-green surfaces |
| Obsidian Gold | Gold accent on warm graphite surfaces |
| Midnight Sapphire | Sapphire accent on deep navy surfaces |
| Aurora Pine | Mint accent on dark pine surfaces |
| Crimson Noir | Rose accent on oxblood surfaces |
| Graphite Copper | Copper accent on neutral graphite surfaces |
| Velvet Indigo | Lavender accent on dark indigo surfaces |

### Custom palettes

Palette files live in `.system/res/palettes`. Each file uses seven semantic color slots:

```ini
version=1
name=Obsidian Gold
color1=0xD8B36AFF
color2=0x14130FFF
color3=0x2A261CFF
color4=0xF2ECDFFF
color5=0x120E06FF
color6=0xC2B48FFF
color7=0x080705FF
```

| Slot | Role |
| --- | --- |
| `color1` | Selection fill |
| `color2` | Header and footer surface |
| `color3` | Raised surface |
| `color4` | Primary text |
| `color5` | Text on the selection |
| `color6` | Secondary text and hints |
| `color7` | Backdrop |

Colors use `0xRRGGBBAA`. Imported colors are normalized for dark surfaces and readable text contrast. The original palette file is not rewritten.

To add a palette, copy a file into `.system/res/palettes`, change `name` and the color values, then select it in Settings, Appearance, Color Palette.

## Fonts

Departure Mono is the default font. The frontend loads fonts from `.system/res` and lists them in Settings, Appearance, Font.

| Setting | Values |
| --- | --- |
| Font | Any supported font file in `.system/res` |
| Font style | Normal or bold |
| Font size | Compact, Comfortable, Large, Extra Large |

The four size steps map to progressively larger frontend and list text. The default is Comfortable, tuned for the 640 x 480 RG40XX H panel. The font menu lists the fonts present in `.system/res`. The stock build ships Departure Mono, Silkscreen, Home Video, and Fake Receipt.

Legacy `font=0` and `font=1` values from older NextUI configurations are migrated to Departure Mono on load.

To install a font, copy a `.ttf` or `.otf` file into `.system/res` and select it in Settings. Custom fonts are sorted, and filenames are shown without extensions or `-Regular` suffixes.

## Motion

| Setting | Values | Effect |
| --- | --- | --- |
| Menu animations | On or off | Enables selection glide and title scrolling |
| Menu transitions | Off, Snappy, Comfortable | Controls screen transition duration |

Snappy transitions run for 100 ms. Comfortable transitions run for 160 ms. Selection glide runs for eight eased frames at 60 fps and paces each frame against elapsed time, so the motion stays consistent when a frame takes longer to present.

Disabling menu animations removes the selection glide and title scrolling. Disabling transitions removes the screen transition animation.

## In-game shortcuts

MinArch shortcut bindings are stored in the active MinArch configuration under `.userdata`. The default is Game Switcher on Menu + R1. Other actions are unassigned until bound in the MinArch menu.

The one-time shortcut migration is described in [CONTROLS.md](CONTROLS.md).

## Resetting settings

To restore frontend defaults, close the frontend, back up `.userdata/shared/minuisettings.txt`, and delete the file. The frontend recreates it with default values on the next start.

Deleting the file does not affect ROMs, saves, save states, or per-game MinArch configuration.
