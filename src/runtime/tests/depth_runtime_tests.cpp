#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"
#include "rhythm/runtime/viewers.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Demand() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "capture.demand";
    document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                       registry.MakeNode(2, "output.texture")};
    document.edges_ = {{1, 1, 2, "source"}};
    document.output_ = 2;
    // A large authored graph must not allocate attachments for unobserved branches.
    for (graph::NodeId base = 10; base < 1010; base += 5) {
        document.nodes_.push_back(registry.MakeNode(base, "geometry.cube"));
        document.nodes_.push_back(registry.MakeNode(base + 1, "scene.instance"));
        document.nodes_.push_back(registry.MakeNode(base + 2, "scene.capture"));
        document.nodes_.push_back(registry.MakeNode(base + 3, "scene.color"));
        document.nodes_.push_back(registry.MakeNode(base + 4, "scene.depth"));
        document.edges_.push_back({base, base, base + 1, "geometry"});
        document.edges_.push_back({base + 1, base + 1, base + 2, "scene"});
        document.edges_.push_back({base + 2, base + 2, base + 3, "capture"});
        document.edges_.push_back({base + 3, base + 2, base + 4, "capture"});
    }
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::FrameContext context;
    context.extent_ = {64, 32};
    const auto evaluate = [&](std::span<const graph::NodeId> observed) {
        const auto compiled = graph::Compile(document, registry, observed);
        Require(std::holds_alternative<graph::ExecutionPlan>(compiled),
                "large capture demand graph did not compile");
        renderer.BeginFrame();
        auto result = runtime.Evaluate(std::get<graph::ExecutionPlan>(compiled), context, renderer);
        renderer.EndFrame();
        return result;
    };
    evaluate({});
    const auto base_bytes = renderer.Stats().texture_bytes_;
    Require(renderer.Stats().passes_ == 1 && renderer.Stats().live_meshes_ == 0,
            "unobserved captures allocated or drew");
    const std::array<graph::NodeId, 2> observed{13, 14};
    auto result = evaluate(observed);
    const auto find = [&](graph::NodeId id) -> const runtime::NodeOutput& {
        for (const auto& output : result.outputs_)
            if (output.node_ == id) return output;
        throw std::runtime_error("capture demand output missing");
    };
    const auto capture = find(12).scene_image_.value();
    Require(find(13).texture_ == capture.color_ &&
                    find(14).depth_->texture_ == capture.depth_.texture_ &&
                    renderer.Stats().passes_ == 1 && renderer.Stats().live_meshes_ == 1,
            "two extractors must share one capture pass");
    const auto captured_bytes = renderer.Stats().texture_bytes_;
    Require(captured_bytes > base_bytes, "requested capture lacks attachment accounting");
    result = evaluate(observed);
    Require(renderer.Stats().passes_ == 0 && renderer.Stats().texture_bytes_ == captured_bytes,
            "unchanged capture demand redrew or reallocated");
    {
        runtime::Viewers viewers;
        viewers.BeginFrame(0, true, 0);
        renderer.BeginFrame();
        viewers.Capture(result, observed, renderer);
        renderer.EndFrame();
        Require(viewers.Outputs().size() == 2 && renderer.Stats().passes_ == 2,
                "capture previews did not independently visualize color and depth");
        Require(renderer.IsValid(capture.color_) && renderer.IsValid(capture.depth_.texture_),
                "preview retired the owning capture");
        viewers.BeginFrame(1, false, 0);
        Require(viewers.Outputs().empty() && renderer.Stats().texture_bytes_ == captured_bytes,
                "hidden capture previews retained resources");
    }
    result = evaluate({});
    Require(renderer.Stats().texture_bytes_ == base_bytes && renderer.Stats().live_meshes_ == 0 &&
                    !renderer.IsValid(capture.color_) && !renderer.IsValid(capture.depth_.texture_),
            "removed demand retained captured attachments or mesh");
    result = evaluate(observed);
    Require(renderer.IsValid(find(12).scene_image_->color_) && renderer.Stats().passes_ == 1,
            "reintroduced demand failed to rebuild capture");
    runtime.Reset();
    Require(renderer.Stats().texture_bytes_ == 0 && renderer.Stats().live_meshes_ == 0,
            "large capture graph leaked after reset");
    std::cout << "1002-node capture demand: base " << base_bytes << " bytes, observed "
              << captured_bytes << " bytes; pruning, sharing, previews and release passed\n";
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
        Demand();
        std::cout << "Capture/depth/DOF runtime contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
