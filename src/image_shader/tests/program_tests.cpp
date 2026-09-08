#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rhythm/image_shader/program.h"

namespace {
std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    if (size > rhythm::image_shader::kMaximumArtifactBytes)
        throw std::length_error("fixture limit");
    std::vector<std::uint8_t> bytes(std::size_t(size), 0);
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!input) throw std::runtime_error("fixture read");
    return bytes;
}
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid shader artifact accepted");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm::image_shader;
    try {
        if (argc != 3)
            throw std::invalid_argument("program tests require desktop and GLES fixtures");
        Program program;
        program.expression_ =
                "mix(Sample(uv), vec4(uv.x, uv.y, sin(time) * 0.5 + 0.5, 1.0), clamp(a, 0.0, 1.0))";
        program.compiler_sha256_ = std::string(64, 'a');
        program.artifacts_ = {Read(argv[1]), Read(argv[2])};
        const auto encoded = Encode(program);
        const auto decoded = Decode(encoded);
        if (decoded.expression_ != program.expression_ ||
            decoded.artifacts_ != program.artifacts_ || Encode(decoded) != encoded)
            throw std::runtime_error("shader program round trip");
        for (const auto length : {0u, 7u, 19u, 83u, 90u})
            Reject([&] { Decode(std::span(encoded).first(length)); });
        auto bad_bundle = encoded;
        bad_bundle[8] = bad_bundle[9] = bad_bundle[10] = bad_bundle[11] = 255;
        Reject([&] { Decode(bad_bundle); });
        bad_bundle = encoded;
        bad_bundle.push_back(0);
        Reject([&] { Decode(bad_bundle); });
        for (std::size_t target = 0; target < 2; ++target) {
            const auto& artifact = program.artifacts_[target];
            for (const auto length : {0u, 12u, 21u, 30u})
                Reject([&] {
                    ValidateArtifact(std::span(artifact).first(length), Target(target));
                });
            for (const auto offset : {0u, 3u, 4u, 12u, 16u, 20u, 23u}) {
                auto bad = artifact;
                bad[offset] = 255;
                Reject([&] { ValidateArtifact(bad, Target(target)); });
            }
            Reject([&] { ValidateArtifact(artifact, Target(1 - target)); });
        }
        program.expression_ = "while(true) {}";
        Reject([&] { Encode(program); });
        std::cout << "Image shader program: both compiler products, fixed bindings, truncation, "
                     "profile and bundle checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
