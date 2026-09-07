#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("signal.contract:" + std::to_string(location.line()));
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "signal.test";
        document.output_ = 8;
        document.nodes_ = {
                registry.MakeNode(1, "core.time"),        registry.MakeNode(2, "signal.noise"),
                registry.MakeNode(3, "signal.sample"),    registry.MakeNode(4, "scalar.map"),
                registry.MakeNode(5, "scalar.compare"),   registry.MakeNode(6, "scalar.select"),
                registry.MakeNode(7, "texture.gradient"), registry.MakeNode(8, "output.texture")};
        document.edges_ = {{1, 1, 2, "time"},  {2, 2, 3, "signal"},    {3, 3, 4, "value"},
                           {4, 4, 5, "a"},     {5, 5, 6, "condition"}, {6, 6, 7, "amount"},
                           {7, 7, 8, "source"}};
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        const auto evaluate = [&](double seconds) {
            const auto compiled = graph::Compile(document, registry);
            Check(std::holds_alternative<graph::ExecutionPlan>(compiled));
            renderer.BeginFrame();
            const auto frame =
                    runtime.Evaluate(std::get<graph::ExecutionPlan>(compiled), {seconds}, renderer);
            renderer.EndFrame();
            return frame;
        };
        const auto value = [](const runtime::FrameResult& frame, graph::NodeId id) {
            for (const auto& output : frame.outputs_)
                if (output.node_ == id) return output.scalar_;
            throw std::runtime_error("signal.missing");
        };
        const auto start = value(evaluate(0), 2);
        const auto end = value(evaluate(1), 2);
        Check(start == 33350994.0 / 4294967295.0 && end == 2767685996.0 / 4294967295.0);
        Check(start >= 0 && start <= 1 && end >= 0 && end <= 1 && start != end);
        Check(std::abs(value(evaluate(0.5), 2) - (start + end) * 0.5) < 1e-12);
        Check(value(evaluate(0), 2) == start);
        Check(evaluate(0).evaluated_ == 0);
        document.nodes_[1].properties_["noise_mode"] = 0.0;
        Check(value(evaluate(0.9), 2) == start);
        document.nodes_[1].properties_["noise_mode"] = 1.0;
        Check(std::abs(value(evaluate(0.25), 2) - (start * 0.75 + end * 0.25)) < 1e-12);
        document.nodes_[1].properties_["seed"] = 4294967295.0;
        Check(value(evaluate(0), 2) != start && std::isfinite(value(evaluate(1.0e300), 2)));
        document.nodes_[1].properties_["seed"] = 0.5;
        Check(!registry.ValidateNode(document.nodes_[1]).empty());
        document.nodes_[1].properties_["seed"] = 0.0;
        document.edges_[2].from_ = 1;  // Use a known time ramp for numerical boundary checks.
        document.nodes_[3].properties_["input_min"] = 1.0;
        document.nodes_[3].properties_["input_max"] = 3.0;
        document.nodes_[3].properties_["output_min"] = 10.0;
        document.nodes_[3].properties_["output_max"] = 20.0;
        Check(value(evaluate(0), 4) == 10 && value(evaluate(2), 4) == 15 &&
              value(evaluate(4), 4) == 20);
        document.nodes_[3].properties_["map_mode"] = 1.0;
        Check(value(evaluate(4), 4) == 25);
        document.nodes_[3].properties_["input_min"] = 3.0;
        document.nodes_[3].properties_["input_max"] = 1.0;
        Check(value(evaluate(2), 4) == 15);
        document.nodes_[3].properties_["input_max"] = 3.0;
        Check(value(evaluate(2), 4) == 10);
        document.edges_[3].from_ = 1;
        document.nodes_[4].properties_["b"] = 0.5;
        Check(value(evaluate(0.25), 6) == 0 && value(evaluate(0.75), 6) == 1);
        document.nodes_[4].properties_["compare_mode"] = 2.0;
        Check(value(evaluate(0.5000005), 5) == 1 && value(evaluate(0.500002), 5) == 0);
        document.nodes_[4].properties_["compare_mode"] = 3.0;
        Check(value(evaluate(0.5), 5) == 0);
        document.nodes_[4].properties_["compare_mode"] = 1.0;
        Check(value(evaluate(0.25), 5) == 1 && value(evaluate(0.75), 5) == 0);
        document.nodes_[4].properties_["compare_mode"] = 4.0;
        Check(value(evaluate(0.5), 5) == 1 && value(evaluate(0.25), 5) == 0);
        document.nodes_[4].properties_["compare_mode"] = 5.0;
        Check(value(evaluate(0.5), 5) == 1 && value(evaluate(0.75), 5) == 0);
        document.nodes_[4].properties_["compare_mode"] = 3.0;
        document.nodes_[5].properties_["a"] = -2.0;
        document.nodes_[5].properties_["b"] = 3.0;
        Check(value(evaluate(0.5), 6) == -2 && value(evaluate(0.75), 6) == 3);
        std::cout << "signal contracts passed: seeded noise, ranges, comparisons, selection, dirty "
                     "cache\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
