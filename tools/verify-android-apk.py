"""Verify packaged native dependencies, 16 KiB compatibility and Player isolation."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import re
import zipfile


SYSTEM_LIBRARIES = {"libandroid.so", "liblog.so", "libEGL.so", "libGLESv1_CM.so",
                    "libGLESv2.so", "libGLESv3.so", "libOpenSLES.so", "libm.so",
                    "libdl.so", "libc.so", "libz.so", "libvulkan.so", "libaaudio.so"}


def inspect_elf(data):
    if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 18)[0] != 183:
        raise ValueError("Expected little-endian AArch64 ELF64")
    program_offset = struct.unpack_from("<Q", data, 32)[0]
    entry_size, count = struct.unpack_from("<HH", data, 54)
    segments = [struct.unpack_from("<IIQQQQQQ", data, program_offset + i * entry_size) for i in range(count)]
    loads = [segment for segment in segments if segment[0] == 1]
    if not loads or any(segment[7] < 16384 or (segment[2] - segment[3]) % 16384 for segment in loads):
        raise ValueError("ELF load segments do not support 16 KiB pages")
    dynamic = next(segment for segment in segments if segment[0] == 2)
    entries = [struct.unpack_from("<qQ", data, offset) for offset in range(dynamic[2], dynamic[2] + dynamic[5], 16)]
    address = next(value for tag, value in entries if tag == 5)
    segment = next(segment for segment in loads if segment[3] <= address < segment[3] + segment[5])
    strings = address - segment[3] + segment[2]
    dependencies = []
    for tag, value in entries:
        if tag == 1:
            start = strings + value
            dependencies.append(data[start:data.index(b"\0", start)].decode("ascii"))
    return dependencies


def verify(apk):
    with zipfile.ZipFile(apk) as archive:
        if archive.testzip() is not None:
            raise ValueError("APK CRC failure")
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Duplicate APK entries")
        libraries = {Path(name).name: name for name in names if name.startswith("lib/") and name.endswith(".so")}
        if set(libraries) != {"libmain.so", "libSDL3.so"}:
            raise ValueError(f"Unexpected native library set: {libraries}")
        dependencies = {}
        with apk.open("rb") as raw:
            for name, entry in libraries.items():
                info = archive.getinfo(entry)
                raw.seek(info.header_offset)
                header = raw.read(30)
                filename_length, extra_length = struct.unpack_from("<HH", header, 26)
                offset = info.header_offset + 30 + filename_length + extra_length
                if info.compress_type != zipfile.ZIP_STORED or offset % 16384:
                    raise ValueError(f"Native library must be uncompressed and 16 KiB aligned: {name}")
                data = archive.read(entry)
                dependencies[name] = inspect_elf(data)
                unknown = set(dependencies[name]) - SYSTEM_LIBRARIES - libraries.keys()
                if unknown:
                    raise ValueError(f"Missing runtime libraries for {name}: {unknown}")
                for forbidden in (b"QApplication", b"QWidget", b"ImGui::", b"imgui_node_editor", b"rhythm::editor::", b"rhythm::studio::"):
                    if forbidden in data:
                        raise ValueError(f"Studio/backend boundary leaked into {name}: {forbidden}")
        package = archive.read("assets/signal_texture.rhythmpack")
        with zipfile.ZipFile(io.BytesIO(package)) as runtime_archive:
            runtime_manifest = json.loads(runtime_archive.read("manifest.json"))
        catalog = json.loads(archive.read("assets/effects/catalog.json"))
        if not 1 <= len(catalog) <= 256 or len({entry["id"] for entry in catalog}) != len(catalog):
            raise ValueError("Invalid built-in effect catalog")
        expected_effects = {"assets/effects/catalog.json"}
        for entry in catalog:
            if not re.fullmatch(r"[a-z0-9_]+", entry["id"]) or entry["package"] != "effects/" + entry["id"] + ".rhythmpack":
                raise ValueError("Invalid built-in effect asset path")
            asset = "assets/" + entry["package"]
            expected_effects.add(asset)
            data = archive.read(asset)
            if hashlib.sha256(data).hexdigest() != entry["sha256"]:
                raise ValueError("Built-in effect hash mismatch")
            with zipfile.ZipFile(io.BytesIO(data)) as effect:
                manifest = json.loads(effect.read("manifest.json"))
                if effect.testzip() or manifest["canvas"] != entry["canvas"]:
                    raise ValueError("Built-in effect canvas/CRC mismatch")
                if hashlib.sha256(effect.read("runtime/program.pb")).hexdigest() != manifest["program_sha256"]:
                    raise ValueError("Built-in program hash mismatch")
            if not all(entry["titles"].get(locale) for locale in ("zh-CN", "en-US")):
                raise ValueError("Missing built-in effect translations")
            if entry.get("tier") not in ("basic", "advanced", "example"):
                raise ValueError("Invalid built-in effect tier")
            if not all(isinstance(entry.get("descriptions", {}).get(locale), str)
                       for locale in ("zh-CN", "en-US")):
                raise ValueError("Missing built-in search descriptions")
            if "thumbnail" in entry:
                if entry["thumbnail"] != "effects/" + entry["id"] + ".png":
                    raise ValueError("Invalid thumbnail path")
                asset = "assets/" + entry["thumbnail"]
                expected_effects.add(asset)
                thumbnail = archive.read(asset)
                if hashlib.sha256(thumbnail).hexdigest() != entry["thumbnail_sha256"] or struct.unpack_from(
                        ">II", thumbnail, 16) != (256, 144):
                    raise ValueError("Built-in thumbnail mismatch")
        if {name for name in names if name.startswith("assets/effects/")} != expected_effects:
            raise ValueError("Stale or missing built-in effect assets")
        if not any(name.startswith("assets/notices/miniz/") for name in names):
            raise ValueError("Missing ZIP dependency notice")
        if "assets/resonance_demo.wav" not in names or "assets/notices/ffmpeg/profile.json" not in names:
            raise ValueError("Missing music fixture or FFmpeg provenance")
        media = json.loads(archive.read("assets/notices/ffmpeg/profile.json"))
        if media["license"] != "LGPL-2.1-or-later" or media["application_sha256"] != hashlib.sha256(
                archive.read(libraries["libmain.so"])).hexdigest():
            raise ValueError("FFmpeg profile does not match the packaged application")
    return {"apk": str(apk), "sha256": hashlib.sha256(apk.read_bytes()).hexdigest(),
            "package_sha256": hashlib.sha256(package).hexdigest(), "native_dependencies": dependencies,
            "native_alignment": 16384, "profile": "arm64-v8a GLES3", "runtime_profile": runtime_manifest["profile"],
            "runtime_abi": runtime_manifest["program_abi"],
            "media_license": media["license"],
            "builtin_effects": len(catalog),
            "device_acceptance": "Not established by archive verification"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    report = verify(args.apk)
    text = json.dumps(report, indent=4)
    if args.report:
        args.report.write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
