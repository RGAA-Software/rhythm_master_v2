#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "instances.runtime";
    document.nodes_ = {registry.MakeNode(1, "geometry.cube"), registry.MakeNode(2, "point.grid"),
                       registry.MakeNode(3, "scene.point_instances"),
                       registry.MakeNode(4, "scene.render"),
                       registry.MakeNode(5, "output.texture")};
    document.nodes_[1].properties_["columns"] = 32.0;
    document.nodes_[1].properties_["rows"] = 32.0;
    document.nodes_[1].properties_["point_size"] = 0.02;
    document.edges_ = {
            {1, 1, 3, "geometry"}, {2, 2, 3, "points"}, {3, 3, 4, "scene"}, {4, 4, 5, "source"}};
    document.output_ = 5;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::FrameContext context;
    context.extent_ = {64, 64};
    const auto evaluate = [&] {
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        renderer.BeginFrame();
        try {
            auto result = runtime.Evaluate(plan, context, renderer);
            renderer.EndFrame();
            return result;
        } catch (...) {
            renderer.EndFrame();
            throw;
        }
    };
    const auto scene_output = [](const runtime::FrameResult& frame) {
        const auto found = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                        [](const auto& output) { return output.node_ == 3; });
        return found->scene_;
    };
    auto frame = evaluate();
    const auto quiet = scene_output(frame);
    Require(quiet->instances_.size() == 1024 && renderer.Stats().live_meshes_ == 1,
            "one geometry backs a thousand bounded instance records");
    const auto& first = quiet->instances_.front();
    const auto source = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                     [](const auto& output) { return output.node_ == 2; });
    Require(source != frame.outputs_.end() && source->points_, "point source available");
    for (std::size_t index = 0; index < quiet->instances_.size(); ++index)
        Require(quiet->instances_[index].origin_ ==
                        scene::InstanceOrigin{3, 0, source->points_->at(index).id_,
                                              source->points_generation_},
                "point instances retain stable source element and generation");
    Require(std::abs(first.transform_.values_[5] - 0.2) < 1e-6 &&
                    std::abs(first.transform_.values_[13] - 0.1) < 1e-6,
            "normalized point size maps to world scale and a floor-centered column");
    Require(evaluate().evaluated_ == 0, "unchanged points and absent music reuse the snapshot");
    context.external_.audio_.emplace();
    context.external_.audio_->valid_ = true;
    context.external_.audio_->sample_rate_ = 48000;
    context.external_.audio_->generation_ = 1;
    context.external_.audio_->mono_bands_.fill(0.5f);
    frame = evaluate();
    const auto active = scene_output(frame);
    Require(active->instances_.front().origin_ == first.origin_,
            "music animation preserves object identity");
    Require(std::abs(active->instances_.front().transform_.values_[5] - 1.4) < 1e-6,
            "audio changes invalidate cached instances and drive height");
    Require(first.transform_.values_[5] < 0.201 &&
                    active->instances_.front().geometry_ == first.geometry_,
            "updates retain immutable old transforms and shared geometry");
    context.external_.audio_->mono_bands_.fill(0);
    context.external_.audio_->mono_bands_.front() = 1;
    frame = evaluate();
    Require(scene_output(frame)->instances_.back().transform_.values_[5] < 0.201,
            "frequency selection does not apply low-band energy to every point");
    context.external_.audio_->valid_ = false;
    context.external_.audio_->mono_bands_.fill(1);
    frame = evaluate();
    Require(scene_output(frame)->instances_.front().transform_.values_[5] < 0.201,
            "invalid audio resets heights rather than retaining stale energy");
    document.nodes_[2].properties_["instance_limit"] = 1023.0;
    bool rejected = false;
    try {
        evaluate();
    } catch (const std::length_error&) {
        rejected = true;
    }
    Require(rejected, "over-capacity points reject instead of silently dropping instances");
    runtime.Reset();
    Require(renderer.Stats().live_meshes_ == 0, "reset releases instance mesh resources");
    std::cout << "Point instances: scale, audio frequency input, caching, budgets and cleanup "
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
