"""Build the recorded shaderc source archive with CMake/Ninja and 20 workers."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def write_changed(path, data):
    if not path.is_file() or path.read_bytes() != data:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=20)
    parser.add_argument("--build", type=Path, default=ROOT / "out/shader-tool")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error("jobs must be 1..64")
    build = args.build.resolve()
    if not build.is_relative_to(ROOT / "out"):
        parser.error("Tool sources and build outputs must remain in the project out directory")
    record = json.loads((ROOT / "provenance/shaderc_source_build.json").read_text(encoding="utf-8"))
    archive = ROOT / record["archive"]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != record["archive_sha256"]:
        raise ValueError("Shader tool source archive differs from the recorded snapshot")
    source = build / "source"
    with zipfile.ZipFile(archive) as bundle:
        for name, expected in record["files"].items():
            destination = (source / name).resolve()
            if not destination.is_relative_to(source):
                raise ValueError("Source inventory path escapes the tool workspace")
            data = bundle.read(name)
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError(f"Shader tool source mismatch: {name}")
            write_changed(destination, data)
    lines = ["cmake_minimum_required(VERSION 3.24)", "project(shader_tool LANGUAGES C CXX)",
             "set(CMAKE_CXX_STANDARD 20)", "set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)",
             'set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /utf-8")',
             'set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG")',
             "add_compile_options(/GR- /EHs-c- /fp:fast /arch:AVX /Gy /GF /GS- /Brepro)"]

    def quoted(values):
        return " ".join('"' + value.replace('"', '\\"') + '"' for value in values)

    for name, module in record["modules"].items():
        declaration = "add_executable" if name == "shaderc" else "add_library"
        kind = "" if name == "shaderc" else " STATIC"
        lines.append(f"{declaration}({name}{kind} {quoted(module['sources'])})")
        lines.append(f"target_include_directories({name} PRIVATE {quoted(module['includes'])})")
        lines.append(f"target_compile_definitions({name} PRIVATE {quoted(module['definitions'])})")
        lines.append(f"target_compile_options({name} PRIVATE {quoted(module['options'])})")
        if module["dependencies"]:
            lines.append(f"target_link_libraries({name} PRIVATE {' '.join(module['dependencies'])})")
    lines.extend(["target_link_libraries(shaderc PRIVATE psapi)",
                  "target_link_options(shaderc PRIVATE /Brepro /OPT:REF /OPT:ICF)"])
    write_changed(source / "CMakeLists.txt", ("\n".join(lines) + "\n").encode("utf-8"))
    spec = importlib.util.spec_from_file_location("windows_build", ROOT / "tools/build-windows.py")
    windows = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(windows)
    environment = windows.msvc_environment()
    subprocess.run(["cmake", "-S", str(source), "-B", str(build / "build"), "-G", "Ninja",
                    "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
                   check=True, env=environment)
    subprocess.run(["cmake", "--build", str(build / "build"), "--parallel", str(args.jobs),
                    "--target", "shaderc"], check=True, env=environment)
    compiler = build / "build/shaderc.exe"
    result = {"compiler": str(compiler), "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
              "source_archive_sha256": record["archive_sha256"]}
    (build / "build-result.json").write_text(json.dumps(result, indent=4) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
