"""Compile data-authored templates with host protoc; never construct graphs in C++."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import content_identity

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
total_asset_bytes = 0
seen_assets = set()
for record in manifest.get("assets", []):
    digest = record["sha256"]
    if not re.fullmatch(r"[0-9a-f]{64}", digest) or digest in seen_assets:
        raise ValueError("Invalid or duplicate template asset identity")
    seen_assets.add(digest)
    relative = Path("assets/sha256") / digest[:2] / digest
    source_asset = source / relative
    if not source_asset.resolve().is_relative_to((source / "assets").resolve()):
        raise ValueError("Template asset escapes its source directory")
    size = source_asset.stat().st_size
    if size != record["bytes"] or size > 8 * 1024 * 1024 - total_asset_bytes:
        raise ValueError("Template asset byte budget")
    total_asset_bytes += size
    data = source_asset.read_bytes()
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError("Template asset hash mismatch")
    target_asset = destination / relative
    target_asset.parent.mkdir(parents=True, exist_ok=True)
    target_asset.write_bytes(data)
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
content_identity.write_compiled_identity(source, destination)
