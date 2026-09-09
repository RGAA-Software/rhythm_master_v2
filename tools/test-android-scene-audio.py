"""Short APK touch check: persisted queue, paused Go, audio dissolve and GPU scene takeover.

Requires the saved program to begin with Luminous Concerto, as prepared by the
program UI acceptance fixture. Preserves saved entries and application data.
Uses the production audio driver; records presentation estimates, not acoustic
loopback or listening quality. Leaves playback paused after a successful check.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
APP = "org.rhythmmaster.player"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--adb", type=Path, default=Path("D:/android/sdk/platform-tools/adb.exe"))
    parser.add_argument("--apk", type=Path, default=ROOT / "out/android-arm64-release/apk/rhythm-player-release.apk")
    parser.add_argument("--locale", choices=("zh-CN", "en-US"), default="zh-CN")
    parser.add_argument("--hard-cut-head", action="store_true",
                        help="Set zero duration through the draft program UI, leaving saved data unchanged")
    args = parser.parse_args()
    output = ROOT / "out/android-scene-audio" / uuid.uuid4().hex
    output.mkdir(parents=True)
    print("Scene/audio APK evidence:", output, flush=True)
    sequence = 0

    def adb(*arguments, timeout=30):
        result = subprocess.run([str(args.adb), "-s", args.serial, *map(str, arguments)],
                                capture_output=True, timeout=timeout)
        if result.returncode:
            (output / "adb-failure.txt").write_bytes(result.stdout + result.stderr)
            raise RuntimeError(f"ADB failed: {arguments!r}; evidence: {output}")
        return result.stdout

    def saved():
        # Check remote existence before exec-out: its exit code alone is insufficient.
        adb("shell", "run-as", APP, "test", "-f", "files/performance/list.json")
        return adb("exec-out", "run-as", APP, "cat", "files/performance/list.json")

    def shot(name):
        data = adb("exec-out", "screencap", "-p")
        (output / (name + ".png")).write_bytes(data)
        return struct.unpack(">II", data[16:24])

    def hierarchy():
        nonlocal sequence
        remote = "/sdcard/rhythm-scene-audio.xml"
        result = adb("shell", "uiautomator", "dump", "--compressed", remote)
        if b"dumped to:" not in result:
            raise RuntimeError("UI hierarchy not refreshed; rejecting stale dump")
        data = adb("shell", "cat", remote)
        sequence += 1
        (output / f"queue-{sequence}.xml").write_bytes(data)
        return ET.fromstring(data)

    def go():
        folder = "values-zh-rCN" if args.locale == "zh-CN" else "values"
        strings = {node.attrib["name"]: node.text for node in ET.parse(
            ROOT / "platforms/android/res" / folder / "strings.xml").getroot()}
        for _ in range(4):
            root = hierarchy()
            for node in root.iter("node"):
                if node.get("text") == strings["scene_go"] and node.get("enabled") == "true":
                    x1, y1, x2, y2 = map(int, re.findall(r"\d+", node.get("bounds", "")))
                    if y2 - y1 >= 50:
                        adb("shell", "input", "tap", (x1 + x2) // 2, (y1 + y2) // 2)
                        return
            scroll = next(node for node in root.iter("node")
                          if node.get("class") == "android.widget.ScrollView")
            x1, y1, x2, y2 = map(int, re.findall(r"\d+", scroll.get("bounds", "")))
            adb("shell", "input", "swipe", (x1 + x2) // 2, y2 - 50,
                (x1 + x2) // 2, y1 + 100, 350)
        raise RuntimeError("Ready queue Go button unavailable")

    original = saved()
    program = json.loads(original)
    if not program["entries"] or program["entries"][0]["work"]["content_id"] != "official.templates.luminous_concerto":
        raise ValueError("Saved program must begin with the existing Concerto acceptance fixture")
    if not args.hard_cut_head and program["entries"][0]["transition_seconds"] != 1:
        raise ValueError("The normal audio fade fixture requires a one-second head transition")
    (output / "program-before.json").write_bytes(original)
    try:
        (output / "install.txt").write_bytes(adb("install", "-r", args.apk, timeout=60))
        with (output / "program-reopen.txt").open("w", encoding="utf-8") as log:
            subprocess.run([sys.executable, str(ROOT / "tools/test-android-program-ui.py"),
                            "--serial", args.serial, "--adb", str(args.adb), "--locale", args.locale,
                            "--reopen-only", *(["--hard-cut-head"] if args.hard_cut_head else [])],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        width, height = shot("paused-ready")
        pid = adb("shell", "pidof", APP).decode().strip()

        def logs():
            return adb("logcat", "-d", "--pid=" + pid, "-t", 500)

        go()
        time.sleep(.5)
        pending = logs()
        (output / "paused-go.log").write_bytes(pending)
        if b"scene committed" in pending:
            raise RuntimeError("Paused Go advanced the scene")
        shot("paused-go")
        adb("shell", "input", "keyevent", 4)
        adb("shell", "input", "tap", round(width * .783), round(height * .134))
        time.sleep(.6)
        shot("transition")
        deadline = time.monotonic() + 8
        log = logs()
        while b"audio_synchronized=1" not in log and time.monotonic() < deadline:
            time.sleep(.2)
            log = logs()
        (output / "transition.log").write_bytes(log)
        if b"audio_synchronized=1" not in log:
            raise RuntimeError("No consumed-audio scene commitment in actual APK")
        rows = re.findall(rb"scene audio id=(\d+) phase=(\d+) consumed=(\d+) elapsed=(\d+) previous=([\d.]+) incoming=([\d.]+) driver=(\w+) error=([^\r\n]*)", log)
        completed = next((row for row in rows if row[1] == b"5"), None)
        if completed is None or completed[6] in (b"dummy", b"unknown") or completed[7]:
            raise RuntimeError("Production audio driver did not confirm transition")
        matching = [row for row in rows if row[0] == completed[0]]
        if not args.hard_cut_head and not any(row[1] == b"3" for row in matching):
            raise RuntimeError("No audible mixing state observed")
        counters = [int(row[2]) for row in matching]
        expected_frames = 0 if args.hard_cut_head else 48000
        if counters != sorted(counters) or counters[-1] <= counters[0] or int(completed[3]) != expected_frames:
            raise RuntimeError("Device consumed counters did not confirm the requested transition")
        shot("committed")
        (output / "audio-flinger.txt").write_bytes(adb("shell", "dumpsys", "media.audio_flinger"))
        adb("shell", "input", "tap", round(width * .783), round(height * .134))
        # Successful commitment, not Go, removes the active row. Inspect the
        # actual queue UI after pausing instead of trusting the native log alone.
        adb("shell", "input", "tap", round(width * .675), round(height * .458))
        root = hierarchy()
        spinner = next(node for node in root.iter("node")
                       if node.get("class") == "android.widget.Spinner")
        remaining = len(program["entries"]) - 1
        if remaining:
            x1, y1, x2, y2 = map(int, re.findall(r"\d+", spinner.get("bounds", "")))
            adb("shell", "input", "tap", (x1 + x2) // 2, (y1 + y2) // 2)
            rows_ui = [node.get("text") for node in hierarchy().iter("node")
                       if node.get("class") == "android.widget.CheckedTextView"]
            if len(rows_ui) != remaining or not rows_ui[0].startswith(program["entries"][1]["title"]):
                raise RuntimeError("Committed scene did not remove exactly its queue row")
            adb("shell", "input", "keyevent", 4)
        elif any(node.get("text") for node in spinner.iter("node")):
            raise RuntimeError("Committed last scene left a queue row")
        shot("queue-after-commit")
        adb("shell", "input", "keyevent", 4)
        if saved() != original:
            raise RuntimeError("Playback changed the saved performance program")
        result = {"status": "passed", "serial": args.serial, "driver": completed[6].decode(),
                  "apk_sha256": hashlib.sha256(args.apk.read_bytes()).hexdigest(),
                  "transition_id": int(completed[0]), "consumed_frames": counters,
                  "previous_seconds": float(completed[4]), "incoming_seconds": float(completed[5]),
                  "hard_cut_head_draft": args.hard_cut_head,
                  "remaining_queue_rows": remaining,
                  "saved_program_unchanged": True, "acoustic_capture": False}
        (output / "result.json").write_text(json.dumps(result, indent=4) + "\n", encoding="utf-8")
        print("APK queue touch / paused Go / production audio / scene commitment passed", flush=True)
    except Exception:
        shot("failure")
        raise


if __name__ == "__main__":
    main()
