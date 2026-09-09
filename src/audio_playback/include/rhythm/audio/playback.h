#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "rhythm/audio/features.h"
#include "rhythm/audio/transition.h"

namespace rhythm::storage {
class FileBytes;
}
namespace rhythm::media {
struct AudioArrangementSource;
struct AudioArrangementFiles;
struct SoundtrackSource;
}  // namespace rhythm::media

namespace rhythm::audio {
enum class PlaybackState { kStopped, kLoading, kPlaying, kPaused, kEnded, kFailed };
struct PlaybackSnapshot {
    PlaybackState state_ = PlaybackState::kStopped;
    std::uint64_t generation_ = 0;
    double position_seconds_ = 0;
    std::optional<double> duration_seconds_{};
    std::optional<Features> features_{};
    std::uint32_t queued_frames_ = 0;
    // Latest pause intent; state_ separately acknowledges the worker/device.
    bool paused_ = false;
    // Latest device master-volume intent, independent of authored source gain.
    float volume_ = 1;
    // Worker has released the preceding source and processed this load/seek/stop.
    // Loop generations can advance generation_ while this acknowledgment stays
    // unchanged. Stop/Load intent cannot advance this acknowledgment by itself.
    std::uint64_t source_generation_ = 0;
    // Continuous device/analysis counters for this load or seek, including loops.
    // These reset on source replacement, not at each audible loop generation.
    std::uint64_t submitted_frames_ = 0;
    std::uint64_t consumed_frames_ = 0;
    AudioTransitionSnapshot transition_{};
    // Current source/device failure, separate from a rejected incoming scene.
    std::string error_{};
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
    void Load(std::shared_ptr<const std::vector<std::uint8_t>> bytes);
    void Load(storage::FileBytes bytes);
    void Load(media::AudioArrangementSource arrangement);
    void Load(media::AudioArrangementFiles files);
    // Atomically installs authored gain/loop without changing device master volume.
    void LoadSoundtrack(const media::SoundtrackSource& source);
    // UI-thread value commands; decoding/preparation stays on the existing worker.
    // One outstanding transition. Failure preserves the accepted current source.
    // With no selected source, start an explicit silent old bus at time zero;
    // the scene host retains its own visual origin when adopting that clock.
    std::uint64_t BeginTransition(const media::SoundtrackSource& source, double duration_seconds,
                                  TransitionCurve curve = TransitionCurve::kLinear);
    bool CancelTransition(std::uint64_t id);
    void Stop();
    void Seek(double seconds);
    void Pause(bool paused);
    void SetVolume(float volume);
    // Appends the next decoded iteration to the same bounded device stream.
    // Analysis/time generation changes when consumption reaches the boundary.
    // Disabling stops after the iteration already submitted to the queue.
    // No flush/close is inserted at a normal loop boundary; device underruns,
    // hardware timing and arbitrary sample discontinuities are not concealed.
    void SetLoop(bool loop);
    PlaybackSnapshot Snapshot() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::audio
