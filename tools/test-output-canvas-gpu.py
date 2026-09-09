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
    parser.add_argument("--scope", action="store_true")
    parser.add_argument("--automation", action="store_true")
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
    if args.automation:
        command.append("--automation")
    subprocess.run(command,
                   check=True, timeout=85)
    captures = (["driven"] if args.automation else []) + ["before", "moved", "undone", "reopened"]
    for name in captures:
        rect = json.loads((output / (name + ".json")).read_text(encoding="utf-8"))
        rows = capture.read_tga(output / (name + ".tga"))
        moved = name in {"moved", "reopened"}
        probes = [(.37, not moved), (.06, moved), (.78, True)] if args.scope else [(.3, not moved), (.85, moved)]
        for fraction, red in probes:
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
    if args.scope:
        print("Actual pixels match edited and unaffected instances, undo and saved reopen")
    elif args.automation:
        print("Actual pixels preserve position when freezing the driver, then match drag, undo and saved reopen")
    else:
        print("Actual pixels match drag, undo and saved reopen")


if __name__ == "__main__":
    main()
