#pragma once

#include <map>

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
struct ImagePlacement {
    render::Extent source_{};
    double pixel_aspect_ = 1;
    double clockwise_rotation_ = 0;
    bool fill_ = false;
};
render::DrawList FramedImageDraw(render::TextureHandle texture, render::Extent extent,
                                 const ImagePlacement& placement);
// One upload per immutable asset, shared by all image-node previews and output.
// Render-thread confined, cleared with Runtime's device/resource lifecycle.
class ImageUploads final {
   public:
    void Retain(const graph::ExecutionPlan& plan, const assets::Images& images);
    render::DrawList Draw(const graph::Node& node, render::Extent extent,
                          const assets::Images& images, render::Renderer& renderer);

   private:
    std::map<std::string, render::Texture> textures_{};
};
}  // namespace rhythm::runtime::detail
