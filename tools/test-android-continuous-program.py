"""Short Redmi K40S touch acceptance of the mixed built-in performance draft.

Uses the 2400x1080/440dpi acceptance device's main-panel anchors. Dialog controls
are located in fresh accessibility dumps. Does not save the temporary program,
uninstall, clear app data, replace the IME, or claim acoustic/endurance coverage.
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

from android_view_bounds import center, read_views


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--adb", default="D:/android/sdk/platform-tools/adb.exe")
    parser.add_argument("--prepared", action="store_true", help="Use the already prepared mixed draft")
    args = parser.parse_args()
    output = ROOT / "out/android-continuous-program" / uuid.uuid4().hex
    output.mkdir(parents=True)
    print("Continuous program evidence:", output, flush=True)
    strings = {n.attrib["name"]: n.text for n in ET.parse(
        ROOT / "platforms/android/res/values-zh-rCN/strings.xml").getroot()}
    sequence = 0

    def adb(*command, timeout=30):
        result = subprocess.run([args.adb, "-s", args.serial, *map(str, command)],
                                capture_output=True, timeout=timeout)
        if result.returncode:
            (output / "adb-failure.txt").write_bytes(result.stdout + result.stderr)
            raise RuntimeError("ADB failed: " + repr(command))
        return result.stdout

    def saved():
        adb("shell", "run-as", APP, "test", "-f", "files/performance/list.json")
        return adb("exec-out", "run-as", APP, "cat", "files/performance/list.json")

    def shot(name):
        data = adb("exec-out", "screencap", "-p")
        (output / (name + ".png")).write_bytes(data)
        return struct.unpack(">II", data[16:24])

    def hierarchy():
        nonlocal sequence
        remote = "/sdcard/rhythm-continuous-program.xml"
        result = adb("shell", "uiautomator", "dump", "--compressed", remote)
        if b"dumped to:" not in result:
            raise RuntimeError("Fresh UI hierarchy unavailable")
        data = adb("shell", "cat", remote)
        sequence += 1
        (output / f"ui-{sequence}.xml").write_bytes(data)
        return ET.fromstring(data)

    def bounds(node):
        return list(map(int, re.findall(r"\d+", node.get("bounds", ""))))

    def tap(x, y):
        adb("shell", "input", "tap", round(x), round(y))
        time.sleep(.3)

    def touch(node):
        x1, y1, x2, y2 = bounds(node)
        tap((x1 + x2) / 2, (y1 + y2) / 2)

    def go():
        for _ in range(6):
            root = hierarchy()
            node = next((n for n in root.iter("node") if n.get("text") == strings["scene_go"]
                         and n.get("enabled") == "true" and bounds(n)[3] - bounds(n)[1] >= 40), None)
            if node is not None:
                touch(node)
                return
            scroll = next(n for n in root.iter("node") if n.get("class") == "android.widget.ScrollView")
            x1, y1, x2, y2 = bounds(scroll)
            adb("shell", "input", "swipe", (x1 + x2) // 2, y2 - 40, (x1 + x2) // 2, y1 + 70, 350)
        raise RuntimeError("Prepared scene Go unavailable")

    def main_action(size, action):
        name = "player_queue" if action == "queue" else "player_pause"
        tap(*center(read_views(adb), name))

    original = saved()
    (output / "program-before.json").write_bytes(original)
    if not args.prepared:
        with (output / "prepare.log").open("w", encoding="utf-8") as log:
            subprocess.run([sys.executable, str(ROOT / "tools/test-android-program-ui.py"),
                            "--serial", args.serial, "--adb", args.adb, "--reopen-only", "--mixed-draft"],
                           stdout=log, stderr=subprocess.STDOUT, check=True)
    size = shot("prepared")
    if size != (2400, 1080) or b"440" not in adb("shell", "wm", "density"):
        raise RuntimeError("This touch anchor profile requires the 2400x1080 440dpi acceptance device")
    pid = adb("shell", "pidof", APP).decode().strip()

    def logs():
        return adb("logcat", "-d", "--pid=" + pid, "-t", 1500)

    def commits(data):
        return re.findall(rb"scene committed transition=(\d+) audio_synchronized=(\d) width=(\d+) height=(\d+)", data)

    count = len(commits(logs()))
    accepted = []
    try:
        for index, (width, height, audio) in enumerate(((1280, 720, 1), (720, 1280, 0), (1280, 720, 1))):
            go()
            time.sleep(.4)
            assert len(commits(logs())) == count, "Paused Go committed a scene"
            adb("shell", "input", "keyevent", 4)
            main_action(size, "pause")
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and len(commits(logs())) == count:
                time.sleep(.2)
            data = logs()
            (output / f"transition-{index}.log").write_bytes(data)
            rows = commits(data)
            assert len(rows) == count + 1, "Exactly one queued work must commit"
            count += 1
            record = tuple(map(int, rows[-1]))
            assert record[1:] == (audio, width, height), "Wrong scene canvas or audio takeover"
            time.sleep(.8)
            size = shot(f"accepted-{index}")
            assert (size[0] > size[1]) == (width > height), "Accepted canvas did not control orientation"
            main_action(size, "pause")
            if index == 1:
                adb("shell", "input", "keyevent", 3)
                time.sleep(.7)
                adb("shell", "am", "start", "-n", APP + "/.PlayerActivity")
                time.sleep(1)
                assert adb("shell", "pidof", APP).decode().strip() == pid, "Short background cycle restarted the process"
                shot("portrait-resumed")
            main_action(size, "queue")
            root = hierarchy()
            spinner = next(n for n in root.iter("node") if n.get("class") == "android.widget.Spinner")
            remaining = 2 - index
            if remaining:
                touch(spinner)
                items = [n.get("text") for n in hierarchy().iter("node") if n.get("class") == "android.widget.CheckedTextView"]
                assert len(items) == remaining, "Queue row lost across commit/orientation/background"
                adb("shell", "input", "keyevent", 4)
            else:
                assert not any(n.get("text") for n in spinner.iter("node")), "Completed program left a row"
            accepted.append({"transition": record[0], "canvas": [width, height], "audio_synchronized": bool(audio), "remaining": remaining})
            assert saved() == original, "Playback overwrote saved program"
        (output / "result.json").write_text(json.dumps({"status": "passed", "accepted": accepted,
            "apk_sha256": hashlib.sha256((ROOT / "out/android-arm64-release/apk/rhythm-player-release.apk").read_bytes()).hexdigest(),
            "saved_program_unchanged": True, "acoustic_capture": False}, indent=4) + "\n", encoding="utf-8")
        print("Mixed Android program: consumed handoffs, portrait orientation, background recovery and queue retention passed", flush=True)
    except Exception:
        shot("failure")
        (output / "failure.log").write_bytes(logs())
        raise


if __name__ == "__main__":
    main()
