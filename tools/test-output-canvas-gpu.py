"""Verify changed pixels from Studio's actual graph selection and canvas drag."""

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
    parser.add_argument("--scene", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve() / uuid.uuid4().hex
    if not output.is_relative_to(root / "out"):
        raise ValueError("Evidence must stay inside project output")
    output.mkdir(parents=True)
    spec = importlib.util.spec_from_file_location("capture", root / "tools/test-effects-gpu.py")
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    print(f"Studio canvas evidence: {output}", flush=True)
    command = [str(args.executable.resolve()), str(args.resources.resolve()), str(output)]
    if args.scene:
        command.append("--scene")
    subprocess.run(command,
                   check=True, timeout=85)
    for name in ["before", "moved", "undone", "reopened"]:
        rect = json.loads((output / (name + ".json")).read_text(encoding="utf-8"))
        rows = capture.read_tga(output / (name + ".tga"))
        moved = name in {"moved", "reopened"}
        for fraction, red in [(.3, not moved), (.85, moved)]:
            x = round(rect["x"] + rect["width"] * fraction)
            y = round(rect["y"] + rect["height"] * .6)
            for dy in range(-3, 4):
                for dx in range(-3, 4):
                    pixel = rows[y + dy][x + dx]
                    if red:
                        valid = pixel[0] >= 250 and pixel[1] <= 4 and pixel[2] <= 4
                    else:
                        # Transparent canvas shows the editor's dark background.
                        valid = max(pixel) < 45
                    if not valid:
                        raise AssertionError(f"{name} at {fraction}: {pixel}, red={red}; {output}")
    print("Actual output moved right; undo restored it and reopen kept the saved transform")


if __name__ == "__main__":
    main()
