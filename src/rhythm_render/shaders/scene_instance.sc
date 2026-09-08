$input a_position, a_normal, a_texcoord0, a_tangent, i_data0, i_data1, i_data2, i_data3, i_data4, i_data5, i_data6, i_data7, i_data8
$output v_world_position, v_world_normal, v_scene_color, v_scene_uv, v_world_tangent, v_world_bitangent
#include <bgfx_shader.sh>
void main()
{
    mat4 model = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    mat4 normal = mtxFromCols(i_data4, i_data5, i_data6, i_data7);
    vec4 world = mul(model, vec4(a_position, 1.0));
    gl_Position = mul(u_viewProj, world);
    v_world_position = world.xyz;
    v_world_normal = mul(normal, vec4(a_normal, 0.0)).xyz;
    v_scene_color = i_data8;
    v_scene_uv = a_texcoord0;
    vec3 n = normalize(v_world_normal);
    vec3 t = mul(model, vec4(a_tangent.xyz, 0.0)).xyz;
    t = t - n * dot(n, t);
    t /= max(length(t), 0.000001);
    float orientation = dot(cross(i_data0.xyz, i_data1.xyz), i_data2.xyz) < 0.0 ? -1.0 : 1.0;
    v_world_tangent = t;
    v_world_bitangent = cross(n, t) * a_tangent.w * orientation;
}
