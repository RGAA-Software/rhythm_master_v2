"""Verify embedded PNG alpha, framing, cached resize and device reconstruction on D3D11."""

import argparse
import importlib.util
from pathlib import Path
import subprocess
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
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
    subprocess.run([str(args.executable.resolve()), str(root / "src/media/tests/fixtures/alpha.png"),
                    str(output)], check=True, timeout=60)
    for scenario in range(5):
        rows = capture.read_tga(output / f"image-{scenario}.tga")
        height = 64 if scenario == 3 else 128
        for y in range(height):
            for x in range(128):
                expected = (0, 0, 0) if scenario == 0 and (y < 16 or y >= 112) else (30, 10, 50)
                if scenario == 4:
                    expected = (60, 100, 20)
                if any(abs(a-b) > 2 for a, b in zip(rows[y][x], expected)):
                    raise AssertionError(
                        f"Image alpha/framing case {scenario} at {x},{y}: {rows[y][x]}, {output}")
    print(f"Embedded image alpha/framing, device reconstruction and cached resize passed: {output}")


if __name__ == "__main__":
    main()
