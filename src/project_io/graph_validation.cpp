#include "graph_validation.h"

#include <cmath>
#include <set>
#include <stdexcept>

#include "rhythm/graph/bindings.h"
#include "rhythm/graph/controls.h"

namespace rhythm::project::detail {
void ValidateGraph(const graph::Document& document) {
    (void)graph::DescribeControls(document);
    if (document.components_.size() > 256) throw std::invalid_argument("project.component_limits");
    std::set<std::string> component_types;
    std::size_t stored_nodes = document.nodes_.size();
    std::size_t stored_edges = document.edges_.size() + document.bindings_.size();
    for (const auto& definition : document.components_) {
        stored_nodes += definition.nodes_.size();
        stored_edges += definition.edges_.size() + definition.bindings_.size();
        if (stored_nodes > 10000 || stored_edges > 40000 || definition.version_ != 1 ||
            !definition.type_.starts_with("component.") || definition.type_.size() <= 10 ||
            definition.type_.size() > 256 || definition.title_.size() > 256 ||
            !component_types.insert(definition.type_).second || definition.inputs_.size() > 128 ||
            definition.parameters_.size() > 128)
            throw std::invalid_argument("project.component_limits");
        graph::Document body;
        body.id_ = definition.type_;
        body.nodes_ = definition.nodes_;
        body.edges_ = definition.edges_;
        body.output_ = definition.output_;
        body.signals_ = definition.signals_;
        body.bindings_ = definition.bindings_;
        ValidateGraph(body);
        std::set<graph::NodeId> local_nodes;
        for (const auto& node : definition.nodes_) local_nodes.insert(node.id_);
        std::set<std::string> keys;
        std::set<std::pair<graph::NodeId, std::string>> targets;
        for (const auto& port : definition.inputs_)
            if (!graph::ValidSignalName(port.key_) || !keys.insert(port.key_).second ||
                !local_nodes.contains(port.node_) || port.input_.empty() ||
                port.input_.size() > 128 || !targets.emplace(port.node_, port.input_).second)
                throw std::invalid_argument("project.component_input");
        keys.clear();
        targets.clear();
        for (const auto& parameter : definition.parameters_)
            if (!graph::ValidSignalName(parameter.key_) || !keys.insert(parameter.key_).second ||
                !local_nodes.contains(parameter.node_) || parameter.property_.empty() ||
                parameter.property_.size() > 128 || parameter.group_.size() > 128 ||
                (parameter.minimum_ && !std::isfinite(*parameter.minimum_)) ||
                (parameter.maximum_ && !std::isfinite(*parameter.maximum_)) ||
                (parameter.minimum_ && parameter.maximum_ &&
                 *parameter.minimum_ > *parameter.maximum_) ||
                !targets.emplace(parameter.node_, parameter.property_).second)
                throw std::invalid_argument("project.component_parameter");
    }
    if (!graph::ValidCanvas(document.canvas_)) throw std::invalid_argument("project.canvas");
    if (document.id_.empty() || document.id_.size() > 256 || document.nodes_.size() > 10000 ||
        document.edges_.size() > 40000)
        throw std::invalid_argument("project.graph_limits");
    std::set<graph::NodeId> ids;
    for (const auto& node : document.nodes_) {
        if (!node.id_ || !ids.insert(node.id_).second || node.type_.empty() ||
            node.type_.size() > 256 || node.properties_.size() > 128 || !node.version_)
            throw std::invalid_argument("project.node");
        for (const auto& [key, value] : node.properties_) {
            if (key.empty() || key.size() > 128) throw std::invalid_argument("project.property");
            if (std::holds_alternative<double>(value) && !std::isfinite(std::get<double>(value)))
                throw std::invalid_argument("project.nonfinite");
            if (std::holds_alternative<assets::AssetId>(value)) {
                const auto& asset = std::get<assets::AssetId>(value);
                if (!asset.sha256_.empty() && !assets::ValidId(asset))
                    throw std::invalid_argument("project.asset_reference");
            }
            if (std::holds_alternative<graph::Color>(value)) {
                const auto color = std::get<graph::Color>(value);
                for (const auto channel : {color.r_, color.g_, color.b_, color.a_})
                    if (!std::isfinite(channel)) throw std::invalid_argument("project.nonfinite");
            }
        }
    }
    if (!ids.contains(document.output_)) throw std::invalid_argument("project.output");
    std::set<std::uint64_t> edges;
    std::set<std::pair<graph::NodeId, std::string>> ports;
    const auto resolved = graph::ResolveEdges(document);
    if (std::holds_alternative<std::vector<graph::Diagnostic>>(resolved))
        throw std::invalid_argument("project.binding");
    for (const auto& edge : std::get<std::vector<graph::Edge>>(resolved))
        if (!edge.id_ || !edges.insert(edge.id_).second || !ids.contains(edge.from_) ||
            !ids.contains(edge.to_) || edge.input_.empty() || edge.input_.size() > 128 ||
            !ports.emplace(edge.to_, edge.input_).second)
            throw std::invalid_argument("project.edge");
}
}  // namespace rhythm::project::detail
