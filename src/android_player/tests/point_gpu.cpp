#include <GLES3/gl3.h>

#include <array>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"
#include "scene_fixture.h"
#include "scene_graph_fixture.h"

namespace rhythm::validation {
void VerifyPointPixels(render::Renderer& renderer) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "probe.points";
    document.nodes_ = {registry.MakeNode(1, "point.grid"), registry.MakeNode(2, "point.transform"),
                       registry.MakeNode(3, "point.render"),
                       registry.MakeNode(4, "output.texture")};
    document.nodes_[0].properties_["columns"] = 1.0;
    document.nodes_[0].properties_["rows"] = 1.0;
    document.nodes_[0].properties_["point_size"] = 0.75;
    document.nodes_[0].properties_["color_a"] = graph::Color{1, 0, 0, 0.5};
    document.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "points"}, {3, 3, 4, "source"}};
    document.output_ = 4;
    runtime::Runtime runtime;
    for (int scenario = 0; scenario < 3; ++scenario) {
        document.nodes_[2].properties_["point_style"] = scenario == 0 ? 1.0 : 0.0;
        document.nodes_[1].properties_["opacity"] = scenario == 2 ? 0.5 : 1.0;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        for (int frame = 0; frame < 4; ++frame) {
            renderer.BeginFrame();
            const auto output = runtime.Evaluate(plan, {0, 0, {16, 16}}, renderer);
            render::DrawList draw;
            draw.width_ = draw.height_ = 16;
            draw.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
            draw.indices_ = {0, 1, 2, 0, 2, 3};
            draw.commands_ = {{output.final_, 0, 6, {0, 0, 16, 16}}};
            renderer.Submit({}, draw);
            renderer.EndFrame();
        }
        std::array<std::uint8_t, 16 * 16 * 4> pixels{};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        constexpr auto center = (8 * 16 + 8) * 4;
        constexpr auto corner = (2 * 16 + 2) * 4;
        const int expected = scenario == 2 ? 64 : 128;
        if (glGetError() != GL_NO_ERROR || pixels[center] < expected - 2 ||
            pixels[center] > expected + 2 || pixels[center + 1] > 1 || pixels[center + 2] > 1 ||
            (scenario == 0 ? pixels[corner] < 125 : pixels[corner] > 2))
            throw std::runtime_error("probe.point_pixels");
    }
    std::cout << "Point GPU pixels: grid/transform/render, square/soft-circle and composed alpha "
                 "passed\n";
    document.id_ = "probe.point_physics";
    document.nodes_[0].properties_["point_size"] = 0.25;
    document.nodes_[0].properties_["center_y"] = 0.2;
    document.nodes_[1] = registry.MakeNode(2, "point.physics2d");
    document.nodes_[1].properties_["restitution"] = 0.0;
    document.nodes_[1].properties_["body_shape"] = 1.0;
    document.nodes_[2].properties_["point_style"] = 1.0;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    for (int frame = 0; frame < 184; ++frame) {
        renderer.BeginFrame();
        const auto output =
                runtime.Evaluate(plan, {std::min(frame, 180) / 60.0, 0, {16, 16}}, renderer);
        render::DrawList draw;
        draw.width_ = draw.height_ = 16;
        draw.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
        draw.indices_ = {0, 1, 2, 0, 2, 3};
        draw.commands_ = {{output.final_, 0, 6, {0, 0, 16, 16}}};
        renderer.Submit({}, draw);
        renderer.EndFrame();
    }
    std::array<std::uint8_t, 16 * 16 * 4> pixels{};
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    constexpr auto landed = (1 * 16 + 8) * 4;
    constexpr auto initial = (12 * 16 + 8) * 4;
    if (glGetError() != GL_NO_ERROR || pixels[landed] < 126 || pixels[landed] > 130 ||
        pixels[landed + 1] > 1 || pixels[landed + 2] > 1 || pixels[initial] > 1)
        throw std::runtime_error("probe.point_physics_pixels");
    std::cout << "Point physics GPU pixels: simulated box rests at the canvas floor passed\n";
    SceneFixture scene_fixture(renderer);
    const std::array<std::array<int, 3>, 13> expected{{{0, 255, 0},
                                                       {0, 255, 0},
                                                       {0, 0, 0},
                                                       {0, 255, 0},
                                                       {255, 0, 0},
                                                       {0, 255, 0},
                                                       {0, 0, 0},
                                                       {0, 0, 0},
                                                       {0, 0, 0},
                                                       {82, 82, 82},
                                                       {0, 0, 0},
                                                       {20, 20, 20},
                                                       {64, 128, 255}}};
    for (int scenario = 0; scenario < 13; ++scenario) {
        for (int frame = 0; frame < 4; ++frame) scene_fixture.Draw(renderer, scenario);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("probe.scene_readback");
        for (std::size_t channel = 0; channel < 3; ++channel)
            if (std::abs(static_cast<int>(pixels[(8 * 16 + 8) * 4 + channel]) -
                         expected[scenario][channel]) > 2)
                throw std::runtime_error("probe.scene_pixels." + std::to_string(scenario));
    }
    std::cout << "Scene GPU pixels: depth order, front/back culling, double side and depth clear "
                 "passed\n";
    SceneGraphFixture graph_fixture;
    const std::array<std::array<int, 3>, 9> graph_expected{{{0, 255, 0},
                                                            {0, 255, 0},
                                                            {0, 0, 0},
                                                            {0, 0, 255},
                                                            {0, 255, 0},
                                                            {0, 0, 0},
                                                            {0, 255, 0},
                                                            {82, 82, 82},
                                                            {20, 20, 20}}};
    for (int scenario = 0; scenario < 9; ++scenario) {
        for (int frame = 0; frame < 4; ++frame) graph_fixture.Draw(renderer, scenario);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("probe.scene_graph_readback");
        for (std::size_t channel = 0; channel < 3; ++channel)
            if (std::abs(static_cast<int>(pixels[(8 * 16 + 8) * 4 + channel]) -
                         graph_expected[scenario][channel]) > 2)
                throw std::runtime_error("probe.scene_graph_pixels." + std::to_string(scenario));
    }
    std::cout << "Published scene graph GPU pixels: geometry, transform, material, camera and node "
                 "preview passed\n";
}
}  // namespace rhythm::validation
