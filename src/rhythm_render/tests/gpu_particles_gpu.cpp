#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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
void Mapping(render::Renderer& renderer) {
    auto source = renderer.CreateGpuPoints(65);
    auto first = renderer.CreateGpuPoints(65);
    auto second = renderer.CreateGpuPoints(65);
    auto reference = renderer.CreateGpuPoints(65);
    auto target = renderer.CreateTexture({128, 128});
    render::GpuParticleStep step;
    step.reset_ = true;
    step.spawn_count_ = 1;
    step.center_ = {.25f, .5f, 0};
    step.radius_ = step.speed_ = step.flow_ = 0;
    step.size_ = .2f;
    step.color_a_ = step.color_b_ = {1, 1, 1, 1};
    Update(renderer, source.Handle(), step);
    const auto original = Capture(renderer, target.Handle(), source.Handle());
    for (const auto shift : {.125f, -.0625f}) {
        render::GpuPointMapping mapping;
        mapping.transform_[12] = shift;
        mapping.color_ = {.5f, 1, .25f, 1};
        mapping.size_ = .5f;
        renderer.BeginFrame();
        renderer.MapGpuPoints(source.Handle(), first.Handle(), mapping);
        renderer.MapGpuPoints(first.Handle(), second.Handle(), mapping);
        renderer.EndFrame();
        auto expected_step = step;
        expected_step.center_[0] += shift * 2;
        expected_step.size_ *= .25f;
        expected_step.color_a_ = expected_step.color_b_ = {.25f, 1, .0625f, 1};
        Update(renderer, reference.Handle(), expected_step);
        const auto actual = Capture(renderer, target.Handle(), second.Handle());
        const auto expected = Capture(renderer, target.Handle(), reference.Handle());
        std::uint64_t energy = 0;
        for (std::size_t i = 0; i < actual.rgba_.size(); ++i) {
            energy += actual.rgba_[i];
            if (std::abs(int(actual.rgba_[i]) - int(expected.rgba_[i])) > 2)
                throw std::runtime_error("gpu_point_mapping.reference_pixels");
        }
        if (energy < 100) throw std::runtime_error("gpu_point_mapping.empty_result");
        if (Capture(renderer, target.Handle(), source.Handle()).rgba_ != original.rgba_)
            throw std::runtime_error("gpu_point_mapping.source_mutation");
    }
    const auto retained = Capture(renderer, target.Handle(), second.Handle());
    source = {};
    first = {};
    if (Capture(renderer, target.Handle(), second.Handle()).rgba_ != retained.rgba_)
        throw std::runtime_error("gpu_point_mapping.output_lifetime");
    std::cout << "GPU point mapping public API: two-stage pixels, changed parameters, "
                 "unchanged input and independent output lifetime passed\n";
}
void SoftParticleFalloff(render::Renderer& renderer) {
    auto target = renderer.CreateTexture({64, 64});
    auto points = renderer.CreateGpuPoints(1);
    render::GpuParticleStep step;
    step.reset_ = true;
    step.spawn_count_ = 1;
    step.center_ = {.5f, .5f, 0};
    step.radius_ = step.speed_ = step.flow_ = 0;
    step.size_ = .3f;
    step.color_a_ = step.color_b_ = {1, 1, 1, 1};
    Update(renderer, points.Handle(), step);
    const auto image = Capture(renderer, target.Handle(), points.Handle());
    const auto red = [&](std::size_t x, std::size_t y) { return image.rgba_[(y * 64 + x) * 4]; };
    const auto center = red(32, 32);
    const auto middle = red(40, 32);
    const auto edge = red(47, 32);
    std::cout << "gpu_particles soft falloff center=" << int(center) << " middle=" << int(middle)
              << " edge=" << int(edge) << '\n';
    if (!(center > middle && middle > edge)) throw std::runtime_error("gpu_particles.soft_falloff");
    render::GpuPointStyle local_glow;
    local_glow.glow_radius_ = 2.0f;
    const auto expanded = Capture(renderer, target.Handle(), points.Handle(), local_glow);
    const auto expanded_inner = expanded.rgba_[(32 * 64 + 36) * 4];
    const auto expanded_middle = expanded.rgba_[(32 * 64 + 40) * 4];
    const auto expanded_outer = expanded.rgba_[(32 * 64 + 44) * 4];
    const auto expanded_edge = expanded.rgba_[(32 * 64 + 47) * 4];
    if (expanded_edge <= edge) throw std::runtime_error("gpu_particles.local_glow_extent");
    if (!(expanded_inner > expanded_middle && expanded_middle > expanded_outer &&
          expanded_outer > expanded_edge))
        throw std::runtime_error("gpu_particles.local_glow_smooth_falloff");
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
void AtlasShapeVariation(render::Renderer& renderer) {
    auto target = renderer.CreateTexture({64, 64});
    auto points = renderer.CreateGpuPoints(1);
    render::GpuParticleStep step;
    step.reset_ = true;
    step.spawn_count_ = 1;
    step.center_ = {.5f, .5f, 0};
    step.radius_ = step.speed_ = step.flow_ = 0;
    step.size_ = .5f;
    step.color_a_ = step.color_b_ = {1, 1, 1, 1};
    Update(renderer, points.Handle(), step);
    const auto red = [](const render::ReadbackImage& image, std::size_t x, std::size_t y) {
        return image.rgba_[(y * 64 + x) * 4];
    };
    const auto analytic = Capture(renderer, target.Handle(), points.Handle());
    if (red(analytic, 32, 32) < 200) throw std::runtime_error("gpu_point_atlas.baseline");
    // A hollow ring cell modulates the analytic envelope: center dark, ring bright.
    std::vector<std::uint8_t> ring_pixels(16 * 16 * 4, 0);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const float dx = x - 7.5f, dy = y - 7.5f;
            const float r = std::sqrt(dx * dx + dy * dy);
            if (r > 2.5f && r < 7.9f) {
                const auto offset = (y * 16 + x) * 4;
                ring_pixels[offset] = ring_pixels[offset + 1] = ring_pixels[offset + 2] =
                        ring_pixels[offset + 3] = 255;
            }
        }
    auto ring = renderer.CreateTexture({16, 16}, ring_pixels);
    render::GpuPointStyle style;
    style.atlas_ = render::GpuPointAtlas{ring.Handle(), 1, 1};
    const auto ringed = Capture(renderer, target.Handle(), points.Handle(), style);
    if (red(ringed, 32, 32) > 30) throw std::runtime_error("gpu_point_atlas.shape_center");
    // The spawn size random varies the sprite extent; 8px stays inside every
    // possible quad and maps onto the ring band.
    if (red(ringed, 40, 32) < 60) throw std::runtime_error("gpu_point_atlas.shape_ring");
    // Texture v=0 must be the sprite's canvas-top: only the top-left quadrant glows.
    std::vector<std::uint8_t> quadrant_pixels(16 * 16 * 4, 0);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const auto offset = (y * 16 + x) * 4;
            quadrant_pixels[offset] = quadrant_pixels[offset + 1] = quadrant_pixels[offset + 2] =
                    quadrant_pixels[offset + 3] = 255;
        }
    auto quadrant = renderer.CreateTexture({16, 16}, quadrant_pixels);
    style.atlas_ = render::GpuPointAtlas{quadrant.Handle(), 1, 1};
    const auto oriented = Capture(renderer, target.Handle(), points.Handle(), style);
    int bright = 0;
    for (std::size_t y = 0; y < 64; ++y)
        for (std::size_t x = 0; x < 64; ++x) {
            if (red(oriented, x, y) <= 40) continue;
            ++bright;
            if (x >= 38 || y >= 38) throw std::runtime_error("gpu_point_atlas.orientation");
        }
    if (bright < 20) throw std::runtime_error("gpu_point_atlas.orientation_empty");
    // Two cells, one solid and one empty: the stable spawn random retires half
    // of an identical crowd relative to a single solid cell.
    std::vector<std::uint8_t> solid_pixels(16 * 16 * 4, 255);
    auto solid = renderer.CreateTexture({16, 16}, solid_pixels);
    std::vector<std::uint8_t> pair_pixels(32 * 16 * 4, 0);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const auto offset = (y * 32 + x) * 4;
            pair_pixels[offset] = pair_pixels[offset + 1] = pair_pixels[offset + 2] =
                    pair_pixels[offset + 3] = 255;
        }
    auto pair = renderer.CreateTexture({32, 16}, pair_pixels);
    auto crowd = renderer.CreateGpuPoints(128);
    render::GpuParticleStep spawn;
    spawn.reset_ = true;
    spawn.spawn_count_ = 128;
    spawn.center_ = {.5f, .5f, 0};
    spawn.radius_ = .3f;
    spawn.speed_ = spawn.flow_ = 0;
    spawn.lifetime_ = 100;
    spawn.size_ = .01f;
    spawn.color_a_ = spawn.color_b_ = {1, 1, 1, 1};
    Update(renderer, crowd.Handle(), spawn);
    const auto lit = [](const render::ReadbackImage& image) {
        return std::count_if(image.rgba_.begin(), image.rgba_.end(),
                             [](auto byte) { return byte > 40; });
    };
    render::GpuPointStyle one_cell;
    one_cell.atlas_ = render::GpuPointAtlas{solid.Handle(), 1, 1};
    const auto full = lit(Capture(renderer, target.Handle(), crowd.Handle(), one_cell));
    render::GpuPointStyle two_cells;
    two_cells.atlas_ = render::GpuPointAtlas{pair.Handle(), 2, 1};
    const auto halved = lit(Capture(renderer, target.Handle(), crowd.Handle(), two_cells));
    std::cout << "gpu_point_atlas full=" << full << " two_cells=" << halved << '\n';
    if (full < 100 || halved > full * 3 / 4 || halved < full / 4)
        throw std::runtime_error("gpu_point_atlas.cell_selection");
}
void SoftDepthIntersection(render::Renderer& renderer) {
    if (!renderer.SupportsSampleableDepth()) {
        std::cout << "gpu_point soft depth unsupported on this device/profile\n";
        return;
    }
    const render::Extent extent{64, 64};
    const std::array<render::MeshVertex, 4> vertices{
            {{-20, -20, 0}, {20, -20, 0}, {20, 20, 0}, {-20, 20, 0}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    auto color = renderer.CreateTexture(extent);
    auto depth = renderer.CreateDepthTexture(extent);
    auto target = renderer.CreateTexture(extent);
    // Orthographic near=1 far=11; a plane at distance 6 covers the whole canvas.
    render::SceneDrawList scene;
    scene.projection_[0] = scene.projection_[5] = 0.1f;
    scene.projection_[10] = -0.2f;
    scene.projection_[14] = -1.2f;
    render::MeshDraw plane;
    plane.mesh_ = mesh.Handle();
    plane.model_[14] = -6;
    plane.double_sided_ = true;
    scene.draws_ = {plane};
    auto points = renderer.CreateGpuPoints(1);
    render::GpuParticleStep step;
    step.reset_ = true;
    step.spawn_count_ = 1;
    step.center_ = {.5f, .5f, 0};
    step.radius_ = step.speed_ = step.flow_ = 0;
    step.lifetime_ = 100;
    step.size_ = .5f;
    step.color_a_ = step.color_b_ = {1, 1, 1, 1};
    const auto capture_soft = [&](const render::GpuPointStyle& style) {
        renderer.BeginFrame();
        renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), scene);
        renderer.SubmitGpuPoints(target.Handle(), points.Handle(), style);
        auto ticket = renderer.RequestReadback(target.Handle());
        renderer.EndFrame();
        for (int i = 0; i < 32; ++i) {
            if (auto image = ticket.Poll()) return std::move(*image);
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        throw std::runtime_error("gpu_point_soft.readback_timeout");
    };
    render::GpuPointSoftDepth soft{depth.Handle(), 1, 11, true, 1.0f};
    Update(renderer, points.Handle(), step);
    const auto baseline = capture_soft({});
    constexpr std::size_t kCenter = (32 * 64 + 32) * 4;
    if (baseline.rgba_[kCenter] < 200) throw std::runtime_error("gpu_point_soft.baseline");
    render::GpuPointStyle style;
    style.soft_depth_ = soft;
    // Well in front of the plane: identical to the analytic-only baseline.
    step.center_[2] = 3;
    Update(renderer, points.Handle(), step);
    if (capture_soft(style).rgba_ != baseline.rgba_)
        throw std::runtime_error("gpu_point_soft.in_front");
    // Behind the scene surface: fully faded.
    step.center_[2] = 8;
    Update(renderer, points.Handle(), step);
    const auto hidden = capture_soft(style);
    if (std::ranges::any_of(hidden.rgba_, [](auto v) { return v != 0; }))
        throw std::runtime_error("gpu_point_soft.behind");
    // Half a fade distance in front: alpha halves across the band.
    step.center_[2] = 5.5f;
    Update(renderer, points.Handle(), step);
    const auto faded = capture_soft(style);
    const auto expected = baseline.rgba_[kCenter] / 2;
    if (std::abs(int(faded.rgba_[kCenter]) - expected) > 6)
        throw std::runtime_error("gpu_point_soft.band." +
                                 std::to_string(faded.rgba_[kCenter]));
    std::cout << "gpu_point_soft front=identical behind=hidden band="
              << int(faded.rgba_[kCenter]) << "/" << int(baseline.rgba_[kCenter]) << '\n';
}
}  // namespace
void VerifyGpuParticles(render::Renderer& renderer) {
    if (!renderer.SupportsGpuPoints()) {
        std::cout << "gpu_particles unsupported on this device/profile\n";
        return;
    }
    Mapping(renderer);
    SoftParticleFalloff(renderer);
    SamplingOrientation(renderer);
    AtlasShapeVariation(renderer);
    SoftDepthIntersection(renderer);
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
