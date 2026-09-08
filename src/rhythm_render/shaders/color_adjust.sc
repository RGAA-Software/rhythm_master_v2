$input v_color0, v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);
uniform vec4 u_color_adjust;
uniform vec4 u_color_limits;

void main()
{
    vec4 pixel = texture2D(s_tex, v_texcoord0) * v_color0;
    vec3 rgb = pixel.rgb / max(pixel.a, 0.000001);
    float luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    rgb = mix(vec3(luminance, luminance, luminance), rgb, u_color_adjust.z);
    rgb = ((rgb - 0.5) * u_color_adjust.y + 0.5) * u_color_adjust.x;
    rgb = mix(rgb, vec3(1.0, 1.0, 1.0) - rgb, u_color_adjust.w);
    gl_FragColor = vec4(clamp(rgb, 0.0, u_color_limits.x) * pixel.a, pixel.a);
}
