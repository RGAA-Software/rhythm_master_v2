#include "rhythm/parameters/control_sequence.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace rhythm::parameters {
namespace {
void Time(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 1e9)
        throw std::invalid_argument("cue.time");
}
}  // namespace
ControlSequence::ControlSequence(const ControlBank& bank, std::vector<ControlCue> cues)
    : defaults_(bank.Resolve()) {
    if (cues.size() > kMaximumCues) throw std::length_error("cue.budget");
    std::set<std::uint64_t> ids;
    for (auto& cue : cues) {
        Time(cue.seconds_);
        Time(cue.fade_);
        if (!cue.id_ || !ids.insert(cue.id_).second || cue.title_.empty() ||
            cue.title_.size() > 128 ||
            std::any_of(cue.title_.begin(), cue.title_.end(),
                        [](unsigned char value) { return value < 32; }) ||
            cue.seconds_ + cue.fade_ > 1e9 ||
            (!cues_.empty() && cue.seconds_ <= cues_.back().seconds_))
            throw std::invalid_argument("cue.definition");
        auto from = Sample(cue.seconds_);
        auto target = bank.Snapshot(cue.snapshot_);
        transitions_.push_back({std::move(from), std::move(target)});
        cues_.push_back(std::move(cue));
    }
}
ControlValues ControlSequence::Sample(double seconds) const {
    Time(seconds);
    const auto next =
            std::upper_bound(cues_.begin(), cues_.end(), seconds,
                             [](double value, const auto& cue) { return value < cue.seconds_; });
    if (next == cues_.begin()) return defaults_;
    const auto index = static_cast<std::size_t>(next - cues_.begin() - 1);
    const auto& cue = cues_[index];
    auto amount = cue.fade_ == 0 ? 1.0 : std::clamp((seconds - cue.seconds_) / cue.fade_, 0.0, 1.0);
    if (cue.smooth_) amount = amount * amount * (3 - 2 * amount);
    auto result = transitions_[index].from_;
    for (auto& [id, value] : result)
        value = std::lerp(value, transitions_[index].to_.at(id), amount);
    return result;
}
std::optional<std::uint64_t> ControlSequence::Active(double seconds) const {
    Time(seconds);
    const auto next =
            std::upper_bound(cues_.begin(), cues_.end(), seconds,
                             [](double value, const auto& cue) { return value < cue.seconds_; });
    return next == cues_.begin() ? std::nullopt : std::optional(std::prev(next)->id_);
}
std::vector<std::uint64_t> ControlSequence::Crossed(double from, double to) const {
    Time(from);
    Time(to);
    std::vector<std::uint64_t> result;
    if (to <= from) return result;
    for (const auto& cue : cues_)
        if (cue.seconds_ > from && cue.seconds_ <= to) result.push_back(cue.id_);
    return result;
}
ControlValues EvaluateControls(const ControlBank& bank,
                               const std::optional<ControlSequence>& sequence, double seconds,
                               const ControlValues& overrides) {
    auto validated = bank.Resolve(overrides);
    if (!sequence) return validated;
    auto result = sequence->Sample(seconds);
    for (const auto& [id, value] : overrides) result.at(id) = value;
    return result;
}
}  // namespace rhythm::parameters
