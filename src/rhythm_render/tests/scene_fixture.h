#pragma once

#include <array>

#include "rhythm/render/renderer.h"

namespace rhythm::validation {
// Shared numerical GPU fixture used by the D3D and GLES host boundaries.
class SceneFixture final {
   public:
    explicit SceneFixture(render::Renderer& renderer) {
        const std::array<render::MeshVertex, 3> vertices{
                {{-.8f, -.8f, 0}, {.8f, -.8f, 0}, {0, .8f, 0}}};
        const std::array<std::uint32_t, 3> forward{0, 1, 2}, backward{0, 2, 1};
        mesh_ = renderer.CreateMesh(vertices, forward);
        reversed_ = renderer.CreateMesh(vertices, backward);
        target_ = renderer.CreateTexture({16, 16});
    }
    render::Readback Draw(render::Renderer& renderer, int scenario, bool capture = false) {
        render::SceneDrawList scene;
        render::MeshDraw near;
        near.mesh_ = scenario == 2 || scenario == 3 ? reversed_.Handle() : mesh_.Handle();
        near.model_[14] = -0.5f;
        near.color_ = {0, 1, 0, 1};
        near.double_sided_ = scenario == 0 || scenario == 3;
        if (scenario == 5) near.model_[0] = -1;
        if (scenario == 6) near.model_[14] = 2;
        if (scenario == 7) near.model_[14] = -2;
        if (scenario >= 8) {
            near.color_ = {1, 1, 1, 1};
            near.unlit_ = false;
            near.roughness_ = 1;
            scene.camera_position_ = {0, 0, 100};
            if (scenario >= 9 && scenario <= 11)
                scene.lights_.push_back({{0, 0, scenario == 10 ? -1.0f : 1.0f}, {1, 1, 1}});
            if (scenario == 11) near.metallic_ = 1;
            if (scenario == 12) near.emissive_ = {0.25f, 0.5f, 1};
        }
        scene.draws_.push_back(near);
        if (scenario == 0 || scenario == 4) {
            auto far = near;
            far.model_[14] = 0.5f;
            far.color_ = {1, 0, 0, 1};
            far.double_sided_ = true;
            if (scenario == 4) scene.draws_.clear();
            scene.draws_.push_back(far);
        }
        renderer.BeginFrame();
        renderer.SubmitScene(target_.Handle(), scene);
        render::DrawList draw;
        draw.width_ = draw.height_ = 16;
        draw.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
        draw.indices_ = {0, 1, 2, 0, 2, 3};
        draw.commands_ = {{target_.Handle(), 0, 6, {0, 0, 16, 16}}};
        renderer.Submit({}, draw, 0x000000ff);
        auto ticket = capture ? renderer.RequestReadback(target_.Handle()) : render::Readback{};
        renderer.EndFrame();
        return ticket;
    }

   private:
    render::Mesh mesh_{};
    render::Mesh reversed_{};
    render::Texture target_{};
};
}  // namespace rhythm::validation
