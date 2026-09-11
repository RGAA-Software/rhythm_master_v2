$input v_texcoord0, v_color0
#include <bgfx_shader.sh>
void main()
{
    float radius = dot(v_texcoord0, v_texcoord0);
    // A Gaussian core retains a bright center while the feathered cutoff removes
    // the quad corners. This keeps enlarged particles luminous rather than flat discs.
    float core = exp2(-4.5 * radius);
    float cutoff = 1.0 - smoothstep(0.78, 1.0, radius);
    float alpha = v_color0.a * core * cutoff;
    gl_FragColor = vec4(v_color0.rgb * alpha, alpha);
}
