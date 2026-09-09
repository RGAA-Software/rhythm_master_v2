#include "shader_assets.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::prepared_assets::detail {
std::shared_ptr<const image_shader::Resources> PrepareShaders(
        const graph::ExecutionPlan& plan, std::span<const project::PackagedAsset> assets,
        std::stop_token stop, const image_shader::Resources& previous) {
    auto resources = std::make_shared<image_shader::Resources>();
    std::size_t total = 0;
    for (const auto& instruction : plan.instructions_) {
        if (stop.stop_requested()) throw std::runtime_error("asset.cancelled");
        if (instruction.operation_ != graph::Operation::kTextureShader) continue;
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        if (resources->programs_.contains(id.sha256_)) continue;
        const auto found = std::find_if(assets.begin(), assets.end(),
                                        [&](const auto& asset) { return asset.record_.id_ == id; });
        if (!assets::ValidId(id) || found == assets.end())
            throw std::invalid_argument("shader.asset_missing");
        if (found->record_.media_type_ != image_shader::kMediaType)
            throw std::invalid_argument("shader.media_type");
        if (resources->programs_.size() == image_shader::kMaximumPrograms ||
            found->bytes_.size() > image_shader::kMaximumProgramBytes - total)
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
        resources->programs_.emplace(id.sha256_, image_shader::Decode(bytes));
    }
    return resources;
}
}  // namespace rhythm::prepared_assets::detail
