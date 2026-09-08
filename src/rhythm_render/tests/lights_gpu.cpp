#include <array>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"
namespace rhythm::validation {
void VerifyPositionalLights(render::Renderer& renderer) {
    using namespace render;
    const std::array<MeshVertex, 4> vertices{
            {{-0.9f, -0.9f}, {0.9f, -0.9f}, {0.9f, 0.9f}, {-0.9f, 0.9f}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    auto target = renderer.CreateTexture({32, 32});
    SceneDrawList scene;
    MeshDraw draw;
    draw.mesh_ = mesh.Handle();
    draw.unlit_ = false;
    draw.double_sided_ = true;
    draw.roughness_ = 1;
    scene.draws_ = {draw};
    PositionalLight light;
    light.radiance_ = {1, 0, 0};
    light.position_ = {0, 0, 1};
    light.range_ = 100;
    const auto capture = [&] {
        scene.positional_lights_ = {light};
        renderer.BeginFrame();
        renderer.SubmitScene(target.Handle(), scene);
        auto ticket = renderer.RequestReadback(target.Handle());
        renderer.EndFrame();
        for (int i = 0; i < 32; ++i) {
            if (auto image = ticket.Poll()) return int(image->rgba_[(16 * 32 + 16) * 4]);
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        throw std::runtime_error("lights.readback_timeout");
    };
    const auto near = capture();
    light.position_[2] = 2;
    const auto far = capture();
    if (near < 50 || far < 10 || near < far * 3 || near > far * 5)
        throw std::runtime_error("lights.distance_falloff");
    light.range_ = 1;
    if (capture() != 0) throw std::runtime_error("lights.range_cutoff");
    light.range_ = 100;
    light.spot_ = true;
    light.cone_angle_ = 10;
    const auto inside = capture();
    light.direction_ = {1, 0, 0};
    if (inside < 10 || capture() != 0) throw std::runtime_error("lights.spot_cone");
    std::cout << "Point/spot lighting: inverse-square distance, range cutoff and cone passed\n";
}
}  // namespace rhythm::validation
