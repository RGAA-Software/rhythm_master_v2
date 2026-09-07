"""Extract pinned QR sources from the authorized read-only GammaRay reference."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
REVISION = "ba64cf722578d8efdf85d5464d4db8ce14660cf4"
PREFIX = "src/px_deps/px_common/qrcode/"
HASHES = {
    "qr_generator.h": "035b0401fa50b229eff30750813f82797967ccbd2a001dd4d35fe4e19eacfcc6",
    "qr_generator.cpp": "5936408b649483fd92324730b3cb3aed262c242fd43737fd8bb0780a76431a72",
    "qrcodegen.hpp": "b779c3b156cf7a57ce789d6fee4fc991ccc2913774d26c909d22bb8f26b2a793",
    "qrcodegen.cpp": "1f3b3fcdac6954c32cf583ccd02ec9b5901f756a38c461acedc70be4a77d3757",
}


def write_changed(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gammaray", type=Path, default=Path("D:/GoCloud/GammaRayPremium"))
    args = parser.parse_args()
    contents = {}
    for name, expected in HASHES.items():
        data = subprocess.check_output(
            ["git", "-C", str(args.gammaray), "show", f"{REVISION}:{PREFIX}{name}"])
        if hashlib.sha256(data).hexdigest() != expected:
            raise SystemExit(f"Pinned QR source hash mismatch: {name}")
        contents[name] = data
    # Preserve upstream source bytes/style and the complete embedded MIT notice.
    for name in ("qrcodegen.hpp", "qrcodegen.cpp"):
        write_changed(ROOT / "third_party/sources/qrcodegen" / name, contents[name])
    notice = contents["qrcodegen.hpp"].split(b"*/", 1)[0] + b"*/\n"
    write_changed(ROOT / "third_party/notices/qrcodegen/LICENSE", notice)
    common = {
        "source_url": args.gammaray.resolve().as_uri(),
        "revision": REVISION,
        "byte_identity": "git objects, not mutable worktree; SHA-256 before writing",
    }
    third_party = {
        **common,
        "upstream_url": "https://github.com/nayuki/QR-Code-generator",
        "upstream_revision": "unknown; exact embedded source pinned by parent revision and hashes",
        "license": "MIT; Project Nayuki; complete notice retained in both sources and notices/qrcodegen/LICENSE",
        "files": {PREFIX + name: HASHES[name] for name in ("qrcodegen.hpp", "qrcodegen.cpp")},
        "destination": "third_party/sources/qrcodegen",
        "target": "qr_generator via private qr_nayuki static library",
        "modifications": "None",
        "dependencies": "C++ standard library only",
        "distribution": "Retain copyright and complete MIT permission/disclaimer with binaries/source; no project outbound license selected",
    }
    first_party = {
        **common,
        "files": {PREFIX + name: HASHES[name] for name in ("qr_generator.h", "qr_generator.cpp")},
        "license": "Explicitly authorized first-party reuse; outbound project license pending",
        "notice": "Created by RGAA on 2024/4/8",
        "target": "src/qr/qr_generator.cpp and include/rhythm/qr/generator.h",
        "modifications": [
            "Project value-only API, Google naming, initialized members, four spaces",
            "Bounded payload/pixels and four-module quiet zone",
            "Integer-only scaling, automatic mask and version, minimum medium error correction",
            "Binary-safe payload; no payload logging or global logger dependency",
        ],
        "embedded_third_party": "Nayuki remains MIT; see third_party/qrcodegen_source.json",
    }
    for path, record in (("third_party/qrcodegen_source.json", third_party),
                         ("provenance/gammaray_qr.json", first_party)):
        write_changed(ROOT / path, (json.dumps(record, indent=4) + "\n").encode())
    print("Pinned QR sources and separate first/third-party provenance prepared")


if __name__ == "__main__":
    main()
