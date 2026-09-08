#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback ticket) {
    for (int i = 0; i < 32; ++i) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("depth.readback_timeout");
}
}  // namespace
void VerifySampleableDepth(render::Renderer& renderer) {
    using namespace render;
    if (!renderer.SupportsSampleableDepth()) throw std::runtime_error("depth.unsupported");
    const std::array<MeshVertex, 4> vertices{
            {{-20, -20, 0}, {20, -20, 0}, {20, 20, 0}, {-20, 20, 0}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    {
        const Extent extent{64, 64};
        std::vector<std::uint8_t> pixels(64 * 64 * 4);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                const auto offset = (y * 64 + x) * 4;
                const auto value = ((x / 4 + y / 4) % 2) ? 255 : 0;
                pixels[offset] = pixels[offset + 1] = pixels[offset + 2] = std::uint8_t(value);
                pixels[offset + 3] = 128;
            }
        auto checker = renderer.CreateTexture(extent, pixels);
        auto color = renderer.CreateTexture(extent);
        auto depth = renderer.CreateDepthTexture(extent);
        auto output = renderer.CreateTexture(extent);
        SceneDrawList scene;
        scene.projection_[10] = -0.2f;
        scene.projection_[14] = -1.2f;
        MeshDraw plane;
        plane.mesh_ = mesh.Handle();
        plane.model_[14] = -2;
        plane.double_sided_ = true;
        scene.draws_ = {plane};
        DrawList quad;
        quad.width_ = quad.height_ = 64;
        quad.vertices_ = {{0, 0, 0, 0}, {64, 0, 1, 0}, {64, 64, 1, 1}, {0, 64, 0, 1}};
        quad.indices_ = {0, 1, 2, 0, 2, 3};
        quad.commands_ = {{checker.Handle(), 0, 6, {0, 0, 64, 64}}};
        for (float focus : {2.0f, 8.0f}) {
            quad.commands_[0].depth_of_field_ =
                    DepthOfField{depth.Handle(), {1, 11, true}, focus, 8, 12, 64};
            renderer.BeginFrame();
            renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), scene);
            renderer.Submit(output.Handle(), quad);
            auto ticket = renderer.RequestReadback(output.Handle());
            renderer.EndFrame();
            const auto image = Complete(renderer, std::move(ticket));
            double difference = 0, contrast = 0;
            for (int y = 12; y < 52; ++y)
                for (int x = 12; x < 52; ++x) {
                    const auto offset = (y * 64 + x) * 4;
                    const int expected = pixels[offset] ? 128 : 0;
                    difference += std::abs(int(image.rgba_[offset]) - expected);
                    contrast += std::abs(int(image.rgba_[offset]) - 64);
                    if (std::abs(int(image.rgba_[offset + 3]) - 128) > 2)
                        throw std::runtime_error("depth.dof_alpha");
                }
            if ((focus == 2 && difference / 1600 > 2) || (focus == 8 && contrast / 1600 > 35))
                throw std::runtime_error("depth.dof_focus." + std::to_string(difference / 1600) +
                                         ".contrast." + std::to_string(contrast / 1600));
        }
    }
    for (const Extent extent : {Extent{64, 32}, Extent{32, 64}}) {
        auto color = renderer.CreateTexture(extent, {}, TexturePrecision::kFloat16);
        auto depth = renderer.CreateDepthTexture(extent);
        auto linear = renderer.CreateTexture(extent, {}, TexturePrecision::kFloat16);
        auto output = renderer.CreateTexture(extent);
        for (bool orthographic : {false, true}) {
            for (float distance : {2.0f, 6.0f, 9.0f}) {
                constexpr float n = 1, f = 11;
                SceneDrawList scene;
                if (orthographic) {
                    scene.projection_[0] = scene.projection_[5] = 0.1f;
                    scene.projection_[10] = -2 / (f - n);
                    scene.projection_[14] = -(f + n) / (f - n);
                } else {
                    scene.projection_[0] = scene.projection_[5] = 1;
                    scene.projection_[10] = -(f + n) / (f - n);
                    scene.projection_[11] = -1;
                    scene.projection_[14] = -2 * f * n / (f - n);
                    scene.projection_[15] = 0;
                }
                MeshDraw plane;
                plane.mesh_ = mesh.Handle();
                plane.model_[14] = -distance;
                plane.double_sided_ = true;
                scene.draws_.push_back(plane);
                DrawList quad;
                quad.width_ = extent.width_;
                quad.height_ = extent.height_;
                quad.vertices_ = {{0, 0, 0, 0},
                                  {quad.width_, 0, 1, 0},
                                  {quad.width_, quad.height_, 1, 1},
                                  {0, quad.height_, 0, 1}};
                quad.indices_ = {0, 1, 2, 0, 2, 3};
                quad.commands_ = {{depth.Handle(), 0, 6, {0, 0, quad.width_, quad.height_}}};
                quad.commands_[0].depth_linearization_ =
                        DepthLinearization{n, f, orthographic, true};
                renderer.BeginFrame();
                renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), scene);
                renderer.Submit(linear.Handle(), quad);
                quad.commands_[0].texture_ = linear.Handle();
                quad.commands_[0].depth_linearization_.reset();
                renderer.Submit(output.Handle(), quad);
                auto ticket = renderer.RequestReadback(output.Handle());
                renderer.EndFrame();
                const auto image = Complete(renderer, std::move(ticket));
                const auto center = (extent.height_ / 2 * extent.width_ + extent.width_ / 2) * 4;
                const auto expected = std::lround((distance - n) / (f - n) * 255);
                if (std::abs(long(image.rgba_[center]) - expected) > 2)
                    throw std::runtime_error("depth.pixel." + std::to_string(image.rgba_[center]) +
                                             ".expected." + std::to_string(expected));
                // Replace explicit attachments with legacy depth and back on the same color target.
                renderer.BeginFrame();
                renderer.SubmitScene(color.Handle(), scene);
                renderer.EndFrame();
            }
        }
    }
    std::cout << "Sampleable depth: perspective/orthographic near/far, portrait/landscape passed\n";
}
}  // namespace rhythm::validation
