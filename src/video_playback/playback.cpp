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
    void Request(double seconds, std::uint64_t generation, bool loop) {
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400 * 7)
            throw std::invalid_argument("video request time");
        std::lock_guard lock(mutex_);
        if (demand_.revision_ && demand_.seconds_ == seconds && demand_.generation_ == generation &&
            demand_.loop_ == loop)
            return;
        if (!demand_.revision_ || demand_.generation_ != generation || seconds < demand_.seconds_ ||
            seconds - demand_.seconds_ > 0.5 || loop != demand_.loop_) {
            cancel_.request_stop();
            cancel_ = {};
            demand_.cancel_ = cancel_.get_token();
            ++demand_.epoch_;
            snapshot_ = {};
        }
        demand_.seconds_ = seconds;
        demand_.generation_ = generation;
        demand_.loop_ = loop;
        ++demand_.revision_;
        snapshot_.pending_ = true;
        snapshot_.generation_ = generation;
        wake_.notify_all();
    }
    PlaybackSnapshot Snapshot() const {
        std::lock_guard lock(mutex_);
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
                    if (next && next->seconds_ <= target + 1e-9) {
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
void Playback::Request(double seconds, std::uint64_t generation, bool loop) {
    impl_->Request(seconds, generation, loop);
}
PlaybackSnapshot Playback::Snapshot() const { return impl_->Snapshot(); }
}  // namespace rhythm::video
