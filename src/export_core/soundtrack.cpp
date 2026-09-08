#include "soundtrack.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::exporting::detail {
Soundtrack::Soundtrack(const std::optional<std::filesystem::path>& path, float gain,
                       std::stop_token stop,
                       std::optional<media::AudioArrangementSource> arrangement)
    : gain_(gain) {
    if (!std::isfinite(gain) || gain < 0 || gain > 1)
        throw std::invalid_argument("export.audio_gain");
    if (path)
        decoder_.emplace(*path, 1, stop);
    else if (arrangement)
        mixer_.emplace(std::move(*arrangement));
    if (!analyzer_.Reset(48000, 1)) throw std::runtime_error("export.audio_analysis");
}
std::optional<audio::Features> Soundtrack::Features() const {
    const auto features = analyzer_.Snapshot();
    return features.valid_ ? std::optional(features) : std::nullopt;
}
std::vector<float> Soundtrack::Next(std::uint32_t frames, std::stop_token stop) {
    if (!frames || frames > 4096) throw std::invalid_argument("export.audio_block");
    std::vector<float> samples(static_cast<std::size_t>(frames) * 2);
    std::size_t copied = 0;
    while (copied < samples.size()) {
        if (stop.stop_requested()) throw std::runtime_error("export.canceled");
        if ((!decoder_ && !mixer_) || eof_) break;
        if (!block_ || offset_ == block_->samples_.size()) {
            block_ = mixer_ ? mixer_->Read(stop) : decoder_->Read(stop);
            offset_ = 0;
            if (!block_) {
                eof_ = true;
                break;
            }
        }
        const auto count = std::min(samples.size() - copied, block_->samples_.size() - offset_);
        std::copy_n(block_->samples_.begin() + offset_, count, samples.begin() + copied);
        copied += count;
        offset_ += count;
    }
    if (!analyzer_.Push(samples, 2, sample_)) throw std::runtime_error("export.audio_analysis");
    sample_ += frames;
    for (auto& sample : samples) sample *= gain_;
    return samples;
}
}  // namespace rhythm::exporting::detail
