#include "rhythm/qr/async_reader.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <stop_token>
#include <vector>

#include "frame_validation.h"
#include "rhythm/foundation/blocking_executor.h"

namespace rhythm::qr {
class AsyncReader::Impl final {
   public:
    ~Impl() {
        cancellation_.request_stop();
        executor_.RequestStop(foundation::ShutdownMode::kDrain);
        executor_.Join();
    }
    foundation::BlockingExecutor executor_{{1, 1}};
    std::future<CompletedScan> pending_{};
    std::stop_source cancellation_{};
    std::optional<std::int64_t> last_submission_us_{};
    std::uint64_t generation_ = 0;
    std::uint64_t highest_generation_ = 0;
};
AsyncReader::AsyncReader() : impl_(std::make_unique<Impl>()) {}
AsyncReader::~AsyncReader() = default;
bool AsyncReader::Begin(std::uint64_t generation) {
    if (!generation || generation <= impl_->highest_generation_) return false;
    Cancel();
    impl_->generation_ = generation;
    impl_->highest_generation_ = generation;
    return true;
}
bool AsyncReader::Busy() const { return impl_->pending_.valid(); }
void AsyncReader::Cancel() {
    impl_->cancellation_.request_stop();
    impl_->generation_ = 0;
}
ScanSubmit AsyncReader::Submit(const LuminanceView& frame, std::int64_t now_us) {
    if (!impl_->generation_) return ScanSubmit::kInactive;
    if (!detail::ValidLuminance(frame)) return ScanSubmit::kInvalidFrame;
    if (now_us < 0 || now_us > (std::int64_t{1} << 52) ||
        (impl_->last_submission_us_ && now_us < *impl_->last_submission_us_))
        return ScanSubmit::kInvalidTime;
    if (Busy()) return ScanSubmit::kBusy;
    if (impl_->last_submission_us_ && now_us - *impl_->last_submission_us_ < 200'000)
        return ScanSubmit::kRateLimited;
    try {
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(frame.width_) * frame.height_);
        for (std::uint32_t row = 0; row < frame.height_; ++row)
            std::copy_n(frame.bytes_.begin() + row * frame.row_stride_, frame.width_,
                        pixels.begin() + row * frame.width_);
        impl_->cancellation_ = {};
        auto task = std::make_shared<std::packaged_task<CompletedScan()>>(
                [pixels = std::move(pixels), width = frame.width_, height = frame.height_,
                 generation = impl_->generation_, stop = impl_->cancellation_.get_token()] {
                    if (stop.stop_requested()) return CompletedScan{generation, {}};
                    return CompletedScan{generation, ReadLuminance({pixels, width, height, width})};
                });
        auto future = task->get_future();
        if (impl_->executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
            return ScanSubmit::kFailed;
        impl_->pending_ = std::move(future);
        impl_->last_submission_us_ = now_us;
        return ScanSubmit::kAccepted;
    } catch (const std::exception&) {
        return ScanSubmit::kFailed;
    }
}
std::optional<CompletedScan> AsyncReader::Take() {
    if (!Busy() || impl_->pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return std::nullopt;
    auto completed = impl_->pending_.get();
    if (impl_->cancellation_.stop_requested() || completed.generation_ != impl_->generation_)
        return std::nullopt;
    return completed;
}
}  // namespace rhythm::qr
