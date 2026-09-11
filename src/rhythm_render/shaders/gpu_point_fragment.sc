$input v_texcoord0, v_color0
#include <bgfx_shader.sh>
uniform vec4 u_gpu_view;
void main()
{
    float radius = dot(v_texcoord0, v_texcoord0);
    float glow_radius = max(u_gpu_view.w, 1.0);
    float core = exp2(-4.5 * radius * glow_radius * glow_radius);
    float halo = exp2(-1.5 * radius) * clamp((glow_radius - 1.0) * 0.24, 0.0, 0.72);
    float cutoff = 1.0 - smoothstep(0.78, 1.0, radius);
    float alpha = v_color0.a * max(core, halo) * cutoff;
    gl_FragColor = vec4(v_color0.rgb * alpha, alpha);
}
