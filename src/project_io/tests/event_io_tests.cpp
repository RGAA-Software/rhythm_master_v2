#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "graph.pb.h"
#include "rhythm/editor/commands.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "downgraded event format accepted");
}
double Envelope(const rhythm::graph::Document& document) {
    using namespace rhythm;
    const auto package = project::DecodePackage(project::EncodePackage(document, "Event work"));
    const auto& plan = package.program_;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    double value = -1;
    for (const auto seconds : {0.0, 0.49, 0.5, 0.505}) {
        renderer.BeginFrame();
        const auto frame = runtime.Evaluate(plan, {seconds, 0, {32, 32}}, renderer);
        Check(renderer.IsValid(frame.final_) && !frame.rejected_event_total_,
              "published event graph failed runtime evaluation");
        for (std::size_t index = 0; index < plan.instructions_.size(); ++index)
            if (plan.instructions_[index].operation_ == graph::Operation::kEventEnvelope)
                value = frame.outputs_[index].scalar_;
        renderer.EndFrame();
    }
    return value;
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 2, "event IO test workspace required");
        graph::Registry registry;
        editor::Snapshot source;
        source.document_.id_ = "event-template";
        source.document_.beat_grid_ = parameters::BeatSettings{};
        source.document_.nodes_ = {
                registry.MakeNode(1, "event.beat"), registry.MakeNode(2, "event.envelope"),
                registry.MakeNode(3, "texture.gradient"), registry.MakeNode(4, "output.texture")};
        source.document_.edges_ = {{1, 1, 2, "events"}, {2, 2, 3, "amount"}, {3, 3, 4, "source"}};
        source.document_.output_ = 4;
        source.positions_ = {{1, {0, 0}}, {2, {300, 0}}, {3, {600, 0}}, {4, {900, 0}}};
        const std::array<graph::NodeId, 2> selection{2, 3};
        source = std::get<editor::Snapshot>(
                editor::MakeComponent(source, registry, selection, 5, "Event color"));
        Check(std::abs(Envelope(source.document_) - 0.5) < 1e-9,
              "component event port lost its trigger");
        schema::GraphProject graph_message;
        Check(graph_message.ParseFromString(project::EncodeGraph(source.document_)) &&
                      graph_message.schema_version() == 7,
              "event graph lacks its minimum schema");
        graph_message.set_schema_version(6);
        Reject([&] { project::DecodeGraph(graph_message.SerializeAsString()); });
        // Even an unused component body requires the event-aware authoring schema.
        auto unused = source.document_;
        unused.nodes_ = {registry.MakeNode(9, "texture.gradient"),
                         registry.MakeNode(10, "output.texture")};
        unused.edges_ = {{1, 9, 10, "source"}};
        unused.output_ = 10;
        unused.beat_grid_.reset();
        Check(graph_message.ParseFromString(project::EncodeGraph(unused)) &&
                      graph_message.schema_version() == 7 && !graph_message.has_beat_grid(),
              "embedded event definition lost schema or invented a beat grid");
        Check(project::DecodeGraph(graph_message.SerializeAsString()).components_.size() == 1,
              "event component without a root grid failed to reopen");
        unused.components_.clear();
        Check(graph_message.ParseFromString(project::EncodeGraph(unused)) &&
                      graph_message.schema_version() == 2,
              "ordinary graphs changed their schema");

        editor::Snapshot destination;
        destination.document_ = unused;
        destination.document_.id_ = "user-project";
        editor::History history(destination);
        std::vector<graph::NodeId> fresh;
        for (std::size_t index = 0; index < source.document_.nodes_.size(); ++index)
            fresh.push_back(history.ReserveNodeId());
        const auto applied = std::get<editor::Snapshot>(
                editor::InstantiateTemplate(history.Current(), source, fresh));
        Check(history.Apply(applied, history.Current().document_.revision_) && history.Undo() &&
                      history.Current().document_.id_ == "user-project" && history.Redo(),
              "template event application is not one reversible transaction");
        Check(std::abs(Envelope(history.Current().document_) - 0.5) < 1e-9,
              "template remapping lost event source or component input");
        const auto directory =
                std::filesystem::path(argv[1]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        project::Save(directory, history.Current());
        const auto reopened = project::Load(directory);
        Check(reopened.warnings_.empty() &&
                      std::abs(Envelope(reopened.snapshot_.document_) - 0.5) < 1e-9,
              "saved/reopened event work differs from its applied template");
        const auto expanded = std::get<editor::Snapshot>(
                editor::ExpandAllComponents(reopened.snapshot_, registry, 1000));
        Check(std::abs(Envelope(expanded.document_) - 0.5) < 1e-9,
              "unpacking event component lost typed wiring");
        const auto plan =
                std::get<graph::ExecutionPlan>(graph::Compile(expanded.document_, registry));
        const auto program = project::EncodeProgram(plan);
        schema::CompiledProgram program_message;
        Check(program_message.ParseFromString(program) && program_message.abi_version() == 5,
              "event runtime package lacks its minimum ABI");
        project::DecodeProgram(program, 5);
        Reject([&] { project::DecodeProgram(program, 4); });
        program_message.set_abi_version(4);
        Reject([&] { project::DecodeProgram(program_message.SerializeAsString()); });
        // The event source itself now belongs to the component. Its track must
        // follow template IDs, packing/unpacking, deletion and history as data.
        editor::Snapshot recorded;
        recorded.document_.id_ = "recorded-template";
        recorded.document_.nodes_ = {
                registry.MakeNode(1, "event.input"), registry.MakeNode(2, "event.envelope"),
                registry.MakeNode(3, "texture.gradient"), registry.MakeNode(4, "output.texture")};
        const parameters::EventTrack track({{42, 0.5, parameters::EventKind::kPulse, 1},
                                            {43, 1.5, parameters::EventKind::kReset, 1}});
        recorded.document_.nodes_[0].properties_["actions"] = track;
        recorded.document_.edges_ = {{1, 1, 2, "events"}, {2, 2, 3, "amount"}, {3, 3, 4, "source"}};
        recorded.document_.output_ = 4;
        recorded.positions_ = {{1, {0, 0}}, {2, {300, 0}}, {3, {600, 0}}, {4, {900, 0}}};
        Check(std::abs(Envelope(recorded.document_) - 0.5) < 1e-9, "track package replay failed");
        auto malformed = project::EncodeGraph(recorded.document_);
        Check(graph_message.ParseFromString(malformed), "track schema parse");
        auto& action = *(*graph_message.mutable_nodes(0)->mutable_properties())["actions"]
                                .mutable_event_track()
                                ->mutable_actions(0);
        action.set_kind(schema::EventTrack::KIND_UNSPECIFIED);
        Reject([&] { project::DecodeGraph(graph_message.SerializeAsString()); });
        action.set_kind(schema::EventTrack::KIND_GATE);
        action.set_value(0.5);
        Reject([&] { project::DecodeGraph(graph_message.SerializeAsString()); });
        // Unknown fields belong to a stable action, not a recycled row ID.
        action.set_kind(schema::EventTrack::KIND_PULSE);
        action.set_value(1);
        action.GetReflection()->MutableUnknownFields(&action)->AddVarint(99, 123);
        auto extended = project::DecodeGraph(graph_message.SerializeAsString());
        extended.nodes_[0].properties_["actions"] =
                parameters::EventTrack({{42, 0.75, parameters::EventKind::kPulse, 1}}, 43);
        schema::GraphProject rewritten;
        Check(rewritten.ParseFromString(project::EncodeGraph(extended)), "extended track parse");
        const auto& moved = rewritten.nodes(0).properties().at("actions").event_track().actions(0);
        Check(moved.id() == 42 && moved.seconds() == 0.75 &&
                      moved.GetReflection()->GetUnknownFields(moved).field_count() == 1,
              "retiming lost an action extension");
        extended.nodes_[0].properties_["actions"] = parameters::EventTrack({}, 43);
        auto deleted_reopened = project::DecodeGraph(project::EncodeGraph(extended));
        const auto retired = std::get<parameters::EventTrack>(
                deleted_reopened.nodes_[0].properties_.at("actions"));
        Check(retired.Events().empty() && retired.LastId() == 43,
              "deleted allocation watermark lost on reopen");
        parameters::EventRecorder replacement;
        replacement.Begin(retired, {0, 1, parameters::EventOrigin::kManual}, 1, 0);
        Check(replacement.Capture({0.5, {0, 1, parameters::EventOrigin::kManual}, 1, 1}) ==
                      parameters::RecordingAdmission::kRecorded,
              "new action after deletion rejected");
        extended.nodes_[0].properties_["actions"] = replacement.Finish();
        Check(rewritten.ParseFromString(project::EncodeGraph(extended)), "new action parse");
        const auto& fresh_action =
                rewritten.nodes(0).properties().at("actions").event_track().actions(0);
        Check(fresh_action.id() == 44 &&
                      fresh_action.GetReflection()->GetUnknownFields(fresh_action).empty(),
              "new action inherited a retired action's extension");
        const std::array<graph::NodeId, 3> recorded_selection{1, 2, 3};
        recorded = std::get<editor::Snapshot>(
                editor::MakeComponent(recorded, registry, recorded_selection, 5, "Recorded color"));
        editor::History recording_history(destination);
        fresh.clear();
        for (std::size_t index = 0; index < recorded.document_.nodes_.size(); ++index)
            fresh.push_back(recording_history.ReserveNodeId());
        auto recorded_applied = std::get<editor::Snapshot>(
                editor::InstantiateTemplate(recording_history.Current(), recorded, fresh));
        Check(recording_history.Apply(recorded_applied,
                                      recording_history.Current().document_.revision_),
              "recorded template transaction");
        project::Save(directory / "recorded", recording_history.Current());
        const auto recorded_reopened = project::Load(directory / "recorded");
        auto unpacked = std::get<editor::Snapshot>(
                editor::ExpandAllComponents(recorded_reopened.snapshot_, registry, 2000));
        Check(std::abs(Envelope(unpacked.document_) - 0.5) < 1e-9,
              "recorded component/template/save/unpack/package output differs");
        bool found_track = false;
        for (const auto& node : unpacked.document_.nodes_)
            if (node.type_ == "event.input") {
                found_track = true;
                Check(node.id_ != 1 && std::get<parameters::EventTrack>(
                                               node.properties_.at("actions")) == track,
                      "remapped owner lost action identity or content");
            }
        Check(found_track && recording_history.Undo() && recording_history.Redo(),
              "recorded template undo/redo failed");
        Check(std::abs(Envelope(recording_history.Current().document_) - 0.5) < 1e-9,
              "recorded history replay changed output");
        std::cout << "Event component/template remapping, history, save/reopen, package replay "
                     "and minimum schema/ABI passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
