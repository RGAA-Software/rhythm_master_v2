#include <iostream>
#include <stdexcept>

#include "rhythm/media/audio_arrangement.h"

int main() {
    using namespace rhythm;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("audio.arrangement_contract");
        };
        const auto reject = [&](const std::vector<media::AudioClip>& clips) {
            bool rejected = false;
            try {
                (void)media::AudioArrangement(clips);
            } catch (const std::exception&) {
                rejected = true;
            }
            check(rejected);
        };
        media::AudioClip clip{1, "Bass", {std::string(64, 'a')}};
        clip.timing_ = {2, 8, 1, 3};
        auto arrangement = media::AudioArrangement({clip});
        check(arrangement.Samples()[0].duration_ == 96000 &&
              arrangement.DurationSamples() == 192000);
        clip.timing_.end_ = parameters::ClipEnd::kLoop;
        arrangement = media::AudioArrangement({clip});
        check(arrangement.DurationSamples() == 480000);
        std::vector<media::AudioClip> clips;
        for (std::uint64_t id = 1; id <= 4; ++id) {
            clip.id_ = id;
            clips.push_back(clip);
        }
        check(media::AudioArrangement(clips).Clips().size() == 4);
        clip.id_ = 5;
        clips.push_back(clip);
        reject(clips);
        clips.back().timing_.start_ = 10;
        check(media::AudioArrangement(clips).Clips().size() == 5);
        clips.back().id_ = 1;
        reject(clips);
        clip.timing_.rate_ = 2;
        reject({clip});
        clip.timing_.rate_ = 1;
        clip.timing_.end_ = parameters::ClipEnd::kHold;
        reject({clip});
        clip.timing_.end_ = parameters::ClipEnd::kBlank;
        clip.timing_.source_out_ = clip.timing_.source_in_ + 1e-7;
        reject({clip});
        std::cout << "Audio arrangement: native rate, sample trim, overlap budget and validation "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
