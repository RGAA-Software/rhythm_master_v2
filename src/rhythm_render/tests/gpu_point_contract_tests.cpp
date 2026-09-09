#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

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
    Require(rejected, "invalid GPU point operation accepted");
}
void Run() {
    using namespace rhythm::render;
    auto renderer = Renderer::CreateNull();
    auto other = Renderer::CreateNull();
    auto points = renderer.CreateGpuPoints(1024);
    const auto stale = points.Handle();
    Require(renderer.IsValid(stale) && !other.IsValid(stale), "device-scoped point handle");
    Require(renderer.Stats().gpu_point_bytes_ == 65536 && renderer.Stats().gpu_point_buffers_ == 1,
            "fixed-layout allocation participates in the point memory budget");
    auto target = renderer.CreateTexture({16, 16});
    GpuParticleStep step;
    Reject([&] { renderer.UpdateGpuParticles(stale, step); });
    renderer.BeginFrame();
    Reject([&] { renderer.UpdateGpuParticles(stale, step); });
    Reject([&] { renderer.SubmitGpuPoints(target.Handle(), stale); });
    step.reset_ = true;
    step.spawn_count_ = 1024;
    renderer.UpdateGpuParticles(stale, step);
    renderer.SubmitGpuPoints(target.Handle(), stale);
    const std::array<std::uint8_t, 4> blue{0, 0, 255, 255};
    auto map = renderer.CreateTexture({1, 1}, blue);
    GpuPointStyle sampled;
    sampled.sampling_ = GpuPointSampling{map.Handle(), 1, 1};
    renderer.SubmitGpuPoints(target.Handle(), stale, sampled);
    Reject([&] { renderer.UpdateTexture(map.Handle(), blue); });
    sampled.sampling_->color_amount_ = -1;
    Reject([&] { renderer.SubmitGpuPoints(target.Handle(), stale, sampled); });
    sampled.sampling_->color_amount_ = 1;
    sampled.sampling_->texture_ = target.Handle();
    Reject([&] { renderer.SubmitGpuPoints(target.Handle(), stale, sampled); });
    auto foreign = other.CreateTexture({1, 1}, blue);
    sampled.sampling_->texture_ = foreign.Handle();
    Reject([&] { renderer.SubmitGpuPoints(target.Handle(), stale, sampled); });
    sampled.sampling_->texture_ = map.Handle();
    map = {};
    Reject([&] { renderer.SubmitGpuPoints(target.Handle(), stale, sampled); });
    step.spawn_count_ = 1025;
    Reject([&] { renderer.UpdateGpuParticles(stale, step); });
    step.spawn_count_ = 0;
    step.seconds_ = 1;
    Reject([&] { renderer.UpdateGpuParticles(stale, step); });
    step.seconds_ = 0;
    step.center_[0] = std::nanf("");
    Reject([&] { renderer.UpdateGpuParticles(stale, step); });
    renderer.EndFrame();
    auto moved = std::move(points);
    Require(!renderer.IsValid(points.Handle()) && renderer.IsValid(moved.Handle()),
            "ownership move");
    moved = {};
    Require(!renderer.IsValid(stale) && renderer.Stats().gpu_point_bytes_ == 0,
            "release invalidates observer");
    auto replacement = renderer.CreateGpuPoints(1024);
    Require(replacement.Handle().generation_ != stale.generation_, "slot reuse changes generation");
    Reject([&] { renderer.CreateGpuPoints(0); });
    Reject([&] { renderer.CreateGpuPoints(kMaximumGpuPoints + 1); });
    replacement = {};
    std::vector<GpuPoints> large;
    for (int i = 0; i < 4; ++i) large.push_back(renderer.CreateGpuPoints(kMaximumGpuPoints));
    Reject([&] { renderer.CreateGpuPoints(1); });
    large.clear();
    auto invalidated = renderer.CreateGpuPoints(1);
    renderer.Invalidate();
    Require(!renderer.IsValid(invalidated.Handle()),
            "device invalidation rejects retained observers");
    Reject([&] { renderer.CreateGpuPoints(1); });
    std::cout << "GPU point contracts: initialization, frame order, generations, memory and loss "
                 "passed\n";
}
}  // namespace
int main() {
    try {
        Run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
