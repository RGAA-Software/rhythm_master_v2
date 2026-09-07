"""Run the same installed-vcpkg media contracts on Windows and USB Android."""

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
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S%f")
    remote = "/data/local/tmp/rhythm-media-" + stamp
    adb = [args.adb, "-s", args.serial]
    windows = ROOT / "out/media-validation/windows/media/deploy/media_audio_tests.exe"
    android = ROOT / "out/media-validation/android/media/media_audio_tests"
    subprocess.run([*adb, "shell", "mkdir", "-p", remote], check=True)
    subprocess.run([*adb, "push", str(android), remote + "/"], check=True, stdout=subprocess.DEVNULL)
    notice = Path("C:/source/vcpkg/installed/arm64-android/share/ffmpeg/copyright")
    subprocess.run([*adb, "push", str(notice), remote + "/FFMPEG-NOTICES.txt"], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([*adb, "shell", "chmod", "700", remote + "/media_audio_tests"], check=True)
    commands = {"windows": [str(windows), str(windows.parent / "fixtures")],
                "android": [*adb, "shell", shlex.quote(remote + "/media_audio_tests") + " " + shlex.quote(remote + "/fixtures")]}
    results = {"time_utc": datetime.now(timezone.utc).isoformat(), "device_directory": remote, "platforms": {}}
    for platform, command in commands.items():
        run = subprocess.run(command, capture_output=True, text=True, timeout=90)
        log = ROOT / f"out/media-test-{platform}.log"
        log.write_text(run.stdout + run.stderr, encoding="utf-8")
        if run.returncode:
            raise RuntimeError(f"Media contracts failed: {log}")
        print(platform, run.stdout.strip(), flush=True)
        binary = windows if platform == "windows" else android
        results["platforms"][platform] = {"sha256": hashlib.sha256(binary.read_bytes()).hexdigest()}
    for name in ("audio_output", "audio_playback"):
        binary = ROOT / f"out/media-validation/windows/{name}/deploy/{name}_tests.exe"
        command = [str(binary)]
        if name == "audio_playback":
            command += [str(windows.parent / "fixtures")]
        run = subprocess.run(command, capture_output=True, text=True, timeout=90)
        log = ROOT / f"out/{name}-test-windows.log"
        log.write_text(run.stdout + run.stderr, encoding="utf-8")
        if run.returncode:
            raise RuntimeError(f"Device playback contracts failed: {log}")
        print("windows", run.stdout.strip(), flush=True)
        results["platforms"]["windows"][name + "_sha256"] = hashlib.sha256(binary.read_bytes()).hexdigest()
    (ROOT / "out/media-validation/results.json").write_text(json.dumps(results, indent=4) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
