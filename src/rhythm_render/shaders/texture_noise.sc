$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Adapted from TiXL PerlinNoise2d.hlsl; imported algorithm retains upstream names.
// Copyright 2010-2016 Thomas Mann, Daniel Szymanski, Andreas Rose, Framefield GmbH.
// MIT: third_party/notices/tixl-effects/LICENSE.txt; provenance/tixl_effects.json.
uniform vec4 u_noise_settings;
uniform vec4 u_noise_color_a;
uniform vec4 u_noise_color_b;
uniform vec4 u_noise_domain;
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
    vec3 p = vec3(uv * u_noise_settings.x + u_noise_settings.w,
                  mod(u_noise_settings.y, 256.0));
    float sum = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    float norm = 0.0;
    for (int octave = 0; octave < 4; ++octave)
    {
        sum += amplitude * perlinTileable(p * frequency, vec3(256.0, 256.0, 256.0));
        norm += amplitude;
        p += 0.77 * float(octave);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    float amount = clamp(sum / norm * 0.5 * u_noise_settings.z + 0.5, 0.0, 1.0);
    vec4 first = vec4(u_noise_color_a.rgb * u_noise_color_a.a, u_noise_color_a.a);
    vec4 second = vec4(u_noise_color_b.rgb * u_noise_color_b.a, u_noise_color_b.a);
    gl_FragColor = mix(first, second, amount) * v_color0;
}
