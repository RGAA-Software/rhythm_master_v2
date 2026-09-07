"""Capture an actual Player graph and encode a reproducible synthetic-audio preview."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--executable", type=Path,
                        default=ROOT / "out/windows/src/windows_spike/windows_effects_gpu_tests.exe")
    parser.add_argument("--ffmpeg", type=Path, default=Path(
        "C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe"))
    parser.add_argument("--silent", action="store_true", help="Render with zero audio features")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("Preview output must stay inside project out")
    output = output / uuid.uuid4().hex
    output.mkdir(parents=True)
    result = subprocess.run([str(args.executable.resolve()), str(output),
                             str(args.package.resolve())] + (["--silent"] if args.silent else []),
                            capture_output=True, timeout=120)
    (output / "render.log").write_bytes(result.stdout + result.stderr)
    result.check_returncode()
    for index in range(180):
        if not (output / f"preview-{index}.tga").is_file():
            raise ValueError(f"Missing rendered frame {index}: {output}")
    subprocess.run([str(args.ffmpeg), "-hide_banner", "-loglevel", "error", "-nostdin",
                    "-framerate", "30", "-i", str(output / "preview-%d.tga"),
                    "-frames:v", "180", "-c:v", "libx264", "-pix_fmt", "yuv420p",
                    "-movflags", "+faststart", str(output / "preview.mp4")],
                   check=True, timeout=120)
    subprocess.run([str(args.ffmpeg), "-hide_banner", "-loglevel", "error", "-nostdin",
                    "-i", str(output / "preview-120.tga"), "-frames:v", "1",
                    str(output / "preview.png")], check=True, timeout=30)
    record = {"package": str(args.package.resolve()),
              "sha256": hashlib.sha256(args.package.read_bytes()).hexdigest(),
              "frames": 180, "fps": 30, "extent": [960, 540],
              "input": "Zero audio features (silence)" if args.silent else
                       "Deterministic synthetic canonical audio features; not live audio capture",
              "renderer": "Actual Windows D3D11 Player session",
              "visual_acceptance": "pending", "encoding_tool": str(args.ffmpeg)}
    (output / "preview.json").write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")
    print(output)


if __name__ == "__main__":
    main()
