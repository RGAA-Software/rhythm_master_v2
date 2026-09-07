#include "rhythm/media/waveform.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>
#include <stop_token>

#include "rhythm/foundation/blocking_executor.h"
#include "rhythm/media/audio_decoder.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::media {
void WaveformAccumulator::Append(std::span<const float> stereo, std::uint64_t first_frame) {
    if (stereo.size() % kAudioChannels || stereo.size() > kAudioBlockFrames * kAudioChannels ||
        first_frame != overview_.frames_ ||
        stereo.size() / kAudioChannels > std::numeric_limits<std::uint64_t>::max() - first_frame)
        throw std::invalid_argument("waveform.discontinuous");
    for (std::size_t offset = 0; offset < stereo.size(); offset += kAudioChannels) {
        if (overview_.frames_ / overview_.frames_per_peak_ == kMaximumWaveformBins) {
            for (std::size_t index = 0; index < kMaximumWaveformBins / 2; ++index) {
                const auto first = overview_.peaks_[index * 2];
                const auto second = overview_.peaks_[index * 2 + 1];
                overview_.peaks_[index] = {std::min(first.minimum_, second.minimum_),
                                           std::max(first.maximum_, second.maximum_)};
            }
            std::fill(overview_.peaks_.begin() + kMaximumWaveformBins / 2, overview_.peaks_.end(),
                      WaveformPeak{});
            overview_.frames_per_peak_ *= 2;
        }
        const auto index = static_cast<std::size_t>(overview_.frames_ / overview_.frames_per_peak_);
        auto& peak = overview_.peaks_[index];
        for (std::size_t channel = 0; channel < kAudioChannels; ++channel) {
            const auto raw = stereo[offset + channel];
            const auto value = std::isfinite(raw) ? std::clamp(raw, -1.0F, 1.0F) : 0;
            peak.minimum_ = std::min(peak.minimum_, value);
            peak.maximum_ = std::max(peak.maximum_, value);
        }
        ++overview_.frames_;
        overview_.count_ = index + 1;
    }
}
class WaveformScanner::Impl final {
   public:
    ~Impl() {
        Cancel();
        executor_.RequestStop(foundation::ShutdownMode::kDrain);
        executor_.Join();
    }
    bool Start(std::filesystem::path source) {
        if (Busy()) return false;
        stop_ = {};
        frames_.store(0);
        auto task = std::make_shared<std::packaged_task<WaveformResult()>>(
                [this, source = std::move(source), stop = stop_.get_token()] {
                    return Scan(source, stop);
                });
        auto result = task->get_future();
        if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
            return false;
        pending_ = std::move(result);
        return true;
    }
    bool Busy() const { return pending_.valid(); }
    void Cancel() { stop_.request_stop(); }
    std::optional<WaveformResult> Take() {
        if (!Busy() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return {};
        auto result = pending_.get();
        if (stop_.stop_requested()) return WaveformResult{{}, "waveform.canceled"};
        return result;
    }
    double SecondsScanned() const { return static_cast<double>(frames_.load()) / kAudioSampleRate; }

   private:
    WaveformResult Scan(const std::filesystem::path& source, std::stop_token stop) {
        try {
            if (stop.stop_requested()) return {{}, "waveform.canceled"};
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
            AudioDecoder decoder(storage::FileBytes::Open(source, 256ULL * 1024 * 1024), 1, stop);
            WaveformAccumulator accumulator;
            while (const auto block = decoder.Read(stop)) {
                accumulator.Append(block->samples_, block->first_sample_);
                frames_.store(accumulator.Overview().frames_);
                if (std::chrono::steady_clock::now() > deadline) return {{}, "waveform.timeout"};
            }
            if (!accumulator.Overview().frames_) return {{}, "waveform.empty"};
            return {accumulator.Overview(), {}};
        } catch (const std::exception&) {
            return {{}, stop.stop_requested() ? "waveform.canceled" : "waveform.failed"};
        }
    }
    foundation::BlockingExecutor executor_{{1, 1}};
    std::future<WaveformResult> pending_{};
    std::stop_source stop_{};
    std::atomic<std::uint64_t> frames_{0};
};
WaveformScanner::WaveformScanner() : impl_(std::make_unique<Impl>()) {}
WaveformScanner::~WaveformScanner() = default;
bool WaveformScanner::Start(std::filesystem::path source) {
    return impl_->Start(std::move(source));
}
bool WaveformScanner::Busy() const { return impl_->Busy(); }
void WaveformScanner::Cancel() { impl_->Cancel(); }
std::optional<WaveformResult> WaveformScanner::Take() { return impl_->Take(); }
double WaveformScanner::SecondsScanned() const { return impl_->SecondsScanned(); }
}  // namespace rhythm::media
