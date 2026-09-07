#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/render/renderer.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected, "invalid mesh/scene operation must reject");
}
void Run() {
    using namespace rhythm::render;
    auto renderer = Renderer::CreateNull();
    auto other = Renderer::CreateNull();
    Require(renderer.SupportsScenes(), "Null supports scene contract validation");
    std::array<MeshVertex, 3> vertices{{{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    auto mesh = renderer.CreateMesh(vertices, indices);
    const auto handle = mesh.Handle();
    Require(renderer.IsValid(handle) && !other.IsValid(handle), "mesh handles are device scoped");
    Require(renderer.Stats().mesh_bytes_ == sizeof(vertices) + sizeof(indices),
            "mesh memory accounting");
    auto moved = std::move(mesh);
    Require(mesh.Handle() == MeshHandle{} && moved.Handle() == handle,
            "mesh move transfers ownership");
    vertices[0].x_ = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { renderer.CreateMesh(vertices, indices); });
    Require(renderer.Stats().live_meshes_ == 1, "invalid upload preserves resources");
    auto target = renderer.CreateTexture({16, 16});
    SceneDrawList list;
    list.draws_.push_back({handle});
    Reject([&] { renderer.SubmitScene(target.Handle(), list); });
    renderer.BeginFrame();
    Reject([&] { renderer.SubmitScene({}, list); });
    renderer.SubmitScene(target.Handle(), list);
    Require(renderer.Stats().texture_bytes_ == 16 * 16 * 8 && renderer.Stats().draws_ == 1,
            "scene depth attachment uses the texture budget");
    renderer.SubmitScene(target.Handle(), list);
    Require(renderer.Stats().texture_bytes_ == 16 * 16 * 8, "depth attachment reused");
    auto bad = list;
    bad.lights_.resize(5);
    Reject([&] { renderer.SubmitScene(target.Handle(), bad); });
    bad.lights_.resize(1);
    bad.lights_[0].direction_ = {};
    Reject([&] { renderer.SubmitScene(target.Handle(), bad); });
    bad = list;
    bad.draws_[0].roughness_ = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { renderer.SubmitScene(target.Handle(), bad); });
    renderer.EndFrame();
    moved = {};
    Require(!renderer.IsValid(handle) && renderer.Stats().live_meshes_ == 0,
            "mesh RAII release invalidates observers");
    renderer.BeginFrame();
    Reject([&] { renderer.SubmitScene(target.Handle(), list); });
    renderer.EndFrame();
    vertices[0].x_ = -1;
    auto replacement = renderer.CreateMesh(vertices, indices);
    Require(replacement.Handle() != handle, "recycled slots advance generation");
    renderer.Invalidate();
    Require(!renderer.IsValid(replacement.Handle()), "device loss invalidates mesh observers");
    replacement = {};
    target = {};
    Require(renderer.Stats().mesh_bytes_ == 0 && renderer.Stats().texture_bytes_ == 0,
            "owners can clean up after device loss");
    std::cout << "scene render contracts: mesh ownership, generations, finite input, depth budget "
                 "and device loss passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
