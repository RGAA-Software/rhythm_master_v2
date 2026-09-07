"""Incrementally build our media adapter against existing vcpkg libraries."""

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("windows", "android"))
    parser.add_argument("--vcpkg", type=Path, default=Path("C:/source/vcpkg"))
    parser.add_argument("--ndk", type=Path, default=Path("D:/android/sdk/ndk/29.0.14206865"))
    parser.add_argument("--jobs", type=int, default=20)
    parser.add_argument("--sdk", type=Path, help="Isolated installed vcpkg triplet override")
    parser.add_argument("--build", type=Path, help="Separate incremental validation directory")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error("jobs must be 1..64")
    triplet = "x64-windows" if args.platform == "windows" else "arm64-android"
    sdk = args.sdk or args.vcpkg / "installed" / triplet
    configuration = "Debug" if args.platform == "windows" else "Release"
    host_build = "windows" if args.platform == "windows" else "android-arm64"
    options = ["-DCMAKE_BUILD_TYPE=" + configuration, "-DRHYTHM_MEDIA_SDK=" + sdk.as_posix(),
               "-DRHYTHM_SDL_BUILD=" + (ROOT / "out" / host_build / "deps/sdl").as_posix()]
    if args.platform == "windows":
        cache = (ROOT / "out/windows/CMakeCache.txt").read_text(encoding="utf-8")
        prefix = next(line.split("=", 1)[1] for line in cache.splitlines()
                      if line.startswith("RHYTHM_MSVC_INCLUDE_PREFIX:"))
        options += ["-DRHYTHM_MSVC_INCLUDE_PREFIX=" + prefix]
    else:
        options += ["-DCMAKE_TOOLCHAIN_FILE=" + (args.ndk / "build/cmake/android.toolchain.cmake").as_posix(),
                    "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=26", "-DANDROID_STL=c++_static"]
    build = args.build or ROOT / "out/media-validation" / args.platform
    subprocess.run(["cmake", "-S", str(ROOT / "probes/media"), "-B", str(build), "-G", "Ninja", *options], check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True)


if __name__ == "__main__":
    main()
