#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"
#include "scene_pass.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
const runtime::NodeOutput& Find(const runtime::FrameResult& frame, graph::NodeId node) {
    const auto found = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                    [node](const auto& item) { return item.node_ == node; });
    if (found == frame.outputs_.end()) throw std::runtime_error("missing deformation output");
    return *found;
}
void Run() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "deformation.runtime";
    const assets::AssetId asset{std::string(64, 'd')};
    document.nodes_ = {
            registry.MakeNode(1, "geometry.glb"),    registry.MakeNode(2, "audio.feature"),
            registry.MakeNode(3, "geometry.deform"), registry.MakeNode(4, "geometry.deform"),
            registry.MakeNode(5, "scene.instance"),  registry.MakeNode(6, "scene.render"),
            registry.MakeNode(7, "output.texture"),  registry.MakeNode(8, "scene.instance"),
            registry.MakeNode(9, "scene.merge")};
    document.nodes_[0].properties_["asset"] = asset;
    document.nodes_[3].properties_["deform_taper"] = 0.2;
    document.edges_ = {{1, 1, 3, "geometry"}, {2, 2, 3, "deform_twist"},
                       {3, 3, 4, "geometry"}, {4, 4, 5, "geometry"},
                       {5, 5, 9, "a"},        {6, 1, 8, "geometry"},
                       {7, 8, 9, "b"},        {8, 9, 6, "scene"},
                       {9, 6, 7, "source"}};
    document.output_ = 7;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto model = scene::Cube();
    model.images_.push_back({1, 1, {128, 128, 255, 255}});
    model.materials_[0].unlit_ = false;
    model.materials_[0].textures_.images_.fill(0u);
    scene::Resources resources;
    resources.models_.push_back(scene::DescribeModel(asset, std::move(model)));
    runtime::FrameContext context{0, 0, {64, 64}};
    context.resources_ = std::make_shared<const scene::Resources>(std::move(resources));
    context.external_.audio_.emplace();
    context.external_.audio_->valid_ = true;
    context.external_.audio_->generation_ = 1;
    context.external_.audio_->sample_rate_ = 48000;
    context.external_.audio_->loudness_ = 0.5f;
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
    const auto old = Find(frame, 4).geometry_;
    Require(old->deformations_.size() == 2 && old->model_ == original->model_ &&
                    old->upload_id_ == original->id_ &&
                    old->upload_revision_ == original->revision_,
            "modifier chain shares immutable base model, embedded pixels and upload identity");
    Require(renderer.Stats().live_meshes_ == 1 && renderer.Stats().live_textures_ == 3,
            "original and deformed instances share one mesh and embedded image upload");
    Require(evaluate().evaluated_ == 0, "unchanged modifier chain reuses snapshots");
    {
        runtime::detail::ScenePass inspection;
        const auto first = inspection.Build(*Find(frame, 9).scene_, {}, {64, 64}, renderer);
        Require(first.draws_.size() == 2 && first.draws_[0].mesh_ == first.draws_[1].mesh_ &&
                        first.draws_[0].deformations_.size() == 2 &&
                        first.draws_[1].deformations_.empty(),
                "GPU draw records retain per-instance modifiers while sharing mesh storage");
        const auto handle = first.draws_[0].mesh_;
        const auto image = first.draws_[0].textures_.slots_[0];
        runtime::Viewers viewers;
        for (int index = 1; index <= 20; ++index) {
            context.external_.audio_->loudness_ = float(index) / 20;
            frame = evaluate();
            const auto list = inspection.Build(*Find(frame, 9).scene_, {}, {64, 64}, renderer);
            Require(list.draws_[0].mesh_ == handle && list.draws_[0].textures_.slots_[0] == image &&
                            list.draws_[0].deformations_[0].twist_ ==
                                    context.external_.audio_->loudness_,
                    "music only updates vertex uniforms; no mesh or image reupload");
            renderer.BeginFrame();
            viewers.BeginFrame(index * 0.1, true, 0);
            const std::array<graph::NodeId, 1> demand{4};
            viewers.Capture(frame, demand, renderer);
            renderer.EndFrame();
            Require(viewers.Outputs().size() == 1 && renderer.Stats().live_meshes_ == 3,
                    "inline deformation preview has one bounded independent mesh cache");
        }
        Require(old->deformations_[0].twist_ == 0.5 && original->deformations_.empty(),
                "updated controls never mutate previous geometry snapshots");
        viewers.BeginFrame(3, false, 0);
        Require(renderer.Stats().live_meshes_ == 2,
                "hidden deformation preview releases its cache");
    }
    runtime.Reset();
    Require(renderer.Stats().live_meshes_ == 0 && renderer.Stats().live_textures_ == 0,
            "deformation and embedded image caches release on reset");
    auto oversized = plan;
    for (std::uint64_t index = 10; index <= 14; ++index)
        oversized.instructions_.push_back(
                {registry.MakeNode(index, "geometry.deform"),
                 graph::Operation::kGeometryDeform,
                 {index == 10 ? 0 : oversized.instructions_.size() - 1, {}, {}}});
    Require(graph::ValidateSceneBudget(oversized).has_value(),
            "fifth modifier rejects before execution");
    std::cout << "Deformation: music, immutable models/images, shared upload, chained uniforms, "
                 "previews and limits passed\n";
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
