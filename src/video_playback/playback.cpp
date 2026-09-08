#include "rhythm/video/playback.h"

#include <cmath>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <variant>

namespace rhythm::video {
namespace {
using Source =
        std::variant<std::filesystem::path, std::shared_ptr<const std::vector<std::uint8_t>>>;
struct Demand {
    double seconds_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t epoch_ = 0;
    std::uint64_t revision_ = 0;
    bool loop_ = false;
    std::optional<double> source_out_{};
    std::stop_token cancel_{};
};
}  // namespace
class Playback::Impl final {
   public:
    explicit Impl(Source source) : source_(std::move(source)) {
        if (std::holds_alternative<std::shared_ptr<const std::vector<std::uint8_t>>>(source_)) {
            const auto& bytes = std::get<std::shared_ptr<const std::vector<std::uint8_t>>>(source_);
            if (!bytes || bytes->empty() || bytes->size() > 16 * 1024 * 1024)
                throw std::invalid_argument("video source byte budget");
        }
        worker_ = std::jthread([this](std::stop_token stop) { Run(stop); });
    }
    ~Impl() {
        worker_.request_stop();
        {
            std::lock_guard lock(mutex_);
            cancel_.request_stop();
        }
        wake_.notify_all();
    }
    std::uint64_t Request(double seconds, std::uint64_t generation, bool loop,
                          std::optional<double> source_out) {
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400 * 7)
            throw std::invalid_argument("video request time");
        if (source_out && (!std::isfinite(*source_out) || *source_out <= seconds ||
                           *source_out > 86400 * 7 || loop))
            throw std::invalid_argument("video source out");
        std::lock_guard lock(mutex_);
        if (demand_.revision_ && demand_.seconds_ == seconds && demand_.generation_ == generation &&
            demand_.loop_ == loop && demand_.source_out_ == source_out)
            return demand_.revision_;
        if (!demand_.revision_ || demand_.generation_ != generation || seconds < demand_.seconds_ ||
            seconds - demand_.seconds_ > 0.5 || loop != demand_.loop_ ||
            source_out != demand_.source_out_) {
            cancel_.request_stop();
            cancel_ = {};
            demand_.cancel_ = cancel_.get_token();
            ++demand_.epoch_;
            snapshot_ = {};
        }
        demand_.seconds_ = seconds;
        demand_.generation_ = generation;
        demand_.loop_ = loop;
        demand_.source_out_ = source_out;
        ++demand_.revision_;
        snapshot_.pending_ = true;
        snapshot_.generation_ = generation;
        wake_.notify_all();
        resolved_.notify_all();
        return demand_.revision_;
    }
    PlaybackSnapshot Snapshot() const {
        std::lock_guard lock(mutex_);
        return snapshot_;
    }
    PlaybackSnapshot Resolve(double seconds, std::uint64_t generation, bool loop,
                             std::stop_token stop, std::optional<double> source_out) {
        if (stop.stop_requested()) throw std::runtime_error("video.resolve_canceled");
        const auto revision = Request(seconds, generation, loop, source_out);
        std::unique_lock lock(mutex_);
        resolved_.wait(lock, stop,
                       [&] { return demand_.revision_ != revision || !snapshot_.pending_; });
        if (stop.stop_requested()) throw std::runtime_error("video.resolve_canceled");
        if (demand_.revision_ != revision) throw std::runtime_error("video.resolve_superseded");
        return snapshot_;
    }

