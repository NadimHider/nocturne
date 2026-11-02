# Dark palettes

Nocturne ships 35 built-in palettes: the original 10 plus 25 additional options.
Choose **Settings → Appearance → Color Palette**. Left/Right applies a palette
immediately, and Confirm opens the complete list. Individual color edits switch
the selection to Custom; choosing Custom restores the last custom colors.

All palettes use the same seven semantic roles: selection accent, header/footer
surface, raised surface, primary text, selected text, secondary text, and backdrop.
The loader normalizes dark surfaces and readable text, and derives selected text
from its contrast against the selection accent. Colors below are RGB; palette files
store opaque RGBA values.

## Everyday dark additions

These 15 palettes use muted accents and distinct dark surfaces with clear text.

| Palette | Accent | Backdrop | Character |
| --- | --- | --- | --- |
| Slate Mist | `#A9BDC9` | `#080B0F` | Cool slate and soft blue-gray. |
| Ocean Ink | `#72B8D4` | `#060C12` | Blue-black surfaces with a sea-blue accent. |
| Moss Stone | `#A8BA89` | `#090D07` | Muted olive on moss-charcoal. |
| Plum Smoke | `#B8A0CB` | `#0C080F` | Dusty plum with cool mauve text. |
| Rosewood | `#D1A0AA` | `#0F080B` | Warm rose accents over wine-charcoal. |
| Cocoa Ash | `#C5AD94` | `#0D0A07` | Soft sand on dark cocoa. |
| Storm Teal | `#79BBB7` | `#060D0E` | Desaturated teal with storm-gray surfaces. |
| Denim Dusk | `#91AAD7` | `#080B13` | Quiet denim blues. |
| Mulberry | `#C79CC0` | `#0F070F` | Berry accent with purple-black surfaces. |
| Desert Night | `#CDBA83` | `#0D0C07` | Pale ochre on dark desert stone. |
| Silver Fir | `#9CB9AD` | `#080E0A` | Silver-green with fir-charcoal. |
| Lavender Fog | `#AAA8D2` | `#0A0A10` | Gray lavender and violet slate. |
| Rust Velvet | `#C5A08C` | `#0F0A07` | Muted terracotta and warm brown. |
| Arctic Slate | `#98C6D0` | `#070C10` | Ice-blue accent on blue-gray. |
| Sage Linen | `#B9C4A3` | `#0B0E08` | Warm sage with neutral green surfaces. |

## Dark-room additions

The 10 palettes prefixed **Night** use near-black backdrops, darker raised surfaces,
softened primary text, and dimmer accents. They retain at least 4.5:1 contrast for
primary and secondary text on each shipped surface. They deliberately avoid pure
white selection fills. Set the handheld's display brightness to suit the room;
a palette does not control the LCD backlight or change the colors inside a game.

| Palette | Accent | Backdrop | Character |
| --- | --- | --- | --- |
| Night Amber | `#A89170` | `#020201` | Soft amber for a warm dark-room view. |
| Night Fern | `#8C9F80` | `#010301` | Low-glare fern green. |
| Night Harbor | `#7D9CA8` | `#010203` | Muted harbor blue. |
| Night Heather | `#A08BA7` | `#030103` | Soft heather violet. |
| Night Clay | `#AA8D7D` | `#030201` | Warm clay without a bright white accent. |
| Night Graphite | `#979B9E` | `#020202` | Neutral near-black and softened silver. |
| Night Tide | `#7B9F98` | `#010302` | Muted sea-green. |
| Night Garnet | `#AA8891` | `#030102` | Dusty garnet over deep burgundy. |
| Night Indigo | `#8E92AE` | `#010103` | Soft indigo over deep blue-black. |
| Night Olive | `#9F9C79` | `#020201` | Dim olive and warm stone. |

## Existing palettes

Nocturne, Ember, Iris, Tidal, Obsidian Gold, Midnight Sapphire, Aurora Pine,
Crimson Noir, Graphite Copper, and Velvet Indigo remain available unchanged.

## Custom palettes

Built-in files live in `.system/res/palettes`. Put personal palette files in
`/Palettes` at the SD card root so they remain separate from shipped files.
Start with a copy of an existing file and give it a unique `name=` value.

```text
version=1
name=My Dark Palette
color1=0x8BD3C7FF
color2=0x12191FFF
color3=0x24313AFF
color4=0xE4EBEFFF
color5=0x071210FF
color6=0xA7B6C0FF
color7=0x070A0DFF
```

The selected-text color (`color5`) is derived by the loader. The other text colors
may be brightened and surfaces darkened during normalization; always judge a
custom palette on the device after applying it.

## Validation

Run `python3 tests/nocturne/test_palettes.py` from the repository root. It validates
all 35 shipped files, their names and seven opaque colors, uniqueness after
normalization, text contrast on all three background surfaces, selection contrast,
and additional luminance limits for the 10 Night palettes. The standard-library
checker mirrors the 16-step normalization in `workspace/all/common/theme.h`;
keep it in sync with changes to that header. Hardware review is still required
for perceived glare and readability at different backlight settings.
