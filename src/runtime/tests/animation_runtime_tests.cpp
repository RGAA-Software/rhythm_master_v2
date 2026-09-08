#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"
#include "scene_pass.h"

namespace {
using namespace rhythm;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
const runtime::NodeOutput& Find(const runtime::FrameResult& frame, graph::NodeId id) {
    const auto found = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                    [id](const auto& value) { return value.node_ == id; });
    if (found == frame.outputs_.end()) throw std::runtime_error("missing animation output");
    return *found;
}
void Run() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "animation.runtime";
    const assets::AssetId asset{std::string(64, 'a')};
    document.nodes_ = {
            registry.MakeNode(1, "geometry.glb"),     registry.MakeNode(2, "audio.feature"),
            registry.MakeNode(3, "geometry.animate"), registry.MakeNode(4, "scene.instance"),
            registry.MakeNode(5, "scene.instance"),   registry.MakeNode(6, "scene.merge"),
            registry.MakeNode(7, "scene.render"),     registry.MakeNode(8, "output.texture")};
    document.nodes_[0].properties_["asset"] = asset;
    document.nodes_[2].properties_["animation_second"] = 1.0;
    document.edges_ = {{1, 1, 3, "geometry"}, {2, 2, 3, "animation_blend"},
                       {3, 3, 4, "geometry"}, {4, 1, 5, "geometry"},
                       {5, 4, 6, "a"},        {6, 5, 6, "b"},
                       {7, 6, 7, "scene"},    {8, 7, 8, "source"}};
    document.output_ = 8;
    auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto model = scene::Cube();
    model.images_.push_back({1, 1, {128, 128, 255, 255}});
    model.materials_[0].textures_.images_[0] = 0;
    model.rest_pose_ = {{1, {}}};
    model.animations_.emplace_back(
            "x", std::vector<scene::AnimationTrack>{{1,
                                                     scene::AnimationProperty::kTranslation,
                                                     scene::AnimationInterpolation::kLinear,
                                                     3,
                                                     {0, 2},
                                                     {{0, 0, 0, 0}, {4, 0, 0, 0}}}});
    model.animations_.emplace_back(
            "y", std::vector<scene::AnimationTrack>{{1,
                                                     scene::AnimationProperty::kTranslation,
                                                     scene::AnimationInterpolation::kLinear,
                                                     3,
                                                     {0, 2},
                                                     {{0, 0, 0, 0}, {0, 8, 0, 0}}}});
    scene::Resources resources;
    resources.models_.push_back(scene::DescribeModel(asset, std::move(model)));
    runtime::FrameContext context{0.5, 0, {64, 64}};
    context.resources_ = std::make_shared<const scene::Resources>(std::move(resources));
    context.external_.audio_.emplace();
    context.external_.audio_->valid_ = true;
    context.external_.audio_->sample_rate_ = 48000;
    context.external_.audio_->generation_ = 1;
    context.external_.audio_->loudness_ = 0.25f;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    const auto evaluate = [&] {
        renderer.BeginFrame();
        auto frame = runtime.Evaluate(plan, context, renderer);
        renderer.EndFrame();
        return frame;
    };
    auto frame = evaluate();
    const auto original = Find(frame, 1).geometry_;
    const auto old = Find(frame, 3).geometry_;
    Require(old->model_ == original->model_ && old->upload_id_ == original->id_ &&
                    old->pose_->at(1).translation_ == scene::Vector3{0.75, 0.5, 0},
            "music blends two clips without copying the base model");
    Require(evaluate().evaluated_ == 0, "paused unchanged transport retains all snapshots");
    Require(renderer.Stats().live_meshes_ == 1 && renderer.Stats().live_textures_ == 3,
            "animated and static copies share mesh and image uploads");
    {
        runtime::detail::ScenePass inspection;
        const auto first = inspection.Build(*Find(frame, 6).scene_, {}, {64, 64}, renderer);
        Require(first.draws_.size() == 2 && first.draws_[0].mesh_ == first.draws_[1].mesh_ &&
                        first.draws_[0].model_[12] == 0.75f && first.draws_[0].model_[13] == 0.5f &&
                        first.draws_[1].model_[12] == 0,
                "independent posed hierarchy reaches draw records");
        const auto mesh = first.draws_[0].mesh_;
        const auto image = first.draws_[0].textures_.slots_[0];
        runtime::Viewers viewers;
        for (int i = 0; i < 20; ++i) {
            context.seconds_ = static_cast<double>(i) / 10;
            frame = evaluate();
            const auto list = inspection.Build(*Find(frame, 6).scene_, {}, {64, 64}, renderer);
            Require(list.draws_[0].mesh_ == mesh && list.draws_[0].textures_.slots_[0] == image &&
                            std::abs(list.draws_[0].model_[12] - float(context.seconds_ * 1.5)) <
                                    1e-5,
                    "time updates pose without static resource reupload");
            renderer.BeginFrame();
            viewers.BeginFrame(context.seconds_, true, 0);
            const std::array<graph::NodeId, 1> demand{3};
            viewers.Capture(frame, demand, renderer);
            renderer.EndFrame();
            Require(viewers.Outputs().size() == 1, "animated geometry has a live node preview");
        }
        Require(old->pose_->at(1).translation_ == scene::Vector3{0.75, 0.5, 0},
                "previous pose snapshots remain immutable");
        context.seconds_ = 0.5;
        context.external_.audio_->loudness_ = 1;
        frame = evaluate();
        Require(Find(frame, 3).geometry_->pose_->at(1).translation_ == scene::Vector3{0, 2, 0},
                "audio updates blend while transport stays fixed");
    }
    runtime.Reset();
    Require(renderer.Stats().live_meshes_ == 0 && renderer.Stats().live_textures_ == 0,
            "animation caches release on reset");
    document.nodes_.push_back(registry.MakeNode(9, "scalar.constant"));
    document.nodes_.back().properties_["value"] = 0.5;
    document.edges_.push_back({9, 9, 3, "time"});
    plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    (void)evaluate();
    context.seconds_ = 100;
    Require(evaluate().evaluated_ == 0,
            "explicit time input disconnects automatic clock invalidation");
    auto oversized = plan;
    for (std::uint64_t i = 10; i < 43; ++i)
        oversized.instructions_.push_back({registry.MakeNode(i, "geometry.animate"),
                                           graph::Operation::kGeometryAnimate,
                                           {0, {}, {}}});
    Require(graph::ValidateSceneBudget(oversized).has_value(), "pose snapshot budget rejects");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Animation: clock, music blending, shared resources, pose previews and limits "
                     "passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
