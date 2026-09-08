#include "rhythm/image_shader/program.h"

#include <algorithm>
#include <stdexcept>

#include "rhythm/image_shader/source.h"

namespace rhythm::image_shader {
void Validate(const Program& program) {
    if (ValidateExpression(program.expression_) || program.compiler_sha256_.size() != 64 ||
        program.compiler_sha256_.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::invalid_argument("shader.program_profile");
    ValidateArtifact(program.artifacts_[0], Target::kWindowsSm5);
    ValidateArtifact(program.artifacts_[1], Target::kGles300);
}
std::vector<std::uint8_t> Encode(const Program& program) {
    Validate(program);
    std::vector<std::uint8_t> bytes{'R', 'M', 'S', 'H', '0', '0', '1', '\n'};
    const auto word = [&](std::size_t value) {
        for (std::size_t index = 0; index < 4; ++index)
            bytes.push_back(std::uint8_t(value >> (index * 8)));
    };
    word(program.expression_.size());
    word(program.artifacts_[0].size());
    word(program.artifacts_[1].size());
    bytes.insert(bytes.end(), program.compiler_sha256_.begin(), program.compiler_sha256_.end());
    bytes.insert(bytes.end(), program.expression_.begin(), program.expression_.end());
    for (const auto& artifact : program.artifacts_)
        bytes.insert(bytes.end(), artifact.begin(), artifact.end());
    return bytes;
}
Program Decode(std::span<const std::uint8_t> bytes) {
    constexpr std::array<std::uint8_t, 8> kMagic{'R', 'M', 'S', 'H', '0', '0', '1', '\n'};
    if (bytes.size() < 84 || bytes.size() > 84 + kMaximumSourceBytes + 2 * kMaximumArtifactBytes ||
        !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
        throw std::invalid_argument("shader.bundle_header");
    const auto word = [&](std::size_t offset) {
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < 4; ++index)
            value |= std::uint32_t(bytes[offset + index]) << (index * 8);
        return std::size_t(value);
    };
    const auto source_size = word(8), windows_size = word(12), gles_size = word(16);
    if (source_size > kMaximumSourceBytes || windows_size > kMaximumArtifactBytes ||
        gles_size > kMaximumArtifactBytes ||
        84 + source_size + windows_size + gles_size != bytes.size())
        throw std::invalid_argument("shader.bundle_sizes");
    Program program;
    program.compiler_sha256_.assign(bytes.begin() + 20, bytes.begin() + 84);
    bytes = bytes.subspan(84);
    program.expression_.assign(bytes.begin(), bytes.begin() + source_size);
    bytes = bytes.subspan(source_size);
    program.artifacts_[0].assign(bytes.begin(), bytes.begin() + windows_size);
    bytes = bytes.subspan(windows_size);
    program.artifacts_[1].assign(bytes.begin(), bytes.end());
    Validate(program);
    return program;
}
}  // namespace rhythm::image_shader
