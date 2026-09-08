#include "texture_lifetimes.h"

#include <algorithm>
#include <set>

namespace rhythm::runtime::detail {
namespace {
bool DynamicSource(graph::Operation operation) {
    using enum graph::Operation;
    switch (operation) {
        case kTime:
        case kSessionTime:
        case kParticipantRole:
        case kSharedControl:
        case kAudioFeature:
        case kAudioBand:
        case kAudioSpectrum:
        case kPointInstances:
        case kFeedback:
        case kTextureTrail:
        case kTextureVideo:
        case kTextureShader:
        case kParticleEmitter:
        case kGpuParticleEmitter:
        case kPointPhysics:
            return true;
        default:
            return false;
    }
}
bool OrdinaryTarget(graph::Operation operation) {
    using enum graph::Operation;
    switch (operation) {
        case kGradient:
        case kTransform:
        case kBlend:
        case kAudioSpectrum:
        case kAffine:
        case kShape:
        case kMask:
        case kComposite:
        case kColorAdjust:
        case kPointRender:
        case kGpuPointRender:
        case kTextureLinearize:
        case kTextureDisplay:
        case kTextureFxaa:
        case kDepthLinearize:
        case kDepthOfField:
        case kTextureNoise:
        case kTextureMapping:
        case kTextureContours:
        case kTextureDisplace:
        case kTextureStack:
        case kTextureShader:
            return true;
        default:
            return false;
    }
}
}  // namespace
bool SamePlan(const graph::ExecutionPlan& a, const graph::ExecutionPlan& b) {
    return a.document_id_ == b.document_id_ && a.revision_ == b.revision_ &&
           a.output_ == b.output_ && a.canvas_ == b.canvas_ &&
           std::equal(a.instructions_.begin(), a.instructions_.end(), b.instructions_.begin(),
                      b.instructions_.end(), [](const auto& left, const auto& right) {
                          return left.node_ == right.node_ && left.operation_ == right.operation_ &&
                                 left.inputs_ == right.inputs_;
                      });
}
void TextureLifetimes::Prepare(const graph::ExecutionPlan& plan,
                               const std::optional<std::vector<graph::NodeId>>& retained) {
    if (plan_ && retained_ == retained && SamePlan(*plan_, plan)) return;
    ++generation_;
    plan_ = plan;
    retained_ = retained;
    const auto count = plan.instructions_.size();
    primary_.assign(count, false);
    recyclable_.assign(count, false);
    retire_.assign(count + 1, {});
    std::vector<std::size_t> pending{plan.output_};
    while (!pending.empty()) {
        const auto index = pending.back();
        pending.pop_back();
        if (primary_.at(index)) continue;
        primary_[index] = true;
        for (const auto input : plan.instructions_[index].inputs_)
            if (input) pending.push_back(*input);
    }
    if (!retained) return;
    const std::set<graph::NodeId> observed(retained->begin(), retained->end());
    std::vector<bool> pinned(count, false), dynamic(count, false);
    std::vector<std::size_t> last(count);
    std::vector<std::vector<std::size_t>> consumers(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto& instruction = plan.instructions_[index];
        last[index] = std::max(last[index], index);
        pinned[index] =
                pinned[index] || index == plan.output_ || observed.contains(instruction.node_.id_);
        if (DynamicSource(instruction.operation_)) {
            dynamic[index] = true;
            pending.push_back(index);
        }
        for (const auto input : instruction.inputs_) {
            if (!input) continue;
            consumers.at(*input).push_back(index);
            last.at(*input) =
                    std::max(last[*input],
                             instruction.operation_ == graph::Operation::kFeedback ? count : index);
            // These outputs can alias an input. Preserve that input independently
            // of downstream demand, including zero-radius blur and zero-life trail.
            if (instruction.operation_ == graph::Operation::kOutput ||
                instruction.operation_ == graph::Operation::kGaussianBlur ||
                instruction.operation_ == graph::Operation::kTextureTrail ||
                instruction.operation_ == graph::Operation::kMaterialTextures ||
                instruction.operation_ == graph::Operation::kSceneEnvironment)
                pinned.at(*input) = true;
        }
    }
    // Propagate through feedback cycles without relying on topological order.
    while (!pending.empty()) {
        const auto index = pending.back();
        pending.pop_back();
        for (const auto consumer : consumers[index]) {
            if (dynamic[consumer]) continue;
            dynamic[consumer] = true;
            pending.push_back(consumer);
        }
    }
    for (std::size_t index = 0; index < count; ++index) {
        recyclable_[index] = dynamic[index] && !pinned[index] &&
                             OrdinaryTarget(plan.instructions_[index].operation_);
        if (recyclable_[index]) retire_[last[index]].push_back(index);
    }
}
render::Texture TexturePool::Acquire(render::Extent extent, render::Renderer& renderer,
                                     render::TexturePrecision precision) {
    const auto found = std::find_if(free_.begin(), free_.end(), [&](const auto& entry) {
        return entry.extent_ == extent && entry.precision_ == precision;
    });
    if (found == free_.end()) return renderer.CreateTexture(extent, {}, precision);
    auto texture = std::move(found->texture_);
    free_.erase(found);
    return texture;
}
void TexturePool::Recycle(render::Texture texture, render::Extent extent,
                          render::TexturePrecision precision) {
    if (texture.Handle().device_) free_.push_back({std::move(texture), extent, precision, epoch_});
}
void TexturePool::EndFrame() {
    std::erase_if(free_, [&](const auto& entry) { return entry.epoch_ != epoch_; });
}
}  // namespace rhythm::runtime::detail
