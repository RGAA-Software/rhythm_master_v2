#include "preparation_gpu_contracts.h"

#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace rhythm::validation {
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback& ticket) {
    for (int frame = 0; frame < 16; ++frame) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("prepared frame readback deadline");
}
}  // namespace
void VerifyPreparedPixels(render::Renderer& renderer) {
    const auto baseline = renderer.Stats().texture_bytes_;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "prepared-pixels";
    document.nodes_ = {registry.MakeNode(1, "core.time"), registry.MakeNode(2, "texture.gradient"),
                       registry.MakeNode(3, "texture.video"),
                       registry.MakeNode(4, "texture.blend")};
    const assets::AssetId asset{std::string(64, 'a')};
    document.nodes_[2].properties_["asset"] = asset;
    document.edges_ = {{1, 1, 2, "amount"}, {2, 2, 4, "a"}, {3, 3, 4, "b"}};
    for (graph::NodeId id = 5; id <= 20; ++id) {
        document.nodes_.push_back(registry.MakeNode(id, "texture.transform"));
        document.nodes_.back().properties_["scale"] = 0.99;
        document.edges_.push_back({id, id - 1, id, "source"});
    }
    document.nodes_.push_back(registry.MakeNode(21, "texture.feedback"));
    document.nodes_.push_back(registry.MakeNode(22, "texture.composite"));
    document.nodes_.back().properties_["composite_mode"] = 1.0;
    document.nodes_.back().properties_["amount"] = 0.5;
    document.nodes_.push_back(registry.MakeNode(23, "output.texture"));
    document.edges_.insert(
            document.edges_.end(),
            {{21, 22, 21, "source"}, {22, 20, 22, "a"}, {23, 21, 22, "b"}, {24, 22, 23, "source"}});
    document.output_ = 23;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto video = std::make_shared<media::VideoFrame>();
    video->info_.width_ = video->info_.height_ = 1;
    video->rgba_ = {40, 150, 70, 255};
    runtime::FrameContext context{0.5, 1, {64, 64}, false};
    context.advance_state_ = false;
    context.retained_textures_ = std::vector<graph::NodeId>{};
    context.videos_ = {{3, asset, video, 1, 1, 1}};
    runtime::Runtime reference, prepared;
    renderer.BeginFrame();
    const auto direct = reference.Evaluate(plan, context, renderer);
    auto direct_ticket = renderer.RequestReadback(direct.final_);
    renderer.EndFrame();
    const auto expected = Complete(renderer, direct_ticket);
    prepared.BeginPreparation(plan, context);
    runtime::PreparationProgress progress;
    unsigned steps = 0;
    do {
        renderer.BeginFrame();
        progress = prepared.PrepareNext(renderer, {3, 100});
        Require(progress.state_ != runtime::PreparationState::kFailed && ++steps <= 23,
                "staged actual GPU execution failed");
        renderer.EndFrame();
    } while (progress.state_ == runtime::PreparationState::kPending);
    Require(steps > 1 && progress.output_ && progress.output_->recycled_textures_ > 10,
            "preparation spans frames and retains intermediate texture reuse");
    renderer.BeginFrame();
    auto prepared_ticket = renderer.RequestReadback(progress.output_->final_);
    renderer.EndFrame();
    const auto actual = Complete(renderer, prepared_ticket);
    Require(actual.rgba_ == expected.rgba_,
            "prepared GPU output differs from one frozen ordinary evaluation");
    bool visible = false;
    for (std::size_t i = 0; i < actual.rgba_.size(); i += 4)
        visible |= actual.rgba_[i] + actual.rgba_[i + 1] + actual.rgba_[i + 2] > 30;
    Require(visible, "comparison cannot pass on two empty outputs");
    for (int frame = 1; frame <= 3; ++frame) {
        context.seconds_ = 0.5 + frame * 0.1;
        context.advance_state_ = true;
        renderer.BeginFrame();
        const auto first = reference.Evaluate(plan, context, renderer);
        const auto second = prepared.Evaluate(plan, context, renderer);
        auto a = renderer.RequestReadback(first.final_);
        auto b = renderer.RequestReadback(second.final_);
        renderer.EndFrame();
        const auto first_image = Complete(renderer, a);
        const auto second_image = Complete(renderer, b);
        Require(first_image.rgba_ == second_image.rgba_,
                "preparation advanced feedback or changed subsequent live graph evaluation");
    }
    reference.Reset();
    prepared.Reset();
    Require(renderer.Stats().texture_bytes_ == baseline, "all prepared GPU resources released");
    std::cout << "Prepared GPU pixels: " << steps
              << " steps, staged video, dynamic texture reuse, "
                 "frozen feedback and subsequent live frames match ordinary evaluation\n";
}
}  // namespace rhythm::validation
