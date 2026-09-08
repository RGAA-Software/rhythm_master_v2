$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// GLM 0.9.9.8 color-space equations (MIT alternative) and TiXL Reinhard (MIT).
// See provenance/color_pipeline.json and retained third_party/notices.
SAMPLER2D(s_tex, 0);
uniform vec4 u_color_pipeline;
float ToLinear(float c) {
    c = clamp(c, 0.0, 1.0);
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
float ToSrgb(float c) {
    c = clamp(c, 0.0, 1.0);
    return c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}
void main() {
    vec4 pixel = texture2D(s_tex, v_texcoord0) * v_color0;
    // Additive targets can contain alpha > 1. Preserve radiance and cap coverage.
    float alpha = clamp(pixel.a, 0.0, 1.0);
    vec3 rgb = pixel.rgb / max(alpha, 0.000001);
    if (u_color_pipeline.x > 0.5) rgb = vec3(ToLinear(rgb.r), ToLinear(rgb.g), ToLinear(rgb.b));
    rgb = max(rgb * u_color_pipeline.w, vec3(0.0, 0.0, 0.0));
    if (u_color_pipeline.z > 0.5) rgb = rgb / (rgb + vec3(1.0, 1.0, 1.0));
    if (u_color_pipeline.y > 0.5) rgb = vec3(ToSrgb(rgb.r), ToSrgb(rgb.g), ToSrgb(rgb.b));
    gl_FragColor = vec4(min(rgb, vec3(65504.0, 65504.0, 65504.0)) * alpha, alpha);
}
