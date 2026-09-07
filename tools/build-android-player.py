"""Incrementally build native Player and assemble a signed local acceptance APK."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
import android_media


ROOT = Path(__file__).resolve().parents[1]


def run(arguments, **kwargs):
    print("Running", Path(str(arguments[0])).name, flush=True)
    result = subprocess.run([str(arg) for arg in arguments], cwd=ROOT, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0, **kwargs)
    print(result.stdout, end="", flush=True)
    result.check_returncode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=Path("D:/android/sdk"))
    parser.add_argument("--ndk", default="29.0.14206865")
    parser.add_argument("--java", type=Path, default=Path("C:/Program Files/Android/Android Studio/jbr"))
    parser.add_argument("--target-sdk", type=Path, default=Path("C:/source/vcpkg/installed/arm64-android"))
    parser.add_argument("--protoc", type=Path, default=Path("C:/source/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe"))
    parser.add_argument("--package", type=Path, default=ROOT / "out/windows-release/content/packages/resonance_gate.rhythmpack")
    parser.add_argument("--media-sdk", type=Path, default=ROOT / "out/vcpkg-media-android/arm64-android")
    parser.add_argument("--skip-native", action="store_true")
    parser.add_argument("--build", type=Path)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    args = parser.parse_args()
    build = (args.build or ROOT / ("out/android-arm64" if args.configuration == "Debug"
                                  else "out/android-arm64-release")).resolve()
    ndk = args.sdk / "ndk" / args.ndk
    if not args.skip_native:
        run(["cmake", "-S", ROOT, "-B", build, "-G", "Ninja",
             f"-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake",
             "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-26", "-DCMAKE_BUILD_TYPE=" + args.configuration,
             "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", "-DRHYTHM_BUILD_ANDROID_PLAYER=ON",
             "-DRHYTHM_BUILD_PROJECT_IO=ON", f"-DRHYTHM_IO_SDK={args.target_sdk}",
             "-DRHYTHM_BUILD_MEDIA=ON", f"-DRHYTHM_MEDIA_SDK={args.media_sdk}",
             f"-DRHYTHM_PLAYER_PACKAGE={args.package}",
             f"-DRHYTHM_PROTOC={args.protoc}", f"-DCMAKE_FIND_ROOT_PATH={args.target_sdk}"])
        run(["cmake", "--build", build, "--target", "rhythm_android", "--parallel", "20"])
    source = ROOT / "platforms/android"
    output = build / "apk"
    output.mkdir(parents=True, exist_ok=True)
    if not android_media.enabled(build):
        raise ValueError("Android music controls require RHYTHM_BUILD_MEDIA=ON with the validated vcpkg SDK")
    originals = [build / "src/android_player/libmain.so", build / "deps/sdl/libSDL3.so"]
    native_directory = output / "native"
    native_directory.mkdir(exist_ok=True)
    strip = ndk / "toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe"
    natives = []
    for original in originals:
        packaged = native_directory / original.name
        native_stamp = packaged.with_suffix(".sha256")
        digest = hashlib.sha256(original.read_bytes()).hexdigest()
        if not packaged.exists() or not native_stamp.exists() or native_stamp.read_text(encoding="utf-8") != digest:
            shutil.copy2(original, packaged)
            run([strip, "--strip-unneeded", packaged])
            native_stamp.write_text(digest, encoding="utf-8")
        natives.append(packaged)
    media = android_media.prepare(build, ndk, natives[0])
    demo = ROOT / "out/windows-release/content/audio/resonance_demo.wav"
    java = args.java / "bin"
    android_jar = args.sdk / "platforms/android-35/android.jar"
    build_tools = args.sdk / "build-tools/36.1.0"
    inputs = sorted(source.rglob("*.java")) + sorted(source.rglob("*.xml")) + natives + [args.package]
    inputs += sorted((ROOT / "third_party/sources/sdl/android-project/app/src/main/java").rglob("*.java"))
    inputs += [Path(__file__), ROOT / "tools/verify-android-apk.py", ROOT / "third_party/README.md"]
    inputs += [ROOT / "tools/android_media.py", ROOT / "tools/relink-android-player.py",
               media / "profile.json", media / "COPYING.LGPLv2.1", demo]
    inputs += sorted(path for path in (ROOT / "third_party/notices").rglob("*") if path.is_file())
    fingerprint = hashlib.sha256()
    for path in inputs:
        fingerprint.update(str(path).encode())
        fingerprint.update(path.read_bytes())
    signature = fingerprint.hexdigest()
    stamp = output / "inputs.sha256"
    apk = output / ("rhythm-player-" + args.configuration.lower() + ".apk")
    if apk.exists() and stamp.exists() and stamp.read_text(encoding="utf-8") == signature:
        print(f"APK unchanged: {apk}")
        return
    assets = output / "assets"
    assets.mkdir(exist_ok=True)
    shutil.copy2(args.package, assets / "signal_texture.rhythmpack")
    shutil.copy2(demo, assets / "resonance_demo.wav")
    shutil.copytree(ROOT / "third_party/notices", assets / "notices", dirs_exist_ok=True)
    shutil.copy2(ROOT / "third_party/README.md", assets / "THIRD_PARTY_NOTICES.md")
    media_notices = assets / "notices/ffmpeg"
    media_notices.mkdir(parents=True, exist_ok=True)
    for name in ("profile.json", "COPYING.LGPLv2.1"):
        shutil.copy2(media / name, media_notices / name)
    (media_notices / "RELINK.txt").write_text(
        "This local acceptance APK uses FFmpeg LGPL-2.1-or-later.\n"
        "Matching source, vcpkg build recipe and tested replacement/relink materials\n"
        "are in the companion rhythm-player-" + args.configuration.lower() + "-relink.zip.\n"
        "No additional restriction on modifying FFmpeg or debugging those changes is imposed.\n"
        "Project outbound license/public release remains undecided.\n", encoding="utf-8")
    generated = output / "generated"
    generated.mkdir(exist_ok=True)
    run([build_tools / "aapt2.exe", "compile", "--dir", source / "res", "-o", output / "resources.zip"])
    run([build_tools / "aapt2.exe", "link", "-o", output / "base.apk", "--manifest", source / "AndroidManifest.xml",
         "-I", android_jar, "--java", generated, "-A", assets, output / "resources.zip"])
    classes = output / "classes"
    classes.mkdir(exist_ok=True)
    sources = [path for path in inputs if path.suffix == ".java"] + list(generated.rglob("*.java"))
    source_list = output / "java-sources.txt"
    source_list.write_text("\n".join('"' + path.as_posix() + '"' for path in sources), encoding="utf-8")
    run([java / "javac.exe", "-encoding", "UTF-8", "-source", "8", "-target", "8", "-Xlint:-options",
         "-classpath", android_jar, "-d", classes, "@" + str(source_list)])
    run([java / "jar.exe", "--create", "--file", output / "classes.jar", "-C", classes, "."])
    dex = output / "dex"
    dex.mkdir(exist_ok=True)
    run([java / "java.exe", "-cp", build_tools / "lib/d8.jar", "com.android.tools.r8.D8", "--min-api", "26",
         "--lib", android_jar, "--output", dex, output / "classes.jar"])
    unsigned = output / "unsigned.apk"
    shutil.copy2(output / "base.apk", unsigned)
    with zipfile.ZipFile(unsigned, "a") as archive:
        for path in dex.glob("*.dex"):
            archive.write(path, path.name, compress_type=zipfile.ZIP_DEFLATED)
        for path in natives:
            archive.write(path, "lib/arm64-v8a/" + path.name, compress_type=zipfile.ZIP_STORED)
    aligned = output / "aligned.apk"
    run([build_tools / "zipalign.exe", "-P", "16", "-f", "4", unsigned, aligned])
    # Both configurations use the existing local acceptance identity, allowing
    # ordinary updates without uninstalling the user's imported projects.
    keystore = ROOT / "out/android-arm64/apk/local-debug.keystore"
    keystore.parent.mkdir(parents=True, exist_ok=True)
    if not keystore.exists():
        run([java / "keytool.exe", "-genkeypair", "-keystore", keystore, "-storepass", "android",
             "-keypass", "android", "-alias", "androiddebugkey", "-dname", "CN=Local Android Debug,O=Rhythm Master,C=CN",
             "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000"])
    run([java / "java.exe", "-jar", build_tools / "lib/apksigner.jar", "sign", "--ks", keystore,
         "--ks-pass", "pass:android", "--key-pass", "pass:android", "--out", apk, aligned])
    run([java / "java.exe", "-jar", build_tools / "lib/apksigner.jar", "verify", "--verbose", apk])
    run([build_tools / "zipalign.exe", "-c", "-P", "16", "4", apk])
    run([sys.executable, ROOT / "tools/verify-android-apk.py", apk, "--report", output / "verification.json"])
    stamp.write_text(signature, encoding="utf-8")
    print(f"Built local acceptance APK: {apk}")


if __name__ == "__main__":
    main()
