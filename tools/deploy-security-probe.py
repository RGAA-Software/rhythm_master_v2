"""Deploy isolated identity validation binaries, runtime closure and TLS notices."""

import hashlib
import importlib.util
import json
import msvcrt
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("deploy_windows", ROOT / "tools/deploy-windows.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
executable = Path(sys.argv[1])
# Independent executable link steps can finish concurrently. Serialize the shared
# DLL closure and manifest publication in this private build directory.
with (executable.parent / ".security-deploy.lock").open("a+b") as deployment_lock:
    if deployment_lock.seek(0, 2) == 0:
        deployment_lock.write(b"0")
        deployment_lock.flush()
    deployment_lock.seek(0)
    msvcrt.locking(deployment_lock.fileno(), msvcrt.LK_LOCK, 1)
    try:
        config = {
            "executable": str(executable), "runtime_dlls": sys.argv[3] if len(sys.argv) > 3 else "", "compiler": sys.argv[2],
            "configuration": "Release", "source_root": str(ROOT),
            "build_root": str(ROOT / "out/security"),
            "sdk": sys.argv[4] if len(sys.argv) > 4 else str(ROOT / "out/security"), "io_sdk": str(ROOT / "out/security"),
        }
        destination = executable.parent / "deploy"
        for source in module.resolve_dependencies(config).values():
            module.copy_file(source, executable.parent / source.name)
            module.copy_file(source, destination / source.name)
        module.copy_file(executable, destination / executable.name)
        module.copy_file(ROOT / "third_party/sources/openssl-quic-3.5.8/LICENSE.txt",
                         destination / "notices/openssl/LICENSE.txt")
        if len(sys.argv) > 3 and sys.argv[3]:
            module.copy_file(ROOT / "third_party/sources/msquic-probe/LICENSE",
                             destination / "notices/msquic/LICENSE")
        manifest = {path.relative_to(destination).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in sorted(destination.rglob("*")) if path.is_file()
                    and path.name != "deployment-manifest.json"}
        (destination / "deployment-manifest.json").write_text(json.dumps(manifest, indent=4) + "\n")
        print(f"Identity probe deployed to {destination / executable.name}")
    finally:
        deployment_lock.seek(0)
        msvcrt.locking(deployment_lock.fileno(), msvcrt.LK_UNLCK, 1)
