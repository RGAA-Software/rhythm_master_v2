#pragma once

#include "rhythm/editor/history.h"

namespace rhythm::editor {
using EditResult = std::variant<Snapshot, graph::Diagnostic>;
// Pure value commands: errors leave the source snapshot untouched. History
// applies successful values using the caller's expected revision.
EditResult AddNode(const Snapshot& snapshot, const graph::Registry& registry, std::string_view type,
                   Position position, graph::NodeId id);
EditResult Connect(const Snapshot& snapshot, const graph::Registry& registry, graph::NodeId from,
                   graph::NodeId to, std::string_view input);
EditResult DefineSignal(const Snapshot& snapshot, const graph::Registry& registry,
                        std::string_view name, graph::NodeId source);
EditResult BindInput(const Snapshot& snapshot, const graph::Registry& registry, graph::NodeId node,
                     std::string_view input, std::string_view signal);
Snapshot RemoveSignal(const Snapshot& snapshot, std::string_view name);
Snapshot UnbindInput(const Snapshot& snapshot, graph::NodeId node, std::string_view input);
EditResult MakeComponent(const Snapshot& snapshot, const graph::Registry& registry,
                         std::span<const graph::NodeId> selection, graph::NodeId instance,
                         std::string_view title);
EditResult ExpandAllComponents(const Snapshot& snapshot, const graph::Registry& registry,
                               graph::NodeId first_fresh_id);
// Clones the entire referenced component closure for one instance. Other
// instances keep their existing embedded definitions and parameter values.
EditResult DetachComponent(const Snapshot& snapshot, const graph::Registry& registry,
                           graph::NodeId instance, std::string_view unique_type);
// Replaces content as one undoable edit, preserving project identity. Fresh node
// IDs come from History's allocator, never from a template's reusable local IDs.
EditResult InstantiateTemplate(const Snapshot& snapshot, const Snapshot& content,
                               std::span<const graph::NodeId> node_ids);
}  // namespace rhythm::editor
