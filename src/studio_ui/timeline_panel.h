#pragma once

#include "rhythm/editor/history.h"
#include "rhythm/runtime/frame_clock.h"

namespace rhythm::studio {
struct TimelineEdit {
    std::optional<editor::Snapshot> committed_{};
    bool preview_changed_ = false;
};
// Editor transport and curve-edit transaction. Only graph curves are persisted;
// transport/view settings are local to the editing session.
class TimelinePanel final {
   public:
    double Advance(double host_seconds, bool seekable);
    TimelineEdit Draw(const editor::Snapshot& base, bool seekable,
                      const std::map<std::string, std::string>& text);
    void Restart();
    void ResetEdit() { draft_.reset(); }
    [[nodiscard]] bool Paused() const { return paused_; }
    [[nodiscard]] std::uint64_t Generation() const { return generation_; }
    [[nodiscard]] const std::optional<editor::Snapshot>& Preview() const { return draft_; }

   private:
    runtime::FrameClock clock_{};
    std::optional<editor::Snapshot> draft_{};
    graph::NodeId track_ = 0;
    std::uint64_t generation_ = 0;
    double duration_ = 10;
    double fps_ = 60;
    double bpm_ = 120;
    int unit_ = 0;
    bool paused_ = false;
    bool loop_ = false;
};
}  // namespace rhythm::studio
