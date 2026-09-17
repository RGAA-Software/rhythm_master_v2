$input v_texcoord0, v_texcoord1, v_texcoord2, v_color0
#include <bgfx_shader.sh>
uniform vec4 u_gpu_view;
uniform vec4 u_gpu_atlas;
uniform vec4 u_gpu_soft;
uniform vec4 u_gpu_soft_projection;
SAMPLER2D(s_gpu_atlas, 1);
SAMPLER2D(s_gpu_depth, 2);
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
    // Godot proximity fade: reconstruct the scene distance and smooth-fade
    // across the configured view-space band as the particle approaches it.
    float soft = 1.0;
    if (u_gpu_soft.x > 0.5) {
        float d = texture2D(s_gpu_depth, clamp(v_texcoord2.xy, vec2_splat(0.0),
                                               vec2_splat(1.0)))
                          .r;
        float depth_near = u_gpu_soft_projection.x;
        float depth_far = u_gpu_soft_projection.y;
        float scene_z = u_gpu_soft_projection.z > 0.5
                            ? depth_near + d * (depth_far - depth_near)
                            : (depth_near * depth_far) /
                                  (depth_far * (1.0 - d) + depth_near * d);
        soft = smoothstep(0.0, max(u_gpu_soft.y, 0.0001), scene_z - v_texcoord2.z);
    }
    float alpha = v_color0.a * max(core, halo) * support * shape_alpha * soft;
    gl_FragColor = vec4(v_color0.rgb * tint * alpha, alpha);
}
