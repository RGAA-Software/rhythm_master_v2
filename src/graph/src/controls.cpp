#include "rhythm/graph/controls.h"

#include <set>
#include <stdexcept>

namespace rhythm::graph {
void PruneControls(Document& document) {
    std::set<NodeId> ids;
    for (const auto& node : document.nodes_)
        if (node.type_ == "control.scalar") ids.insert(node.id_);
    std::erase_if(document.control_titles_,
                  [&](const auto& item) { return !ids.contains(item.first); });
    for (auto& snapshot : document.control_snapshots_)
        std::erase_if(snapshot.values_,
                      [&](const auto& item) { return !ids.contains(item.first); });
    if (ids.empty()) document.control_snapshots_.clear();
}
parameters::ControlBank DescribeControls(const Document& document) {
    if (document.control_titles_.size() > parameters::ControlBank::kMaximumControls)
        throw std::length_error("control.budget");
    std::vector<parameters::ControlDefinition> definitions;
    std::set<NodeId> ids;
    for (const auto& node : document.nodes_) {
        if (node.type_ != "control.scalar") continue;
        if (!ids.insert(node.id_).second) throw std::invalid_argument("control.definition");
        for (const auto key : {"value", "control_minimum", "control_maximum"}) {
            const auto found = node.properties_.find(key);
            if (found != node.properties_.end() && !std::holds_alternative<double>(found->second))
                throw std::invalid_argument("control.definition");
        }
        const auto title = document.control_titles_.find(node.id_);
        definitions.push_back({node.id_,
                               title == document.control_titles_.end()
                                       ? "Control " + std::to_string(node.id_)
                                       : title->second,
                               Scalar(node, "control_minimum", 0),
                               Scalar(node, "control_maximum", 1), Scalar(node, "value", 0)});
    }
    for (const auto& [id, title] : document.control_titles_)
        if (!ids.contains(id)) throw std::invalid_argument("control.definition");
    return parameters::ControlBank(std::move(definitions), document.control_snapshots_);
}
parameters::ControlBank SelectControls(const parameters::ControlBank& bank,
                                       std::span<const Instruction> instructions) {
    std::set<NodeId> ids;
    for (const auto& instruction : instructions)
        if (instruction.operation_ == Operation::kControlScalar) ids.insert(instruction.node_.id_);
    std::vector<parameters::ControlDefinition> definitions;
    for (const auto& control : bank.Definitions())
        if (ids.contains(control.id_)) definitions.push_back(control);
    if (definitions.empty()) return {};
    std::vector<parameters::ControlSnapshot> snapshots;
    for (auto snapshot : bank.Snapshots()) {
        std::erase_if(snapshot.values_,
                      [&](const auto& value) { return !ids.contains(value.first); });
        snapshots.push_back(std::move(snapshot));
    }
    return parameters::ControlBank(std::move(definitions), std::move(snapshots));
}
}  // namespace rhythm::graph
