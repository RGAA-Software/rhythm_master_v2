#include "rhythm/parameters/controls.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace rhythm::parameters {
namespace {
bool Title(const std::string& title) {
    return !title.empty() && title.size() <= 128 &&
           std::none_of(title.begin(), title.end(), [](unsigned char value) { return value < 32; });
}
}  // namespace
ControlBank::ControlBank(std::vector<ControlDefinition> definitions,
                         std::vector<ControlSnapshot> snapshots)
    : definitions_(std::move(definitions)), snapshots_(std::move(snapshots)) {
    if (definitions_.size() > kMaximumControls || snapshots_.size() > kMaximumSnapshots)
        throw std::length_error("control.budget");
    std::set<ControlId> ids;
    for (const auto& control : definitions_) {
        if (!control.id_ || !ids.insert(control.id_).second || !Title(control.title_) ||
            !std::isfinite(control.minimum_) || !std::isfinite(control.maximum_) ||
            !std::isfinite(control.value_) || control.minimum_ < -1e6 || control.maximum_ > 1e6 ||
            control.minimum_ >= control.maximum_ || control.value_ < control.minimum_ ||
            control.value_ > control.maximum_)
            throw std::invalid_argument("control.definition");
    }
    ids.clear();
    for (const auto& snapshot : snapshots_) {
        if (!snapshot.id_ || !ids.insert(snapshot.id_).second || !Title(snapshot.title_) ||
            definitions_.empty())
            throw std::invalid_argument("control.snapshot");
        (void)Resolve(snapshot.values_);
    }
}
ControlValues ControlBank::Resolve(const ControlValues& values) const {
    if (values.size() > definitions_.size()) throw std::invalid_argument("control.values");
    ControlValues result;
    for (const auto& control : definitions_) result.emplace(control.id_, control.value_);
    for (const auto& [id, value] : values) {
        const auto found = std::find_if(definitions_.begin(), definitions_.end(),
                                        [id](const auto& control) { return control.id_ == id; });
        if (found == definitions_.end() || !std::isfinite(value) || value < found->minimum_ ||
            value > found->maximum_)
            throw std::invalid_argument("control.values");
        result.at(id) = value;
    }
    return result;
}
ControlValues ControlBank::Snapshot(std::uint64_t id) const {
    const auto found = std::find_if(snapshots_.begin(), snapshots_.end(),
                                    [id](const auto& snapshot) { return snapshot.id_ == id; });
    if (found == snapshots_.end()) throw std::invalid_argument("control.snapshot");
    return Resolve(found->values_);
}
ControlValues ControlBank::Blend(const ControlValues& first, const ControlValues& second,
                                 double amount) const {
    if (!std::isfinite(amount) || amount < 0 || amount > 1)
        throw std::invalid_argument("control.blend");
    auto result = Resolve(first);
    const auto target = Resolve(second);
    for (auto& [id, value] : result) value = std::lerp(value, target.at(id), amount);
    return result;
}
}  // namespace rhythm::parameters
