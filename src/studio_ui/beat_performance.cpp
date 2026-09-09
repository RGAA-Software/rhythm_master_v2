#include "beat_performance.h"

#include <stdexcept>

namespace rhythm::studio {
std::optional<parameters::ControlValues> BeatPerformance::Advance(
        const runtime::PlaybackSample& sample, std::uint64_t generation,
        const std::optional<parameters::BeatSettings>& grid, const parameters::ControlBank& bank,
        bool current_plan) {
    actions_.Observe(sample, generation, grid);
    if (!current_plan) {
        Cancel();
        return {};
    }
    if (const auto action = actions_.TakeDue(player::PerformanceActionKind::kSnapshot)) {
        try {
            auto values = bank.Snapshot(action->target_);
            actions_.Resolve(action->id_, true);
            return values;
        } catch (const std::invalid_argument&) {
            actions_.Resolve(action->id_, false);
        }
    }
    return {};
}
std::optional<editor::Snapshot> BeatPerformance::Draw(
        const editor::Snapshot& snapshot, double seconds,
        const std::map<std::string, std::string>& text) {
    const auto edit = panel_.Draw(snapshot.document_.beat_grid_, seconds, text);
    if (control_ui::DrawPerformanceAction(Status(), text)) actions_.Cancel(Status().id_);
    if (!edit.changed_) return {};
    Cancel(player::PerformanceActionReason::kGridChanged);
    auto next = snapshot;
    next.document_.beat_grid_ = edit.grid_;
    return next;
}
void BeatPerformance::RequestSnapshot(std::uint64_t id) {
    actions_.Request(player::PerformanceActionKind::kSnapshot, id, panel_.Mode());
}
void BeatPerformance::Cancel(player::PerformanceActionReason reason) { actions_.CancelAll(reason); }
void BeatPerformance::Reset() {
    Cancel();
    panel_.Reset();
}
}  // namespace rhythm::studio
