"""Compile owned render shaders for the target backend and embed their bytes."""

import argparse
import importlib.util
import os
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("shader_compiler", ROOT / "tools/compile-shader.py")
compiler_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler_module)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--platform", choices=("windows", "android"), required=True)
    parser.add_argument("--group", choices=("color", "scene", "filter", "glow", "noise", "mapping", "displace", "execution_probe", "gpu_points", "color_pipeline", "depth", "environment", "antialias"), default="color")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("Generated shader header must remain in the project build output")
    shader = ROOT / "src/rhythm_render/shaders"
    programs = [("color_adjust.sc", "fragment", "varying.def.sc", "kColorAdjustShader")]
    if args.group == "execution_probe":
        programs = [("probe_instance.sc", "vertex", "probe_varying.def.sc", "kProbeVertexShader"),
                    ("probe_color.sc", "fragment", "probe_varying.def.sc", "kProbeFragmentShader"),
                    ("probe_update.sc", "compute", None, "kProbeComputeShader"),
                    ("attribute_probe_vertex.sc", "vertex", "attribute_probe_varying.def.sc", "kAttributeProbeVertexShader"),
                    ("gpu_point_map.sc", "compute", None, "kAttributeProbeComputeShader"),
                    ("transparency_probe_vertex.sc", "vertex", "transparency_probe_varying.def.sc", "kTransparencyProbeVertexShader"),
                    ("transparency_probe_color.sc", "fragment", "transparency_probe_varying.def.sc", "kTransparencyProbeColorShader"),
                    ("transparency_probe_resolve.sc", "fragment", "transparency_probe_varying.def.sc", "kTransparencyProbeResolveShader")]
    elif args.group == "gpu_points":
        programs = [("gpu_point_vertex.sc", "vertex", "gpu_point_varying.def.sc", "kGpuPointVertexShader"),
                    ("gpu_point_fragment.sc", "fragment", "gpu_point_varying.def.sc", "kGpuPointFragmentShader"),
                    ("gpu_particle_update.sc", "compute", None, "kGpuParticleComputeShader"),
                    ("gpu_point_map.sc", "compute", None, "kGpuPointMapShader")]
    elif args.group == "antialias":
        programs = [("texture_fxaa.sc", "fragment", "varying.def.sc", "kTextureFxaaShader")]
    elif args.group == "color_pipeline":
        programs = [("color_pipeline.sc", "fragment", "varying.def.sc", "kColorPipelineShader")]
    elif args.group == "environment":
        programs = [("environment_filter.sc", "fragment", "varying.def.sc", "kEnvironmentFilterShader")]
    elif args.group == "depth":
        programs = [("depth_linear.sc", "fragment", "varying.def.sc", "kDepthLinearShader"),
                    ("depth_of_field.sc", "fragment", "varying.def.sc", "kDepthOfFieldShader")]
    elif args.group == "scene":
        programs = [("scene_vertex.sc", "vertex", "scene_varying.def.sc", "kSceneVertexShader"),
                    ("scene_instance.sc", "vertex", "scene_varying.def.sc", "kSceneInstanceShader"),
                    ("scene_skin_vertex.sc", "vertex", "scene_varying.def.sc", "kSceneSkinVertexShader"),
                    ("scene_skin_instance.sc", "vertex", "scene_varying.def.sc", "kSceneSkinInstanceShader"),
                    ("scene_morph_vertex.sc", "vertex", "scene_varying.def.sc", "kSceneMorphVertexShader"),
                    ("scene_morph_instance.sc", "vertex", "scene_varying.def.sc", "kSceneMorphInstanceShader"),
                    ("scene_fragment.sc", "fragment", "scene_varying.def.sc", "kSceneFragmentShader"),
                    ("scene_depth_prepass.sc", "fragment", "scene_varying.def.sc", "kSceneDepthPrepassShader")]
    elif args.group == "filter":
        programs = [("texture_filter.sc", "fragment", "varying.def.sc", "kTextureFilterShader")]
    elif args.group == "glow":
        programs = [("texture_glow_filter.sc", "fragment", "varying.def.sc", "kTextureGlowFilterShader"),
                    ("texture_glow_downsample.sc", "fragment", "varying.def.sc", "kTextureGlowDownsampleShader"),
                    ("texture_glow_upsample.sc", "fragment", "varying.def.sc", "kTextureGlowUpsampleShader"),
                    ("texture_glow_composite.sc", "fragment", "varying.def.sc", "kTextureGlowCompositeShader")]
    elif args.group == "noise":
        programs = [("texture_noise.sc", "fragment", "varying.def.sc", "kTextureNoiseShader")]
    elif args.group == "displace":
        programs = [("texture_displace.sc", "fragment", "varying.def.sc", "kTextureDisplaceShader"),
                    ("texture_trail.sc", "fragment", "varying.def.sc", "kTextureTrailShader")]
    elif args.group == "mapping":
        programs = [("texture_mapping.sc", "fragment", "varying.def.sc", "kTextureMappingShader"),
                    ("texture_contours.sc", "fragment", "varying.def.sc", "kTextureContoursShader")]
    header = "#pragma once\n#include <cstdint>\nnamespace rhythm::render::detail {\n"
    for source, stage, varying, symbol in programs:
        binary = output.with_suffix("." + stage + ".bin")
        compiler_module.compile_shader(args.compiler, shader / source, binary,
            stage, ROOT / "third_party/sources/bgfx/src", shader / varying if varying else None,
            args.platform, ("310_es" if stage == "compute" else "300_es")
            if args.platform == "android" else "s_5_0")
        data = binary.read_bytes()
        lines = [", ".join(f"0x{byte:02x}" for byte in data[offset:offset+16])
                 for offset in range(0, len(data), 16)]
        header += "inline constexpr std::uint8_t " + symbol + "[] = {\n"
        header += ",\n".join("    " + line for line in lines) + "\n};\n"
    header += "}\n"
    if output.exists() and output.read_text(encoding="utf-8") == header:
        return
    with tempfile.TemporaryDirectory(prefix="shader-header-", dir=output.parent) as temporary:
        candidate = Path(temporary) / output.name
        candidate.write_text(header, encoding="utf-8")
        os.replace(candidate, output)


if __name__ == "__main__":
    main()
