#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/render/renderer.h"

namespace {
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("invalid morph accepted");
}
void Run() {
    using namespace rhythm::render;
    auto renderer = Renderer::CreateNull();
    const std::array<MeshVertex, 3> vertices{{{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    std::array<MorphTarget, 4> morphs;
    for (auto& morph : morphs) morph.deltas_.resize(3);
    auto mesh = renderer.CreateMesh(vertices, indices, {}, morphs);
    if (renderer.Stats().mesh_bytes_ != sizeof(vertices) + sizeof(indices) + 1024 * 16 + 3 * 24)
        throw std::runtime_error("morph storage accounting");
    auto target = renderer.CreateTexture({16, 16});
    SceneDrawList scene;
    scene.draws_.push_back({mesh.Handle()});
    scene.draws_[0].morph_weights_ = {0.3f, -0.2f, 1, 0};
    renderer.BeginFrame();
    renderer.SubmitScene(target.Handle(), scene);
    scene.draws_[0].morph_weights_[3] = 101;
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.draws_[0].morph_weights_[3] = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    renderer.EndFrame();
    morphs[0].deltas_.pop_back();
    Reject([&] { renderer.CreateMesh(vertices, indices, {}, morphs); });
    morphs[0].deltas_.resize(3);
    morphs[0].deltas_[0].position_[0] = std::numeric_limits<float>::infinity();
    Reject([&] { renderer.CreateMesh(vertices, indices, {}, morphs); });
    const std::array<MorphTarget, 5> oversized;
    Reject([&] { renderer.CreateMesh(vertices, indices, {}, oversized); });
    const auto old = mesh.Handle();
    mesh = {};
    if (renderer.Stats().mesh_bytes_ != 0) throw std::runtime_error("morph resources retained");
    mesh = renderer.CreateMesh(vertices, indices);
    if (mesh.Handle() == old) throw std::runtime_error("morph stale generation reused");
    scene.draws_[0].mesh_ = mesh.Handle();
    scene.draws_[0].morph_weights_ = {1, 0, 0, 0};
    renderer.BeginFrame();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.draws_[0].morph_weights_ = {};
    renderer.SubmitScene(target.Handle(), scene);
    renderer.EndFrame();
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Morph: targets, weight admission, storage and ownership passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
