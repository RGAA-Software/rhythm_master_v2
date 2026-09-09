"""P6.1 compile-only experiment; does not adopt a runtime material profile."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import uuid

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "src/rhythm_render/shaders"
spec = importlib.util.spec_from_file_location("shader_compiler", ROOT / "tools/compile-shader.py")
compiler_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler_module)


def metadata(path):
    data = path.read_bytes()
    if data[:4] not in (b"FSH\x0c", b"VSH\x0c"):
        raise ValueError("Expected the validated revision 12 shader container")
    inputs, outputs = struct.unpack_from("<II", data, 4)
    count = struct.unpack_from("<H", data, 20)[0]
    offset = 22
    uniforms = {}
    for _ in range(count):
        size = data[offset]
        offset += 1
        name = data[offset:offset + size].decode("ascii")
        offset += size
        kind, number, register, registers, component, dimension, texture_format = struct.unpack_from(
            "<BBHHBBH", data, offset)
        offset += 10
        if name in uniforms:
            raise ValueError("Duplicate uniform")
        uniforms[name] = {"type": kind, "number": number, "register": register,
                          "register_count": registers, "component": component,
                          "dimension": dimension, "format": texture_format}
    return {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
            "input_hash": inputs, "output_hash": outputs, "uniforms": uniforms}


def fragment(expression):
    source = (SHADERS / "scene_fragment.sc").read_text(encoding="utf-8")
    marker = "void main()"
    application = "    vec3 color = base;"
    if source.count(marker) != 1 or source.count(application) != 1:
        raise ValueError("Scene wrapper changed; review material injection semantics")
    function = """uniform vec4 u_surface_params;
uniform vec4 u_surface_info;
vec3 SurfaceTint(vec2 uv, vec3 position, vec3 normal, float time,
                 float a, float b, float c, float d) {
    return EXPRESSION;
}
""".replace("EXPRESSION", expression)
    source = source.replace(marker, function + marker)
    return source.replace(application, """    vec3 tint = SurfaceTint(uv, v_world_position, v_world_normal,
        u_surface_info.x, u_surface_params.x, u_surface_params.y,
        u_surface_params.z, u_surface_params.w);
    if (any(notEqual(tint, tint))) tint = vec3_splat(0.0);
    base *= clamp(tint, vec3_splat(0.0), vec3_splat(1.0));
""" + application)


def probe(compiler, output):
    output = output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("Probe artifacts must stay in project out")
    output = output / uuid.uuid4().hex
    output.mkdir(parents=True)
    print(f"Material compile experiment: {output}", flush=True)
    for include in SHADERS.glob("*.sh"):
        shutil.copyfile(include, output / include.name)
    cases = {
        "neutral": "vec3(1.0, 1.0, 1.0)",
        "animated_surface": "vec3(uv.x, 0.5 + 0.5 * sin(position.y + time * a), abs(normal.z))",
        "parameter_surface": "clamp(vec3(a, b, c) * d, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0))",
    }
    invalid = {"invalid_type": "vec2(1.0, 0.0)", "undefined_name": "MissingSurface(uv)"}
    records = []
    for platform, profile in (("windows", "s_5_0"), ("android", "300_es")):
        directory = output / platform
        directory.mkdir()

        def compile_one(source, name, stage="fragment"):
            artifact = directory / (name + ".bin")
            compiler_module.compile_shader(compiler, source, artifact, stage,
                                           ROOT / "third_party/sources/bgfx/src",
                                           SHADERS / "scene_varying.def.sc", platform, profile)
            return artifact

        baseline = metadata(compile_one(SHADERS / "scene_fragment.sc", "baseline"))
        vertices = {}
        for name in ("scene_vertex", "scene_instance", "scene_skin_vertex", "scene_skin_instance",
                     "scene_morph_vertex", "scene_morph_instance"):
            vertices[name] = metadata(compile_one(SHADERS / (name + ".sc"), name, "vertex"))
            if vertices[name]["output_hash"] != baseline["input_hash"]:
                raise ValueError(f"Baseline varying mismatch: {platform}/{name}")
        results = {}
        for name, expression in cases.items():
            source = output / (name + ".sc")
            source.write_text(fragment(expression), encoding="utf-8")
            first = compile_one(source, name)
            result = metadata(first)
            second = compile_one(source, name + "_repeat")
            if first.read_bytes() != second.read_bytes():
                raise ValueError(f"Non-reproducible material output: {platform}/{name}")
            if result["input_hash"] != baseline["input_hash"]:
                raise ValueError("Material fragment changed scene varying contract")
            required = set(baseline["uniforms"])
            if not required.issubset(result["uniforms"]):
                raise ValueError("Existing PBR binding disappeared")
            if set(result["uniforms"]) - required - {"u_surface_params", "u_surface_info"}:
                raise ValueError("Unexpected material binding")
            results[name] = result
        rejected = {}
        for name, expression in invalid.items():
            source = output / (name + ".sc")
            source.write_text(fragment(expression), encoding="utf-8")
            try:
                compile_one(source, name)
            except RuntimeError as error:
                diagnostic = str(error)
                (directory / (name + ".txt")).write_text(diagnostic, encoding="utf-8")
                rejected[name] = diagnostic[-2048:]
            else:
                raise ValueError(f"Invalid material expression accepted: {platform}/{name}")
        records.append({"platform": platform, "baseline": baseline, "vertices": vertices,
                        "materials": results, "rejected": rejected})
        print(f"{platform}: 6 vertex contracts, 3 repeated fragments, 2 rejected expressions", flush=True)
    report = {"scope": "Host compilation and header compatibility only; no native GPU linking, "
                       "pixels, package validation, hot replacement or runtime adoption claimed",
              "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
              "scene_source_sha256": hashlib.sha256((SHADERS / "scene_fragment.sc").read_bytes()).hexdigest(),
              "records": records}
    (output / "results.json").write_text(json.dumps(report, indent=4) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "out/shader-tool/build/shaderc.exe")
    parser.add_argument("--output", type=Path, default=ROOT / "out/material-profile")
    args = parser.parse_args()
    probe(args.compiler.resolve(), args.output)


if __name__ == "__main__":
    main()
