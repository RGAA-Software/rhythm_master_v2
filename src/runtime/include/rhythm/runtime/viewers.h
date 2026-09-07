#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime {
namespace detail {
class ScenePreviews;
}
// Host-thread preview cache. Bounded GPU copies, fixed 256x144, 15 Hz.
// BeginFrame controls evaluation of viewer-only branches; Capture never reads
// pixels back to the CPU. Hidden previews release their owned textures.
class Viewers final {
   public:
    Viewers();
    ~Viewers();
    Viewers(const Viewers&) = delete;
    Viewers& operator=(const Viewers&) = delete;
    static constexpr std::size_t kMaxPreviews = 8;
    bool BeginFrame(double seconds, bool visible, std::uint64_t reset_generation);
    void Capture(const FrameResult& frame, std::span<const graph::NodeId> nodes,
                 render::Renderer& renderer);
    std::span<const NodeOutput> Outputs() const { return outputs_; }

   private:
    std::vector<render::Texture> textures_{};
    render::Texture point_sprite_{};
    std::vector<NodeOutput> outputs_{};
    std::optional<double> last_capture_{};
    std::uint64_t reset_generation_ = 0;
    bool due_ = false;
    std::unique_ptr<detail::ScenePreviews> scenes_{};
};
}  // namespace rhythm::runtime
