#include "rhythm/model_assets/prepare.h"

#include <picosha2.h>

#include <algorithm>
#include <set>
#include <stdexcept>

#include "rhythm/model_import/gltf.h"

namespace rhythm::model_assets {
namespace {
std::vector<graph::GeometryBudget> Budgets(const graph::ExecutionPlan& plan,
                                           const scene::Resources& resources) {
    std::vector<graph::GeometryBudget> result;
    for (const auto& instruction : plan.instructions_) {
        if (instruction.operation_ != graph::Operation::kGeometryGlb) continue;
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        const auto& model = scene::FindModel(resources, id);
        result.push_back({instruction.node_.id_, model.vertices_, model.indices_,
                          model.draw_indices_, model.draws_});
    }
    return result;
}
void CheckCancelled(std::stop_token stop) {
    if (stop.stop_requested()) throw std::runtime_error("model.cancelled");
}
}  // namespace

bool Covers(const graph::ExecutionPlan& plan, const scene::Resources& resources) {
    try {
        const auto budgets = Budgets(plan, resources);
        return !graph::ValidateSceneBudget(plan, budgets);
    } catch (const std::exception&) {
        return false;
    }
}

std::shared_ptr<const scene::Resources> Prepare(const graph::ExecutionPlan& plan,
                                                std::span<const project::PackagedAsset> assets,
                                                std::stop_token stop) {
    CheckCancelled(stop);
    if (assets.size() > project::kMaximumPackageAssets || plan.instructions_.size() > 10000)
        throw std::length_error("model.asset_count");
    std::size_t total = 0;
    std::set<std::string> ids;
    for (const auto& asset : assets) {
        CheckCancelled(stop);
        if (!assets::ValidId(asset.record_.id_) ||
            !assets::ValidMediaType(asset.record_.media_type_) ||
            asset.record_.bytes_ != asset.bytes_.size() ||
            !ids.insert(asset.record_.id_.sha256_).second)
            throw std::invalid_argument("model.asset_record");
        if (asset.bytes_.size() > project::kMaximumPackageAssetBytes - total)
            throw std::length_error("model.asset_bytes");
        total += asset.bytes_.size();
        if (picosha2::hash256_hex_string(asset.bytes_.begin(), asset.bytes_.end()) !=
            asset.record_.id_.sha256_)
            throw std::invalid_argument("model.asset_hash");
    }
    auto resources = std::make_shared<scene::Resources>();
    ids.clear();
    std::uint64_t vertices = 0, indices = 0;
    for (const auto& instruction : plan.instructions_) {
        CheckCancelled(stop);
        if (instruction.operation_ != graph::Operation::kGeometryGlb) continue;
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        if (!assets::ValidId(id)) throw std::invalid_argument("model.asset_missing");
        if (!ids.insert(id.sha256_).second) continue;
        const auto found = std::find_if(assets.begin(), assets.end(),
                                        [&](const auto& asset) { return asset.record_.id_ == id; });
        if (found == assets.end()) throw std::invalid_argument("model.asset_missing");
        const auto bytes = std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(found->bytes_.data()), found->bytes_.size());
        auto model = scene::DescribeModel(id, model_import::ReadGlb(bytes, stop));
        vertices += model.vertices_;
        indices += model.indices_;
        if (vertices > 250000 || indices > 750000) throw std::length_error("graph.scene_budget");
        resources->models_.push_back(std::move(model));
    }
    CheckCancelled(stop);
    const auto budgets = Budgets(plan, *resources);
    if (graph::ValidateSceneBudget(plan, budgets)) throw std::length_error("graph.scene_budget");
    return resources;
}
}  // namespace rhythm::model_assets
