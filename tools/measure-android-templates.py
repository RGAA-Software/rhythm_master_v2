"""Measure actual GLES offscreen template execution; not APK/display or thermal acceptance."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

from content_identity import verify_package_source

ROOT = Path(__file__).resolve().parents[1]
NAMES = ("layered_neon", "aurora_clouds", "firefly_garden", "prismatic_lotus", "stellar_currents", "orbital_reliquary", "resonance_gate", "resonance_live")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--adb", type=Path, default=Path("D:/android/sdk/platform-tools/adb.exe"))
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--balanced", action="store_true")
    parser.add_argument("--build", type=Path, default=ROOT / "out/android-arm64-release")
    parser.add_argument("--packages", type=Path, default=ROOT / "out/windows-release/content/packages")
    parser.add_argument("--name", action="append", help="Existing bundled template directory name")
    args = parser.parse_args()
    if args.compact and args.balanced:
        parser.error("Choose either compact or balanced")
    for name in args.name or NAMES:
        if not name.replace("_", "").isalnum() or not (ROOT / "content/templates" / name / "manifest.json").is_file():
            parser.error("Expected an existing bundled template name")
    run_id = uuid.uuid4().hex
    output = ROOT / "out/android-template-measurements" / run_id
    output.mkdir(parents=True)
    remote = "/data/local/tmp/rhythm-template-measure-" + run_id

    def adb(*arguments, timeout=180):
        run = subprocess.run([str(args.adb), "-s", args.serial, *map(str, arguments)],
                             capture_output=True, timeout=timeout)
        log = (run.stdout + run.stderr).decode("utf-8", errors="replace")
        if run.returncode:
            (output / "failure.log").write_text(log, encoding="utf-8")
            raise RuntimeError(f"ADB failed: {output}\n{log}")
        return log

    adb("shell", "mkdir", "-p", remote)
    executable = args.build / "src/android_player/android_gpu_contract_tests"
    adb("push", executable, remote + "/measure")
    adb("shell", "chmod", "700", remote + "/measure")
    results = {"serial": args.serial, "compact": args.compact, "balanced": args.balanced, "remote": remote,
               "build": str(args.build), "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
               "templates": {}}
    for name in args.name or NAMES:
        package = args.packages / (name + ".rhythmpack")
        source_sha = verify_package_source(ROOT / "content/templates" / name, package)
        adb("push", package, remote + "/" + name)
        print("Measuring", name, flush=True)
        log = adb("shell", "LD_LIBRARY_PATH=/data/local/tmp", remote + "/measure", remote + "/" + name,
                  "--benchmark-compact" if args.compact else "--benchmark-balanced" if args.balanced else "--benchmark")
        (output / (name + ".log")).write_text(log, encoding="utf-8")
        line = next(line for line in log.splitlines() if line.startswith("native_offscreen_frames="))
        results["templates"][name] = {"sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
                                        "source_sha256": source_sha, "measurement": line}
        (output / "results.json").write_text(json.dumps(results, indent=4) + "\n", encoding="utf-8")
        print(line, flush=True)
    print(output, flush=True)


if __name__ == "__main__":
    main()
