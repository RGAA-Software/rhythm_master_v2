$input v_world_position, v_world_normal
#include <bgfx_shader.sh>
#include "godot_brdf.sh"
uniform vec4 u_scene_color;
uniform vec4 u_scene_material;
uniform vec4 u_scene_emissive;
uniform vec4 u_scene_camera;
uniform vec4 u_scene_view;
uniform vec4 u_scene_light_directions[4];
uniform vec4 u_scene_light_colors[4];
void main()
{
    vec3 color = u_scene_color.rgb;
    if (u_scene_material.z < 0.5)
    {
        vec3 view_vector = u_scene_camera.xyz - v_world_position;
        vec3 view = view_vector / max(length(view_vector), 1e-6);
        if (u_scene_view.w > 0.5) view = u_scene_view.xyz;
        vec3 normal = v_world_normal / max(length(v_world_normal), 1e-6);
        if (u_scene_material.w > 0.5 && dot(normal, view) < 0.0) normal = -normal;
        color = u_scene_emissive.rgb;
        for (int i = 0; i < 4; ++i)
        {
            if (float(i) < u_scene_camera.w)
                color += GodotDirectional(normal, u_scene_light_directions[i].xyz, view,
                    u_scene_light_colors[i].rgb, u_scene_color.rgb,
                    u_scene_material.x, max(u_scene_material.y, 0.05));
        }
    }
    gl_FragColor = vec4(color * u_scene_color.a, u_scene_color.a);
}
