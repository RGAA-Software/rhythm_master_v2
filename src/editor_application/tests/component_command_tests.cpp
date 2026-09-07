#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "rhythm/editor/commands.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    editor::Snapshot original;
    original.document_.id_ = "component.commands";
    original.document_.nodes_ = {
            registry.MakeNode(1, "scalar.constant"), registry.MakeNode(2, "texture.gradient"),
            registry.MakeNode(3, "texture.affine"), registry.MakeNode(4, "output.texture")};
    original.document_.output_ = 4;
    original.document_.edges_ = {{1, 2, 3, "source"}, {2, 3, 4, "source"}};
    original.document_.signals_ = {{"drive", 1}};
    original.document_.bindings_ = {{2, "amount", "drive"}};
    original.positions_ = {{1, {10, 10}}, {2, {200, 10}}, {3, {500, 10}}, {4, {800, 10}}};
    editor::History history(original);
    const auto instance = history.ReserveNodeId();
    const std::vector<graph::NodeId> selection{2, 3};
    const auto wrapped = std::get<editor::Snapshot>(
            editor::MakeComponent(original, registry, selection, instance, "Card"));
    Require(wrapped.document_.nodes_.size() == 3 && wrapped.document_.components_.size() == 1,
            "wrap selection into a single node");
    Require(wrapped.document_.bindings_[0].node_ == instance &&
                    wrapped.document_.components_[0].inputs_[0].node_ == 2,
            "named incoming control reaches public port");
    Require(wrapped.positions_.at(instance) == original.positions_.at(3),
            "component keeps output position");
    Require(std::get<graph::ExecutionPlan>(graph::Compile(wrapped.document_, registry))
                            .instructions_.size() == 4,
            "wrapped graph executes same primitive count");
    const auto duplicate = std::get<editor::Snapshot>(
            editor::AddNode(wrapped, registry, wrapped.document_.components_[0].type_, {900, 400},
                            history.ReserveNodeId()));
    Require(duplicate.document_.nodes_.size() == 4 &&
                    duplicate.document_.nodes_.back().properties_.contains("scale"),
            "reuse component with public defaults");
    Require(history.Apply(wrapped, original.document_.revision_), "component creation commit");
    const auto first_fresh = history.ReserveNodeId();
    const auto expanded = std::get<editor::Snapshot>(
            editor::ExpandAllComponents(history.Current(), registry, first_fresh));
    Require(expanded.document_.components_.empty() && expanded.document_.bindings_.empty(),
            "debug expansion contains explicit ordinary dependencies");
    graph::NodeId largest = 0;
    for (const auto& node : expanded.document_.nodes_) largest = std::max(largest, node.id_);
    Require(largest >= first_fresh &&
                    history.Apply(expanded, history.Current().document_.revision_),
            "fresh expanded identities commit");
    Require(history.Undo() && history.Current().document_.components_.size() == 1 &&
                    history.ReserveNodeId() > largest,
            "undo preserves definition and never reuses expanded IDs");
    const std::vector<graph::NodeId> multiple_outputs{1, 2};
    Require(std::holds_alternative<graph::Diagnostic>(
                    editor::MakeComponent(original, registry, multiple_outputs, 99, "Ambiguous")),
            "multiple observable outputs reject transactionally");
    Require(original.document_.nodes_.size() == 4 && original.document_.components_.empty(),
            "source snapshots unchanged");
    std::cout << "component commands: selection boundaries, named inputs, reuse, public defaults, "
                 "expansion and undo identity passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
