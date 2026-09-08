#include "rhythm/video_sources/streams.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

#include "rhythm/graph/video_clip.h"

#if defined(RHYTHM_HAS_VIDEO_PLAYBACK)
#include "rhythm/video/playback.h"
#endif

namespace rhythm::video_sources {
class Streams::Impl final {
   public:
    std::vector<runtime::VideoInput> Update(const graph::ExecutionPlan& plan,
                                            const prepared_assets::Resources& resources,
                                            double seconds, std::uint64_t generation,
                                            bool resolve = false, std::stop_token stop = {}) {
        if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("video.time");
        if (stop.stop_requested()) throw std::runtime_error("video.resolve_canceled");
        std::vector<runtime::VideoInput> result;
        std::set<graph::NodeId> active;
        error_.clear();
        for (const auto& instruction : plan.instructions_) {
            if (instruction.operation_ != graph::Operation::kTextureVideo) continue;
            if (!active.insert(instruction.node_.id_).second || active.size() > 4)
                throw std::length_error("video.instance_budget");
            const auto& node = instruction.node_;
            const auto& id = std::get<assets::AssetId>(node.properties_.at("asset"));
            const auto found = std::find_if(resources.videos_.begin(), resources.videos_.end(),
                                            [&](const auto& source) { return source.id_ == id; });
            if (found == resources.videos_.end())
                throw std::invalid_argument("video.asset_missing");
            const bool is_clip = node.type_ == "texture.video_clip";
            const auto clip = is_clip ? graph::DescribeVideoClip(node) : parameters::ClipInterval{};
            const auto sample = is_clip ? clip.Sample(seconds) : parameters::ClipSample{};
            if (is_clip && !sample.active_) continue;
#if defined(RHYTHM_HAS_VIDEO_PLAYBACK)
            auto& entry = entries_[node.id_];
            if (!entry.playback_ || entry.source_ != id) {
                entry = {};
                entry.source_ = id;
                entry.playback_ = std::make_unique<video::Playback>(found->bytes_);
            }
            const double target =
                    is_clip ? sample.source_seconds_
                            : std::clamp(seconds * graph::Scalar(node, "video_speed", 1) +
                                                 graph::Scalar(node, "video_offset", 0),
                                         0.0, 86400.0 * 7);
            const bool loop = !is_clip && graph::Scalar(node, "video_loop", 1) != 0;
            const auto source_out =
                    is_clip ? std::optional<double>(clip.Timing().source_out_) : std::nullopt;
            if (!resolve) entry.playback_->Request(target, generation, loop, source_out);
            const auto snapshot =
                    resolve ? entry.playback_->Resolve(target, generation, loop, stop, source_out)
                            : entry.playback_->Snapshot();
            if (!snapshot.error_.empty()) error_ = snapshot.error_;
            if (resolve && !error_.empty()) throw std::runtime_error(error_);
            if (!snapshot.frame_) continue;
            const auto duration = snapshot.duration_seconds_;
            if (is_clip && duration && clip.Timing().source_out_ > *duration + 1e-6) {
                error_ = "clip.source_range";
                if (resolve) throw std::invalid_argument(error_);
                continue;
            }
            const double local =
                    loop && duration && *duration > 0 ? std::fmod(target, *duration) : target;
            // A loop may advance on the host before the worker has rewound.
            // Do not present the prior cycle's final frame in the next cycle.
            if (snapshot.frame_->seconds_ > local + 1e-9 || snapshot.generation_ != generation)
                continue;
            if (entry.worker_revision_ != snapshot.frame_revision_ ||
                entry.generation_ != generation) {
                entry.worker_revision_ = snapshot.frame_revision_;
                entry.generation_ = generation;
                entry.revision_ = next_revision_++;
            }
            result.push_back({node.id_, id, snapshot.frame_, entry.revision_, generation,
                              is_clip ? sample.gain_ : 1});
#else
            static_cast<void>(generation);
            static_cast<void>(resolve);
            throw std::runtime_error("video.decoder_unavailable");
#endif
        }
#if defined(RHYTHM_HAS_VIDEO_PLAYBACK)
        std::erase_if(entries_, [&](const auto& item) { return !active.contains(item.first); });
#endif
        return result;
    }
    void Reset() {
#if defined(RHYTHM_HAS_VIDEO_PLAYBACK)
        entries_.clear();
#endif
        error_.clear();
    }
    const std::string& Error() const { return error_; }

   private:
#if defined(RHYTHM_HAS_VIDEO_PLAYBACK)
    struct Entry {
        assets::AssetId source_{};
        std::unique_ptr<video::Playback> playback_{};
        std::uint64_t worker_revision_ = 0;
        std::uint64_t generation_ = 0;
        std::uint64_t revision_ = 0;
    };
    std::map<graph::NodeId, Entry> entries_{};
    std::uint64_t next_revision_ = 1;
#endif
    std::string error_{};
};
Streams::Streams() : impl_(std::make_unique<Impl>()) {}
Streams::~Streams() = default;
Streams::Streams(Streams&&) noexcept = default;
Streams& Streams::operator=(Streams&&) noexcept = default;
std::vector<runtime::VideoInput> Streams::Update(const graph::ExecutionPlan& plan,
                                                 const prepared_assets::Resources& resources,
                                                 double seconds, std::uint64_t generation) {
    return impl_->Update(plan, resources, seconds, generation);
}
void Streams::Reset() { impl_->Reset(); }
std::vector<runtime::VideoInput> Streams::Resolve(const graph::ExecutionPlan& plan,
                                                  const prepared_assets::Resources& resources,
                                                  double seconds, std::uint64_t generation,
                                                  std::stop_token stop) {
    return impl_->Update(plan, resources, seconds, generation, true, stop);
}
const std::string& Streams::Error() const { return impl_->Error(); }
}  // namespace rhythm::video_sources
