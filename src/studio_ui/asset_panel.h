#pragma once

#include <array>
#include <map>
#include <optional>
#include <span>

#include "rhythm/assets/importer.h"

namespace rhythm::studio {
struct AssetEdit {
    std::optional<assets::AssetRecord> added_{};
    std::optional<assets::AssetId> removed_{};
};
// Owns the import interaction and bounded worker. Results are applied by Studio
// as normal history commands; the worker never touches editor state.
class AssetPanel final {
   public:
    AssetEdit Draw(const std::filesystem::path& directory,
                   std::span<const assets::AssetRecord> records,
                   const std::map<std::string, std::string>& catalog);

   private:
    assets::Importer importer_{};
    std::array<char, 4097> source_{};
    std::string status_{};
};
}  // namespace rhythm::studio
