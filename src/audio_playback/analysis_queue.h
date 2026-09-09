#pragma once

#include <deque>
#include <optional>

#include "rhythm/audio/analyzer.h"
#include "rhythm/media/audio_decoder.h"

namespace rhythm::audio::detail {
// Retains only bounded, already-submitted PCM until the estimated presentation
// cursor consumes it. Analysis therefore follows output rather than decode-ahead.
class AnalysisQueue final {
   public:
    AnalysisQueue(std::uint64_t generation, std::uint64_t first_sample);
    void Append(media::AudioBlock block);
    // Explicit source handoff may begin at a nonzero source-local sample after
    // a fade. Ordinary Append retains its stricter zero-origin loop contract.
    // Neither operation publishes a generation until device consumption.
    void AppendHandoff(media::AudioBlock block);
    // A fully drained transition may end without any new-source solo PCM.
    // Publish its terminal source position without resetting device counters.
    void FinishHandoff(std::uint64_t generation, std::uint64_t first_sample);
    void Consume(std::uint64_t frames);
    std::optional<Features> Snapshot() const;
    std::uint64_t Consumed() const { return consumed_; }
    std::uint64_t Generation() const { return generation_; }
    std::uint64_t Position() const { return position_; }

   private:
    void AppendChecked(media::AudioBlock block, bool handoff);
    Analyzer analyzer_{};
    std::deque<media::AudioBlock> blocks_{};
    std::uint64_t generation_ = 0;
    std::uint64_t append_generation_ = 0;
    std::uint64_t next_sample_ = 0;
    std::uint64_t position_ = 0;
    std::uint64_t submitted_ = 0;
    std::uint64_t consumed_ = 0;
    std::size_t front_offset_ = 0;
    std::size_t retained_samples_ = 0;
};
}  // namespace rhythm::audio::detail
