$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Adapted from TiXL PerlinNoise2d.hlsl; imported algorithm retains upstream names.
// Copyright 2010-2016 Thomas Mann, Daniel Szymanski, Andreas Rose, Framefield GmbH.
// MIT: third_party/notices/tixl-effects/LICENSE.txt; provenance/tixl_effects.json.
uniform vec4 u_noise_settings;
uniform vec4 u_noise_color_a;
uniform vec4 u_noise_color_b;
uniform vec4 u_noise_domain; // aspect, offset x/y, base lattice cells per pixel
uniform vec4 u_noise_spectral; // octaves, roughness gain, warp strength, band filter
vec3 hash33(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.xxy + p.yzz) * p.zyx);
}

float grad(vec3 cell, vec3 pos)
{
    vec3 g = hash33(cell) * 2.0 - 1.0;
    g /= max(length(g), 0.000001);
    return dot(g, pos);
}

vec3 fade(vec3 t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}
float perlinTileable(vec3 p, vec3 period)
{
    vec3 i = floor(p);
    vec3 f = fract(p);

    vec3 w = fade(f);

    // Wrap EVERY corner coordinate (not just base i / i+1)
    vec3 c000 = mod(i + vec3(0, 0, 0), period);
    vec3 c100 = mod(i + vec3(1, 0, 0), period);
    vec3 c010 = mod(i + vec3(0, 1, 0), period);
    vec3 c110 = mod(i + vec3(1, 1, 0), period);

    vec3 c001 = mod(i + vec3(0, 0, 1), period);
    vec3 c101 = mod(i + vec3(1, 0, 1), period);
    vec3 c011 = mod(i + vec3(0, 1, 1), period);
    vec3 c111 = mod(i + vec3(1, 1, 1), period);

    float n000 = grad(c000, f - vec3(0, 0, 0));
    float n100 = grad(c100, f - vec3(1, 0, 0));
    float n010 = grad(c010, f - vec3(0, 1, 0));
    float n110 = grad(c110, f - vec3(1, 1, 0));

    float n001 = grad(c001, f - vec3(0, 0, 1));
    float n101 = grad(c101, f - vec3(1, 0, 1));
    float n011 = grad(c011, f - vec3(0, 1, 1));
    float n111 = grad(c111, f - vec3(1, 1, 1));

    float nx00 = mix(n000, n100, w.x);
    float nx10 = mix(n010, n110, w.x);
    float nx01 = mix(n001, n101, w.x);
    float nx11 = mix(n011, n111, w.x);

    float nxy0 = mix(nx00, nx10, w.y);
    float nxy1 = mix(nx01, nx11, w.y);

    return mix(nxy0, nxy1, w.z);
}


void main()
{
    vec2 uv = (v_texcoord0 - 0.5) * vec2(u_noise_domain.x, 1.0);
    vec3 p = vec3(uv * u_noise_settings.x + u_noise_settings.w + u_noise_domain.yz,
                  mod(u_noise_settings.y, 256.0));
    // Domain warp: two decorrelated single-octave lattice samples displace the
    // base domain, breaking the grid alignment that magnification exposes.
    if (u_noise_spectral.z > 0.0)
    {
        vec3 warp_p = p + vec3(31.7, 17.3, 11.9);
        p.xy += vec2(perlinTileable(warp_p, vec3(256.0, 256.0, 256.0)),
                     perlinTileable(warp_p + vec3(47.3, 83.1, 23.7),
                                    vec3(256.0, 256.0, 256.0))) *
                u_noise_spectral.z;
    }
    float sum = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    float norm = 0.0;
    for (int octave = 0; octave < 8; ++octave)
    {
        if (float(octave) >= u_noise_spectral.x) break;
        // Band limiting fades octaves whose lattice cells fall below two pixels;
        // u_noise_domain.w is the base-frequency footprint in cells per pixel.
        float weight = 1.0;
        if (u_noise_spectral.w > 0.0)
            weight = 1.0 - smoothstep(0.5, 1.0, u_noise_domain.w * frequency);
        sum += amplitude * weight * perlinTileable(p * frequency, vec3(256.0, 256.0, 256.0));
        norm += amplitude * weight;
        p += 0.77 * float(octave);
        frequency *= 2.0;
        amplitude *= u_noise_spectral.y;
    }
    float amount = clamp(sum / max(norm, 1e-6) * 0.5 * u_noise_settings.z + 0.5, 0.0, 1.0);
    vec4 first = vec4(u_noise_color_a.rgb * u_noise_color_a.a, u_noise_color_a.a);
    vec4 second = vec4(u_noise_color_b.rgb * u_noise_color_b.a, u_noise_color_b.a);
    gl_FragColor = mix(first, second, amount) * v_color0;
}
