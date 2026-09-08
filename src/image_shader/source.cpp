#include "rhythm/image_shader/source.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

// Imported lexer implementation is compiled from the installed vcpkg SYSTEM
// header. Its borrowed character pointers exist only in this synchronous adapter.
#define STB_C_LEXER_IMPLEMENTATION
#include <stb_c_lexer.h>

namespace rhythm::image_shader {
namespace {
bool AllowedIdentifier(std::string_view name) {
    constexpr std::array<std::string_view, 44> kNames{
            "uv",          "time",   "resolution", "a",    "b",       "c",         "d",
            "pi",          "Sample", "vec2",       "vec3", "vec4",    "float",     "mix",
            "clamp",       "min",    "max",        "sin",  "cos",     "tan",       "abs",
            "floor",       "ceil",   "fract",      "pow",  "exp",     "log",       "sqrt",
            "inversesqrt", "length", "distance",   "dot",  "cross",   "normalize", "step",
            "smoothstep",  "atan",   "mod",        "sign", "radians", "degrees",   "true",
            "false",       "reflect"};
    return std::find(kNames.begin(), kNames.end(), name) != kNames.end() ||
           (!name.empty() && name.size() <= 4 &&
            name.find_first_not_of("xyzwrgbastpq") == name.npos);
}
}  // namespace
std::optional<Diagnostic> ValidateExpression(std::string_view expression) {
    if (expression.empty() || expression.size() > kMaximumSourceBytes)
        return Diagnostic{"shader.source_limit"};
    // Reject directives and comments before lexing: the default upstream lexer
    // discards them, but they must never affect the generated shader wrapper.
    if (expression.find('#') != expression.npos || expression.find("/*") != expression.npos ||
        expression.find("//") != expression.npos ||
        std::any_of(expression.begin(), expression.end(), [](unsigned char value) {
            return value >= 127 || (value < 32 && value != '\n' && value != '\r' && value != '\t');
        }))
        return Diagnostic{"shader.source_characters"};
    const std::string source(expression);
    std::array<char, 256> scratch{};
    stb_lexer lexer{};
    stb_c_lexer_init(&lexer, source.data(), source.data() + source.size(), scratch.data(),
                     int(scratch.size()));
    std::uint32_t tokens = 0, samples = 0;
    int depth = 0;
    const auto fail = [&](std::string code) {
        stb_lex_location location{};
        stb_c_lexer_get_location(&lexer, lexer.where_firstchar, &location);
        return std::optional<Diagnostic>{{std::move(code), std::uint32_t(location.line_number),
                                          std::uint32_t(location.line_offset + 1)}};
    };
    while (stb_c_lexer_get_token(&lexer)) {
        if (++tokens > 1024) return fail("shader.token_limit");
        switch (lexer.token) {
            case CLEX_id: {
                // The older installed desktop stb has a known string_len bug;
                // identifiers are still terminated in the bounded scratch array.
                const std::string_view identifier(lexer.string);
                if (!AllowedIdentifier(identifier)) return fail("shader.identifier");
                if (identifier == "Sample" && ++samples > 16) return fail("shader.sample_limit");
                break;
            }
            case CLEX_intlit:
                if (std::abs(double(lexer.int_number)) > 1000000)
                    return fail("shader.literal_limit");
                break;
            case CLEX_floatlit:
                if (!std::isfinite(lexer.real_number) || std::abs(lexer.real_number) > 1000000)
                    return fail("shader.literal_limit");
                break;
            case '(':
                if (++depth > 32) return fail("shader.nesting_limit");
                break;
            case ')':
                if (--depth < 0) return fail("shader.parentheses");
                break;
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '.':
            case ',':
            case '<':
            case '>':
            case '!':
            case '?':
            case ':':
            case CLEX_eq:
            case CLEX_noteq:
            case CLEX_lesseq:
            case CLEX_greatereq:
            case CLEX_andand:
            case CLEX_oror:
                break;
            default:
                return fail("shader.token");
        }
    }
    if (depth != 0) return Diagnostic{"shader.parentheses"};
    return {};
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
