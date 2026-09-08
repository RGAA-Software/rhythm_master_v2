#include "rhythm/media/audio_arrangement.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace rhythm::media {
AudioArrangement::AudioArrangement(std::vector<AudioClip> clips) : clips_(std::move(clips)) {
    if (clips_.empty() || clips_.size() > kMaximumClips)
        throw std::invalid_argument("audio.clip_budget");
    std::set<std::uint64_t> ids;
    std::vector<std::pair<std::uint64_t, int>> events;
    const auto sample = [](double seconds) {
        return static_cast<std::uint64_t>(std::llround(seconds * kSampleRate));
    };
    for (const auto& clip : clips_) {
        const parameters::ClipInterval interval(clip.timing_);
        if (!clip.id_ || !ids.insert(clip.id_).second || !assets::ValidId(clip.asset_) ||
            clip.title_.empty() || clip.title_.size() > 128 ||
            std::any_of(clip.title_.begin(), clip.title_.end(),
                        [](unsigned char c) { return c < 32; }) ||
            !std::isfinite(clip.gain_) || clip.gain_ < 0 || clip.gain_ > 1 ||
            !std::isfinite(clip.pan_) || clip.pan_ < -1 || clip.pan_ > 1 ||
            clip.timing_.rate_ != 1 || clip.timing_.end_ == parameters::ClipEnd::kHold)
            throw std::invalid_argument("audio.clip_invalid");
        AudioClipSamples aligned{sample(clip.timing_.start_), sample(clip.timing_.duration_),
                                 sample(clip.timing_.source_in_), sample(clip.timing_.source_out_)};
        if (!aligned.duration_ || aligned.source_in_ >= aligned.source_out_)
            throw std::invalid_argument("audio.clip_samples");
        if (clip.timing_.end_ == parameters::ClipEnd::kBlank)
            aligned.duration_ =
                    std::min(aligned.duration_, aligned.source_out_ - aligned.source_in_);
        duration_ = std::max(duration_, aligned.start_ + aligned.duration_);
        events.emplace_back(aligned.start_, 1);
        events.emplace_back(aligned.start_ + aligned.duration_, -1);
        samples_.push_back(aligned);
    }
    // End events precede starts at the same sample: touching clips do not overlap.
    std::sort(events.begin(), events.end());
    int active = 0;
    for (const auto& [position, change] : events) {
        static_cast<void>(position);
        active += change;
        if (active > static_cast<int>(kMaximumConcurrent))
            throw std::length_error("audio.concurrent_budget");
    }
}
}  // namespace rhythm::media
