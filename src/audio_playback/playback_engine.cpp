#include "playback_engine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::audio::detail {
PlaybackEngine::PlaybackEngine(const PlaybackSource& source, StreamOptions options,
                               std::uint64_t generation, std::uint64_t first_sample,
                               std::stop_token stop)
    : stream_(std::make_unique<TransitionStream>(source, options, stop)),
      submitted_identity_{options.source_id_, 0},
      submitted_generation_(generation),
      decoded_end_(first_sample),
      last_loop_intent_(options.loop_) {
    if (first_sample) stream_->Seek(first_sample, stop);
    device_ = std::make_unique<OutputDevice>();
    presentation_ = std::make_unique<PlaybackPresentation>(
            generation, StreamPosition{submitted_identity_, first_sample});
    state_.generation_ = generation;
    state_.position_seconds_ = double(first_sample) / media::kAudioSampleRate;
    state_.duration_seconds_ = stream_->Info().duration_seconds_;
    state_.state_ = PlaybackState::kPaused;
}
void PlaybackEngine::Begin(const PlaybackSource& source, StreamOptions options,
                           std::uint64_t duration, media::CrossfadeCurve curve,
                           std::stop_token stop) {
    if (ended_input_ || state_.state_ == PlaybackState::kEnded ||
        AudioTransitionActive(state_.transition_.state_))
        throw std::logic_error("audio.transition_not_ready");
    stream_->Begin(source, options, duration, curve, stop);
    const auto start = device_->Snapshot().submitted_frames_;
    state_.transition_ = {options.source_id_,
                          AudioTransitionState::kQueued,
                          start,
                          start + duration,
                          0,
                          duration};
    incoming_options_ = options;
}
void PlaybackEngine::Recover(std::string error, std::uint64_t boundary) {
    state_.transition_.state_ = AudioTransitionState::kRecovering;
    state_.transition_.error_ = std::move(error);
    recovery_frame_ = boundary;
}
bool PlaybackEngine::Cancel() {
    if (!AudioTransitionActive(state_.transition_.state_) || !stream_->Cancel()) return false;
    Recover({}, device_->Snapshot().submitted_frames_);
    return true;
}
void PlaybackEngine::Submit(std::stop_token stop,
                            const std::function<std::uint64_t()>& next_generation) {
    const auto start = device_->Snapshot().submitted_frames_;
    auto block = stream_->Read(stop);
    const auto progress = stream_->Progress();
    state_.transition_.clipped_samples_ = progress.clipped_samples_;
    if (progress.state_ == FadeState::kFailed &&
        state_.transition_.state_ != AudioTransitionState::kRecovering &&
        state_.transition_.state_ != AudioTransitionState::kFailed)
        Recover(progress.error_, start);
    if (!block) {
        ended_input_ = true;
        device_->FinishInput();
        state_.duration_seconds_ = double(decoded_end_) / media::kAudioSampleRate;
        return;
    }
    if (block->identity_ != submitted_identity_) {
        submitted_generation_ = next_generation();
        submitted_identity_ = block->identity_;
    }
    if (!device_->Queue(block->samples_))
        throw std::runtime_error("playback queue budget exceeded");
    decoded_end_ = block->first_sample_ + block->samples_.size() / media::kAudioChannels;
    presentation_->Append(std::move(*block), submitted_generation_);
}
void PlaybackEngine::Observe(std::uint64_t heard, bool ended) {
    auto& fade = state_.transition_;
    const auto sources = presentation_->Sources();
    std::optional<StreamPosition> incoming;
    if (sources.current_.identity_.source_ == fade.id_)
        incoming = sources.current_;
    else if (sources.incoming_ && sources.incoming_->identity_.source_ == fade.id_)
        incoming = sources.incoming_;
    fade.incoming_presented_ = incoming.has_value();
    if (incoming) {
        fade.incoming_seconds_ = double(incoming->sample_) / media::kAudioSampleRate;
        fade.incoming_iteration_ = incoming->identity_.iteration_;
    }
    if (fade.state_ == AudioTransitionState::kRecovering) {
        if (heard > recovery_frame_ || (ended && heard == recovery_frame_))
            fade.state_ = fade.error_.empty() ? AudioTransitionState::kCanceled
                                              : AudioTransitionState::kFailed;
    } else if (fade.state_ == AudioTransitionState::kQueued ||
               fade.state_ == AudioTransitionState::kMixing) {
        fade.elapsed_frames_ = heard > fade.start_frame_
                                       ? std::min(heard - fade.start_frame_, fade.duration_frames_)
                                       : 0;
        if (heard > fade.start_frame_) fade.state_ = AudioTransitionState::kMixing;
        if ((heard > fade.end_frame_ || (ended && heard == fade.end_frame_)) &&
            stream_->Confirm()) {
            fade.state_ = AudioTransitionState::kCompleted;
            last_loop_intent_ = incoming_options_.loop_;
            state_.duration_seconds_ = stream_->Info().duration_seconds_;
            if (ended)
                state_.duration_seconds_ =
                        std::max(state_.duration_seconds_.value_or(0), state_.position_seconds_);
        }
    }
}
void PlaybackEngine::Step(bool paused, float volume, bool loop, std::stop_token stop,
                          const std::function<std::uint64_t()>& next_generation) {
    if (loop != last_loop_intent_) {
        stream_->SetLoop(loop);
        last_loop_intent_ = loop;
    }
    if (state_.state_ == PlaybackState::kEnded) return;
    device_->SetVolume(volume);
    if (device_->Snapshot().paused_ != paused) device_->Pause(paused);
    state_.state_ = paused ? PlaybackState::kPaused : PlaybackState::kPlaying;
    state_.paused_ = paused;
    if (paused) {
        drain_started_.reset();
        return;
    }
    if (!ended_input_ && device_->Snapshot().queued_frames_ < 8192) Submit(stop, next_generation);
    const auto output = device_->Snapshot();
    const auto latency_frames = static_cast<std::uint64_t>(
            std::ceil(output.device_buffer_seconds_ * media::kAudioSampleRate));
    auto heard = output.pulled_frames_ - std::min(output.pulled_frames_, latency_frames);
    if (ended_input_ && output.queued_frames_ == 0) {
        if (!drain_started_) drain_started_ = Clock::now();
        const double elapsed =
                std::chrono::duration<double>(Clock::now() - *drain_started_).count();
        const auto progressed = static_cast<std::uint64_t>(elapsed * media::kAudioSampleRate);
        heard = std::min(output.submitted_frames_, heard + progressed);
        if (heard == output.submitted_frames_) {
            state_.state_ = PlaybackState::kEnded;
            device_->Pause(true);
        }
    }
    presentation_->Consume(std::max(heard, presentation_->Consumed()));
    if (state_.state_ == PlaybackState::kEnded &&
        stream_->Progress().state_ == FadeState::kSubmitted &&
        AudioTransitionActive(state_.transition_.state_)) {
        const auto sources = presentation_->Sources();
        if (sources.incoming_ &&
            sources.incoming_->identity_.source_ == incoming_options_.source_id_)
            presentation_->FinishHandoff(*sources.incoming_, next_generation());
    }
    state_.generation_ = presentation_->Generation();
    state_.position_seconds_ = double(presentation_->Position()) / media::kAudioSampleRate;
    state_.features_ = presentation_->FeaturesSnapshot();
    state_.queued_frames_ = output.queued_frames_;
    state_.submitted_frames_ = output.submitted_frames_;
    state_.consumed_frames_ = presentation_->Consumed();
    Observe(state_.consumed_frames_, state_.state_ == PlaybackState::kEnded);
}
}  // namespace rhythm::audio::detail
