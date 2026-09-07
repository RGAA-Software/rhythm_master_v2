#include "rhythm/audio/output.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace rhythm::audio {
namespace {
constexpr std::uint32_t kSampleRate = 48000;
constexpr std::uint32_t kChannels = 2;
constexpr std::uint32_t kMaximumQueuedFrames = 24000;
constexpr std::uint32_t kMaximumBlockFrames = 4096;

void Check(bool result, const std::string& operation) {
    if (!result) {
        throw std::runtime_error(operation + ": " + SDL_GetError());
    }
}
class AudioSubsystem final {
   public:
    AudioSubsystem() { Check(SDL_InitSubSystem(SDL_INIT_AUDIO), "initialize audio output"); }
    ~AudioSubsystem() { SDL_QuitSubSystem(SDL_INIT_AUDIO); }
    AudioSubsystem(const AudioSubsystem&) = delete;
    AudioSubsystem& operator=(const AudioSubsystem&) = delete;
};
struct StreamDelete {
    // SDL_OpenAudioDeviceStream transfers ownership of the opened logical
    // device to this stream. Destroying the stream closes both resources.
    void operator()(SDL_AudioStream* value) const { SDL_DestroyAudioStream(value); }
};
}  // namespace
class OutputDevice::Impl final {
   public:
    Impl() {
        const SDL_AudioSpec format{SDL_AUDIO_F32, kChannels, kSampleRate};
        stream_.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr,
                                                nullptr));
        Check(static_cast<bool>(stream_), "open audio output");
    }
    bool Queue(std::span<const float> samples) {
        if (samples.size() % kChannels != 0 || samples.size() > kMaximumBlockFrames * kChannels ||
            !std::all_of(samples.begin(), samples.end(), [](float value) {
                return std::isfinite(value) && value >= -1 && value <= 1;
            })) {
            throw std::invalid_argument("output requires bounded finite stereo PCM");
        }
        const auto frames = static_cast<std::uint32_t>(samples.size() / kChannels);
        if (Snapshot().queued_frames_ + frames > kMaximumQueuedFrames) {
            return false;
        }
        if (!samples.empty()) {
            Check(SDL_PutAudioStreamData(stream_.get(), samples.data(),
                                         static_cast<int>(samples.size_bytes())),
                  "queue audio output");
            submitted_frames_ += frames;
        }
        return true;
    }
    void Pause(bool paused) {
        Check(paused ? SDL_PauseAudioStreamDevice(stream_.get())
                     : SDL_ResumeAudioStreamDevice(stream_.get()),
              "pause/resume audio output");
        paused_ = paused;
    }
    void Clear() {
        Check(SDL_ClearAudioStream(stream_.get()), "clear audio output");
        submitted_frames_ = 0;
    }
    void FinishInput() { Check(SDL_FlushAudioStream(stream_.get()), "flush audio output"); }
    void SetVolume(float volume) {
        if (!std::isfinite(volume) || volume < 0 || volume > 1) {
            throw std::invalid_argument("audio volume must be 0..1");
        }
        Check(SDL_SetAudioStreamGain(stream_.get(), volume), "set audio volume");
    }
    OutputSnapshot Snapshot() const {
        const int queued = SDL_GetAudioStreamQueued(stream_.get());
        if (queued < 0) {
            Check(false, "query audio queue");
        }
        OutputSnapshot snapshot{};
        snapshot.submitted_frames_ = submitted_frames_;
        snapshot.queued_frames_ = static_cast<std::uint32_t>(queued) / (sizeof(float) * kChannels);
        snapshot.pulled_frames_ =
                submitted_frames_ -
                std::min<std::uint64_t>(submitted_frames_, snapshot.queued_frames_);
        snapshot.paused_ = paused_;
        SDL_AudioSpec format{};
        int frames = 0;
        Check(SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream_.get()), &format, &frames),
              "query audio device");
        if (format.freq > 0 && frames > 0) {
            snapshot.device_buffer_seconds_ = static_cast<double>(frames) / format.freq;
        }
        return snapshot;
    }

   private:
    AudioSubsystem subsystem_{};
    std::unique_ptr<SDL_AudioStream, StreamDelete> stream_{};
    std::uint64_t submitted_frames_ = 0;
    bool paused_ = true;
};
OutputDevice::OutputDevice() : impl_(std::make_unique<Impl>()) {}
OutputDevice::~OutputDevice() = default;
bool OutputDevice::Queue(std::span<const float> samples) { return impl_->Queue(samples); }
void OutputDevice::Pause(bool paused) { impl_->Pause(paused); }
void OutputDevice::Clear() { impl_->Clear(); }
void OutputDevice::FinishInput() { impl_->FinishInput(); }
void OutputDevice::SetVolume(float volume) { impl_->SetVolume(volume); }
OutputSnapshot OutputDevice::Snapshot() const { return impl_->Snapshot(); }
}  // namespace rhythm::audio
