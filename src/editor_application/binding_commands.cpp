#include <algorithm>

#include "rhythm/editor/commands.h"
#include "rhythm/graph/bindings.h"

namespace rhythm::editor {
Snapshot UnbindInput(const Snapshot& snapshot, graph::NodeId node, std::string_view input) {
    auto next = snapshot;
    std::erase_if(next.document_.bindings_, [&](const auto& binding) {
        return binding.node_ == node && binding.input_ == input;
    });
    return next;
}
Snapshot RemoveSignal(const Snapshot& snapshot, std::string_view name) {
    auto next = snapshot;
    std::erase_if(next.document_.signals_,
                  [&](const auto& signal) { return signal.name_ == name; });
    std::erase_if(next.document_.bindings_,
                  [&](const auto& binding) { return binding.signal_ == name; });
    return next;
}
EditResult BindInput(const Snapshot& snapshot, const graph::Registry& registry, graph::NodeId node,
                     std::string_view input, std::string_view signal) {
    const auto found =
            std::find_if(snapshot.document_.signals_.begin(), snapshot.document_.signals_.end(),
                         [&](const auto& entry) { return entry.name_ == signal; });
    if (found == snapshot.document_.signals_.end())
        return graph::Diagnostic{"graph.binding_missing", node, std::string(signal)};
    const auto connected = Connect(snapshot, registry, found->source_, node, input);
    if (std::holds_alternative<graph::Diagnostic>(connected))
        return std::get<graph::Diagnostic>(connected);
    auto next = UnbindInput(snapshot, node, input);
    std::erase_if(next.document_.edges_,
                  [&](const auto& edge) { return edge.to_ == node && edge.input_ == input; });
    if (next.document_.bindings_.size() >= 4096 ||
        next.document_.bindings_.size() + next.document_.edges_.size() >= 40000)
        return graph::Diagnostic{"graph.limit"};
    next.document_.bindings_.push_back({node, std::string(input), std::string(signal)});
    return next;
}
EditResult DefineSignal(const Snapshot& snapshot, const graph::Registry& registry,
                        std::string_view name, graph::NodeId source) {
    if (!graph::ValidSignalName(name)) return graph::Diagnostic{"graph.binding_name", source};
    if (std::none_of(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                     [&](const auto& node) { return node.id_ == source; }))
        return graph::Diagnostic{"graph.missing_node", source};
    auto next = snapshot;
    const auto found = std::find_if(next.document_.signals_.begin(), next.document_.signals_.end(),
                                    [&](const auto& entry) { return entry.name_ == name; });
    if (found != next.document_.signals_.end()) {
        found->source_ = source;
    } else {
        if (next.document_.signals_.size() >= 256) return graph::Diagnostic{"graph.limit"};
        next.document_.signals_.push_back({std::string(name), source});
    }
    for (const auto& binding : next.document_.bindings_) {
        if (binding.signal_ != name) continue;
        const auto validation = Connect(next, registry, source, binding.node_, binding.input_);
        if (std::holds_alternative<graph::Diagnostic>(validation))
            return std::get<graph::Diagnostic>(validation);
    }
    return next;
}
}  // namespace rhythm::editor
