#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "rhythm/audio/features.h"

namespace rhythm::audio {
enum class PlaybackState { kStopped, kLoading, kPlaying, kPaused, kEnded, kFailed };
struct PlaybackSnapshot {
    PlaybackState state_ = PlaybackState::kStopped;
    std::uint64_t generation_ = 0;
    double position_seconds_ = 0;
    std::optional<double> duration_seconds_{};
    std::optional<Features> features_{};
    std::uint32_t queued_frames_ = 0;
};

// UI-thread commands publish desired values; a single worker owns file I/O,
// decoding, output and analysis. Latest load/seek wins. Snapshots contain no PCM.
// Position is estimated from device consumption minus the reported device buffer;
// it is not a hardware presentation timestamp. Stop/Load/Seek do not join a worker.
class FilePlayback final {
   public:
    FilePlayback();
    ~FilePlayback();
    FilePlayback(const FilePlayback&) = delete;
    FilePlayback& operator=(const FilePlayback&) = delete;
    void Load(const std::filesystem::path& path);
    void Stop();
    void Seek(double seconds);
    void Pause(bool paused);
    void SetVolume(float volume);
    // Restarts after the device tail drains, with a new analysis generation.
    // This is bounded repeat playback, not a gapless music-editing loop.
    void SetLoop(bool loop);
    PlaybackSnapshot Snapshot() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::audio
