#pragma once

#include <functional>

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
                      const std::string& section_action = "timeline.add_section");
    void Reset();
    const std::optional<editor::Snapshot>& Preview() const { return draft_; }

   private:
    std::optional<editor::Snapshot> draft_{};
    TimeSectionEditor sections_{};
    std::string section_error_{};
    bool curve_draft_ = false;
    graph::NodeId track_ = 0;
};
}  // namespace rhythm::studio
