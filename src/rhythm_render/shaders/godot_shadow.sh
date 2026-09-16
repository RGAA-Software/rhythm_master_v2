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
float PointShadowFace(vec3 relative) {
    vec3 magnitude = abs(relative);
    if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z)
        return relative.x >= 0.0 ? 0.0 : 1.0;
    if (magnitude.y >= magnitude.z)
        return relative.y < 0.0 ? 2.0 : 3.0;
    return relative.z >= 0.0 ? 4.0 : 5.0;
}
vec3 PointShadowRay(float face, vec2 clip) {
    // Godot cube camera forward/right/up bases in +X, -X, -Y, +Y, +Z, -Z order.
    if (face < 0.5) return vec3(1.0, -clip.y, -clip.x);
    if (face < 1.5) return vec3(-1.0, -clip.y, clip.x);
    if (face < 2.5) return vec3(clip.x, -1.0, -clip.y);
    if (face < 3.5) return vec3(clip.x, 1.0, clip.y);
    if (face < 4.5) return vec3(clip.x, -clip.y, 1.0);
    return vec3(-clip.x, -clip.y, -1.0);
}
float ShadowTap(vec2 uv, float depth, float face, vec2 offset, float point_cube,
                float forward_depth, vec3 light_position) {
    if (point_cube < 0.5) return ShadowCompare(uv + offset, depth, face);
    // Preserve the source face's forward depth while reconstructing the tap ray.
    // An out-of-face tap is then compared in the adjacent face's projected depth space.
    vec2 tap_uv = uv + offset;
    vec2 tap_clip = vec2(tap_uv.x * 2.0 - 1.0, 1.0 - tap_uv.y * 2.0);
    vec3 relative = PointShadowRay(face, tap_clip) * forward_depth;
    float tap_face = PointShadowFace(relative);
    vec4 projected = PointShadowProjection(tap_face, vec4(light_position + relative, 1.0));
    if (projected.w <= 0.0) return 1.0;
    vec3 clip = projected.xyz / projected.w;
    if (abs(clip.x) > 1.0001 || abs(clip.y) > 1.0001 || abs(clip.z) > 1.0) return 1.0;
    vec2 remapped_uv = vec2(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
    float remapped_depth = clip.z * 0.5 + 0.5 - u_scene_shadow_settings.y;
    return ShadowCompare(remapped_uv, remapped_depth, tap_face);
}
float GodotShadow(vec3 position, vec3 normal, vec3 toward_light, float view_depth,
                  vec3 light_position) {
    vec3 offset = normal * (1.0 - max(0.0, dot(normal, toward_light))) * u_scene_shadow_settings.z;
    float face = u_scene_shadow_cascade.y > 0.5 && view_depth > u_scene_shadow_cascade.x ? 1.0 : 0.0;
    vec4 world = vec4(position + offset, 1.0);
    vec4 projected;
    float point_cube = u_scene_shadow_cascade.z;
    if (point_cube > 0.5) {
        face = PointShadowFace(world.xyz - light_position);
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
        result += ShadowTap(uv, depth, face, vec2(step, 0.0), point_cube, projected.w,
                            light_position);
        result += ShadowTap(uv, depth, face, vec2(-step, 0.0), point_cube, projected.w,
                            light_position);
        result += ShadowTap(uv, depth, face, vec2(0.0, step), point_cube, projected.w,
                            light_position);
        result += ShadowTap(uv, depth, face, vec2(0.0, -step), point_cube, projected.w,
                            light_position);
        return result * 0.2;
    }
    result += ShadowTap(uv, depth, face, vec2(step * 2.0, 0.0), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(-step * 2.0, 0.0), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(0.0, step * 2.0), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(0.0, -step * 2.0), point_cube, projected.w,
                        light_position);
    // Godot avoids the remaining eight comparisons when the distant cross is uniform.
    if (result <= 0.000001) return 0.0;
    if (result >= 4.999999) return 1.0;
    result += ShadowTap(uv, depth, face, vec2(step, 0.0), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(-step, 0.0), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(0.0, step), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(0.0, -step), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(step, step), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(-step, step), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(step, -step), point_cube, projected.w,
                        light_position);
    result += ShadowTap(uv, depth, face, vec2(-step, -step), point_cube, projected.w,
                        light_position);
    return result * (1.0 / 13.0);
}
