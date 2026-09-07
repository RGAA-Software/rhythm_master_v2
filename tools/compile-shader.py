"""Bounded local shaderc experiment; publish only complete valid artifacts."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def compile_shader(compiler, source, output, stage, include, varying=None,
                   platform="windows", profile="s_5_0"):
    output = output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("Shader experiment output must stay in this project's out directory")
    source_bytes = source.read_bytes()
    if len(source_bytes) > 1024 * 1024:
        raise ValueError("Shader source exceeds experiment limit")
    compiler_hash = hashlib.sha256(compiler.read_bytes()).hexdigest()
    include_hashes = {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in sorted(include.glob("*.sh"))}
    include_hashes.update({str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in sorted(source.parent.glob("*.sh"))})
    if varying:
        include_hashes[str(varying.resolve())] = hashlib.sha256(varying.read_bytes()).hexdigest()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="shader-", dir=output.parent) as temporary:
        candidate = Path(temporary) / "candidate.bin"
        command = [str(compiler), "-f", str(source), "-o", str(candidate),
                   "--type", stage, "--platform", platform, "-p", profile,
                   "-i", str(include)]
        if varying:
            command += ["--varyingdef", str(varying)]
        completed = subprocess.run(command, capture_output=True, timeout=30)
        if completed.returncode:
            raise RuntimeError(completed.stderr.decode("utf-8", errors="replace")[-8192:])
        if source.read_bytes() != source_bytes or any(
                hashlib.sha256(Path(path).read_bytes()).hexdigest() != expected
                for path, expected in include_hashes.items()):
            raise RuntimeError("Shader source changed while compiling; stale result discarded")
        artifact = candidate.read_bytes()
        if artifact[:3] != {"vertex": b"VSH", "fragment": b"FSH", "compute": b"CSH"}[stage]:
            raise ValueError("Invalid shader container")
        if len(artifact) < 16 or len(artifact) > 4 * 1024 * 1024:
            raise ValueError("Shader artifact size limit")
        os.replace(candidate, output)
    record = {"compiler": str(compiler.resolve()),
              "compiler_sha256": compiler_hash,
              "source": str(source.resolve()), "source_sha256": hashlib.sha256(source_bytes).hexdigest(),
              "include_hashes": include_hashes,
              "artifact_sha256": hashlib.sha256(artifact).hexdigest(), "stage": stage,
              "platform": platform, "profile": profile}
    output.with_suffix(".json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    for name in ("compiler", "source", "output", "include"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--stage", choices=("vertex", "fragment", "compute"), required=True)
    parser.add_argument("--varying", type=Path)
    parser.add_argument("--platform", choices=("windows", "android"), default="windows")
    parser.add_argument("--profile", default="s_5_0")
    args = parser.parse_args()
    compile_shader(args.compiler, args.source, args.output, args.stage, args.include, args.varying,
                   args.platform, args.profile)


if __name__ == "__main__":
    main()
