#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "vector_pass.h"

namespace {
using namespace rhythm;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
double Area(const render::DrawList& list) {
    double area = 0;
    for (std::size_t index = 0; index < list.indices_.size(); index += 3) {
        const auto a = list.vertices_.at(list.indices_[index]);
        const auto b = list.vertices_.at(list.indices_[index + 1]);
        const auto c = list.vertices_.at(list.indices_[index + 2]);
        area += std::abs((b.x_ - a.x_) * (c.y_ - a.y_) - (b.y_ - a.y_) * (c.x_ - a.x_)) * .5;
    }
    return area;
}
void GeometryCache() {
    graph::Registry registry;
    runtime::detail::VectorMeshes cache;
    std::array<runtime::NodeOutput, 2> inputs{};
    inputs[0].node_ = 1;
    inputs[0].version_ = 1;
    inputs[0].path_ = std::make_shared<const scene::Path>(
            scene::Path{{{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}}, true});
    inputs[1].node_ = 2;
    inputs[1].version_ = 1;
    inputs[1].path_ = std::make_shared<const scene::Path>(
            scene::Path{{{-.5, -.5, 0}, {.5, -.5, 0}, {.5, .5, 0}, {-.5, .5, 0}}, true});
    graph::Instruction instruction{
            registry.MakeNode(3, "texture.path_fill"), graph::Operation::kVectorFill, {0}};
    const auto draw = [&](float width = 200, float height = 100) {
        render::DrawList list;
        list.width_ = width;
        list.height_ = height;
        cache.Draw(instruction, inputs, {}, list);
        return list;
    };
    auto list = draw();
    Require(Area(list) == 2500, "aspect-preserving projection area");
    for (const auto vertex : list.vertices_)
        Require(vertex.x_ >= 75 && vertex.x_ <= 125 && vertex.y_ >= 25 && vertex.y_ <= 75,
                "centered projection bounds");
    const auto color = list.vertices_.front().color_;
    instruction.node_.properties_["color_a"] = graph::Color{1, 0, 0, 1};
    list = draw();
    Require(cache.Builds() == 1 && list.vertices_.front().color_ != color,
            "color-only edit reuses geometry and updates draw color");
    instruction.inputs_.push_back(1);
    Require(Area(draw()) == 1875 && cache.Builds() == 2, "hole subtracts projected area");
    Require(Area(draw(400, 200)) == 7500 && cache.Builds() == 3, "resize invalidates mesh");
    ++inputs[0].version_;
    draw(400, 200);
    Require(cache.Builds() == 4, "source version invalidates mesh");
    instruction = {
            registry.MakeNode(4, "texture.path_stroke"), graph::Operation::kVectorStroke, {0, 1}};
    inputs[1].scalar_ = 2;
    const auto thin = Area(draw());
    inputs[1].scalar_ = 8;
    Require(Area(draw()) > thin * 3 && cache.Builds() == 6,
            "signal-driven width rebuilds stroke geometry");
    cache.Retain({});
    Require(cache.Size() == 0, "removed vector nodes release CPU meshes");
}
void RuntimeIntegration() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "vector.runtime";
    document.nodes_ = {registry.MakeNode(1, "path.helix"),
                       registry.MakeNode(2, "texture.path_fill"),
                       registry.MakeNode(3, "output.texture")};
    document.nodes_[0].properties_["path_height"] = 0.0;
    document.nodes_[0].properties_["path_turns"] = 1.0;
    document.nodes_[0].properties_["path_closed"] = 1.0;
    document.nodes_[1].properties_["path_plane"] = 1.0;
    document.edges_ = {{1, 1, 2, "path"}, {2, 2, 3, "source"}};
    document.output_ = 3;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    const auto evaluate = [&] {
        renderer.BeginFrame();
        const auto frame = runtime.Evaluate(plan, {0, 0, {200, 100}}, renderer);
        renderer.EndFrame();
        return frame;
    };
    Require(renderer.IsValid(evaluate().final_), "typed path fill produces runtime texture");
    Require(evaluate().evaluated_ == 0, "static vector graph reuses outputs");
    runtime.Reset();
    Require(renderer.Stats().live_textures_ == 0, "reset releases vector textures");
}
}  // namespace
int main() {
    try {
        GeometryCache();
        RuntimeIntegration();
        std::cout << "Vector projection, holes, cache, driven width and runtime lifecycle passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
