#pragma once

#include <array>

#include "catalog_preview.h"
#include "rhythm/content/semantic.h"
#include "rhythm/platform/host.h"

namespace rhythm::studio {
class SemanticPalette final {
   public:
    std::optional<std::size_t> Draw(std::span<const content::Semantic> entries,
                                    const std::string& locale,
                                    const std::map<std::string, std::string>& text,
                                    platform::Host& host, render::Renderer& renderer,
                                    double seconds, const runtime::ExternalInputs& inputs);

   private:
    std::array<char, 129> filter_{};
    std::optional<std::size_t> selected_{};
    CatalogPreview preview_{};
    graph::Registry registry_{};
};
}  // namespace rhythm::studio
