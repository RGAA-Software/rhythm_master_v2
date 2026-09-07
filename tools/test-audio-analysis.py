"""Run canonical audio analysis contracts on Windows and a selected USB Android device."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--adb", default="D:/android/sdk/platform-tools/adb.exe")
    args = parser.parse_args()
    binaries = {"windows": ROOT / "out/windows/src/audio_analysis/audio_analysis_tests.exe",
                "android": ROOT / "out/android-arm64/src/audio_analysis/audio_analysis_tests"}
    adb = [args.adb, "-s", args.serial]
    remote = "/data/local/tmp/rhythm-audio-" + datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S")
    subprocess.run([*adb, "shell", "mkdir", "-p", remote], check=True)
    subprocess.run([*adb, "push", str(binaries["android"]), str(ROOT / "third_party/notices/audio_fft"), remote + "/"],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run([*adb, "shell", "chmod", "700", remote + "/audio_analysis_tests"], check=True)
    commands = {"windows": [str(binaries["windows"])],
                "android": [*adb, "shell", shlex.quote(remote + "/audio_analysis_tests")]}
    result = {"time_utc": datetime.now(timezone.utc).isoformat(), "device_directory": remote, "platforms": {}}
    for platform, command in commands.items():
        run = subprocess.run(command, capture_output=True, text=True, timeout=90)
        log = ROOT / f"out/audio-analysis-{platform}.log"
        log.write_text(run.stdout + run.stderr, encoding="utf-8")
        if run.returncode:
            raise RuntimeError(f"Audio contracts failed: {log}")
        print(platform, run.stdout.strip(), flush=True)
        result["platforms"][platform] = {"sha256": hashlib.sha256(binaries[platform].read_bytes()).hexdigest()}
    output = ROOT / "out/audio-analysis-validation.json"
    output.write_text(json.dumps(result, indent=4) + "\n", encoding="utf-8")
    print("Recorded", output)


if __name__ == "__main__":
    main()
