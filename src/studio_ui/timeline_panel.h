#pragma once

#include <filesystem>
#include <functional>

#include "rhythm/editor/history.h"
#include "rhythm/runtime/playback_clock.h"
#include "time_section_editor.h"
#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "waveform_panel.h"
#endif

namespace rhythm::studio {
struct TimelineEdit {
    std::optional<editor::Snapshot> committed_{};
    bool preview_changed_ = false;
};
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
    void ResetEdit() {
        draft_.reset();
        sections_.Reset();
        curve_draft_ = false;
        section_error_.clear();
    }
    [[nodiscard]] bool Paused() const { return clock_.Paused(); }
    [[nodiscard]] std::uint64_t Generation() const { return clock_.Generation(); }
    [[nodiscard]] const std::optional<editor::Snapshot>& Preview() const { return draft_; }

   private:
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    WaveformPanel waveform_{};
#endif
    runtime::PlaybackClock clock_{};
    runtime::PlaybackCommand command_{};
    std::optional<editor::Snapshot> draft_{};
    TimeSectionEditor sections_{};
    std::string section_error_{};
    bool curve_draft_ = false;
    graph::NodeId track_ = 0;
    double duration_ = 10;
    double fps_ = 60;
    double bpm_ = 120;
    int unit_ = 0;
    bool loop_ = false;
};
}  // namespace rhythm::studio
