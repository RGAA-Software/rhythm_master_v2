#include <bgfx_shader.sh>
#include "mesh_deformation.sh"
#ifdef RHYTHM_SKIN
#include "mesh_skinning.sh"
#endif
uniform mat4 u_scene_normal;
uniform vec4 u_scene_color;
void main()
{
    vec3 position = a_position, normal = a_normal, tangent = a_tangent.xyz;
    float skin_orientation = 1.0;
#ifdef RHYTHM_SKIN
    skin_orientation = SkinMesh(position, normal, tangent, a_indices, a_weight);
#endif
    DeformMesh(position, normal, tangent);
    gl_Position = mul(u_modelViewProj, vec4(position, 1.0));
    v_world_position = mul(u_model[0], vec4(position, 1.0)).xyz;
    v_world_normal = mul(u_scene_normal, vec4(normal, 0.0)).xyz;
    v_scene_color = u_scene_color;
    v_scene_uv = a_texcoord0;
    vec3 n = normalize(v_world_normal);
    vec3 t = mul(u_model[0], vec4(tangent, 0.0)).xyz;
    t = t - n * dot(n, t);
    t /= max(length(t), 0.000001);
    float orientation = dot(cross(u_model[0][0].xyz, u_model[0][1].xyz), u_model[0][2].xyz) < 0.0 ? -1.0 : 1.0;
    v_world_tangent = t;
    v_world_bitangent = cross(n, t) * a_tangent.w * skin_orientation * orientation;
}
