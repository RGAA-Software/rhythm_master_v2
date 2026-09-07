#include "rhythm/cluster_player/playback_schedule.h"

#include <algorithm>

namespace rhythm::cluster_player {
namespace {
constexpr std::int64_t kMaximumTime = std::int64_t{1} << 52;
bool ValidTime(std::int64_t time) { return time >= 0 && time <= kMaximumTime; }
std::optional<std::uint32_t> Profile(const player::PreparedPackage& package) {
    const auto profile = package.Profile();
    if (!profile) return std::nullopt;
    switch (*profile) {
        case project::PackageProfile::kTextureSignalV2:
            return 0;
        case project::PackageProfile::kTextureSignalV1:
            return 1;
        case project::PackageProfile::kTextureSignalAssetsV1:
            return 2;
    }
    return std::nullopt;
}
}  // namespace
bool PlaybackSchedule::Prepare(cluster::SceneIdentity scene) {
    if (pending_scene_ && scene == *pending_scene_) return true;
    if (!scene.generation_ || scene.generation_ <= generation_ || scene.profile_ > 2 ||
        std::all_of(scene.package_hash_.begin(), scene.package_hash_.end(),
                    [](auto byte) { return byte == 0; }))
        return false;
    CancelPending();
    pending_scene_ = scene;
    generation_ = scene.generation_;
    last_start_us_ = 0;
    return true;
}
bool PlaybackSchedule::Install(const cluster::SceneIdentity& scene,
                               player::PreparedPackage package) {
    if (!pending_scene_ || scene != *pending_scene_ || !package.Ready() ||
        package.Digest() != scene.package_hash_ || Profile(package) != scene.profile_)
        return false;
    if (!prepared_) prepared_ = std::move(package);
    return true;
}
bool PlaybackSchedule::Ready(const cluster::SceneIdentity& scene) const {
    return pending_scene_ && scene == *pending_scene_ && prepared_ && prepared_->Ready();
}
bool PlaybackSchedule::Advance(std::int64_t host_now_us) {
    if (!ValidTime(host_now_us) || (last_host_us_ && host_now_us < *last_host_us_)) return false;
    last_host_us_ = host_now_us;
    return true;
}
bool PlaybackSchedule::Commit(const cluster::SceneIdentity& scene, std::int64_t start_at_us,
                              std::int64_t origin_us, std::int64_t host_now_us) {
    if (!Ready(scene) || !ValidTime(start_at_us) || !ValidTime(origin_us) ||
        !ValidTime(host_now_us) || origin_us > start_at_us || start_at_us < host_now_us ||
        start_at_us - host_now_us > 30000000 ||
        (origin_us != start_at_us && !prepared_->SupportsAnalyticSeek()) || !Advance(host_now_us))
        return false;
    if (timing_) return timing_->start_at_us_ == start_at_us && timing_->origin_us_ == origin_us;
    if (start_at_us <= last_start_us_) return false;
    timing_ = Timing{start_at_us, origin_us};
    last_start_us_ = start_at_us;
    return true;
}
ScheduleStep PlaybackSchedule::ApplyAt(std::int64_t host_now_us, player::Session& session) {
    if (!Advance(host_now_us)) return ScheduleStep::kInvalidTime;
    if (!pending_scene_) return ScheduleStep::kIdle;
    if (!timing_ || !prepared_ || host_now_us < timing_->start_at_us_)
        return ScheduleStep::kWaiting;
    if (host_now_us - timing_->start_at_us_ > 250000) {
        timing_.reset();
        return ScheduleStep::kMissed;
    }
    const auto initial_seconds =
            prepared_->SupportsAnalyticSeek()
                    ? static_cast<double>(host_now_us - timing_->origin_us_) / 1000000
                    : 0.0;
    session.LoadPrepared(std::move(*prepared_), initial_seconds);
    active_origin_us_ = timing_->origin_us_;
    prepared_.reset();
    timing_.reset();
    pending_scene_.reset();
    return ScheduleStep::kApplied;
}
void PlaybackSchedule::CancelPending() {
    prepared_.reset();
    timing_.reset();
    pending_scene_.reset();
}
}  // namespace rhythm::cluster_player
