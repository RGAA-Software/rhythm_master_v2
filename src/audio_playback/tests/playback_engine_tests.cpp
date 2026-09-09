#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "playback_engine.h"
#if defined(__ANDROID__)
#include "audio_test_host.h"
#endif

namespace {
using namespace rhythm;
using namespace rhythm::audio;
using namespace rhythm::audio::detail;
using namespace std::chrono_literals;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
PlaybackSnapshot Wait(PlaybackEngine& engine, std::uint64_t& generation,
                      const std::function<bool(const PlaybackSnapshot&)>& predicate, bool loop) {
    const auto deadline = std::chrono::steady_clock::now() + 4s;
    while (std::chrono::steady_clock::now() < deadline) {
        engine.Step(false, 0, loop, {}, [&] { return ++generation; });
        const auto state = engine.Snapshot();
        Require(state.submitted_frames_ >= state.consumed_frames_ && state.queued_frames_ <= 24000,
                "one bounded device counter through handoff");
        if (predicate(state)) return state;
        std::this_thread::sleep_for(5ms);
    }
    throw std::runtime_error("audio.engine_timeout");
}
void Run(const std::filesystem::path& directory) {
    const PlaybackSource old = directory / "tone.flac";
    const PlaybackSource next = directory / "tone44100.wav";
    {
        std::uint64_t generation = 2;
        PlaybackEngine engine(old, {1, 0.8F, true}, 1, 0, {});
        engine.Begin(next, {2, 0.4F, true}, 12000, media::CrossfadeCurve::kLinear, {});
        for (int index = 0; index < 3; ++index) {
            engine.Step(true, 0, true, {}, [&] { return ++generation; });
            std::this_thread::sleep_for(15ms);
        }
        const auto paused = engine.Snapshot();
        Require(paused.transition_.state_ == AudioTransitionState::kQueued &&
                        paused.transition_.elapsed_frames_ == 0 && paused.consumed_frames_ == 0,
                "prepared fade freezes while paused");
        const auto mixed = Wait(
                engine, generation,
                [](const auto& state) {
                    return state.transition_.state_ == AudioTransitionState::kMixing &&
                           state.features_.has_value();
                },
                true);
        Require(mixed.transition_.previous_presented_ &&
                        mixed.transition_.previous_seconds_ == mixed.position_seconds_ &&
                        mixed.transition_.incoming_presented_ &&
                        mixed.transition_.elapsed_frames_ < 12000 &&
                        mixed.features_->generation_ == mixed.generation_,
                "heard mixed PCM has one FFT");
        const auto completed = Wait(
                engine, generation,
                [](const auto& state) {
                    return state.transition_.state_ == AudioTransitionState::kCompleted;
                },
                true);
        Require(completed.consumed_frames_ > completed.transition_.end_frame_ &&
                        completed.generation_ > 1 && completed.transition_.incoming_presented_ &&
                        completed.position_seconds_ == completed.transition_.incoming_seconds_,
                "device consumption confirms exact incoming clock and generation");
        bool rejected = false;
        try {
            engine.Begin(directory / "missing.flac", {++generation}, 100,
                         media::CrossfadeCurve::kLinear, {});
        } catch (const std::exception&) {
            rejected = true;
        }
        const auto continued = Wait(
                engine, generation,
                [&](const auto& state) {
                    return state.consumed_frames_ > completed.consumed_frames_;
                },
                true);
        Require(rejected && continued.state_ == PlaybackState::kPlaying &&
                        continued.transition_.state_ == AudioTransitionState::kCompleted,
                "preparation failure leaves current device and accepted scene intact");
    }
    {
        std::uint64_t generation = 12;
        PlaybackEngine engine(old, {10, 1, false}, 10, 0, {});
        engine.Begin(next, {11, 1, false}, 101, media::CrossfadeCurve::kLinear, {});
        engine.Step(false, 0, false, {}, [&] { return ++generation; });
        engine.Step(false, 0, false, {}, [&] { return ++generation; });
        Require(engine.Cancel(), "cancel during decode-ahead window");
        const auto canceled = Wait(
                engine, generation,
                [](const auto& state) {
                    return state.transition_.state_ == AudioTransitionState::kCanceled;
                },
                false);
        Require(canceled.transition_.error_.empty() &&
                        std::abs(canceled.position_seconds_ * 48000 -
                                 double(canceled.consumed_frames_)) < 1e-6,
                "cancellation recovers continuous old clock after queued PCM drains");
    }
    const assets::AssetId asset{std::string(64, 'a')};
    media::AudioClip clip{1, "Late invalid", asset};
    clip.timing_ = {0.12, 0.2, 0, 0.2};
    const auto bad = std::make_shared<const std::vector<std::uint8_t>>(32, std::uint8_t{0});
    const PlaybackSource broken = std::make_shared<const media::AudioArrangementSource>(
            media::AudioArrangementSource{media::AudioArrangement({clip}), {{asset, bad, {}}}});
    {
        std::uint64_t generation = 22;
        PlaybackEngine engine(old, {20, 1, true}, 20, 0, {});
        engine.Begin(broken, {21}, 24000, media::CrossfadeCurve::kLinear, {});
        const auto recovered = Wait(
                engine, generation,
                [](const auto& state) {
                    return state.transition_.state_ == AudioTransitionState::kFailed;
                },
                true);
        Require(!recovered.transition_.error_.empty() &&
                        recovered.state_ == PlaybackState::kPlaying && recovered.error_.empty(),
                "incoming failure does not fail current playback");
        const auto after = Wait(
                engine, generation,
                [&](const auto& state) {
                    return state.consumed_frames_ > recovered.consumed_frames_ + 4096 &&
                           state.features_.has_value();
                },
                true);
        Require(after.features_->rms_ > 0.01F, "current source remains audible after failed fade");
    }
    {
        std::uint64_t generation = 32;
        PlaybackEngine engine(old, {30}, 30, 0, {});
        engine.Begin(next, {31}, 60001, media::CrossfadeCurve::kLinear, {});
        const auto ended = Wait(
                engine, generation,
                [](const auto& state) { return state.state_ == PlaybackState::kEnded; }, false);
        Require(ended.consumed_frames_ == 60001 && ended.submitted_frames_ == 60001 &&
                        ended.transition_.state_ == AudioTransitionState::kCompleted &&
                        ended.generation_ > 30 && ended.transition_.incoming_presented_ &&
                        ended.position_seconds_ == ended.transition_.incoming_seconds_ &&
                        ended.duration_seconds_ == ended.position_seconds_,
                "short sources drain exact padded fade and publish terminal new source without "
                "extra PCM");
    }
}
}  // namespace
int main(int argc, char** argv) {
    try {
#if defined(__ANDROID__)
        rhythm::audio::test::InitializeNativeAudioTest();
#endif
        Require(argc == 2, "media fixture directory required");
        Run(std::filesystem::path(argv[1]));
        std::cout << "device engine: pause, mixed FFT, audible handoff, queued cancel, late "
                     "failure and short EOF passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
