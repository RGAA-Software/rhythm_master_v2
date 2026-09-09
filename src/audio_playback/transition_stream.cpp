#include "transition_stream.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rhythm::audio::detail {
namespace {
void ValidateOptions(const StreamOptions& options) {
    if (!options.source_id_ || !std::isfinite(options.gain_) || options.gain_ < 0 ||
        options.gain_ > 1)
        throw std::invalid_argument("audio.transition_options");
}
void CheckStop(std::stop_token stop) {
    if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
}
}  // namespace
class TransitionStream::Lane final {
   public:
    Lane(const PlaybackSource& source, StreamOptions options, media::AudioCursorBudget budget,
         std::stop_token stop)
        : options_(options),
          required_(RequiredCursors(source)),
          stream_(std::make_unique<AudioStream>(source, options.source_id_, stop, budget)) {}
    std::size_t Available(std::stop_token stop) {
        CheckStop(stop);
        if (offset_ < samples_.size()) return (samples_.size() - offset_) / media::kAudioChannels;
        if (ended_) return 0;
        auto block = stream_->Read(stop);
        if (!block && options_.loop_) {
            if (iteration_ == std::numeric_limits<std::uint64_t>::max())
                throw std::overflow_error("audio.loop_identity");
            stream_->Seek(0, options_.source_id_, stop);
            block = stream_->Read(stop);
            if (!block) throw std::runtime_error("audio.empty_loop");
            ++iteration_;
        }
        if (!block) {
            ended_ = true;
            return 0;
        }
        if (block->samples_.empty() || block->samples_.size() > media::kAudioBlockFrames * 2 ||
            block->samples_.size() % 2)
            throw std::runtime_error("audio.invalid_block");
        samples_ = std::move(block->samples_);
        position_ = block->first_sample_;
        offset_ = 0;
        return samples_.size() / media::kAudioChannels;
    }
    StreamPcm Take(std::size_t frames) {
        StreamPcm result{{}, {options_.source_id_, iteration_}, position_};
        result.samples_.resize(frames * media::kAudioChannels);
        if (!ended_) {
            if (frames * media::kAudioChannels > samples_.size() - offset_)
                throw std::logic_error("audio.block_alignment");
            std::copy_n(samples_.begin() + static_cast<std::ptrdiff_t>(offset_),
                        result.samples_.size(), result.samples_.begin());
            offset_ += result.samples_.size();
        }
        position_ += frames;
        return result;
    }
    void Advance(std::size_t frames, std::stop_token stop) {
        while (frames) {
            const auto available = Available(stop);
            const auto count = available ? std::min(frames, available) : frames;
            if (!ended_) offset_ += count * media::kAudioChannels;
            position_ += count;
            frames -= count;
        }
    }
    void Seek(std::uint64_t sample, std::stop_token stop) {
        stream_->Seek(sample, options_.source_id_, stop);
        samples_.clear();
        offset_ = 0;
        position_ = sample;
        iteration_ = 0;
        ended_ = false;
    }
    void SetLoop(bool loop) {
        options_.loop_ = loop;
        if (loop) ended_ = false;
    }
    float Gain() const { return options_.gain_; }
    std::size_t Required() const { return required_; }
    media::AudioInfo Info() const { return stream_->Info(); }

