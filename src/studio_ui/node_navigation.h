#pragma once

#include <imgui.h>

#include <set>

#include "rhythm/graph/document.h"

namespace rhythm::studio {
struct NodeNavigationAction {
    std::optional<graph::NodeId> selected_{};
    bool focus_ = false;
    bool fit_ = false;
    bool inspect_ = false;
};
// Read-only author navigation. The cached index is rebuilt on revision, locale
// or scope changes; only visible rows are submitted to ImGui.
class NodeNavigation final {
   public:
    NodeNavigationAction Draw(const graph::Document& document, graph::NodeId selected,
                              const std::map<std::string, std::string>& text);
    void Reset();

   private:
    struct Entry {
        graph::NodeId id_ = 0;
        std::string label_{};
        std::string searchable_{};
    };
    ImGuiTextFilter filter_{};
    std::vector<Entry> entries_{};
    std::map<graph::NodeId, std::set<graph::NodeId>> upstream_{};
    std::map<graph::NodeId, std::set<graph::NodeId>> downstream_{};
    std::string document_id_{};
    std::string language_{};
    std::uint64_t revision_ = 0;
    bool dirty_ = true;
    bool invalid_bindings_ = false;
    int direction_ = 0;
};
}  // namespace rhythm::studio
