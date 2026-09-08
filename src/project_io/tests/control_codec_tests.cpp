#include <iostream>
#include <stdexcept>

#include "graph.pb.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

namespace {
void Require(bool value) {
    if (!value) throw std::runtime_error("control.codec_contract");
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "control.codec";
        document.nodes_ = {registry.MakeNode(1, "control.scalar"),
                           registry.MakeNode(2, "texture.gradient"),
                           registry.MakeNode(3, "output.texture")};
        document.edges_ = {{1, 1, 2, "amount"}, {2, 2, 3, "source"}};
        document.output_ = 3;
        document.control_titles_[1] = "Color amount";
        document.control_snapshots_ = {{11, "Quiet", {{1, 0.1}}}, {12, "Bright", {{1, 0.9}}}};
        const auto bytes = project::EncodeGraph(document);
        const auto restored = project::DecodeGraph(bytes);
        Require(restored.control_titles_ == document.control_titles_ &&
                restored.control_snapshots_ == document.control_snapshots_);
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        const auto program = project::DecodeProgram(project::EncodeProgram(plan));
        Require(program.controls_ == plan.controls_ && program.controls_.Snapshot(12).at(1) == 0.9);
        const auto package =
                project::DecodePackage(project::EncodePackage(document, "Controls", {}));
        Require(package.program_.controls_ == plan.controls_);
        schema::GraphProject message;
        Require(message.ParseFromString(bytes));
        auto& snapshot = *message.mutable_controls()->mutable_snapshots(0);
        snapshot.GetReflection()->MutableUnknownFields(&snapshot)->AddVarint(100, 99);
        Require(message.ParseFromString(
                project::EncodeGraph(project::DecodeGraph(message.SerializeAsString()))));
        const auto& preserved = message.controls().snapshots(0);
        Require(preserved.GetReflection()->GetUnknownFields(preserved).field_count() == 1);
        (*message.mutable_controls()->mutable_snapshots(0)->mutable_values())[999] = 0.5;
        Reject([&] { (void)project::DecodeGraph(message.SerializeAsString()); });
        auto invalid = plan;
        invalid.controls_ = parameters::ControlBank({{1, "Wrong range", 0, 2, 0}});
        Reject([&] { (void)project::EncodeProgram(invalid); });
        schema::CompiledProgram bad;
        Require(bad.ParseFromString(project::EncodeProgram(plan)));
        (*bad.mutable_controls()->mutable_titles())[999] = "Dangling";
        Reject([&] { (void)project::DecodeProgram(bad.SerializeAsString()); });
        document.control_cues_ = {{1, "Opening", 0, 11}, {2, "Rise", 2, 12, 2, true}};
        auto cue_bytes = project::EncodeGraph(document);
        Require(message.ParseFromString(cue_bytes) && message.schema_version() == 5);
        auto& cue = *message.mutable_controls()->mutable_cues(0);
        cue.GetReflection()->MutableUnknownFields(&cue)->AddVarint(100, 77);
        Require(message.ParseFromString(
                project::EncodeGraph(project::DecodeGraph(message.SerializeAsString()))));
        Require(message.controls()
                        .cues(0)
                        .GetReflection()
                        ->GetUnknownFields(message.controls().cues(0))
                        .field_count() == 1);
        message.set_schema_version(4);
        Reject([&] { (void)project::DecodeGraph(message.SerializeAsString()); });
        const auto cued = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        auto cued_program = project::EncodeProgram(cued);
        Require(bad.ParseFromString(cued_program) && bad.abi_version() == 3);
        Require(project::DecodeProgram(cued_program, 3).control_sequence_ ==
                cued.control_sequence_);
        Reject([&] { (void)project::DecodeProgram(cued_program, 2); });
        bad.set_abi_version(2);
        Reject([&] { (void)project::DecodeProgram(bad.SerializeAsString()); });
        const auto cue_package =
                project::DecodePackage(project::EncodePackage(document, "Cue work", {}));
        Require(cue_package.program_.control_sequence_ == cued.control_sequence_ &&
                cue_package.program_.control_sequence_->Sample(3).at(1) == 0.5);
        std::cout << "Controls: project, runtime/package metadata, extensions and invalid "
                     "references passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
