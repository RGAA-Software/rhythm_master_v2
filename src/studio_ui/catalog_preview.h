#pragma once

#include "rhythm/player/package_loader.h"
#include "rhythm/player/session.h"

namespace rhythm::studio {
// One immutable catalog selection, one preparation worker and one live GPU session.
// UI/host thread only. A worker exists only during package preparation; completed
// previews hold no pool lease. Closing cancels pending work and releases GPU state.
class CatalogPreview final {
   public:
    void Select(std::filesystem::path package);
    void Update(render::Renderer& renderer, double seconds, const runtime::ExternalInputs& inputs);
    void Close();
    void Retry();
    render::TextureHandle Texture() const { return texture_; }
    render::Extent Extent() const { return extent_; }
    bool Failed() const { return failed_; }

   private:
    std::filesystem::path selected_{};
    std::filesystem::path requested_{};
    std::optional<player::PackageLoader> loader_{};
    player::Session session_{};
    render::TextureHandle texture_{};
    render::Extent extent_{};
    bool failed_ = false;
};
}  // namespace rhythm::studio
