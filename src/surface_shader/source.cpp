#include <stdexcept>

#include "rhythm/surface_shader/program.h"
#include "surface_template.h"

namespace rhythm::surface_shader {
std::string FragmentSource(std::string_view expression) {
    if (const auto error =
                shader_expression::Validate(expression, shader_expression::Profile::kSurfaceRgb))
        throw std::invalid_argument(error->code_);
    constexpr std::string_view kSlot = "__RHYTHM_SURFACE_EXPRESSION__";
    std::string result(detail::kTemplate);
    const auto position = result.find(kSlot);
    if (position == result.npos || result.find(kSlot, position + kSlot.size()) != result.npos)
        throw std::logic_error("surface.template");
    result.replace(position, kSlot.size(), expression);
    return result;
}
}  // namespace rhythm::surface_shader
