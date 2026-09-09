#pragma once

#include "rhythm/media/crossfade.h"
#include "stream_source.h"

namespace rhythm::audio::detail {
struct StreamIdentity {
    std::uint64_t source_ = 0;
    std::uint64_t iteration_ = 0;
    bool operator==(const StreamIdentity&) const = default;
};
struct StreamPcm {
    std::vector<float> samples_{};
    StreamIdentity identity_{};
    std::uint64_t first_sample_ = 0;
};
struct StreamOptions {
    std::uint64_t source_id_ = 0;
    float gain_ = 1;
    bool loop_ = false;
};
enum class FadeState { kNone, kMixing, kSubmitted, kCanceled, kFailed };
struct FadeProgress {
    FadeState state_ = FadeState::kNone;
    std::uint64_t source_id_ = 0;
    std::uint64_t frames_ = 0;
    std::uint64_t duration_ = 0;
    std::uint64_t clipped_samples_ = 0;
    std::string error_{};
};
// Serialized audio worker only. Two bounded PCM lanes, one logical cursor pool;
// no device, thread, analysis or wall clock. Read never crosses a source/loop
// identity boundary. Submitted means PCM produced, NOT heard by the device.
// Caller owns audible markers and rollback after submission to a device queue.
class TransitionStream final {
   public:
    TransitionStream(const PlaybackSource& source, StreamOptions options,
                     std::stop_token stop = {});
    ~TransitionStream();
    TransitionStream(const TransitionStream&) = delete;
    TransitionStream& operator=(const TransitionStream&) = delete;
    // Strong preparation: failure leaves the current cursor and any existing
    // fade unchanged. Reject a second in-flight fade instead of dropping it.
    void Begin(const PlaybackSource& source, StreamOptions options, std::uint64_t duration,
               media::CrossfadeCurve curve, std::stop_token stop = {});
    std::optional<StreamPcm> Read(std::stop_token stop = {});
    // Restores the old lane at the first frame not yet produced, including the
    // device's decode-ahead period after the last mixed frame. Already queued
    // sound cannot be retracted here; caller tracks its recovery boundary.
    bool Cancel();
    // Device has consumed the fade end: release the rollback lane. Until this
    // acknowledgment, its cursor advances with every produced new-source block.
    bool Confirm();
    void Seek(std::uint64_t sample, std::stop_token stop = {});
    void SetLoop(bool loop);
    media::AudioInfo Info() const;
    FadeProgress Progress() const { return progress_; }
    std::size_t ActiveCursors() const { return budget_.Active(); }
    std::size_t PeakCursors() const { return budget_.Peak(); }

   private:
    class Lane;
    media::AudioCursorBudget budget_{};
    std::unique_ptr<Lane> current_{};
    std::unique_ptr<Lane> incoming_{};
    std::unique_ptr<Lane> rollback_{};
    FadeProgress progress_{};
    media::CrossfadeCurve curve_ = media::CrossfadeCurve::kLinear;
    std::uint64_t last_source_id_ = 0;
};
}  // namespace rhythm::audio::detail
