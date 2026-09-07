"""Retain matching LGPL FFmpeg sources, vcpkg recipe and measured binary configuration."""

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=ROOT / "out/vcpkg-media-lgpl/x64-windows")
    parser.add_argument("--source", type=Path, default=ROOT / "out/vcpkg-media-buildtrees/ffmpeg/src/n6.1.1-95b141da41.clean")
    parser.add_argument("--port", type=Path, default=ROOT / "out/vcpkg-media-buildtrees/versioning_/versions/ffmpeg/15b90b33b76e69c2d9b876b32c4c9b47c97846ed")
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    profiles = {}
    for configuration, directory in (("Release", sdk / "bin"), ("Debug", sdk / "debug/bin")):
        with os.add_dll_directory(str(directory)):
            library = ctypes.CDLL(str(directory / "avcodec-60.dll"))
            library.avcodec_license.restype = ctypes.c_char_p
            library.avcodec_configuration.restype = ctypes.c_char_p
            license_name = library.avcodec_license().decode("utf-8")
            flags = library.avcodec_configuration().decode("utf-8")
            if license_name != "LGPL version 2.1 or later" or any(
                    value in flags for value in ("--enable-gpl", "--enable-nonfree", "--enable-libx264")):
                raise ValueError("The application profile requires the validated LGPL configuration")
            if "--enable-zlib" not in flags:
                raise ValueError("PNG support requires the validated zlib feature")
        profiles[configuration] = {"license": license_name, "configuration": flags,
                                   "dll_sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                                                  for path in sorted(directory.glob("*.dll"))}}
    if not (args.source / "COPYING.LGPLv2.1").is_file() or not (args.port / "portfile.cmake").is_file():
        raise ValueError("Matching vcpkg source tree and recipe are required")
    output = ROOT / "out/release-sources/ffmpeg-vcpkg.zip"
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for prefix, tree in (("ffmpeg", args.source), ("vcpkg-port", args.port)):
            for path in sorted(tree.rglob("*")):
                if path.is_file() and ".git" not in path.parts:
                    info = zipfile.ZipInfo(prefix + "/" + path.relative_to(tree).as_posix())
                    info.compress_type = zipfile.ZIP_DEFLATED
                    archive.writestr(info, path.read_bytes())
        archive.write(ROOT / "probes/media/vcpkg.json", "vcpkg.json")
        archive.writestr("binary-profiles.json", json.dumps(profiles, indent=4))
        archive.writestr("BUILD.txt", "Sources include the exact patches applied by the retained vcpkg port.\n"
                         "Use vcpkg baseline b216ddff25a1f432870e6c340ce79357049ef86e and the enclosed manifest.\n"
                         "Set VCPKG_MAX_CONCURRENCY=20, then vcpkg install --triplet=x64-windows --x-manifest-root=<manifest-directory>.\n"
                         "Compiler: MSVC 14.51.36231; see binary-profiles.json for actual configurations.\n")
    record = {"status": "Windows local application profile validated", "source_url": "https://github.com/FFmpeg/FFmpeg/tree/n6.1.1",
              "revision": "n6.1.1", "vcpkg_port_tree": args.port.name,
              "license": "LGPL-2.1-or-later", "outbound_project_license": "not selected",
              "linkage": "replaceable Windows DLLs; no FFmpeg or x264 static objects in the application",
              "source_archive_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
              "manifest": json.loads((ROOT / "probes/media/vcpkg.json").read_text(encoding="utf-8")),
              "profiles": profiles, "target_modules": ["src/media", "src/audio_playback", "src/audio_ui"],
              "evidence": ["out/media-lgpl-video-tests.log", "out/media-lgpl-audio-tests.log", "out/media-lgpl-playback-tests.log"],
              "distribution_materials": "Application deploy/notices/ffmpeg includes matching source/build ZIP, profile and license notices.",
              "scope": "Local validation only; public release/signing/store gates remain separate."}
    (ROOT / "provenance/media_lgpl_windows.json").write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")
    print(output, output.stat().st_size)


if __name__ == "__main__":
    main()
