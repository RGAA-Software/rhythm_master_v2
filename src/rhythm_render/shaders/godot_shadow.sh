// Adapted from Godot 4.5.1 scene.glsl sample_shadow PCF_5/PCF_13 and normal bias (MIT).
// Copyright (c) 2014-present Godot Engine contributors.
// See provenance/godot_shadows.json and third_party/notices/godot/LICENSE.txt.
SAMPLER2D(s_scene_shadow, 4);
SAMPLER2D(s_scene_shadow_cascade, 7);
SAMPLER2D(s_scene_shadow_point_2, 8);
SAMPLER2D(s_scene_shadow_point_3, 9);
SAMPLER2D(s_scene_shadow_point_4, 10);
SAMPLER2D(s_scene_shadow_point_5, 11);
uniform mat4 u_scene_shadow_matrix;
uniform mat4 u_scene_shadow_cascade_matrix;
uniform mat4 u_scene_shadow_point_matrix_2;
uniform mat4 u_scene_shadow_point_matrix_3;
uniform mat4 u_scene_shadow_point_matrix_4;
uniform mat4 u_scene_shadow_point_matrix_5;
uniform vec4 u_scene_shadow_settings; // light index, depth bias, world normal bias, texel size
uniform vec4 u_scene_shadow_filter;
uniform vec4 u_scene_shadow_cascade; // split view depth, cascade enabled, point cube enabled
float ShadowStored(vec2 uv, float face) {
    if (face < 0.5) return texture2D(s_scene_shadow, uv).r;
    if (face < 1.5) return texture2D(s_scene_shadow_cascade, uv).r;
    if (face < 2.5) return texture2D(s_scene_shadow_point_2, uv).r;
    if (face < 3.5) return texture2D(s_scene_shadow_point_3, uv).r;
    if (face < 4.5) return texture2D(s_scene_shadow_point_4, uv).r;
    return texture2D(s_scene_shadow_point_5, uv).r;
}
float ShadowCompare(vec2 uv, float depth, float face) {
    float stored = ShadowStored(uv, face);
    return depth <= stored ? 1.0 : 0.0;
}
vec4 PointShadowProjection(float face, vec4 position) {
    if (face < 0.5) return mul(u_scene_shadow_matrix, position);
    if (face < 1.5) return mul(u_scene_shadow_cascade_matrix, position);
    if (face < 2.5) return mul(u_scene_shadow_point_matrix_2, position);
    if (face < 3.5) return mul(u_scene_shadow_point_matrix_3, position);
    if (face < 4.5) return mul(u_scene_shadow_point_matrix_4, position);
    return mul(u_scene_shadow_point_matrix_5, position);
}
float GodotShadow(vec3 position, vec3 normal, vec3 toward_light, float view_depth,
                  vec3 light_position) {
    vec3 offset = normal * (1.0 - max(0.0, dot(normal, toward_light))) * u_scene_shadow_settings.z;
    float face = u_scene_shadow_cascade.y > 0.5 && view_depth > u_scene_shadow_cascade.x ? 1.0 : 0.0;
    vec4 world = vec4(position + offset, 1.0);
    vec4 projected;
    if (u_scene_shadow_cascade.z > 0.5) {
        vec3 relative = world.xyz - light_position;
        vec3 magnitude = abs(relative);
        if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z)
            face = relative.x >= 0.0 ? 0.0 : 1.0;
        else if (magnitude.y >= magnitude.z)
            face = relative.y < 0.0 ? 2.0 : 3.0;
        else
            face = relative.z >= 0.0 ? 4.0 : 5.0;
        projected = PointShadowProjection(face, world);
    } else {
        projected = face > 0.5
            ? mul(u_scene_shadow_cascade_matrix, world)
            : mul(u_scene_shadow_matrix, world);
    }
    if (projected.w <= 0.0) return 1.0;
    vec3 clip = projected.xyz / projected.w;
    if (abs(clip.x) > 1.0 || abs(clip.y) > 1.0 || abs(clip.z) > 1.0) return 1.0;
    // Canonical [-1,+1] clip depth and logical top-left texture coordinates.
    vec2 uv = vec2(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
    float depth = clip.z * 0.5 + 0.5 - u_scene_shadow_settings.y;
    float result = ShadowCompare(uv, depth, face);
    if (u_scene_shadow_filter.x < 0.5) return result;
    float step = u_scene_shadow_settings.w;
    if (u_scene_shadow_filter.x < 1.5) {
        result += ShadowCompare(uv + vec2(step, 0.0), depth, face);
        result += ShadowCompare(uv + vec2(-step, 0.0), depth, face);
        result += ShadowCompare(uv + vec2(0.0, step), depth, face);
        result += ShadowCompare(uv + vec2(0.0, -step), depth, face);
        return result * 0.2;
    }
    result += ShadowCompare(uv + vec2(step * 2.0, 0.0), depth, face);
    result += ShadowCompare(uv + vec2(-step * 2.0, 0.0), depth, face);
    result += ShadowCompare(uv + vec2(0.0, step * 2.0), depth, face);
    result += ShadowCompare(uv + vec2(0.0, -step * 2.0), depth, face);
    // Godot avoids the remaining eight comparisons when the distant cross is uniform.
    if (result <= 0.000001) return 0.0;
    if (result >= 4.999999) return 1.0;
    result += ShadowCompare(uv + vec2(step, 0.0), depth, face);
    result += ShadowCompare(uv + vec2(-step, 0.0), depth, face);
    result += ShadowCompare(uv + vec2(0.0, step), depth, face);
    result += ShadowCompare(uv + vec2(0.0, -step), depth, face);
    result += ShadowCompare(uv + vec2(step, step), depth, face);
    result += ShadowCompare(uv + vec2(-step, step), depth, face);
    result += ShadowCompare(uv + vec2(step, -step), depth, face);
    result += ShadowCompare(uv + vec2(-step, -step), depth, face);
    return result * (1.0 / 13.0);
}
