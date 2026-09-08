$input v_texcoord0, v_color0
#include <bgfx_shader.sh>
// Perspective reconstruction adapted from TiXL depth-to-linear.hlsl, MIT.
// See provenance/depth_pipeline.json. Canonical increasing window depth [0,1].
SAMPLER2D(s_tex, 0);
uniform vec4 u_depth_settings; // near, far, orthographic, normalized
void main() {
    float d = texture2D(s_tex, v_texcoord0).r;
    float n = u_depth_settings.x;
    float f = u_depth_settings.y;
    float z = u_depth_settings.z > 0.5 ? n + d * (f - n) : (n * f) / (f * (1.0 - d) + n * d);
    if (u_depth_settings.w > 0.5) z = (z - n) / (f - n);
    gl_FragColor = vec4(min(z, 65504.0), min(z, 65504.0), min(z, 65504.0), 1.0);
}
