#pragma once

#include <array>
#include <optional>

#include "rhythm/runtime/inputs.h"

namespace rhythm::cluster {
struct InputFrame {
    std::uint64_t epoch_ = 0;
    std::uint64_t sequence_ = 0;
    std::int64_t present_us_ = 0;
    runtime::ParticipantInputs inputs_{};
};
enum class InputResult { kAccepted, kOldEpoch, kReplay, kInvalid, kLate, kFuture, kOutOfOrder };
struct InputSample {
    runtime::ParticipantInputs inputs_{};
    std::int64_t age_us_ = 0;
    double gain_ = 1;
    bool fresh_ = true;
};
// Owner-thread bounded timeline, after transport authentication and decoding.
// Roles are discrete; normalized controls interpolate. After the configurable
// hold interval, controls fade to zero instead of replaying stale state forever.
class InputBuffer final {
   public:
    explicit InputBuffer(std::uint64_t epoch, std::int64_t hold_us = 100000,
                         std::int64_t fade_us = 200000);
    void Reset(std::uint64_t epoch);
    InputResult Push(InputFrame frame, std::int64_t host_now_us);
    std::optional<InputSample> Sample(std::int64_t present_us) const;
    std::size_t Size() const { return count_; }
    std::uint64_t Overwritten() const { return overwritten_; }

   private:
    std::array<InputFrame, 32> frames_{};
    std::size_t first_ = 0;
    std::size_t count_ = 0;
    std::uint64_t epoch_ = 0;
    std::uint64_t sequence_ = 0;
    std::uint64_t overwritten_ = 0;
    std::int64_t hold_us_ = 100000;
    std::int64_t fade_us_ = 200000;
};
}  // namespace rhythm::cluster
