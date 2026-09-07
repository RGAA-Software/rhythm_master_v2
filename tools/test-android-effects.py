"""Run the selected local visual contracts on Android, without installing an APK."""

import argparse
import json
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {
    "render_contract_tests": "src/rhythm_render",
    "particle_simulation_tests": "src/particles",
    "trail_tests": "src/runtime",
    "image_runtime_tests": "src/runtime",
    "scene_primitive_tests": "src/scene3d",
    "program_contract_tests": "src/project_io",
    "android_gpu_contract_tests": "src/android_player",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adb", type=Path, default=Path("D:/android/sdk/platform-tools/adb.exe"))
    parser.add_argument("--serial", required=True)
    parser.add_argument("--build", type=Path, default=ROOT / "out/android-arm64")
    args = parser.parse_args()
    run_id = uuid.uuid4().hex
    output = ROOT / "out/android-effects-review" / run_id
    output.mkdir(parents=True)
    remote = "/data/local/tmp/rhythm-effects-" + run_id

    def adb(*arguments, timeout=120):
        result = subprocess.run([str(args.adb), "-s", args.serial, *arguments],
                                capture_output=True, timeout=timeout)
        if result.returncode:
            diagnostic = (result.stdout + result.stderr).decode("utf-8", errors="replace")
            (output / "failure.log").write_text(
                f"adb arguments: {arguments!r}\nexit: {result.returncode}\n{diagnostic}", encoding="utf-8")
            raise RuntimeError(f"adb exit {result.returncode}; evidence: {output}\n{diagnostic}")
        return (result.stdout + result.stderr).decode("utf-8", errors="replace")

    abi = adb("shell", "getprop", "ro.product.cpu.abi").strip()
    if abi != "arm64-v8a":
        raise ValueError("This runner requires an authorized arm64-v8a device")
    adb("shell", "mkdir", "-p", remote)
    results = []
    for target, directory in TARGETS.items():
        adb("push", str(args.build / directory / target), remote + "/" + target)
        adb("shell", "chmod", "700", remote + "/" + target)
        print("Running", target, "on", args.serial, flush=True)
        log = adb("shell", remote + "/" + target)
        (output / (target + ".log")).write_text(log, encoding="utf-8")
        results.append(target)
    record = {"serial": args.serial, "abi": abi, "remote": remote,
              "passed": results, "apk_installation": "not attempted"}
    (output / "results.json").write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")
    print(output, flush=True)


if __name__ == "__main__":
    main()
