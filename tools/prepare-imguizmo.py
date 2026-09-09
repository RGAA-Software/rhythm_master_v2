"""Extract focused ImGuizmo sources from the verified vcpkg download.

The installed non-docking ImGui ABI differs from Studio's validated docking
fork. This source exception keeps the existing ImGui and avoids a second copy.
Run vcpkg install imguizmo:x64-windows first; this tool does not fetch or upgrade.
"""

import argparse
import hashlib
import json
from pathlib import Path
import tarfile


ARCHIVE_SHA512 = (
    "c707e22d3caffcf06c5aab33201ebc4fe352d07e893ee8623db374a7e848bc18d031fb"
    "1904e7ddae5573443783bd70ccf787cd7029074a1de2c7e9fe12a54733"
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpkg", type=Path, default=Path("C:/source/vcpkg"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    archive = args.vcpkg / "downloads/CedricGuillemet-ImGuizmo-1.10.tar.gz"
    if hashlib.sha512(archive.read_bytes()).hexdigest() != ARCHIVE_SHA512:
        raise ValueError("ImGuizmo vcpkg archive checksum mismatch")
    files = {"src/ImGuizmo.cpp": "third_party/sources/imguizmo/ImGuizmo.cpp",
             "src/ImGuizmo.h": "third_party/sources/imguizmo/ImGuizmo.h",
             "LICENSE": "third_party/notices/imguizmo/LICENSE.txt"}
    records = []
    with tarfile.open(archive) as source:
        revision = source.pax_headers.get("comment", "1.10")
        for name, destination in files.items():
            member = source.getmember("ImGuizmo-1.10/" + name)
            if not member.isfile():
                raise ValueError("ImGuizmo source must be a regular file")
            with source.extractfile(member) as stream:
                data = stream.read()
            path = root / destination
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists() or path.read_bytes() != data:
                path.write_bytes(data)
            records.append({"source": name, "target": destination,
                            "sha256": hashlib.sha256(data).hexdigest()})
    record = {
        "source_url": "https://github.com/CedricGuillemet/ImGuizmo",
        "revision": revision, "tag": "1.10", "vcpkg_version": "1.10",
        "archive_sha512": ARCHIVE_SHA512, "license": "MIT",
        "notice": files["LICENSE"], "imported_files": records,
        "modifications": [],
        "target_modules": ["studio_imguizmo", "src/studio_ui/gizmo.cpp"],
        "decision": "Installed x64-windows binary uses ImGui 1.91.9 non-docking; "
                    "Studio uses 1.91.9b docking. sizeof/offset ABI probe confirms mismatch. "
                    "Compile focused unmodified source with Studio's existing ImGui. "
                    "No GraphEditor, sequencer, whole engine or second ImGui imported.",
        "validation": "out/p4-imgui-abi-detail.log; out/p4-gizmo-tests-axis-contract.log",
        "validated_scope": "Windows private adapter: actual ImGui mouse input, perspective/orthographic, "
                           "world translation through rotated nonuniform parent, local/world rotation, "
                           "local scale, cancellation and viewport bounds. No complete Studio 3D workflow yet.",
        "limits": ["World scale explicitly rejects; upstream silently forces local scale.",
                   "Adapter bounds initial hits independently of upstream SetRect mYMax typo.",
                   "No Android editor or whole-engine dependency."],
        "design_reference": {"source_url": "https://github.com/godotengine/godot",
                             "revision": "f62fdbde15035c5576dad93e586201f4d41ef0cb",
                             "file": "scene/3d/node_3d.cpp", "symbol": "Node3D::set_global_transform",
                             "license": "MIT", "notice": "third_party/notices/godot/LICENSE.txt",
                             "reuse": "Study inverse parent times global transform for local writeback; "
                                      "use existing GLM-backed project matrix contracts. No Godot types copied."},
        "outbound_license": "not selected; upstream contributions retain MIT"}
    for destination in ["provenance/imguizmo.json", "third_party/notices/imguizmo/PROVENANCE.json"]:
        path = root / destination
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(record, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(f"Verified ImGuizmo {revision}: {len(records)} focused files")


if __name__ == "__main__":
    main()
