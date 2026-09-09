#include "rhythm/audio/playback.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>

#include "playback_engine.h"

namespace rhythm::audio {
namespace {
struct Request {
    bool loop_ = false;
    detail::PlaybackSource source_{};
    std::uint64_t generation_ = 0;
    std::uint64_t first_sample_ = 0;
    std::uint64_t revision_ = 0;
    bool paused_ = false;
    float volume_ = 1;
    std::stop_token cancel_{};
};
bool HasSource(const Request& request) { return request.source_.index() != 0; }
}  // namespace
class FilePlayback::Impl final {
   public:
    Impl() : worker_([this](std::stop_token stop) { Run(stop); }) {}
    ~Impl() {
        worker_.request_stop();
        {
            std::lock_guard lock(mutex_);
            request_cancel_.request_stop();
        }
        wake_.notify_all();
    }
    void Load(detail::PlaybackSource source) {
        detail::ValidateSource(source);
        std::lock_guard lock(mutex_);
        request_.source_ = std::move(source);
        request_.first_sample_ = 0;
        request_.paused_ = false;
        RestartRequest();
    }
    void Stop() {
        std::lock_guard lock(mutex_);
        request_.source_ = {};
        request_.first_sample_ = 0;
        RestartRequest();
    }
    void Seek(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400 * 7) {
            throw std::invalid_argument("invalid audio seek time");
        }
        std::lock_guard lock(mutex_);
        if (!HasSource(request_)) {
            return;
        }
        request_.first_sample_ = static_cast<std::uint64_t>(seconds * media::kAudioSampleRate);
        RestartRequest();
    }
    void Pause(bool paused) {
        std::lock_guard lock(mutex_);
        request_.paused_ = paused;
        Changed();
    }
    void SetVolume(float volume) {
        if (!std::isfinite(volume) || volume < 0 || volume > 1) {
            throw std::invalid_argument("audio volume must be 0..1");
        }
        std::lock_guard lock(mutex_);
        request_.volume_ = volume;
        Changed();
    }
    void SetLoop(bool loop) {
        std::lock_guard lock(mutex_);
        request_.loop_ = loop;
        Changed();
    }
    PlaybackSnapshot Snapshot() const {
        std::lock_guard lock(mutex_);
        auto result = snapshot_;
        result.paused_ = request_.paused_;
        result.source_generation_ = source_generation_;
        return result;
    }

   private:
    void Changed() {
        ++request_.revision_;
        wake_.notify_all();
    }
    void RestartRequest() {
        request_cancel_.request_stop();
        request_cancel_ = std::stop_source{};
        request_.cancel_ = request_cancel_.get_token();
        request_.generation_ = ++next_generation_;
        snapshot_ = {};
        snapshot_.generation_ = request_.generation_;
        snapshot_.position_seconds_ =
                static_cast<double>(request_.first_sample_) / media::kAudioSampleRate;
        snapshot_.state_ = HasSource(request_) ? PlaybackState::kLoading : PlaybackState::kStopped;
        Changed();
    }
    Request Desired() const {
        std::lock_guard lock(mutex_);
        return request_;
    }
    void Publish(const PlaybackSnapshot& value, std::uint64_t source_generation) {
        std::lock_guard lock(mutex_);
        if (source_generation == request_.generation_) {
            snapshot_ = value;
            source_generation_ = source_generation;
        }
    }
    std::uint64_t ReserveGeneration() {
        std::lock_guard lock(mutex_);
        return ++next_generation_;
    }
    void Repeat(std::uint64_t generation) {
        std::lock_guard lock(mutex_);
        if (request_.generation_ != generation || !request_.loop_ || request_.paused_ ||
            !HasSource(request_))
            return;
        request_.first_sample_ = 0;
        RestartRequest();
    }
    void Run(std::stop_token stop) {
        std::unique_ptr<detail::PlaybackEngine> engine;
        PlaybackSnapshot state{};
        std::uint64_t active_generation = 0;
        bool failed = false;
        for (;;) {
            if (stop.stop_requested()) return;
            const auto request = Desired();
            try {
                if (request.generation_ != active_generation) {
                    engine.reset();
                    active_generation = request.generation_;
                    state = {};
                    state.generation_ = active_generation;
                    failed = false;
                    if (HasSource(request)) {
                        engine = std::make_unique<detail::PlaybackEngine>(
                                request.source_,
                                detail::StreamOptions{active_generation, 1, request.loop_},
                                active_generation, request.first_sample_, request.cancel_);
                        state = engine->Snapshot();
                    }
                    Publish(state, active_generation);
                }
                if (engine && !failed) {
                    engine->Step(request.paused_, request.volume_, request.loop_, request.cancel_,
                                 [this] { return ReserveGeneration(); });
                    state = engine->Snapshot();
                    Publish(state, active_generation);
                }
                if (state.state_ == PlaybackState::kEnded) Repeat(active_generation);
            } catch (const std::exception& error) {
                engine.reset();
                failed = true;
                state.state_ = PlaybackState::kFailed;
                state.error_ = error.what();
                state.features_.reset();
                state.queued_frames_ = 0;
                Publish(state, active_generation);
            }
            std::unique_lock lock(mutex_);
            const auto interval = state.state_ == PlaybackState::kPlaying
                                          ? std::chrono::milliseconds(5)
                                          : std::chrono::milliseconds(250);
            wake_.wait_for(lock, stop, interval,
                           [&] { return request_.revision_ != request.revision_; });
        }
    }

    mutable std::mutex mutex_{};
    std::condition_variable_any wake_{};
    Request request_{};
    std::stop_source request_cancel_{};
    PlaybackSnapshot snapshot_{};
    std::uint64_t source_generation_ = 0;
    std::uint64_t next_generation_ = 0;
    // Destroyed first: join completes while the mailbox and mutex still exist.
    std::jthread worker_{};
};
FilePlayback::FilePlayback() : impl_(std::make_unique<Impl>()) {}
FilePlayback::~FilePlayback() = default;
void FilePlayback::Load(const std::filesystem::path& path) { impl_->Load(path); }
void FilePlayback::Load(std::shared_ptr<const std::vector<std::uint8_t>> bytes) {
    impl_->Load(std::move(bytes));
}
void FilePlayback::Load(storage::FileBytes bytes) { impl_->Load(std::move(bytes)); }
void FilePlayback::Load(media::AudioArrangementSource arrangement) {
    impl_->Load(std::make_shared<const media::AudioArrangementSource>(std::move(arrangement)));
}
void FilePlayback::Load(media::AudioArrangementFiles files) {
    impl_->Load(std::make_shared<const media::AudioArrangementFiles>(std::move(files)));
}
void FilePlayback::Stop() { impl_->Stop(); }
void FilePlayback::Seek(double seconds) { impl_->Seek(seconds); }
void FilePlayback::Pause(bool paused) { impl_->Pause(paused); }
void FilePlayback::SetVolume(float volume) { impl_->SetVolume(volume); }
void FilePlayback::SetLoop(bool loop) { impl_->SetLoop(loop); }
PlaybackSnapshot FilePlayback::Snapshot() const { return impl_->Snapshot(); }
}  // namespace rhythm::audio
