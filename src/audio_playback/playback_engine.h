#pragma once

#include <chrono>
#include <functional>

#include "playback_presentation.h"
#include "rhythm/audio/output.h"
#include "rhythm/audio/playback.h"

namespace rhythm::audio::detail {
// One serialized worker owns this engine. Commands/mailbox/thread lifetime stay
// in FilePlayback; this component owns device, PCM lanes and consumption mapping.
class PlaybackEngine final {
   public:
    PlaybackEngine(const PlaybackSource& source, StreamOptions options, std::uint64_t generation,
                   std::uint64_t first_sample, std::stop_token stop);
    void Step(bool paused, float volume, bool loop, std::stop_token stop,
              const std::function<std::uint64_t()>& next_generation);
    void Begin(const PlaybackSource& source, StreamOptions options, std::uint64_t duration,
               media::CrossfadeCurve curve, std::stop_token stop);
    bool Cancel();
    PlaybackSnapshot Snapshot() const { return state_; }

   private:
    void Submit(std::stop_token stop, const std::function<std::uint64_t()>& next_generation);
    void Observe(std::uint64_t heard, bool ended);
    void Recover(std::string error, std::uint64_t boundary);
    using Clock = std::chrono::steady_clock;
    std::unique_ptr<TransitionStream> stream_{};
    std::unique_ptr<OutputDevice> device_{};
    std::unique_ptr<PlaybackPresentation> presentation_{};
    PlaybackSnapshot state_{};
    StreamIdentity submitted_identity_{};
    std::uint64_t submitted_generation_ = 0;
    std::uint64_t decoded_end_ = 0;
    std::uint64_t recovery_frame_ = 0;
    StreamOptions incoming_options_{};
    bool ended_input_ = false;
    bool last_loop_intent_ = false;
    std::optional<Clock::time_point> drain_started_{};
};
}  // namespace rhythm::audio::detail
