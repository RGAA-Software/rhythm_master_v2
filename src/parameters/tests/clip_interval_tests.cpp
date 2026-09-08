#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/parameters/clip_interval.h"

int main() {
    using namespace rhythm::parameters;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("clip.contract");
        };
        ClipTiming timing{10, 8, 2, 6, 2, ClipEnd::kBlank, 0.5, 0.5, false};
        ClipInterval clip(timing);
        check(!clip.Sample(9).active_ && clip.Sample(10).active_ && clip.Sample(10).gain_ == 0);
        check(clip.Sample(10.25).source_seconds_ == 2.5 && clip.Sample(10.25).gain_ == 0.5);
        check(clip.ActiveDuration() == 2 && !clip.Sample(12).active_);
        timing.end_ = ClipEnd::kLoop;
        clip = ClipInterval(timing);
        check(clip.Sample(12).source_seconds_ == 2 && clip.Sample(13).source_seconds_ == 4);
        check(!clip.Sample(18).active_ && clip.Sample(17.75).gain_ == 0.5);
        timing.end_ = ClipEnd::kHold;
        clip = ClipInterval(timing);
        check(clip.Sample(17).source_seconds_ == std::nextafter(6.0, 2.0));
        check(EnvelopeGain(1, 4, 4, 4, false) == 0.5);
        check(EnvelopeGain(0.5, 4, 2, 0, true) == 0.15625);
        check(clip.Sample(11) == ClipInterval(timing).Sample(11));
        check(!clip.Sample(ClipInterval::kMaximumSeconds + 1).active_);
        bool overflow_rejected = false;
        try {
            const auto huge = std::numeric_limits<double>::max();
            (void)EnvelopeGain(1, 4, huge, huge, false);
        } catch (const std::invalid_argument&) {
            overflow_rejected = true;
        }
        check(overflow_rejected);
        const auto reject = [&](ClipTiming invalid) {
            bool rejected = false;
            try {
                (void)ClipInterval(invalid);
            } catch (const std::exception&) {
                rejected = true;
            }
            check(rejected);
        };
        timing.source_out_ = timing.source_in_;
        reject(timing);
        timing = {};
        timing.rate_ = 0;
        reject(timing);
        timing = {};
        timing.start_ = ClipInterval::kMaximumSeconds;
        reject(timing);
        timing = {};
        timing.end_ = static_cast<ClipEnd>(99);
        reject(timing);
        std::cout << "Media intervals: source trim, rate, loop/hold, half-open gating and shared "
                     "fades passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
