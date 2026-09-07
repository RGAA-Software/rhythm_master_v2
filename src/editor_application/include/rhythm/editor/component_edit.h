#pragma once

#include "rhythm/editor/commands.h"

namespace rhythm::editor {
// UI-thread value draft. Nested navigation shares one undo history; publishing
// the library is a revision-checked transaction against the original project.
class ComponentEdit final {
   public:
    ComponentEdit(Snapshot project, std::string type);
    const graph::ComponentDefinition& Definition() const;
    std::span<const std::string> Path() const { return path_; }
    Snapshot Body() const;
    bool ReplaceBody(Snapshot body);
    bool ReplaceInterface(graph::ComponentDefinition definition);
    bool Enter(graph::NodeId instance);
    void Navigate(std::size_t index);
    bool Undo();
    bool Redo();
    graph::NodeId ReserveNodeId();
    EditResult Finish(const Snapshot& current, const graph::Registry& registry) const;

   private:
    void RepairPath();
    std::string project_id_{};
    std::uint64_t project_revision_ = 0;
    History history_{Snapshot{}};
    std::vector<std::string> path_{};
    std::map<std::string, graph::NodeId> next_ids_{};
};
}  // namespace rhythm::editor
