#include "rhythm/image_shader/source.h"

#include <stdexcept>

namespace rhythm::image_shader {
std::optional<Diagnostic> ValidateExpression(std::string_view expression) {
    return shader_expression::Validate(expression, shader_expression::Profile::kImageRgba);
}
std::string FragmentSource(std::string_view expression) {
    if (const auto diagnostic = ValidateExpression(expression))
        throw std::invalid_argument(diagnostic->code_);
    std::string result = R"($input v_color0, v_texcoord0
#include <bgfx_shader.sh>
#if BGFX_SHADER_LANGUAGE_GLSL
precision highp float;
#endif
SAMPLER2D(s_tex, 0);
uniform vec4 u_image_params;
uniform vec4 u_image_info;
vec4 Sample(vec2 uv) {
    vec4 pixel = texture2D(s_tex, uv);
    return vec4(pixel.rgb / max(pixel.a, 0.000001), pixel.a);
}
vec4 Shade(vec2 uv, float time, vec2 resolution, float a, float b, float c, float d) {
    float pi = 3.141592653589793;
    return (
#line 1
)";
    result += expression;
    result += R"(
    );
}
void main() {
    vec4 pixel = Shade(v_texcoord0, u_image_info.x, u_image_info.yz,
                       u_image_params.x, u_image_params.y, u_image_params.z, u_image_params.w);
    if (pixel.x != pixel.x) pixel.x = 0.0;
    if (pixel.y != pixel.y) pixel.y = 0.0;
    if (pixel.z != pixel.z) pixel.z = 0.0;
    if (pixel.w != pixel.w) pixel.w = 0.0;
    pixel = clamp(pixel, vec4(-65504.0, -65504.0, -65504.0, 0.0), vec4(65504.0, 65504.0, 65504.0, 1.0));
    gl_FragColor = vec4(pixel.rgb * pixel.a, pixel.a) * v_color0;
}
)";
    return result;
}
}  // namespace rhythm::image_shader
