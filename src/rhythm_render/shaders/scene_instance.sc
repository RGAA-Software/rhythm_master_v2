$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3, i_data4, i_data5, i_data6, i_data7, i_data8
$output v_world_position, v_world_normal, v_scene_color
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
}
