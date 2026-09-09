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
        std::cout << "Event component/template remapping, history, save/reopen, package replay "
                     "and minimum schema/ABI passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
