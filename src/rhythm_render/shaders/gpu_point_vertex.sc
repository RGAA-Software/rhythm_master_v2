$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0, v_color0
#include <bgfx_shader.sh>
uniform vec4 u_gpu_view;
void main()
{
    vec2 size = vec2(i_data3.x * u_gpu_view.x, i_data3.x);
    vec2 p = (i_data0.xy + a_position * size) * 2.0 - 1.0;
    p.y = -p.y * u_gpu_view.y;
    gl_Position = vec4(p, 0.0, 1.0);
    if (i_data0.w < 0.0) gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
    v_texcoord0 = a_position;
    float fade = clamp((1.0 - i_data0.w / max(i_data1.w, 0.001)) * 4.0, 0.0, 1.0);
    v_color0 = vec4(i_data2.rgb, i_data2.a * fade * u_gpu_view.z);
}
