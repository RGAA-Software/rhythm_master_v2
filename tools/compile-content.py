"""Compile data-authored templates with host protoc; never construct graphs in C++."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

protoc, schema, source, destination = sys.argv[1:]
schema = Path(schema)
source = Path(source)
destination = Path(destination)
graph = subprocess.run(
    [protoc, "--encode=rhythm.schema.GraphProject", f"--proto_path={schema.parent}", schema.name],
    input=(source / "graph.textproto").read_bytes(), capture_output=True, check=True,
).stdout
editor = (source / "editor.json").read_bytes()
manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
manifest["graph_sha256"] = hashlib.sha256(graph).hexdigest()
manifest["editor_sha256"] = hashlib.sha256(editor).hexdigest()
destination.mkdir(parents=True, exist_ok=True)
(destination / "graph.pb").write_bytes(graph)
(destination / "editor.json").write_bytes(editor)
if (source / "presets.json").is_file():
    (destination / "presets.json").write_bytes((source / "presets.json").read_bytes())
if (source / "thumbnail.rgba").is_file():
    thumbnail = (source / "thumbnail.rgba").read_bytes()
    if len(thumbnail) != 256 * 144 * 4:
        raise ValueError("Catalog thumbnail must be 256x144 RGBA")
    (destination / "thumbnail.rgba").write_bytes(thumbnail)
    if (source / "thumbnail.json").is_file():
        (destination / "thumbnail.json").write_bytes((source / "thumbnail.json").read_bytes())
else:
    (destination / "thumbnail.rgba").unlink(missing_ok=True)
    (destination / "thumbnail.json").unlink(missing_ok=True)
(destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
