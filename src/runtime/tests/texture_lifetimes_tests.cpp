#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "rhythm/runtime/runtime.h"

namespace {
using namespace rhythm;
void Require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
const runtime::NodeOutput& Output(const runtime::FrameResult& result, graph::NodeId id) {
    const auto found = std::find_if(result.outputs_.begin(), result.outputs_.end(),
                                    [&](const auto& output) { return output.node_ == id; });
    if (found == result.outputs_.end()) throw std::runtime_error("missing node output");
    return *found;
}
graph::Document Chain() {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "texture.lifetimes";
    document.nodes_ = {registry.MakeNode(1, "core.time"), registry.MakeNode(2, "texture.gradient"),
                       registry.MakeNode(3, "texture.gradient")};
    document.edges_ = {{1, 1, 2, "amount"}};
    graph::NodeId previous = 2;
    for (graph::NodeId id = 4; id <= 83; ++id) {
        document.nodes_.push_back(registry.MakeNode(id, "texture.blend"));
        document.edges_.push_back({id * 2, previous, id, "a"});
        document.edges_.push_back({id * 2 + 1, 3, id, "b"});
        previous = id;
    }
    document.nodes_.push_back(registry.MakeNode(84, "output.texture"));
    document.output_ = 84;
    document.edges_.push_back({168, 83, 84, "source"});
    return document;
}
}  // namespace
int main() {
    try {
        graph::Registry registry;
        auto document = Chain();
        auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        runtime::FrameContext frame;
        frame.extent_ = {1024, 1024};
        frame.profile_nodes_ = true;
        frame.retained_textures_ = std::vector<graph::NodeId>{10};
        std::uint64_t static_version = 0;
        for (int index = 0; index < 8; ++index) {
            frame.seconds_ = index / 60.0;
            if (index == 4) frame.retained_textures_ = std::vector<graph::NodeId>{20};
            renderer.BeginFrame();
            const auto result = runtime.EvaluateSafely(plan, frame, renderer);
            Require(!result.budget_ && renderer.IsValid(result.final_), "large graph rejected");
            Require(result.recycled_textures_ >= 79, "dynamic targets not recycled");
            Require(renderer.IsValid(Output(result, index < 4 ? 10 : 20).texture_),
                    "observed intermediate was recycled");
            Require(Output(result, 2).texture_ == render::TextureHandle{},
                    "retired output exposed an aliased texture");
            Require(renderer.Stats().texture_bytes_ <= 28ULL * 1024 * 1024,
                    "dynamic graph did not fit its live-set budget");
            if (index == 0) static_version = Output(result, 3).version_;
            Require(Output(result, 3).version_ == static_version, "static cache was redrawn");
            std::uint32_t passes = 0;
            for (const auto& profile : result.profiles_) {
                Require(profile.node_ && profile.cpu_ms_ >= 0, "invalid node CPU profile");
                passes += profile.passes_;
            }
            Require(result.profiles_.size() == plan.instructions_.size() &&
                            passes == renderer.Stats().passes_,
                    "profile pass accounting mismatch");
            renderer.EndFrame();
        }
        frame.advance_state_ = false;
        for (int index = 0; index < 3; ++index) {
            if (index == 1) frame.retained_textures_ = std::vector<graph::NodeId>{2};
            renderer.BeginFrame();
            const auto result = runtime.EvaluateSafely(plan, frame, renderer);
            Require(renderer.IsValid(result.final_), "paused output missing");
            if (index == 1)
                Require(renderer.IsValid(Output(result, 2).texture_),
                        "newly pinned paused output was left retired");
            if (index == 2)
                Require(!result.evaluated_ && !renderer.Stats().passes_,
                        "paused graph re-executed evicted intermediates");
            renderer.EndFrame();
        }
        // A graph edit without revision change must invalidate the paused memo.
        document.edges_.back().from_ = 3;
        plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        renderer.BeginFrame();
        const auto compact = runtime.EvaluateSafely(plan, frame, renderer);
        Require(renderer.IsValid(compact.final_), "edited graph output missing");
        Require(renderer.Stats().texture_bytes_ == 1024ULL * 1024 * 4 + 4,
                "old graph retained idle pool resources");
        renderer.EndFrame();
        runtime.Reset();
        Require(!renderer.Stats().live_textures_, "reset leaked pooled resources");
        std::cout << "texture lifetime reuse, observation, static cache, pause and trim passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
