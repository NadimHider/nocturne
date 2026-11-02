"""Catch the SDL destination-rectangle regression that hid footer captions.

The real app captures use the default palette on a fixed 640x480 display.
The right-hand OPEN caption must have neutral light text beyond its button.
"""
from pathlib import Path

from PIL import Image


def check_previews(directory: Path) -> None:
    for size in range(4):
        for screen in ("nextui", "game-list"):
            path = directory / f"{screen}-{size}.png"
            with Image.open(path) as image:
                assert image.size == (640, 480), path
                caption = image.convert("RGB").crop((540, 410, 615, 450))
                neutral_text = sum(
                    min(pixel) > 80 and max(pixel) - min(pixel) < 40
                    for pixel in caption.getdata()
                )
                assert neutral_text > 30, f"Missing footer caption: {path}"
    print("UI footer captions: ok (all four font sizes)")


if __name__ == "__main__":
    check_previews(Path("tests/nocturne/build/previews"))
