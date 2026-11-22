"""Assemble the Nocturne component update from an existing H700 build."""

import argparse
import re
from hashlib import sha256
from pathlib import Path
from stat import S_IFREG
from tempfile import TemporaryDirectory
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

COMPONENTS = {
    ".system/h700/bin/nextui.elf": "workspace/all/nextui/build/h700/nextui.elf",
    ".system/h700/bin/minarch.elf": "workspace/all/minarch/build/h700/minarch.elf",
    ".system/h700/bin/nextval.elf": "workspace/all/nextval/build/h700/nextval.elf",
    ".system/h700/bin/syncsettings.elf": "workspace/all/syncsettings/build/h700/syncsettings.elf",
    ".system/h700/bin/keymon.elf": "workspace/h700/keymon/keymon.elf",
    ".system/h700/bin/suspend": "skeleton/SYSTEM/h700/bin/suspend",
    ".system/h700/bin/run_hooks.sh": "skeleton/SYSTEM/h700/bin/run_hooks.sh",
    ".system/h700/dat/dmenu.bin": "workspace/h700/boot/output/dmenu.bin",
    "Tools/h700/Settings.pak/settings.elf": "workspace/all/settings/build/h700/settings.elf",
    "Tools/h700/LedControl.pak/ledcontrol.elf": "workspace/all/ledcontrol/build/h700/ledcontrol.elf",
    "Tools/h700/CleanRomset.pak/launch.sh": "skeleton/EXTRAS/Tools/h700/CleanRomset.pak/launch.sh",
}
RESOURCES = Path("skeleton/SYSTEM/res")
REQUIRED_RESOURCES = (
    "DepartureMono-Regular.otf",
    "Fake Receipt.otf",
    "HomeVideo-Regular.ttf",
    "Silkscreen-Regular.ttf",
    "assets@1x.png",
    "palettes/Default.txt",
)


def _component_files() -> dict[str, Path]:
    files = {name: Path(source) for name, source in COMPONENTS.items()}
    resources = {RESOURCES / name for name in REQUIRED_RESOURCES}
    resources.update(
        path for path in RESOURCES.iterdir()
        if path.suffix in {".png", ".svg", ".otf", ".ttf"}
    )
    resources.update((RESOURCES / "palettes").glob("*.txt"))
    files.update({f".system/res/{path.relative_to(RESOURCES).as_posix()}": path for path in resources})
    if len({name.casefold() for name in files}) != len(files):
        raise ValueError("Component paths conflict on the SD card's case-insensitive filesystem")
    for path in files.values():
        if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"Missing or invalid component: {path.as_posix()}")
    return files


def _write_zip_entry(archive: ZipFile, name: str, data: bytes) -> None:
    # Do not inherit checkout timestamps or platform-dependent permission bits.
    entry = ZipInfo(name)
    executable = name.endswith((".elf", ".sh", "/suspend"))
    entry.create_system = 3  # Unix file attributes
    entry.external_attr = (S_IFREG | (0o755 if executable else 0o644)) << 16
    archive.writestr(entry, data, compress_type=ZIP_DEFLATED)


def package_h700(output_dir: Path, commit: str, version: str) -> Path:
    """Write and verify the component ZIP, then write its SHA-256 sidecar."""
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Expected a full Git commit SHA")
    if version and not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("Expected a release version in vMAJOR.MINOR.PATCH format")
    short_commit = commit[:7]
    version = version or f"dev-{short_commit}"
    files = _component_files()
    version_file = ".system/version.txt"
    filename = f"Nocturne-{version}-H700-components.zip"
    output_dir.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(dir=output_dir) as temp:
        staged = Path(temp) / filename
        with ZipFile(staged, "w") as archive:
            for name, source in sorted(files.items()):
                _write_zip_entry(archive, name, source.read_bytes())
            _write_zip_entry(archive, version_file, f"Nocturne {version}\n{short_commit}\n".encode())
        with ZipFile(staged) as archive:
            if set(archive.namelist()) != files.keys() | {version_file} or archive.testzip() is not None:
                raise ValueError("The component archive failed verification")
        checksum = sha256(staged.read_bytes()).hexdigest()
        sidecar = staged.with_suffix(".zip.sha256")
        sidecar.write_bytes(f"{checksum}  {filename}\n".encode())
        archive_path = staged.replace(output_dir / filename)
        sidecar.replace(output_dir / sidecar.name)
    return archive_path


def main() -> None:
    """Run from the repository root after building H700 components."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--commit", required=True, help="Full Git SHA used to build the components")
    parser.add_argument("--version", default="", help="Release tag, or leave empty for a CI build")
    args = parser.parse_args()
    try:
        print(package_h700(args.output_dir, args.commit, args.version))
    except (OSError, ValueError) as error:
        parser.exit(1, f"H700 packaging failed: {error}\n")


if __name__ == "__main__":
    main()
