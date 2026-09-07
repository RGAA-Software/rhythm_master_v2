#pragma once

#include <memory>
#include <string>

#include "rhythm/media/video_decoder.h"

namespace rhythm::video {
struct PlaybackSnapshot {
    std::shared_ptr<const media::VideoFrame> frame_{};
    std::optional<double> duration_seconds_{};
    std::uint64_t generation_ = 0;
    std::uint64_t frame_revision_ = 0;
    double requested_seconds_ = 0;
    bool ended_ = false;
    bool pending_ = true;
    std::string error_{};
};
// One immutable local source, one worker and one replaceable demand. The worker
// owns decoder/current/lookahead frames; Snapshot shares immutable pixels with
// the host. No decoding or waiting occurs in Request/Snapshot. Callers cap the
// number of instances and release old snapshots to retain the three-frame bound.
class Playback final {
   public:
    explicit Playback(const std::filesystem::path& path);
    explicit Playback(std::shared_ptr<const std::vector<std::uint8_t>> bytes);
    ~Playback();
    Playback(const Playback&) = delete;
    Playback& operator=(const Playback&) = delete;
    // Generation changes denote seek/restart. Loop uses source duration; with
    // no loop, EOF holds the final frame. Selection holds the most recent PTS
    // at/before demand, so VFR frames are never presented before their timestamp.
    void Request(double seconds, std::uint64_t generation, bool loop);
    PlaybackSnapshot Snapshot() const;
    // Offline worker only: wait for this exact demand, or throw on cancellation
    // or supersession. Uses the same decoder/PTS selection as realtime playback.
    // Cancellation stops waiting; destruction also cancels and joins decoding.
    PlaybackSnapshot Resolve(double seconds, std::uint64_t generation, bool loop,
                             std::stop_token stop = {});

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::video
