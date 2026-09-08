$input v_texcoord0, v_color0
#include <bgfx_shader.sh>
void main()
{
    float radius = dot(v_texcoord0, v_texcoord0);
    float alpha = v_color0.a * (1.0 - smoothstep(0.0, 1.0, radius));
    gl_FragColor = vec4(v_color0.rgb * alpha, alpha);
}
