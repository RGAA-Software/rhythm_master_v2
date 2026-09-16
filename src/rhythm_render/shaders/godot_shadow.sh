// Adapted from Godot 4.5.1 scene.glsl sample_shadow PCF_5/PCF_13 and normal bias (MIT).
// Copyright (c) 2014-present Godot Engine contributors.
// See provenance/godot_shadows.json and third_party/notices/godot/LICENSE.txt.
SAMPLER2D(s_scene_shadow, 4);
SAMPLER2D(s_scene_shadow_cascade, 7);
uniform mat4 u_scene_shadow_matrix;
uniform mat4 u_scene_shadow_cascade_matrix;
uniform vec4 u_scene_shadow_settings; // light index, depth bias, world normal bias, texel size
uniform vec4 u_scene_shadow_filter;
uniform vec4 u_scene_shadow_cascade; // split view depth, enabled
float ShadowCompare(vec2 uv, float depth, float cascade) {
    float stored = cascade > 0.5 ? texture2D(s_scene_shadow_cascade, uv).r
                                 : texture2D(s_scene_shadow, uv).r;
    return depth <= stored ? 1.0 : 0.0;
}
float GodotShadow(vec3 position, vec3 normal, vec3 toward_light, float view_depth) {
    vec3 offset = normal * (1.0 - max(0.0, dot(normal, toward_light))) * u_scene_shadow_settings.z;
    float cascade = u_scene_shadow_cascade.y > 0.5 && view_depth > u_scene_shadow_cascade.x ? 1.0 : 0.0;
    vec4 projected = cascade > 0.5
        ? mul(u_scene_shadow_cascade_matrix, vec4(position + offset, 1.0))
        : mul(u_scene_shadow_matrix, vec4(position + offset, 1.0));
    if (projected.w <= 0.0) return 1.0;
    vec3 clip = projected.xyz / projected.w;
    if (abs(clip.x) > 1.0 || abs(clip.y) > 1.0 || abs(clip.z) > 1.0) return 1.0;
    // Canonical [-1,+1] clip depth and logical top-left texture coordinates.
    vec2 uv = vec2(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
    float depth = clip.z * 0.5 + 0.5 - u_scene_shadow_settings.y;
    float result = ShadowCompare(uv, depth, cascade);
    if (u_scene_shadow_filter.x < 0.5) return result;
    float step = u_scene_shadow_settings.w;
    if (u_scene_shadow_filter.x < 1.5) {
        result += ShadowCompare(uv + vec2(step, 0.0), depth, cascade);
        result += ShadowCompare(uv + vec2(-step, 0.0), depth, cascade);
        result += ShadowCompare(uv + vec2(0.0, step), depth, cascade);
        result += ShadowCompare(uv + vec2(0.0, -step), depth, cascade);
        return result * 0.2;
    }
    result += ShadowCompare(uv + vec2(step * 2.0, 0.0), depth, cascade);
    result += ShadowCompare(uv + vec2(-step * 2.0, 0.0), depth, cascade);
    result += ShadowCompare(uv + vec2(0.0, step * 2.0), depth, cascade);
    result += ShadowCompare(uv + vec2(0.0, -step * 2.0), depth, cascade);
    // Godot avoids the remaining eight comparisons when the distant cross is uniform.
    if (result <= 0.000001) return 0.0;
    if (result >= 4.999999) return 1.0;
    result += ShadowCompare(uv + vec2(step, 0.0), depth, cascade);
    result += ShadowCompare(uv + vec2(-step, 0.0), depth, cascade);
    result += ShadowCompare(uv + vec2(0.0, step), depth, cascade);
    result += ShadowCompare(uv + vec2(0.0, -step), depth, cascade);
    result += ShadowCompare(uv + vec2(step, step), depth, cascade);
    result += ShadowCompare(uv + vec2(-step, step), depth, cascade);
    result += ShadowCompare(uv + vec2(step, -step), depth, cascade);
    result += ShadowCompare(uv + vec2(-step, -step), depth, cascade);
    return result * (1.0 / 13.0);
}
