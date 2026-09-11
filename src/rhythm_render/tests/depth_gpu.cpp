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
    {
        const Extent extent{64, 64};
        std::vector<std::uint8_t> pixels(64 * 64 * 4, 255);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                const auto offset = (y * 64 + x) * 4;
                pixels[offset] = x < 32 ? 255 : 0;
                pixels[offset + 1] = 0;
                pixels[offset + 2] = x < 32 ? 0 : 255;
            }
        const std::array<MeshVertex, 4> left_vertices{
                {{-1, -1, 0}, {0, -1, 0}, {0, 1, 0}, {-1, 1, 0}}};
        const std::array<MeshVertex, 4> right_vertices{
                {{0, -1, 0}, {1, -1, 0}, {1, 1, 0}, {0, 1, 0}}};
        auto left = renderer.CreateMesh(left_vertices, indices);
        auto right = renderer.CreateMesh(right_vertices, indices);
        auto source = renderer.CreateTexture(extent, pixels);
        auto color = renderer.CreateTexture(extent);
        auto depth = renderer.CreateDepthTexture(extent);
        auto output = renderer.CreateTexture(extent);
        SceneDrawList scene;
        scene.projection_[10] = -0.2f;
        scene.projection_[14] = -1.2f;
        MeshDraw near_plane;
        near_plane.mesh_ = left.Handle();
        near_plane.model_[14] = -2;
        near_plane.double_sided_ = true;
        MeshDraw far_plane = near_plane;
        far_plane.mesh_ = right.Handle();
        far_plane.model_[14] = -8;
        scene.draws_ = {near_plane, far_plane};
        DrawList quad;
        quad.width_ = quad.height_ = 64;
        quad.vertices_ = {{0, 0, 0, 0}, {64, 0, 1, 0}, {64, 64, 1, 1}, {0, 64, 0, 1}};
        quad.indices_ = {0, 1, 2, 0, 2, 3};
        quad.commands_ = {{source.Handle(), 0, 6, {0, 0, 64, 64}}};
        quad.commands_[0].depth_of_field_ =
                DepthOfField{depth.Handle(), {1, 11, true}, 4, 8, 12, 64};
        renderer.BeginFrame();
        renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), scene);
        renderer.Submit(output.Handle(), quad);
        auto ticket = renderer.RequestReadback(output.Handle());
        renderer.EndFrame();
        const auto image = Complete(renderer, std::move(ticket));
        const auto channel = [&](int x, int component) {
            return int(image.rgba_[(32 * 64 + x) * 4 + component]);
        };
        std::cout << "dof_boundary near=" << channel(28, 0) << ',' << channel(28, 2)
                  << " seam_left=" << channel(31, 0) << ',' << channel(31, 2)
                  << " seam_right=" << channel(32, 0) << ',' << channel(32, 2)
                  << " far=" << channel(35, 0) << ',' << channel(35, 2) << '\n';
        constexpr std::array<std::array<int, 2>, 4> kExpected{
                {{173, 82}, {135, 119}, {118, 137}, {80, 175}}};
        constexpr std::array kSampleX{28, 31, 32, 35};
        for (std::size_t index = 0; index < kExpected.size(); ++index) {
            const int x = kSampleX[index];
            if (std::abs(channel(x, 0) - kExpected[index][0]) > 3 ||
                std::abs(channel(x, 2) - kExpected[index][1]) > 3)
                throw std::runtime_error("depth.dof_near_far_pixel");
        }
        if (channel(28, 0) <= channel(28, 2) || channel(35, 2) <= channel(35, 0) ||
            channel(31, 0) <= channel(31, 2) || channel(32, 2) <= channel(32, 0))
            throw std::runtime_error("depth.dof_near_far_boundary");
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
    std::cout << "Sampleable depth: perspective/orthographic near/far, signed CoC boundary, "
                 "portrait/landscape passed\n";
}
}  // namespace rhythm::validation
