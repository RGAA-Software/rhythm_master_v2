#pragma once

#include <array>
#include <map>
#include <set>

#include "catalog_preview.h"
#include "rhythm/platform/host.h"
#include "rhythm/project/store.h"

namespace rhythm::studio {
// Owns catalog selection, bounded thumbnail textures and one asynchronous live preview.
class TemplateBrowser final {
   public:
    std::optional<std::size_t> Draw(std::span<const project::ContentEntry> entries,
                                    const std::string& locale,
                                    const std::map<std::string, std::string>& text,
                                    platform::Host& host, render::Renderer& renderer,
                                    double seconds, const runtime::ExternalInputs& inputs = {});

   private:
    render::TextureHandle Thumbnail(std::size_t index, const project::ContentEntry& entry,
                                    render::Renderer& renderer);
    std::array<char, 129> filter_{};
    std::string tier_{};
    std::string category_{};
    std::optional<std::size_t> selected_{};
    std::map<std::size_t, render::Texture> thumbnails_{};
    std::set<std::size_t> missing_thumbnails_{};
    CatalogPreview preview_{};
};
}  // namespace rhythm::studio
