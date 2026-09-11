$input v_color0, v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);
uniform vec4 u_glow_settings;

void main()
{
    gl_FragColor = vec4(texture2D(s_tex, v_texcoord0).rgb * u_glow_settings.x, 0.0);
}
