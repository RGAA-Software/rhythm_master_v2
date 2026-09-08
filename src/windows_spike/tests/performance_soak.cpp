#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/audio/playback.h"
#include "rhythm/platform/host.h"
#include "rhythm/player/session.h"

// Opt-in real-duration diagnostic: actual audio output (muted after analysis),
// music-clocked graph and D3D presentation. Not an Android application test.
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 3) throw std::invalid_argument("performance_soak music-package seconds");
        const auto duration = std::stoi(argv[2]);
        if (duration < 60 || duration > 3600) throw std::invalid_argument("soak duration 60..3600");
        std::cout << std::unitbuf;
        platform::Host host(true);
        host.Resize({1280, 720});
        auto renderer = host.CreateRenderer();
        player::Session session;
        session.Open(argv[1]);
        const auto soundtrack = session.Soundtrack();
        if (!soundtrack) throw std::runtime_error("soak requires packaged music");
        audio::FilePlayback playback;
        playback.SetVolume(0);
        playback.SetLoop(true);
        if (soundtrack->file_bytes_.Valid())
            playback.Load(soundtrack->file_bytes_);
        else
            playback.Load(soundtrack->bytes_);
        const auto source_generation = playback.Snapshot().generation_;
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        std::uint64_t frames = 0, loops = 0, generation = 0, since_loop = 0;
        std::uint64_t baseline = 0, peak = 0, consumed = 0, measured = 0;
        std::array<std::uint64_t, 2001> histogram{};
        bool observed_audio = false;
        int reported = -1;
        double song_duration = 0;
        for (;;) {
            const auto begin = Clock::now();
            const auto elapsed = std::chrono::duration<double>(begin - start).count();
            if (elapsed >= duration) break;
            if (!host.Poll()) throw std::runtime_error("soak host closed");
            const auto state = playback.Snapshot();
            if (state.state_ == audio::PlaybackState::kFailed ||
                state.state_ == audio::PlaybackState::kEnded)
                throw std::runtime_error("soak audio stopped");
            if (state.features_) observed_audio |= state.features_->rms_ > 0.001F;
            if (state.duration_seconds_) song_duration = *state.duration_seconds_;
            if (elapsed > 10 && (!observed_audio || state.source_generation_ != source_generation))
                throw std::runtime_error("soak source acknowledgment/audio");
            if (state.consumed_frames_ < consumed || state.queued_frames_ > 24000 ||
                state.submitted_frames_ < state.consumed_frames_ ||
                state.submitted_frames_ - state.consumed_frames_ > 32768)
                throw std::runtime_error("soak audio queue/counter regression");
            consumed = state.consumed_frames_;
            if (generation != state.generation_) {
                if (generation) ++loops;
                generation = state.generation_;
                since_loop = 0;
            }
            runtime::ExternalInputs inputs;
            inputs.audio_ = state.features_;
            runtime::PlaybackSample sample{
                    state.position_seconds_, state.generation_,
                    state.paused_ || state.state_ != audio::PlaybackState::kPlaying,
                    state.duration_seconds_};
            renderer.BeginFrame();
            const auto output = session.Tick(elapsed, false, {1280, 720}, renderer, inputs, sample);
            if (output.budget_ || !renderer.IsValid(output.final_))
                throw std::runtime_error("soak graph budget/output");
            render::DrawList draw;
            draw.width_ = 1280;
            draw.height_ = 720;
            draw.vertices_ = {{0, 0, 0, 0}, {1280, 0, 1, 0}, {1280, 720, 1, 1}, {0, 720, 0, 1}};
            draw.indices_ = {0, 1, 2, 0, 2, 3};
            draw.commands_ = {{output.final_, 0, 6, {0, 0, 1280, 720}}};
            renderer.Submit({}, draw);
            renderer.EndFrame();
            const auto bytes = renderer.Stats().texture_bytes_;
            peak = std::max(peak, bytes);
            if (elapsed > 30 && since_loop > 120) {
                if (!baseline) baseline = bytes;
                if (bytes != baseline) throw std::runtime_error("soak settled texture growth");
                const auto ms =
                        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
                ++histogram[std::min<std::size_t>(2000,
                                                  static_cast<std::size_t>(std::ceil(ms * 10)))];
                ++measured;
            }
            if (static_cast<int>(elapsed) / 60 != reported) {
                reported = static_cast<int>(elapsed) / 60;
                std::cout << "seconds=" << elapsed << " frames=" << frames << " loops=" << loops
                          << " texture_bytes=" << bytes << " consumed_frames=" << consumed << '\n';
            }
            ++frames;
            ++since_loop;
        }
        if (!observed_audio || !baseline || song_duration <= 0 ||
            loops + 2 < static_cast<std::uint64_t>(duration / song_duration))
            throw std::runtime_error("soak did not sustain music loops");
        const auto percentile = [&](double fraction) {
            std::uint64_t cumulative = 0;
            for (std::size_t bucket = 0; bucket < histogram.size(); ++bucket) {
                cumulative += histogram[bucket];
                if (cumulative >= std::ceil(measured * fraction)) return bucket / 10.0;
            }
            return 200.0;
        };
        playback.Stop();
        session.ReleaseGraphics();
        for (int frame = 0; frame < 6; ++frame) {
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        if (renderer.Stats().texture_bytes_ != 0)
            throw std::runtime_error("soak retains graph textures");
        std::cout << "completed_seconds=" << duration << " frames=" << frames << " loops=" << loops
                  << " extent=1280x720 stable_texture_bytes=" << baseline
                  << " peak_texture_bytes=" << peak << " frame_p50_ms=" << percentile(0.5)
                  << " frame_p95_ms=" << percentile(0.95) << " frame_ge_200ms=" << histogram.back()
                  << " released_texture_bytes=0\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
