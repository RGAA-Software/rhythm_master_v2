#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

int main() {
    using namespace rhythm;
    try {
        const auto require = [](bool condition) {
            if (!condition) throw std::runtime_error("audio.runtime_contract");
        };
        graph::Registry registry;
        graph::Document document;
        document.id_ = "audio.input";
        document.output_ = 4;
        document.nodes_ = {
                registry.MakeNode(1, "audio.feature"), registry.MakeNode(2, "audio.band"),
                registry.MakeNode(3, "texture.gradient"), registry.MakeNode(4, "output.texture")};
        document.edges_ = {{1, 1, 3, "amount"}, {2, 3, 4, "source"}};
        const std::array<graph::NodeId, 1> viewers{2};
        const auto plan =
                std::get<graph::ExecutionPlan>(graph::Compile(document, registry, viewers));
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        runtime::FrameContext context;
        const auto evaluate = [&] {
            renderer.BeginFrame();
            auto result = runtime.Evaluate(plan, context, renderer);
            renderer.EndFrame();
            return result;
        };
        const auto scalar = [](const runtime::FrameResult& result, graph::NodeId id) {
            for (const auto& node : result.outputs_)
                if (node.node_ == id) return node.scalar_;
            throw std::runtime_error("audio.missing_node");
        };
        require(scalar(evaluate(), 1) == 0);
        audio::Features frame;
        frame.valid_ = true;
        frame.generation_ = 1;
        frame.sample_rate_ = 48000;
        frame.loudness_ = 0.75;
        frame.mono_bands_[20] = 0.5;
        context.external_.audio_ = frame;
        const auto changed = evaluate();
        require(scalar(changed, 1) == 0.75 && scalar(changed, 2) == 0.5);
        require(evaluate().evaluated_ == 0);
        context.external_.audio_->mono_bands_[19] = 0.2f;
        require(evaluate().evaluated_ == 0);
        context.external_.audio_->loudness_ = 0.25;
        require(scalar(evaluate(), 1) == 0.25);
        context.external_.audio_.reset();
        require(scalar(evaluate(), 1) == 0);
        context.external_.audio_ = frame;
        context.external_.audio_->rms_ = std::numeric_limits<float>::quiet_NaN();
        bool rejected = false;
        try {
            evaluate();
        } catch (const std::invalid_argument&) {
            rejected = true;
            renderer.EndFrame();
        }
        require(rejected);
        std::cout << "Audio runtime passed: typed input, missing source, scalar/band consumption, "
                     "incremental cache and invalid-frame rejection\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
