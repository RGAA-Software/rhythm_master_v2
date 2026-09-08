#include <iostream>
#include <stdexcept>

#include "rhythm/graph/controls.h"

int main() {
    using namespace rhythm;
    try {
        const auto require = [](bool value) {
            if (!value) throw std::runtime_error("control.graph_contract");
        };
        graph::Registry registry;
        graph::Document document;
        document.id_ = "controls.test";
        document.nodes_ = {
                registry.MakeNode(1, "control.scalar"), registry.MakeNode(2, "texture.gradient"),
                registry.MakeNode(3, "output.texture"), registry.MakeNode(4, "control.scalar")};
        document.edges_ = {{1, 1, 2, "amount"}, {2, 2, 3, "source"}};
        document.output_ = 3;
        document.control_titles_ = {{1, "Color mix"}, {4, "Disconnected"}};
        document.control_snapshots_ = {{1, "Warm", {{1, 0.8}, {4, 0.2}}}};
        const auto bank = graph::DescribeControls(document);
        require(bank.Definitions().size() == 2);
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        require(plan.instructions_.size() == 3 && plan.controls_.Definitions().size() == 1 &&
                plan.controls_.Snapshot(1) == parameters::ControlValues{{1, 0.8}});
        require(document.control_snapshots_[0].values_.size() == 2);
        const auto valid = document;
        document.control_cues_ = {{1, "Opening", 2, 1, 1}};
        const auto arranged = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        require(arranged.control_sequence_ && arranged.control_sequence_->Sample(3).at(1) == 0.8);
        document.control_snapshots_.clear();
        require(std::holds_alternative<std::vector<graph::Diagnostic>>(
                graph::Compile(document, registry)));
        graph::PruneControls(document);
        require(document.control_cues_.empty());
        document = valid;
        graph::ComponentDefinition nested;
        nested.type_ = "component.nested_macro";
        nested.nodes_ = {registry.MakeNode(1, "control.scalar")};
        nested.output_ = 1;
        const std::vector<graph::ComponentDefinition> components{nested};
        require(!registry.Find(nested.type_, components));
        document.nodes_.pop_back();
        graph::PruneControls(document);
        require(graph::DescribeControls(document).Definitions().size() == 1 &&
                document.control_snapshots_[0].values_.size() == 1);
        document.nodes_.erase(document.nodes_.begin());
        graph::PruneControls(document);
        require(document.control_titles_.empty() && document.control_snapshots_.empty());
        document = valid;
        document.control_titles_[3] = "Not a control";
        require(std::holds_alternative<std::vector<graph::Diagnostic>>(
                graph::Compile(document, registry)));
        document = valid;
        document.nodes_[0].properties_["control_minimum"] = 2.0;
        require(std::holds_alternative<std::vector<graph::Diagnostic>>(
                graph::Compile(document, registry)));
        document = valid;
        document.control_snapshots_[0].values_[1] = 2;
        require(std::holds_alternative<std::vector<graph::Diagnostic>>(
                graph::Compile(document, registry)));
        std::cout << "Control graph: named macros, snapshots, demand filtering and invalid "
                     "references passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
