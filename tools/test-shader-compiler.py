"""Failure and reproducibility checks using the exact local compiler candidate."""

import importlib.util
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("shader_compiler", ROOT / "tools/compile-shader.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
compiler = Path(sys.argv[1])
output = ROOT / "out/shader-contract/protected.bin"
source = ROOT / "src/windows_spike/tests/shaders/compute_probe.sc"
include = ROOT / "third_party/sources/bgfx/src"
module.compile_shader(compiler, source, output, "compute", include)
before = output.read_bytes()
module.compile_shader(compiler, source, output, "compute", include)
if output.read_bytes() != before:
    raise RuntimeError("Identical shader input did not reproduce identical bytes")
invalid = output.parent / "invalid.sc"
invalid.write_text("this is deliberately invalid shader syntax", encoding="utf-8")
try:
    module.compile_shader(compiler, invalid, output, "compute", include)
except RuntimeError:
    pass
else:
    raise RuntimeError("Invalid shader unexpectedly compiled")
if output.read_bytes() != before:
    raise RuntimeError("Failed compile replaced the valid artifact")
mutable = output.parent / "mutable.sc"
mutable.write_bytes(source.read_bytes())
run_compiler = module.subprocess.run
def concurrent_edit(*args, **kwargs):
    result = run_compiler(*args, **kwargs)
    mutable.write_text("edit arrived before publication", encoding="utf-8")
    return result
module.subprocess.run = concurrent_edit
try:
    module.compile_shader(compiler, mutable, output, "compute", include)
except RuntimeError as error:
    if "stale result" not in str(error):
        raise
else:
    raise RuntimeError("Stale shader unexpectedly published")
finally:
    module.subprocess.run = run_compiler
if output.read_bytes() != before:
    raise RuntimeError("Stale compile replaced the valid artifact")
print("Shader contracts passed: reproducible bytes; failed/stale compiles preserve valid artifact")
