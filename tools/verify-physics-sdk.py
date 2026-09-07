"""Reject Box2D SDKs that do not record this project's validated vcpkg patch."""

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    args = parser.parse_args()
    manifest = args.sdk / "share/box2d/vcpkg.spdx.json"
    if not manifest.is_file():
        raise SystemExit("Box2D SDK missing: run python tools/prepare-physics.py")
    files = {item["fileName"]: item for item in json.loads(manifest.read_text(encoding="utf-8"))["files"]}
    for name in ("portfile.cmake", "vcpkg.json", "libm.diff", "rounded-distance.patch"):
        digest = hashlib.sha256((ROOT / "dependencies/vcpkg/ports/box2d" / name).read_bytes()).hexdigest()
        checksums = files.get("./" + name, {}).get("checksums", [])
        if not any(item["algorithm"] == "SHA256" and item["checksumValue"] == digest for item in checksums):
            raise SystemExit("Box2D SDK patch mismatch: run python tools/prepare-physics.py")
    print("Box2D SDK matches the validated vcpkg overlay")


if __name__ == "__main__":
    main()
