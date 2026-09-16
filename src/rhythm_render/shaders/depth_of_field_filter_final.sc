$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_displace_map, 1);
uniform vec4 u_dof_filter; // shape, second pass, blur radius, steps/scale
uniform vec4 u_dof_domain; // reciprocal sampling width/height, unused
#include "godot_dof_filter.sh"
void main() {
    vec4 color;
    float weight;
    DofFilter(v_texcoord0, v_color0, color, weight);
    gl_FragColor = color;
}
