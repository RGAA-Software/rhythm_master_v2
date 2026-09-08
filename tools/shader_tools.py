"""Select and verify recorded host tools for builds and Studio deployment."""

import hashlib
import json


def profile_for(source_root, compiler):
    actual = hashlib.sha256(compiler.read_bytes()).hexdigest()
    for name in ("shaderc_rebuilt_host.json", "shaderc_host.json"):
        path = source_root / "provenance" / name
        if path.is_file():
            profile = json.loads(path.read_text(encoding="utf-8"))
            if profile["compiler_sha256"] == actual:
                return path
    raise RuntimeError("Shader compiler differs from the validated host profiles")


def rebuilt_compiler(source_root):
    profile = source_root / "provenance/shaderc_rebuilt_host.json"
    if not profile.is_file():
        return None
    record = json.loads(profile.read_text(encoding="utf-8"))
    compiler = source_root / record["compiler"]
    if not compiler.is_file():
        raise RuntimeError("Build the recorded host compiler first: python tools/build-shader-tool.py")
    if profile_for(source_root, compiler) != profile:
        raise RuntimeError("Rebuilt compiler does not match its validated profile")
    return compiler
