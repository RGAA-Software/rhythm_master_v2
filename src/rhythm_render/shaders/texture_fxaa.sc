$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Adapted from glsl-fxaa 3.0.0, copyright (c) 2014 Matt DesLauriers (MIT),
// based on copyright (c) 2011 Armin Ronacher (BSD-3-Clause).
// Retained notices: third_party/notices/glsl-fxaa; provenance/fxaa.json.
SAMPLER2D(s_tex, 0);
uniform vec4 u_fxaa_settings; // span, reduce multiplier, reduce minimum, strength
uniform vec4 u_fxaa_domain; // reciprocal source width/height

float Luma(vec4 color, float coverage_weight) {
    return dot(color.rgb, vec3(0.299, 0.587, 0.114)) + color.a * coverage_weight;
}
void main() {
    vec4 center = texture2D(s_tex, v_texcoord0);
    vec2 pixel = u_fxaa_domain.xy;
    vec4 nw = texture2D(s_tex, v_texcoord0 + vec2(-pixel.x, -pixel.y));
    vec4 ne = texture2D(s_tex, v_texcoord0 + vec2(pixel.x, -pixel.y));
    vec4 sw = texture2D(s_tex, v_texcoord0 + vec2(-pixel.x, pixel.y));
    vec4 se = texture2D(s_tex, v_texcoord0 + vec2(pixel.x, pixel.y));
    float alpha_min = min(center.a, min(min(nw.a, ne.a), min(sw.a, se.a)));
    float alpha_max = max(center.a, max(max(nw.a, ne.a), max(sw.a, se.a)));
    // Opaque input follows the upstream luminance equations. Coverage variation
    // also needs a direction, including black silhouettes on transparency.
    float coverage_weight = alpha_max - alpha_min > 0.0001 ? 0.5 : 0.0;
    float luma_nw = Luma(nw, coverage_weight);
    float luma_ne = Luma(ne, coverage_weight);
    float luma_sw = Luma(sw, coverage_weight);
    float luma_se = Luma(se, coverage_weight);
    float luma_m = Luma(center, coverage_weight);
    float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
    float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));
    vec2 direction = vec2(-((luma_nw + luma_ne) - (luma_sw + luma_se)),
                          (luma_nw + luma_sw) - (luma_ne + luma_se));
    float reduce = max((luma_nw + luma_ne + luma_sw + luma_se) *
                       (0.25 * u_fxaa_settings.y), u_fxaa_settings.z);
    float reciprocal = 1.0 / (min(abs(direction.x), abs(direction.y)) + reduce);
    vec2 span = vec2(u_fxaa_settings.x, u_fxaa_settings.x);
    direction = clamp(direction * reciprocal, -span, span) * pixel;
    vec4 first = 0.5 * (texture2D(s_tex, v_texcoord0 + direction * (1.0 / 3.0 - 0.5)) +
                        texture2D(s_tex, v_texcoord0 + direction * (2.0 / 3.0 - 0.5)));
    vec4 second = first * 0.5 + 0.25 *
            (texture2D(s_tex, v_texcoord0 - direction * 0.5) +
             texture2D(s_tex, v_texcoord0 + direction * 0.5));
    float luma_b = Luma(second, coverage_weight);
    vec4 filtered = luma_b < luma_min || luma_b > luma_max ? first : second;
    gl_FragColor = mix(center, filtered, u_fxaa_settings.w) * v_color0;
}
