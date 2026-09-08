#include <algorithm>
#include <set>
#include <stdexcept>
#include <string_view>

#include "rhythm/image_shader/program.h"

namespace rhythm::image_shader {
namespace {
class Reader final {
   public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    std::span<const std::uint8_t> Take(std::size_t size) {
        if (size > bytes_.size()) throw std::invalid_argument("shader.artifact_truncated");
        const auto result = bytes_.first(size);
        bytes_ = bytes_.subspan(size);
        return result;
    }
    std::uint32_t Word(std::size_t size) {
        std::uint32_t value = 0;
        const auto bytes = Take(size);
        for (std::size_t i = 0; i < size; ++i) value |= std::uint32_t(bytes[i]) << (8 * i);
        return value;
    }
    std::size_t Remaining() const { return bytes_.size(); }

   private:
    std::span<const std::uint8_t> bytes_{};
};
void Require(bool value) {
    if (!value) throw std::invalid_argument("shader.artifact_profile");
}
}  // namespace
void ValidateArtifact(std::span<const std::uint8_t> bytes, Target target) {
    Require(bytes.size() >= 27 && bytes.size() <= kMaximumArtifactBytes &&
            (target == Target::kWindowsSm5 || target == Target::kGles300));
    Reader reader(bytes);
    // Fixed current shader container revision, fragment stage, standard quad
    // varyings and no raw storage/image bindings. Derived from the retained
    // backend's shader reader and checked against both actual compiler outputs.
    Require(reader.Word(4) == 0x0c485346 && reader.Word(4) == 0xe1f28301 && reader.Word(4) == 0);
    Require(reader.Word(4) == 0 && reader.Word(4) == 0);
    const auto count = reader.Word(2);
    Require(count <= 3);
    std::set<std::string> names;
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto name_bytes = reader.Take(reader.Word(1));
        const std::string name(name_bytes.begin(), name_bytes.end());
        Require(names.insert(name).second);
        const auto type = reader.Word(1), number = reader.Word(1);
        const auto register_index = reader.Word(2), register_count = reader.Word(2);
        const auto component = reader.Word(1), dimension = reader.Word(1), format = reader.Word(2);
        Require(register_count == 1 && component == 0 && format == 0);
        if (name == "s_tex") {
            Require(type == (target == Target::kWindowsSm5 ? 48u : 0u) && number == 1 &&
                    register_index == 0 && dimension == 2);
        } else {
            Require(name == "u_image_params" || name == "u_image_info");
            Require(type == (target == Target::kWindowsSm5 ? 18u : 2u) &&
                    number == (target == Target::kWindowsSm5 ? 0u : 1u) && dimension == 0 &&
                    (register_index == 0 ||
                     (target == Target::kWindowsSm5 && register_index == 16)));
        }
    }
    const auto code = reader.Take(reader.Word(4));
    Require(!code.empty() && reader.Word(1) == 0);
    if (target == Target::kGles300) {
        Require(reader.Remaining() == 0 && std::all_of(code.begin(), code.end(), [](auto byte) {
                    return byte == '\n' || byte == '\r' || byte == '\t' ||
                           (byte >= 32 && byte < 127);
                }));
    } else {
        Require(reader.Word(1) == 0 && reader.Word(2) <= 32 && reader.Remaining() == 0);
        Reader container(code);
        Require(container.Word(4) == 0x43425844);  // Shader model 5 container.
        container.Take(16);
        Require(container.Word(4) == 1 && container.Word(4) == code.size());
        const auto chunks = container.Word(4);
        Require(chunks > 0 && chunks <= 16);
        for (std::uint32_t index = 0; index < chunks; ++index) {
            const auto offset = container.Word(4);
            Require(offset >= 32 + chunks * 4 && offset <= code.size());
            Reader chunk(code.subspan(offset));
            chunk.Take(4);
            chunk.Take(chunk.Word(4));
        }
    }
}
}  // namespace rhythm::image_shader
