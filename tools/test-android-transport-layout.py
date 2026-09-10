"""Exercise installed Player transport in both authored orientations and fullscreen.

Short functional check only. Does not change system settings or saved programs.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET
import zipfile

from android_view_bounds import require_player_focus, reject_call_ui, APP, center, read_views, require_unclipped

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--adb", default="D:/android/sdk/platform-tools/adb.exe")
    parser.add_argument("--apk", type=Path, default=ROOT / "out/android-arm64-release/apk/rhythm-player-release.apk")
    args = parser.parse_args()
    output = ROOT / "out/android-transport-layout" / uuid.uuid4().hex
    output.mkdir(parents=True)
    print("Transport layout evidence:", output, flush=True)

    def adb(*command):
        if command[:2] == ("shell", "input"):
            require_player_focus(adb)
        result = subprocess.run([args.adb, "-s", args.serial, *map(str, command)],
                                capture_output=True, timeout=40)
        if result.returncode:
            (output / "adb-failure.txt").write_bytes(result.stdout + result.stderr)
            raise RuntimeError("ADB failed: " + repr(command))
        return result.stdout

    def shot(name):
        data = adb("exec-out", "screencap", "-p")
        (output / (name + ".png")).write_bytes(data)
        return struct.unpack(">II", data[16:24])

    def views(name):
        return read_views(adb, output / (name + ".txt"))

    def tap(x, y):
        adb("shell", "input", "tap", round(x), round(y))
        time.sleep(.35)

    def click(name):
        tap(*center(views("before-" + name), name))

    def picker(name):
        remote = "/sdcard/rhythm-transport-layout.xml"
        result = adb("shell", "uiautomator", "dump", "--compressed", remote)
        if b"dumped to:" not in result:
            raise RuntimeError("Fresh picker dump unavailable")
        data = adb("shell", "cat", remote)
        (output / (name + ".xml")).write_bytes(data)
        return ET.fromstring(data)

    def touch(node):
        x1, y1, x2, y2 = map(int, re.findall(r"\d+", node.attrib["bounds"]))
        tap((x1 + x2) / 2, (y1 + y2) / 2)

    def saved():
        # A missing saved program is distinct from an ADB read failure.
        exists = adb("shell", "run-as", APP, "sh", "-c",
                     "'if test -f files/performance/list.json; then echo present; else echo absent; fi'")
        if exists.strip() == b"absent":
            return None
        if exists.strip() != b"present":
            raise RuntimeError("Saved program probe failed")
        return adb("exec-out", "run-as", APP, "cat", "files/performance/list.json")

    reject_call_ui(adb)
    before = saved()
    built_hash = hashlib.sha256(args.apk.read_bytes()).hexdigest()
    installed = adb("shell", "pm", "path", APP).decode().strip().removeprefix("package:")
    installed_hash = adb("shell", "sha256sum", installed).decode().split()[0]
    if installed_hash != built_hash:
        raise RuntimeError("Installed APK is not the current build")
    with zipfile.ZipFile(args.apk) as archive:
        catalog = {entry["id"]: entry for entry in json.loads(archive.read("assets/effects/catalog.json"))}
    record = {"apk_sha256": built_hash, "serial": args.serial, "cases": [], "status": "running"}
    try:
        adb("shell", "am", "force-stop", APP)
        adb("shell", "am", "start", "-n", APP + "/.PlayerActivity")
        time.sleep(4)
        for index, name in enumerate(("aureate_vortex", "porcelain_bloom", "aureate_vortex")):
            tag = str(index) + "-" + name
            entry = catalog[name]
            click("player_choose_effect")
            tree = picker(tag + "-picker")
            field = next(n for n in tree.iter("node") if n.get("resource-id") == "android:id/search_src_text")
            touch(field)
            time.sleep(.7)
            adb("shell", "input", "keyevent", 4)
            words = re.findall(r"[a-zA-Z]+", entry["titles"]["en-US"].lower())
            query = min(words, key=lambda word: (sum(word in item["titles"]["en-US"].lower()
                                                     for item in catalog.values()), -len(word)))
            for character in query:
                adb("shell", "input", "text", character)
                time.sleep(.1)
            tree = picker(tag + "-filtered")
            title = entry["titles"]["zh-CN"]
            choices = [n for n in tree.iter("node") if n.get("text", "").startswith(title + " · ")]
            if len(choices) != 1:
                raise RuntimeError("Catalog selection ambiguous")
            touch(choices[0])
            time.sleep(2)
            size = shot(tag + "-playing")
            portrait = entry["canvas"]["height"] > entry["canvas"]["width"]
            if (size[1] > size[0]) != portrait:
                raise RuntimeError("Scene did not select its authored orientation")
            click("player_pause")
            layout = views(tag + "-paused")
            bounds = {key: require_unclipped(layout, key) for key in
                      ("player_music_time", "player_music_position", "player_pause", "player_choose_effect")}
            shot(tag + "-paused")
            x1, y1, x2, y2 = bounds["player_music_position"]
            tap(x1 + (x2 - x1) * .65, (y1 + y2) / 2)
            time.sleep(.6)
            shot(tag + "-seek")
            click("player_fullscreen")
            time.sleep(1)
            shot(tag + "-fullscreen")
            adb("shell", "input", "keyevent", 4)
            time.sleep(1)
            returned = views(tag + "-returned")
            for key in bounds:
                require_unclipped(returned, key)
            shot(tag + "-returned")
            click("player_pause")
            time.sleep(.8)
            shot(tag + "-resumed")
            record["cases"].append({"effect": name, "package_sha256": entry["sha256"],
                                    "screen": size, "transport_bounds": bounds,
                                    "fullscreen_return_bounds": {key: returned[key] for key in bounds}})
            print(tag, "transport fully visible before/after fullscreen", flush=True)
        adb("shell", "input", "keyevent", 3)
        time.sleep(.5)
        adb("shell", "am", "start", "-n", APP + "/.PlayerActivity")
        time.sleep(1)
        resumed = views("background-return")
        require_unclipped(resumed, "player_music_time")
        shot("background-return")
        record["saved_program_unchanged"] = saved() == before
        if not record["saved_program_unchanged"]:
            raise RuntimeError("Transport test changed saved program")
        record["status"] = "bounds_passed_visual_review_pending"
    except Exception as error:
        record["status"] = "failed"
        record["error"] = str(error)
        shot("failure")
        raise
    finally:
        (output / "results.json").write_text(json.dumps(record, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(output, flush=True)


if __name__ == "__main__":
    main()
