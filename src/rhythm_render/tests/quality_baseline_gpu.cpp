#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
constexpr std::uint16_t kSize = 64;
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback ticket) {
    for (int frame = 0; frame < 64; ++frame) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("quality.readback_timeout");
}
void Save(const std::filesystem::path& path, const render::ReadbackImage& image) {
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << image.extent_.width_ << ' ' << image.extent_.height_ << "\n255\n";
    for (std::size_t index = 0; index < image.rgba_.size(); index += 4)
        file.write(reinterpret_cast<const char*>(image.rgba_.data() + index), 3);
    if (!file) throw std::runtime_error("quality.evidence_write");
}
render::DrawList Quad(render::TextureHandle source, bool fxaa,
                      render::Extent extent = {kSize, kSize}) {
    render::DrawList result;
    const auto width = float(extent.width_), height = float(extent.height_);
    result.width_ = width;
    result.height_ = height;
    result.vertices_ = {{0, 0, 0, 0}, {width, 0, 1, 0}, {width, height, 1, 1}, {0, height, 0, 1}};
    result.indices_ = {0, 1, 2, 0, 2, 3};
    result.commands_ = {{source, 0, 6, {0, 0, width, height}}};
    if (fxaa) result.commands_[0].texture_fxaa_ = render::TextureFxaa{};
    return result;
}
}  // namespace

void MeasureQualityBaseline(render::Renderer& renderer, const std::filesystem::path& directory) {
    using namespace render;
    std::filesystem::create_directories(directory);
    auto target = renderer.CreateTexture({kSize, kSize});
    auto high = renderer.CreateTexture({kSize * 2, kSize * 2});
    auto resolved = renderer.CreateTexture({kSize, kSize});
    const std::array<std::uint32_t, 6> kIndices{0, 1, 2, 0, 2, 3};
    const std::array<MeshVertex, 4> kPlaneA{
            {{-1, -1, -.6f}, {1, -1, .6f}, {1, 1, .6f}, {-1, 1, -.6f}}};
    auto plane_b = kPlaneA;
    for (auto& vertex : plane_b) vertex.z_ *= -1;
    auto mesh_a = renderer.CreateMesh(kPlaneA, kIndices);
    auto mesh_b = renderer.CreateMesh(plane_b, kIndices);
    MeshDraw red;
    red.mesh_ = mesh_a.Handle();
    red.double_sided_ = true;
    red.color_ = {1, 0, 0, .5f};
    auto blue = red;
    blue.mesh_ = mesh_b.Handle();
    blue.color_ = {0, 0, 1, .5f};
    SceneDrawList scene;
    const auto capture = [&](TextureHandle destination) {
        renderer.BeginFrame();
        renderer.SubmitScene(destination, scene);
        auto ticket = renderer.RequestReadback(destination);
        renderer.EndFrame();
        return Complete(renderer, std::move(ticket));
    };
    std::ofstream report(directory / "measurements.txt");
    for (int order = 0; order < 2; ++order) {
        scene.draws_ = order ? std::vector{blue, red} : std::vector{red, blue};
        const auto image = capture(target.Handle());
        Save(directory / (order ? "transparent-ba.ppm" : "transparent-ab.ppm"), image);
        report << "transparent_order=" << order;
        for (int x : {16, 48}) {
            const auto offset = (32 * kSize + x) * 4;
            report << " x=" << x << " red=" << int(image.rgba_[offset])
                   << " blue=" << int(image.rgba_[offset + 2]);
        }
        report << '\n';
    }
    // A 0.45-pixel vertical line moves over one output pixel in eight phases.
    // Post FXAA cannot recover coverage that was absent in the original raster.
    constexpr float kHalfWidth = .45f / kSize;
    const std::array<MeshVertex, 4> kLine{{{-kHalfWidth, -.8f, 0},
                                           {kHalfWidth, -.8f, 0},
                                           {kHalfWidth, .8f, 0},
                                           {-kHalfWidth, .8f, 0}}};
    auto line = renderer.CreateMesh(kLine, kIndices);
    MeshDraw draw;
    draw.mesh_ = line.Handle();
    draw.double_sided_ = true;
    for (int phase = 0; phase < 8; ++phase) {
        draw.model_[12] = (float(phase) / 8 + .0625f) * 2 / kSize;
        scene.draws_ = {draw};
        for (int mode = 0; mode < 3; ++mode) {
            const auto source = mode == 2 ? high.Handle() : target.Handle();
            renderer.BeginFrame();
            renderer.SubmitScene(source, scene);
            renderer.Submit(resolved.Handle(), Quad(source, mode == 1));
            auto ticket = renderer.RequestReadback(resolved.Handle());
            renderer.EndFrame();
            const auto image = Complete(renderer, std::move(ticket));
            std::uint64_t energy = 0;
            for (std::size_t index = 0; index < image.rgba_.size(); index += 4)
                energy += image.rgba_[index];
            report << "line_phase=" << phase << " mode=" << mode << " red_energy=" << energy
                   << '\n';
            if (phase == 0 || phase == 3)
                Save(directory / ("line-" + std::to_string(phase) + "-" + std::to_string(mode) +
                                  ".ppm"),
                     image);
        }
    }
    // Short resolution/admission probe, not an editor FPS or soak benchmark.
    // Eight overlapping translucent surfaces deliberately exercise fill cost.
    const auto baseline_bytes = renderer.Stats().texture_bytes_;
    for (int scale : {1, 2}) {
        constexpr Extent kOutput{1920, 1080};
        auto color = renderer.CreateTexture(
                {std::uint16_t(kOutput.width_ * scale), std::uint16_t(kOutput.height_ * scale)});
        auto output = renderer.CreateTexture(kOutput);
        scene.draws_ = std::vector<MeshDraw>(8, red);
        const auto started = std::chrono::steady_clock::now();
        for (int frame = 0; frame < 12; ++frame) {
            renderer.BeginFrame();
            renderer.SubmitScene(color.Handle(), scene);
            renderer.Submit(output.Handle(), Quad(color.Handle(), false, kOutput));
            auto ticket = renderer.RequestReadback(output.Handle());
            renderer.EndFrame();
            const auto image = Complete(renderer, std::move(ticket));
            if (image.rgba_.at((540 * 1920 + 960) * 4) < 240)
                throw std::runtime_error("quality.full_resolution_output");
        }
        const auto milliseconds = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - started)
                                          .count();
        report << "output=1920x1080 scale=" << scale
               << " frames=12 cpu_gpu_readback_ms=" << milliseconds
               << " retained_texture_bytes=" << renderer.Stats().texture_bytes_ - baseline_bytes
               << '\n';
    }
    report.flush();
    if (!report) throw std::runtime_error("quality.report_write");
    std::cout << "Quality baseline measured intersecting alpha and subpixel motion at "
              << directory.string() << "; this is a measurement, not quality acceptance\n";
}
}  // namespace rhythm::validation
