#pragma once

#include "image_pass.h"

namespace rhythm::runtime::detail {
// Uploads complete before graph passes; stable textures are updated in place.
class VideoUploads final {
   public:
    void Prepare(const graph::ExecutionPlan& plan, std::span<const VideoInput> inputs,
                 render::Renderer& renderer);
    std::uint64_t Revision(graph::NodeId node) const;
    render::DrawList Draw(const graph::Node& node, render::Extent extent) const;

   private:
    struct Entry {
        assets::AssetId source_{};
        std::uint64_t revision_ = 0;
        std::uint64_t generation_ = 0;
        ImagePlacement placement_{};
        render::Texture texture_{};
    };
    std::map<graph::NodeId, Entry> entries_{};
};
}  // namespace rhythm::runtime::detail
