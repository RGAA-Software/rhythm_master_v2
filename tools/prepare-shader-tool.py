"""Capture the pinned legacy shader tool's focused sources and build settings.

Reads the reference checkout only. The source archive and inventory allow later
builds to use this repository's copy without accessing that checkout again.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
CONDITION = "'$(Configuration)|$(Platform)'=='Release|x64'"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def capture(reference):
    legacy = json.loads((ROOT / "provenance/shaderc_host.json").read_text(encoding="utf-8"))
    projects = reference / "cmake-build-qt6/generated/bgfx_tools/build/projects/vs2022"
    modules = {}
    selected = set()
    search_roots = set()
    for name, expected in legacy["build_projects_sha256"].items():
        project = projects / (name + ".vcxproj")
        data = project.read_bytes()
        if digest(data) != expected:
            raise ValueError(f"Reference build settings changed: {name}")
        tree = ET.fromstring(data)
        group = next(e for e in tree.findall("m:ItemDefinitionGroup", NS)
                     if e.get("Condition") == CONDITION)
        compile_settings = group.find("m:ClCompile", NS)

        def values(field):
            text = compile_settings.findtext("m:" + field, "", NS)
            return [value for value in text.split(";") if value and not value.startswith("%(")]

        def relative(value):
            path = (project.parent / value).resolve()
            return path.relative_to(reference).as_posix()

        sources = []
        for item in tree.findall(".//m:ClCompile[@Include]", NS):
            source = relative(item.get("Include"))
            if digest((reference / source).read_bytes()) != legacy["source_files"][source]:
                raise ValueError(f"Reference source changed: {source}")
            selected.add(source)
            search_roots.add((reference / source).parent)
            excluded = any(e.text == "true" and e.get("Condition") in (None, CONDITION)
                           for e in item.findall("m:ExcludedFromBuild", NS))
            if not excluded:
                sources.append(source)
        includes = [relative(value) for value in values("AdditionalIncludeDirectories")]
        search_roots.update(reference / value for value in includes)
        options = compile_settings.findtext("m:AdditionalOptions", "", NS)
        options = options.replace("%(AdditionalOptions)", "").split()
        modules[name] = {"sources": sources, "includes": includes,
                         "definitions": values("PreprocessorDefinitions"), "options": options,
                         "dependencies": [Path(e.get("Include")).stem
                                          for e in tree.findall(".//m:ProjectReference", NS)]}
    # Preserve included fragments and their per-file notices, not upstream tests,
    # prebuilt libraries, application assets or unrelated source trees.
    suffixes = {".h", ".hpp", ".hpp11", ".hxx", ".inl", ".inc", ".def"}
    for directory in search_roots:
        if directory.is_dir():
            for path in directory.rglob("*"):
                if path.is_file() and path.suffix.lower() in suffixes:
                    selected.add(path.relative_to(reference).as_posix())
    selected.update(item["source"] for item in legacy["notices"])
    revision = legacy["source_snapshot_revision"]
    tracked = subprocess.run(["git", "-C", str(reference), "ls-tree", "-r", "--name-only",
                              revision, "--", "3rd"], check=True, capture_output=True,
                             encoding="utf-8").stdout.splitlines()
    changed = subprocess.run(["git", "-C", str(reference), "diff", "--name-only", revision,
                              "--", "3rd"], check=True, capture_output=True,
                             encoding="utf-8").stdout.splitlines()
    if selected - set(tracked) or selected.intersection(changed):
        raise ValueError("Selected source/header files must match the recorded Git snapshot")
    archive = ROOT / "third_party/source_archives/shaderc-1.19.157.zip"
    archive.parent.mkdir(parents=True, exist_ok=True)
    inventory = {}
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for name in sorted(selected):
            data = (reference / name).read_bytes()
            inventory[name] = digest(data)
            entry = zipfile.ZipInfo(name, (2026, 9, 9, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            bundle.writestr(entry, data)
    record = {"source_snapshot_revision": legacy["source_snapshot_revision"],
              "legacy_provenance": "provenance/shaderc_host.json",
              "archive": archive.relative_to(ROOT).as_posix(),
              "archive_sha256": digest(archive.read_bytes()),
              "modules": modules, "files": inventory,
              "modifications": ["Translate the pinned Release x64 source lists and settings to CMake/Ninja; add deterministic compiler/linker flags. No upstream source edits."],
              "scope": "Standalone Windows host tool; no graphics backend replacement"}
    (ROOT / "provenance/shaderc_source_build.json").write_text(
        json.dumps(record, indent=4) + "\n", encoding="utf-8")
    print(f"Captured {len(inventory)} files, {archive.stat().st_size} archive bytes: {archive}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    capture(parser.parse_args().reference.resolve())
