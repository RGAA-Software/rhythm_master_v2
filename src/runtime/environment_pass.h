#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
// Owns one bounded linear atlas per scene renderer. Source version, handle and
// device presentation generation invalidate its GPU contents; energy/rotation do not.
class EnvironmentPass final {
   public:
    void Apply(const std::optional<scene::EnvironmentSettings>& environment,
               std::span<const NodeOutput> outputs, render::SceneDrawList& receivers,
               render::Renderer& renderer);

   private:
    struct Key {
        render::TextureHandle source_{};
        std::uint64_t version_ = 0;
        std::uint64_t presentation_ = 0;
        bool srgb_ = true;
        bool operator==(const Key&) const = default;
    };
    render::Texture atlas_{};
    std::optional<Key> key_{};
};
}  // namespace rhythm::runtime::detail
