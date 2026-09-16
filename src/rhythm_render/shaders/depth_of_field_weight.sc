$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Godot signed near/far CoC generation, MIT; provenance/depth_pipeline.json.
SAMPLER2D(s_displace_map, 1);
uniform vec4 u_depth_settings; // near, far, orthographic, unused
uniform vec4 u_dof_settings; // focus, focus scale, max radius pixels, unused
float Distance(vec2 uv) {
    float d = texture2D(s_displace_map, uv).r;
    float n = u_depth_settings.x;
    float f = u_depth_settings.y;
    return u_depth_settings.z > 0.5 ? n + d * (f - n) : (n * f) / (f * (1.0 - d) + n * d);
}
void main() {
    float distance = Distance(v_texcoord0);
    float weight = clamp((1.0 / u_dof_settings.x - 1.0 / max(distance, 0.001)) *
                         u_dof_settings.y, -1.0, 1.0) * u_dof_settings.z;
    gl_FragColor = vec4(weight, 0.0, 0.0, 1.0);
}
