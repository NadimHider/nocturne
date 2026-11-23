# Validation

See the [latest release notes](https://github.com/NadimHider/nocturne/releases/latest) for
the current artifact's checksum, build, and hardware validation status.
For v1.0.3, see its [release notes and CI runs](https://github.com/NadimHider/nocturne/releases/tag/v1.0.3).
Desktop previews and mocked sleep tests do not substitute for a physical boot,
game launch and save/resume, or repeated sleep/wake on the RG40XX H.

## Historical validation: v1.0.2

The record below applies to v1.0.2 only.

## Build provenance

| Item | Value |
| --- | --- |
| Release | Nocturne v1.0.2 |
| Binary commit | `c2a09365` |
| H700 workflow | [41175554962](https://github.com/NadimHider/nocturne/actions/runs/41175554962), success |
| Full CI | [41175554920](https://github.com/NadimHider/nocturne/actions/runs/41175554920), success |
| Device | Anbernic RG40XX H |
| Card | exFAT, label `Roms` |
| Validation date | 2026-10-31 |

The release tag includes the documentation update that follows the binary commit. The binaries in the release archive were built from `c2a09365`.

The device checks were performed with the frontend binaries built from `04577524` and the boot payload built from `71656dc7`. Commits `71656dc7` and `c2a09365` do not change frontend source. They add the Departure Mono boot splash, align installer text with the Nocturne name, and add the boot payload to CI. The release binaries were rebuilt from `c2a09365`; `minarch.elf` and `settings.elf` differ from the earlier build only because the cross-build is not byte-reproducible. The boot payload in the release is content-identical to the payload validated on the card.

## Automated checks

### Host policy tests

Command:

```sh
make test-nocturne
```

Result: pass. The policy sources are unchanged from v1.0.1, and the same test suite passed for that release.

- Theme normalization: pass
- Switcher navigation and removal policy: pass
- Preview cache promotion, eviction, and clear: pass

The tests cover luminance and contrast normalization, selection text derivation, recent-list navigation, two-step removal, and the three-entry LRU preview cache. The policy sources are unchanged from v1.0.1.

### Full CI

Run [41175554920](https://github.com/NadimHider/nocturne/actions/runs/41175554920) completed successfully.

- All core build jobs passed.
- The aggregate build job passed.
- The package, upload, and attestation stages passed.

### H700 build

Run [41175554962](https://github.com/NadimHider/nocturne/actions/runs/41175554962) completed successfully.

- The H700 cross-build passed.
- The update archive was packaged and uploaded.
- The boot payload was packaged and uploaded as the `h700-boot` artifact.
- The archives were downloaded and installed for validation.

## Binary verification

The installed binaries are aarch64 ELF files. Their `DT_NEEDED` dependency lists match the previous build.

```text
nextui.elf       37A14A6134427FEE5069C34950DBE9FC08540342528B9F29C6128F14AB861D70
minarch.elf      D77B5742F8818AF7E27E4377166182D60735F3072453DB07F879457991A0E723
keymon.elf       2FDD6E090DD8D4C0A624F7C20F28B83EA2FFAADCBC87F19226B136C6A217E025
nextval.elf      2A97D480AE25B83F998511DBAD7F52763806DB7D5151511501F43C54B237213C
syncsettings.elf D3A6C0AA1C3CD338DA5CB5B167CF5758A9253BD7E6E9C333F90C6066A193C673
settings.elf     69D495AC816C38204B28436CC5E5DF07ED0146A532DDD73564047EB7C7DCF6CD
ledcontrol.elf   DE1548F3D209DD783F60717F9619F4D146257091B3EBBF4A375C98B4C67F5BAE
```

| Check | Result |
| --- | --- |
| ELF class | 64-bit |
| Machine | AArch64 (183) |
| Dependency changes | None |
| Frontend hash matches artifact | Pass |
| MinArch hash matches artifact | Pass |
| Settings hash matches artifact | Pass |

## Boot payload verification

The boot shim is a shell script followed by a gzip-compressed tar archive. The embedded payload contains `fbsplash`, `unzip`, and `shim-common.sh`.

```text
dmenu.bin        CFD545ADF2761D0022A164ED49AECD57908CF607706B74ECB3DEC0D9CDDBF7D1
```

| Check | Result |
| --- | --- |
| Script line endings | LF only |
| Payload archive | Valid gzip and tar |
| `fbsplash` machine | AArch64 (183) |
| `fbsplash` GLIBC requirement | 2.17 |
| Splash strings | `NOCTURNE` and `STARTING NOCTURNE` present |
| Legacy splash strings | No `NEXTUI` splash strings |
| Validated payload match | Content-identical to the payload tested on the device |

The payload renders Departure Mono glyphs sampled on the font's native 11 px grid. It does not link SDL_ttf, so the splash works before the card is mounted.

## Card verification

| Check | Result |
| --- | --- |
| `.system`, `.userdata`, `Tools`, `Emus`, `Bios`, `Saves`, `Shaders`, `Roms` at card root | Pass |
| `.system/h700/bin` contains the release binaries | Pass |
| `.system/h700/dat/dmenu.bin` contains the release boot payload | Pass |
| `Tools/h700/Settings.pak` contains the release Settings binary | Pass |
| `Tools/h700/LedControl.pak` contains the release LED Control binary | Pass |
| `Tools/h700/CleanRomset.pak` contains the release cleanup script | Pass |
| `defaultView=0` in `minuisettings.txt` | Pass |
| Built-in palettes present | Pass |
| Removed Next and OG font files absent | Pass |
| Pak Store paks and Quick Menu entry absent | Pass |
| Zero-byte files under `.system` | None |
| `chkdsk` file system scan | No problems |
| Dirty bit | Not set |
| Volume cache | Flushed before eject |

## Device checklist

| Check | Result |
| --- | --- |
| Boot splash reads `NOCTURNE` in Departure Mono with the teal rule and `STARTING NOCTURNE` | Pass |
| Boots to the game-system list by default | Pass |
| Pills render with clean rounded corners and centered text | Pass |
| List typography is reduced and remains readable | Pass |
| Selection glide is smooth | Pass |
| Screen transitions are smooth | Pass |
| Font menu lists only Departure Mono, Silkscreen, Home Video, and Fake Receipt | Pass |
| Font choice persists across frontend restarts | Pass |
| Quick Menu has no Pak Store entry | Pass |
| SFC game art appears for Soul Blazer | Pass |
| PS game art appears for MediEvil II | Pass |
| Arcade game art appears for 1942 | Pass |
| Game launches and exits to the frontend | Pass |

The device checks were performed on the RG40XX H. The boot payload is copied from `.system/h700/dat/dmenu.bin` to the boot partition by `h700-init.sh` on the first boot after installation, so the new splash appears on the following boot.

## Reproducing the build

```sh
git clone https://github.com/NadimHider/nocturne.git
cd nocturne
git checkout c2a09365
make setup
make build PLATFORM=h700 COMPILE_CORES=false
```

The H700 workflow uses the same source and toolchain image. The build is not reproducible byte for byte for `minarch.elf` and `settings.elf`, so release artifacts should be taken from the workflow run that matches the release commit.

## Known limitations

- Nocturne is a component bundle. It does not install Base OS, a bootloader, a kernel, emulator cores, BIOS files, or ROMs.
- The release targets the H700 platform and the RG40XX H. Other H700 devices may work but are not part of this validation.
- Emulator performance and compatibility depend on the selected libretro core, ROM set, and BIOS files.
- Wi-Fi and Bluetooth behavior depends on the Base OS drivers and the local network or accessory.
- HDMI behavior depends on the display and cable. Hotplug handling restarts the frontend by design.
- Game art is card data. The frontend reads `<rom-name>.png` from each system's `.media` folder. The release does not include art.

## Release artifact

`Nocturne-v1.0.2-H700-components.zip` contains the frontend, MinArch, key monitor, supporting utilities, the boot payload, ten palettes, Settings, LED Control, CleanRomset, and `.system/version.txt`.

SHA256:

```text
1A53EA3312D97FB6D390519BDA863C2B7FFB2DE4FEAD5BB123FA132C3A9F3AB8
```

Verify the digest before extracting the archive to a card.
