#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "external.test";
        document.output_ = 5;
        document.nodes_ = {
                registry.MakeNode(1, "session.time"), registry.MakeNode(2, "participant.role"),
                registry.MakeNode(3, "participant.control"),
                registry.MakeNode(4, "texture.gradient"), registry.MakeNode(5, "output.texture")};
        document.edges_ = {{1, 3, 4, "amount"}, {2, 4, 5, "source"}};
        const std::array<graph::NodeId, 2> viewers{1, 2};
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        runtime::FrameContext context{3};
        const auto check = [](bool condition) {
            if (!condition) throw std::runtime_error("external.contract");
        };
        const auto evaluate = [&] {
            const auto plan =
                    std::get<graph::ExecutionPlan>(graph::Compile(document, registry, viewers));
            renderer.BeginFrame();
            const auto result = runtime.Evaluate(plan, context, renderer);
            renderer.EndFrame();
            return result;
        };
        const auto value = [](const runtime::FrameResult& frame, graph::NodeId node) {
            for (const auto& output : frame.outputs_)
                if (output.node_ == node) return output.scalar_;
            throw std::runtime_error("external.missing_node");
        };
        check(value(evaluate(), 1) == 3);
        context.external_.session_seconds_ = 10;
        context.external_.participant_.index_ = 7;
        context.external_.participant_.group_ = 2;
        context.external_.participant_.controls_[0] = 0.75;
        const auto received = evaluate();
        check(value(received, 1) == 10 && value(received, 2) == 7 && value(received, 3) == 0.75);
        context.external_.participant_.controls_[31] = 1;
        check(evaluate().evaluated_ == 0);
        context.seconds_ = 100;
        check(evaluate().evaluated_ == 0);
        document.nodes_[1].properties_["role_value"] = 1.0;
        check(value(evaluate(), 2) == 2);
        document.nodes_[0].properties_["session_value"] = 1.0;
        check(value(evaluate(), 1) == 1);
        context.external_.session_seconds_.reset();
        check(value(evaluate(), 1) == 0);
        context.seconds_ = 200;
        check(evaluate().evaluated_ == 0);
        const auto plan =
                std::get<graph::ExecutionPlan>(graph::Compile(document, registry, viewers));
        context.external_.participant_.controls_[0] = std::numeric_limits<double>::quiet_NaN();
        bool rejected = false;
        try {
            runtime.Evaluate(plan, context, renderer);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
        context.external_.participant_.controls_[0] = 0;
        context.external_.session_seconds_ = -1;
        rejected = false;
        try {
            runtime.Evaluate(plan, context, renderer);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
        std::cout << "external input contracts passed: local/session clock, roles, controls, "
                     "precise invalidation\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
