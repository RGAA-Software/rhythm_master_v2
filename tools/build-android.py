"""Incremental Android builds with separate Debug/Release caches and 20 workers."""

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--ndk", type=Path, default=Path("D:/android/sdk/ndk/29.0.14206865"))
    parser.add_argument("--sdk", type=Path, default=Path("C:/source/vcpkg/installed/arm64-android"))
    parser.add_argument("--build", type=Path)
    parser.add_argument("--target", action="append", default=[])
    parser.add_argument("--jobs", type=int, default=20)
    parser.add_argument("--gles-version", choices=("30", "31"),
                        help="Explicit backend feature-level experiment; otherwise preserve the configured cache")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error("jobs must be 1..64")
    build = args.build or ROOT / ("out/android-arm64" if args.configuration == "Debug" else "out/android-arm64-release")
    options = ["-DCMAKE_TOOLCHAIN_FILE=" + (args.ndk / "build/cmake/android.toolchain.cmake").as_posix(),
               "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-26",
               "-DCMAKE_BUILD_TYPE=" + args.configuration, "-DRHYTHM_BUILD_PROJECT_IO=ON",
               "-DRHYTHM_BUILD_ANDROID_PLAYER=ON", "-DRHYTHM_IO_SDK=" + args.sdk.as_posix(),
               "-DRHYTHM_BUILD_MEDIA=ON",
               "-DRHYTHM_MEDIA_SDK=" + (ROOT / "out/vcpkg-media-android/arm64-android").as_posix(),
               "-DRHYTHM_PLAYER_PACKAGE=" + (ROOT / "out/windows-release/content/packages/resonance_gate.rhythmpack").as_posix(),
               "-DCMAKE_FIND_ROOT_PATH=" + args.sdk.as_posix(),
               "-DRHYTHM_PROTOC=C:/source/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe"]
    if args.gles_version:
        options.append("-DRHYTHM_ANDROID_GLES_VERSION=" + args.gles_version)

    # Reuse only the previously validated shader tool path, not compiler flags or
    # a copied cache that would silently carry Debug performance settings.
    cache = ROOT / "out/android-arm64/CMakeCache.txt"
    if cache.is_file():
        for line in cache.read_text(encoding="utf-8").splitlines():
            if line.startswith("RHYTHM_SHADERC:FILEPATH="):
                options.append("-DRHYTHM_SHADERC=" + line.split("=", 1)[1])
    subprocess.run(["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja", *options], check=True)
    targets = args.target or ["android_player_apk"]
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs), "--target", *targets], check=True)


if __name__ == "__main__":
    main()
