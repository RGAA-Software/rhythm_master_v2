$input a_position, i_data0
$output v_color0
#include <bgfx_shader.sh>
void main()
{
    gl_Position = vec4(a_position + i_data0.xy, 0.0, 1.0);
    v_color0 = vec4(i_data0.zw, 0.0, 1.0);
}
