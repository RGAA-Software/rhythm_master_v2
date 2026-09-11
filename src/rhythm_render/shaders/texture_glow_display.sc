$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Godot glow blend ordering and equations (MIT).
// See provenance/godot_glow.json and retained third_party/notices.
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_glow, 1);
uniform vec4 u_glow_display;
#include "godot_tonemap.sh"
vec3 SoftLight(vec3 color, vec3 glow) {
    glow = clamp(glow, 0.0, 1.0);
    vec3 low = ((16.0 * color - 12.0) * color + 4.0) * color;
    vec3 curve = vec3(color.r <= 0.25 ? low.r : sqrt(color.r),
                      color.g <= 0.25 ? low.g : sqrt(color.g),
                      color.b <= 0.25 ? low.b : sqrt(color.b));
    vec3 applied = color + glow * (curve - color);
    return vec3(color.r > 1.0 ? color.r : applied.r,
                color.g > 1.0 ? color.g : applied.g,
                color.b > 1.0 ? color.b : applied.b);
}
vec3 PreToneGlow(vec3 color, vec3 glow, float mode, float strength) {
    if (mode < 0.5) return color + glow;
    if (mode < 1.5) {
        glow = clamp(glow, 0.0, u_glow_display.z);
        return color + glow - color * glow / u_glow_display.z;
    }
    if (mode < 3.5) return glow;
    return color * (1.0 - clamp(strength, 0.0, 1.0)) + glow;
}
void main() {
    vec4 pixel = texture2D(s_tex, v_texcoord0) * v_color0;
    float alpha = clamp(pixel.a, 0.0, 1.0);
    bool has_coverage = alpha > 0.000001;
    vec3 color = pixel.rgb / (has_coverage ? alpha : 1.0);
    if (u_color_pipeline.x > 0.5)
        color = vec3(ToLinear(color.r), ToLinear(color.g), ToLinear(color.b));
    color = max(color * u_color_pipeline.w, vec3(0.0, 0.0, 0.0));
    float mode = u_glow_display.x;
    float strength = u_glow_display.y;
    vec3 glow = max(texture2D(s_glow, v_texcoord0).rgb * strength, vec3(0.0, 0.0, 0.0));
    if (mode < 1.5 || mode > 2.5) color = PreToneGlow(color, glow, mode, strength);
    color = ApplyToneMapping(color);
    if (mode > 1.5 && mode < 2.5)
        color = SoftLight(color, ApplyToneMapping(glow));
    if (u_color_pipeline.y > 0.5)
        color = vec3(ToSrgb(color.r), ToSrgb(color.g), ToSrgb(color.b));
    float coverage = has_coverage ? alpha : 1.0;
    gl_FragColor = vec4(min(color, vec3(65504.0, 65504.0, 65504.0)) * coverage, alpha);
}
