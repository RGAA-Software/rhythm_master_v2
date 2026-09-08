#pragma once

#include <functional>

#include "audio_clip_editor.h"
#include "cue_editor.h"
#include "curve_editor.h"
#include "rhythm/editor/history.h"
#include "time_section_editor.h"

namespace rhythm::studio {
struct TimelineEdit {
    std::optional<editor::Snapshot> committed_{};
    bool preview_changed_ = false;
};
// UI-thread graph transactions shared by root and component scopes. This editor
// owns no playback clock, audio source or project history. Its caller supplies
// a time/range and applies completed value snapshots to the appropriate history.
class TimeTrackEditor final {
   public:
    TimelineEdit Draw(const editor::Snapshot& base, double playhead, double duration,
                      const std::map<std::string, std::string>& text,
                      const std::function<graph::NodeId()>& reserve_id = {},
                      const std::string& section_action = "timeline.add_section", double bpm = 120);
    void Reset();
    const std::optional<editor::Snapshot>& Preview() const { return draft_; }

   private:
    std::optional<editor::Snapshot> draft_{};
    TimeSectionEditor sections_{};
    CurveEditor curve_editor_{};
    std::string section_error_{};
    bool curve_draft_ = false;
    bool cue_draft_ = false;
    bool audio_draft_ = false;
    AudioClipEditor audio_clips_{};
    CueEditor cues_{};
    graph::NodeId track_ = 0;
};
}  // namespace rhythm::studio
