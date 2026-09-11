$input v_color0, v_texcoord0

#include <bgfx_shader.sh>

// Adapted from Godot glow.glsl MODE_DOWNSAMPLE (MIT).
// Copyright (c) 2014-present Godot Engine contributors.
SAMPLER2D(s_tex, 0);
uniform vec4 u_glow_domain;

void main()
{
    vec2 half_pixel = u_glow_domain.xy * 0.5;
    vec3 color = texture2D(s_tex, v_texcoord0).rgb * 4.0;
    color += texture2D(s_tex, v_texcoord0 - half_pixel).rgb;
    color += texture2D(s_tex, v_texcoord0 + half_pixel).rgb;
    color += texture2D(s_tex, v_texcoord0 - vec2(half_pixel.x, -half_pixel.y)).rgb;
    color += texture2D(s_tex, v_texcoord0 + vec2(half_pixel.x, -half_pixel.y)).rgb;
    gl_FragColor = vec4(color / 8.0, 0.0);
}
