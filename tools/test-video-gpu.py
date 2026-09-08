"""Check real GPU pixels from independently timed FFmpeg video nodes."""

import argparse
import importlib.util
from pathlib import Path
import subprocess
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve() / uuid.uuid4().hex
    if not output.is_relative_to(root / "out"):
        raise ValueError("Captures must stay in project output")
    output.mkdir(parents=True)
    spec = importlib.util.spec_from_file_location("effect_capture", root / "tools/test-effects-gpu.py")
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    subprocess.run([str(args.executable.resolve()), str(args.fixtures.resolve()), str(output)],
                   check=True, timeout=60)
    for scenario, red in enumerate((20, 50, 110, 50, 35)):
        rows = capture.read_tga(output / f"video-{scenario}.tga")
        for y in range(128):
            for x in range(128):
                if any(abs(a-b) > 2 for a, b in zip(rows[y][x], (red, 80, 200))):
                    raise AssertionError(f"Video timestamp/pixel case {scenario}: {rows[y][x]}, {output}")
    for scenario, expected in enumerate(((0, 0, 0), (30, 40, 100), (36, 48, 120),
                                          (160, 80, 200), (0, 0, 0), (40, 80, 200))):
        rows = capture.read_tga(output / f"clip-{scenario}.tga")
        for row in rows:
            for pixel in row:
                if any(abs(a-b) > 2 for a, b in zip(pixel, expected)):
                    raise AssertionError(f"Clip trim/fade case {scenario}: {pixel}, {output}")
    print(f"Video blend, source trim, blank/hold/loop and changing fade pixels passed: {output}")


if __name__ == "__main__":
    main()
