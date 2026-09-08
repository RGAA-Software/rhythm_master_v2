"""Check the bounded image shader profile using the installed native compiler."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("compiler", ROOT / "tools/compile-shader.py")
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-test", "program-test", "compiler", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("Profile fixtures belong in this project's out directory")
    output.mkdir(parents=True, exist_ok=True)
    source = output / "profile.sc"
    subprocess.run([str(args.source_test), str(source)], check=True, timeout=20)
    binaries = []
    for platform, profile in (("windows", "s_5_0"), ("android", "300_es")):
        binary = output / (platform + ".bin")
        compiler.compile_shader(args.compiler, source, binary, "fragment",
                                ROOT / "third_party/sources/bgfx/src",
                                ROOT / "src/rhythm_render/shaders/varying.def.sc", platform, profile)
        binaries.append(binary)
    subprocess.run([str(args.program_test), *map(str, binaries)], check=True, timeout=20)


if __name__ == "__main__":
    main()
