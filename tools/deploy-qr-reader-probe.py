"""Deploy the isolated QR reader with CRT closure and retained upstream notices."""

import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("deploy_windows", ROOT / "tools/deploy-windows.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
executable = Path(sys.argv[1])
config = {"executable": str(executable), "runtime_dlls": "", "compiler": sys.argv[2],
          "configuration": "Release", "source_root": str(ROOT), "build_root": str(ROOT / "out/qr-reader"),
          "sdk": str(ROOT / "out/qr-reader"), "io_sdk": str(ROOT / "out/qr-reader")}
destination = executable.parent / "deploy"
for source in module.resolve_dependencies(config).values():
    module.copy_file(source, executable.parent / source.name)
    module.copy_file(source, destination / source.name)
module.copy_file(executable, destination / executable.name)
module.copy_tree(ROOT / "third_party/notices/zxing", destination / "notices/zxing")
for source, target in (("third_party/sources/zxing-probe/LICENSE", "zxing/LICENSE"),
                       ("third_party/sources/zxing-probe/core/src/libzueci/zueci.h", "libzueci/zueci.h"),
                       ("third_party/sources/zxing-probe/core/src/libzueci/README.md", "libzueci/README.md"),
                       ("third_party/notices/qrcodegen/LICENSE", "qrcodegen/LICENSE")):
    module.copy_file(ROOT / source, destination / "notices" / target)
manifest = {path.relative_to(destination).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(destination.rglob("*")) if path.is_file() and path.name != "deployment-manifest.json"}
(destination / "deployment-manifest.json").write_text(json.dumps(manifest, indent=4) + "\n")
print("QR reader probe deployed:", destination / executable.name)
