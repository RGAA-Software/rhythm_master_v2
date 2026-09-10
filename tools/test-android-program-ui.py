"""Short touch acceptance of Android's persisted performance program.

Preserves installed app data and any existing program by appending test entries.
Leaves the augmented saved program and a prepared queue available for inspection.
Requires a landscape selected work. No endurance or thermal claims.
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

from android_view_bounds import require_player_focus, reject_call_ui


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--adb", type=Path, default=Path("D:/android/sdk/platform-tools/adb.exe"))
    parser.add_argument("--locale", choices=("en-US", "zh-CN"), default="zh-CN")
    parser.add_argument("--reopen-only", action="store_true")
    parser.add_argument("--hard-cut-head", action="store_true",
                        help="Set the first entry's draft duration to zero through UI; do not save")
    parser.add_argument("--mixed-draft", action="store_true",
                        help="Prepare a temporary landscape/portrait/music list through UI; do not save")
    args = parser.parse_args()
    if args.hard_cut_head and not args.reopen_only:
        parser.error("--hard-cut-head requires --reopen-only to preserve the saved program")
    if args.mixed_draft and (not args.reopen_only or args.hard_cut_head):
        parser.error("--mixed-draft requires --reopen-only and excludes --hard-cut-head")
    output = ROOT / "out/android-program-ui" / uuid.uuid4().hex
    output.mkdir(parents=True)
    print(f"Device evidence: {output}", flush=True)
    strings = {item.attrib["name"]: item.text for item in ET.parse(
        ROOT / "platforms/android/res" / ("values-zh-rCN" if args.locale == "zh-CN" else "values") /
        "strings.xml").getroot()}
    sequence = 0

    def adb(*arguments, timeout=25, check=True):
        if arguments[:2] == ("shell", "input"):
            require_player_focus(adb)
        if arguments[:3] == ("shell", "am", "start"):
            reject_call_ui(adb)
        result = subprocess.run([str(args.adb), "-s", args.serial, *map(str, arguments)],
                                capture_output=True, timeout=timeout)
        if check and result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode("utf-8", errors="replace"))
        return result.stdout if check else result

    def shot(name):
        data = adb("exec-out", "screencap", "-p")
        (output / (name + ".png")).write_bytes(data)
        return struct.unpack(">II", data[16:24])

    def hierarchy(name):
        nonlocal sequence
        remote = "/sdcard/rhythm-program-acceptance.xml"
        dumped = adb("shell", "uiautomator", "dump", "--compressed", remote)
        if b"dumped to:" not in dumped:
            raise RuntimeError("UI dump not completed; rejecting stale hierarchy")
        data = adb("shell", "cat", remote)
        sequence += 1
        (output / f"{sequence:02d}-{name}.xml").write_bytes(data)
        return ET.fromstring(data)

    def bounds(node):
        return list(map(int, re.findall(r"\d+", node.get("bounds", ""))))

    def find(root, text):
        for node in root.iter("node"):
            box = bounds(node)
            if (node.get("text") == text or node.get("content-desc") == text) and len(box) == 4 and box[3] - box[1] >= 35:
                return node
        return None

    def tap(x, y):
        adb("shell", "input", "tap", round(x), round(y))
        time.sleep(.3)

    def touch(node):
        if node is None:
            raise RuntimeError("Expected visible control missing")
        x1, y1, x2, y2 = bounds(node)
        tap((x1 + x2) / 2, (y1 + y2) / 2)

    def locate(key, lower=False):
        for _ in range(6):
            root = hierarchy(key)
            node = find(root, strings[key])
            if node is not None:
                return node
            scroll = next((n for n in root.iter("node") if n.get("scrollable") == "true"), None)
            if scroll is None:
                break
            x1, y1, x2, y2 = bounds(scroll)
            top, bottom = y1 + (y2 - y1) * .15, y1 + (y2 - y1) * .85
            start, end = (bottom, top) if lower else (top, bottom)
            adb("shell", "input", "swipe", round((x1 + x2) / 2), round(start),
                round((x1 + x2) / 2), round(end), 450)
        raise RuntimeError(f"Visible control missing: {key}")

    def action(key, lower=False):
        touch(locate(key, lower))

    def saved():
        data = adb("exec-out", "run-as", APP, "cat", "files/performance/list.json")
        return json.loads(data)

    def startup():
        adb("shell", "am", "force-stop", APP)
        adb("shell", "am", "start", "-n", APP + "/.PlayerActivity")
        time.sleep(2)
        width, height = shot("startup")
        if width <= height:
            raise RuntimeError("Select a landscape work before this touch test")
        tap(width * .783, height * .134)  # Pause. Main frame statistics keep changing, so open modal before dumping.
        tap(width * .783, height * .458)  # Performance program in the middle of the scene row.
        return hierarchy("program-open")

    exists = adb("shell", "run-as", APP, "test", "-f", "files/performance/list.json", check=False)
    original = adb("exec-out", "run-as", APP, "cat", "files/performance/list.json") if exists.returncode == 0 else None
    baseline = json.loads(original) if original is not None else None
    count = len(baseline["entries"]) if baseline else 0
    if count > 13 and not args.reopen_only:
        raise RuntimeError("This check needs three free program entries; existing data is retained")
    if baseline:
        (output / "original.json").write_bytes(original)
    try:
        if args.reopen_only:
            if baseline is None:
                raise RuntimeError("An existing program is required")
            snapshot = baseline
        else:
            startup()
            for effect in ("aureate_vortex", "crystal_choir"):
                action("program_add")
                title = json.loads((ROOT / "content/templates" / effect / "manifest.json").read_text(encoding="utf-8"))["titles"][args.locale]
                root = hierarchy("effect-picker")
                touch(next((node for node in root.iter("node") if node.get("text", "").startswith(title + " · ")), None))
            action("program_duplicate")
            action("program_up")
            slider = locate("program_duration", True)
            x1, y1, x2, y2 = bounds(slider)
            tap(x1 + .5 * (x2 - x1), (y1 + y2) / 2)
            root = hierarchy("duration-edited")
            touch(find(root, strings["beat_immediate"]))
            touch(find(hierarchy("mode-menu"), strings["beat_next_bar"]))
            action("program_apply", True)
            action("program_save")
            time.sleep(.5)
            snapshot = saved()
            (output / "saved.json").write_text(json.dumps(snapshot, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
            assert len(snapshot["entries"]) == count + 3, "touch additions not persisted"
            first, duplicate, second = snapshot["entries"][-3:]
            assert first["work"]["content_id"] == "official.templates.aureate_vortex"
            assert second["work"]["content_id"] == "official.templates.crystal_choir"
            assert duplicate["work"] == second["work"] and len({first["id"], duplicate["id"], second["id"]}) == 3
            assert duplicate["id"] > second["id"], "Move up did not reorder duplicate"
            assert duplicate["quantization"] == "bar" and 2.3 <= duplicate["transition_seconds"] <= 2.7, "UI settings were overwritten or not applied"
            if baseline:
                assert snapshot["entries"][:count] == baseline["entries"], "existing program entries changed"
            action("program_remove")
            action("program_reopen")
            time.sleep(.5)
            shot("reopened")
        startup()
        assert saved() == snapshot, "restart changed persisted program"
        mixed = ("aureate_vortex", "porcelain_bloom", "chromatic_loom")
        if args.mixed_draft:
            for _ in snapshot["entries"]:
                action("program_remove", True)
            for effect in mixed:
                action("program_add")
                root = hierarchy("choose-effect")
                # Use actual catalog filters; synthesizing keys through the
                # installed Chinese IME can cancel a composing search string.
                if effect != "aureate_vortex":
                    filter_key, value = ("catalog_tier", "catalog_advanced")
                    touch(find(root, strings[filter_key]))
                    touch(find(hierarchy("catalog-filter"), strings[value]))
                title = json.loads((ROOT / "content/templates" / effect / "manifest.json").read_text(encoding="utf-8"))["titles"][args.locale]
                for _ in range(20):
                    root = hierarchy("filtered-effect")
                    row = next((n for n in root.iter("node") if n.get("text", "").startswith(title + " · ")), None)
                    if row is not None:
                        touch(row)
                        break
                    scroll = next(n for n in root.iter("node") if n.get("class") == "android.widget.ListView")
                    x1, y1, x2, y2 = bounds(scroll)
                    adb("shell", "input", "swipe", (x1 + x2) // 2, y2 - 30,
                        (x1 + x2) // 2, y1 + 30, 350)
                else:
                    raise RuntimeError("Built-in mixed program effect not found: " + effect)
                locate("program_apply", True)
                root = hierarchy("entry-timing")
                mode_labels = {strings[key] for key in ("beat_immediate", "beat_next_beat", "beat_next_bar")}
                touch(next(n for n in root.iter("node") if n.get("text") in mode_labels))
                touch(find(hierarchy("immediate-mode"), strings["beat_immediate"]))
                action("program_apply", True)
            assert saved() == snapshot, "temporary mixed program must not change saved entries"
        if args.hard_cut_head:
            locate("program_up", True)
            root = hierarchy("select-head")
            touch(next(n for n in root.iter("node") if n.get("class") == "android.widget.Spinner"))
            root = hierarchy("program-entries")
            touch(next(n for n in root.iter("node")
                       if n.get("class") == "android.widget.CheckedTextView" and n.get("text", "").startswith("1. ")))
            x1, y1, x2, y2 = bounds(locate("program_duration", True))
            tap(x1 + 1, (y1 + y2) / 2)
            action("program_apply", True)
            assert saved() == snapshot, "draft hard-cut edit must not alter the saved program"
        action("program_prepare")
        time.sleep(2)
        action("scene_queue", True)
        root = hierarchy("prepared-queue")
        spinner = next(n for n in root.iter("node") if n.get("class") == "android.widget.Spinner")
        touch(spinner)
        root = hierarchy("queue-rows")
        rows = [n.get("text") for n in root.iter("node") if n.get("class") == "android.widget.CheckedTextView"]
        expected_count = len(mixed) if args.mixed_draft else len(snapshot["entries"])
        assert len(rows) == expected_count, "draft was not restored into queue"
        if args.mixed_draft:
            for row, effect in zip(rows, mixed):
                title = json.loads((ROOT / "content/templates" / effect / "manifest.json").read_text(encoding="utf-8"))["titles"][args.locale]
                assert row.startswith(title), "mixed program row identity mismatch"
        if rows:
            assert strings["scene_ready"] in rows[0], "saved head work was not prepared"
            assert not any(strings["scene_failed"] in row for row in rows), "program contains an unresolved work"
        adb("shell", "input", "keyevent", 4)
        shot("prepared")
        (output / "result.json").write_text(json.dumps({"serial": args.serial, "entries": len(snapshot["entries"]),
                "queue_rows": rows, "reopen_only": args.reopen_only,
                "mixed_draft": args.mixed_draft,
                "hard_cut_head_draft": args.hard_cut_head, "status": "passed"}, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
        print("Android touch program restart/reopen/prepare passed" if args.reopen_only else
              "Android touch program add/duplicate/order/settings/save/reopen/restart/prepare passed", flush=True)
    except Exception:
        shot("failure")
        (output / "logcat.txt").write_bytes(adb("logcat", "-d", "--pid=" + adb("shell", "pidof", APP).decode().strip(), "-t", 150))
        raise


if __name__ == "__main__":
    main()
