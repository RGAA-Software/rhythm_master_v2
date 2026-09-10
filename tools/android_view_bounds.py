"""Read fresh native view bounds without waiting for an animated window to idle.

The Activity dump is scoped to our app. Geometry is for its unscrolled standard
View hierarchy, not transformed views or arbitrary scrolled containers.
"""

import re

APP = "org.rhythmmaster.player"
ROW = re.compile(r"^( *)([\w.$]+)\{\S+ ([VGI])\S* \S+ (-?\d+),(-?\d+)-(-?\d+),(-?\d+)(.*)\}$")


def parse_views(text):
    if "ACTIVITY " + APP + "/.PlayerActivity" not in text:
        raise ValueError("Player Activity dump missing")
    section = text.split("    View Hierarchy:", 1)
    if len(section) != 2:
        raise ValueError("Fresh native view hierarchy missing")
    stack = []
    views = {}
    for line in section[1].splitlines():
        match = ROW.match(line)
        if not match:
            continue
        indent, kind, visibility, left, top, right, bottom, suffix = match.groups()
        depth = len(indent)
        while stack and stack[-1][0] >= depth:
            stack.pop()
        parent = stack[-1][1] if stack else None
        offset_x, offset_y = parent["bounds"][:2] if parent else (0, 0)
        bounds = [int(left) + offset_x, int(top) + offset_y,
                  int(right) + offset_x, int(bottom) + offset_y]
        visible = bounds.copy()
        if parent:
            clip = parent["visible_bounds"]
            visible = [max(bounds[0], clip[0]), max(bounds[1], clip[1]),
                       min(bounds[2], clip[2]), min(bounds[3], clip[3])]
        if visibility != "V":
            visible = [0, 0, 0, 0]
        record = {"class": kind, "bounds": bounds, "visible_bounds": visible}
        stack.append((depth, record))
        name = re.search(r"(?:app:)?id/(player_\w+)", suffix)
        if name:
            if name[1] in views:
                raise ValueError("Ambiguous Player view ID")
            views[name[1]] = record
    return views


def read_views(adb, destination=None):
    text = adb("shell", "dumpsys", "activity", APP + "/.PlayerActivity").decode("utf-8")
    if destination is not None:
        destination.write_text(text, encoding="utf-8")
    return parse_views(text)


def center(views, name):
    view = views[name]
    left, top, right, bottom = view["visible_bounds"]
    if right <= left or bottom - top < 30:
        raise ValueError("Player control is not reachable: " + name)
    return (left + right) / 2, (top + bottom) / 2


def require_unclipped(views, name):
    view = views[name]
    if view["bounds"] != view["visible_bounds"] or view["bounds"][3] <= view["bounds"][1]:
        raise ValueError("Player control is clipped: " + name)
    return view["bounds"]


def current_focus(adb):
    text = adb("shell", "dumpsys", "window").decode("utf-8")
    top = re.search(r"mTopFocusedDisplayId=(\d+)", text)
    display = None
    candidates = []
    for line in text.splitlines():
        match = re.search(r"Display: mDisplayId=(\d+)", line)
        if match:
            display = match[1]
        if "mCurrentFocus=" in line:
            if top and display == top[1]:
                return line.strip()
            candidates.append(line.strip())
    # Older dumps expose one global focus without display metadata. Ambiguous
    # multi-display dumps must stop input instead of guessing another display.
    return candidates[0] if not top and len(candidates) == 1 else ""


def require_player_focus(adb):
    if APP + "/" not in current_focus(adb):
        raise RuntimeError("Player lost foreground ownership; device input stopped")


def reject_call_ui(adb):
    if "incall" in current_focus(adb).lower():
        raise RuntimeError("Call UI owns the device; leave it untouched")
