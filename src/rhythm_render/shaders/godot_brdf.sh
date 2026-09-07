// Focused adaptation of Godot 4.5.1 drivers/gles3/shaders/scene.glsl:
// D_GGX, V_GGX, SchlickFresnel, F0 and Lambert/Schlick-GGX light_compute paths.
// Copyright (c) 2014-present Godot Engine contributors (see retained AUTHORS.md).
// Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.
// MIT: third_party/notices/godot/LICENSE.txt; provenance/godot_3d.json.
float DistributionGGX(float cos_theta_m, float alpha)
{
    float a = cos_theta_m * alpha;
    float k = alpha / (1.0 - cos_theta_m * cos_theta_m + a * a);
    return k * k * (1.0 / 3.14159265359);
}
// Godot credits Earl Hammon, Jr., "PBR Diffuse Lighting for GGX+Smith Microsurfaces".
float VisibilityGGX(float n_dot_l, float n_dot_v, float alpha)
{
    return 0.5 / mix(2.0 * n_dot_l * n_dot_v, n_dot_l + n_dot_v, alpha);
}
float FresnelSchlick(float u)
{
    float m = 1.0 - u;
    float m2 = m * m;
    return m2 * m2 * m;
}
vec3 GodotDirectional(vec3 normal, vec3 light, vec3 view, vec3 radiance,
                      vec3 albedo, float metallic, float roughness)
{
    float n_dot_l = clamp(dot(normal, light), 0.0, 1.0);
    float n_dot_v = max(dot(normal, view), 1e-4);
    if (n_dot_l <= 0.0) return vec3_splat(0.0);
    vec3 half_vector = normalize(view + light);
    float n_dot_h = clamp(dot(normal, half_vector), 0.0, 1.0);
    float l_dot_h = clamp(dot(light, half_vector), 0.0, 1.0);
    // Godot's default dielectric specular=0.5 gives F0=0.16*0.5*0.5.
    vec3 f0 = mix(vec3_splat(0.04), albedo, metallic);
    float alpha_ggx = roughness * roughness;
    float distribution = DistributionGGX(n_dot_h, alpha_ggx);
    float visibility = VisibilityGGX(n_dot_l, n_dot_v, alpha_ggx);
    float fresnel_power = FresnelSchlick(l_dot_h);
    // Godot's Filament-derived approximate specular occlusion term.
    float f90 = clamp(50.0 * f0.g, 0.0, 1.0);
    vec3 fresnel = f0 + (f90 - f0) * fresnel_power;
    vec3 specular = n_dot_l * distribution * fresnel * visibility;
    vec3 diffuse = albedo * (1.0 - metallic) * n_dot_l * (1.0 / 3.14159265359);
    return (diffuse + specular) * radiance;
}
