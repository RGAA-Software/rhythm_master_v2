#pragma once

#include <memory>
#include <span>

#include "rhythm/audio/features.h"

namespace rhythm::audio {
// Serialized worker access. Push accepts at most 4096 mono/stereo frames, rejects
// invalid/discontinuous input without mutation, and runs at a fixed sample cadence.
// Capture callbacks enqueue PCM only; this analyzer does not belong in a device callback.
class Analyzer final {
   public:
    Analyzer();
    ~Analyzer();
    Analyzer(Analyzer&&) noexcept;
    Analyzer& operator=(Analyzer&&) noexcept;
    Analyzer(const Analyzer&) = delete;
    Analyzer& operator=(const Analyzer&) = delete;
    bool Reset(std::uint32_t sample_rate, std::uint64_t generation, std::uint64_t first_frame = 0);
    bool Push(std::span<const float> interleaved, std::uint32_t channels,
              std::uint64_t first_frame);
    Features Snapshot() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::audio
