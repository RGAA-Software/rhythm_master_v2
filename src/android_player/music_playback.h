#pragma once

#include "rhythm/audio/playback.h"
#include "rhythm/runtime/inputs.h"
#include "rhythm/runtime/playback_clock.h"

namespace rhythm::android_host {
struct MusicFrame {
    runtime::ExternalInputs inputs_{};
    std::optional<runtime::PlaybackSample> playback_{};
    bool failed_ = false;
};
// Host-thread adapter for app-private imported music. A retired file is kept
// until the decoding worker acknowledges its replacement. At most two files
// are owned; source documents are never modified. The shared FFmpeg playback
// service owns decoding, device output, analysis and the presentation clock.
class MusicPlayback final {
   public:
    explicit MusicPlayback(std::filesystem::path cache);
    bool Open(std::filesystem::path path);
    void Apply(const runtime::PlaybackCommand& command);
    void SetSuspended(bool suspended);
    void SetLoop(bool loop) { file_.SetLoop(loop); }
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
    std::filesystem::path cache_{};
    std::optional<FileLease> active_{};
    std::optional<FileLease> retired_{};
    std::uint64_t generation_ = 0;
    bool suspended_ = false;
    bool resume_ = false;
    // Destroyed first, joining the worker before releasing its source leases.
    audio::FilePlayback file_{};
};
}  // namespace rhythm::android_host
