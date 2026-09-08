#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/player/session.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
double Scalar(const rhythm::runtime::FrameResult& frame, rhythm::graph::NodeId id) {
    const auto found = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                    [id](const auto& output) { return output.node_ == id; });
    if (found == frame.outputs_.end()) throw std::runtime_error("missing control output");
    return found->scalar_;
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "control.player";
        document.nodes_ = {
                registry.MakeNode(1, "control.scalar"),   registry.MakeNode(2, "core.time"),
                registry.MakeNode(3, "audio.feature"),    registry.MakeNode(4, "scalar.expression"),
                registry.MakeNode(5, "texture.gradient"), registry.MakeNode(6, "output.texture")};
        document.nodes_[0].properties_["value"] = 0.2;
        document.nodes_[3].properties_["expression"] =
                parameters::Expression("time * 0.01 + a + b");
        document.edges_ = {{1, 1, 4, "a"},
                           {2, 2, 4, "time"},
                           {3, 3, 4, "b"},
                           {4, 4, 5, "amount"},
                           {5, 5, 6, "source"}};
        document.output_ = 6;
        document.control_titles_[1] = "Energy";
        document.control_snapshots_ = {{1, "Quiet", {{1, 0}}}, {2, "Bright", {{1, 1}}}};
        const auto package = project::EncodePackage(document, "Macro test", {});
        player::Session session;
        session.Load(package);
        Require(session.Controls().Definitions().size() == 1,
                "published control metadata reaches Player");
        auto renderer = render::Renderer::CreateNull();
        runtime::ExternalInputs inputs;
        inputs.audio_.emplace();
        inputs.audio_->valid_ = true;
        inputs.audio_->sample_rate_ = 48000;
        inputs.audio_->generation_ = 1;
        inputs.audio_->loudness_ = 0.1f;
        const auto tick = [&](double seconds, bool suspended = false) {
            renderer.BeginFrame();
            auto result = session.Tick(seconds, suspended, {64, 64}, renderer, inputs);
            renderer.EndFrame();
            return result;
        };
        auto frame = tick(0);
        Require(Scalar(frame, 1) == 0.2, "unassigned macro uses saved default");
        frame = tick(1);
        const auto bytes = renderer.Stats().texture_bytes_;
        session.SetPaused(true);
        const auto seconds = session.Seconds();
        inputs.controls_[1] = 0.8;
        inputs.audio_->loudness_ = 0.9f;
        frame = tick(5);
        Require(session.Seconds() == seconds && Scalar(frame, 1) == 0.8 &&
                        std::abs(Scalar(frame, 3) - 0.1) < 1e-6 &&
                        std::abs(Scalar(frame, 4) - (0.9 + seconds * 0.01)) < 1e-6,
                "paused macros update visuals while music snapshot and time stay frozen");
        const auto version = frame.outputs_[0].version_;
        Require(tick(6).outputs_[0].version_ == version, "unchanged paused controls reuse result");
        inputs.controls_ = session.Controls().Blend(session.Controls().Snapshot(1),
                                                    session.Controls().Snapshot(2), 0.35);
        frame = tick(7);
        Require(Scalar(frame, 1) == 0.35 && renderer.Stats().texture_bytes_ == bytes,
                "snapshot blend updates uniforms without texture growth");
        session.ReleaseGraphics();
        Require(renderer.Stats().texture_bytes_ == 0, "surface loss releases resources");
        frame = tick(8);
        Require(Scalar(frame, 1) == 0.35 && session.Seconds() == seconds,
                "public values survive surface recreation through host snapshot");
        inputs.controls_[1] = 2;
        bool rejected = false;
        try {
            (void)tick(9);
        } catch (const std::exception&) {
            rejected = true;
            renderer.EndFrame();
        }
        Require(rejected, "out-of-range public control rejects before rendering");
        session.Load(package);
        inputs.controls_.clear();
        Require(Scalar(tick(10), 1) == 0.2, "new package resets to authored defaults");
        std::cout << "Public controls: package, macros, paused music, snapshots, cache and surface "
                     "recovery passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
