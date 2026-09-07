#include <google/protobuf/unknown_field_set.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "graph.pb.h"
#include "rhythm/graph/compiler.h"
#include "rhythm/graph/components.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::ComponentDefinition definition;
    definition.type_ = "component.test.persisted";
    definition.nodes_ = {registry.MakeNode(1, "scalar.constant"),
                         registry.MakeNode(2, "texture.gradient")};
    definition.output_ = 2;
    definition.signals_ = {{"tone", 1}};
    definition.bindings_ = {{2, "amount", "tone"}};
    definition.inputs_ = {{"amount", 2, "amount"}};
    definition.parameters_ = {{"level", 1, "value", "Color"}};
    definition.parameters_[0].minimum_ = 0;
    definition.parameters_[0].maximum_ = 1;
    graph::Document document;
    document.id_ = "components.persisted";
    document.components_ = {definition};
    document.nodes_ = {registry.MakeNode(10, definition.type_, document.components_),
                       registry.MakeNode(20, "output.texture")};
    document.nodes_[0].properties_["level"] = 0.7;
    document.output_ = 20;
    document.edges_ = {{1, 10, 20, "source"}};
    editor::Snapshot snapshot;
    snapshot.document_ = project::DecodeGraph(project::EncodeGraph(document));
    snapshot.positions_ = {{10, {25, 30}}, {20, {425, 30}}};
    snapshot.component_positions_[definition.type_] = {{1, {20, 50}}, {2, {320, 75}}};
    const auto directory = std::filesystem::path(
            "component-layout-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    project::Save(directory, snapshot);
    const auto loaded = project::Load(directory);
    std::string saved_revision;
    {
        std::ifstream marker(directory / "CURRENT");
        marker >> saved_revision;
    }
    Require(loaded.warnings_.empty() && loaded.snapshot_.positions_ == snapshot.positions_ &&
                    loaded.snapshot_.component_positions_ == snapshot.component_positions_,
            "component canvas layout survives complete project save/load");
    auto bad_layout = snapshot;
    bad_layout.component_positions_[definition.type_][99] = {0, 0};
    bool layout_rejected = false;
    try {
        project::Save(directory, bad_layout);
    } catch (const std::invalid_argument&) {
        layout_rejected = true;
    }
    std::string retained_revision;
    {
        std::ifstream marker(directory / "CURRENT");
        marker >> retained_revision;
    }
    Require(layout_rejected && !saved_revision.empty() && retained_revision == saved_revision,
            "invalid component layout cannot replace the saved revision");
    schema::GraphProject message;
    Require(message.ParseFromString(project::EncodeGraph(document)) &&
                    message.schema_version() == 4,
            "component authoring schema");
    auto& parameter = *message.mutable_components(0)->mutable_parameters(0);
    parameter.GetReflection()->MutableUnknownFields(&parameter)->AddVarint(100, 91);
    const auto restored = project::DecodeGraph(message.SerializeAsString());
    Require(restored.components_[0].parameters_[0].minimum_ == 0 &&
                    restored.components_[0].parameters_[0].maximum_ == 1,
            "curated range persistence");
    Require(restored.components_.size() == 1 &&
                    restored.components_[0].signals_[0].name_ == "tone" &&
                    restored.components_[0].parameters_[0].group_ == "Color",
            "component body and public metadata roundtrip");
    Require(message.ParseFromString(project::EncodeGraph(restored)), "component re-encode");
    const auto& retained = message.components(0).parameters(0);
    Require(retained.GetReflection()->GetUnknownFields(retained).field_count() == 1,
            "component public metadata retains extensions");
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(restored, registry));
    const auto runtime = project::DecodeProgram(project::EncodeProgram(plan));
    Require(runtime.instructions_.size() == 3, "component publishes flat portable program");
    bool found_value = false;
    for (const auto& instruction : runtime.instructions_) {
        Require(instruction.operation_ != graph::Operation::kComponent,
                "no component backend dependency");
        if (instruction.operation_ == graph::Operation::kConstant)
            found_value = graph::Scalar(instruction.node_, "value", 0) == 0.7;
    }
    Require(found_value, "public parameter survives publication");
    message.set_schema_version(3);
    bool rejected = false;
    try {
        project::DecodeGraph(message.SerializeAsString());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "old schema cannot silently ignore components");
    auto invalid = restored;
    invalid.components_.push_back(invalid.components_[0]);
    rejected = false;
    try {
        project::EncodeGraph(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "duplicate component library key rejects");
    std::cout << "component persistence: schema, nested records, unknown metadata, public "
                 "controls, flat publication and duplicate/version rejection passed\n";
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
