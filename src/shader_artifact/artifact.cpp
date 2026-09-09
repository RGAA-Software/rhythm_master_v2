#include "rhythm/shader_artifact/artifact.h"

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <string_view>

namespace rhythm::shader_artifact {
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
void SurfaceBinding(std::string_view name, std::uint32_t type, std::uint32_t number,
                    std::uint32_t register_index, std::uint32_t register_count,
                    std::uint32_t dimension, Target target, std::set<std::uint32_t>& occupied) {
    constexpr std::array<std::string_view, 6> kSamplers{"s_scene_base",   "s_scene_normal",
                                                        "s_scene_orm",    "s_scene_emission",
                                                        "s_scene_shadow", "s_scene_environment"};
    const auto sampler = std::find(kSamplers.begin(), kSamplers.end(), name);
    const bool windows = target == Target::kWindowsSm5;
    if (sampler != kSamplers.end()) {
        Require(type == (windows ? 48u : 0u) && number == 1 && register_count == 1 &&
                register_index == (windows ? std::uint32_t(sampler - kSamplers.begin()) : 0u) &&
                dimension == 2);
        return;
    }
    constexpr std::array<std::string_view, 5> kArrays{
            "u_scene_light_directions", "u_scene_light_colors", "u_scene_light_positions",
            "u_scene_spot_directions", "u_scene_light_ranges"};
    constexpr std::array<std::string_view, 12> kVectors{"u_scene_shadow_settings",
                                                        "u_scene_shadow_filter",
                                                        "u_scene_environment",
                                                        "u_scene_material",
                                                        "u_scene_emissive",
                                                        "u_scene_camera",
                                                        "u_scene_view",
                                                        "u_scene_textures",
                                                        "u_scene_texture_options",
                                                        "u_scene_uv",
                                                        "u_surface_params",
                                                        "u_surface_info"};
    const bool matrix = name == "u_scene_shadow_matrix";
    const bool array = std::find(kArrays.begin(), kArrays.end(), name) != kArrays.end();
    Require(matrix || array || std::find(kVectors.begin(), kVectors.end(), name) != kVectors.end());
    Require(dimension == 0 && register_count == (matrix || array ? 4u : 1u) &&
            type == (windows ? 16u : 0u) + (matrix ? 4u : 2u) &&
            number == (array     ? 4u
                       : windows ? 0u
                                 : 1u));
    if (windows) {
        Require(register_index % 16 == 0 && register_index + register_count * 16 <= 1024);
        for (std::uint32_t offset = 0; offset < register_count; ++offset)
            Require(occupied.insert(register_index / 16 + offset).second);
    } else {
        Require(register_index == 0);
    }
}
}  // namespace
void Validate(std::span<const std::uint8_t> bytes, Target target, Profile profile) {
    Require(profile == Profile::kImageRgba || profile == Profile::kSurfaceRgb);
    Require(bytes.size() >= 27 && bytes.size() <= kMaximumArtifactBytes &&
            (target == Target::kWindowsSm5 || target == Target::kGles300));
    Reader reader(bytes);
    // Fixed current shader container revision, fragment stage, profile-specific
    // varyings and no raw storage/image bindings. Derived from the retained
    // backend's shader reader and checked against both actual compiler outputs.
    Require(reader.Word(4) == 0x0c485346 &&
            reader.Word(4) == (profile == Profile::kImageRgba ? 0xe1f28301u : 601528614u) &&
            reader.Word(4) == 0);
    Require(reader.Word(4) == 0 && reader.Word(4) == 0);
    const auto count = reader.Word(2);
    Require(count <= (profile == Profile::kImageRgba ? 3u : 24u));
    std::set<std::uint32_t> occupied;
    std::set<std::string> names;
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto name_bytes = reader.Take(reader.Word(1));
        const std::string name(name_bytes.begin(), name_bytes.end());
        Require(names.insert(name).second);
        const auto type = reader.Word(1), number = reader.Word(1);
        const auto register_index = reader.Word(2), register_count = reader.Word(2);
        const auto component = reader.Word(1), dimension = reader.Word(1), format = reader.Word(2);
        Require(component == 0 && format == 0);
        if (profile == Profile::kSurfaceRgb) {
            SurfaceBinding(name, type, number, register_index, register_count, dimension, target,
                           occupied);
            continue;
        }
        Require(register_count == 1);
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
        Require(reader.Word(1) == 0);
        const auto uniform_bytes = reader.Word(2);
        Require(uniform_bytes <= (profile == Profile::kImageRgba ? 32u : 1024u) &&
                reader.Remaining() == 0);
        if (profile == Profile::kSurfaceRgb)
            Require(uniform_bytes % 16 == 0 &&
                    (occupied.empty() || (*occupied.rbegin() + 1) * 16 <= uniform_bytes));
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
}  // namespace rhythm::shader_artifact
