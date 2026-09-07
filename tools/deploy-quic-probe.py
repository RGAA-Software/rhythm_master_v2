"""Deploy candidate probe executable and its complete DLL closure privately."""

import importlib.util
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("deploy_windows", ROOT / "tools/deploy-windows.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
config = {
    "executable": sys.argv[1], "runtime_dlls": sys.argv[2], "compiler": sys.argv[3],
    "configuration": "Release", "source_root": str(ROOT), "build_root": str(ROOT / "out/quic"),
    "sdk": sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else str(ROOT / "out/quic"), "io_sdk": str(ROOT / "out/quic"),
}
executable = Path(sys.argv[1])
destination = executable.parent / "deploy"
for source in module.resolve_dependencies(config).values():
    module.copy_file(source, executable.parent / source.name)
    module.copy_file(source, destination / source.name)
module.copy_file(executable, destination / executable.name)
# The candidate is not part of Studio/Player. Its own experiment still carries notices.
if len(sys.argv) > 4 and sys.argv[4] == "quiche":
    module.copy_file(ROOT / "third_party/sources/quiche-probe/COPYING", destination / "notices/quiche/COPYING")
    module.copy_tree(ROOT / "out/quic/notices/quiche", destination / "notices/quiche")
else:
    module.copy_file(ROOT / "third_party/sources/msquic-probe/LICENSE", destination / "notices/msquic/LICENSE")
    module.copy_file(ROOT / "third_party/sources/msquic-probe/submodules/xdp-for-windows/LICENSE",
                     destination / "notices/xdp-for-windows/LICENSE")
if len(sys.argv) > 4 and sys.argv[4] == "1":
    module.copy_file(ROOT / "third_party/sources/openssl-quic-3.5.8/LICENSE.txt",
                     destination / "notices/openssl/LICENSE.txt")
fixtures = ROOT / "out/quic/content/fixtures"
if fixtures.is_dir():
    module.copy_tree(fixtures, destination / "fixtures")
(destination / "README.txt").write_text(
    "Isolated QUIC candidate test, not a Player release.\n"
    f"Run: {executable.name} fixtures\n"
    "Fixture private keys are disposable public test data.\n", encoding="utf-8")
manifest = {path.relative_to(destination).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(destination.rglob("*")) if path.is_file()
            and path.name != "deployment-manifest.json"}
(destination / "deployment-manifest.json").write_text(json.dumps(manifest, indent=4) + "\n")
print(f"Candidate executable, DLL closure, fixtures and notices deployed to {destination}")
