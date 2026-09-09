#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/media/audio_cursor_budget.h"
#include "rhythm/media/crossfade.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid crossfade admitted");
}
double Rms(std::span<const float> samples) {
    double squared = 0;
    for (const auto value : samples) squared += value * value;
    return std::sqrt(squared / static_cast<double>(samples.size()));
}
}  // namespace
int main() {
    using namespace rhythm::media;
    try {
        AudioCursorBudget budget;
        auto shared = budget;
        {
            std::array<AudioCursorBudget::Lease, 4> leases;
            for (auto& lease : leases) lease = shared.Acquire();
            Check(budget.Active() == 4 && budget.Peak() == 4, "budgets not shared");
            Reject([&] { budget.Acquire(); });
            auto moved = std::move(leases[0]);
            Check(!leases[0].Valid() && budget.Active() == 4, "move changed active cursor count");
            moved = {};
            Check(budget.Active() == 3, "released cursor retained");
            leases[0] = budget.Acquire();
        }
        Check(budget.Active() == 0 && budget.Peak() == 4, "cursor leases leaked");
        Reject([&] {
            auto lease = budget.Acquire();
            throw std::runtime_error("preparation failed");
        });
        Check(budget.Active() == 0, "exception retained cursor lease");
        for (const auto curve : {CrossfadeCurve::kLinear, CrossfadeCurve::kEqualPower}) {
            Check(CrossfadeAt(0, 48000, curve).previous_ == 1 &&
                          CrossfadeAt(0, 48000, curve).next_ == 0 &&
                          CrossfadeAt(48000, 48000, curve).previous_ == 0 &&
                          CrossfadeAt(UINT64_MAX, 48000, curve).next_ == 1 &&
                          CrossfadeAt(0, 0, curve).next_ == 1,
                  "fade endpoints not exact");
        }
        std::vector<float> correlated(8192, .9F);
        const auto linear = MixCrossfade(correlated, correlated, 24000, 48000);
        const auto power =
                MixCrossfade(correlated, correlated, 24000, 48000, CrossfadeCurve::kEqualPower);
        Check(!linear.clipped_samples_ &&
                      std::all_of(linear.samples_.begin(), linear.samples_.end(),
                                  [](float sample) { return sample == .9F; }),
              "linear crossfade changed correlated peak");
        Check(power.clipped_samples_ == 8192 && Rms(power.samples_) == 1,
              "equal-power correlated clipping hidden");
        std::vector<float> first(4096), second(4096);
        for (std::size_t sample = 0; sample < first.size(); ++sample) {
            // Independent full cycles over this block: equal power retains RMS,
            // while linear weighting produces the expected energy dip.
            first[sample] = static_cast<float>(
                    .5 * std::sin(sample * 6.283185307179586 * 17 / first.size()));
            second[sample] = static_cast<float>(
                    .5 * std::sin(sample * 6.283185307179586 * 31 / second.size()));
        }
        const auto equal = MixCrossfade(first, second, 119000, 240000, CrossfadeCurve::kEqualPower);
        const auto quieter = MixCrossfade(first, second, 119000, 240000);
        Check(std::abs(Rms(equal.samples_) - Rms(first)) < .001 &&
                      Rms(quieter.samples_) < Rms(equal.samples_) * .72,
              "uncorrelated energy behavior changed");
        const std::array<float, 2> a{.8F, -.8F}, b{.4F, -.4F};
        Check(MixCrossfade(a, b, 0, 0).samples_ == std::vector<float>(b.begin(), b.end()),
              "hard cut retained old sample");
        Check(MixCrossfade({}, b, 0, 0, CrossfadeCurve::kLinear, 1, .5F).samples_[0] == .2F,
              "silent/short source or gain invalid");
        Reject([&] { MixCrossfade(std::span(a).first(1), b, 0, 100); });
        Reject([&] { MixCrossfade(a, b, 0, 240001); });
        auto invalid = a;
        invalid[0] = std::numeric_limits<float>::quiet_NaN();
        Reject([&] { MixCrossfade(invalid, b, 0, 100); });
        std::cout << "crossfade endpoints, correlated/unrelated energy, clipping and shared cursor "
                     "RAII passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
