#pragma once

#include <filesystem>
#include <functional>

#include "rhythm/editor/history.h"
#include "rhythm/runtime/playback_clock.h"
#include "time_track_editor.h"
#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "waveform_panel.h"
#endif

namespace rhythm::studio {
// Editor transport and graph curve/section transactions. Graph edits persist;
// transport/view settings are local to the editing session.
class TimelinePanel final {
   public:
    double Advance(double host_seconds, bool seekable,
                   const std::optional<runtime::PlaybackSample>& source = {});
    TimelineEdit Draw(const editor::Snapshot& base, bool seekable,
                      const std::map<std::string, std::string>& text,
                      const std::optional<std::filesystem::path>& music = {},
                      const std::function<graph::NodeId()>& reserve_id = {});
    void CancelMediaPreview();
    std::size_t WaveformBins() const;
    void Restart();
    runtime::PlaybackCommand TakePlaybackCommand();
    void ResetEdit() { tracks_.Reset(); }
    [[nodiscard]] bool Paused() const { return clock_.Paused(); }
    [[nodiscard]] std::uint64_t Generation() const { return clock_.Generation(); }
    [[nodiscard]] const std::optional<editor::Snapshot>& Preview() const {
        return tracks_.Preview();
    }

   private:
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    WaveformPanel waveform_{};
#endif
    runtime::PlaybackClock clock_{};
    runtime::PlaybackCommand command_{};
    TimeTrackEditor tracks_{};
    double duration_ = 10;
    double fps_ = 60;
    double bpm_ = 120;
    int unit_ = 0;
    bool loop_ = false;
};
}  // namespace rhythm::studio
