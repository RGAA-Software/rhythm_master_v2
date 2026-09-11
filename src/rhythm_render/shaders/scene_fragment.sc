$input v_world_position, v_world_normal, v_scene_color, v_scene_uv, v_world_tangent, v_world_bitangent
#include <bgfx_shader.sh>
#include "godot_brdf.sh"
#include "godot_lights.sh"
#include "godot_shadow.sh"
#include "godot_environment.sh"
uniform vec4 u_scene_material;
uniform vec4 u_scene_emissive;
uniform vec4 u_scene_camera;
uniform vec4 u_scene_view;
uniform vec4 u_scene_light_directions[4];
uniform vec4 u_scene_light_colors[4];
uniform vec4 u_scene_light_positions[4];
uniform vec4 u_scene_spot_directions[4];
uniform vec4 u_scene_light_ranges[4];
SAMPLER2D(s_scene_base, 0);
SAMPLER2D(s_scene_normal, 1);
SAMPLER2D(s_scene_orm, 2);
SAMPLER2D(s_scene_emission, 3);
uniform vec4 u_scene_textures;
uniform vec4 u_scene_texture_options;
uniform vec4 u_scene_uv;
uniform vec4 u_scene_alpha;
// GLM MIT color transfer, shared semantics with color_pipeline.sc.
float MaterialLinear(float c) {
    c = clamp(c, 0.0, 1.0);
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
vec3 MaterialColor(vec4 pixel) {
    vec3 color = pixel.rgb / max(pixel.a, 0.000001);
    if (u_scene_texture_options.x > 0.5)
        color = vec3(MaterialLinear(color.r), MaterialLinear(color.g), MaterialLinear(color.b));
    return color;
}
void main()
{
    vec2 uv = v_scene_uv * u_scene_uv.xy + u_scene_uv.zw;
    vec3 base = v_scene_color.rgb;
    float coverage = v_scene_color.a;
    if (u_scene_textures.x > 0.5) {
        vec4 base_pixel = texture2D(s_scene_base, uv);
        base *= MaterialColor(base_pixel);
        if (u_scene_alpha.x > 0.5) coverage *= base_pixel.a;
    }
    vec3 color = base;
    if (u_scene_material.z < 0.5)
    {
        vec3 view_vector = u_scene_camera.xyz - v_world_position;
        vec3 view = view_vector / max(length(view_vector), 1e-6);
        if (u_scene_view.w > 0.5) view = u_scene_view.xyz;
        vec3 normal = v_world_normal / max(length(v_world_normal), 1e-6);
        vec3 geometric_normal = normal;
        float facing = u_scene_material.w > 0.5 && dot(normal, view) < 0.0 ? -1.0 : 1.0;
        if (u_scene_textures.y > 0.5) {
            vec3 map = texture2D(s_scene_normal, uv).xyz * 2.0 - 1.0;
            map.xy *= u_scene_texture_options.y;
            // Godot/MikkTSpace TBN interpolation; preserve glTF RGB signed Z.
            vec3 mapped = v_world_tangent * map.x + v_world_bitangent * map.y + v_world_normal * map.z;
            normal = mapped / max(length(mapped), 0.000001);
        }
        normal *= facing;
        color = u_scene_emissive.rgb;
        if (u_scene_textures.w > 0.5) color *= MaterialColor(texture2D(s_scene_emission, uv));
        float ao = 1.0;
        float metallic = u_scene_material.x;
        float roughness = u_scene_material.y;
        if (u_scene_textures.z > 0.5) {
            vec3 orm = texture2D(s_scene_orm, uv).rgb;
            ao = orm.r;
            roughness *= orm.g;
            metallic *= orm.b;
        }
        color += GodotEnvironment(normal, view, base, metallic, roughness) * ao;
        for (int i = 0; i < 4; ++i)
        {
            if (float(i) < u_scene_camera.w)
            {
                vec3 direction = u_scene_light_directions[i].xyz;
                float attenuation = 1.0;
                if (u_scene_light_positions[i].w > 0.5)
                {
                    vec3 relative = u_scene_light_positions[i].xyz - v_world_position;
                    float distance = length(relative);
                    direction = relative / max(distance, 0.0001);
                    attenuation = GodotAttenuation(distance, u_scene_light_ranges[i].x, u_scene_light_ranges[i].y);
                    if (u_scene_light_positions[i].w > 1.5)
                        attenuation *= GodotSpot(direction, u_scene_spot_directions[i].xyz,
                            u_scene_spot_directions[i].w, u_scene_light_ranges[i].z);
                }
                if (abs(float(i) - u_scene_shadow_settings.x) < 0.5)
                    attenuation *= GodotShadow(v_world_position, geometric_normal, direction);
                color += GodotDirectional(normal, direction, view,
                    u_scene_light_colors[i].rgb * attenuation, base,
                    metallic, max(roughness, 0.05));
            }
        }
    }
    gl_FragColor = vec4(min(color, vec3(65504.0, 65504.0, 65504.0)) * coverage, coverage);
}
