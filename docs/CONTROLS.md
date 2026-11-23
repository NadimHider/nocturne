# Controls

Nocturne keeps the NextUI control model and adds H700-specific defaults. This page lists the frontend, Quick Menu, Game Switcher, and in-game controls.

## Frontend

| Input | Action |
| --- | --- |
| D-pad | Move the selection |
| L1 / R1 | Jump to the previous or next letter in game lists |
| A | Open the selected item |
| B | Go back |
| Menu | Open Settings, or Quick Menu if Settings is unavailable |
| Start | Open Quick Menu |
| Menu + R1 | Open Game Switcher from the list, Quick Menu, or Settings |
| Menu + Volume | Adjust brightness |
| Power | Sleep |

Menu and Start are separate actions. Menu opens Settings. Start opens Quick Menu. Menu + R1 opens Game Switcher.

## Quick Menu

Quick Menu provides fast access to common destinations and toggles from the frontend.

| Input | Action |
| --- | --- |
| D-pad | Move between entries |
| A | Toggle or open the selected entry |
| B | Close Quick Menu |

The Quick Menu includes a Settings shortcut when the Settings pak is installed, along with Wi-Fi, Bluetooth, sleep, reboot, and power-off toggles where the device supports them.

## Game Switcher

Game Switcher shows recently played games with a preview and a position counter.

| Input | Action |
| --- | --- |
| Left / Right | Previous or next recent game |
| A | Resume the selected game |
| Y, Y | Remove the selected entry from the recent list |
| B | Cancel or go back |

Removal requires two presses of Y. The first press changes the footer to Cancel and Confirm. The second press removes the entry. Removing an entry from the recent list does not delete its ROM, save file, or save state.

If a preview is unavailable, the switcher keeps the game available and shows a fallback message. If the emulator or pak for a recent game is missing, the switcher reports the game as unavailable and offers removal.

## In game

| Input | Action |
| --- | --- |
| Menu | Open the MinArch menu |
| Menu + R1 | Open Game Switcher |
| Menu + Volume | Adjust brightness |
| Power | Sleep |

The MinArch menu provides save states, load states, reset, options, controls, shaders, cheats, and RetroAchievements.

## In-game shortcuts

Nocturne ships conservative shortcut defaults. Game Switcher is bound to Menu + R1. Save state, load state, save and quit, fast-forward, rewind, screenshots, reset, and turbo actions are unassigned until you bind them.

To bind a shortcut:

1. Launch a game.
2. Press Menu to open the MinArch menu.
3. Open Options, then Shortcuts.
4. Select an action and press the button combination you want to assign.
5. Save the configuration for the console or the game.

Shortcuts support single buttons and Menu + button combinations. Bindings are stored in the MinArch configuration under `.userdata`.

### Shortcut migration

Earlier Nocturne builds shipped a broader set of default shortcuts, including Menu + X, Menu + Y, Menu + Start, Menu + Select, Menu + R1, and Menu + L1. Nocturne v1.0.0 migrates configurations that still match those legacy defaults to unassigned actions, except for Game Switcher, which remains on Menu + R1.

The migration runs once and records `nocturne_shortcuts = 1` in the active MinArch configuration. Custom bindings that do not match the legacy defaults are preserved.

## Settings navigation

| Input | Action |
| --- | --- |
| D-pad | Move between settings |
| A | Open a submenu or change a value |
| B | Go back |
| Left / Right | Change a value in place |

Settings is organized into Appearance, Display, System, In-Game, Network, Bluetooth, and About. The full settings reference is in [CONFIGURATION.md](CONFIGURATION.md).
