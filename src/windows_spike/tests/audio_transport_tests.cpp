#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/audio_ui/audio_panel.h"
#include "rhythm/player/session.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
// Actual FFmpeg/device playback with a Null graph backend. The same immutable
// audio snapshot supplies both analysis and the Player presentation clock.
int main(int argc, char* argv[]) {
    using namespace rhythm;
    using namespace std::chrono_literals;
    try {
        Check(argc == 2, "audio_transport <16-second demo wav>");
        graph::Registry registry;
        graph::Document document;
        document.id_ = "music.transport";
        document.output_ = 3;
        document.nodes_ = {registry.MakeNode(1, "core.time"),
                           registry.MakeNode(2, "texture.gradient"),
                           registry.MakeNode(3, "output.texture")};
        document.edges_ = {{1, 1, 2, "amount"}, {2, 2, 3, "source"}};
        player::Session session;
        session.Load(project::EncodePackage(document, "Music transport"));
        auto renderer = render::Renderer::CreateNull();
        audio_ui::AudioPanel audio;
        audio.SetVolume(0);
        audio.LoadFile(argv[1]);
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t evaluated = 0;
        const auto tick = [&] {
            const auto input = audio.Frame();
            Check(input.playback_.has_value(), "selected music keeps a transport source");
            runtime::ExternalInputs external;
            external.audio_ = input.features_;
            const auto seconds =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            renderer.BeginFrame();
            const auto result =
                    session.Tick(seconds, false, {16, 16}, renderer, external, input.playback_);
            renderer.EndFrame();
            Check(session.Seconds() == input.playback_->seconds_, "audio and graph clock differ");
            if (!result.outputs_.empty() && result.outputs_[0].scalar_ != session.Seconds())
                std::cerr << "graph=" << result.outputs_[0].scalar_
                          << " media=" << session.Seconds() << " paused=" << session.Paused()
                          << " frames=" << evaluated << '\n';
            Check(!result.outputs_.empty() && result.outputs_[0].scalar_ == session.Seconds(),
                  "core.time did not follow the media position");
            if (input.features_)
                Check(input.features_->generation_ == input.playback_->generation_,
                      "old analysis survived a transport generation");
            ++evaluated;
            return input;
        };
        const auto wait = [&](const std::function<bool(const audio_ui::AudioInputFrame&)>& ready) {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (std::chrono::steady_clock::now() < deadline) {
                const auto frame = tick();
                if (ready(frame)) return frame;
                std::this_thread::sleep_for(5ms);
            }
            throw std::runtime_error("music transport wait timed out");
        };
        wait([](const auto& frame) { return frame.features_ && frame.playback_->seconds_ > 0.05; });
        audio.ApplyPlayback({true, {}});
        std::this_thread::sleep_for(60ms);
        const auto paused = tick();
        std::this_thread::sleep_for(100ms);
        Check(tick().playback_->seconds_ == paused.playback_->seconds_ && session.Paused(),
              "pause must hold music and graph");
        audio.ApplyPlayback({{}, 4});
        Check(tick().playback_->seconds_ == 4 && session.Paused(), "paused seek");
        audio.ApplyPlayback({false, {}});
        wait([](const auto& frame) { return frame.features_ && frame.playback_->seconds_ > 4.05; });
        audio.ApplyPlayback({true, {}});
        for (int index = 0; index < 20; ++index) audio.ApplyPlayback({{}, index / 10.0});
        audio.ApplyPlayback({{}, 4.5});
        std::this_thread::sleep_for(60ms);
        Check(tick().playback_->seconds_ == 4.5 && session.Paused(), "latest seek wins");
        audio.ApplyPlayback({false, 15.9});
        wait([](const auto& frame) {
            return frame.playback_->paused_ && frame.playback_->seconds_ == 16;
        });
        audio.ApplyPlayback({false, 0});
        const auto restarted = wait([](const auto& frame) {
            return frame.features_ && !frame.playback_->paused_ && frame.playback_->seconds_ > 0;
        });
        Check(restarted.playback_->seconds_ < 1, "restart after EOF");
        std::cout << "audio transport frames=" << evaluated
                  << " shared PCM/time, pause, paused seek, rapid seek, EOF and restart passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
