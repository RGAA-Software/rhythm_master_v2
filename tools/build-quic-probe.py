"""Build isolated QUIC candidates, retaining incremental caches and source identities."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess


ROOT = Path(__file__).resolve().parents[1]
MSQUIC_REVISION = "a01333cf7c2659cce0ff03ef3f21e1ff15bb5b83"
QUICHE_REVISION = "55886df3be579579207104c8e645825b6347a209"
OPENSSL_REVISION = "f4dc4d58b48d346a8270183f89acf826d459b0ca"


def run(args, **kwargs):
    print("Running:", subprocess.list2cmdline([str(arg) for arg in args]), flush=True)
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def msys_path(path):
    path = Path(path).resolve().as_posix()
    return "/" + path[0].lower() + path[2:]


def check_revision(source, expected):
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if actual != expected:
        raise SystemExit(f"Unexpected candidate revision: {source.name}")


def openssl_android(source, args):
    """Upstream's Android superbuild assumes a Unix host; use an isolated MSYS build."""
    suffix = "-shared" if args.shared_tls else ""
    build = ROOT / ("out/quic/openssl-3.5.8-android" + suffix)
    install = ROOT / ("out/quic/openssl-3.5.8-android" + suffix + "-install")
    build.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    ndk = msys_path(args.ndk)
    flags = ["android-arm64", "-U__ANDROID_API__", f"-D__ANDROID_API__={args.android_api}",
             "shared" if args.shared_tls else "no-shared", "no-tests",
             "no-apps", "no-docs", "no-module", "no-legacy", "no-engine",
             "--prefix=" + msys_path(install), "--libdir=lib"]
    if args.shared_tls:
        flags.append("-Wl,-z,max-page-size=16384")
    env["ANDROID_NDK_ROOT"] = ndk
    env["MSYS2_ARG_CONV_EXCL"] = "*"
    env["MSYS_NO_PATHCONV"] = "1"
    # This is compiler orchestration only; no shell deletion/moving or user payloads.
    commands = ["set -eu", "export PATH=" + shlex.quote(ndk + "/toolchains/llvm/prebuilt/windows-x86_64/bin:/usr/bin"),
                "cd " + shlex.quote(msys_path(build))]
    fingerprint = hashlib.sha256(json.dumps([OPENSSL_REVISION, str(args.ndk), flags]).encode()).hexdigest()
    stamp = build / "rhythm-configure.sha256"
    if not stamp.exists() or stamp.read_text() != fingerprint or not (build / "Makefile").exists():
        commands.append("perl " + shlex.join([msys_path(source / "Configure"), *flags]))
    commands.append(f"make -j{args.jobs} install_sw")
    run([args.msys / "usr/bin/bash.exe", "-c", "\n".join(commands)], env=env)
    stamp.write_text(fingerprint)
    return install


def openssl_windows(source, args):
    suffix = "-shared" if args.shared_tls else ""
    build = ROOT / ("out/quic/openssl-3.5.8-windows" + suffix)
    install = ROOT / ("out/quic/openssl-3.5.8-windows" + suffix + "-install")
    build.mkdir(parents=True, exist_ok=True)
    flags = ["VC-WIN64A", "shared" if args.shared_tls else "no-shared", "no-tests", "no-apps", "no-docs", "no-module",
             "no-legacy", "no-engine", "--prefix=" + str(install), "--libdir=lib"]
    fingerprint = hashlib.sha256(json.dumps([OPENSSL_REVISION, flags]).encode()).hexdigest()
    stamp = build / "rhythm-configure.sha256"
    if not stamp.exists() or stamp.read_text() != fingerprint or not (build / "makefile").exists():
        run(["perl", source / "Configure", *flags], cwd=build)
    # Upstream's native Windows makefile uses nmake, which has no -j option.
    run(["nmake", "/nologo", "install_sw"], cwd=build)
    stamp.write_text(fingerprint)
    return install


