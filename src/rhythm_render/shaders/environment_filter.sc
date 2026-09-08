$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// TiXL MIT Hammersley/GGX prefilter adapted to a bounded equirectangular atlas.
// GLM MIT color transfer. See provenance/environment_lighting.json and notices.
SAMPLER2D(s_tex, 0);
uniform vec4 u_environment_filter;
float LinearEnvironment(float c) {
    c = clamp(c, 0.0, 1.0);
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
vec3 ReadEnvironment(vec3 direction) {
    vec2 uv = vec2(atan2(direction.z, direction.x) / 6.28318530718 + 0.5,
                   acos(clamp(direction.y, -1.0, 1.0)) / 3.14159265359);
    uv.y = clamp(uv.y, u_environment_filter.y, 1.0 - u_environment_filter.y);
    vec4 pixel = texture2DLod(s_tex, uv, 0.0);
    vec3 color = max(pixel.rgb / max(pixel.a, 0.000001), vec3(0.0, 0.0, 0.0));
    if (u_environment_filter.x > 0.5)
        color = vec3(LinearEnvironment(color.r), LinearEnvironment(color.g), LinearEnvironment(color.b));
    return min(color, vec3(65504.0, 65504.0, 65504.0));
}
float RadicalInverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
void main() {
    // Six tiles: five GGX roughness levels and cosine-weighted diffuse.
    // One pixel gutter on each edge prevents cross-tile interpolation.
    vec2 pixel = v_texcoord0 * vec2(780.0, 66.0);
    float tile = min(floor(pixel.x / 130.0), 5.0);
    vec2 uv = (pixel - vec2(tile * 130.0 + 1.0, 1.0)) / vec2(128.0, 64.0);
    float phi = (uv.x - 0.5) * 6.28318530718;
    float theta = clamp(uv.y, 0.0, 1.0) * 3.14159265359;
    vec3 normal = vec3(cos(phi) * sin(theta), cos(theta), sin(phi) * sin(theta));
    vec3 result = vec3(0.0, 0.0, 0.0);
    if (tile < 0.5) {
        result = ReadEnvironment(normal);
    } else {
        vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(up, normal));
        vec3 bitangent = cross(normal, tangent);
        float roughness = tile * 0.25;
        float a = roughness * roughness;
        float weight = 0.0;
        for (int j = 0; j < 64; ++j) {
            vec2 xi = vec2(float(j) / 64.0, RadicalInverse(uint(j)));
            float angle = 6.28318530718 * xi.x;
            float cosine = tile > 4.5 ? sqrt(1.0 - xi.y) :
                sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
            float sine = sqrt(max(0.0, 1.0 - cosine * cosine));
            vec3 half_vector = tangent * (sine * cos(angle)) + bitangent * (sine * sin(angle)) + normal * cosine;
            vec3 light = tile > 4.5 ? half_vector : 2.0 * dot(normal, half_vector) * half_vector - normal;
            float n_dot_l = tile > 4.5 ? 1.0 : max(dot(normal, light), 0.0);
            // Divide before accumulating to remain finite in half-range inputs.
            result += ReadEnvironment(light) * (n_dot_l / 64.0);
            weight += n_dot_l / 64.0;
        }
        result /= max(weight, 0.000001);
    }
    gl_FragColor = vec4(min(result, vec3(65504.0, 65504.0, 65504.0)), 1.0);
}
