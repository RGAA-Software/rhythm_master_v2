#pragma once

#include <array>

#include "rhythm/editor/commands.h"

namespace rhythm::studio {
// Owns the export-name draft and binding-command diagnostics for one selection.
class BindingEditor final {
   public:
    std::optional<editor::Snapshot> Draw(const editor::Snapshot& snapshot, graph::NodeId selected,
                                         const graph::Registry& registry,
                                         const std::map<std::string, std::string>& text);
    void Reset();

   private:
    graph::NodeId selected_ = 0;
    std::array<char, 129> name_{};
    std::optional<graph::Diagnostic> error_{};
};
}  // namespace rhythm::studio
