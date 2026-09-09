#pragma once

#include <deque>

#include "analysis_queue.h"
#include "transition_stream.h"

namespace rhythm::audio::detail {
struct PresentedSources {
    StreamPosition current_{};
    std::optional<StreamPosition> incoming_{};
};
// Maps the SAME consumed PCM counter to FFT and both source-local clocks.
// Metadata is bounded by the analysis backlog (32768 frames, each span >= 1).
// Loop/source boundaries are represented exactly, including fractional blocks.
class PlaybackPresentation final {
   public:
    PlaybackPresentation(std::uint64_t generation, StreamPosition initial);
    void Append(StreamPcm block, std::uint64_t generation);
    void Consume(std::uint64_t frames);
    PresentedSources Sources() const { return sources_; }
    std::optional<Features> FeaturesSnapshot() const { return analysis_.Snapshot(); }
    std::uint64_t Consumed() const { return analysis_.Consumed(); }
    std::uint64_t Generation() const { return analysis_.Generation(); }
    std::uint64_t Position() const { return analysis_.Position(); }

   private:
    struct Span {
        PresentedSources sources_{};
        std::uint64_t start_ = 0;
        std::uint64_t frames_ = 0;
    };
    AnalysisQueue analysis_{1, 0};
    std::deque<Span> spans_{};
    PresentedSources sources_{};
    StreamIdentity appended_identity_{};
    std::uint64_t appended_generation_ = 0;
    std::uint64_t submitted_ = 0;
};
}  // namespace rhythm::audio::detail
