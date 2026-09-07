#include "video_pass.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace rhythm::runtime::detail {
void VideoUploads::Prepare(const graph::ExecutionPlan& plan, std::span<const VideoInput> inputs,
                           render::Renderer& renderer) {
    if (inputs.size() > 4) throw std::length_error("video.instance_budget");
    std::set<graph::NodeId> active;
    for (const auto& instruction : plan.instructions_) {
        if (instruction.operation_ != graph::Operation::kTextureVideo) continue;
        if (!active.insert(instruction.node_.id_).second || active.size() > 4)
            throw std::length_error("video.instance_budget");
        const auto& source = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        const auto input = std::find_if(inputs.begin(), inputs.end(), [&](const auto& candidate) {
            return candidate.node_ == instruction.node_.id_ && candidate.source_ == source;
        });
        if (input == inputs.end() || !input->frame_) {
            entries_.erase(instruction.node_.id_);
            continue;
        }
        const auto& image = *input->frame_;
        const auto& info = image.info_;
        if (!assets::ValidId(source) || !input->revision_ || !info.width_ || !info.height_ ||
            info.width_ > 4096 || info.height_ > 4096 ||
            std::uint64_t(info.width_) * info.height_ > 2073600 ||
            image.rgba_.size() != std::size_t(info.width_) * info.height_ * 4 ||
            !std::isfinite(info.pixel_aspect_) || info.pixel_aspect_ <= 0 ||
            info.pixel_aspect_ > 100 || !std::isfinite(info.clockwise_rotation_))
            throw std::invalid_argument("video.invalid_frame");
        auto& entry = entries_[input->node_];
        const render::Extent extent{static_cast<std::uint16_t>(info.width_),
                                    static_cast<std::uint16_t>(info.height_)};
        if (entry.source_ != source || entry.placement_.source_ != extent) entry = {};
        if (!renderer.IsValid(entry.texture_.Handle()))
            entry.texture_ = renderer.CreateTexture(extent, image.rgba_);
        else if (entry.revision_ != input->revision_ || entry.generation_ != input->generation_)
            renderer.UpdateTexture(entry.texture_.Handle(), image.rgba_);
        entry.source_ = source;
        entry.revision_ = input->revision_;
        entry.generation_ = input->generation_;
        entry.placement_ = {extent, info.pixel_aspect_, info.clockwise_rotation_, false};
    }
    std::erase_if(entries_, [&](const auto& item) { return !active.contains(item.first); });
}
std::uint64_t VideoUploads::Revision(graph::NodeId node) const {
    const auto found = entries_.find(node);
    return found == entries_.end() ? 0 : found->second.revision_;
}
render::DrawList VideoUploads::Draw(const graph::Node& node, render::Extent extent) const {
    const auto found = entries_.find(node.id_);
    if (found == entries_.end()) {
        render::DrawList empty;
        empty.width_ = extent.width_;
        empty.height_ = extent.height_;
        return empty;
    }
    auto placement = found->second.placement_;
    placement.fill_ = graph::Scalar(node, "image_fill", 0) != 0;
    return FramedImageDraw(found->second.texture_.Handle(), extent, placement);
}
}  // namespace rhythm::runtime::detail
