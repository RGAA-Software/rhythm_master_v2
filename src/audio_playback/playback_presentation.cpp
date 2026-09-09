#include "playback_presentation.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rhythm::audio::detail {
PlaybackPresentation::PlaybackPresentation(std::uint64_t generation, StreamPosition initial)
    : analysis_(generation, initial.sample_),
      sources_{initial, {}},
      appended_identity_(initial.identity_),
      appended_generation_(generation) {
    if (!initial.identity_.source_) throw std::invalid_argument("audio.presentation_identity");
}
void PlaybackPresentation::Append(StreamPcm block, std::uint64_t generation) {
    const bool changed = block.identity_ != appended_identity_;
    const bool fresh = generation > appended_generation_;
    const auto frames = block.samples_.size() / media::kAudioChannels;
    const auto maximum = std::numeric_limits<std::uint64_t>::max() - frames;
    if (!block.identity_.source_ || changed != fresh ||
        (!changed && generation != appended_generation_) || block.first_sample_ > maximum ||
        submitted_ > maximum ||
        (changed && block.identity_.source_ == appended_identity_.source_ &&
         block.identity_.iteration_ <= appended_identity_.iteration_) ||
        (block.secondary_ &&
         (!block.secondary_->identity_.source_ || block.secondary_->sample_ > maximum ||
          block.secondary_->identity_.source_ == block.identity_.source_)))
        throw std::invalid_argument("audio.presentation_identity");
    Span span{{{block.identity_, block.first_sample_}, block.secondary_}, submitted_, frames};
    // Metadata allocation precedes PCM append, so allocation failure cannot
    // leave the analysis queue without its matching source identity span.
    spans_.push_back(span);
    try {
        media::AudioBlock pcm{std::move(block.samples_), block.first_sample_, generation};
        if (changed && block.identity_.source_ != appended_identity_.source_)
            analysis_.AppendHandoff(std::move(pcm));
        else
            analysis_.Append(std::move(pcm));
    } catch (...) {
        spans_.pop_back();
        throw;
    }
    appended_identity_ = block.identity_;
    appended_generation_ = generation;
    submitted_ += frames;
}
void PlaybackPresentation::Consume(std::uint64_t frames) {
    analysis_.Consume(frames);
    while (!spans_.empty() && frames > spans_.front().start_) {
        const auto& span = spans_.front();
        const auto offset = std::min(frames - span.start_, span.frames_);
        sources_ = span.sources_;
        sources_.current_.sample_ += offset;
        if (sources_.secondary_) sources_.secondary_->sample_ += offset;
        if (offset < span.frames_) break;
        spans_.pop_front();
    }
}
void PlaybackPresentation::FinishHandoff(StreamPosition position, std::uint64_t generation) {
    if (!spans_.empty() || !position.identity_.source_ || generation <= appended_generation_)
        throw std::invalid_argument("audio.undrained_handoff");
    analysis_.FinishHandoff(generation, position.sample_);
    sources_ = {position, {}};
    appended_identity_ = position.identity_;
    appended_generation_ = generation;
}
}  // namespace rhythm::audio::detail
