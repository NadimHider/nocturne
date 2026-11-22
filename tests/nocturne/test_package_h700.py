"""Test the release packager without a cross-toolchain."""

import hashlib
import os
import subprocess
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZipFile

PACKAGER = Path(__file__).resolve().parents[2] / "scripts" / "package_h700.py"
COMMIT = "0123456789abcdef" * 2 + "01234567"
NAME = "Nocturne-v1.0.4-H700-components.zip"
BOOT = "workspace/h700/boot/output/dmenu.bin"
FILES = {
    "workspace/all/nextui/build/h700/nextui.elf": ".system/h700/bin/nextui.elf",
    "workspace/all/minarch/build/h700/minarch.elf": ".system/h700/bin/minarch.elf",
    "workspace/all/nextval/build/h700/nextval.elf": ".system/h700/bin/nextval.elf",
    "workspace/all/syncsettings/build/h700/syncsettings.elf": ".system/h700/bin/syncsettings.elf",
    "workspace/h700/keymon/keymon.elf": ".system/h700/bin/keymon.elf",
    "skeleton/SYSTEM/h700/bin/suspend": ".system/h700/bin/suspend",
    "skeleton/SYSTEM/h700/bin/run_hooks.sh": ".system/h700/bin/run_hooks.sh",
    BOOT: ".system/h700/dat/dmenu.bin",
    "workspace/all/settings/build/h700/settings.elf": "Tools/h700/Settings.pak/settings.elf",
    "workspace/all/ledcontrol/build/h700/ledcontrol.elf": "Tools/h700/LedControl.pak/ledcontrol.elf",
    "skeleton/EXTRAS/Tools/h700/CleanRomset.pak/launch.sh": "Tools/h700/CleanRomset.pak/launch.sh",
}
RESOURCES = (
    "DepartureMono-Regular.otf",
    "Fake Receipt.otf",
    "HomeVideo-Regular.ttf",
    "Silkscreen-Regular.ttf",
    "assets@1x.png",
    "palettes/Default.txt",
    "palettes/Night Graphite.txt",
)


class PackageTest(unittest.TestCase):
    def setUp(self) -> None:
        temp = TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.output = self.root / "out"
        for path in (*FILES, *(f"skeleton/SYSTEM/res/{name}" for name in RESOURCES)):
            self.make_file(path)
        self.make_file("skeleton/SYSTEM/res/unrelated.bios")
        self.make_file("skeleton/SYSTEM/res/cores/test_libretro.so")
        self.make_file("Saves/test.sav")

    def make_file(self, path: str) -> None:
        source = self.root / path
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes(path.encode())

    def package(self, version: str = "v1.0.4") -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable, str(PACKAGER), "--commit", COMMIT,
                "--version", version, "--output-dir", str(self.output),
            ],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

    def test_release_contains_only_components_and_has_matching_checksum(self) -> None:
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)
        archive = self.output / NAME
        assets = dict(FILES)
        assets.update({f"skeleton/SYSTEM/res/{name}": f".system/res/{name}" for name in RESOURCES})
        with ZipFile(archive) as package:
            self.assertIsNone(package.testzip())
            self.assertCountEqual(package.namelist(), [*assets.values(), ".system/version.txt"])
            self.assertEqual(package.read(".system/version.txt"), b"Nocturne v1.0.4\n0123456\n")
            for source, name in assets.items():
                self.assertEqual(package.read(name), source.encode())
            self.assertTrue(package.getinfo(".system/h700/bin/suspend").external_attr >> 16 & 0o111)
        checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
        self.assertEqual((self.output / f"{NAME}.sha256").read_text(), f"{checksum}  {NAME}\n")

    def test_archive_is_stable_when_source_timestamps_change(self) -> None:
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)
        archive = self.output / NAME
        before = archive.read_bytes()
        os.utime(self.root / BOOT, (1_650_000_000, 1_650_000_000))
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(archive.read_bytes(), before)

    def test_missing_boot_payload_fails_without_creating_archive(self) -> None:
        (self.root / BOOT).unlink()
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(BOOT, result.stderr)
        self.assertFalse(self.output.exists())

    def test_empty_required_resource_fails_without_creating_archive(self) -> None:
        (self.root / "skeleton/SYSTEM/res/palettes/Default.txt").write_bytes(b"")
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("palettes/Default.txt", result.stderr)
        self.assertFalse(self.output.exists())

    def test_rejects_version_that_is_not_a_release_tag(self) -> None:
        result = self.package("v1.0.4/..")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("version", result.stderr)
        self.assertFalse(self.output.exists())

    def test_untagged_build_has_a_distinct_name(self) -> None:
        result = self.package("")
        self.assertEqual(result.returncode, 0, result.stderr)
        archive = self.output / "Nocturne-dev-0123456-H700-components.zip"
        with ZipFile(archive) as package:
            self.assertEqual(package.read(".system/version.txt"), b"Nocturne dev-0123456\n0123456\n")


if __name__ == "__main__":
    unittest.main()
