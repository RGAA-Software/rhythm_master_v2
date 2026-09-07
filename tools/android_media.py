"""Retain vcpkg FFmpeg notices and a relocatable, tested Android relink bundle."""

import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def cache_values(build):
    values = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if not line.startswith(("#", "//")) and ":" in line and "=" in line:
            key, value = line.split("=", 1)
            values[key.split(":", 1)[0]] = value
    return values


def enabled(build):
    return cache_values(build).get("RHYTHM_BUILD_MEDIA") == "ON"


def prepare(build, ndk, packaged_library):
    cache = cache_values(build)
    sdk = Path(cache["RHYTHM_MEDIA_SDK"])
    trees = ROOT / "out/vcpkg-media-android-buildtrees"
    configuration = (trees / "ffmpeg/arm64-android-rel/config.h").read_text(encoding="utf-8")
    license_name = re.search(r'#define FFMPEG_LICENSE "([^"]+)"', configuration).group(1)
    flags = re.search(r'#define FFMPEG_CONFIGURATION "([^"]+)"', configuration).group(1)
    if license_name != "LGPL version 2.1 or later" or any(
            flag in flags for flag in ("--enable-gpl", "--enable-nonfree")):
        raise ValueError("Android application requires the validated vcpkg LGPL profile")
    source = trees / "ffmpeg/src/n6.1.1-95b141da41.clean"
    port = trees / "versioning_/versions/ffmpeg/15b90b33b76e69c2d9b876b32c4c9b47c97846ed"
    if not (source / "COPYING.LGPLv2.1").is_file() or not (port / "portfile.cmake").is_file():
        raise ValueError("Matching Android vcpkg source and recipe are required")
    output = build / "apk/media"
    output.mkdir(parents=True, exist_ok=True)
    record = {
        "source_url": "https://github.com/FFmpeg/FFmpeg/tree/n6.1.1",
        "revision": "n6.1.1", "port_revision": port.name,
        "license": "LGPL-2.1-or-later", "configuration": flags,
        "linkage": "static libraries in libmain.so; companion relink bundle provided",
        "project_outbound_license": "not selected; local acceptance build",
        "sdk_archives": {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sorted((sdk / "lib").glob("*.a"))},
        "application_sha256": hashlib.sha256(packaged_library.read_bytes()).hexdigest(),
        "unstripped_application_sha256": hashlib.sha256((build / "src/android_player/libmain.so").read_bytes()).hexdigest(),
    }
    profile = output / "profile.json"
    profile_text = json.dumps(record, indent=4) + "\n"
    if not profile.exists() or profile.read_text(encoding="utf-8") != profile_text:
        profile.write_text(profile_text, encoding="utf-8")
    shutil.copy2(sdk / "share/ffmpeg/copyright", output / "COPYING.LGPLv2.1")
    source_archive = output / "ffmpeg-vcpkg-sources.zip"
    if not source_archive.exists():
        with zipfile.ZipFile(source_archive, "w", zipfile.ZIP_DEFLATED) as archive:
            for prefix, directory in (("ffmpeg", source), ("vcpkg-port", port)):
                for path in sorted(directory.rglob("*")):
                    if path.is_file() and ".git" not in path.parts:
                        archive.write(path, prefix + "/" + path.relative_to(directory).as_posix())
            archive.write(ROOT / "probes/media/vcpkg.json", "vcpkg.json")
            archive.writestr("android-configuration.txt", flags + "\n")
    relink(build, ndk, output)
    return output


