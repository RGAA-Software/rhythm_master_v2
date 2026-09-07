"""Prepare missing glTF parser headers through the existing vcpkg checkout."""

import os
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", default="D:/android/sdk/ndk/29.0.14206865")
    args = parser.parse_args()
    packages = ["cgltf:" + triplet for triplet in ("x64-windows", "arm64-android")
                if not (Path("C:/source/vcpkg/installed") / triplet / "include/cgltf.h").is_file()]
    if not packages:
        return
    environment = dict(os.environ, VCPKG_MAX_CONCURRENCY="20",
                       ANDROID_NDK_HOME=args.ndk)
    subprocess.run(["C:/source/vcpkg/vcpkg.exe", "install", *packages, "--classic",
                    "--x-install-root=" + str(ROOT / "out/vcpkg-scene"),
                    "--x-buildtrees-root=" + str(ROOT / "out/vcpkg-scene-buildtrees"),
                    "--x-packages-root=" + str(ROOT / "out/vcpkg-scene-packages")],
                   check=True, env=environment)


if __name__ == "__main__":
    main()
