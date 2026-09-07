$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Focused adaptation of TiXL Displace.hlsl (MIT).
// Copyright 2010-2016 Thomas Mann, Daniel Szymanski, Andreas Rose, Framefield GmbH.
// Source revision, exact license and modifications: provenance/tixl_effects.json.
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_displace_map, 1);
uniform vec4 u_displace_settings; // strength, rotation degrees, RG mode, unused
uniform vec4 u_displace_domain; // map sample radius UV, target aspect, unused

float MapHeight(vec2 uv)
{
    // Premultiplied height makes transparent maps neutral without division by alpha.
    return dot(texture2D(s_displace_map, clamp(uv, 0.0, 1.0)).rgb, vec3_splat(0.3333333333));
}
void main()
{
    vec2 uv = v_texcoord0;
    vec2 displacement;
    if (u_displace_settings.z > 0.5)
    {
        vec4 value = texture2D(s_displace_map, uv);
        displacement = value.rg * 2.0 - vec2_splat(value.a);
    }
    else
    {
        vec2 dx = vec2(u_displace_domain.x, 0.0);
        vec2 dy = vec2(0.0, u_displace_domain.y);
        displacement = vec2(MapHeight(uv + dx) - MapHeight(uv - dx),
                            MapHeight(uv + dy) - MapHeight(uv - dy));
    }
    float angle = radians(u_displace_settings.y);
    displacement = vec2(cos(angle) * displacement.x - sin(angle) * displacement.y,
                        sin(angle) * displacement.x + cos(angle) * displacement.y);
    uv += displacement * u_displace_settings.x / vec2(u_displace_domain.z, 1.0);
    uv = 1.0 - abs(mod(uv, 2.0) - 1.0);
    gl_FragColor = texture2D(s_tex, uv) * v_color0;
}
