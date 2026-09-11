$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// GLM 0.9.9.8 transfer equations and Godot tone mapping (MIT).
// See provenance/color_pipeline.json and retained third_party/notices.
SAMPLER2D(s_tex, 0);
#include "godot_tonemap.sh"
void main() {
    vec4 pixel = texture2D(s_tex, v_texcoord0) * v_color0;
    // Additive targets can contain alpha > 1. Preserve radiance and cap coverage.
    float alpha = clamp(pixel.a, 0.0, 1.0);
    bool has_coverage = alpha > 0.000001;
    vec3 rgb = pixel.rgb / (has_coverage ? alpha : 1.0);
    if (u_color_pipeline.x > 0.5) rgb = vec3(ToLinear(rgb.r), ToLinear(rgb.g), ToLinear(rgb.b));
    rgb = max(rgb * u_color_pipeline.w, vec3(0.0, 0.0, 0.0));
    rgb = ApplyToneMapping(rgb);
    if (u_color_pipeline.y > 0.5) rgb = vec3(ToSrgb(rgb.r), ToSrgb(rgb.g), ToSrgb(rgb.b));
    float coverage = has_coverage ? alpha : 1.0;
    gl_FragColor = vec4(min(rgb, vec3(65504.0, 65504.0, 65504.0)) * coverage, alpha);
}
