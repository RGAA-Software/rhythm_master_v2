$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_color0
#include <bgfx_shader.sh>
uniform vec4 u_attribute_probe;
void main()
{
    gl_Position = vec4((u_attribute_probe.x + a_position.x * 0.5) / 64.0 - 1.0,
                       a_position.y, 0.0, 1.0);
    v_color0 = u_attribute_probe.y < 0.5 ? i_data0
             : u_attribute_probe.y < 1.5 ? i_data1
             : u_attribute_probe.y < 2.5 ? i_data2 : i_data3;
}
