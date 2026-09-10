"""Bundle the existing authored effects and their thumbnails for offline selection."""

import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import zipfile
import zlib
import content_identity


ROOT = Path(__file__).resolve().parents[1]
FEATURED = ("aureate_vortex", "aurora_braid", "chromatic_loom", "crystal_choir", "harmonic_city",
            "lumen_corridor", "orbital_reliquary", "phase_loom", "porcelain_bloom",
            "prismatic_lotus", "resonance_gate", "resonance_live", "resonant_arcade",
            "sonic_enamel", "spectral_foundry", "spectral_nebula", "stellar_currents",
            "stratified_ink", "torque_garden")


def sources(package_directory):
    result = []
    for manifest in sorted((ROOT / "content/templates").glob("*/manifest.json")):
        package = package_directory / (manifest.parent.name + ".rhythmpack")
        if not package.is_file():
            raise ValueError(f"Build runtime_builtin_content before Android packaging: {package}")
        content_identity.verify_package_source(manifest.parent, package)
        result.extend((manifest, package, package.with_suffix('.source.json')))
        thumbnail = manifest.parent / "thumbnail.rgba"
        if thumbnail.is_file():
            result.append(thumbnail)
    return result


def png(rgba):
    if len(rgba) != 256 * 144 * 4:
        raise ValueError("Expected authored 256x144 RGBA thumbnail")
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    pixels = b"".join(b"\0" + rgba[y * 1024:(y + 1) * 1024] for y in range(144))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 256, 144, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))


def prepare(package_directory, assets):
    directory = assets / "effects"
    directory.mkdir(exist_ok=True)
    if directory.is_symlink() or directory.resolve().parent != assets.resolve():
        raise ValueError("Invalid effect staging directory")
    entries = []
    expected = {"catalog.json"}
    for manifest_path in sources(package_directory):
        if manifest_path.name != "manifest.json":
            continue
        effect_id = manifest_path.parent.name
        if not re.fullmatch(r"[a-z0-9_]+", effect_id):
            raise ValueError("Invalid effect ID")
        authored = json.loads(manifest_path.read_text(encoding="utf-8"))
        package = package_directory / (effect_id + ".rhythmpack")
        with zipfile.ZipFile(package) as archive:
            runtime = json.loads(archive.read("manifest.json"))
        entry = {"id": effect_id, "titles": authored["titles"], "canvas": runtime["canvas"],
                 "content_id": authored["content_id"], "content_version": authored["content_version"],
                 "source_sha256": content_identity.verify_package_source(manifest_path.parent, package),
                 "tier": authored.get("tier", "example"),
                 "descriptions": authored.get("descriptions", {"zh-CN": "", "en-US": ""}),
                 "audio": any(op.startswith("audio.") or op in ("texture.spectrum", "scene.point_instances")
                              for op in runtime["operators"]),
                 "package": "effects/" + package.name,
                 "sha256": hashlib.sha256(package.read_bytes()).hexdigest()}
        shutil.copy2(package, directory / package.name)
        expected.add(package.name)
        thumbnail = manifest_path.parent / "thumbnail.rgba"
        if thumbnail.is_file():
            name = effect_id + ".png"
            data = png(thumbnail.read_bytes())
            (directory / name).write_bytes(data)
            entry["thumbnail"] = "effects/" + name
            entry["thumbnail_sha256"] = hashlib.sha256(data).hexdigest()
            expected.add(name)
        entries.append(entry)
    entries.sort(key=lambda entry: (FEATURED.index(entry["id"]) if entry["id"] in FEATURED
                                   else len(FEATURED), entry["id"]))
    (directory / "catalog.json").write_text(json.dumps(entries, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    # Only generated files in this exact, dedicated staging directory are owned here.
    for path in directory.iterdir():
        if path.name not in expected:
            if path.is_symlink() or not path.is_file() or path.resolve().parent != directory.resolve():
                raise ValueError(f"Unexpected effect staging entry: {path}")
            path.unlink()
