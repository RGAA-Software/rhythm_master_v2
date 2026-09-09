#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "rhythm/render/budget.h"
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
void Mapping() {
    using namespace rhythm::render;
    auto renderer = Renderer::CreateNull();
    auto other = Renderer::CreateNull();
    auto source = renderer.CreateGpuPoints(65);
    auto first = renderer.CreateGpuPoints(65);
    auto second = renderer.CreateGpuPoints(65);
    auto wrong_size = renderer.CreateGpuPoints(64);
    auto foreign = other.CreateGpuPoints(65);
    auto target = renderer.CreateTexture({16, 16});
    Reject([&] { renderer.MapGpuPoints(source.Handle(), first.Handle()); });
    renderer.BeginFrame();
    Reject([&] { renderer.MapGpuPoints(source.Handle(), first.Handle()); });
    Reject([&] { renderer.SubmitGpuPoints(target.Handle(), first.Handle()); });
    GpuParticleStep step;
    step.reset_ = true;
    step.spawn_count_ = 65;
    renderer.UpdateGpuParticles(source.Handle(), step);
    Reject([&] { renderer.MapGpuPoints(source.Handle(), source.Handle()); });
    Reject([&] { renderer.MapGpuPoints(source.Handle(), wrong_size.Handle()); });
    Reject([&] { renderer.MapGpuPoints(source.Handle(), foreign.Handle()); });
    Reject([&] { renderer.MapGpuPoints(foreign.Handle(), first.Handle()); });
    GpuPointMapping mapping;
    for (const auto invalid : {std::nanf(""), 17.0f, -17.0f}) {
        mapping.transform_[0] = invalid;
        Reject([&] { renderer.MapGpuPoints(source.Handle(), first.Handle(), mapping); });
    }
    mapping = {};
    mapping.transform_[3] = .1f;
    Reject([&] { renderer.MapGpuPoints(source.Handle(), first.Handle(), mapping); });
    mapping = {};
    for (const auto invalid : {std::nanf(""), 1.1f, -.1f}) {
        mapping.color_[3] = invalid;
        Reject([&] { renderer.MapGpuPoints(source.Handle(), first.Handle(), mapping); });
    }
    mapping = {};
    for (const auto invalid : {std::nanf(""), 17.0f, -.1f}) {
        mapping.size_ = invalid;
        Reject([&] { renderer.MapGpuPoints(source.Handle(), first.Handle(), mapping); });
    }
    Reject([&] { renderer.SubmitGpuPoints(target.Handle(), first.Handle()); });
    Require(renderer.Stats().passes_ == 1, "failed maps consumed passes");
    renderer.MapGpuPoints(source.Handle(), first.Handle());
    renderer.MapGpuPoints(first.Handle(), second.Handle());
    renderer.SubmitGpuPoints(target.Handle(), second.Handle());
    renderer.EndFrame();
    const auto stale = first.Handle();
    first = {};
    first = renderer.CreateGpuPoints(65);
    renderer.BeginFrame();
    Reject([&] { renderer.MapGpuPoints(stale, second.Handle()); });
    Reject([&] { renderer.MapGpuPoints(source.Handle(), stale); });
    Reject([&] { renderer.MapGpuPoints(first.Handle(), second.Handle()); });
    for (std::uint32_t i = 0; i < kMaximumOffscreenPasses; ++i)
        renderer.MapGpuPoints(source.Handle(), second.Handle());
    Reject([&] { renderer.MapGpuPoints(source.Handle(), first.Handle()); });
    renderer.EndFrame();
    renderer.BeginFrame();
    Reject([&] { renderer.MapGpuPoints(first.Handle(), second.Handle()); });
    renderer.MapGpuPoints(source.Handle(), first.Handle());
    renderer.EndFrame();
    renderer.Invalidate();
    Reject([&] { renderer.MapGpuPoints(source.Handle(), second.Handle()); });
    std::cout << "GPU point map contracts: alias, capacity, initialization, parameters, "
                 "generations and pass rejection passed (Null backend)\n";
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
        Mapping();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
