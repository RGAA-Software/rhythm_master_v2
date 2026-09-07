#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

#include "rhythm/editor/commands.h"
#include "rhythm/graph/components.h"

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
    auto customized = duplicate;
    const auto selected =
            std::find_if(customized.document_.nodes_.begin(), customized.document_.nodes_.end(),
                         [&](const auto& node) { return node.id_ == instance; });
    selected->properties_["scale"] = 0.55;
    const auto unpacked = std::get<editor::Snapshot>(
            editor::UnpackComponent(customized, registry, instance, 100));
    Require(unpacked.document_.nodes_.size() == 5 &&
                    unpacked.document_.components_ == customized.document_.components_ &&
                    unpacked.component_positions_ == customized.component_positions_ &&
                    unpacked.positions_.at(instance) == customized.positions_.at(instance),
            "unpack changes one instance and retains shared definitions, layout and output ID");
    const auto output_node =
            std::find_if(unpacked.document_.nodes_.begin(), unpacked.document_.nodes_.end(),
                         [&](const auto& node) { return node.id_ == instance; });
    Require(output_node->type_ == "texture.affine" &&
                    graph::Scalar(*output_node, "scale", 0) == 0.55 &&
                    unpacked.document_.bindings_.front().node_ == 100,
            "instance parameters and external named inputs transfer into the body");
    Require(std::count_if(unpacked.document_.nodes_.begin(), unpacked.document_.nodes_.end(),
                          [](const auto& node) { return node.type_.starts_with("component."); }) ==
                    1,
            "the second instance remains a component");
    editor::History unpack_history(customized);
    Require(unpack_history.Apply(unpacked, customized.document_.revision_) &&
                    unpack_history.Undo() &&
                    unpack_history.Current().document_.nodes_ == customized.document_.nodes_ &&
                    unpack_history.ReserveNodeId() > 100,
            "unpack is one undo step and reserves generated identities");
    const auto outer = std::get<editor::Snapshot>(editor::MakeComponent(
            customized, registry, std::array<graph::NodeId, 1>{instance}, 200, "Outer"));
    const auto shallow =
            std::get<editor::Snapshot>(editor::UnpackComponent(outer, registry, 200, 300));
    Require(std::count_if(shallow.document_.nodes_.begin(), shallow.document_.nodes_.end(),
                          [](const auto& node) { return node.type_.starts_with("component."); }) ==
                            2 &&
                    std::holds_alternative<graph::ExecutionPlan>(
                            graph::Compile(shallow.document_, registry)),
            "one-level unpack preserves nested components and forwards public inputs");
    {
        editor::Snapshot signals;
        signals.document_.id_ = "unpack.named";
        graph::ComponentDefinition definition;
        definition.type_ = "component.test.named";
        definition.nodes_ = {registry.MakeNode(1, "scalar.constant"),
                             registry.MakeNode(2, "texture.gradient")};
        definition.output_ = 2;
        definition.signals_ = {{"drive", 1}};
        definition.bindings_ = {{2, "amount", "drive"}};
        definition.inputs_ = {{"level", 2, "amount"}};
        signals.document_.components_ = {definition};
        signals.document_.nodes_ = {
                registry.MakeNode(10, definition.type_, signals.document_.components_),
                registry.MakeNode(20, "output.texture"), registry.MakeNode(30, "scalar.constant")};
        signals.document_.output_ = 20;
        signals.document_.edges_ = {{1, 10, 20, "source"}};
        signals.document_.signals_ = {
                {"drive", 30}, {"component.10.signal.1", 30}, {"picture", 10}};
        for (const bool external : {false, true}) {
            signals.document_.bindings_ =
                    external ? std::vector<graph::SignalBinding>{{10, "level", "drive"}}
                             : std::vector<graph::SignalBinding>{};
            const auto result =
                    std::get<editor::Snapshot>(editor::UnpackComponent(signals, registry, 10, 100));
            Require(result.document_.signals_.size() == 4 &&
                            result.document_.bindings_.size() == 1 &&
                            result.document_.bindings_[0].input_ == "amount" &&
                            result.document_.bindings_[0].signal_ ==
                                    (external ? "drive" : "component.10.signal.2") &&
                            std::holds_alternative<graph::ExecutionPlan>(
                                    graph::Compile(result.document_, registry)),
                    "scoped signal names avoid collisions; external inputs replace internal "
                    "defaults");
        }
    }
    Require(std::holds_alternative<graph::Diagnostic>(
                    editor::UnpackComponent(original, registry, 2, 100)),
            "ordinary node unpack is rejected without editing the document");
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
