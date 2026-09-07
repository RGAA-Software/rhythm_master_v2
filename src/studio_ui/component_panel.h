#pragma once

#include <array>

#include "rhythm/editor/commands.h"

namespace rhythm::studio {
enum class ComponentActionKind { kCreate, kAdd, kExpand, kEdit, kDetach, kUnpack };
struct ComponentAction {
    ComponentActionKind kind_ = ComponentActionKind::kCreate;
    std::string value_{};
};
// Owns the component-name draft and embedded-library palette. Actions are values;
// the Studio commits other active edits before executing a structural action.
class ComponentPanel final {
   public:
    std::optional<ComponentAction> Draw(const graph::Document& document,
                                        std::span<const graph::NodeId> selection,
                                        const std::map<std::string, std::string>& text);

   private:
    std::array<char, 257> name_{};
};
editor::EditResult ExecuteComponentAction(const ComponentAction& action,
                                          const editor::Snapshot& snapshot,
                                          const graph::Registry& registry,
                                          std::span<const graph::NodeId> selection,
                                          graph::NodeId fresh_id, editor::Position insertion);
}  // namespace rhythm::studio
