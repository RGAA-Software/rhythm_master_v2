#include "analysis_queue.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::audio::detail {
AnalysisQueue::AnalysisQueue(std::uint64_t generation, std::uint64_t first_sample)
    : generation_(generation), first_sample_(first_sample) {
    if (!analyzer_.Reset(media::kAudioSampleRate, generation, first_sample)) {
        throw std::invalid_argument("invalid playback analysis origin");
    }
}
void AnalysisQueue::Append(media::AudioBlock block) {
    const auto frames = block.samples_.size() / media::kAudioChannels;
    if (block.generation_ != generation_ || block.first_sample_ != first_sample_ + submitted_ ||
        block.samples_.size() % media::kAudioChannels != 0 || frames == 0 ||
        frames > media::kAudioBlockFrames || submitted_ - consumed_ + frames > 32768) {
        throw std::invalid_argument("discontinuous or unbounded playback analysis queue");
    }
    blocks_.push_back(std::move(block));
    submitted_ += frames;
}
void AnalysisQueue::Consume(std::uint64_t frames) {
    if (frames < consumed_ || frames > submitted_) {
        throw std::invalid_argument("invalid playback consumption cursor");
    }
    while (consumed_ < frames) {
        const auto& block = blocks_.front();
        const auto available = block.samples_.size() / media::kAudioChannels - front_offset_;
        const auto count =
                static_cast<std::size_t>(std::min<std::uint64_t>(available, frames - consumed_));
        const auto samples = std::span(block.samples_)
                                     .subspan(front_offset_ * media::kAudioChannels,
                                              count * media::kAudioChannels);
        if (!analyzer_.Push(samples, media::kAudioChannels, first_sample_ + consumed_)) {
            throw std::runtime_error("playback analysis rejected PCM");
        }
        consumed_ += count;
        front_offset_ += count;
        if (front_offset_ == block.samples_.size() / media::kAudioChannels) {
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
