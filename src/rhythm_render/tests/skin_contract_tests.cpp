#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/render/renderer.h"

namespace {
void Require(bool condition) {
    if (!condition) throw std::runtime_error("skin contract mismatch");
}
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("invalid skin accepted");
}
void Run() {
    using namespace rhythm::render;
    auto renderer = Renderer::CreateNull();
    const std::array<MeshVertex, 3> vertices{{{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    std::array<SkinWeights, 3> weights;
    for (auto& vertex : weights) vertex.joints_.fill(47);
    auto mesh = renderer.CreateMesh(vertices, indices, weights);
    Require(renderer.Stats().mesh_bytes_ == sizeof(vertices) + sizeof(indices) + sizeof(weights));
    auto target = renderer.CreateTexture({16, 16});
    SceneDrawList scene;
    scene.draws_.push_back({mesh.Handle()});
    renderer.BeginFrame();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.draws_[0].bones_.assign(48, kIdentityMatrix);
    renderer.SubmitScene(target.Handle(), scene);
    auto oversized = scene;
    oversized.draws_.resize(1366, scene.draws_[0]);
    Reject([&] { renderer.SubmitScene(target.Handle(), oversized); });
    scene.draws_[0].bones_.pop_back();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.draws_[0].bones_.assign(49, kIdentityMatrix);
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.draws_[0].bones_.resize(48);
    scene.draws_[0].bones_[47][12] = std::numeric_limits<float>::infinity();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.draws_[0].bones_[47] = kIdentityMatrix;
    scene.draws_[0].bones_[47][3] = 1;
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    renderer.EndFrame();
    Reject([&] { renderer.CreateMesh(vertices, indices, std::span(weights).first(2)); });
    weights[0].joints_[3] = 48;
    Reject([&] { renderer.CreateMesh(vertices, indices, weights); });
    weights[0].joints_[3] = 0;
    weights[0].weights_[0] = -1;
    Reject([&] { renderer.CreateMesh(vertices, indices, weights); });
    weights[0].weights_[0] = 0;
    Reject([&] { renderer.CreateMesh(vertices, indices, weights); });
    weights[0].weights_[0] = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { renderer.CreateMesh(vertices, indices, weights); });
    const auto old = mesh.Handle();
    mesh = {};
    Require(renderer.Stats().mesh_bytes_ == 0);
    mesh = renderer.CreateMesh(vertices, indices);
    Require(mesh.Handle() != old);
    scene.draws_[0].mesh_ = mesh.Handle();
    scene.draws_[0].bones_.assign(1, kIdentityMatrix);
    renderer.BeginFrame();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.draws_[0].bones_.clear();
    renderer.SubmitScene(target.Handle(), scene);
    renderer.EndFrame();
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Skin: vertex stream, palettes, admission and ownership passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
