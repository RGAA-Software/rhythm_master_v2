#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "rhythm/runtime/runtime.h"

namespace rhythm::validation {
namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback& ticket) {
    for (int frame = 0; frame < 16; ++frame) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("event readback timeout");
}
int Brightness(const render::ReadbackImage& image) {
    int maximum = 0;
    for (std::size_t index = 0; index < image.rgba_.size(); index += 4)
        maximum = std::max({maximum, int(image.rgba_[index]), int(image.rgba_[index + 1]),
                            int(image.rgba_[index + 2])});
    return maximum;
}
}  // namespace
void VerifyEventPixels(render::Renderer& renderer) {
    graph::Registry registry;
    graph::Document envelope;
    envelope.id_ = "gpu-event-envelope";
    envelope.beat_grid_ = parameters::BeatSettings{};
    envelope.nodes_ = {registry.MakeNode(1, "event.beat"), registry.MakeNode(2, "event.envelope"),
                       registry.MakeNode(3, "texture.gradient"),
                       registry.MakeNode(4, "output.texture")};
    envelope.nodes_[1].properties_["attack"] = 0.0;
    envelope.nodes_[1].properties_["decay"] = 0.0;
    envelope.nodes_[1].properties_["sustain"] = 1.0;
    envelope.nodes_[1].properties_["duration"] = 0.1;
    for (const auto key : {"color_a", "color_b"})
        envelope.nodes_[2].properties_[key] = graph::Color{1, 1, 1, 1};
    envelope.edges_ = {{1, 1, 2, "events"}, {2, 2, 3, "amount"}, {3, 3, 4, "source"}};
    envelope.output_ = 4;
    {
        runtime::Runtime runtime;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(envelope, registry));
        for (const auto [seconds, expected] :
             {std::pair{0.0, 64}, {0.49, 64}, {0.5, 255}, {0.9, 64}}) {
            renderer.BeginFrame();
            const auto frame = runtime.Evaluate(plan, {seconds, 0, {64, 64}}, renderer);
            auto ticket = renderer.RequestReadback(frame.final_);
            renderer.EndFrame();
            const auto image = Complete(renderer, ticket);
            Check(std::abs(Brightness(image) - expected) <= 2,
                  "event envelope did not change actual GPU pixels at its timestamps");
        }
    }
    for (const auto history_type : {"texture.trail", "texture.feedback"}) {
        graph::Document document;
        document.id_ = history_type;
        document.beat_grid_ = parameters::BeatSettings{};
        document.nodes_ = {
                registry.MakeNode(1, "texture.gradient"), registry.MakeNode(2, history_type),
                registry.MakeNode(3, "texture.composite"), registry.MakeNode(4, "output.texture"),
                registry.MakeNode(5, "event.beat")};
        document.nodes_[1].properties_["trail_half_life"] = 1.0;
        if (std::string_view(history_type) == "texture.feedback")
            document.nodes_[1].properties_.erase("trail_half_life");
        document.nodes_[2].properties_["composite_mode"] = 1.0;
        document.nodes_[2].properties_["amount"] = 1.0;
        // Readback accepts the SDR output, not the trail's floating history.
        document.nodes_[2].properties_["texture_precision"] = 1.0;
        const bool feedback = std::string_view(history_type) == "texture.feedback";
        document.edges_ = {{1, 1, 3, "a"},
                           {2, 2, 3, "b"},
                           {3, feedback ? 3u : 1u, 2, "source"},
                           {4, 3, 4, "source"},
                           {5, 5, 2, "reset"}};
        document.output_ = 4;
        runtime::Runtime targeted, independent;
        for (int index = 0; index <= 30; ++index) {
            for (const auto key : {"color_a", "color_b"})
                document.nodes_[0].properties_[key] =
                        index == 0 ? graph::Color{1, 1, 1, 1} : graph::Color{0, 0, 0, 1};
            auto baseline = document;
            baseline.edges_.pop_back();
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
            const auto reference =
                    std::get<graph::ExecutionPlan>(graph::Compile(baseline, registry));
            renderer.BeginFrame();
            runtime::FrameContext context{index / 60.0, 0, {64, 64}};
            const auto first = targeted.Evaluate(plan, context, renderer);
            const auto second = independent.Evaluate(reference, context, renderer);
            render::Readback a, b;
            if (index >= 29) {
                a = renderer.RequestReadback(first.final_);
                b = renderer.RequestReadback(second.final_);
            }
            renderer.EndFrame();
            if (index >= 29) {
                const auto actual = Complete(renderer, a);
                const auto retained = Complete(renderer, b);
                Check(Brightness(retained) > 80, "history reference did not retain earlier pixels");
                if (index == 29)
                    Check(actual.rgba_ == retained.rgba_, "history changed before reset event");
                else
                    Check(Brightness(actual) <= 2, "targeted reset retained old history pixels");
            }
        }
    }
    {
        graph::Document document;
        document.id_ = "gpu-particle-event-reset";
        document.beat_grid_ = parameters::BeatSettings{};
        document.nodes_ = {
                registry.MakeNode(1, "gpu.particles"), registry.MakeNode(2, "gpu.render"),
                registry.MakeNode(3, "output.texture"), registry.MakeNode(4, "event.beat")};
        document.nodes_[0].properties_["particle_capacity"] = 128.0;
        document.nodes_[0].properties_["initial_fill"] = 0.0;
        document.nodes_[0].properties_["point_size"] = 0.03;
        for (const auto key : {"color_a", "color_b"})
            document.nodes_[0].properties_[key] = graph::Color{1, 1, 1, 1};
        document.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "source"}, {3, 4, 1, "reset"}};
        document.output_ = 3;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        runtime::Runtime runtime;
        for (int index = 0; index <= 30; ++index) {
            renderer.BeginFrame();
            const auto frame = runtime.Evaluate(plan, {index / 60.0, 0, {64, 64}}, renderer);
            if (index >= 29)
                std::cout << "GPU frame=" << index << " evaluated=" << frame.evaluated_
                          << " passes=" << renderer.Stats().passes_ << '\n';
            render::Readback ticket;
            if (index >= 29) ticket = renderer.RequestReadback(frame.final_);
            renderer.EndFrame();
            if (index >= 29) {
                const auto image = Complete(renderer, ticket);
                std::cout << "GPU reset frame=" << index << " peak=" << Brightness(image) << '\n';
                Check(index == 29 ? Brightness(image) > 20 : Brightness(image) <= 2,
                      "GPU particle event reset failed actual pixel clearing");
            }
        }
    }
    std::cout << "Event GPU pixels: timed ADSR, isolated feedback/trail reset and GPU particle "
                 "clearing passed\n";
}
}  // namespace rhythm::validation
