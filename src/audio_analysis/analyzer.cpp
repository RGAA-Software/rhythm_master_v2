#include "rhythm/audio/analyzer.h"

#include <algorithm>
#include <limits>

#include "detail/band_map.h"
#include "detail/fft.h"
#include "detail/onset.h"
#include "detail/tempo.h"

namespace rhythm::audio {
namespace {
constexpr std::size_t kFftSize = 4096;
constexpr std::uint64_t kMaximumPosition = std::uint64_t{1} << 52;
}  // namespace
class Analyzer::Impl final {
   public:
    void Analyze() {
        const auto analyze_channel = [&](std::size_t channel, std::array<float, kBandCount>& raw) {
            for (std::size_t index = 0; index < kFftSize; ++index)
                window_[index] = rings_[channel][(written_ + index) % kFftSize];
            fft_.MagnitudeSpectrum(window_, magnitudes_, true, false);
            mapper_.Map(magnitudes_, raw);
        };
        std::array<float, kBandCount> raw_left{};
        std::array<float, kBandCount> raw_right{};
        std::array<float, kBandCount> raw_mono{};
        analyze_channel(0, raw_left);
        analyze_channel(1, raw_right);
        analyze_channel(2, raw_mono);
        double squares = 0;
        double weighted = 0;
        double magnitude_sum = 0;
        for (std::size_t index = 0; index < kFftSize; ++index) {
            const double left = rings_[0][index];
            const double right = rings_[1][index];
            squares += 0.5 * (left * left + right * right);
        }
        frame_.rms_ = static_cast<float>(std::sqrt(squares / kFftSize));
        frame_.loudness_ = 1 - std::exp(-frame_.rms_ * 4);
        const auto dt = static_cast<float>(hop_) / static_cast<float>(frame_.sample_rate_);
        if (frame_.rms_ >= 0.0002f) {
            const float target = std::clamp(0.16f / frame_.rms_, 0.35f, 12.0f);
            if (!gain_initialized_) {
                frame_.spectrum_gain_ = target;
                gain_initialized_ = true;
            } else {
                const float response = target < frame_.spectrum_gain_ ? 0.08f : 1.25f;
                frame_.spectrum_gain_ +=
                        (target - frame_.spectrum_gain_) * (1 - std::exp(-dt / response));
            }
        }
        const auto normalize = [&](const auto& raw, auto& output) {
            for (std::size_t band = 0; band < kBandCount; ++band) {
                const float ratio =
                        static_cast<float>(std::max(mapper_.BandCenter(band), 20.0) / 1000.0);
                const float tilt = std::clamp(std::pow(ratio, 0.12f), 0.75f, 1.5f);
                const float value =
                        1 - std::exp(-std::max(0.0f,
                                               raw[band] * 0.004f * frame_.spectrum_gain_ * tilt));
                output[band] = value < 0.002f ? 0 : std::clamp(value, 0.0f, 1.0f);
            }
        };
        normalize(raw_left, frame_.left_bands_);
        normalize(raw_right, frame_.right_bands_);
        normalize(raw_mono, frame_.mono_bands_);
        for (std::size_t bin = 0; bin < magnitudes_.size(); ++bin) {
            const double magnitude = std::max(0.0f, magnitudes_[bin]);
            weighted += magnitude * static_cast<double>(bin) * frame_.sample_rate_ / kFftSize;
            magnitude_sum += magnitude;
        }
        frame_.spectral_centroid_hz_ =
                magnitude_sum > 0 ? static_cast<float>(weighted / magnitude_sum) : 0;
        frame_.end_sample_ = next_sample_;
        frame_.center_seconds_ =
                static_cast<double>(next_sample_ - kFftSize / 2) / frame_.sample_rate_;
        const double relative_seconds =
                static_cast<double>(written_ - kFftSize / 2) / frame_.sample_rate_;
        onset_.Process(frame_.mono_bands_, kBandCount, analysis_count_++, relative_seconds, dt);
        frame_.onset_strength_ = 0;
        if (onset_.HasOnset()) {
            ++frame_.onset_id_;
            frame_.last_onset_seconds_ = onset_.LastOnset().time_seconds_ +
                                         static_cast<double>(origin_) / frame_.sample_rate_;
            frame_.onset_strength_ = onset_.LastOnset().strength_;
            tempo_.AddOnset(onset_.LastOnset().time_seconds_, onset_.LastOnset().strength_);
        }
        frame_.bpm_ = tempo_.Bpm();
        frame_.bpm_confidence_ = tempo_.Confidence();
        frame_.valid_ = true;
    }
    Features frame_{};
    detail::Fft fft_{kFftSize};
    detail::BandMap mapper_{kFftSize, 48000};
    detail::Onset onset_{};
    detail::Tempo tempo_{};
    std::array<std::array<float, kFftSize>, 3> rings_{};
    std::array<float, kFftSize> window_{};
    std::vector<float> magnitudes_ = std::vector<float>(kFftSize / 2);
    std::uint64_t next_sample_ = 0;
    std::uint64_t origin_ = 0;
    std::uint64_t written_ = 0;
    std::size_t analysis_count_ = 0;
    std::size_t since_analysis_ = 0;
    std::size_t hop_ = 800;
    bool gain_initialized_ = false;
};
Analyzer::Analyzer() : impl_(std::make_unique<Impl>()) {}
Analyzer::~Analyzer() = default;
Analyzer::Analyzer(Analyzer&&) noexcept = default;
Analyzer& Analyzer::operator=(Analyzer&&) noexcept = default;
bool Analyzer::Reset(std::uint32_t sample_rate, std::uint64_t generation,
                     std::uint64_t first_frame) {
    if (!impl_ || sample_rate < 8000 || sample_rate > 192000 || !generation ||
        generation <= impl_->frame_.generation_ || first_frame > kMaximumPosition)
        return false;
    impl_->mapper_ = detail::BandMap(kFftSize, sample_rate);
    impl_->frame_ = {};
    impl_->frame_.sample_rate_ = sample_rate;
    impl_->frame_.generation_ = generation;
    impl_->next_sample_ = first_frame;
    impl_->origin_ = first_frame;
    impl_->written_ = 0;
    impl_->analysis_count_ = 0;
    impl_->since_analysis_ = 0;
    impl_->hop_ = std::max<std::size_t>(1, (sample_rate + 30) / 60);
    impl_->gain_initialized_ = false;
    impl_->rings_ = {};
    impl_->onset_.Reset();
    impl_->tempo_.Reset();
    return true;
}
bool Analyzer::Push(std::span<const float> samples, std::uint32_t channels,
                    std::uint64_t first_frame) {
    if (!impl_ || !impl_->frame_.generation_ || (channels != 1 && channels != 2) ||
        samples.empty() || samples.size() % channels || samples.size() / channels > 4096 ||
        first_frame != impl_->next_sample_ ||
        samples.size() / channels > kMaximumPosition - first_frame ||
        std::any_of(samples.begin(), samples.end(),
                    [](auto value) { return !std::isfinite(value) || value < -1 || value > 1; }))
        return false;
    for (std::size_t index = 0; index < samples.size(); index += channels) {
        const auto position = impl_->written_ % kFftSize;
        const auto left = samples[index];
        const auto right = channels == 2 ? samples[index + 1] : left;
        impl_->rings_[0][position] = left;
        impl_->rings_[1][position] = right;
        impl_->rings_[2][position] = 0.5f * (left + right);
        ++impl_->written_;
        ++impl_->next_sample_;
        if (++impl_->since_analysis_ >= impl_->hop_) {
            impl_->since_analysis_ = 0;
            if (impl_->written_ >= kFftSize) impl_->Analyze();
        }
    }
    return true;
}
Features Analyzer::Snapshot() const { return impl_ ? impl_->frame_ : Features{}; }
}  // namespace rhythm::audio
