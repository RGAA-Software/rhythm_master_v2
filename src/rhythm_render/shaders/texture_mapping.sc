$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Angular replication adapted from Material Maker kaleidoscope2.mmg (MIT).
// Copyright (c) 2018-present Rodolphe Suescun and contributors.
// Polar mapping adapted from TiXL PolarCoordinates.hlsl (MIT).
// Copyright 2010-2016 Thomas Mann, Daniel Szymanski, Andreas Rose, Framefield GmbH.
// Exact sources/licenses/adaptations: provenance/*_effects.json.
SAMPLER2D(s_tex, 0);
uniform vec4 u_mapping_settings; // scale, rotation degrees, travel, twist
uniform vec4 u_mapping_domain; // aspect, sectors, radial power, polar mode

void main()
{
    vec2 p = (v_texcoord0 - 0.5) * vec2(u_mapping_domain.x, 1.0);
    float radius = max(length(p), 0.002);
    float angle = atan2(p.y, p.x + 0.0000001) + radians(u_mapping_settings.y);
    vec2 uv;
    if (u_mapping_domain.w > 0.5)
    {
        float radial = pow(radius * 2.0, u_mapping_domain.z) / u_mapping_settings.x;
        uv = vec2(angle / 6.28318530718 + 0.5 + radial * u_mapping_settings.w,
                  radial + u_mapping_settings.z);
    }
    else
    {
        float sector = 6.28318530718 / u_mapping_domain.y;
        angle += radius * u_mapping_settings.w;
        // Fold both sides of each sector to share the exact seam sample.
        float folded = abs(mod(angle + sector * 0.5, sector) - sector * 0.5);
        uv = vec2(cos(folded), sin(folded)) * radius / u_mapping_settings.x;
        uv.x /= u_mapping_domain.x;
        uv += vec2(0.5, 0.5 + u_mapping_settings.z);
    }
    uv = 1.0 - abs(mod(uv, 2.0) - 1.0);
    gl_FragColor = texture2D(s_tex, uv) * v_color0;
}
