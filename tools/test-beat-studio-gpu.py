"""Check pixels from the real Studio quantized snapshot UI workflow."""

import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--resources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve() / uuid.uuid4().hex
    if not output.is_relative_to(root / "out"):
        raise ValueError("Evidence must stay in project output")
    output.mkdir(parents=True)
    spec = importlib.util.spec_from_file_location("capture", root / "tools/test-effects-gpu.py")
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    print(f"Studio beat evidence: {output}", flush=True)
    subprocess.run([str(args.executable.resolve()), str(args.resources.resolve()), str(output)],
                   check=True, timeout=75)
    for name, expected in [("paused", (255, 0, 0)), ("before", (255, 0, 0)),
                           ("after", (0, 0, 255)), ("reopened", (255, 0, 0))]:
        rows = capture.read_tga(output / (name + ".tga"))
        position = json.loads((output / (name + ".json")).read_text(encoding="utf-8"))
        x, y = round(position["x"]), round(position["y"])
        for offset_y in range(-8, 9):
            for offset_x in range(-8, 9):
                actual = rows[y + offset_y][x + offset_x]
                if any(abs(a - b) > 3 for a, b in zip(actual, expected)):
                    raise AssertionError(f"{name}: current output {actual}, expected {expected}; {output}")
    print("Studio output pixels stayed red while paused/before beat and became blue after beat")


if __name__ == "__main__":
    main()
