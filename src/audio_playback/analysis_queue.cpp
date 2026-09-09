#include "analysis_queue.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::audio::detail {
AnalysisQueue::AnalysisQueue(std::uint64_t generation, std::uint64_t first_sample)
    : generation_(generation),
      append_generation_(generation),
      next_sample_(first_sample),
      position_(first_sample) {
    if (!analyzer_.Reset(media::kAudioSampleRate, generation, first_sample)) {
        throw std::invalid_argument("invalid playback analysis origin");
    }
}
void AnalysisQueue::Append(media::AudioBlock block) { AppendChecked(std::move(block), false); }
void AnalysisQueue::AppendHandoff(media::AudioBlock block) {
    AppendChecked(std::move(block), true);
}
void AnalysisQueue::AppendChecked(media::AudioBlock block, bool handoff) {
    const auto frames = block.samples_.size() / media::kAudioChannels;
    const bool boundary =
            block.generation_ > append_generation_ && (handoff || block.first_sample_ == 0);
    if ((handoff && !boundary) ||
        (!boundary &&
         (block.generation_ != append_generation_ || block.first_sample_ != next_sample_)) ||
        block.samples_.size() % media::kAudioChannels != 0 || frames == 0 ||
        frames > media::kAudioBlockFrames || submitted_ - consumed_ + frames > 32768 ||
        block.samples_.capacity() > 65536 - retained_samples_) {
        throw std::invalid_argument("discontinuous or unbounded playback analysis queue");
    }
    const auto generation = block.generation_;
    const auto next_sample = block.first_sample_ + frames;
    blocks_.push_back(std::move(block));
    retained_samples_ += blocks_.back().samples_.capacity();
    append_generation_ = generation;
    next_sample_ = next_sample;
    submitted_ += frames;
}
void AnalysisQueue::Consume(std::uint64_t frames) {
    if (frames < consumed_ || frames > submitted_) {
        throw std::invalid_argument("invalid playback consumption cursor");
    }
    while (consumed_ < frames) {
        const auto& block = blocks_.front();
        if (block.generation_ != generation_) {
            if (!analyzer_.Reset(media::kAudioSampleRate, block.generation_, block.first_sample_))
                throw std::runtime_error("playback source analysis reset");
            generation_ = block.generation_;
        }
        const auto available = block.samples_.size() / media::kAudioChannels - front_offset_;
        const auto count =
                static_cast<std::size_t>(std::min<std::uint64_t>(available, frames - consumed_));
        const auto samples = std::span(block.samples_)
                                     .subspan(front_offset_ * media::kAudioChannels,
                                              count * media::kAudioChannels);
        if (!analyzer_.Push(samples, media::kAudioChannels, block.first_sample_ + front_offset_)) {
            throw std::runtime_error("playback analysis rejected PCM");
        }
        consumed_ += count;
        front_offset_ += count;
        position_ = block.first_sample_ + front_offset_;
        if (front_offset_ == block.samples_.size() / media::kAudioChannels) {
            retained_samples_ -= block.samples_.capacity();
            blocks_.pop_front();
            front_offset_ = 0;
        }
    }
}
std::optional<Features> AnalysisQueue::Snapshot() const {
    const auto features = analyzer_.Snapshot();
    return features.valid_ ? std::optional(features) : std::nullopt;
}
}  // namespace rhythm::audio::detail
