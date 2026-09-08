"""Build optimized Windows acceptance bundles, preserving separate Debug caches."""

import argparse
import base64
import json
import os
from pathlib import Path
import subprocess

import shader_tools

ROOT = Path(__file__).resolve().parents[1]


def msvc_environment():
    # Fixed PowerShell code: paths and environment values are never interpolated
    # into shell source. Return the environment privately to child build tools.
    script = r"""
    $ErrorActionPreference = 'Stop'
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs_root = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs_root) { throw 'MSVC x64 tools are required.' }
    Import-Module (Join-Path $vs_root 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs_root -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    $env:VSLANG = '1033'
    $build_environment = @{}
    Get-ChildItem Env: | ForEach-Object { $build_environment[$_.Name] = $_.Value }
    [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
    ConvertTo-Json -InputObject $build_environment -Compress
    """
    encoded = base64.b64encode(script.encode("utf-16-le")).decode("ascii")
    result = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-EncodedCommand", encoded],
                            capture_output=True, check=True, encoding="utf-8")
    environment = os.environ.copy()
    environment.update(json.loads(result.stdout.strip()))
    return environment


def cache_values(path):
    values = {}
    if path.is_file():
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.startswith(("#", "//")) and ":" in line and "=" in line:
                name, value = line.split("=", 1)
                values[name.split(":", 1)[0]] = value
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--build", type=Path)
    parser.add_argument("--target", action="append", default=[])
    parser.add_argument("--jobs", type=int, default=20)
    parser.add_argument("--configure-only", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error("jobs must be 1..64")
    build = args.build or ROOT / ("out/windows" if args.configuration == "Debug" else "out/windows-release")
    existing = cache_values(build / "CMakeCache.txt")
    if existing.get("CMAKE_BUILD_TYPE", args.configuration) != args.configuration:
        parser.error("Use a separate build directory; changing an existing cache's configuration is refused")
    validated = cache_values(ROOT / "out/windows/CMakeCache.txt")
    validated.update(existing)
    rebuilt = shader_tools.rebuilt_compiler(ROOT)
    if rebuilt:
        validated["RHYTHM_SHADERC"] = rebuilt.as_posix()
    options = ["-DCMAKE_BUILD_TYPE=" + args.configuration, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
               "-DRHYTHM_BUILD_WINDOWS_SPIKE=ON", "-DRHYTHM_BUILD_MEDIA=ON",
               "-DRHYTHM_MEDIA_SDK=" + (ROOT / "out/vcpkg-media-lgpl/x64-windows").as_posix()]
    # Transfer validated SDK/tool locations only, never compiler flags or cache files.
    for name in ("RHYTHM_SHADERC", "RHYTHM_SPIKE_SDK", "RHYTHM_IO_SDK", "RHYTHM_MEDIA_SDK",
                 "RHYTHM_PHYSICS_SDK", "RHYTHM_CGLTF_INCLUDE", "RHYTHM_GLM_INCLUDE",
                 "RHYTHM_PARTICLE_GLM_INCLUDE", "RHYTHM_MSVC_INCLUDE_PREFIX"):
        if name in validated:
            options.append(f"-D{name}={validated[name]}")
    environment = msvc_environment()
    requested = dict(option[2:].split("=", 1) for option in options)
    if args.configure_only or any(existing.get(name) != value for name, value in requested.items()):
        subprocess.run(["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja", *options],
                       check=True, env=environment)
    # Ninja already reruns CMake when owned build configuration changes.
    if args.configure_only:
        return
    # These existing targets invoke Python deployment even after a no-op build.
    targets = [{"rhythm_master": "studio_deploy", "rhythm_player": "player_deploy"}.get(target, target)
               for target in args.target] or ["studio_deploy", "player_deploy"]
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs), "--target", *targets],
                   check=True, env=environment)
    print(f"Windows {args.configuration} build completed: {build}")


if __name__ == "__main__":
    main()
