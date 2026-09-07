"""Build functional/flow/LAN tests against an already built pinned MsQuic candidate."""

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("windows", "android"))
    parser.add_argument("--shared-tls", action="store_true")
    parser.add_argument("--ndk", type=Path, default=Path("D:/android/sdk/ndk/29.0.14206865"))
    parser.add_argument("--jobs", type=int, default=20)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error("jobs must be 1..64")
    suffix = ("-openssl" if args.platform == "windows" else "") + ("-shared" if args.shared_tls else "")
    candidate = ROOT / ("out/quic/msquic-" + args.platform + suffix)
    build = ROOT / ("out/quic/probe-" + args.platform + suffix)
    options = ["-DCMAKE_BUILD_TYPE=Release", "-DQUIC_PROBE_OPENSSL=ON",
               "-DMSQUIC_SOURCE=" + (ROOT / "third_party/sources/msquic-probe").as_posix()]
    if args.platform == "windows":
        library = candidate / "obj/Release/msquic.lib"
        options += ["-DMSQUIC_RUNTIME=" + (candidate / "bin/Release/msquic.dll").as_posix()]
        prefix = next((line.split("=", 1)[1] for line in (ROOT / "out/windows/CMakeCache.txt").read_text(encoding="utf-8").splitlines()
                       if line.startswith("RHYTHM_MSVC_INCLUDE_PREFIX:")), None)
        if not prefix:
            raise SystemExit("Initialize the MSVC environment and measured include-prefix first")
        options += ["-DRHYTHM_MSVC_INCLUDE_PREFIX=" + prefix]
        if args.shared_tls:
            options += ["-DQUIC_PROBE_TLS_ROOT=" + (ROOT / "out/quic/openssl-3.5.8-windows-shared-install").as_posix()]
    else:
        library = candidate / "bin/Release/libmsquic.so"
        options += ["-DCMAKE_TOOLCHAIN_FILE=" + (args.ndk / "build/cmake/android.toolchain.cmake").as_posix(),
                    "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=26", "-DANDROID_STL=c++_static"]
    if not library.is_file():
        raise SystemExit("Build the matching MsQuic candidate first")
    options += ["-DMSQUIC_LIBRARY=" + library.as_posix()]
    subprocess.run(["cmake", "-S", str(ROOT / "probes/quic"), "-B", str(build), "-G", "Ninja", *options], check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True)


if __name__ == "__main__":
    main()
