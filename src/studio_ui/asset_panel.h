#pragma once

#include <array>
#include <map>
#include <optional>
#include <span>

#include "rhythm/assets/importer.h"
#include "rhythm/editor/asset_commands.h"

namespace rhythm::studio {
struct AssetEdit {
    std::optional<assets::AssetRecord> added_{};
    std::optional<assets::AssetId> removed_{};
    std::optional<assets::AssetId> replaced_{};
    std::optional<assets::AssetId> restored_{};
    std::optional<std::vector<assets::AssetCheck>> checked_{};
};
// Owns the import interaction and bounded worker. Results are applied by Studio
// as normal history commands; the worker never touches editor state.
class AssetPanel final {
   public:
    AssetEdit Draw(const std::filesystem::path& directory, const editor::Snapshot& snapshot,
                   const std::map<std::string, std::string>& catalog);
    bool Busy() const { return importer_.Busy(); }
    void Report(std::string status, assets::AssetId selected) {
        status_ = std::move(status);
        selected_ = std::move(selected);
    }

   private:
    assets::Importer importer_{};
    std::array<char, 4097> source_{};
    std::string status_{};
    assets::AssetId selected_{};
    std::optional<assets::AssetRecord> replacing_{};
    std::vector<assets::AssetCheck> checks_{};
    std::filesystem::path directory_{};
    std::string project_id_{};
    bool discard_pending_ = false;
};
}  // namespace rhythm::studio
