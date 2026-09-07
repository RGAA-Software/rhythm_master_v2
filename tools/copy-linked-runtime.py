"""Copy declared CMake runtime DLLs beside a built Windows executable."""

import argparse
import filecmp
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    args = parser.parse_args()
    lines = args.config.read_text(encoding="utf-8").splitlines()
    destination = Path(lines[0]).parent
    for value in lines[1:]:
        for name in value.split(";"):
            if not name:
                continue
            source = Path(name)
            target = destination / source.name
            if source.resolve() != target.resolve() and (
                    not target.is_file() or not filecmp.cmp(source, target, shallow=False)):
                shutil.copy2(source, target)


if __name__ == "__main__":
    main()