   private:
    StreamOptions options_{};
    std::size_t required_ = 0;
    std::unique_ptr<AudioStream> stream_{};
    std::vector<float> samples_{};
    std::size_t offset_ = 0;
    std::uint64_t position_ = 0;
    std::uint64_t iteration_ = 0;
    bool ended_ = false;
};
TransitionStream::TransitionStream(const PlaybackSource& source, StreamOptions options,
                                   std::stop_token stop) {
    ValidateOptions(options);
    current_ = std::make_unique<Lane>(source, options, budget_, stop);
    last_source_id_ = options.source_id_;
}
TransitionStream::~TransitionStream() = default;
void TransitionStream::Begin(const PlaybackSource& source, StreamOptions options,
                             std::uint64_t duration, media::CrossfadeCurve curve,
                             std::stop_token stop) {
    ValidateOptions(options);
    (void)media::CrossfadeAt(0, duration, curve);
    CheckStop(stop);
    if (incoming_ || rollback_) throw std::logic_error("audio.transition_busy");
    if (options.source_id_ <= last_source_id_) throw std::invalid_argument("audio.source_identity");
    if (current_->Required() + RequiredCursors(source) > media::AudioCursorBudget::kMaximum)
        throw std::length_error("audio.transition_cursor_budget");
    auto prepared = std::make_unique<Lane>(source, options, budget_, stop);
    if (!prepared->Available(stop)) throw std::invalid_argument("audio.empty_transition_source");
    CheckStop(stop);
    incoming_ = std::move(prepared);
    last_source_id_ = options.source_id_;
    progress_ = {FadeState::kMixing, options.source_id_, 0, duration};
    curve_ = curve;
}
std::optional<StreamPcm> TransitionStream::Read(std::stop_token stop) {
    CheckStop(stop);
    if (incoming_ && progress_.frames_ == progress_.duration_) {
        rollback_ = std::move(current_);
        current_ = std::move(incoming_);
        progress_.state_ = FadeState::kSubmitted;
    }
    if (incoming_) {
        std::size_t next_available = 0;
        try {
            next_available = incoming_->Available(stop);
        } catch (const std::exception& error) {
            if (stop.stop_requested()) throw;
            progress_.state_ = FadeState::kFailed;
            progress_.error_ = error.what();
            incoming_.reset();
            // Incoming failure occurs before reading the old cursor, so this
            // call resumes precisely at the first PCM frame not yet produced.
            return Read(stop);
        }
        const auto previous_available = current_->Available(stop);
        auto frames = static_cast<std::size_t>(std::min<std::uint64_t>(
                media::kAudioBlockFrames, progress_.duration_ - progress_.frames_));
        if (next_available) frames = std::min(frames, next_available);
        if (previous_available) frames = std::min(frames, previous_available);
        auto previous = current_->Take(frames);
        auto next = incoming_->Take(frames);
        previous.incoming_ = StreamPosition{next.identity_, next.first_sample_};
        auto mixed = media::MixCrossfade(previous.samples_, next.samples_, progress_.frames_,
                                         progress_.duration_, curve_, current_->Gain(),
                                         incoming_->Gain());
        previous.samples_ = std::move(mixed.samples_);
        progress_.clipped_samples_ += mixed.clipped_samples_;
        progress_.frames_ += frames;
        return previous;
    }
    std::size_t frames = 0;
    try {
        frames = current_->Available(stop);
    } catch (const std::exception& error) {
        if (stop.stop_requested() || !rollback_) throw;
        current_ = std::move(rollback_);
        progress_.state_ = FadeState::kFailed;
        progress_.error_ = error.what();
        return Read(stop);
    }
    if (!frames) return {};
    if (rollback_) rollback_->Advance(frames, stop);
    auto result = current_->Take(frames);
    for (auto& sample : result.samples_) sample *= current_->Gain();
    return result;
}
bool TransitionStream::Cancel() {
    if (!incoming_ && !rollback_) return false;
    if (rollback_) current_ = std::move(rollback_);
    incoming_.reset();
    progress_.state_ = FadeState::kCanceled;
    return true;
}
bool TransitionStream::Confirm() {
    if (!rollback_ || progress_.state_ != FadeState::kSubmitted) return false;
    rollback_.reset();
    return true;
}
void TransitionStream::Seek(std::uint64_t sample, std::stop_token stop) {
    if (rollback_) {
        rollback_->Seek(sample, stop);
        current_ = std::move(rollback_);
    } else
        current_->Seek(sample, stop);
    incoming_.reset();
    progress_ = {};
}
void TransitionStream::SetLoop(bool loop) {
    if (rollback_)
        rollback_->SetLoop(loop);
    else
        current_->SetLoop(loop);
}
media::AudioInfo TransitionStream::Info() const { return current_->Info(); }
}  // namespace rhythm::audio::detail
