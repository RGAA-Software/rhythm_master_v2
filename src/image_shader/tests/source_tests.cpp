#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rhythm/image_shader/source.h"

int main(int argc, char* argv[]) {
    using namespace rhythm::image_shader;
    try {
        for (const auto expression :
             {"vec4(uv.x, uv.y, sin(time) * 0.5 + 0.5, 1.0)",
              "mix(Sample(uv), vec4(a, b, c, d), 0.5)", "vec4(normalize(vec3(uv, 1.0)), 1.0)"})
            if (ValidateExpression(expression))
                throw std::runtime_error("valid image expression rejected");
        for (const auto expression : {"", "#define x 1", "vec4(1); while(true) {}", "Sample(uv[0])",
                                      "unknown(uv)", "a = 1.0", "++a", "1e100", "/* hidden */ a",
                                      "// hidden\na", "\"string\"", "(a", "a)", "a; return b"})
            if (!ValidateExpression(expression))
                throw std::runtime_error("unsupported image syntax accepted");
        if (!ValidateExpression(std::string(8193, 'a')) ||
            !ValidateExpression(std::string(33, '(') + "a" + std::string(33, ')')))
            throw std::runtime_error("source/nesting limits missing");
        std::string samples = "Sample(uv)";
        for (int index = 1; index < 17; ++index) samples += "+Sample(uv)";
        if (!ValidateExpression(samples)) throw std::runtime_error("texture sample limit missing");
        const auto diagnostic = ValidateExpression("vec4(\nunknown(uv))");
        if (!diagnostic || diagnostic->line_ != 2 || diagnostic->column_ != 1)
            throw std::runtime_error("shader diagnostic location");
        if (argc == 2) {
            std::ofstream output(std::filesystem::path(argv[1]), std::ios::binary);
            output << FragmentSource(
                    "mix(Sample(uv), vec4(uv.x, uv.y, sin(time) * 0.5 + 0.5, 1.0), clamp(a, 0.0, "
                    "1.0))");
            if (!output) throw std::runtime_error("shader fixture write");
        }
        std::cout << "Image shader source: bounded lexer, identifiers, samples, diagnostics and "
                     "wrapper passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
