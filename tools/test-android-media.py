"""Run selected native FFmpeg video contracts on an authorized USB device."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--adb", type=Path, default=Path("D:/android/sdk/platform-tools/adb.exe"))
    parser.add_argument("--build", type=Path, default=ROOT / "out/media-validation/android-lgpl")
    parser.add_argument("--fixtures", type=Path, default=ROOT / "out/video-fixtures")
    args = parser.parse_args()
    run_id = uuid.uuid4().hex
    output = ROOT / "out/android-media-review" / run_id
    output.mkdir(parents=True)
    remote = "/data/local/tmp/rhythm-media-" + run_id

    def adb(*arguments):
        result = subprocess.run([str(args.adb), "-s", args.serial, *arguments],
                                capture_output=True, timeout=60)
        log = (result.stdout + result.stderr).decode("utf-8", errors="replace")
        if result.returncode:
            (output / "failure.log").write_text(log, encoding="utf-8")
            raise RuntimeError(f"ADB failed: {arguments!r}; evidence: {output}")
        return log

    abi = adb("shell", "getprop", "ro.product.cpu.abi").strip()
    if abi != "arm64-v8a":
        raise ValueError("An authorized arm64-v8a device is required")
    adb("shell", "mkdir", "-p", remote)
    adb("push", str(args.fixtures.resolve()), remote + "/fixtures")
    results = []
    for module, name in (("media", "media_video_tests"), ("video_playback", "video_playback_tests")):
        binary = args.build / module / name
        digest = hashlib.sha256(binary.read_bytes()).hexdigest()
        adb("push", str(binary.resolve()), remote + "/" + name)
        adb("shell", "chmod", "700", remote + "/" + name)
        print("Running", name, "on", args.serial, flush=True)
        log = adb("shell", remote + "/" + name, remote + "/fixtures")
        (output / (name + ".log")).write_text(log, encoding="utf-8")
        results.append({"test": name, "binary_sha256": digest})
    record = {"serial": args.serial, "abi": abi, "remote": remote, "passed": results,
              "apk_installation": "not attempted"}
    (output / "results.json").write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")
    print(output, flush=True)


if __name__ == "__main__":
    main()
