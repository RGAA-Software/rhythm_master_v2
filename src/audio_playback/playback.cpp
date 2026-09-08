#include "rhythm/audio/playback.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>

#include "analysis_queue.h"
#include "rhythm/audio/output.h"
#include "rhythm/media/audio_decoder.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::audio {
namespace {
using Clock = std::chrono::steady_clock;
struct Request {
    bool loop_ = false;
    std::optional<std::filesystem::path> path_{};
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
    storage::FileBytes file_bytes_{};
    std::uint64_t generation_ = 0;
    std::uint64_t first_sample_ = 0;
    std::uint64_t revision_ = 0;
    bool paused_ = false;
    float volume_ = 1;
    std::stop_token cancel_{};
};
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
    void Load(const std::filesystem::path& path) {
        std::lock_guard lock(mutex_);
        request_.path_ = path;
        request_.bytes_.reset();
        request_.file_bytes_ = {};
        request_.first_sample_ = 0;
        request_.paused_ = false;
        RestartRequest();
    }
    void Load(std::shared_ptr<const std::vector<std::uint8_t>> bytes) {
        if (!bytes || bytes->empty() || bytes->size() > 16 * 1024 * 1024)
            throw std::invalid_argument("embedded media byte budget");
        std::lock_guard lock(mutex_);
        request_.path_.reset();
        request_.bytes_ = std::move(bytes);
        request_.file_bytes_ = {};
        request_.first_sample_ = 0;
        request_.paused_ = false;
        RestartRequest();
    }
    void Load(storage::FileBytes bytes) {
        if (!bytes.Valid() || !bytes.Size()) throw std::invalid_argument("media file range");
        std::lock_guard lock(mutex_);
        request_.path_.reset();
        request_.bytes_.reset();
        request_.file_bytes_ = std::move(bytes);
        request_.first_sample_ = 0;
        request_.paused_ = false;
        RestartRequest();
    }
    void Stop() {
        std::lock_guard lock(mutex_);
        request_.path_.reset();
        request_.bytes_.reset();
        request_.file_bytes_ = {};
        request_.first_sample_ = 0;
        RestartRequest();
    }
    void Seek(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400 * 7) {
            throw std::invalid_argument("invalid audio seek time");
        }
        std::lock_guard lock(mutex_);
        if (!request_.path_ && !request_.bytes_ && !request_.file_bytes_.Valid()) {
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
        snapshot_.state_ = request_.path_ || request_.bytes_ || request_.file_bytes_.Valid()
                                   ? PlaybackState::kLoading
                                   : PlaybackState::kStopped;
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
    std::optional<std::uint64_t> ReserveLoop(std::uint64_t source_generation) {
        std::lock_guard lock(mutex_);
        if (request_.generation_ != source_generation || !request_.loop_) return {};
        // Reserve a future audible generation without replacing the source or
        // its cancellation token. Publication changes only when consumption
        // reaches this generation's first PCM block.
        return ++next_generation_;
    }
    void Repeat(std::uint64_t generation) {
        std::lock_guard lock(mutex_);
        if (request_.generation_ != generation || !request_.loop_ || request_.paused_ ||
            (!request_.path_ && !request_.bytes_ && !request_.file_bytes_.Valid()))
            return;
        request_.first_sample_ = 0;
        RestartRequest();
    }
    void Run(std::stop_token stop) {
        std::unique_ptr<media::AudioDecoder> decoder;
        std::unique_ptr<OutputDevice> device;
        std::unique_ptr<detail::AnalysisQueue> analysis;
        std::optional<Clock::time_point> drain_started;
        PlaybackSnapshot state{};
        std::uint64_t active_generation = 0;
        std::uint64_t origin = 0;
        std::uint64_t decoded_end = 0;
        bool ended_input = false;
        bool failed = false;
        for (;;) {
            if (stop.stop_requested()) {
                return;
            }
            const auto request = Desired();
            try {
                if (request.generation_ != active_generation) {
                    device.reset();
                    decoder.reset();
                    analysis.reset();
                    drain_started.reset();
                    active_generation = request.generation_;
                    state = {};
                    state.generation_ = active_generation;
                    failed = false;
                    ended_input = false;
                    origin = request.first_sample_;
                    decoded_end = origin;
                    if (request.path_ || request.bytes_ || request.file_bytes_.Valid()) {
                        decoder = request.file_bytes_.Valid()
                                          ? std::make_unique<media::AudioDecoder>(
                                                    request.file_bytes_, active_generation,
                                                    request.cancel_)
                                  : request.bytes_
                                          ? std::make_unique<media::AudioDecoder>(request.bytes_,
                                                                                  active_generation,
                                                                                  request.cancel_)
                                          : std::make_unique<media::AudioDecoder>(*request.path_,
                                                                                  active_generation,
                                                                                  request.cancel_);
                        if (origin) {
                            decoder->Seek(origin, active_generation, request.cancel_);
                        }
                        device = std::make_unique<OutputDevice>();
                        analysis =
                                std::make_unique<detail::AnalysisQueue>(active_generation, origin);
                        state.duration_seconds_ = decoder->Info().duration_seconds_;
                        state.position_seconds_ =
                                static_cast<double>(origin) / media::kAudioSampleRate;
                        state.state_ = PlaybackState::kPaused;
                    }
                    Publish(state, active_generation);
                }
                if (device && !failed && state.state_ != PlaybackState::kEnded) {
                    device->SetVolume(request.volume_);
                    if (device->Snapshot().paused_ != request.paused_) {
                        device->Pause(request.paused_);
                    }
                    state.state_ =
                            request.paused_ ? PlaybackState::kPaused : PlaybackState::kPlaying;
                    if (!request.paused_) {
                        if (!ended_input && device->Snapshot().queued_frames_ < 8192) {
                            auto block = decoder->Read(request.cancel_);
                            if (!block) {
                                state.duration_seconds_ =
                                        static_cast<double>(decoded_end) / media::kAudioSampleRate;
                                if (const auto generation = ReserveLoop(active_generation)) {
                                    decoder->Seek(0, *generation, request.cancel_);
                                    block = decoder->Read(request.cancel_);
                                    if (!block) throw std::runtime_error("empty audio loop");
                                }
                            }
                            if (block) {
                                if (!device->Queue(block->samples_)) {
                                    throw std::runtime_error("playback queue budget exceeded");
                                }
                                decoded_end = block->first_sample_ +
                                              block->samples_.size() / media::kAudioChannels;
                                analysis->Append(std::move(*block));
                            } else {
                                ended_input = true;
                                device->FinishInput();
                            }
                        }
                        const auto output = device->Snapshot();
                        const auto latency_frames = static_cast<std::uint64_t>(
                                std::ceil(output.device_buffer_seconds_ * media::kAudioSampleRate));
                        auto heard = output.pulled_frames_ -
                                     std::min(output.pulled_frames_, latency_frames);
                        if (ended_input && output.queued_frames_ == 0) {
                            if (!drain_started) {
                                drain_started = Clock::now();
                            }
                            const double elapsed =
                                    std::chrono::duration<double>(Clock::now() - *drain_started)
                                            .count();
                            const auto progressed =
                                    static_cast<std::uint64_t>(elapsed * media::kAudioSampleRate);
                            heard = std::min(output.submitted_frames_, heard + progressed);
                            if (heard == output.submitted_frames_) {
                                state.state_ = PlaybackState::kEnded;
                                device->Pause(true);
                            }
                        }
                        analysis->Consume(std::max(heard, analysis->Consumed()));
                        state.generation_ = analysis->Generation();
                        state.position_seconds_ =
                                static_cast<double>(analysis->Position()) / media::kAudioSampleRate;
                        state.features_ = analysis->Snapshot();
                        state.queued_frames_ = output.queued_frames_;
                        state.submitted_frames_ = output.submitted_frames_;
                        state.consumed_frames_ = analysis->Consumed();
                    } else {
                        // Paused wall time must not advance an in-progress tail drain.
                        drain_started.reset();
                    }
                    Publish(state, active_generation);
                }
                if (state.state_ == PlaybackState::kEnded) Repeat(active_generation);
            } catch (const std::exception&) {
                device.reset();
                decoder.reset();
                analysis.reset();
                failed = true;
                state.state_ = PlaybackState::kFailed;
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
void FilePlayback::Stop() { impl_->Stop(); }
void FilePlayback::Seek(double seconds) { impl_->Seek(seconds); }
void FilePlayback::Pause(bool paused) { impl_->Pause(paused); }
void FilePlayback::SetVolume(float volume) { impl_->SetVolume(volume); }
void FilePlayback::SetLoop(bool loop) { impl_->SetLoop(loop); }
PlaybackSnapshot FilePlayback::Snapshot() const { return impl_->Snapshot(); }
}  // namespace rhythm::audio
