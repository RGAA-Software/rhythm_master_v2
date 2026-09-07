#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("scalar.contract");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "scalar.test";
        document.output_ = 6;
        document.nodes_ = {
                registry.MakeNode(1, "core.time"),        registry.MakeNode(2, "time.local"),
                registry.MakeNode(3, "scalar.constant"),  registry.MakeNode(4, "scalar.math"),
                registry.MakeNode(5, "texture.gradient"), registry.MakeNode(6, "output.texture")};
        document.edges_ = {{1, 1, 2, "time"},
                           {2, 2, 4, "a"},
                           {3, 3, 4, "b"},
                           {4, 4, 5, "amount"},
                           {5, 5, 6, "source"}};
        document.nodes_[1].properties_["time_mode"] = 1.0;
        document.nodes_[1].properties_["speed"] = 2.0;
        document.nodes_[1].properties_["offset"] = -1.0;
        document.nodes_[2].properties_["value"] = 2.0;
        document.nodes_[3].properties_["math_mode"] = 2.0;
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        const auto evaluate = [&](double seconds) {
            const auto result = graph::Compile(document, registry);
            Check(std::holds_alternative<graph::ExecutionPlan>(result));
            renderer.BeginFrame();
            const auto frame =
                    runtime.Evaluate(std::get<graph::ExecutionPlan>(result), {seconds}, renderer);
            renderer.EndFrame();
            return frame;
        };
        const auto value = [](const runtime::FrameResult& frame, graph::NodeId id) {
            for (const auto& output : frame.outputs_)
                if (output.node_ == id) return output.scalar_;
            throw std::runtime_error("scalar.missing_output");
        };
        Check(value(evaluate(0), 2) == 3);
        Check(value(evaluate(1), 4) == 2);
        Check(value(evaluate(2.5), 4) == 0);
        const auto unchanged = evaluate(2.5);
        Check(unchanged.evaluated_ == 0 && renderer.Stats().passes_ == 0);
        document.nodes_[1].properties_["time_mode"] = 2.0;
        Check(value(evaluate(3), 2) == 3);
        document.nodes_[3].properties_["math_mode"] = 3.0;
        document.nodes_[2].properties_["value"] = 0.0;
        Check(value(evaluate(3), 4) == 0);
        document.nodes_[1].properties_["speed"] = -1.0;
        Check(std::isfinite(value(evaluate(1.0e300), 2)));
        document.nodes_[3].properties_["math_mode"] = 0.5;
        Check(std::holds_alternative<std::vector<graph::Diagnostic>>(
                graph::Compile(document, registry)));
        document.nodes_[3].properties_["math_mode"] = 0.0;
        document.nodes_[1].properties_["speed"] = 1.0;
        document.nodes_[1].properties_["offset"] = 0.0;
        document.nodes_[1].properties_["time_mode"] = 0.0;
        document.nodes_.push_back(registry.MakeNode(7, "scalar.curve"));
        document.edges_[1].from_ = 7;
        document.edges_.push_back({6, 2, 7, "time"});
        Check(value(evaluate(0.5), 4) == 0.5);
        Check(value(evaluate(3), 4) == 1);
        document.nodes_.push_back(registry.MakeNode(8, "scalar.expression"));
        document.nodes_.back().properties_["a"] = 2.0;
        document.nodes_.back().properties_["expression"] =
                parameters::Expression("a + sin(time * tau)");
        document.edges_[3].from_ = 8;
        document.edges_.push_back({7, 1, 8, "time"});
        Check(std::abs(value(evaluate(0.25), 8) - 3) < 1e-12);
        Check(evaluate(0.25).evaluated_ == 0);
        document.edges_.pop_back();
        Check(value(evaluate(0.25), 8) == 2);
        document.edges_.push_back({7, 3, 8, "a"});
        document.nodes_[2].properties_["value"] = 7.0;
        Check(value(evaluate(0.25), 8) == 7);
        std::cout << "scalar/time/curve/expression contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
