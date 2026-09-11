$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// GLM 0.9.9.8 transfer equations and Godot tone mapping (MIT).
// See provenance/color_pipeline.json and retained third_party/notices.
SAMPLER2D(s_tex, 0);
uniform vec4 u_color_pipeline;
uniform vec4 u_tonemap_parameters;
float ToLinear(float c) {
    c = clamp(c, 0.0, 1.0);
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
float ToSrgb(float c) {
    c = clamp(c, 0.0, 1.0);
    return c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}
vec3 ToneReinhard(vec3 color) {
    float white_squared = u_tonemap_parameters.x;
    vec3 white_color = white_squared * color;
    return (white_color + color * color) / (white_color + vec3(white_squared, white_squared, white_squared));
}
vec3 ToneFilmic(vec3 color) {
    const float A = 0.88;
    const float B = 0.60;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.01;
    const float F = 0.30;
    vec3 mapped = ((color * (A * color + C * B) + D * E) /
                   (color * (A * color + B) + D * F)) - E / F;
    return mapped / u_tonemap_parameters.x;
}
vec3 ToneAces(vec3 color) {
    const mat3 rgb_to_rrt = mat3(
        vec3(1.074942, 0.638244, 0.086814),
        vec3(0.136800, 1.635012, 0.028188),
        vec3(0.051120, 0.240894, 1.507986));
    const mat3 odt_to_rgb = mat3(
        vec3(1.60475, -0.53108, -0.07367),
        vec3(-0.10208, 1.10813, -0.00605),
        vec3(-0.00327, -0.07276, 1.07602));
    color = mul(rgb_to_rrt, color);
    vec3 mapped = (color * (color + 0.0245786) - 0.000090537) /
                  (color * (0.983729 * color + 0.432951) + 0.238081);
    return mul(odt_to_rgb, mapped) / u_tonemap_parameters.x;
}
vec3 AllenWp(vec3 value) {
    const float crossover = 0.18;
    const float shoulder = 0.82;
    vec3 shifted = value - crossover;
    vec3 scaled = u_tonemap_parameters.z * shifted;
    vec3 upper = scaled * (1.0 + shifted / u_tonemap_parameters.w) /
                 (1.0 + scaled / shoulder) + crossover;
    vec3 powered = pow(value, vec3(u_tonemap_parameters.x, u_tonemap_parameters.x,
                                   u_tonemap_parameters.x));
    vec3 lower = powered / (powered + u_tonemap_parameters.y);
    return vec3(value.r < crossover ? lower.r : upper.r,
                value.g < crossover ? lower.g : upper.g,
                value.b < crossover ? lower.b : upper.b);
}
vec3 ToneAgx(vec3 color) {
    const mat3 inset = mat3(
        vec3(0.5448147465, 0.1404169485, 0.0888104196),
        vec3(0.3737873984, 0.7541375546, 0.1788717564),
        vec3(0.0813978551, 0.1054454970, 0.7323178240));
    const mat3 outset = mat3(
        vec3(1.9648874117, -0.2993133649, -0.1643527425),
        vec3(-0.8559884957, 1.3263979646, -0.2381839694),
        vec3(-0.1088989160, -0.0270845997, 1.4025367120));
    color = mul(color, inset);
    color = min(vec3(1.0, 1.0, 1.0), AllenWp(color));
    return mul(color, outset);
}
void main() {
    vec4 pixel = texture2D(s_tex, v_texcoord0) * v_color0;
    // Additive targets can contain alpha > 1. Preserve radiance and cap coverage.
    float alpha = clamp(pixel.a, 0.0, 1.0);
    bool has_coverage = alpha > 0.000001;
    vec3 rgb = pixel.rgb / (has_coverage ? alpha : 1.0);
    if (u_color_pipeline.x > 0.5) rgb = vec3(ToLinear(rgb.r), ToLinear(rgb.g), ToLinear(rgb.b));
    rgb = max(rgb * u_color_pipeline.w, vec3(0.0, 0.0, 0.0));
    if (u_color_pipeline.z > 0.5 && u_color_pipeline.z < 1.5) rgb = ToneReinhard(rgb);
    else if (u_color_pipeline.z < 2.5 && u_color_pipeline.z > 1.5) rgb = ToneFilmic(rgb);
    else if (u_color_pipeline.z < 3.5 && u_color_pipeline.z > 2.5) rgb = ToneAces(rgb);
    else if (u_color_pipeline.z > 3.5) rgb = ToneAgx(rgb);
    if (u_color_pipeline.y > 0.5) rgb = vec3(ToSrgb(rgb.r), ToSrgb(rgb.g), ToSrgb(rgb.b));
    float coverage = has_coverage ? alpha : 1.0;
    gl_FragColor = vec4(min(rgb, vec3(65504.0, 65504.0, 65504.0)) * coverage, alpha);
}
