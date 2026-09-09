#include "shader_assets.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::prepared_assets::detail {
namespace {
struct Profile {
    graph::Operation operation_ = graph::Operation::kTextureShader;
    std::string_view media_type_{};
    std::size_t maximum_programs_ = 0;
    std::size_t maximum_bytes_ = 0;
};
template <typename Resources, typename Decode>
std::shared_ptr<const Resources> PreparePrograms(const graph::ExecutionPlan& plan,
                                                 std::span<const project::PackagedAsset> assets,
                                                 std::stop_token stop, const Resources& previous,
                                                 Profile profile, Decode decode) {
    auto resources = std::make_shared<Resources>();
    std::size_t total = 0;
    for (const auto& instruction : plan.instructions_) {
        if (stop.stop_requested()) throw std::runtime_error("asset.cancelled");
        if (instruction.operation_ != profile.operation_) continue;
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        if (resources->programs_.contains(id.sha256_)) continue;
        const auto found = std::find_if(assets.begin(), assets.end(),
                                        [&](const auto& asset) { return asset.record_.id_ == id; });
        if (!assets::ValidId(id) || found == assets.end())
            throw std::invalid_argument("shader.asset_missing");
        if (found->record_.media_type_ != profile.media_type_)
            throw std::invalid_argument("shader.media_type");
        if (resources->programs_.size() == profile.maximum_programs_ ||
            found->bytes_.size() > profile.maximum_bytes_ - total)
            throw std::length_error("shader.program_budget");
        total += found->bytes_.size();
        const auto cached = previous.programs_.find(id.sha256_);
        if (cached != previous.programs_.end()) {
            resources->programs_.emplace(id.sha256_, cached->second);
            continue;
        }
        // Package records and hashes have already been checked by model preparation.
        const auto bytes = std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(found->bytes_.data()), found->bytes_.size());
        resources->programs_.emplace(id.sha256_, decode(bytes));
    }
    return resources;
}
}  // namespace
std::shared_ptr<const image_shader::Resources> PrepareShaders(
        const graph::ExecutionPlan& plan, std::span<const project::PackagedAsset> assets,
        std::stop_token stop, const image_shader::Resources& previous) {
    return PreparePrograms(plan, assets, stop, previous,
                           {graph::Operation::kTextureShader, image_shader::kMediaType,
                            image_shader::kMaximumPrograms, image_shader::kMaximumProgramBytes},
                           image_shader::Decode);
}
std::shared_ptr<const surface_shader::Resources> PrepareSurfaces(
        const graph::ExecutionPlan& plan, std::span<const project::PackagedAsset> assets,
        std::stop_token stop, const surface_shader::Resources& previous) {
    return PreparePrograms(plan, assets, stop, previous,
                           {graph::Operation::kMaterialShader, surface_shader::kMediaType,
                            surface_shader::kMaximumPrograms, surface_shader::kMaximumProgramBytes},
                           surface_shader::Decode);
}
}  // namespace rhythm::prepared_assets::detail
