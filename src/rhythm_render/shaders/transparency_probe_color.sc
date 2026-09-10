$input v_texcoord0
#include <bgfx_shader.sh>
uniform vec4 u_transparency_color;
void main()
{
    gl_FragColor = vec4(u_transparency_color.rgb * u_transparency_color.a, u_transparency_color.a);
}
