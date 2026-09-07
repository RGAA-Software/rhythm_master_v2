#include "rhythm/graph/bindings.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace rhythm::graph {
bool ValidSignalName(std::string_view name) {
    return !name.empty() && name.size() <= 128 &&
           std::all_of(name.begin(), name.end(), [](unsigned char value) {
               return value >= 32 && value != 127 && value != '#';
           });
}
EdgeResolution ResolveEdges(const Document& document) {
    std::vector<Diagnostic> diagnostics;
    if (document.nodes_.size() > 10000 || document.signals_.size() > 256 ||
        document.bindings_.size() > 4096 ||
        document.edges_.size() + document.bindings_.size() > 40000) {
        return std::vector<Diagnostic>{{"graph.limit"}};
    }
    std::set<NodeId> nodes;
    for (const auto& node : document.nodes_) nodes.insert(node.id_);
    std::map<std::string, NodeId> signals;
    for (const auto& signal : document.signals_) {
        if (!ValidSignalName(signal.name_) || !signals.emplace(signal.name_, signal.source_).second)
            diagnostics.push_back({"graph.binding_name", signal.source_, signal.name_});
        if (!nodes.contains(signal.source_))
            diagnostics.push_back({"graph.missing_node", signal.source_, signal.name_});
    }
    auto edges = document.edges_;
    std::uint64_t next_id = 0;
    for (const auto& edge : edges) next_id = std::max(next_id, edge.id_);
    for (const auto& binding : document.bindings_) {
        const auto found = signals.find(binding.signal_);
        if (found == signals.end()) {
            diagnostics.push_back({"graph.binding_missing", binding.node_, binding.signal_});
            continue;
        }
        if (!nodes.contains(binding.node_)) {
            diagnostics.push_back({"graph.missing_node", binding.node_, binding.input_});
            continue;
        }
        if (binding.input_.empty() || binding.input_.size() > 128) {
            diagnostics.push_back({"graph.missing_port", binding.node_, binding.input_});
            continue;
        }
        if (next_id == std::numeric_limits<std::uint64_t>::max()) {
            diagnostics.push_back({"graph.id_exhausted", binding.node_});
            break;
        }
        edges.push_back({++next_id, found->second, binding.node_, binding.input_});
    }
    return diagnostics.empty() ? EdgeResolution(std::move(edges))
                               : EdgeResolution(std::move(diagnostics));
}
}  // namespace rhythm::graph
