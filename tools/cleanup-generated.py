"""Remove rebuildable test artifacts while preserving incremental build caches."""

import argparse
from pathlib import Path
import shutil


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = (ROOT / "out").resolve()
TOP_LEVEL_KEEP = {
    "android-arm64",
    "android-arm64-release",
    "media-sdk",
    "release-sources",
    "shader-tool",
    "vcpkg-media-lgpl",
    "vcpkg-physics",
    "vcpkg-physics-buildtrees",
    "vcpkg-physics-packages",
    "vcpkg-scene",
    "vcpkg-scene-buildtrees",
    "vcpkg-scene-packages",
    "windows",
    "windows-release",
}
WINDOWS_RELEASE_KEEP = {"CMakeFiles", "content", "deps", "generated", "src"}


def inside_output(path: Path) -> Path:
    resolved = path.resolve()
    resolved.relative_to(OUTPUT)
    if resolved == OUTPUT:
        raise RuntimeError("Refusing to remove the output root")
    return resolved


def directory_bytes(path: Path) -> int:
    return sum(entry.stat().st_size for entry in path.rglob("*") if entry.is_file())


def candidates(include_top_level: bool = True) -> list[Path]:
    result = []
    if not OUTPUT.is_dir():
        return result
    if include_top_level:
        for entry in OUTPUT.iterdir():
            if (entry.is_dir() or entry.is_symlink()) and entry.name not in TOP_LEVEL_KEEP:
                if not entry.name.endswith(".log.runs"):
                    result.append(entry)
    release = OUTPUT / "windows-release"
    if release.is_dir():
        for entry in release.iterdir():
            if (entry.is_dir() or entry.is_symlink()) and entry.name not in WINDOWS_RELEASE_KEEP:
                if not entry.name.endswith(".log.runs"):
                    result.append(entry)
        result.extend([
            release / "content/template-switch-tests",
            release / "src/windows_spike/deploy/content/template-switch-tests",
            release / "src/windows_player/deploy/content/template-switch-tests",
        ])
    unique = {inside_output(path) for path in result if path.exists() or path.is_symlink()}
    return sorted(unique)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Perform the listed removals")
    parser.add_argument("--delivery", action="store_true",
                        help="Limit cleanup to Windows delivery test artifacts")
    args = parser.parse_args()
    targets = candidates(include_top_level=not args.delivery)
    total = sum(directory_bytes(path) for path in targets if path.is_dir() and not path.is_symlink())
    for path in targets:
        print(path.relative_to(ROOT))
    print(f"{len(targets)} generated directories, {total / (1024 ** 3):.2f} GiB")
    if not args.apply:
        print("Dry run; pass --apply to remove them.")
        return
    for path in targets:
        inside_output(path)
        if path.is_symlink():
            path.unlink()
        else:
            shutil.rmtree(path)
    print("Generated test artifacts removed.")


if __name__ == "__main__":
    main()
