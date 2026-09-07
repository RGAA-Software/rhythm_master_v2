"""Incrementally build the pinned QR decoder candidate; Windows requires the MSVC environment."""

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("windows", "android"))
    parser.add_argument("--ndk", type=Path, default=Path("D:/android/sdk/ndk/29.0.14206865"))
    parser.add_argument("--jobs", type=int, default=20)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error("jobs must be 1..64")
    revision = subprocess.check_output(["git", "-C", str(ROOT / "third_party/sources/zxing-probe"),
                                        "rev-parse", "HEAD"], text=True).strip()
    if revision != "d6068bcebeb8fd9f0d35a99b00d202be86a14dbe":
        raise SystemExit("QR candidate differs from the validated source pin")
    options = ["-DCMAKE_BUILD_TYPE=Release"]
    if args.platform == "windows":
        cache = ROOT / "out/windows/CMakeCache.txt"
        prefix = next((line.split("=", 1)[1] for line in cache.read_text(encoding="utf-8").splitlines()
                       if line.startswith("RHYTHM_MSVC_INCLUDE_PREFIX:")), None)
        if not prefix:
            raise SystemExit("Run the Windows include-prefix/configure probe first")
        options += ["-DRHYTHM_MSVC_INCLUDE_PREFIX=" + prefix]
    else:
        options += ["-DCMAKE_TOOLCHAIN_FILE=" + (args.ndk / "build/cmake/android.toolchain.cmake").as_posix(),
                    "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=26", "-DANDROID_STL=c++_static"]
    build = ROOT / "out/qr-reader" / args.platform
    subprocess.run(["cmake", "-S", str(ROOT / "probes/qr_reader"), "-B", str(build), "-G", "Ninja", *options], check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True)


if __name__ == "__main__":
    main()
