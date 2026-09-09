#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "rhythm/shader_artifact/artifact.h"

namespace {
std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size < 24 || size > std::streamoff(rhythm::shader_artifact::kMaximumArtifactBytes))
        throw std::runtime_error("artifact.fixture");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) throw std::runtime_error("artifact.read");
    return bytes;
}
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("artifact.invalid_accepted");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm::shader_artifact;
    try {
        if (argc != 2) throw std::runtime_error("artifact.fixture_directory_required");
        for (const auto target : {Target::kWindowsSm5, Target::kGles300}) {
            for (const auto name : {"neutral", "animated_surface", "parameter_surface"}) {
                const auto bytes = Read(std::filesystem::path(argv[1]) /
                                        (target == Target::kWindowsSm5 ? "windows" : "android") /
                                        (std::string(name) + ".bin"));
                Validate(bytes, target, Profile::kSurfaceRgb);
                Reject([&] { Validate(bytes, target, Profile::kImageRgba); });
                Reject([&] { Validate(bytes, target, static_cast<Profile>(255)); });
                Reject([&] { Validate(bytes, static_cast<Target>(255), Profile::kSurfaceRgb); });
                for (const auto length : {0u, 7u, 21u, 30u})
                    Reject([&] {
                        Validate(std::span(bytes).first(length), target, Profile::kSurfaceRgb);
                    });
                for (const auto offset : {0u, 3u, 4u, 12u, 16u, 20u, 23u}) {
                    auto bad = bytes;
                    bad[offset] = 255;
                    Reject([&] { Validate(bad, target, Profile::kSurfaceRgb); });
                }
                auto bad = bytes;
                bad.push_back(0);
                Reject([&] { Validate(bad, target, Profile::kSurfaceRgb); });
                if (target == Target::kWindowsSm5) {
                    bad = bytes;
                    bad[bad.size() - 1] = bad[bad.size() - 2] = 0;
                    Reject([&] { Validate(bad, target, Profile::kSurfaceRgb); });
                }
                // First uniform type, array count, register and register count.
                const auto fields = std::size_t(23 + bytes[22]);
                for (const auto relative : {0u, 1u, 3u, 5u, 6u, 7u, 9u}) {
                    bad = bytes;
                    bad[fields + relative] = 255;
                    Reject([&] { Validate(bad, target, Profile::kSurfaceRgb); });
                }
            }
        }
        std::cout << "Surface artifacts: both targets, optimized variants, profile isolation and "
                     "malformed bindings rejected\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
