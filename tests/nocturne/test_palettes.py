#!/usr/bin/env python3
"""Validate shipped palette files using the normalization in common/theme.h.

Run from any directory with Python 3; no third-party packages are required.
The channel math, integer truncation, and 16-step searches deliberately match C.
Keep these functions in sync with theme.h when changing palette normalization.
"""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
PALETTES = ROOT / "skeleton/SYSTEM/res/palettes"
ON_SELECT = 0x071210FF
TEXT = 0xE4EBEFFF
RAISED = 0x24313AFF


def rgb(color):
    return [(color >> shift) & 255 for shift in (24, 16, 8)]


def packed(channels):
    r, g, b = (int(channel) for channel in channels)
    return (r << 24) | (g << 16) | (b << 8) | 255


def luminance(color):
    channels = [value / 255 for value in rgb(color)]
    linear = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
              for v in channels]
    return sum(v * weight for v, weight in zip(linear, (0.2126, 0.7152, 0.0722)))


def contrast(a, b):
    light, dark = sorted((luminance(a), luminance(b)), reverse=True)
    return (light + 0.05) / (dark + 0.05)


def limit_luminance(color, ceiling):
    if luminance(color) <= ceiling:
        return color | 255
    lo, hi = 0.0, 1.0
    for _ in range(16):
        scale = (lo + hi) * 0.5
        if luminance(packed(v * scale for v in rgb(color))) > ceiling:
            hi = scale
        else:
            lo = scale
    return packed(v * lo for v in rgb(color))


def ensure_contrast(color, background, minimum):
    color |= 255
    if contrast(color, background) >= minimum:
        return color
    lo, hi = 0.0, 1.0
    for _ in range(16):
        t = (lo + hi) * 0.5
        candidate = packed(v + (255 - v) * t for v in rgb(color))
        if contrast(candidate, background) >= minimum:
            hi = t
        else:
            lo = t
    return packed(v + (255 - v) * hi for v in rgb(color))


def normalize(colors):
    result = [color | 255 for color in colors]
    for index, ceiling in ((1, 0.035), (2, 0.070), (6, 0.015)):
        result[index] = limit_luminance(result[index], ceiling)
    for index in (3, 5):
        result[index] = ensure_contrast(result[index], RAISED, 4.5)
    result[4] = (ON_SELECT if contrast(ON_SELECT, result[0]) >= contrast(TEXT, result[0])
                 else TEXT)
    return result


class PaletteTests(unittest.TestCase):
    def test_shipped_palettes(self):
        paths = sorted(PALETTES.glob("*.txt"))
        self.assertEqual(len(paths), 35, "Ship the original 10 plus 25 new palettes")
        names, signatures, night_count = set(), set(), 0
        for path in paths:
            with self.subTest(palette=path.stem):
                pairs = [line.split("=", 1) for line in path.read_text().splitlines()
                         if line and not line.startswith("#")]
                fields = dict(pairs)
                self.assertEqual(len(pairs), len(fields), "Duplicate palette key")
                self.assertEqual(set(fields), {"version", "name", *(f"color{i}" for i in range(1, 8))})
                self.assertEqual(fields["version"], "1")
                name = fields["name"]
                self.assertTrue(name)
                self.assertLess(len(name.encode("utf-8")), 64)
                self.assertNotIn(name.casefold(), names)
                names.add(name.casefold())
                for index in range(1, 8):
                    self.assertRegex(fields[f"color{index}"], r"^0x[0-9A-F]{6}FF$")
                colors = normalize([int(fields[f"color{i}"], 16) for i in range(1, 8)])
                self.assertNotIn(tuple(colors), signatures, "Visually identical palette")
                signatures.add(tuple(colors))
                for index, ceiling in ((1, 0.035), (2, 0.070), (6, 0.015)):
                    self.assertLessEqual(luminance(colors[index]), ceiling + 0.000001)
                # Actual backgrounds, rather than only the reference raised surface.
                for text_index in (3, 5):
                    for surface_index in (1, 2, 6):
                        self.assertGreaterEqual(contrast(colors[text_index], colors[surface_index]), 4.499,
                                                f"color{text_index + 1} on color{surface_index + 1}")
                self.assertGreaterEqual(contrast(colors[4], colors[0]), 4.5)
                if name.startswith("Night "):
                    night_count += 1
                    self.assertLessEqual(luminance(colors[6]), 0.002)
                    self.assertLessEqual(luminance(colors[2]), 0.02)
                    self.assertLessEqual(luminance(colors[3]), 0.6)
                    self.assertLessEqual(luminance(colors[0]), 0.4)
        self.assertEqual(night_count, 10)


if __name__ == "__main__":
    unittest.main()