def msquic(args):
    source = ROOT / "third_party/sources/msquic-probe"
    check_revision(source, MSQUIC_REVISION)
    tls = args.tls or ("schannel" if args.platform == "windows" else "openssl")
    suffix = "-openssl" if args.platform == "windows" and tls == "openssl" else ""
    if args.shared_tls:
        if tls != "openssl":
            raise SystemExit("--shared-tls requires the OpenSSL candidate")
        suffix += "-shared"
    build = ROOT / "out/quic" / ("msquic-" + args.platform + suffix)
    options = ["-DCMAKE_BUILD_TYPE=Release", "-DQUIC_BUILD_TOOLS=OFF", "-DQUIC_BUILD_TEST=OFF",
               "-DQUIC_ENABLE_LOGGING=OFF", "-DBUILD_SHARED_LIBS=ON"]
    if tls == "openssl":
        tls_source = ROOT / "third_party/sources/openssl-quic-3.5.8"
        check_revision(tls_source, OPENSSL_REVISION)
        install = (openssl_windows if args.platform == "windows" else openssl_android)(tls_source, args)
        extension = ".lib" if args.platform == "windows" else (".so" if args.shared_tls else ".a")
        options += ["-DQUIC_USE_EXTERNAL_OPENSSL=ON",
                    "-DQUIC_OPENSSL_INCLUDE_DIR=" + str(install / "include"),
                    "-DQUIC_OPENSSL_LIB_DIR=" + str(install / "lib"),
                    "-DLIB_CRYPTO:FILEPATH=" + str(install / ("lib/libcrypto" + extension)),
                    "-DLIB_SSL:FILEPATH=" + str(install / ("lib/libssl" + extension))]
    if args.platform == "windows":
        options += ["-DQUIC_TLS_LIB=" + tls, "-DQUIC_STATIC_LINK_CRT=OFF",
                    "-DQUIC_STATIC_LINK_PARTIAL_CRT=OFF",
                    "-DRHYTHM_SHARED_TLS_APPLICATION_DIR=" + ("ON" if args.shared_tls else "OFF")]
        for line in (ROOT / "out/windows/CMakeCache.txt").read_text(encoding="utf-8").splitlines():
            if line.startswith("RHYTHM_MSVC_INCLUDE_PREFIX:"):
                options.append("-DRHYTHM_MSVC_INCLUDE_PREFIX=" + line.split("=", 1)[1])
    else:
        options += ["-DQUIC_TLS_LIB=openssl",
                    "-DCMAKE_TOOLCHAIN_FILE=" + str(args.ndk / "build/cmake/android.toolchain.cmake"),
                    "-DANDROID_ABI=arm64-v8a", f"-DANDROID_PLATFORM={args.android_api}", "-DANDROID_STL=c++_static",
                    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"]
    run(["cmake", "-S", source, "-B", build, "-G", "Ninja", *options])
    run(["cmake", "--build", build, "--parallel", args.jobs])


def quiche(args):
    source = ROOT / "third_party/sources/quiche-probe"
    check_revision(source, QUICHE_REVISION)
    env = os.environ.copy()
    env["CARGO_TARGET_DIR"] = str(ROOT / "out/quic/quiche-target")
    env["CMAKE_GENERATOR"] = "Ninja"
    if args.platform == "android":
        target = "aarch64-linux-android"
        toolchain = args.ndk / "toolchains/llvm/prebuilt/windows-x86_64/bin"
        env["ANDROID_NDK_HOME"] = args.ndk.as_posix()
        env["ANDROID_NDK_ROOT"] = args.ndk.as_posix()
        # Match the NDK CMake toolchain's canonical compiler paths. Supplying its
        # .cmd wrappers makes boring-sys's second configure reset to the host OS.
        env["CC_aarch64_linux_android"] = (toolchain / "clang.exe").as_posix()
        env["CXX_aarch64_linux_android"] = (toolchain / "clang++.exe").as_posix()
        env["CFLAGS_aarch64_linux_android"] = "--target=aarch64-linux-android26"
        env["CXXFLAGS_aarch64_linux_android"] = "--target=aarch64-linux-android26"
        env["AR_aarch64_linux_android"] = (toolchain / "llvm-ar.exe").as_posix()
        env["CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER"] = env["CC_aarch64_linux_android"]
        env["CARGO_TARGET_AARCH64_LINUX_ANDROID_RUSTFLAGS"] = "-C link-arg=--target=aarch64-linux-android26"
        # The failed compiler-path configuration can leave CMake identifying the
        # Android target as Windows. Discard only that diagnosed configuration,
        # retaining Cargo caches, source dependencies and every built object.
        android_build = ROOT / "out/quic/quiche-target/aarch64-linux-android"
        for system in android_build.rglob("CMakeSystem.cmake"):
            if 'set(CMAKE_SYSTEM_NAME "Windows")' in system.read_text():
                targets = [system, system.parents[2] / "CMakeCache.txt"]
                targets += list(system.parent.glob("CMake*Compiler.cmake"))
                for path in targets:
                    if not path.resolve().is_relative_to(android_build.resolve()):
                        raise SystemExit("Unexpected candidate configuration path")
                    path.unlink(missing_ok=True)
        # Repair only malformed compiler-path entries left by the diagnosed first
        # cross-build. Keep every cache/object directory; future inputs use '/' above.
        for name in ("CMakeCCompiler.cmake", "CMakeCXXCompiler.cmake"):
            for path in (ROOT / "out/quic/quiche-target/aarch64-linux-android").rglob(name):
                text = path.read_text()
                fixed = re.sub(r'(set\(CMAKE_C(?:XX)?_COMPILER ")([^"\n]+)("\))',
                               lambda match: match[1] + match[2].replace("\\", "/") + match[3], text)
                if fixed != text:
                    path.write_text(fixed)
    else:
        target = "x86_64-pc-windows-msvc"
    run(["cargo", "build", "--locked", "--manifest-path", source / "Cargo.toml", "-p", "quiche",
         "--release", "--features", "ffi", "--target", target, "--jobs", args.jobs], env=env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", choices=("msquic", "quiche"))
    parser.add_argument("platform", choices=("windows", "android"))
    parser.add_argument("--ndk", type=Path, default=Path("D:/android/sdk/ndk/29.0.14206865"))
    parser.add_argument("--msys", type=Path, default=Path("C:/msys64"))
    parser.add_argument("--android-api", type=int, default=26)
    parser.add_argument("--tls", choices=("schannel", "openssl"))
    parser.add_argument("--shared-tls", action="store_true", help="Isolated shared provider build, preserving static candidate caches")
    parser.add_argument("--jobs", type=int, default=20)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error("jobs must be 1..64")
    if args.shared_tls and args.candidate != "msquic":
        parser.error("shared provider experiment applies only to MsQuic")
    os.environ["PATH"] = str(ROOT / "out/quic/nasm") + os.pathsep + os.environ["PATH"]
    (msquic if args.candidate == "msquic" else quiche)(args)


if __name__ == "__main__":
    main()
