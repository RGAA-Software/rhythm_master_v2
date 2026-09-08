// glTF relative targets: original + sum(weight * delta). Morph precedes skin.
#if BGFX_SHADER_LANGUAGE_GLSL
precision highp float;
precision highp sampler2D;
#endif
SAMPLER2D(s_morph_targets, 6);
uniform vec4 u_morph_weights;
// texture height, vertex count, target count, skin enabled.
uniform vec4 u_morph_info;
vec3 MorphDelta(float index)
{
    vec2 uv = vec2(mod(index, 1024.0) + 0.5, floor(index / 1024.0) + 0.5)
              / vec2(1024.0, u_morph_info.x);
    return texture2DLod(s_morph_targets, uv, 0.0).xyz;
}
void MorphMesh(inout vec3 position, inout vec3 normal, inout vec3 tangent, float vertex_index)
{
    vec3 old_normal = normal, old_tangent = tangent;
    for (int target = 0; target < 4; ++target)
    {
        float weight = u_morph_weights[target];
        if (float(target) >= u_morph_info.z || weight == 0.0) continue;
        float index = (float(target) * u_morph_info.y + vertex_index) * 3.0;
        position += weight * MorphDelta(index);
        normal += weight * MorphDelta(index + 1.0);
        tangent += weight * MorphDelta(index + 2.0);
    }
    normal = length(normal) > 0.000001 ? normalize(normal) : old_normal;
    tangent = length(tangent) > 0.000001 ? normalize(tangent) : old_tangent;
}
