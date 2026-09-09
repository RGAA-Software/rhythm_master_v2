#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>

#include "rhythm/audio/capture.h"
#include "rhythm/runtime/playback_clock.h"
#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "rhythm/audio/playback.h"
#include "rhythm/media/soundtrack.h"
#endif

namespace rhythm::audio_ui {
struct AudioInputFrame {
    std::optional<audio::Features> features_{};
    std::optional<runtime::PlaybackSample> playback_{};
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    audio::PlaybackSnapshot file_{};
#endif
};
class AudioPanel final {
   public:
    void Draw(const std::map<std::string, std::string>& text);
    std::optional<audio::Features> Snapshot() const;
    // Features and presentation time are taken from one worker snapshot.
    AudioInputFrame Frame() const;
    void ApplyPlayback(const runtime::PlaybackCommand& command);
    void SetSuspended(bool suspended);
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    void LoadFile(const std::filesystem::path& path);
    void LoadSoundtrack(const media::SoundtrackSource& source);
    std::uint64_t BeginSoundtrackTransition(const media::SoundtrackSource& source, double duration,
                                            bool paused);
    bool CancelSoundtrackTransition(std::uint64_t id);
    // Accept metadata after consumed confirmation; never reload or seek the music.
    void AdoptSoundtrack(const media::SoundtrackSource& source, std::uint64_t id);
    void LoadArrangement(media::AudioArrangementFiles files);
    void ClearFile();
    void SetLoop(bool loop);
    void SetVolume(float volume);
    void SetDemoFile(std::filesystem::path path);
    std::optional<std::filesystem::path> SelectedFile() const;
    float Volume() const { return volume_; }
    bool Loop() const { return loop_; }
#endif

   private:
    audio::SystemCapture capture_{};
    bool suspended_ = false;
    bool resume_capture_ = false;
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    void DrawFile(const std::map<std::string, std::string>& text);
    void SelectSoundtrack(const media::SoundtrackSource& source);
    audio::FilePlayback file_{};
    std::array<char, 4096> file_path_{};
    float volume_ = 1;
    float seek_seconds_ = 0;
    bool seeking_ = false;
    bool resume_file_ = false;
    bool loop_ = false;
    std::filesystem::path demo_file_{};
    std::filesystem::path loaded_file_{};
    std::shared_ptr<const std::vector<std::uint8_t>> embedded_{};
    storage::FileBytes streamed_{};
    std::optional<media::AudioArrangementSource> arrangement_{};
    std::shared_ptr<const media::AudioArrangementFiles> arrangement_files_{};
    bool media_selected_ = false;
    std::optional<media::Soundtrack> soundtrack_binding_{};
    std::uint64_t transition_id_ = 0;
#endif
};
}  // namespace rhythm::audio_ui
