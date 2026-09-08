// Godot 4.5.1 MIT environment BRDF approximation (Lazarov 2013).
// Adapted atlas sampling; see provenance/environment_lighting.json and notices.
SAMPLER2D(s_scene_environment, 5);
uniform vec4 u_scene_environment; // energy, cos(rotation), sin(rotation)
vec3 EnvironmentAtlas(vec3 direction, float tile) {
    direction = vec3(direction.x * u_scene_environment.y - direction.z * u_scene_environment.z,
                     direction.y, direction.x * u_scene_environment.z + direction.z * u_scene_environment.y);
    vec2 uv = vec2(fract(atan2(direction.z, direction.x) / 6.28318530718 + 0.5),
                   acos(clamp(direction.y, -1.0, 1.0)) / 3.14159265359);
    uv = (uv * vec2(128.0, 64.0) + vec2(tile * 130.0 + 1.0, 1.0)) / vec2(780.0, 66.0);
    return texture2D(s_scene_environment, uv).rgb;
}
vec3 GodotEnvironment(vec3 normal, vec3 view, vec3 base, float metallic, float roughness) {
    if (u_scene_environment.x <= 0.0) return vec3(0.0, 0.0, 0.0);
    vec3 reflection = reflect(-view, normal);
    float level = clamp(roughness, 0.0, 1.0) * 4.0;
    float lower = floor(level);
    vec3 specular = mix(EnvironmentAtlas(reflection, lower),
                        EnvironmentAtlas(reflection, min(lower + 1.0, 4.0)), level - lower);
    vec3 diffuse = EnvironmentAtlas(normal, 5.0) * base * (1.0 - metallic);
    vec3 f0 = mix(vec3(0.04, 0.04, 0.04), base, metallic);
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float ndotv = clamp(dot(normal, view), 0.0, 1.0);
    float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
    vec2 env = vec2(-1.04, 1.04) * a004 + r.zw;
    specular *= env.x * f0 + env.y * clamp(50.0 * f0.g, metallic, 1.0);
    return max(diffuse + specular, vec3(0.0, 0.0, 0.0)) * u_scene_environment.x;
}
