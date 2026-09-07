"""Incrementally build the physics adapter against an installed vcpkg SDK."""

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("windows", "android"))
    parser.add_argument("--ndk", type=Path, default=Path("D:/android/sdk/ndk/29.0.14206865"))
    args = parser.parse_args()
    triplet = "x64-windows" if args.platform == "windows" else "arm64-android"
    # Stock 3.1.1 fails rounded-shape sensor overlap regression. Use the vcpkg
    # overlay SDK until another installed version passes the same validation.
    sdk = ROOT / "out/vcpkg-physics" / triplet
    subprocess.run(["python", str(ROOT / "tools/verify-physics-sdk.py"), "--sdk", str(sdk)], check=True)
    options = ["-DCMAKE_BUILD_TYPE=Debug", "-DRHYTHM_PHYSICS_SDK=" + sdk.as_posix()]
    if args.platform == "windows":
        cache = (ROOT / "out/windows/CMakeCache.txt").read_text(encoding="utf-8")
        prefix = next(line.split("=", 1)[1] for line in cache.splitlines()
                      if line.startswith("RHYTHM_MSVC_INCLUDE_PREFIX:"))
        options.append("-DRHYTHM_MSVC_INCLUDE_PREFIX=" + prefix)
    else:
        options += ["-DCMAKE_TOOLCHAIN_FILE=" + (args.ndk / "build/cmake/android.toolchain.cmake").as_posix(),
                    "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=26", "-DANDROID_STL=c++_static"]
    build = ROOT / "out/physics-validation" / args.platform
    subprocess.run(["cmake", "-S", str(ROOT / "probes/physics"), "-B", str(build), "-G", "Ninja", *options], check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", "20"], check=True)


if __name__ == "__main__":
    main()
