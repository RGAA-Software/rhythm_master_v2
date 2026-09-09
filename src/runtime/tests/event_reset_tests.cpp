#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
rhythm::runtime::NodeOutput Output(const rhythm::runtime::FrameResult& frame,
                                   rhythm::graph::NodeId id) {
    for (const auto& output : frame.outputs_)
        if (output.node_ == id) return output;
    throw std::runtime_error("reset output missing");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        for (const bool gpu : {false, true}) {
            graph::Document document;
            document.id_ = "targeted-reset";
            document.beat_grid_ = parameters::BeatSettings{};
            document.nodes_ = {registry.MakeNode(1, gpu ? "gpu.particles" : "point.emitter"),
                               registry.MakeNode(2, gpu ? "gpu.render" : "point.render"),
                               registry.MakeNode(3, "output.texture"),
                               registry.MakeNode(4, "event.beat"),
                               registry.MakeNode(5, "point.emitter")};
            document.nodes_[0].properties_["particle_capacity"] = 128.0;
            document.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "source"}, {3, 4, 1, "reset"}};
            document.output_ = 3;
            const auto plan = std::get<graph::ExecutionPlan>(
                    graph::Compile(document, registry, std::array<graph::NodeId, 1>{5}));
            auto renderer = render::Renderer::CreateNull();
            runtime::Runtime runtime;
            const auto evaluate = [&](double seconds, bool advance = true) {
                renderer.BeginFrame();
                runtime::FrameContext frame{seconds};
                frame.advance_state_ = advance;
                auto output = runtime.Evaluate(plan, frame, renderer);
                renderer.EndFrame();
                Check(renderer.IsValid(output.final_), "reset lost the current output");
                return output;
            };
            auto frame = evaluate(0);
            for (int index = 1; index <= 29; ++index) frame = evaluate(index / 60.0);
            const auto target = Output(frame, 1);
            const auto independent = Output(frame, 5);
            if (!gpu)
                Check(target.points_ && !target.points_->empty(),
                      "particle fixture did not advance");
            frame = evaluate(0.5);
            Check(Output(frame, 5).points_generation_ == independent.points_generation_ &&
                          Output(frame, 5).points_->size() >= independent.points_->size(),
                  "targeted event reset unrelated emitter");
            if (gpu) {
                Check(Output(frame, 1).gpu_points_ != target.gpu_points_ &&
                              !renderer.IsValid(target.gpu_points_) &&
                              renderer.IsValid(Output(frame, 1).gpu_points_),
                      "GPU event reset did not reconstruct only its state");
            } else {
                Check(Output(frame, 1).points_->empty() &&
                              Output(frame, 1).points_generation_ != target.points_generation_,
                      "CPU event reset retained old particles/identity");
            }
            const auto reset = Output(frame, 1);
            frame = evaluate(0.5);
            Check(Output(frame, 1).gpu_points_ == reset.gpu_points_ &&
                          Output(frame, 1).points_generation_ == reset.points_generation_,
                  "same event reset state twice");
            frame = evaluate(0.5, false);
            Check(Output(frame, 1).gpu_points_ == reset.gpu_points_,
                  "pause reconstructed GPU state");
        }
        std::cout << "Targeted CPU/GPU event resets preserve unrelated state and deduplicate "
                     "repeated frames\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
