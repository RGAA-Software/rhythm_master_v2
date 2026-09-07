"""Assemble the Windows acceptance bundle using the executable's PE imports."""

import argparse
import filecmp
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def copy_file(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.resolve() == destination.resolve():
        return
    if destination.is_file() and filecmp.cmp(source, destination, shallow=False):
        return
    try:
        shutil.copy2(source, destination)
    except PermissionError as error:
        raise RuntimeError(f"Cannot update {destination}; close the running app.") from error


def copy_tree(source, destination):
    if not source.is_dir():
        raise RuntimeError(f"Required resource directory is missing: {source}")
    for entry in sorted(source.rglob("*")):
        if entry.is_file() and entry.name != ".writer" and not entry.name.endswith(".tmp"):
            copy_file(entry, destination / entry.relative_to(source))


def version_key(path):
    return [int(part) if part.isdigit() else part.lower()
            for part in re.split(r"(\d+)", str(path))]


def runtime_directories(config):
    directories = []
    for key in ("media_sdk", "sdk", "io_sdk"):
        if config.get(key):
            sdk = Path(config[key])
            if config["configuration"].lower() == "debug":
                directories.append(sdk / "debug/bin")
            directories.append(sdk / "bin")
    compiler = Path(config["compiler"])
    vc_root = next((parent for parent in compiler.parents if parent.name.lower() == "vc"), None)
    if vc_root is None:
        raise RuntimeError(f"Cannot locate MSVC runtime relative to {compiler}")
    crt = list(vc_root.glob("Redist/MSVC/*/x64/Microsoft.VC*.CRT"))
    crt.extend(vc_root.glob("Redist/MSVC/*/debug_nonredist/x64/Microsoft.VC*.DebugCRT"))
    directories.extend(sorted(crt, key=version_key, reverse=True))
    windows_sdk = Path(os.environ.get("WindowsSdkDir") or
                       str(Path(os.environ["ProgramFiles(x86)"]) / "Windows Kits/10"))
    directories.extend(sorted(windows_sdk.glob("bin/*/x64/ucrt"), key=version_key, reverse=True))
    return directories


def imports(binary, dumpbin):
    result = subprocess.run([str(dumpbin), "/nologo", "/dependents", str(binary)],
                            capture_output=True, check=True, timeout=30,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    # DLL lines are ASCII even with a localized dumpbin header.
    return [line.strip().decode("ascii") for line in result.stdout.splitlines()
            if re.fullmatch(rb"\s*[\w.+-]+\.dll\s*", line, re.IGNORECASE)]


def resolve_dependencies(config):
    executable = Path(config["executable"])
    dumpbin = Path(config["compiler"]).parent / "dumpbin.exe"
    system_directory = Path(os.environ["SystemRoot"]) / "System32"
    directories = runtime_directories(config)
    # Prefer the configured SDK over old copies beside a previous build.
    directories.append(executable.parent)
    linked = {Path(path).name.lower(): Path(path)
              for path in config["runtime_dlls"].split(";") if path}
    resolved = {}
    pending = [executable]
    while pending:
        binary = pending.pop()
        for name in imports(binary, dumpbin):
            key = name.lower()
            if key in resolved or key.startswith(("api-ms-", "ext-ms-")):
                continue
            if key.startswith(("qt5", "qt6")):
                raise RuntimeError(f"Unexpected Qt dependency: {name}")
            is_crt = key.startswith(("msvcp140", "vcruntime140", "concrt140")) or key == "ucrtbased.dll"
            if not is_crt and (system_directory / name).is_file():
                continue
            source = linked.get(key)
            if source is None:
                source = next((directory / name for directory in directories
                               if (directory / name).is_file()), None)
            if source is None or not source.is_file():
                raise RuntimeError(f"Missing dependency {name}, imported by {binary}")
            resolved[key] = source
            pending.append(source)
    return resolved


def deploy(config):
    executable = Path(config["executable"])
    if not executable.is_file():
        raise RuntimeError(f"Build the executable first: {executable}")
    dependencies = resolve_dependencies(config)
    if config.get("media_sdk"):
        profile = json.loads((Path(config["source_root"]) / "provenance/media_lgpl_windows.json")
                             .read_text(encoding="utf-8"))
        expected = profile["profiles"][config["configuration"]]["dll_sha256"]
        for dependency in dependencies.values():
            if dependency.name in expected and hashlib.sha256(dependency.read_bytes()).hexdigest() != expected[dependency.name]:
                raise RuntimeError(f"Media dependency differs from the validated profile: {dependency}")
    destination = executable.parent / "deploy"
    for source in dependencies.values():
        copy_file(source, executable.parent / source.name)
        copy_file(source, destination / source.name)
    copy_file(executable, destination / executable.name)
    source_root = Path(config["source_root"])
    copy_tree(Path(config["build_root"]) / "content", destination / "content")
    copy_tree(source_root / "locales", destination / "locales")
    copy_tree(source_root / "third_party/notices", destination / "notices")
    if config.get("media_sdk"):
        media_sdk = Path(config["media_sdk"])
        for package in ("ffmpeg", "zlib"):
            copy_tree(media_sdk / "share" / package, destination / "notices" / package)
        media_sources = source_root / "out/release-sources/ffmpeg-vcpkg.zip"
        if not media_sources.is_file():
            raise RuntimeError("Prepare matching FFmpeg source/build materials before application deployment")
        copy_file(media_sources, destination / "notices/ffmpeg/source-and-build.zip")
        copy_file(source_root / "provenance/media_lgpl_windows.json", destination / "notices/ffmpeg/validated-profile.json")
    copy_file(source_root / "third_party/README.md", destination / "THIRD_PARTY_NOTICES.md")
    (destination / "README.txt").write_text(
        f"Rhythm Master - {config['configuration']} local acceptance build\n\n"
        f"Double-click {executable.name}. No SDK PATH setup is required.\n"
        "Copy the entire deploy folder when moving this build.\n"
        "Projects are saved in the per-user RhythmMaster/Studio data directory.\n",
        encoding="utf-8")
    manifest = {
        "configuration": config["configuration"],
        "runtime_dlls": {source.name: str(source) for source in dependencies.values()},
        "files": {path.relative_to(destination).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in sorted(destination.rglob("*")) if path.is_file()
                  and path.name != "deployment-manifest.json"
                  and "smoke-project.rhythmproj" not in path.parts},
    }
    (destination / "deployment-manifest.json").write_text(
        json.dumps(manifest, indent=4) + "\n", encoding="utf-8")
    print(f"Deployed {config['configuration']}: {len(dependencies)} DLLs + executable/resources -> {destination}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, type=Path,
                        help="CMake-generated deploy-config-<configuration>.txt")
    args = parser.parse_args()
    config = dict(line.split("=", 1) for line in args.config.read_text(encoding="utf-8").splitlines() if line)
    deploy(config)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"Deployment failed: {error}", file=sys.stderr)
        sys.exit(1)
