#include "encoding_queue.h"

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace rhythm::exporting::detail {
class EncodingQueue::Impl final {
   public:
    Impl(std::filesystem::path path, media::EncodingSettings settings, std::stop_token stop) {
        cancellation_.emplace(stop, [this] {
            stop_.request_stop();
            wake_.notify_all();
        });
        worker_ = std::jthread([this, path = std::move(path), settings] { Run(path, settings); });
    }
    ~Impl() {
        stop_.request_stop();
        wake_.notify_all();
    }
    void Push(render::ReadbackImage image, std::vector<float> audio) {
        std::unique_lock lock(mutex_);
        wake_.wait(lock, stop_.get_token(),
                   [&] { return frames_.size() < 2 || error_ || closing_; });
        Check();
        if (closing_) throw std::logic_error("export.encoder_closed");
        frames_.push_back({std::move(image), std::move(audio)});
        wake_.notify_all();
    }
    std::uint64_t Completed() const {
        std::lock_guard lock(mutex_);
        Check();
        return completed_;
    }
    void Finish() {
        std::unique_lock lock(mutex_);
        Check();
        closing_ = true;
        wake_.notify_all();
        wake_.wait(lock, stop_.get_token(), [&] { return finished_ || error_; });
        Check();
    }

   private:
    struct Frame {
        render::ReadbackImage image_{};
        std::vector<float> audio_{};
    };
    void Check() const {
        if (error_) std::rethrow_exception(error_);
        if (stop_.stop_requested()) throw std::runtime_error("export.canceled");
    }
    void Run(const std::filesystem::path& path, const media::EncodingSettings& settings) {
        try {
            media::AvWriter writer(path, settings, stop_.get_token());
            for (;;) {
                Frame frame;
                {
                    std::unique_lock lock(mutex_);
                    wake_.wait(lock, stop_.get_token(),
                               [&] { return closing_ || !frames_.empty(); });
                    Check();
                    if (frames_.empty() && closing_) break;
                    frame = std::move(frames_.front());
                    frames_.pop_front();
                    wake_.notify_all();
                }
                writer.WriteVideo(frame.image_.rgba_);
                if (settings.audio_) writer.WriteAudio(frame.audio_);
                {
                    std::lock_guard lock(mutex_);
                    ++completed_;
                }
            }
            writer.Finish();
            {
                std::lock_guard lock(mutex_);
                finished_ = true;
            }
        } catch (...) {
            std::lock_guard lock(mutex_);
            error_ = std::current_exception();
        }
        wake_.notify_all();
    }
    mutable std::mutex mutex_{};
    std::condition_variable_any wake_{};
    std::deque<Frame> frames_{};
    std::exception_ptr error_{};
    std::uint64_t completed_ = 0;
    bool closing_ = false;
    bool finished_ = false;
    std::stop_source stop_{};
    // Unregister the caller's callback, then join before destroying shared state.
    std::jthread worker_{};
    std::optional<std::stop_callback<std::function<void()>>> cancellation_{};
};
EncodingQueue::EncodingQueue(std::filesystem::path path, media::EncodingSettings settings,
                             std::stop_token stop)
    : impl_(std::make_unique<Impl>(std::move(path), settings, stop)) {}
EncodingQueue::~EncodingQueue() = default;
void EncodingQueue::Push(render::ReadbackImage image, std::vector<float> audio) {
    impl_->Push(std::move(image), std::move(audio));
}
std::uint64_t EncodingQueue::Completed() const { return impl_->Completed(); }
void EncodingQueue::Finish() { impl_->Finish(); }
}  // namespace rhythm::exporting::detail
