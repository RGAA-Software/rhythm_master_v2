#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
// Host-thread GPU cache keyed by published geometry identity, not object address.
// Owns uploads and validated hierarchy, and prunes geometry no longer in the scene.
class ScenePass final {
   public:
    render::SceneDrawList Build(const scene::Scene& scene, const scene::Camera& camera,
                                render::Extent extent, render::Renderer& renderer);

   private:
    struct Uploaded {
        std::shared_ptr<const scene::Model> model_{};
        std::map<scene::NodeId, scene::WorldNode> worlds_{};
        std::vector<render::Mesh> meshes_{};
    };
    using Key = std::pair<std::uint64_t, std::uint64_t>;
    std::map<Key, Uploaded> uploads_{};
};
}  // namespace rhythm::runtime::detail
