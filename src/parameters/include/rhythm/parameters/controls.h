#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace rhythm::parameters {
using ControlId = std::uint64_t;
using ControlValues = std::map<ControlId, double>;
struct ControlDefinition {
    ControlId id_ = 0;
    std::string title_{};
    double minimum_ = 0;
    double maximum_ = 1;
    double value_ = 0;
    bool operator==(const ControlDefinition&) const = default;
};
struct ControlSnapshot {
    std::uint64_t id_ = 0;
    std::string title_{};
    ControlValues values_{};
    bool operator==(const ControlSnapshot&) const = default;
};
// Immutable after validation. Shared by Studio, published work and Player.
// IDs refer to project control nodes, never UI/native object addresses.
class ControlBank final {
   public:
    ControlBank() = default;
    ControlBank(std::vector<ControlDefinition> definitions,
                std::vector<ControlSnapshot> snapshots = {});
    std::span<const ControlDefinition> Definitions() const { return definitions_; }
    std::span<const ControlSnapshot> Snapshots() const { return snapshots_; }
    // Partial values inherit saved defaults. Unknown IDs and out-of-range values
    // reject atomically. A newly added control can coexist with older snapshots.
    ControlValues Resolve(const ControlValues& values = {}) const;
    ControlValues Snapshot(std::uint64_t id) const;
    // Pure interpolation; the caller supplies progress from the existing clock,
    // a gesture or an automation curve. This object owns no playback clock.
    ControlValues Blend(const ControlValues& first, const ControlValues& second,
                        double amount) const;
    bool operator==(const ControlBank&) const = default;
    static constexpr std::size_t kMaximumControls = 64;
    static constexpr std::size_t kMaximumSnapshots = 64;

   private:
    std::vector<ControlDefinition> definitions_{};
    std::vector<ControlSnapshot> snapshots_{};
};
}  // namespace rhythm::parameters
