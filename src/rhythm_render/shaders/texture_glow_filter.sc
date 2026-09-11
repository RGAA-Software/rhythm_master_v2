$input v_color0, v_texcoord0

#include <bgfx_shader.sh>

// Adapted from Godot glow.glsl MODE_FILTER (MIT), dual-filtering glow.
// Copyright (c) 2014-present Godot Engine contributors.
// See provenance/godot_glow.json and third_party/notices/godot/LICENSE.txt.
SAMPLER2D(s_tex, 0);
uniform vec4 u_glow_settings;
uniform vec4 u_glow_domain;

void main()
{
    vec2 half_pixel = u_glow_domain.xy * 0.5;
    vec3 color = texture2D(s_tex, v_texcoord0).rgb * 4.0;
    color += texture2D(s_tex, v_texcoord0 - half_pixel).rgb;
    color += texture2D(s_tex, v_texcoord0 + half_pixel).rgb;
    color += texture2D(s_tex, v_texcoord0 - vec2(half_pixel.x, -half_pixel.y)).rgb;
    color += texture2D(s_tex, v_texcoord0 + vec2(half_pixel.x, -half_pixel.y)).rgb;
    color /= 8.0;
    float maximum = max(color.r, max(color.g, color.b));
    float feedback = max(smoothstep(u_glow_settings.x,
                                    u_glow_settings.x + u_glow_settings.y, maximum),
                         u_glow_settings.z);
    color = min(color * feedback,
                vec3(u_glow_settings.w, u_glow_settings.w, u_glow_settings.w));
    gl_FragColor = vec4(color, 0.0);
}
