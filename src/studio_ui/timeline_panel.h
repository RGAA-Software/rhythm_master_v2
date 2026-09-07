#pragma once

#include "rhythm/editor/history.h"
#include "rhythm/runtime/playback_clock.h"

namespace rhythm::studio {
struct TimelineEdit {
    std::optional<editor::Snapshot> committed_{};
    bool preview_changed_ = false;
};
// Editor transport and curve-edit transaction. Only graph curves are persisted;
// transport/view settings are local to the editing session.
class TimelinePanel final {
   public:
    double Advance(double host_seconds, bool seekable,
                   const std::optional<runtime::PlaybackSample>& source = {});
    TimelineEdit Draw(const editor::Snapshot& base, bool seekable,
                      const std::map<std::string, std::string>& text);
    void Restart();
    runtime::PlaybackCommand TakePlaybackCommand();
    void ResetEdit() { draft_.reset(); }
    [[nodiscard]] bool Paused() const { return clock_.Paused(); }
    [[nodiscard]] std::uint64_t Generation() const { return clock_.Generation(); }
    [[nodiscard]] const std::optional<editor::Snapshot>& Preview() const { return draft_; }

   private:
    runtime::PlaybackClock clock_{};
    runtime::PlaybackCommand command_{};
    std::optional<editor::Snapshot> draft_{};
    graph::NodeId track_ = 0;
    double duration_ = 10;
    double fps_ = 60;
    double bpm_ = 120;
    int unit_ = 0;
    bool loop_ = false;
};
}  // namespace rhythm::studio
