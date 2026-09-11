$input v_color0, v_texcoord0

#include <bgfx_shader.sh>

// Adapted from Godot blur_raster.glsl MODE_GAUSSIAN_BLUR (MIT).
// Copyright (c) 2014-present Godot Engine contributors.
// See provenance/godot_blur.json and third_party/notices/godot/LICENSE.txt.
SAMPLER2D(s_tex, 0);
uniform vec4 u_texture_filter;

void main()
{
    vec2 step = u_texture_filter.xy;
    vec4 a = texture2D(s_tex, v_texcoord0 + step * vec2(-1.0, -1.0));
    vec4 b = texture2D(s_tex, v_texcoord0 + step * vec2(0.0, -1.0));
    vec4 c = texture2D(s_tex, v_texcoord0 + step * vec2(1.0, -1.0));
    vec4 d = texture2D(s_tex, v_texcoord0 + step * vec2(-0.5, -0.5));
    vec4 e = texture2D(s_tex, v_texcoord0 + step * vec2(0.5, -0.5));
    vec4 f = texture2D(s_tex, v_texcoord0 + step * vec2(-1.0, 0.0));
    vec4 g = texture2D(s_tex, v_texcoord0);
    vec4 h = texture2D(s_tex, v_texcoord0 + step * vec2(1.0, 0.0));
    vec4 i = texture2D(s_tex, v_texcoord0 + step * vec2(-0.5, 0.5));
    vec4 j = texture2D(s_tex, v_texcoord0 + step * vec2(0.5, 0.5));
    vec4 k = texture2D(s_tex, v_texcoord0 + step * vec2(-1.0, 1.0));
    vec4 l = texture2D(s_tex, v_texcoord0 + step * vec2(0.0, 1.0));
    vec4 m = texture2D(s_tex, v_texcoord0 + step * vec2(1.0, 1.0));
    float base_weight = 0.5 / 4.0;
    float lesser_weight = 0.125 / 4.0;
    vec4 color = (d + e + i + j) * base_weight;
    color += (a + b + g + f) * lesser_weight;
    color += (b + c + h + g) * lesser_weight;
    color += (f + g + l + k) * lesser_weight;
    color += (g + h + m + l) * lesser_weight;
    gl_FragColor = color * v_color0;
}
