"""Snapshot the read-only experiment SDK and retain its supplied notices."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--installed", type=Path, default=Path("C:/source/vcpkg/installed"))
    args = parser.parse_args()
    installed = args.installed.resolve()
    status = (installed / "vcpkg/status").read_text(encoding="utf-8")
    records = {}
    features = {}
    for paragraph in status.split("\n\n"):
        fields = dict(line.split(": ", 1) for line in paragraph.splitlines()
                      if not line.startswith(" ") and ": " in line)
        if "Package" in fields:
            key = (fields["Package"], fields["Architecture"])
            if "Feature" in fields:
                features.setdefault(key, []).append(fields)
            else:
                records[key] = fields
    for key, entries in features.items():
        records[key]["Installed-Features"] = [entry["Feature"] for entry in entries]
        records[key]["Depends"] = ", ".join(
            [records[key].get("Depends", "")] + [entry.get("Depends", "") for entry in entries])
    pending = [("protobuf", "x64-windows"), ("nlohmann-json", "x64-windows"),
               ("freetype", "x64-windows"), ("protobuf", "arm64-android"),
               ("nlohmann-json", "arm64-android")]
    selected = {}
    while pending:
        key = pending.pop()
        if key in selected:
            continue
        fields = records[key]
        selected[key] = fields
        for dependency in fields.get("Depends", "").split(","):
            match = re.match(r"\s*([\w-]+)(?::([\w-]+))?", dependency)
            if match:
                pending.append((match[1], match[2] or key[1]))
    inventory = []
    for (package, triplet), fields in sorted(selected.items()):
        share = installed / triplet / "share" / package
        notice_dir = ROOT / "third_party/notices/sdk" / triplet / package
        notice_dir.mkdir(parents=True, exist_ok=True)
        for name in ("copyright", "vcpkg.spdx.json"):
            if (share / name).is_file():
                shutil.copyfile(share / name, notice_dir / name)
        lists = list((installed / "vcpkg/info").glob(f"{package}_*_{triplet}.list"))
        if len(lists) != 1:
            raise RuntimeError(f"Ambiguous SDK file list: {package}:{triplet}")
        files = []
        for relative in sorted(lists[0].read_text(encoding="utf-8").splitlines()):
            path = installed / relative
            if not path.resolve().is_relative_to(installed):
                raise RuntimeError("SDK file escapes its root")
            if path.is_file():
                files.append({"path": relative, "sha256": digest(path)})
        inventory.append({"package": package, "triplet": triplet,
                          "version": fields.get("Version", fields.get("Version-Semver", "")),
                          "port_version": fields.get("Port-Version", "0"),
                          "abi": fields.get("Abi", ""), "dependencies": fields.get("Depends", ""),
                          "features": fields.get("Installed-Features", []),
                          "files": files})
    source_notices = {
        "sdl": "LICENSE.txt", "imgui": "LICENSE.txt", "node_editor": "LICENSE",
        "picosha2": "LICENSE", "bgfx": "LICENSE", "bx": "LICENSE", "bimg": "LICENSE",
        "bx/include/tinystl": "LICENSE", "bgfx/3rdparty/directx-headers": "LICENSE",
    }
    for directory, name in source_notices.items():
        target = ROOT / "third_party/notices/sources" / directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / "third_party/sources" / directory / name, target)
    output = {"purpose": "Experiment SDK snapshot, not a production dependency lock",
              "installed_root": installed.as_posix(), "packages": inventory}
    (ROOT / "third_party/sdk_inventory.json").write_text(
        json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Recorded {len(inventory)} SDK packages and retained supplied notices")


if __name__ == "__main__":
    main()
