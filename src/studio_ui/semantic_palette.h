#pragma once

#include <array>

#include "rhythm/content/semantic.h"

namespace rhythm::studio {
class SemanticPalette final {
   public:
    std::optional<std::size_t> Draw(std::span<const content::Semantic> entries,
                                    const std::string& locale,
                                    const std::map<std::string, std::string>& text);

   private:
    std::array<char, 129> filter_{};
};
}  // namespace rhythm::studio
