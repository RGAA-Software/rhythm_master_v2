$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0, v_color0
#include <bgfx_shader.sh>
uniform vec4 u_gpu_view;
uniform vec4 u_gpu_sample;
SAMPLER2D(s_gpu_sample, 0);
void main()
{
    vec4 sampled = vec4_splat(1.0);
    if (u_gpu_sample.x > 0.0 || u_gpu_sample.y > 0.0)
        sampled = texture2DLod(s_gpu_sample, clamp(i_data0.xy, vec2_splat(0.0), vec2_splat(1.0)), 0.0);
    float luma = clamp(dot(sampled.rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
    float point_size = i_data3.x * mix(1.0, luma, u_gpu_sample.y);
    vec2 size = vec2(point_size * u_gpu_view.x, point_size) * u_gpu_view.w;
    vec2 p = (i_data0.xy + a_position * size) * 2.0 - 1.0;
    p.y = -p.y * u_gpu_view.y;
    gl_Position = vec4(p, 0.0, 1.0);
    if (i_data0.w < 0.0) gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
    v_texcoord0 = a_position;
    float fade = clamp((1.0 - i_data0.w / max(i_data1.w, 0.001)) * 4.0, 0.0, 1.0);
    vec3 straight_color = sampled.a > 0.00001 ? sampled.rgb / sampled.a : vec3_splat(0.0);
    vec3 color = mix(i_data2.rgb, straight_color, u_gpu_sample.x);
    float alpha = i_data2.a * mix(1.0, sampled.a, u_gpu_sample.x);
    v_color0 = vec4(color, alpha * fade * u_gpu_view.z);
}
