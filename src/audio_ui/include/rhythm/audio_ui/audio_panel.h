#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>

#include "rhythm/audio/capture.h"
#include "rhythm/runtime/playback_clock.h"
#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "rhythm/audio/playback.h"
#endif

namespace rhythm::audio_ui {
struct AudioInputFrame {
    std::optional<audio::Features> features_{};
    std::optional<runtime::PlaybackSample> playback_{};
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
    void SetVolume(float volume);
    void SetDemoFile(std::filesystem::path path);
    std::optional<std::filesystem::path> SelectedFile() const;
    float Volume() const { return volume_; }
#endif

   private:
    audio::SystemCapture capture_{};
    bool suspended_ = false;
    bool resume_capture_ = false;
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    void DrawFile(const std::map<std::string, std::string>& text);
    audio::FilePlayback file_{};
    std::array<char, 4096> file_path_{};
    float volume_ = 1;
    float seek_seconds_ = 0;
    bool seeking_ = false;
    bool resume_file_ = false;
    bool loop_ = false;
    std::filesystem::path demo_file_{};
    std::filesystem::path loaded_file_{};
    bool media_selected_ = false;
#endif
};
}  // namespace rhythm::audio_ui
