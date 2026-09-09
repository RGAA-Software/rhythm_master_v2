#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback ticket) {
    for (int wait = 0; wait < 32; ++wait) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("instances.readback_timeout");
}
}  // namespace
void VerifySceneInstances(render::Renderer& renderer,
                          std::optional<render::SurfaceProgramInput> surface) {
    constexpr std::uint32_t kCount = 16;
    const std::array<render::MeshVertex, 4> vertices{{{-0.8f, -0.8f, 0, 0.6f, 0, 0.8f},
                                                      {0.8f, -0.8f, 0, 0.6f, 0, 0.8f},
                                                      {0.8f, 0.8f, 0, 0.6f, 0, 0.8f},
                                                      {-0.8f, 0.8f, 0, 0.6f, 0, 0.8f}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    auto alternate = renderer.CreateMesh(vertices, indices);
    auto target = renderer.CreateTexture({64, 64});
    for (int scenario = 0; scenario < 7; ++scenario) {
        render::SceneDrawList list;
        list.lights_ = {{{0, 0, 1}, {1, 1, 1}}};
        list.camera_position_ = {0, 0, 4};
        list.view_[12] = 0.08f;
        list.projection_[0] = 0.8f;
        list.projection_[5] = 0.9f;
        for (std::uint32_t i = 0; i < kCount; ++i) {
            render::MeshDraw draw;
            draw.surface_program_ = surface;
            draw.mesh_ = mesh.Handle();
            draw.color_ = {0.3f, 0.7f, 0.9f, scenario == 5 ? 0.5f : 1.0f};
            draw.unlit_ = scenario != 1 && scenario != 4;
            draw.double_sided_ = scenario == 4;
            draw.roughness_ = 0.8f;
            if (scenario == 0 || scenario == 1) draw.color_[0] = float(i) / kCount;
            float sx = 0.2f, sy = scenario == 1 ? 0.1f : 0.2f;
            if (scenario == 2 || ((scenario == 4 || scenario == 6) && i % 2)) sx = -sx;
            const float angle = float(i) * 0.1f, c = std::cos(angle), s = std::sin(angle);
            draw.model_[0] = c * sx;
            draw.model_[1] = s * sx;
            draw.model_[4] = -s * sy;
            draw.model_[5] = c * sy;
            draw.model_[12] = scenario == 5 ? 0 : float(i % 4) * 0.5f - 0.75f;
            draw.model_[13] = scenario == 5 ? 0 : float(i / 4) * 0.5f - 0.75f;
            draw.model_[14] = scenario == 5 ? 0.5f - float(i) * 0.05f : 0;
            draw.normal_[0] = c / sx;
            draw.normal_[1] = s / sx;
            draw.normal_[4] = -s / sy;
            draw.normal_[5] = c / sy;
            if (scenario == 3) {
                draw.color_[0] = float(i / 4) * 0.2f;
                draw.roughness_ = 0.2f + float(i / 4) * 0.2f;
            }
            list.draws_.push_back(draw);
        }
        std::array<render::ReadbackImage, 2> images{};
        for (int mode = 0; mode < 2; ++mode) {
            // Equal geometry under distinct handles forces the ordinary path.
            if (mode == 1)
                for (std::uint32_t i = 0; i < kCount; ++i)
                    list.draws_[i].mesh_ = i % 2 ? alternate.Handle() : mesh.Handle();
            renderer.BeginFrame();
            renderer.SubmitScene(target.Handle(), list);
            const auto expected = mode == 1 || scenario >= 5 ? kCount : scenario == 3 ? 4U : 1U;
            if (renderer.Stats().draws_ != expected)
                throw std::runtime_error("instances.submission_count." + std::to_string(scenario));
            auto ticket = renderer.RequestReadback(target.Handle());
            renderer.EndFrame();
            images[mode] = Complete(renderer, std::move(ticket));
        }
        std::size_t visible = 0;
        for (std::size_t i = 0; i < images[0].rgba_.size(); ++i) {
            visible += images[0].rgba_[i] > 20;
            if (std::abs(int(images[0].rgba_[i]) - int(images[1].rgba_[i])) > 2)
                throw std::runtime_error("instances.pixel_equivalence." + std::to_string(scenario));
        }
        if (visible < 40) throw std::runtime_error("instances.empty_output");
        std::cout << "scene_instances scenario=" << scenario << " records=" << kCount
                  << " pixels=equivalent\n";
    }
    auto large_target = renderer.CreateTexture({256, 256});
    for (const std::uint32_t count : {1000U, 10000U}) {
        render::SceneDrawList list;
        list.draws_.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            render::MeshDraw draw;
            draw.surface_program_ = surface;
            draw.mesh_ = mesh.Handle();
            draw.model_[0] = draw.model_[5] = 0.009f;
            draw.model_[12] = float(i % 100) * 0.02f - 0.99f;
            draw.model_[13] = float(i / 100) * 0.02f - 0.99f;
            draw.color_ = {float(i % 100) / 100, 0.7f, 1, 1};
            list.draws_.push_back(draw);
        }
        renderer.BeginFrame();
        const auto start = std::chrono::steady_clock::now();
        renderer.SubmitScene(large_target.Handle(), list);
        const auto cpu_ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                        .count();
        if (renderer.Stats().draws_ != 1) throw std::runtime_error("instances.scale_submissions");
        auto ticket = renderer.RequestReadback(large_target.Handle());
        renderer.EndFrame();
        const auto image = Complete(renderer, std::move(ticket));
        std::size_t visible = 0;
        for (std::size_t i = 2; i < image.rgba_.size(); i += 4) visible += image.rgba_[i] > 200;
        if (visible < count) throw std::runtime_error("instances.scale_pixels");
        std::cout << "scene_instances records=" << count << " draws=1 cpu_submit_ms=" << cpu_ms
                  << " instance_bytes=" << count * 144U << " visible_pixels=" << visible << '\n';
    }
}
}  // namespace rhythm::validation
