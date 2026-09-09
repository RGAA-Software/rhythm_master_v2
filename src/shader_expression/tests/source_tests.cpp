#include <iostream>
#include <stdexcept>

#include "rhythm/shader_expression/source.h"

int main() {
    using namespace rhythm::shader_expression;
    try {
        for (const auto profile : {Profile::kImageRgba, Profile::kSurfaceRgb}) {
            if (Validate("vec3(uv.x, a * sin(time), pi)", profile))
                throw std::runtime_error("expression.common_inputs");
            for (const auto expression : {"", "#define a 1", "/* hidden */a", "a=1", "a; return b",
                                          "a[1]", "++a", "unknown(uv)", "1e100", "a)", "(a"})
                if (!Validate(expression, profile))
                    throw std::runtime_error("expression.admission");
            if (!Validate(std::string(8193, 'a'), profile) ||
                !Validate(std::string(33, '(') + "a" + std::string(33, ')'), profile))
                throw std::runtime_error("expression.budgets");
            const auto error = Validate("vec3(\nunknown(uv))", profile);
            if (!error || error->line_ != 2 || error->column_ != 1)
                throw std::runtime_error("expression.diagnostic");
        }
        for (const auto expression : {"vec3(position.x, normal.z, time)", "normalize(normal)"}) {
            if (Validate(expression, Profile::kSurfaceRgb) ||
                !Validate(expression, Profile::kImageRgba))
                throw std::runtime_error("expression.surface_inputs");
        }
        for (const auto expression : {"Sample(uv)", "vec3(resolution, a)"}) {
            if (Validate(expression, Profile::kImageRgba) ||
                !Validate(expression, Profile::kSurfaceRgb))
                throw std::runtime_error("expression.image_inputs");
        }
        if (!Validate("a", static_cast<Profile>(255)))
            throw std::runtime_error("expression.unknown_profile");
        std::cout << "Shared expression admission: isolated profiles, budgets and diagnostics "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
