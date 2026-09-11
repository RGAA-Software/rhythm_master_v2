$input v_world_position, v_world_normal, v_scene_color, v_scene_uv, v_world_tangent, v_world_bitangent
#include <bgfx_shader.sh>
uniform vec4 u_scene_alpha;
uniform vec4 u_scene_textures;
uniform vec4 u_scene_uv;
SAMPLER2D(s_scene_base, 0);
void main()
{
    vec2 uv = v_scene_uv * u_scene_uv.xy + u_scene_uv.zw;
    float coverage = v_scene_color.a;
    if (u_scene_textures.x > 0.5) coverage *= texture2D(s_scene_base, uv).a;
    if (coverage < u_scene_alpha.y) discard;
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
