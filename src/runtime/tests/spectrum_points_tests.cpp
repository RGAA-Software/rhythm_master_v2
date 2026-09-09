#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "spectrum_points.h"
#include "texture_lifetimes.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Sampling() {
    graph::Registry registry;
    graph::Instruction instruction{
            registry.MakeNode(1, "point.spectrum"), graph::Operation::kSpectrumPoints, {0}};
    instruction.node_.properties_["point_count"] = 3.0;
    instruction.node_.properties_["band_first"] = 0.0;
    instruction.node_.properties_["band_last"] = 1.0;
    instruction.node_.properties_["spectrum_layout"] = 0.0;
    std::array<runtime::NodeOutput, 1> inputs{};
    inputs[0].scalar_ = 1;
    runtime::ExternalInputs external;
    external.audio_.emplace();
    external.audio_->valid_ = true;
    external.audio_->mono_bands_[1] = 1;
    auto points = runtime::detail::SpectrumPoints(instruction, inputs, external);
    Require(points->size() == 3 && points->front().x_ == 0 && points->back().x_ == 1 &&
                    points->front().y_ == .5f && points->back().y_ == .25f &&
                    points->at(1).y_ == .375f,
            "ordered log-band endpoints and interpolated middle map into canvas coordinates");
    Require(points->front().color_ != points->back().color_, "magnitude drives point color");
    instruction.node_.properties_["audio_channel"] = 1.0;
    external.audio_->left_bands_[0] = 1;
    external.audio_->left_bands_[1] = 0;
    auto left = runtime::detail::SpectrumPoints(instruction, inputs, external);
    Require(left->front().y_ == .25f && left->back().y_ == .5f,
            "selected stereo channel changes sampling source");
    inputs[0].scalar_ = std::numeric_limits<double>::quiet_NaN();
    Require(runtime::detail::SpectrumPoints(instruction, inputs, external)->front().y_ == .5f,
            "nonfinite driven gain has a finite baseline");
    inputs[0].scalar_ = 1;
    instruction.node_.properties_["spectrum_layout"] = 1.0;
    external.audio_->valid_ = false;
    const auto radial = runtime::detail::SpectrumPoints(instruction, inputs, external);
    Require(std::abs(radial->front().x_ - .5f) < 1e-6 &&
                    std::abs(radial->front().y_ - .3f) < 1e-6 &&
                    radial->front().id_ != radial->back().id_ &&
                    radial->front().x_ != radial->back().x_,
            "no audio keeps radial baseline without duplicating seam point");
    Require(points->at(1).y_ == .375f, "previous point snapshot is immutable");
}
void RuntimeGraph() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "spectrum.path.runtime";
    document.nodes_ = {
            registry.MakeNode(1, "point.spectrum"), registry.MakeNode(2, "path.from_points"),
            registry.MakeNode(3, "texture.path_fill"), registry.MakeNode(4, "texture.affine"),
            registry.MakeNode(5, "output.texture")};
    document.nodes_[1].properties_["path_closed"] = 1.0;
    document.edges_ = {
            {1, 1, 2, "points"}, {2, 2, 3, "path"}, {3, 3, 4, "source"}, {4, 4, 5, "source"}};
    document.output_ = 5;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    runtime::detail::TextureLifetimes lifetimes;
    lifetimes.Prepare(plan, std::vector<graph::NodeId>{});
    Require(lifetimes.Recyclable(2),
            "unobserved dynamic vector output participates in texture reuse");
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::FrameContext context{0, 0, {128, 128}};
    context.external_.audio_.emplace();
    context.external_.audio_->valid_ = true;
    context.external_.audio_->generation_ = 1;
    context.external_.audio_->sample_rate_ = 48000;
    context.external_.audio_->mono_bands_.fill(.2f);
    const auto evaluate = [&] {
        renderer.BeginFrame();
        const auto result = runtime.Evaluate(plan, context, renderer);
        renderer.EndFrame();
        return result;
    };
    const auto initial = evaluate();
    Require(renderer.IsValid(initial.final_) && initial.outputs_[1].path_->closed_,
            "spectrum points to closed path to vector fill produces a texture");
    Require(evaluate().evaluated_ == 0, "unchanged audio reuses point and path snapshots");
    context.external_.audio_->mono_bands_.fill(.8f);
    const auto changed = evaluate();
    Require(changed.outputs_[0].points_->front().y_ < initial.outputs_[0].points_->front().y_ &&
                    changed.outputs_[0].points_generation_ ==
                            initial.outputs_[0].points_generation_ &&
                    changed.outputs_[1].path_ != initial.outputs_[1].path_,
            "audio updates geometry while sample identities remain stable");
    runtime.Reset();
    Require(renderer.Stats().live_textures_ == 0, "reset releases bridge render resources");
    auto excessive = plan;
    excessive.instructions_[0].node_.properties_["point_count"] = 513.0;
    Require(graph::ValidatePointBudget(excessive).has_value(), "point bridge admission is bounded");
}
}  // namespace
int main() {
    try {
        Sampling();
        RuntimeGraph();
        std::cout << "Spectrum interpolation, channels, immutable points, path bridge and reuse "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
