"""Configure and incrementally build isolated room identity/admission validation."""

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
    suffix = "-shared" if args.shared_tls else ""
    tls_root = ROOT / f"out/quic/openssl-3.5.8-{args.platform}{suffix}-install"
    msquic = ROOT / ("out/quic/msquic-" + args.platform + ("-openssl" if args.platform == "windows" else "") + suffix)
    build = ROOT / ("out/security/" + args.platform + suffix)
    options = ["-DCMAKE_BUILD_TYPE=Release", "-DRHYTHM_SECURITY_OPENSSL_ROOT=" + tls_root.as_posix(),
               "-DRHYTHM_SECURITY_SHARED_TLS=" + ("ON" if args.shared_tls else "OFF"),
               "-DMSQUIC_SOURCE=" + (ROOT / "third_party/sources/msquic-probe").as_posix()]
    if args.platform == "windows":
        library = msquic / "obj/Release/msquic.lib"
        options += ["-DMSQUIC_RUNTIME=" + (msquic / "bin/Release/msquic.dll").as_posix()]
        cache = ROOT / "out/windows/CMakeCache.txt"
        prefix = next((line.split("=", 1)[1] for line in cache.read_text(encoding="utf-8").splitlines()
                       if line.startswith("RHYTHM_MSVC_INCLUDE_PREFIX:")), None)
        if not prefix:
            raise SystemExit("Run the Windows configure/include-prefix probe in an MSVC environment first")
        options += ["-DRHYTHM_MSVC_INCLUDE_PREFIX=" + prefix]
    else:
        library = msquic / "bin/Release/libmsquic.so"
        options += ["-DCMAKE_TOOLCHAIN_FILE=" + (args.ndk / "build/cmake/android.toolchain.cmake").as_posix(),
                    "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=26", "-DANDROID_STL=c++_static"]
    if not library.is_file() or not (tls_root / "include/openssl/opensslv.h").is_file():
        raise SystemExit("Build the matching pinned MsQuic/OpenSSL candidate before the identity probe")
    options += ["-DMSQUIC_LIBRARY=" + library.as_posix()]
    subprocess.run(["cmake", "-S", str(ROOT / "probes/security"), "-B", str(build), "-G", "Ninja", *options], check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True)


if __name__ == "__main__":
    main()
