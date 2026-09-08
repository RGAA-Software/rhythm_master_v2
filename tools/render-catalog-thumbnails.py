"""Produce actual rendered, aspect-fitted catalog thumbnails with recorded provenance."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", action="append", help="Catalog entry directory name; default: all")
    parser.add_argument("--kind", choices=("templates", "semantic"), default="templates")
    parser.add_argument("--build", type=Path, default=ROOT / "out/windows-release")
    args = parser.parse_args()
    source_root = ROOT / "content" / args.kind
    names = args.name or sorted(path.name for path in source_root.iterdir() if path.is_dir())
    executable = args.build.resolve() / "src/windows_spike/windows_effects_gpu_tests.exe"
    ffmpeg = Path("C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe")
    output = ROOT / "out/catalog-thumbnails" / uuid.uuid4().hex
    for name in names:
        source = (source_root / name).resolve()
        if source.parent != source_root or not (source / "manifest.json").is_file():
            raise ValueError("Expected an existing project-owned catalog entry directory name")
        package_directory = "packages" if args.kind == "templates" else "semantic_packages"
        package = args.build.resolve() / "content" / package_directory / (name + ".rhythmpack")
        work = output / name
        work.mkdir(parents=True)
        rendered = subprocess.run([str(executable), str(work), str(package), "--thumbnail"],
                                  capture_output=True, timeout=120)
        (work / "render.log").write_bytes(rendered.stdout + rendered.stderr)
        rendered.check_returncode()
        subprocess.run([str(ffmpeg), "-hide_banner", "-loglevel", "error", "-nostdin",
                        "-i", str(work / "thumbnail.tga"), "-frames:v", "1", "-vf", "format=rgb24,format=rgba",
                        "-f", "rawvideo", str(work / "thumbnail.rgba")], check=True, timeout=30)
        pixels = (work / "thumbnail.rgba").read_bytes()
        if len(pixels) != 256 * 144 * 4:
            raise ValueError("Unexpected thumbnail dimensions")
        (source / "thumbnail.rgba").write_bytes(pixels)
        record = {"template": name, "package_sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
                  "thumbnail_sha256": hashlib.sha256(pixels).hexdigest(), "extent": [256, 144],
                  "seconds": 4, "renderer": "Windows D3D11 Player", "input": "synthetic canonical audio features",
                  "aspect": "preserved; black letterbox", "visual_acceptance": "pending"}
        (source / "thumbnail.json").write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")
        print(name, flush=True)
    print(output, flush=True)


if __name__ == "__main__":
    main()
