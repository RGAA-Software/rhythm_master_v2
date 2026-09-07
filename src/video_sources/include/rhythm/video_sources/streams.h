#pragma once

#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/runtime/video_inputs.h"

namespace rhythm::video_sources {
// Host bridge for independent video nodes. Sources have already been verified
// and probed on a preparation worker. Runtime only receives immutable samples.
class Streams final {
   public:
    Streams();
    ~Streams();
    Streams(Streams&&) noexcept;
    Streams& operator=(Streams&&) noexcept;
    Streams(const Streams&) = delete;
    Streams& operator=(const Streams&) = delete;
    std::vector<runtime::VideoInput> Update(const graph::ExecutionPlan& plan,
                                            const prepared_assets::Resources& resources,
                                            double seconds, std::uint64_t generation);
    // Offline worker only. Waits for each exact video demand using the same
    // per-node playback/loop/offset rules; never call this from the live UI.
    std::vector<runtime::VideoInput> Resolve(const graph::ExecutionPlan& plan,
                                             const prepared_assets::Resources& resources,
                                             double seconds, std::uint64_t generation,
                                             std::stop_token stop = {});
    void Reset();
    const std::string& Error() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::video_sources
