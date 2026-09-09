#pragma once

#include "rhythm/audio/playback.h"
#include "rhythm/media/soundtrack.h"
#include "rhythm/runtime/inputs.h"
#include "rhythm/runtime/playback_clock.h"

namespace rhythm::android_host {
struct MusicFrame {
    runtime::ExternalInputs inputs_{};
    std::optional<runtime::PlaybackSample> playback_{};
    bool failed_ = false;
    audio::PlaybackSnapshot audio_{};
};
// Host-thread adapter for app-private imported music. A retired file is kept
// until the decoding worker acknowledges its replacement. At most two files
// are owned; source documents are never modified. The shared FFmpeg playback
// service owns decoding, device output, analysis and the presentation clock.
class MusicPlayback final {
   public:
    explicit MusicPlayback(std::filesystem::path cache);
    bool Open(std::filesystem::path path);
    void Open(const media::SoundtrackSource& source);
    std::uint64_t BeginSoundtrackTransition(const media::SoundtrackSource& source, double duration,
                                            bool paused);
    bool CancelSoundtrackTransition(std::uint64_t id);
    void AdoptSoundtrack(const media::SoundtrackSource& source, std::uint64_t id);
    void Clear();
    void Apply(const runtime::PlaybackCommand& command);
    void SetSuspended(bool suspended);
    void SetLoop(bool loop) {
        file_.SetLoop(loop);
        loop_ = loop;
    }
    bool Loop() const { return loop_; }
    void SetVolume(float volume) { file_.SetVolume(volume); }
    MusicFrame Frame();

   private:
    struct FileLease {
        explicit FileLease(std::filesystem::path path) : path_(std::move(path)) {}
        ~FileLease();
        FileLease(FileLease&& other) noexcept;
        FileLease(const FileLease&) = delete;
        FileLease& operator=(const FileLease&) = delete;
        std::filesystem::path path_{};
    };
    void Collect();
    void SelectSoundtrack(const media::SoundtrackSource& source);
    std::filesystem::path cache_{};
    std::optional<FileLease> active_{};
    std::optional<FileLease> retired_{};
    std::shared_ptr<const std::vector<std::uint8_t>> embedded_{};
    storage::FileBytes streamed_{};
    std::optional<media::AudioArrangementSource> arrangement_{};
    bool selected_ = false;
    bool loop_ = false;
    std::uint64_t generation_ = 0;
    bool suspended_ = false;
    bool resume_ = false;
    std::uint64_t transition_id_ = 0;
    // Destroyed first, joining the worker before releasing its source leases.
    audio::FilePlayback file_{};
};
}  // namespace rhythm::android_host
