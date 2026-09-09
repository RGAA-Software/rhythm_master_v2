#include "rhythm/shader_expression/source.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

// Imported lexer implementation is compiled from the installed vcpkg SYSTEM
// header. Its borrowed character pointers exist only in this synchronous adapter.
#define STB_C_LEXER_IMPLEMENTATION
#include <stb_c_lexer.h>

namespace rhythm::shader_expression {
namespace {
bool AllowedIdentifier(std::string_view name, Profile profile) {
    if (name == "position" || name == "normal") return profile == Profile::kSurfaceRgb;
    if (name == "Sample" || name == "resolution") return profile == Profile::kImageRgba;
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
std::optional<Diagnostic> Validate(std::string_view expression, Profile profile) {
    if (profile != Profile::kImageRgba && profile != Profile::kSurfaceRgb)
        return Diagnostic{"shader.expression_profile"};
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
                if (!AllowedIdentifier(identifier, profile)) return fail("shader.identifier");
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
}  // namespace rhythm::shader_expression
