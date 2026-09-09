"""Generate the bounded RGB surface wrapper from the canonical scene fragment."""

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPRESSION_SLOT = "__RHYTHM_SURFACE_EXPRESSION__"


def template():
    source = (ROOT / "src/rhythm_render/shaders/scene_fragment.sc").read_text(encoding="utf-8")
    if source.count("void main()") != 1 or source.count("    vec3 color = base;") != 1:
        raise ValueError("Scene shader changed; review the surface application contract")
    function = """#if BGFX_SHADER_LANGUAGE_GLSL
precision highp float;
#endif
uniform vec4 u_surface_params;
uniform vec4 u_surface_info;
vec3 SurfaceTint(vec2 uv, vec3 position, vec3 normal, float time,
                 float a, float b, float c, float d) {
    float pi = 3.141592653589793;
    return (
#line 1
__RHYTHM_SURFACE_EXPRESSION__
    );
}
"""
    source = source.replace("void main()", function + "void main()")
    return source.replace("    vec3 color = base;", """    vec3 tint = SurfaceTint(uv, v_world_position, v_world_normal,
        u_surface_info.x, u_surface_params.x, u_surface_params.y,
        u_surface_params.z, u_surface_params.w);
    if (any(notEqual(tint, tint))) tint = vec3_splat(0.0);
    base *= clamp(tint, vec3_splat(0.0), vec3_splat(1.0));
    vec3 color = base;""")


def fragment(expression):
    return template().replace(EXPRESSION_SLOT, expression)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("Generated surface header belongs in project out")
    source = template()
    if ')SURFACE"' in source:
        raise ValueError("Surface template contains the C++ raw-string delimiter")
    header = ('#pragma once\n#include <string_view>\nnamespace rhythm::surface_shader::detail {\n'
              'inline constexpr std::string_view kTemplate = R"SURFACE(' + source + ')SURFACE";\n}\n')
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != header:
        output.write_text(header, encoding="utf-8")


if __name__ == "__main__":
    main()
