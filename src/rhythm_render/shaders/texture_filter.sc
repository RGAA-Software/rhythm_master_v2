$input v_color0, v_texcoord0

#include <bgfx_shader.sh>

// Adapted from TiXL Bloom-SeparableBlurPS.hlsl and Bloom-DownsamplePS.hlsl.
// Copyright 2010-2016 Thomas Mann, Daniel Szymanski, Andreas Rose, Framefield GmbH.
// MIT: third_party/notices/tixl-effects/LICENSE.txt; provenance/tixl_effects.json.
SAMPLER2D(s_tex, 0);
uniform vec4 u_texture_filter;

void main()
{
    vec2 step = u_texture_filter.xy;
    vec4 color;
    if (u_texture_filter.z > 0.5)
    {
        color = texture2D(s_tex, v_texcoord0 + vec2(-step.x, -step.y));
        color += texture2D(s_tex, v_texcoord0 + vec2(step.x, -step.y));
        color += texture2D(s_tex, v_texcoord0 + vec2(-step.x, step.y));
        color += texture2D(s_tex, v_texcoord0 + vec2(step.x, step.y));
        color *= 0.25;
    }
    else
    {
        color = texture2D(s_tex, v_texcoord0) * 0.2270270270;
        color += texture2D(s_tex, v_texcoord0 + step * 1.3846153846) * 0.3162162162;
        color += texture2D(s_tex, v_texcoord0 - step * 1.3846153846) * 0.3162162162;
        color += texture2D(s_tex, v_texcoord0 + step * 3.2307692308) * 0.0702702703;
        color += texture2D(s_tex, v_texcoord0 - step * 3.2307692308) * 0.0702702703;
    }
    gl_FragColor = color * v_color0;
}
