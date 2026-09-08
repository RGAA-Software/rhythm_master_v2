#pragma once

#include <array>

#include "rhythm/content/component_library.h"

namespace rhythm::studio {
struct LibraryRequest {
    std::optional<graph::NodeId> save_instance_{};
    std::filesystem::path load_directory_{};
};
struct LibraryInsertion {
    content::LibraryResult result_{};
    editor::Position position_{};
};
// Owns the per-user library view and asynchronous operation state. Structural
// edits are returned to Studio's normal expected-revision/undo boundary.
class ComponentLibraryPanel final {
   public:
    void Initialize(const std::filesystem::path& directory);
    bool Busy() const { return library_ && library_->Busy(); }
    std::optional<LibraryInsertion> Take();
    std::optional<LibraryRequest> Draw(const graph::Document& document,
                                       std::span<const graph::NodeId> selection,
                                       const std::map<std::string, std::string>& text);
    void Start(const LibraryRequest& request, const editor::Snapshot& snapshot,
               const std::filesystem::path& project_assets, editor::Position insertion);
    bool StartOfficial(const content::Semantic& semantic, const editor::Snapshot& snapshot,
                       const std::filesystem::path& project_assets, editor::Position insertion);

   private:
    std::optional<content::ComponentLibrary> library_{};
    std::vector<content::ComponentEntry> entries_{};
    std::array<char, 4096> import_path_{};
    std::string status_{};
    std::filesystem::path saved_{};
    editor::Position insertion_{};
};
}  // namespace rhythm::studio
