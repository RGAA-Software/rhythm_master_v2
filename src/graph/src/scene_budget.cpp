#include <algorithm>
#include <cmath>

#include "rhythm/graph/compiler.h"

namespace rhythm::graph {
std::optional<Diagnostic> ValidateSceneBudget(
        const ExecutionPlan& plan, std::optional<std::span<const GeometryBudget>> geometry) {
    if (plan.instructions_.size() > 10000) return Diagnostic{"graph.limit"};
    struct Counts {
        std::uint64_t instances_ = 0;
        std::uint64_t indices_ = 0;
        std::uint64_t lights_ = 0;
        std::uint64_t draws_ = 0;
    };
    std::vector<Counts> counts(plan.instructions_.size());
    std::uint64_t vertices = 0, indices = 0, snapshots = 0, draws = 0;
    for (std::size_t index = 0; index < plan.instructions_.size(); ++index) {
        const auto& instruction = plan.instructions_[index];
        auto& count = counts[index];
        const auto fail = [&] {
            return std::optional<Diagnostic>{{"graph.scene_budget", instruction.node_.id_}};
        };
        const auto source = [&](std::size_t port) -> std::optional<Counts> {
            if (port >= instruction.inputs_.size() || !instruction.inputs_[port] ||
                *instruction.inputs_[port] >= index)
                return {};
            return counts[*instruction.inputs_[port]];
        };
        switch (instruction.operation_) {
            case Operation::kGeometryGlb: {
                const auto asset = instruction.node_.properties_.find("asset");
                if (asset == instruction.node_.properties_.end() ||
                    !std::holds_alternative<assets::AssetId>(asset->second) ||
                    !assets::ValidId(std::get<assets::AssetId>(asset->second)))
                    return Diagnostic{"graph.asset_missing", instruction.node_.id_, "asset"};
                count.instances_ = 1;
                if (geometry) {
                    const auto found = std::find_if(
                            geometry->begin(), geometry->end(),
                            [&](const auto& item) { return item.node_ == instruction.node_.id_; });
                    if (found == geometry->end() || found->vertices_ > 250000 ||
                        found->indices_ > 750000 || found->draw_indices_ > 3000000 ||
                        found->draws_ > 4096)
                        return fail();
                    vertices += found->vertices_;
                    indices += found->indices_;
                    count.indices_ = found->draw_indices_;
                    count.draws_ = found->draws_;
                }
                break;
            }
            case Operation::kDirectionalLight:
                count.lights_ = 1;
                break;
            case Operation::kGeometryCube:
                vertices += 24;
                indices += 36;
                count = {1, 36};
                count.draws_ = 1;
                break;
            case Operation::kGeometrySphere: {
                const auto segments = Scalar(instruction.node_, "radial_segments", 32);
                const auto rings = Scalar(instruction.node_, "rings", 16);
                if (!std::isfinite(segments) || !std::isfinite(rings) || segments < 3 ||
                    segments > 256 || rings < 1 || rings > 128 ||
                    std::floor(segments) != segments || std::floor(rings) != rings)
                    return fail();
                vertices += static_cast<std::uint64_t>((rings + 2) * (segments + 1));
                count = {1, static_cast<std::uint64_t>((rings + 1) * segments * 6)};
                count.draws_ = 1;
                indices += count.indices_;
                break;
            }
            case Operation::kGeometryTorus: {
                const auto segments = Scalar(instruction.node_, "radial_segments", 64);
                const auto tubes = Scalar(instruction.node_, "tube_segments", 16);
                if (!std::isfinite(segments) || !std::isfinite(tubes) || segments < 3 ||
                    segments > 256 || tubes < 3 || tubes > 128 ||
                    std::floor(segments) != segments || std::floor(tubes) != tubes)
                    return fail();
                vertices += static_cast<std::uint64_t>((segments + 1) * (tubes + 1));
                count = {1, static_cast<std::uint64_t>(segments * tubes * 6)};
                count.draws_ = 1;
                indices += count.indices_;
                break;
            }
            case Operation::kPointInstances:
            case Operation::kSceneInstance:
            case Operation::kSceneTransform:
            case Operation::kSceneMerge:
            case Operation::kSceneRender:
            case Operation::kSceneCapture: {
                const auto a = source(0);
                if (!a) return fail();
                count = *a;
                if (instruction.operation_ == Operation::kPointInstances) {
                    const auto limit = Scalar(instruction.node_, "instance_limit", 1024);
                    if (!std::isfinite(limit) || limit < 1 || limit > kMaximumSceneInstances ||
                        std::floor(limit) != limit || instruction.inputs_.size() < 2 ||
                        !instruction.inputs_[1] || *instruction.inputs_[1] >= index)
                        return fail();
                    count.instances_ *= static_cast<std::uint64_t>(limit);
                    count.indices_ *= static_cast<std::uint64_t>(limit);
                    count.draws_ *= static_cast<std::uint64_t>(limit);
                }
                if (instruction.operation_ == Operation::kSceneMerge) {
                    const auto b = source(1);
                    if (!b) return fail();
                    count.instances_ += b->instances_;
                    count.indices_ += b->indices_;
                    count.lights_ += b->lights_;
                    count.draws_ += b->draws_;
                }
                if (instruction.operation_ == Operation::kSceneRender ||
                    instruction.operation_ == Operation::kSceneCapture) {
                    draws += count.indices_;
                    count = {};
                } else {
                    snapshots += count.instances_;
                }
                break;
            }
            default:
                break;
        }
        if (vertices > 250000 || indices > 750000 || count.instances_ > kMaximumSceneInstances ||
            count.indices_ > 3000000 || snapshots > kMaximumSceneSnapshots || draws > 3000000 ||
            count.lights_ > 4 || count.draws_ > 16384)
            return fail();
    }
    return {};
}
}  // namespace rhythm::graph
