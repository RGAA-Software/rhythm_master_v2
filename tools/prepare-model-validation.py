"""Create an isolated Studio acceptance project from the pinned Cesium GLB fixture."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--protoc", type=Path, default=Path(
        "C:/source/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe"))
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "out") or output.exists():
        raise ValueError("Use a fresh directory inside this project's out directory")
    source = ROOT / "content/templates/rotating_cube"
    fixture = ROOT / "third_party/assets/khronos-box/Box.glb"
    notices = ROOT / "third_party/notices/khronos-box"
    records = []
    output.mkdir(parents=True)
    for path, media_type in [(fixture, "model/gltf-binary")] + [
            (path, "text/plain") for path in sorted(notices.iterdir()) if path.is_file()]:
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        target = output / "assets/sha256" / digest[:2] / digest
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        records.append({"sha256": digest, "bytes": len(data), "media_type": media_type})
    graph = (source / "graph.textproto").read_text(encoding="utf-8")
    graph = graph.replace('schema_version: 2\n', 'schema_version: 4\n', 1)
    graph = graph.replace('official-rotating_cube', 'validation-cesium-box')
    graph = graph.replace('type_key: "geometry.cube" schema_version: 1',
                          'type_key: "geometry.glb" schema_version: 1\n'
                          '    properties { key: "asset" value { asset_sha256: "'
                          + records[0]["sha256"] + '" } }')
    schema = ROOT / "src/project_io/schema/graph.proto"
    compiled = subprocess.run([str(args.protoc), "--encode=rhythm.schema.GraphProject",
                               f"--proto_path={schema.parent}", schema.name],
                              input=graph.encode(), capture_output=True, check=True).stdout
    editor = (source / "editor.json").read_bytes()
    revision = hashlib.sha256(compiled).hexdigest()[:16] + "-1-0"
    directory = output / "revisions" / revision
    directory.mkdir(parents=True)
    (directory / "graph.pb").write_bytes(compiled)
    (directory / "editor.json").write_bytes(editor)
    manifest = {"format": "rhythm.project", "manifest_version": 1,
                "revision_id": revision, "project_id": "validation-cesium-box",
                "graph_revision": 0, "title": "Cesium Box — CC-BY-4.0 / GLB validation",
                "graph_sha256": hashlib.sha256(compiled).hexdigest(),
                "editor_sha256": hashlib.sha256(editor).hexdigest(), "assets": records}
    (directory / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=4), encoding="utf-8")
    (output / "CURRENT").write_text(revision, encoding="utf-8")
    # Human-readable attribution also remains beside the project. The same
    # notices are embedded assets, retained by the normal standalone publisher.
    (output / "ATTRIBUTION.txt").write_bytes((notices / "ATTRIBUTION.txt").read_bytes())
    print(output)


if __name__ == "__main__":
    main()
