#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "scene.sampling";
        document.nodes_ = {
                registry.MakeNode(1, "geometry.sphere"), registry.MakeNode(2, "scene.instance"),
                registry.MakeNode(3, "scene.render"), registry.MakeNode(4, "output.texture")};
        document.edges_ = {{1, 1, 2, "geometry"}, {2, 2, 3, "scene"}, {3, 3, 4, "source"}};
        document.output_ = 4;
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        runtime::FrameContext frame;
        frame.extent_ = {128, 64};
        auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        const auto evaluate = [&] {
            renderer.BeginFrame();
            const auto result = runtime.Evaluate(plan, frame, renderer);
            renderer.EndFrame();
            return result;
        };
        const auto original = evaluate();
        Check(renderer.IsValid(original.final_) && renderer.Stats().passes_ == 1,
              "default scene render changed its single-pass behavior");
        const auto original_bytes = renderer.Stats().texture_bytes_;
        document.nodes_[2].properties_["scene_antialiasing"] = 1.0;
        plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        const auto sampled = evaluate();
        Check(renderer.IsValid(sampled.final_) && renderer.Stats().passes_ == 2,
              "supersampling did not render and resolve");
        const auto sampled_bytes = renderer.Stats().texture_bytes_;
        Check(sampled_bytes > original_bytes && !renderer.IsValid(original.final_),
              "sampling switch retained obsolete scene attachments");
        Check(evaluate().evaluated_ == 0 && renderer.Stats().passes_ == 0 &&
                      renderer.Stats().texture_bytes_ == sampled_bytes,
              "unchanged supersampled scene reallocated or redrew");
        document.nodes_[2].properties_["scene_antialiasing"] = 0.0;
        plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        evaluate();
        Check(renderer.Stats().texture_bytes_ == original_bytes &&
                      !renderer.IsValid(sampled.final_),
              "disabling sampling retained high-resolution color/depth");
        document.nodes_[2].properties_["scene_antialiasing"] = 1.0;
        document.nodes_[2].properties_["texture_precision"] = 2.0;
        plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        frame.extent_ = {64, 128};
        const auto portrait = evaluate();
        Check(renderer.Precision(portrait.final_) == render::TexturePrecision::kFloat16,
              "portrait supersampling lost requested precision");
        runtime.Reset();
        Check(renderer.Stats().texture_bytes_ == 0 && renderer.Stats().live_meshes_ == 0,
              "supersampling resources leaked on reset");
        frame.extent_ = {3840, 2160};
        renderer.BeginFrame();
        const auto over_budget = runtime.EvaluateSafely(plan, frame, renderer);
        renderer.EndFrame();
        Check(over_budget.budget_.has_value() && !renderer.IsValid(over_budget.final_),
              "oversized Float16 sampling silently bypassed the texture budget");
        runtime.Reset();
        frame.extent_ = {128, 64};
        Check(renderer.IsValid(evaluate().final_),
              "sampling failed to recover after admission error");
        runtime.Reset();
        Check(renderer.Stats().texture_bytes_ == 0, "failed sampling leaked resources");
        runtime.BeginPreparation(plan, frame);
        runtime::PreparationProgress progress;
        for (int step = 0; step < 4; ++step) {
            renderer.BeginFrame();
            progress = runtime.PrepareNext(renderer, {1, 100});
            renderer.EndFrame();
            Check(progress.state_ != runtime::PreparationState::kFailed,
                  "staged sampling preparation failed");
        }
        Check(progress.state_ == runtime::PreparationState::kReady &&
                      progress.required_passes_ == 2 && progress.output_ &&
                      renderer.IsValid(progress.output_->final_),
              "staged preparation omitted sampling resolve or exposed incomplete output");
        runtime.Reset();
        Check(renderer.Stats().texture_bytes_ == 0, "staged sampling leaked resources");
        std::cout << "Scene supersampling: default, resolve, cache, precision, resize and budgets "
                     "passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
