"""Prepare a pinned NASM executable locally for QUIC candidate TLS builds."""

import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
URL = "https://www.nasm.us/pub/nasm/releasebuilds/3.01/win64/nasm-3.01-win64.zip"
SHA256 = "e0ba5157007abc7b1a65118a96657a961ddf55f7e3f632ee035366dfce039ca4"


def main():
    directory = ROOT / "out/quic"
    directory.mkdir(parents=True, exist_ok=True)
    archive = directory / "nasm-3.01-win64.zip"
    if not archive.exists():
        with urllib.request.urlopen(URL, timeout=30) as response:
            archive.write_bytes(response.read(2 * 1024 * 1024))
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise SystemExit("NASM candidate tool archive hash mismatch")
    destination = directory / "nasm"
    destination.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive) as zipped:
        # Explicit names only; never extract arbitrary archive paths.
        for name in ("nasm.exe", "LICENSE"):
            data = zipped.read("nasm-3.01/" + name)
            target = destination / name
            if not target.exists() or target.read_bytes() != data:
                target.write_bytes(data)
    (destination / "source.json").write_text(json.dumps({
        "url": URL, "version": "3.01", "archive_sha256": SHA256,
        "use": "Local build tool only, not bundled in Studio/Player",
        "license": "Complete upstream notice in LICENSE",
        "nasm_sha256": hashlib.sha256((destination / "nasm.exe").read_bytes()).hexdigest(),
    }, indent=4) + "\n")
    print(f"Pinned NASM tool ready: {destination}")


if __name__ == "__main__":
    main()
