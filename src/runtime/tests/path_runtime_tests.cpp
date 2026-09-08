#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"
#include "scene_ops.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
const runtime::NodeOutput& Find(const runtime::FrameResult& frame, graph::NodeId id) {
    const auto found = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                    [id](const auto& output) { return output.node_ == id; });
    if (found == frame.outputs_.end()) throw std::runtime_error("missing path output");
    return *found;
}
void OrderedPoints() {
    graph::Registry registry;
    graph::Instruction instruction{
            registry.MakeNode(1, "path.from_points"), graph::Operation::kPathFromPoints, {0}};
    instruction.node_.properties_["path_samples"] = 3.0;
    runtime::NodeOutput source;
    runtime::NodeOutput result;
    source.points_ = std::make_shared<const particles::PointCloud>(particles::PointCloud{
            {1, 0, 0}, {2, 0.25f, 0.25f}, {3, 0.5f, 0.5f}, {4, 0.75f, 0.75f}, {5, 1, 1}});
    const auto evaluate = [&] {
        runtime::detail::EvaluatePath(instruction, std::span{&source, 1}, result);
    };
    evaluate();
    Require(result.path_->points_.size() == 3 && result.path_->points_[0].x_ == -2 &&
                    result.path_->points_[1].x_ == 0 && result.path_->points_[2].y_ == -2,
            "point bridge samples in source order and maps canvas Y to world Y");
    source.points_ = std::make_shared<const particles::PointCloud>(
            particles::PointCloud{{1, 0.5f, 0.5f}, {2, 0.5f, 0.5f}, {3, 0.5f, 0.5f}});
    evaluate();
    Require(result.path_->points_.empty(), "coincident points publish an empty path");
    source.points_ = std::make_shared<const particles::PointCloud>();
    evaluate();
    Require(scene::Tube(*result.path_, 0.1).meshes_.empty(),
            "empty emitters produce empty geometry without failing the graph");
}
void Run() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "path.runtime";
    document.nodes_ = {
            registry.MakeNode(1, "audio.feature"),  registry.MakeNode(2, "path.helix"),
            registry.MakeNode(3, "path.resample"),  registry.MakeNode(4, "geometry.tube"),
            registry.MakeNode(5, "scene.instance"), registry.MakeNode(6, "scene.render"),
            registry.MakeNode(7, "output.texture"), registry.MakeNode(8, "core.time")};
    document.nodes_[1].properties_["path_samples"] = 48.0;
    document.nodes_[2].properties_["path_samples"] = 96.0;
    document.edges_ = {{1, 1, 2, "path_radius"}, {2, 2, 3, "path"},  {3, 3, 4, "path"},
                       {4, 4, 5, "geometry"},    {5, 5, 6, "scene"}, {6, 6, 7, "source"},
                       {7, 8, 2, "path_phase"}};
    document.output_ = 7;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::Viewers viewers;
    runtime::FrameContext context{0, 0, {64, 64}};
    context.external_.audio_.emplace();
    context.external_.audio_->valid_ = true;
    context.external_.audio_->generation_ = 1;
    context.external_.audio_->sample_rate_ = 48000;
    context.external_.audio_->loudness_ = 0.5f;
    const auto evaluate = [&] {
        renderer.BeginFrame();
        auto frame = runtime.Evaluate(plan, context, renderer);
        renderer.EndFrame();
        return frame;
    };
    auto frame = evaluate();
    const auto old_path = Find(frame, 2).path_;
    const auto old_geometry = Find(frame, 4).geometry_;
    Require(old_path->points_.size() == 48 && Find(frame, 3).path_->points_.size() == 96 &&
                    renderer.Stats().live_meshes_ == 1 && renderer.IsValid(frame.final_),
            "audio to ordered path to tube renders a bounded mesh");
    Require(evaluate().evaluated_ == 0, "unchanged path and mesh reuse immutable snapshots");
    context.external_.audio_->loudness_ = 0.8f;
    frame = evaluate();
    Require(std::abs(Find(frame, 2).path_->points_[0].x_ - 0.8) < 1e-6 &&
                    old_path->points_[0].x_ == 0.5 && Find(frame, 4).geometry_ != old_geometry,
            "music changes mesh while prior path and geometry remain immutable");
    const auto bytes = renderer.Stats().mesh_bytes_;
    for (int index = 1; index <= 20; ++index) {
        context.seconds_ = index * 0.1;
        frame = evaluate();
        renderer.BeginFrame();
        viewers.BeginFrame(context.seconds_, true, 0);
        const std::array<graph::NodeId, 2> demand{2, 4};
        viewers.Capture(frame, demand, renderer);
        renderer.EndFrame();
        Require(viewers.Outputs().size() == 2 && renderer.Stats().live_meshes_ == 3,
                "animated path and geometry previews replace rather than accumulate meshes");
    }
    Require(Find(frame, 2).path_->points_[0].z_ > 0,
            "time-driven phase rotates the path in world space");
    viewers.BeginFrame(context.seconds_, false, 0);
    Require(renderer.Stats().live_meshes_ == 1 && renderer.Stats().mesh_bytes_ == bytes,
            "hiding path previews releases their geometry");
    runtime.Reset();
    Require(renderer.Stats().live_meshes_ == 0 && renderer.Stats().live_textures_ == 0,
            "reset releases path render resources");
    document.edges_[2].from_ = 1;
    Require(std::holds_alternative<std::vector<graph::Diagnostic>>(
                    graph::Compile(document, registry)),
            "scalar cannot masquerade as a path");
    auto oversized = plan;
    for (auto& instruction : oversized.instructions_)
        if (instruction.node_.id_ == 2) instruction.node_.properties_["path_samples"] = 1025.0;
    Require(graph::ValidateSceneBudget(oversized).has_value(),
            "path capacity is checked before allocation");
    oversized = {};
    for (std::uint64_t index = 1; index <= 65; ++index) {
        auto node = registry.MakeNode(index, "path.helix");
        node.properties_["path_samples"] = 1024.0;
        oversized.instructions_.push_back({node, graph::Operation::kPathHelix});
    }
    Require(graph::ValidateSceneBudget(oversized).has_value(),
            "aggregate path snapshots are bounded");
    OrderedPoints();
    std::cout << "Paths: typed graph, music/time, immutable cache, previews, cleanup and budgets "
                 "passed\n";
}
}  // namespace
int main() {
    try {
        Run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
