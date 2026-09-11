$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Godot circular bokeh golden-angle gather, MIT; provenance/depth_pipeline.json.
// Sample raw depth with the matching camera; accumulate premultiplied RGBA.
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_displace_map, 1);
uniform vec4 u_depth_settings; // near, far, orthographic, unused
uniform vec4 u_dof_settings; // focus, focus scale, max radius pixels, sample count
uniform vec4 u_dof_domain; // reciprocal width/height, radial step, unused
float Distance(vec2 uv) {
    float d = texture2D(s_displace_map, uv).r;
    float n = u_depth_settings.x;
    float f = u_depth_settings.y;
    return u_depth_settings.z > 0.5 ? n + d * (f - n) : (n * f) / (f * (1.0 - d) + n * d);
}
float BlurWeight(float distance) {
    // Godot uses negative CoC for near blur and positive CoC for far blur.
    return clamp((1.0 / u_dof_settings.x - 1.0 / max(distance, 0.001)) *
                 u_dof_settings.y, -1.0, 1.0) * u_dof_settings.z;
}
void main() {
    float center_depth = Distance(v_texcoord0);
    float center_weight = BlurWeight(center_depth);
    vec4 color = texture2D(s_tex, v_texcoord0);
    float total = 1.0;
    float radius = sqrt(u_dof_domain.z);
    for (int i = 0; i < 64; ++i) {
        if (float(i) >= u_dof_settings.w || radius >= u_dof_settings.z) break;
        float angle = float(i) * 2.39996323;
        vec2 uv = v_texcoord0 + vec2(cos(angle), sin(angle)) * u_dof_domain.xy * radius;
        float sample_weight = BlurWeight(Distance(uv));
        float limit = abs(sample_weight);
        if (sample_weight > center_weight)
            limit = min(limit, abs(center_weight) * 2.0);
        float weight = smoothstep(radius - 0.5, radius + 0.5, limit);
        color += mix(color / total, texture2D(s_tex, uv), weight);
        total += 1.0;
        radius += u_dof_domain.z / radius;
    }
    gl_FragColor = color / total * v_color0;
}
