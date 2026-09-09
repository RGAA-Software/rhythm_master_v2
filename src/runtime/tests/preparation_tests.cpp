#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
graph::ExecutionPlan Chain(std::uint64_t count = 42) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "prepared-chain";
    document.nodes_ = {registry.MakeNode(1, "core.time"), registry.MakeNode(2, "texture.gradient")};
    document.edges_ = {{1, 1, 2, "amount"}};
    for (std::uint64_t id = 3; id <= count; ++id) {
        document.nodes_.push_back(registry.MakeNode(id, "texture.transform"));
        document.edges_.push_back({id, id - 1, id, "source"});
    }
    document.nodes_.push_back(registry.MakeNode(count + 1, "output.texture"));
    document.edges_.push_back({count + 1, count, count + 1, "source"});
    document.output_ = count + 1;
    return std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
}
void Run() {
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    auto plan = Chain();
    runtime::FrameContext context{7, 1, {32, 32}, false};
    runtime.BeginPreparation(plan, context);
    plan.instructions_.clear();
    context.seconds_ = 99;
    std::size_t previous = 0;
    unsigned steps = 0;
    runtime::PreparationProgress ready;
    for (;;) {
        renderer.BeginFrame();
        const auto progress = runtime.PrepareNext(renderer, {3, 100});
        renderer.EndFrame();
        ++steps;
        Require(progress.state_ != runtime::PreparationState::kFailed &&
                        progress.completed_nodes_ > previous &&
                        progress.completed_nodes_ - previous <= 3 && progress.total_nodes_ == 43,
                "bounded steps own a fixed plan and make monotonic progress");
        previous = progress.completed_nodes_;
        if (progress.state_ == runtime::PreparationState::kReady) {
            ready = progress;
            break;
        }
        Require(!progress.output_, "partial results never expose render handles");
        if (steps == 1) {
            bool rejected = false;
            try {
                runtime.Evaluate(Chain(), {}, renderer);
            } catch (const std::logic_error&) {
                rejected = true;
            }
            Require(rejected, "ordinary frame cannot overwrite in-flight preparation");
        }
    }
    Require(steps == 15 && ready.output_ && renderer.IsValid(ready.output_->final_) &&
                    ready.output_->outputs_.front().scalar_ == 7,
            "completed output uses captured time, not number of preparation frames or mutated "
            "input");
    runtime.Reset();
    Require(renderer.Stats().texture_bytes_ == 0, "completed resources released");

    runtime.BeginPreparation(Chain(), {0, 2, {32, 32}, false});
    renderer.BeginFrame();
    Require(runtime.PrepareNext(renderer, {3, 100}).state_ == runtime::PreparationState::kPending,
            "partial preparation exists to cancel");
    renderer.EndFrame();
    Require(renderer.Stats().texture_bytes_ > 0, "preparation owns actual resources");
    runtime.Reset();
    Require(renderer.Stats().texture_bytes_ == 0, "cancellation releases partial resources");
    runtime.BeginPreparation(Chain(), {0, 2, {32, 32}, false});
    renderer.BeginFrame();
    (void)runtime.PrepareNext(renderer, {3, 100});
    renderer.EndFrame();
    auto replacement = render::Renderer::CreateNull();
    replacement.BeginFrame();
    const auto lost = runtime.PrepareNext(replacement, {3, 100});
    replacement.EndFrame();
    Require(lost.state_ == runtime::PreparationState::kFailed &&
                    lost.error_ == "runtime.preparation_device_changed" &&
                    renderer.Stats().texture_bytes_ == 0,
            "renderer replacement rejects stale partial handles and releases old resources");
    runtime.BeginPreparation(Chain(250), {0, 3, {16, 16}, false});
    runtime::PreparationProgress excessive;
    do {
        renderer.BeginFrame();
        excessive = runtime.PrepareNext(renderer, {8, 100});
        renderer.EndFrame();
    } while (excessive.state_ == runtime::PreparationState::kPending);
    Require(excessive.state_ == runtime::PreparationState::kFailed &&
                    excessive.budget_ == render::Budget::kPasses && !excessive.output_ &&
                    renderer.Stats().texture_bytes_ == 0,
            "splitting work over frames cannot bypass the ordinary scene pass budget");

    auto current = renderer.CreateTexture({32, 32});
    {
        auto pressure = renderer.CreateTexture({8192, 8190});
        const auto baseline = renderer.Stats().texture_bytes_;
        runtime.BeginPreparation(Chain(), {0, 3, {64, 64}, false});
        runtime::PreparationProgress failure;
        for (int step = 0; step < 43; ++step) {
            renderer.BeginFrame();
            failure = runtime.PrepareNext(renderer, {1, 100});
            renderer.EndFrame();
            if (failure.state_ == runtime::PreparationState::kFailed) break;
        }
        Require(failure.state_ == runtime::PreparationState::kFailed && failure.budget_ &&
                        !failure.output_ && renderer.IsValid(current.Handle()) &&
                        renderer.Stats().texture_bytes_ == baseline,
                "shared budget failure releases candidate and keeps accepted resources");
    }
}
void Videos() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "staged-video";
    const assets::AssetId id{std::string(64, 'a')};
    document.nodes_ = {registry.MakeNode(1, "texture.video"), registry.MakeNode(2, "texture.video"),
                       registry.MakeNode(3, "texture.blend"),
                       registry.MakeNode(4, "output.texture")};
    document.nodes_[0].properties_["asset"] = id;
    document.nodes_[1].properties_["asset"] = id;
    document.edges_ = {{1, 1, 3, "a"}, {2, 2, 3, "b"}, {3, 3, 4, "source"}};
    document.output_ = 4;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto frame = std::make_shared<media::VideoFrame>();
    frame->info_.width_ = frame->info_.height_ = 1;
    frame->rgba_ = {255, 0, 0, 255};
    runtime::FrameContext context{0, 1, {16, 16}, false};
    context.videos_ = {{1, id, frame, 1, 1, 1}, {2, id, frame, 1, 1, 1}};
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime.BeginPreparation(plan, context);
    for (unsigned step = 1; step <= 4; ++step) {
        renderer.BeginFrame();
        const auto progress = runtime.PrepareNext(renderer, {1, 100});
        Require(progress.completed_nodes_ == step &&
                        progress.state_ != runtime::PreparationState::kFailed,
                "video graph progresses one node at a time");
        if (step == 1)
            Require(renderer.Stats().live_textures_ == 3,
                    "first step uploads only first video, white and its render target");
        if (step == 2)
            Require(renderer.Stats().live_textures_ == 5,
                    "second video is uploaded in a later host frame");
        renderer.EndFrame();
    }
    runtime.Reset();
    Require(renderer.Stats().texture_bytes_ == 0, "staged video resources released");
}
}  // namespace
int main() {
    try {
        Run();
        Videos();
        std::cout << "Runtime preparation: bounded steps, captured inputs, hidden partial output, "
                     "cancellation, shared resource failure and staged video upload passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
