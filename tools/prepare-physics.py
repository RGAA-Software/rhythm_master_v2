"""Install the validated Box2D vcpkg overlay in this project's isolated SDK."""

import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpkg", type=Path, default=Path("C:/source/vcpkg"))
    parser.add_argument("--ndk", type=Path, default=Path("D:/android/sdk/ndk/29.0.14206865"))
    parser.add_argument("--platform", choices=("windows", "android", "all"), default="all")
    args = parser.parse_args()
    triplets = {"windows": ["x64-windows"], "android": ["arm64-android"],
                "all": ["x64-windows", "arm64-android"]}[args.platform]
    environment = dict(os.environ, ANDROID_NDK_HOME=str(args.ndk), VCPKG_MAX_CONCURRENCY="20")
    options = [
        "--classic", "--overlay-ports=" + str(ROOT / "dependencies/vcpkg/ports"),
        "--x-install-root=" + str(ROOT / "out/vcpkg-physics"),
        "--x-buildtrees-root=" + str(ROOT / "out/vcpkg-physics-buildtrees"),
        "--x-packages-root=" + str(ROOT / "out/vcpkg-physics-packages"),
    ]
    packages = ["box2d:" + item for item in triplets]
    subprocess.run([str(args.vcpkg / "vcpkg.exe"), "install", *packages, *options],
                   check=True, env=environment)
    # Classic install retains already-installed versions. Upgrade only this
    # explicit package list in the isolated root when the overlay revision changes.
    subprocess.run([str(args.vcpkg / "vcpkg.exe"), "upgrade", *packages,
                    "--no-dry-run", *options], check=True, env=environment)


if __name__ == "__main__":
    main()
