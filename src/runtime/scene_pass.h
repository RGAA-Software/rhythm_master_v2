#pragma once

#include <tuple>

#include "rhythm/runtime/runtime.h"
#include "shadow_pass.h"

namespace rhythm::runtime::detail {
// Host-thread GPU cache keyed by published geometry identity, not object address.
// Owns uploads and validated hierarchy, and prunes geometry no longer in the scene.
class ScenePass final {
   public:
    render::SceneDrawList Build(const scene::Scene& scene, const scene::Camera& camera,
                                render::Extent extent, render::Renderer& renderer,
                                std::span<const NodeOutput> outputs = {});

   private:
    struct Uploaded {
        std::shared_ptr<const scene::Model> model_{};
        std::map<scene::NodeId, scene::WorldNode> worlds_{};
        std::vector<render::Mesh> meshes_{};
    };
    using Key = std::tuple<std::uint64_t, std::uint64_t, bool>;
    std::map<Key, Uploaded> uploads_{};
    ShadowPass shadow_{};
};
}  // namespace rhythm::runtime::detail
