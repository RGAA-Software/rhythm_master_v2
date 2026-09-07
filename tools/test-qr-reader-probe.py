"""Run bounded QR image and asynchronous lifecycle probes on Windows and USB Android."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(command, log):
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"QR probe failed: {log}")
    print(result.stdout.strip(), flush=True)
    return result.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--adb", default="D:/android/sdk/platform-tools/adb.exe")
    args = parser.parse_args()
    windows = ROOT / "out/qr-reader/windows/deploy/qr_reader_tests.exe"
    android = ROOT / "out/qr-reader/android/qr_reader_tests"
    evidence = {"time_utc": datetime.now(timezone.utc).isoformat(),
                "scope": "Synthetic image decoding and worker lifecycle, not camera/permission acceptance",
                "windows": {"sha256": hashlib.sha256(windows.read_bytes()).hexdigest(),
                            "result": run([str(windows)], ROOT / "out/qr-reader-windows-tests.log")}}
    adb = [args.adb, "-s", args.serial]
    remote = "/data/local/tmp/rhythm-qr-reader-" + datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S")
    subprocess.run([*adb, "shell", "mkdir", "-p", remote], check=True)
    subprocess.run([*adb, "push", str(android), str(windows.parent / "notices"), remote + "/"],
                   check=True, stdout=subprocess.DEVNULL)
    executable = remote + "/qr_reader_tests"
    output = run([*adb, "shell", "chmod 700 " + shlex.quote(executable) + " && " + shlex.quote(executable)],
                 ROOT / "out/qr-reader-android-tests.log")
    evidence["android"] = {"sha256": hashlib.sha256(android.read_bytes()).hexdigest(),
                           "device_directory": remote, "result": output}
    (ROOT / "out/qr-reader/validation.json").write_text(json.dumps(evidence, indent=4) + "\n", encoding="utf-8")
    print("QR validation recorded")


if __name__ == "__main__":
    main()
