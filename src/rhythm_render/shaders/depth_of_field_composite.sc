$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Godot half-resolution bokeh composite adapted to an explicit graph target.
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_displace_map, 1);
SAMPLER2D(s_dof_original_weight, 2);
SAMPLER2D(s_dof_original_color, 3);
void main() {
    vec4 blurred = texture2D(s_tex, v_texcoord0);
    float center_weight = texture2D(s_displace_map, v_texcoord0).r;
    float sample_weight = texture2D(s_dof_original_weight, v_texcoord0).r;
    float mix_amount = sample_weight < center_weight
                               ? clamp(max(abs(center_weight), abs(sample_weight)), 0.0, 1.0)
                               : clamp(abs(center_weight), 0.0, 1.0);
    vec4 original = texture2D(s_dof_original_color, v_texcoord0);
    gl_FragColor = vec4(mix(original.rgb, blurred.rgb, mix_amount), original.a) * v_color0;
}