def relink(build, ndk, media):
    ninja = (build / "build.ninja").read_text(encoding="utf-8")
    block = ninja.split("build src/android_player/libmain.so: ", 1)[1].split("\n\n", 1)[0]
    lines = block.splitlines()
    objects = re.sub(r"\$(.)", r"\1", lines[0].split(" | ", 1)[0]).split()[1:]
    properties = dict(line.strip().split(" = ", 1) for line in lines[1:] if " = " in line)
    libraries = shlex.split(properties["LINK_LIBRARIES"])
    flags = shlex.split(properties.get("LANGUAGE_COMPILE_FLAGS", ""))
    flags += shlex.split(properties.get("LINK_FLAGS", ""))
    kit = build / "apk/relink"
    kit.mkdir(parents=True, exist_ok=True)
    inputs = kit / "inputs"
    inputs.mkdir(exist_ok=True)
    arguments = ["-fPIC", *flags, "-shared", "-Wl,-soname,libmain.so"]
    sources = {}
    for value in [*objects, *libraries]:
        path = Path(value)
        if not path.is_absolute():
            path = build / path
        if path.is_file():
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            name = digest[:16] + "-" + path.name
            copied = inputs / name
            if not copied.exists():
                shutil.copy2(path, copied)
            relative = "inputs/" + name
            arguments.append(relative)
            sources[relative] = {"original": str(path), "sha256": digest}
        elif value.startswith("-"):
            arguments.append(value)
        else:
            raise ValueError("Missing relink input: " + value)
    (kit / "link.json").write_text(json.dumps(arguments, indent=4) + "\n", encoding="utf-8")
    (kit / "inputs.json").write_text(json.dumps(sources, indent=4) + "\n", encoding="utf-8")
    shutil.copy2(ROOT / "tools/relink-android-player.py", kit / "relink.py")
    shutil.copy2(media / "profile.json", kit / "media-profile.json")
    shutil.copy2(media / "ffmpeg-vcpkg-sources.zip", kit / "ffmpeg-vcpkg-sources.zip")
    shutil.copytree(ROOT / "third_party/notices", kit / "notices", dirs_exist_ok=True)
    shutil.copy2(media / "COPYING.LGPLv2.1", kit / "COPYING.LGPLv2.1")
    (kit / "README.txt").write_text(
        "Local Android acceptance relink materials, not a public release license selection.\n"
        "FFmpeg is LGPL-2.1-or-later. Other inputs retain their own copyright and notices.\n"
        "No additional restriction on modifying FFmpeg or reverse engineering for debugging\n"
        "such modifications is imposed by this acceptance bundle.\n"
        "Use Android NDK 29.0.14206865 and Python 3: python relink.py --ndk <NDK-directory>\n"
        "The script uses only the enclosed link inputs plus the NDK system/runtime libraries.\n"
        "Replace the FFmpeg archives identified in inputs.json to link modified versions.\n"
        "Exact patched FFmpeg source, vcpkg recipe and manifest are in ffmpeg-vcpkg-sources.zip.\n"
        "Build replacements with that vcpkg manifest, arm64-android, API 26, PIC enabled;\n"
        "see media-profile.json for the measured configuration. No project-owned FFmpeg build.\n"
        "To install a modified library, replace lib/arm64-v8a/libmain.so in an unsigned copy\n"
        "of the companion APK, zipalign -P 16 and sign it with your own local key.\n"
        "Use a separate app ID or preserve your original signing key for updates.\n",
        encoding="utf-8")
    fingerprint = hashlib.sha256((kit / "link.json").read_bytes() +
                                 (kit / "media-profile.json").read_bytes() +
                                 (kit / "relink.py").read_bytes()).hexdigest()
    stamp = kit / "verified.sha256"
    if not stamp.exists() or stamp.read_text(encoding="utf-8") != fingerprint:
        subprocess.run([sys.executable, str(kit / "relink.py"), "--ndk", str(ndk)], check=True)
        stamp.write_text(fingerprint, encoding="utf-8")
    archive_path = build / "apk" / ("rhythm-player-" + cache_values(build)["CMAKE_BUILD_TYPE"].lower() + "-relink.zip")
    archive_stamp = archive_path.with_suffix(".sha256")
    if not archive_path.exists() or not archive_stamp.exists() or archive_stamp.read_text(encoding="utf-8") != fingerprint:
        with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
            # Select current inputs only; older incremental input copies stay in
            # the cache but must not accumulate in the distributed bundle.
            selected = [kit / relative for relative in sources]
            selected += [path for path in kit.rglob("*") if path.is_file() and "inputs" not in path.relative_to(kit).parts
                         and path.name not in ("libmain.so", "link.rsp", "verified.sha256")]
            for path in sorted(selected):
                archive.write(path, path.relative_to(kit).as_posix())
        archive_stamp.write_text(fingerprint, encoding="utf-8")
