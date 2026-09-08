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
void Shadows() {
    using namespace rhythm::render;
    auto renderer = Renderer::CreateNull();
    auto target = renderer.CreateTexture({16, 16});
    auto color = renderer.CreateTexture({256, 256});
    auto depth = renderer.CreateDepthTexture({256, 256});
    SceneDrawList scene;
    scene.lights_.push_back({});
    scene.shadow_ = SceneShadow{};
    scene.shadow_->depth_ = depth.Handle();
    scene.shadow_->resolution_ = 256;
    renderer.BeginFrame();
    renderer.SubmitScene(target.Handle(), scene);
    Reject([&] { renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), scene); });
    scene.shadow_->depth_ = color.Handle();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.shadow_->depth_ = depth.Handle();
    scene.shadow_->resolution_ = 512;
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.shadow_->resolution_ = 256;
    scene.shadow_->light_ = 1;
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.shadow_->light_ = 0;
    scene.lights_.clear();
    scene.positional_lights_.push_back({});
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    scene.positional_lights_[0].spot_ = true;
    renderer.SubmitScene(target.Handle(), scene);
    scene.shadow_->normal_bias_ = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    renderer.EndFrame();
}
void MaterialTextures() {
    using namespace rhythm::render;
    auto renderer = Renderer::CreateNull();
    auto other = Renderer::CreateNull();
    const std::array<MeshVertex, 3> vertices{{{-1, -1}, {1, -1}, {0, 1}}};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    const std::array<std::uint8_t, 4> pixel{128, 128, 255, 255};
    auto mesh = renderer.CreateMesh(vertices, indices);
    auto target = renderer.CreateTexture({16, 16});
    auto depth = renderer.CreateDepthTexture({16, 16});
    auto texture = renderer.CreateTexture({1, 1}, pixel);
    auto foreign = other.CreateTexture({1, 1}, pixel);
    SceneDrawList scene;
    scene.draws_.push_back({mesh.Handle()});
    renderer.BeginFrame();
    auto& slots = scene.draws_[0].textures_.slots_;
    for (const auto invalid :
         {foreign.Handle(), depth.Handle(), target.Handle(), TextureHandle{0, 1, 0}}) {
        slots[0] = invalid;
        Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
        Reject([&] { renderer.SubmitSceneDepth(target.Handle(), depth.Handle(), scene); });
    }
    slots = {texture.Handle(), texture.Handle(), texture.Handle(), texture.Handle()};
    renderer.SubmitSceneDepth(target.Handle(), depth.Handle(), scene);
    Reject([&] { renderer.UpdateTexture(texture.Handle(), pixel); });
    renderer.EndFrame();
    renderer.BeginFrame();
    renderer.UpdateTexture(texture.Handle(), pixel);
    const auto stale = texture.Handle();
    texture = {};
    auto replacement = renderer.CreateTexture({1, 1}, pixel);
    Require(replacement.Handle() != stale, "material texture generations advance on reuse");
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    slots = {};
    scene.draws_[0].textures_.normal_scale_ = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { renderer.SubmitScene(target.Handle(), scene); });
    renderer.EndFrame();
}
}  // namespace
int main() {
    try {
        Run();
        MaterialTextures();
        Shadows();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
