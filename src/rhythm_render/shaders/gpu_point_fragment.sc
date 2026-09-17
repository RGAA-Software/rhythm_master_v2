$input v_texcoord0, v_texcoord1, v_color0
#include <bgfx_shader.sh>
uniform vec4 u_gpu_view;
uniform vec4 u_gpu_atlas;
SAMPLER2D(s_gpu_atlas, 1);
void main()
{
    float radius = dot(v_texcoord0, v_texcoord0);
    float glow_radius = max(u_gpu_view.w, 1.0);
    float core = exp2(-4.5 * radius * glow_radius * glow_radius);
    float halo = exp2(-1.5 * radius) * clamp((glow_radius - 1.0) * 0.24, 0.0, 0.72);
    // Fade across most of the sprite support. A narrow edge cutoff makes large
    // points read as discs even when the analytic core itself is smooth.
    float support = 1.0 - smoothstep(0.20, 1.0, radius);
    vec3 tint = vec3_splat(1.0);
    float shape_alpha = 1.0;
    if (u_gpu_atlas.z > 0.5) {
        vec2 uv = clamp(v_texcoord0 * 0.5 + 0.5, vec2_splat(0.0), vec2_splat(1.0)) *
                      v_texcoord1.zw +
                  v_texcoord1.xy;
        vec4 sprite = texture2D(s_gpu_atlas, uv);
        tint = sprite.a > 0.00001 ? sprite.rgb / sprite.a : vec3_splat(0.0);
        shape_alpha = sprite.a;
    }
    float alpha = v_color0.a * max(core, halo) * support * shape_alpha;
    gl_FragColor = vec4(v_color0.rgb * tint * alpha, alpha);
}
