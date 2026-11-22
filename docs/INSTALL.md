# Installation

Nocturne is distributed as an H700 component bundle for an existing Base OS and NextUI card. It updates the frontend, MinArch, key monitor, supporting utilities, and tool paks. It does not create a bootable card, and it does not include a bootloader, kernel, Base OS, emulator cores, BIOS files, or ROMs.

## Supported device

| Item | Requirement |
| --- | --- |
| Device | Anbernic RG40XX H |
| SoC | Allwinner H700 |
| Display | 640 x 480 |
| Base system | Base OS with NextUI installed |
| Card format | exFAT |
| Card layout | `.system`, `.userdata`, `Tools`, `Emus`, `Bios`, `Saves`, `Shaders`, and `Roms` at the card root |

The tested card uses the exFAT label `Roms`. A label or drive letter alone does not identify the correct card. In a two-card setup, locate the card that contains `.system/h700/bin/nextui.elf`, not a card that only contains ROMs.

## Before you start

Back up the card to another drive before updating. In particular, keep a copy of:

- `.system`, including the existing boot payload and shared UI assets
- `.userdata`, which contains frontend settings, per-game configuration, and logs
- `Saves`, which contains save files and save states
- `Roms`, which contains the game library
- `Tools`, including any custom paks
- `Bios`, `Cheats`, and `Shaders`, if you have customized them

The release archive overwrites frontend binaries, palette files, and the bundled tool paks. It does not delete ROMs, saves, or user settings.

## Card layout

The system directories must be at the root of the card:

```text
SD card root
|-- .system
|   |-- h700
|   |   |-- bin
|   |   |-- paks
|   |   `-- cores
|   |-- res
|   |   `-- palettes
|   `-- version.txt
|-- .userdata
|-- Bios
|-- Cheats
|-- Emus
|-- Roms
|-- Saves
|-- Shaders
`-- Tools
```

Placing `.system` or `Tools` inside `Roms` prevents the frontend from finding its binaries and paks. This is the most common cause of a card that appears to boot but never starts the frontend.

## Update an existing card

1. Confirm `.system/h700/bin/nextui.elf` exists on the target card and that your backup is readable. If the file is missing, stop: see [Fresh installation](#fresh-installation) or check the other card slot.
2. Download `Nocturne-v*-H700-components.zip` from the [latest release](https://github.com/NadimHider/nocturne/releases/latest). If using the GitHub CLI, download into an empty folder:

   ```sh
   gh release download --repo NadimHider/nocturne --pattern 'Nocturne-v*-H700-components.zip'
   ```

3. Compare the ZIP's SHA-256 with the release notes or the matching `.zip.sha256` download. Run `Get-FileHash -Algorithm SHA256` on Windows, `sha256sum` on Linux, or `shasum -a 256` on macOS, followed by the filename. Do not use the archive if the hashes differ.
4. Extract to an empty folder on your computer. Copy the contents of the extracted `.system` and `Tools` folders into the matching folders on the card, merging folders and replacing only the files in the archive. Show hidden files if needed. Do not delete the existing folders or move them under `Roms`.
5. Safely eject the card, boot the RG40XX H, and complete the [verification checklist](#verification-checklist). If it fails, follow [Rollback](#rollback).

### Archive contents

| Path | Contents |
| --- | --- |
| `.system/h700/bin/nextui.elf` | Frontend |
| `.system/h700/bin/minarch.elf` | Libretro launcher |
| `.system/h700/bin/keymon.elf` | H700 key monitor |
| `.system/h700/bin/nextval.elf` | Value editor used by the frontend |
| `.system/h700/bin/syncsettings.elf` | Settings synchronization utility |
| `.system/h700/dat/dmenu.bin` | Boot shim and Departure Mono boot splash payload |
| `.system/res/` | Shared UI assets, fonts, and built-in palettes |
| `Tools/h700/Settings.pak/settings.elf` | Settings application |
| `Tools/h700/LedControl.pak/ledcontrol.elf` | LED control application |
| `Tools/h700/CleanRomset.pak/launch.sh` | ROM folder cleanup tool |
| `.system/version.txt` | Release and build stamp |

## Fresh installation

Nocturne does not replace the Base OS or the initial NextUI installation. Prepare the card first:

1. Install Base OS and NextUI on the SD card using the upstream instructions.
2. Boot the device once and confirm that the stock frontend starts.
3. Power off the device and move the card back to the computer.
4. Apply the Nocturne archive to the card root.
5. Add ROMs, BIOS files, and emulator cores as needed.
6. Boot the device and complete the verification checklist.

The upstream NextUI documentation covers Base OS preparation, supported cores, and pak layout:

- https://nextui.loveretro.games/docs/
- https://github.com/LoveRetro/NextUI

## Verification checklist

After the first boot with Nocturne:

- The boot splash reads `NOCTURNE` in Departure Mono with a teal rule and `STARTING NOCTURNE` underneath.
- The frontend opens on the game-system list, not Quick Menu or Settings.
- The list font is smaller than the stock NextUI size and remains readable.
- Selection pills have clean rounded corners and centered text.
- Moving the selection is smooth, with no visible stutter.
- Menu opens Settings. Start opens Quick Menu. Menu + R1 opens Game Switcher.
- Settings, Appearance, Font lists only Departure Mono, Silkscreen, Home Video, and Fake Receipt.
- Changing the font and restarting the frontend preserves the choice.
- Quick Menu has no Pak Store entry.
- Game art appears for games that have a matching `<rom-name>.png` in the system's `.media` folder.
- A game launches, runs, and exits back to the frontend.
- Save states and resume work for the tested core.
- Wi-Fi, Bluetooth, brightness, LED control, sleep, and battery reporting behave as expected.

The boot payload is copied from `.system/h700/dat/dmenu.bin` to the boot partition by `h700-init.sh` on the first boot after installation. If the splash still shows the previous wordmark, reboot once more.

The release validation record is in [VALIDATION.md](VALIDATION.md).

## Rollback

If the device does not boot or the frontend does not start:

1. Power off the device and move the card to the computer.
2. Using the ZIP's file list, restore each replaced file from your backup, including `.system/version.txt` and the boot payload. Remove only files you can confirm were newly added by the update. Leave saves, ROMs, settings, and custom tools alone.
3. If you do not have a backup, try applying the previous Nocturne release. This does not remove files introduced by the newer bundle.
4. Safely eject the card and boot the device. Reboot again if it needs to copy the restored boot payload.

Nocturne does not modify the Base OS partitions or the bootloader, so a component rollback is usually enough.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Device powers on but the frontend does not start | `.system/h700/bin/nextui.elf` exists and `.system` is at the card root |
| Frontend starts but Settings is missing | `Tools/h700/Settings.pak` exists and contains `settings.elf` |
| Game does not launch | The emulator pak, libretro core, and required BIOS files are present |
| Game list is empty | ROM folders use the expected `Folder Name (TAG)` naming and contain supported files |
| Frontend shows a blank screen after an update | Restore the previous binaries from backup |
| Card is not recognized | The card is exFAT, inserted correctly, and was ejected safely |
| Build cannot start | Docker or Podman is available and can pull `ghcr.io/loveretro/h700-toolchain:latest` |

If the card reports file system errors, stop the update and make a readable backup before attempting repairs.
