"""Check manual beat selection and paused recall on an installed Android Player.

Requires the selected work to have distinct snapshots and a landscape canvas. Preserves
app data, restarts only this app, and leaves playback paused after verification.
This short device check does not establish thermal/endurance acceptance.
"""

import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
APP = "org.rhythmmaster.player"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adb", type=Path, default=Path("D:/android/sdk/platform-tools/adb.exe"))
    parser.add_argument("--serial", required=True)
    parser.add_argument("--locale", choices=("en-US", "zh-CN"), default="zh-CN")
    args = parser.parse_args()
    output = ROOT / "out/android-beat-ui" / uuid.uuid4().hex
    output.mkdir(parents=True)
    print(f"Device evidence: {output}", flush=True)
    folder = "values-zh-rCN" if args.locale == "zh-CN" else "values"
    strings = {item.attrib["name"]: item.text for item in ET.parse(
        ROOT / "platforms/android/res" / folder / "strings.xml").getroot()}
    remote = "/sdcard/rhythm-beat-acceptance.xml"
    count = 0
    pid = ""

    def adb(*arguments, timeout=20):
        result = subprocess.run([str(args.adb), "-s", args.serial, *map(str, arguments)],
                                capture_output=True, timeout=timeout)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode("utf-8", errors="replace"))
        return result.stdout

    def screenshot(name):
        data = adb("exec-out", "screencap", "-p")
        (output / (name + ".png")).write_bytes(data)
        return struct.unpack(">II", data[16:24])

    def hierarchy(label):
        nonlocal count
        result = adb("shell", "uiautomator", "dump", "--compressed", remote)
        if b"dumped to:" not in result:
            raise RuntimeError("UI hierarchy capture did not complete; refusing a stale XML file")
        data = adb("shell", "cat", remote)
        count += 1
        (output / f"{count:02d}-{label}.xml").write_bytes(data)
        return ET.fromstring(data)

    def tap(x, y):
        adb("shell", "input", "tap", round(x), round(y))
        time.sleep(0.25)

    def find(root, key):
        label = strings[key]
        for node in root.iter("node"):
            if node.get("text") == label or node.get("content-desc") == label:
                bounds = list(map(int, re.findall(r"\d+", node.get("bounds", ""))))
                if len(bounds) == 4 and bounds[3] - bounds[1] >= 40:
                    return node, bounds
        return None

    def click(root, key):
        item = find(root, key)
        if not item:
            raise RuntimeError(f"Visible control missing: {key}")
        _, (x1, y1, x2, y2) = item
        tap((x1 + x2) / 2, (y1 + y2) / 2)

    def scroll_to(key, upward):
        for _ in range(7):
            root = hierarchy("scroll")
            if find(root, key):
                return root
            first, second = (0.76, 0.31) if upward else (0.31, 0.76)
            adb("shell", "input", "swipe", round(width * .72), round(height * first),
                round(width * .72), round(height * second), 650)
        raise RuntimeError(f"Cannot scroll to {key}")

    def open_controls():
        tap(width * .892, height * .338)
        return hierarchy("controls")

    def back():
        adb("shell", "input", "keyevent", 4)
        time.sleep(.3)

    def has(root, value):
        return any(value in node.get("text", "") for node in root.iter("node"))

    def choose_alternate():
        root = scroll_to("controls_first", True)
        nodes = list(root.iter("node"))
        first = next(i for i, node in enumerate(nodes) if node.get("text") == strings["controls_first"])
        spinner = next(node for node in nodes[first + 1:] if node.get("class") == "android.widget.Spinner")
        bounds = list(map(int, re.findall(r"\d+", spinner.get("bounds"))))
        tap((bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2)
        menu = hierarchy("snapshot-menu")
        choices = [node for node in menu.iter("node") if node.get("text") and
                   node.get("class") == "android.widget.CheckedTextView"]
        if len(choices) < 2:
            raise RuntimeError("This probe needs at least two distinct snapshots")
        bounds = list(map(int, re.findall(r"\d+", choices[-1].get("bounds"))))
        tap((bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2)
        return scroll_to("controls_recall", True)

    def macro_values(root):
        values = {}
        for node in root.iter("node"):
            match = re.fullmatch(r"(.+): (-?\d+\.\d+)", node.get("text", ""))
            if match:
                values[match[1]] = float(match[2])
        return values

    try:
        adb("shell", "am", "force-stop", APP)
        adb("shell", "am", "start", "-n", APP + "/.PlayerActivity")
        time.sleep(2)
        pid = adb("shell", "pidof", APP).decode("utf-8").strip()
        width, height = screenshot("startup")
        if width <= height:
            raise RuntimeError("Select a landscape work with snapshots before running this probe")
        tap(width * .783, height * .134)  # Pause initial playback.
        tap(width * .892, height * .134)  # Restart while retaining pause.
        root = open_controls()
        click(root, "beat_settings")
        root = hierarchy("settings")
        enabled = find(root, "beat_enabled")
        if not enabled:
            raise RuntimeError("Beat enable checkbox missing")
        if enabled[0].get("checked") != "true":
            click(root, "beat_enabled")
        root = scroll_to("beat_apply", True)
        click(root, "beat_apply")
        root = scroll_to("beat_timing", False)
        click(root, "beat_settings")  # Collapse details before choosing timing.
        root = hierarchy("grid-enabled")
        spinner = next(node for node in root.iter("node")
                       if node.get("class") == "android.widget.Spinner" and
                       node.get("content-desc") == strings["beat_timing"])
        bounds = list(map(int, re.findall(r"\d+", spinner.get("bounds"))))
        tap((bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2)
        root = hierarchy("mode-menu")
        click(root, "beat_next_beat")
        time.sleep(.7)  # Cross several refresh/layout callbacks.
        back()
        root = open_controls()
        if not any(node.get("text", "").startswith(strings["beat_next_beat"] + " ·")
                   for node in root.iter("node")):
            raise RuntimeError("Next-beat selection did not survive refresh and reopening")
        screenshot("mode-retained")
        print("Next-beat mode retained across refresh and page reopening", flush=True)
        root = choose_alternate()
        before_values = macro_values(root)
        click(root, "controls_recall")
        root = scroll_to("controls_recall", True)
        click(root, "controls_recall")
        back()
        root = open_controls()
        if not has(root, strings["beat_pending"] + " @ 0.500 s"):
            raise RuntimeError("Paused recall did not retain its next-beat target")
        screenshot("paused-pending")
        click(root, "beat_cancel_snapshot")
        root = hierarchy("cancelled")
        if not has(root, strings["beat_cancelled"]):
            raise RuntimeError("Recall cancellation was not reflected by the host")
        root = choose_alternate()
        click(root, "controls_recall")
        back()
        tap(width * .783, height * .134)  # Resume and cross the target beat.
        time.sleep(.8)
        tap(width * .783, height * .134)
        root = open_controls()
        if not has(root, strings["beat_completed"]):
            raise RuntimeError("Resumed recall did not complete")
        screenshot("recall-completed")
        root = scroll_to("controls_recall", True)
        after_values = macro_values(root)
        if not any(key in after_values and value != after_values[key] for key, value in before_values.items()):
            raise RuntimeError("Completed alternate snapshot did not change displayed runtime macro values")
        screenshot("changed-runtime-values")
        logs = adb("logcat", "-d", "--pid=" + pid, "-s", "RhythmPerformance:I", "AndroidRuntime:E")
        (output / "device.log").write_bytes(logs)
        text = logs.decode("utf-8")
        records = re.findall(r"id=(\d+) kind=0 target=\d+ state=(\d+) reason=\d+ due=([\d.]+) seconds=([\d.]+)", text)
        completed = [item for item in records if item[1] == "3"]
        if len(completed) != 1 or float(completed[0][3]) < float(completed[0][2]):
            raise RuntimeError("Expected exactly one completed recall, never before the beat")
        (output / "results.json").write_text(json.dumps({"serial": args.serial, "pid": pid,
            "checks": ["mode_refresh_reopen", "paused_next_beat", "duplicate_request", "cancel", "resume_execution"],
            "completed": completed, "before_values": before_values, "after_values": after_values,
            "scope": "installed APK UI, short functional check"}, indent=4) + "\n", encoding="utf-8")
        print("Android beat UI checks passed", flush=True)
    except Exception as error:
        screenshot("failure")
        if pid:
            (output / "device.log").write_bytes(adb("logcat", "-d", "--pid=" + pid,
                                                    "-s", "RhythmPerformance:I", "AndroidRuntime:E"))
        (output / "failure.txt").write_text(str(error) + "\n", encoding="utf-8")
        raise


if __name__ == "__main__":
    main()
