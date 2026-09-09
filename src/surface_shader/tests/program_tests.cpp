#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rhythm/image_shader/program.h"
#include "rhythm/surface_shader/program.h"

namespace {
std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size < 24 || size > 512 * 1024) throw std::runtime_error("surface.fixture");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) throw std::runtime_error("surface.read");
    return bytes;
}
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("surface.invalid_accepted");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm::surface_shader;
    try {
        if (argc != 2) throw std::runtime_error("surface.fixture_directory_required");
        const std::filesystem::path directory(argv[1]);
        Program program;
        program.expression_ = "clamp(vec3(a, b, c) * d, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0))";
        program.compiler_sha256_ = std::string(64, 'a');
        program.artifacts_ = {Read(directory / "windows/parameter_surface.bin"),
                              Read(directory / "android/parameter_surface.bin")};
        const auto bytes = Encode(program);
        const auto decoded = Decode(bytes);
        if (Encode(decoded) != bytes || decoded.expression_ != program.expression_ ||
            decoded.artifacts_ != program.artifacts_)
            throw std::runtime_error("surface.roundtrip");
        Reject([&] { rhythm::image_shader::Decode(bytes); });
        for (const auto size : {0u, 7u, 19u, 83u, 90u})
            Reject([&] { Decode(std::span(bytes).first(size)); });
        auto bad = bytes;
        bad.push_back(0);
        Reject([&] { Decode(bad); });
        for (const auto offset : {3u, 4u, 8u, 12u, 16u, 20u}) {
            bad = bytes;
            bad[offset] = 255;
            Reject([&] { Decode(bad); });
        }
        auto invalid = program;
        invalid.expression_ = "Sample(uv)";
        Reject([&] { Encode(invalid); });
        const auto source = FragmentSource(program.expression_);
        if (source.find(program.expression_) == source.npos ||
            source.find("GodotDirectional") == source.npos ||
            source.find("__RHYTHM_SURFACE_EXPRESSION__") != source.npos)
            throw std::runtime_error("surface.wrapper");
        Reject([&] { FragmentSource("#define arbitrary 1"); });
        Reject([&] { FragmentSource("vec3(resolution, time)"); });
        std::ofstream file(directory / "surface-generated.sc", std::ios::binary);
        file << source;
        if (!file) throw std::runtime_error("surface.wrapper_write");
        std::cout << "Surface bundle: profile isolation, roundtrip, invalid input and canonical "
                     "wrapper passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
