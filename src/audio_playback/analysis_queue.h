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
    void Consume(std::uint64_t frames);
    std::optional<Features> Snapshot() const;
    std::uint64_t Consumed() const { return consumed_; }

   private:
    Analyzer analyzer_{};
    std::deque<media::AudioBlock> blocks_{};
    std::uint64_t generation_ = 0;
    std::uint64_t first_sample_ = 0;
    std::uint64_t submitted_ = 0;
    std::uint64_t consumed_ = 0;
    std::size_t front_offset_ = 0;
};
}  // namespace rhythm::audio::detail
