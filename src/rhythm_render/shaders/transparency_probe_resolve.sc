$input v_texcoord0
// Accumulation normalization adapted from bgfx example 19 (BSD-2-Clause).
// Copyright 2011-2025 Branimir Karadzic. All rights reserved.
// provenance/bgfx_oit.json; third_party/notices/bgfx-oit/LICENSE.
#include <bgfx_shader.sh>
SAMPLER2D(s_transparency_accumulation, 0);
SAMPLER2D(s_transparency_revealage, 1);
void main()
{
    vec4 accumulation = texture2D(s_transparency_accumulation, v_texcoord0);
    float opacity = 1.0 - texture2D(s_transparency_revealage, v_texcoord0).a;
    vec3 color = accumulation.rgb / max(accumulation.a, 0.0001);
    gl_FragColor = vec4(color * opacity, opacity);
}
