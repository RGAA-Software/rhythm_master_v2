"""Compile both render backends twice without modifying the active build cache."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GROUPS = ("color", "scene", "filter", "noise", "mapping", "displace",
          "execution_probe", "gpu_points", "color_pipeline", "depth", "environment")


def verify(compiler, output):
    output = output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("Compiler verification belongs in the project out directory")
    output.mkdir(parents=True, exist_ok=True)
    compiler_hash = hashlib.sha256(compiler.read_bytes()).hexdigest()
    records = []
    for platform in ("windows", "android"):
        for group in GROUPS:
            headers = []
            for attempt in range(2):
                header = output / platform / str(attempt) / (group + ".h")
                header.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run([sys.executable, str(ROOT / "tools/build-render-shaders.py"),
                                "--compiler", str(compiler), "--output", str(header),
                                "--platform", platform, "--group", group],
                               check=True, timeout=240)
                headers.append(header.read_bytes())
            if headers[0] != headers[1]:
                raise RuntimeError(f"Non-reproducible shader output: {platform}/{group}")
            symbols = re.findall(rb"(k\w+)\[\] = \{([^}]+)\}", headers[0])
            if not symbols:
                raise RuntimeError("No embedded shader programs found")
            programs = {}
            for symbol, initializer in symbols:
                data = bytes(int(value, 16) for value in re.findall(rb"0x([0-9a-f]{2})", initializer))
                if data[:3] not in (b"VSH", b"FSH", b"CSH") or data[3] != 12:
                    raise RuntimeError(f"Unsupported container {data[:4]!r}: {platform}/{symbol!r}")
                programs[symbol.decode("ascii")] = {
                    "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                    "container": data[:3].decode("ascii") + str(data[3]),
                }
            records.append({"platform": platform, "group": group, "programs": programs,
                            "header_sha256": hashlib.sha256(headers[0]).hexdigest()})
            print(f"{platform}/{group}: {len(programs)} programs, identical repeated output", flush=True)
    if hashlib.sha256(compiler.read_bytes()).hexdigest() != compiler_hash:
        raise RuntimeError("Compiler changed during verification")
    record = {"compiler": str(compiler.resolve()), "compiler_sha256": compiler_hash,
              "scope": "Compilation and repeated shader bytes only; native GPU checks are separate",
              "groups": records}
    (output / "verification.json").write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    verify(args.compiler.resolve(), args.output)


if __name__ == "__main__":
    main()
