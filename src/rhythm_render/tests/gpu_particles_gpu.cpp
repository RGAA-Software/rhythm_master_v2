#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"
namespace rhythm::validation {
namespace {
render::ReadbackImage Capture(render::Renderer& renderer, render::TextureHandle target,
                              render::GpuPointHandle points,
                              const render::GpuPointStyle& style = {}) {
    renderer.BeginFrame();
    renderer.SubmitGpuPoints(target, points, style);
    auto ticket = renderer.RequestReadback(target);
    renderer.EndFrame();
    for (int i = 0; i < 32; ++i) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("gpu_particles.readback_timeout");
}
void Update(render::Renderer& renderer, render::GpuPointHandle points,
            const render::GpuParticleStep& step) {
    renderer.BeginFrame();
    renderer.UpdateGpuParticles(points, step);
    renderer.EndFrame();
}
void SamplingOrientation(render::Renderer& renderer) {
    const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
    auto source = renderer.CreateTexture({1, 1}, white);
    auto map = renderer.CreateTexture({2, 2});
    auto target = renderer.CreateTexture({64, 64});
    render::DrawList list;
    list.width_ = list.height_ = 2;
    list.vertices_ = {{0, 0, 0, 0, 0xff0000ff}, {2, 0, 1, 0, 0xff0000ff}, {2, 1, 1, 1, 0xff0000ff},
                      {0, 1, 0, 1, 0xff0000ff}, {0, 1, 0, 0, 0xffff0000}, {2, 1, 1, 0, 0xffff0000},
                      {2, 2, 1, 1, 0xffff0000}, {0, 2, 0, 1, 0xffff0000}};
    list.indices_ = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    list.commands_.push_back({source.Handle(), 0, 12, {0, 0, 2, 2}});
    renderer.BeginFrame();
    renderer.Submit(map.Handle(), list, 0);
    renderer.EndFrame();
    auto points = renderer.CreateGpuPoints(1);
    render::GpuParticleStep step;
    step.reset_ = true;
    step.spawn_count_ = 1;
    step.center_ = {.5f, .25f, 0};
    step.radius_ = step.speed_ = step.flow_ = 0;
    step.size_ = .15f;
    step.color_a_ = step.color_b_ = {1, 1, 1, 1};
    render::GpuPointStyle style;
    style.sampling_ = render::GpuPointSampling{map.Handle(), 1, 0};
    Update(renderer, points.Handle(), step);
    auto image = Capture(renderer, target.Handle(), points.Handle(), style);
    constexpr std::size_t kTop = (16 * 64 + 32) * 4;
    if (image.rgba_[kTop] < 200 || image.rgba_[kTop + 2] > 10)
        throw std::runtime_error("gpu_particles.sample_top_origin");
    step.center_[1] = .75f;
    Update(renderer, points.Handle(), step);
    image = Capture(renderer, target.Handle(), points.Handle(), style);
    constexpr std::size_t kBottom = (48 * 64 + 32) * 4;
    if (image.rgba_[kBottom + 2] < 200 || image.rgba_[kBottom] > 10)
        throw std::runtime_error("gpu_particles.sample_bottom_origin");
}
}  // namespace
void VerifyGpuParticles(render::Renderer& renderer) {
    if (!renderer.SupportsGpuPoints()) {
        std::cout << "gpu_particles unsupported on this device/profile\n";
        return;
    }
    SamplingOrientation(renderer);
    auto target = renderer.CreateTexture({64, 64});
    // Non-workgroup-aligned capacity and wrapping ring exercise bounds guards.
    auto points = renderer.CreateGpuPoints(65);
    render::GpuParticleStep step;
    step.reset_ = true;
    step.spawn_start_ = 64;
    step.spawn_count_ = 2;
    step.radius_ = 0;
    step.speed_ = 0;
    step.flow_ = 0;
    step.gravity_ = {};
    step.lifetime_ = 0.05f;
    step.size_ = 0.3f;
    step.color_a_ = step.color_b_ = {1, 0, 0, 1};
    Update(renderer, points.Handle(), step);
    auto first = Capture(renderer, target.Handle(), points.Handle());
    constexpr std::size_t kCenter = (32 * 64 + 32) * 4;
    if (first.rgba_[kCenter] < 240 || first.rgba_[kCenter + 1] != 0)
        throw std::runtime_error("gpu_particles.spawn_color");
    auto paused = Capture(renderer, target.Handle(), points.Handle());
    if (first.rgba_ != paused.rgba_) throw std::runtime_error("gpu_particles.read_mutation");
    // Attribute sampling is a view of unchanged point records, not a simulation update.
    const std::array<std::uint8_t, 4> blue{0, 0, 255, 255};
    auto map = renderer.CreateTexture({1, 1}, blue);
    render::GpuPointStyle sampled;
    sampled.sampling_ = render::GpuPointSampling{map.Handle(), 1, 0};
    const auto colored = Capture(renderer, target.Handle(), points.Handle(), sampled);
    if (colored.rgba_[kCenter] != 0 || colored.rgba_[kCenter + 2] < 240)
        throw std::runtime_error("gpu_particles.sample_color");
    sampled.sampling_->size_amount_ = 1;
    const auto smaller = Capture(renderer, target.Handle(), points.Handle(), sampled);
    const auto visible = [](const render::ReadbackImage& image) {
        return std::count_if(image.rgba_.begin(), image.rgba_.end(),
                             [](auto byte) { return byte > 0; });
    };
    if (visible(smaller) >= visible(colored) / 2)
        throw std::runtime_error("gpu_particles.sample_size");
    const std::array<std::uint8_t, 4> transparent{0, 0, 0, 0};
    renderer.BeginFrame();
    renderer.UpdateTexture(map.Handle(), transparent);
    renderer.EndFrame();
    sampled.sampling_->size_amount_ = 0;
    const auto masked = Capture(renderer, target.Handle(), points.Handle(), sampled);
    if (visible(masked) != 0) throw std::runtime_error("gpu_particles.sample_alpha");
    const auto unchanged = Capture(renderer, target.Handle(), points.Handle());
    if (unchanged.rgba_ != first.rgba_) throw std::runtime_error("gpu_particles.sample_mutation");
    step.reset_ = false;
    step.spawn_count_ = 0;
    step.seconds_ = 0.03f;
    Update(renderer, points.Handle(), step);
    Update(renderer, points.Handle(), step);
    auto expired = Capture(renderer, target.Handle(), points.Handle());
    if (std::ranges::any_of(expired.rgba_, [](auto v) { return v != 0; }))
        throw std::runtime_error("gpu_particles.expiration");
    step.seconds_ = 0;
    step.reset_ = true;
    step.spawn_count_ = 65;
    step.color_a_ = step.color_b_ = {0, 1, 0, 1};
    Update(renderer, points.Handle(), step);
    auto reset = Capture(renderer, target.Handle(), points.Handle());
    if (reset.rgba_[kCenter] != 0 || reset.rgba_[kCenter + 1] < 240)
        throw std::runtime_error("gpu_particles.reset_color");
    step.spawn_count_ = 0;
    Update(renderer, points.Handle(), step);
    auto clear = Capture(renderer, target.Handle(), points.Handle());
    if (std::ranges::any_of(clear.rgba_, [](auto v) { return v != 0; }))
        throw std::runtime_error("gpu_particles.reset_clear");
    step.spawn_start_ = 0;
    step.spawn_count_ = 1;
    step.center_ = {0.25f, 0.5f, 0};
    step.lifetime_ = 1;
    step.size_ = 0.05f;
    step.gravity_ = {8, 0};
    Update(renderer, points.Handle(), step);
    auto still = Capture(renderer, target.Handle(), points.Handle());
    step.reset_ = false;
    step.spawn_count_ = 0;
    step.seconds_ = 0.03f;
    renderer.BeginFrame();
    for (int i = 0; i < 6; ++i) renderer.UpdateGpuParticles(points.Handle(), step);
    renderer.EndFrame();
    auto moved = Capture(renderer, target.Handle(), points.Handle());
    const auto centroid = [](const render::ReadbackImage& image) {
        double sum = 0, weight = 0;
        for (std::size_t i = 0; i < image.rgba_.size(); i += 4) {
            weight += image.rgba_[i + 1];
            sum += double((i / 4) % 64) * image.rgba_[i + 1];
        }
        if (!weight) throw std::runtime_error("gpu_particles.motion_empty");
        return sum / weight;
    };
    if (centroid(moved) - centroid(still) < 7)
        throw std::runtime_error("gpu_particles.same_frame_compute_barrier");
    step.reset_ = true;
    step.center_ = {0.5f, 0.5f, 0};
    step.gravity_ = {};
    // Capacity-scale smoke: no CPU point uploads, one compute and one draw.
    for (const auto count : {10000U, 100000U, render::kMaximumGpuPoints}) {
        points = renderer.CreateGpuPoints(count);
        step.spawn_start_ = 0;
        step.spawn_count_ = count;
        step.radius_ = 0.4f;
        step.size_ = 0.001f;
        step.seconds_ = 1.0f / 60;
        step.flow_ = 0.2f;
        renderer.BeginFrame();
        renderer.UpdateGpuParticles(points.Handle(), step);
        renderer.SubmitGpuPoints(target.Handle(), points.Handle());
        const auto stats = renderer.Stats();
        if (stats.draws_ != 1 || stats.passes_ != 2 ||
            stats.gpu_point_bytes_ != std::uint64_t(count) * 64)
            throw std::runtime_error("gpu_particles.scale_accounting");
        renderer.EndFrame();
        auto image = Capture(renderer, target.Handle(), points.Handle());
        if (std::ranges::none_of(image.rgba_, [](auto v) { return v > 20; }))
            throw std::runtime_error("gpu_particles.scale_empty");
        std::cout << "gpu_particles count=" << count << " bytes=" << stats.gpu_point_bytes_
                  << " compute=1 draw=1 visible=yes\n";
    }
    std::cout << "gpu_particles spawn/wrap/pause/expiry/reset=pass\n";
}
}  // namespace rhythm::validation
