"""Copy changed resource files without touching unchanged incremental outputs."""
import argparse
import filecmp
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    if not args.source.is_dir():
        raise FileNotFoundError(args.source)
    for source in args.source.rglob("*"):
        if not source.is_file() or source.name == ".writer" or source.suffix == ".tmp":
            continue
        destination = args.destination / source.relative_to(args.source)
        if destination.is_file() and filecmp.cmp(source, destination, shallow=False):
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


if __name__ == "__main__":
    main()
