#pragma once

#include "rhythm/control_ui/beat_panel.h"
#include "rhythm/editor/history.h"

namespace rhythm::studio {
// Studio live performance requests and their authoring grid. Playback remains
// in TimelinePanel; recalled values are transient overrides, not graph edits.
class BeatPerformance final {
   public:
    std::optional<parameters::ControlValues> Advance(
            const runtime::PlaybackSample& sample, std::uint64_t generation,
            const std::optional<parameters::BeatSettings>& grid,
            const parameters::ControlBank& bank, bool current_plan);
    std::optional<editor::Snapshot> Draw(const editor::Snapshot& snapshot, double seconds,
                                         const std::map<std::string, std::string>& text);
    void RequestSnapshot(std::uint64_t id);
    void Cancel(player::PerformanceActionReason reason =
                        player::PerformanceActionReason::kSourceChanged);
    void Reset();
    const player::PerformanceAction& Status() const {
        return actions_.Status(player::PerformanceActionKind::kSnapshot);
    }

   private:
    control_ui::BeatPanel panel_{};
    player::PerformanceActions actions_{};
};
}  // namespace rhythm::studio
