#include <algorithm>
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
                                    [id](const auto& output) { return output.node_ == id; });
    if (found == frame.outputs_.end()) throw std::runtime_error("missing morph output");
    return *found;
}
void Run() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "morph.runtime";
    const assets::AssetId asset{std::string(64, 'b')};
    document.nodes_ = {
            registry.MakeNode(1, "geometry.glb"),   registry.MakeNode(2, "geometry.animate"),
            registry.MakeNode(3, "geometry.morph"), registry.MakeNode(4, "audio.feature"),
            registry.MakeNode(5, "scene.instance"), registry.MakeNode(6, "scene.instance"),
            registry.MakeNode(7, "scene.merge"),    registry.MakeNode(8, "scene.render"),
            registry.MakeNode(9, "output.texture")};
    document.nodes_[0].properties_["asset"] = asset;
    document.nodes_[2].properties_["morph_weight_2"] = -0.25;
    document.nodes_[2].properties_["morph_weight_4"] = 3.0;
    document.edges_ = {{1, 1, 2, "geometry"}, {2, 2, 3, "geometry"}, {3, 4, 3, "morph_weight_1"},
                       {4, 3, 5, "geometry"}, {5, 1, 6, "geometry"}, {6, 5, 7, "a"},
                       {7, 6, 7, "b"},        {8, 7, 8, "scene"},    {9, 8, 9, "source"}};
    document.output_ = 9;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto model = scene::Cube();
    model.meshes_[0].morphs_.resize(2);
    for (auto& target : model.meshes_[0].morphs_) {
        target.deltas_.resize(model.meshes_[0].vertices_.size());
        for (auto& delta : target.deltas_) delta.position_ = {0.5f, 0, 0};
    }
    model.rest_pose_[1].weights_ = {0.1, 0.2, 0, 0};
    model.animations_.emplace_back(
            "Move", std::vector<scene::AnimationTrack>{{1,
                                                        scene::AnimationProperty::kTranslation,
                                                        scene::AnimationInterpolation::kLinear,
                                                        3,
                                                        {0, 2},
                                                        {{0, 0, 0, 0}, {2, 0, 0, 0}}}});
    scene::Resources resources;
    resources.models_.push_back(scene::DescribeModel(asset, std::move(model)));
    runtime::FrameContext context{0.5, 0, {64, 64}};
    context.resources_ = std::make_shared<const scene::Resources>(std::move(resources));
    context.external_.audio_.emplace();
    context.external_.audio_->valid_ = true;
    context.external_.audio_->sample_rate_ = 48000;
    context.external_.audio_->generation_ = 1;
    context.external_.audio_->loudness_ = 0.75f;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    const auto evaluate = [&] {
        renderer.BeginFrame();
        auto result = runtime.Evaluate(plan, context, renderer);
        renderer.EndFrame();
        return result;
    };
    auto frame = evaluate();
    const auto original = Find(frame, 1).geometry_;
    const auto old = Find(frame, 3).geometry_;
    Require(old->model_ == original->model_ && old->upload_id_ == original->id_ &&
                    old->pose_->at(1).translation_.x_ == 0.5 &&
                    old->pose_->at(1).weights_ == std::array<double, 4>{0.75, -0.25, 0, 0},
            "music weights preserve animation and share immutable model");
    Require(renderer.Stats().live_meshes_ == 1, "morph does not duplicate base mesh");
    Require(evaluate().evaluated_ == 0, "unchanged morph input reuses snapshots");
    {
        runtime::detail::ScenePass inspection;
        auto draws = inspection.Build(*Find(frame, 7).scene_, {}, {64, 64}, renderer);
        Require(draws.draws_[0].morph_weights_ == std::array<float, 4>{0.75f, -0.25f, 0, 0} &&
                        draws.draws_[1].morph_weights_ == std::array<float, 4>{0.1f, 0.2f, 0, 0},
                "draws use per-instance weights or GLB defaults and zero unused slots");
        const auto mesh = draws.draws_[0].mesh_;
        const auto bytes = renderer.Stats().mesh_bytes_;
        for (int i = 0; i < 20; ++i) {
            context.external_.audio_->loudness_ = float(i) / 20;
            frame = evaluate();
            draws = inspection.Build(*Find(frame, 7).scene_, {}, {64, 64}, renderer);
            Require(draws.draws_[0].mesh_ == mesh && renderer.Stats().mesh_bytes_ == bytes &&
                            draws.draws_[0].morph_weights_[0] ==
                                    context.external_.audio_->loudness_,
                    "music updates weights without reallocating morph storage");
        }
        Require(old->pose_->at(1).weights_[0] == 0.75, "old pose remains immutable");
        runtime::Viewers viewers;
        renderer.BeginFrame();
        viewers.BeginFrame(1, true, 0);
        const std::array<graph::NodeId, 1> demand{3};
        viewers.Capture(frame, demand, renderer);
        renderer.EndFrame();
        Require(viewers.Outputs().size() == 1, "morph node preview renders");
    }
    runtime.Reset();
    Require(renderer.Stats().live_meshes_ == 0 && renderer.Stats().mesh_bytes_ == 0,
            "morph storage released on reset");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Morph: music, preserved animation, defaults, shared uploads and previews "
                     "passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
