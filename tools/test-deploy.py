"""Run the packaged app with a system-only PATH and an unrelated working directory."""

import argparse
from contextlib import contextmanager
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import time


@contextmanager
def process_modules(process_id):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    psapi.EnumProcessModulesEx.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.HMODULE),
                                          wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), wintypes.DWORD]
    psapi.EnumProcessModulesEx.restype = wintypes.BOOL
    psapi.GetModuleFileNameExW.argtypes = [wintypes.HANDLE, wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD]
    psapi.GetModuleFileNameExW.restype = wintypes.DWORD
    handle = kernel.OpenProcess(0x0400 | 0x0010, False, process_id)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())

    def read_modules():
        modules = (wintypes.HMODULE * 2048)()
        needed = wintypes.DWORD()
        if not psapi.EnumProcessModulesEx(handle, modules, ctypes.sizeof(modules), ctypes.byref(needed), 3):
            return set()  # Loader initialization/termination may race the sample.
        if needed.value > ctypes.sizeof(modules):
            raise RuntimeError("Module capture buffer was too small")
        paths = set()
        for module in modules[:needed.value // ctypes.sizeof(wintypes.HMODULE)]:
            buffer = ctypes.create_unicode_buffer(32768)
            if psapi.GetModuleFileNameExW(handle, module, buffer, len(buffer)):
                paths.add(Path(buffer.value))
        return paths

    try:
        yield read_modules
    finally:
        kernel.CloseHandle(handle)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    output = args.output_directory.resolve()
    working_directory = output / "unrelated-cwd"
    working_directory.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((executable.parent / "deployment-manifest.json").read_text(encoding="utf-8"))
    expected = {name.lower() for name in manifest["runtime_dlls"]}
    if not expected or not all((executable.parent / name).is_file() for name in expected):
        raise RuntimeError("Deployment DLLs are incomplete")
    environment = dict(os.environ)
    system_root = os.environ["SystemRoot"]
    environment["PATH"] = os.pathsep.join([str(Path(system_root) / "System32"), system_root])
    # Suppress missing-DLL crash dialogs inherited by this test's child only.
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.SetErrorMode.argtypes = [wintypes.UINT]
    kernel.SetErrorMode.restype = wintypes.UINT
    previous_mode = kernel.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    loaded = set()
    try:
        with (output / "stdout.log").open("w", encoding="utf-8") as stdout, \
                (output / "stderr.log").open("w", encoding="utf-8") as stderr, \
                subprocess.Popen([str(executable), "--smoke"], cwd=working_directory,
                                 env=environment, stdout=stdout, stderr=stderr,
                                 creationflags=subprocess.CREATE_NO_WINDOW) as process:
            try:
                with process_modules(process.pid) as read_modules:
                    deadline = time.monotonic() + 30
                    while process.poll() is None:
                        loaded.update(read_modules())
                        if time.monotonic() >= deadline:
                            raise RuntimeError("Deployed smoke test timed out")
                        time.sleep(0.05)
                return_code = process.wait()
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
    finally:
        kernel.SetErrorMode(previous_mode)
        (output / "loaded-modules.txt").write_text(
            "\n".join(sorted(str(path) for path in loaded)) + "\n", encoding="utf-8")
    stdout_text = (output / "stdout.log").read_text(encoding="utf-8", errors="replace")
    if return_code != 0:
        raise RuntimeError(f"Packaged app failed: 0x{return_code & 0xffffffff:08x}; see {output}")
    for marker in ("gpu_frames=30", "visible_nodes=8/8 viewers=5 inline_previews=5"):
        if marker not in stdout_text:
            raise RuntimeError(f"Smoke evidence missing: {marker}; see {output}")
    bundled_modules = {path.name.lower(): path for path in loaded if path.name.lower() in expected}
    if expected - bundled_modules.keys():
        raise RuntimeError(f"Bundled DLLs not observed: {sorted(expected - bundled_modules.keys())}")
    for path in loaded:
        if path.name.lower().startswith(("qt5", "qt6")):
            raise RuntimeError(f"Unexpected Qt module: {path}")
        if path.name.lower() in expected and path.parent != executable.parent:
            raise RuntimeError(f"DLL loaded outside deploy: {path}")
    print(f"Deployment smoke passed: {len(bundled_modules)} local DLLs, system-only PATH, unrelated cwd, 30 GPU frames.")


if __name__ == "__main__":
    main()
