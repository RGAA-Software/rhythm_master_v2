#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"
#include "texture_lifetimes.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
const runtime::NodeOutput& Find(const runtime::FrameResult& frame, graph::NodeId id) {
    const auto found = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                    [id](const auto& output) { return output.node_ == id; });
    if (found == frame.outputs_.end()) throw std::runtime_error("missing GPU sampling output");
    return *found;
}
void Run() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "gpu.sampling.runtime";
    document.nodes_ = {
            registry.MakeNode(1, "core.time"),     registry.MakeNode(2, "texture.gradient"),
            registry.MakeNode(3, "gpu.particles"), registry.MakeNode(4, "gpu.texture_sample"),
            registry.MakeNode(5, "gpu.render"),    registry.MakeNode(6, "output.texture")};
    document.nodes_[2].properties_["particle_capacity"] = 64.0;
    document.edges_ = {{1, 1, 2, "amount"},       {2, 3, 4, "points"}, {3, 2, 4, "source"},
                       {4, 1, 4, "sample_color"}, {5, 4, 5, "points"}, {6, 5, 6, "source"}};
    document.output_ = 6;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    runtime::detail::TextureLifetimes lifetimes;
    lifetimes.Prepare(plan, std::vector<graph::NodeId>{});
    for (std::size_t index = 0; index < plan.instructions_.size(); ++index)
        if (plan.instructions_[index].node_.id_ == 2)
            Require(!lifetimes.Recyclable(index),
                    "lazy sampling retains its texture past the alias node");
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::FrameContext context{.25, 0, {64, 64}};
    context.retained_textures_ = std::vector<graph::NodeId>{};
    const auto evaluate = [&] {
        renderer.BeginFrame();
        const auto frame = runtime.Evaluate(plan, context, renderer);
        renderer.EndFrame();
        return frame;
    };
    const auto initial = evaluate();
    Require(renderer.IsValid(initial.final_) && Find(initial, 4).gpu_sampling_.has_value() &&
                    Find(initial, 4).gpu_sampling_->texture_ == Find(initial, 2).texture_ &&
                    Find(initial, 4).gpu_sampling_->color_amount_ == .25f &&
                    Find(initial, 4).gpu_points_ == Find(initial, 3).gpu_points_ &&
                    !Find(initial, 3).gpu_sampling_ && renderer.Stats().gpu_point_buffers_ == 1,
            "sampled view reuses the source buffer and keeps its own driven attributes");
    context.seconds_ = .5;
    const auto changed = evaluate();
    Require(Find(changed, 4).gpu_sampling_->color_amount_ == .5f &&
                    Find(initial, 4).gpu_sampling_->color_amount_ == .25f,
            "time changes view attributes without mutating prior value snapshots");
    runtime::Viewers viewers;
    renderer.BeginFrame();
    viewers.BeginFrame(.5, true, 0);
    const std::array<graph::NodeId, 1> demand{4};
    viewers.Capture(changed, demand, renderer);
    renderer.EndFrame();
    Require(viewers.Outputs().size() == 1 && renderer.IsValid(viewers.Outputs().front().texture_),
            "sampled GPU output can be inspected inline with the same attribute view");
    viewers.BeginFrame(.5, false, 0);
    runtime.Reset();
    Require(renderer.Stats().gpu_point_buffers_ == 0 && renderer.Stats().live_textures_ == 0,
            "reset releases borrowed-map owners and point buffers");
    document.nodes_.push_back(registry.MakeNode(7, "gpu.texture_sample"));
    document.edges_[4].from_ = 7;
    document.edges_.push_back({7, 4, 7, "points"});
    document.edges_.push_back({8, 2, 7, "source"});
    const auto compiled = graph::Compile(document, registry);
    Require(std::holds_alternative<std::vector<graph::Diagnostic>>(compiled),
            "ambiguous chained lazy sampling is rejected rather than silently dropping a map");
    const auto& errors = std::get<std::vector<graph::Diagnostic>>(compiled);
    Require(std::any_of(errors.begin(), errors.end(),
                        [](const auto& error) { return error.code_ == "graph.gpu_sample_chain"; }),
            "chain failure has an actionable diagnostic");
    std::cout << "GPU texture view: typed graph, driven attributes, source retention, previews and "
                 "cleanup passed\n";
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