   private:
    std::unique_ptr<media::VideoDecoder> Open(const Demand& demand) const {
        if (std::holds_alternative<std::filesystem::path>(source_))
            return std::make_unique<media::VideoDecoder>(std::get<std::filesystem::path>(source_),
                                                         demand.generation_, demand.cancel_);
        const auto& bytes = std::get<std::shared_ptr<const std::vector<std::uint8_t>>>(source_);
        return std::make_unique<media::VideoDecoder>(std::span<const std::uint8_t>(*bytes),
                                                     demand.generation_, demand.cancel_);
    }
    void Publish(const Demand& demand, PlaybackSnapshot snapshot) {
        std::lock_guard lock(mutex_);
        if (demand.epoch_ != demand_.epoch_) return;
        snapshot.pending_ = demand.revision_ != demand_.revision_;
        snapshot_ = std::move(snapshot);
        resolved_.notify_all();
    }
    void Run(std::stop_token stop) {
        std::unique_ptr<media::VideoDecoder> decoder;
        std::shared_ptr<const media::VideoFrame> current;
        std::shared_ptr<const media::VideoFrame> next;
        std::uint64_t revision = 0, epoch = 0, frame_revision = 0;
        std::optional<double> duration;
        bool eof = false;
        for (;;) {
            Demand demand;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, stop, [&] { return demand_.revision_ != revision; });
                if (stop.stop_requested()) return;
                demand = demand_;
            }
            revision = demand.revision_;
            try {
                const auto restart = [&] {
                    next.reset();
                    current.reset();
                    decoder.reset();
                    decoder = Open(demand);
                    duration = decoder->Info().duration_seconds_;
                    if (duration && (!std::isfinite(*duration) || *duration <= 0)) duration.reset();
                    eof = false;
                    epoch = demand.epoch_;
                };
                if (!decoder || epoch != demand.epoch_) restart();
                double target = demand.loop_ && duration ? std::fmod(demand.seconds_, *duration)
                                                         : demand.seconds_;
                if (current && target + 1e-9 < current->seconds_) restart();
                for (;;) {
                    if (demand.cancel_.stop_requested())
                        throw std::runtime_error("video cancelled");
                    if (!next && !eof) {
                        auto decoded = decoder->Read(demand.cancel_);
                        if (decoded)
                            next = std::make_shared<const media::VideoFrame>(std::move(*decoded));
                        else
                            eof = true;
                    }
                    if (next && next->seconds_ <= target + 1e-9 &&
                        (!demand.source_out_ || next->seconds_ < *demand.source_out_)) {
                        current = std::move(next);
                        ++frame_revision;
                        continue;
                    }
                    if (eof && current && !duration) {
                        duration = current->seconds_ + current->duration_seconds_;
                        if (*duration <= 0) throw std::runtime_error("video duration unavailable");
                        if (demand.loop_ && demand.seconds_ >= *duration) {
                            target = std::fmod(demand.seconds_, *duration);
                            const auto known_duration = duration;
                            restart();
                            duration = known_duration;
                            continue;
                        }
                    }
                    break;
                }
                if (!current && !next && eof) throw std::runtime_error("video contains no frames");
                PlaybackSnapshot snapshot;
                snapshot.frame_ = current;
                snapshot.duration_seconds_ = duration;
                snapshot.generation_ = demand.generation_;
                snapshot.frame_revision_ = frame_revision;
                snapshot.requested_seconds_ = demand.seconds_;
                snapshot.ended_ = !demand.loop_ && eof && duration && demand.seconds_ >= *duration;
                Publish(demand, std::move(snapshot));
            } catch (const std::exception& error) {
                decoder.reset();
                current.reset();
                next.reset();
                PlaybackSnapshot snapshot;
                snapshot.generation_ = demand.generation_;
                snapshot.error_ = error.what();
                Publish(demand, std::move(snapshot));
            }
        }
    }
    Source source_{};
    mutable std::mutex mutex_{};
    std::condition_variable_any wake_{};
    std::condition_variable_any resolved_{};
    Demand demand_{};
    PlaybackSnapshot snapshot_{};
    std::stop_source cancel_{};
    // Declared last: joins before decoder demand, mutex and source are destroyed.
    std::jthread worker_{};
};
Playback::Playback(const std::filesystem::path& path) : impl_(std::make_unique<Impl>(path)) {}
Playback::Playback(std::shared_ptr<const std::vector<std::uint8_t>> bytes)
    : impl_(std::make_unique<Impl>(std::move(bytes))) {}
Playback::~Playback() = default;
void Playback::Request(double seconds, std::uint64_t generation, bool loop,
                       std::optional<double> source_out) {
    impl_->Request(seconds, generation, loop, source_out);
}
PlaybackSnapshot Playback::Snapshot() const { return impl_->Snapshot(); }
PlaybackSnapshot Playback::Resolve(double seconds, std::uint64_t generation, bool loop,
                                   std::stop_token stop, std::optional<double> source_out) {
    return impl_->Resolve(seconds, generation, loop, stop, source_out);
}
}  // namespace rhythm::video
