#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"
#include "rhythm/runtime/viewers.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document doc;
    doc.id_ = "depth.runtime";
    const auto node = [&](graph::NodeId id, const char* type) {
        doc.nodes_.push_back(registry.MakeNode(id, type));
    };
    node(1, "geometry.cube");
    node(2, "scene.instance");
    node(3, "scene.camera");
    node(4, "scene.capture");
    node(5, "scene.color");
    node(6, "scene.depth");
    node(7, "texture.dof");
    node(8, "texture.display");
    node(9, "output.texture");
    node(10, "depth.linearize");
    doc.edges_ = {{1, 1, 2, "geometry"}, {2, 2, 4, "scene"},   {3, 3, 4, "camera"},
                  {4, 4, 5, "capture"},  {5, 4, 6, "capture"}, {6, 5, 7, "source"},
                  {7, 6, 7, "depth"},    {8, 7, 8, "source"},  {9, 8, 9, "source"},
                  {10, 6, 10, "depth"}};
    doc.output_ = 9;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    const auto evaluate = [&](render::Extent extent, bool reuse) {
        const std::array<graph::NodeId, 1> observed{10};
        auto compiled = graph::Compile(doc, registry, observed);
        Require(std::holds_alternative<graph::ExecutionPlan>(compiled), "capture graph compiles");
        runtime::FrameContext frame{0, 0, extent};
        if (reuse) frame.retained_textures_ = std::vector<graph::NodeId>{};
        renderer.BeginFrame();
        auto result = runtime.Evaluate(std::get<graph::ExecutionPlan>(compiled), frame, renderer);
        renderer.EndFrame();
        return result;
    };
    auto result = evaluate({64, 32}, false);
    const auto output = [&](graph::NodeId id) -> const runtime::NodeOutput& {
        for (const auto& item : result.outputs_)
            if (item.node_ == id) return item;
        throw std::runtime_error("depth.output_missing");
    };
    const auto initial = output(4).scene_image_.value();
    Require(renderer.Precision(output(10).texture_) == render::TexturePrecision::kFloat16,
            "linear depth viewer evaluates as floating data");
    Require(output(5).texture_ == initial.color_ &&
                    output(6).depth_->texture_ == initial.depth_.texture_,
            "extractors share one captured pair");
    Require(renderer.Precision(output(7).texture_) == render::TexturePrecision::kFloat16,
            "DOF inherits capture precision");
    Require(renderer.Precision(result.final_) == render::TexturePrecision::kUnorm8,
            "final SDR mapping");
    const auto bytes = renderer.Stats().texture_bytes_;
    result = evaluate({64, 32}, true);
    Require(output(6).depth_->texture_ == initial.depth_.texture_,
            "reuse never retires captured depth");
    Require(renderer.Stats().texture_bytes_ <= bytes, "reuse bounded");
    doc.nodes_[2].properties_["projection"] = 1.0;
    doc.nodes_[2].properties_["near_plane"] = 0.2;
    doc.nodes_[2].properties_["far_plane"] = 20.0;
    result = evaluate({32, 64}, false);
    Require(!renderer.IsValid(initial.color_) && !renderer.IsValid(initial.depth_.texture_),
            "resize retires both attachments");
    Require(output(6).depth_->projection_.orthographic_ && output(6).depth_->projection_.far_ == 20,
            "exact camera metadata follows depth");
    {
        runtime::Viewers viewers;
        Require(viewers.BeginFrame(0, true, 0), "preview due");
        renderer.BeginFrame();
        const std::array<graph::NodeId, 3> nodes{4, 5, 6};
        viewers.Capture(result, nodes, renderer);
        renderer.EndFrame();
    }
    runtime.Reset();
    Require(renderer.Stats().texture_bytes_ == 0 && renderer.Stats().mesh_bytes_ == 0,
            "capture, extractors and previews release all owners");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Capture/depth/DOF runtime contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
