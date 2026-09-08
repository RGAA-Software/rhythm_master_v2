#include <bgfx_shader.sh>
#include "mesh_deformation.sh"
#ifdef RHYTHM_SKIN
#include "mesh_skinning.sh"
#endif
void main()
{
    mat4 model = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    mat4 normal = mtxFromCols(i_data4, i_data5, i_data6, i_data7);
    vec3 position = a_position, local_normal = a_normal, tangent = a_tangent.xyz;
    float skin_orientation = 1.0;
#ifdef RHYTHM_SKIN
    skin_orientation = SkinMesh(position, local_normal, tangent, a_indices, a_weight);
#endif
    DeformMesh(position, local_normal, tangent);
    vec4 world = mul(model, vec4(position, 1.0));
    gl_Position = mul(u_viewProj, world);
    v_world_position = world.xyz;
    v_world_normal = mul(normal, vec4(local_normal, 0.0)).xyz;
    v_scene_color = i_data8;
    v_scene_uv = a_texcoord0;
    vec3 n = normalize(v_world_normal);
    vec3 t = mul(model, vec4(tangent, 0.0)).xyz;
    t = t - n * dot(n, t);
    t /= max(length(t), 0.000001);
    float orientation = dot(cross(i_data0.xyz, i_data1.xyz), i_data2.xyz) < 0.0 ? -1.0 : 1.0;
    v_world_tangent = t;
    v_world_bitangent = cross(n, t) * a_tangent.w * skin_orientation * orientation;
}
