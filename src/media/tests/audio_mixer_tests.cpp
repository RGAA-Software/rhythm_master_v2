#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/media/audio_mixer.h"

int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("audio.mixer_contract");
        };
        check(argc == 2);
        const auto path = std::filesystem::path(argv[1]) / std::filesystem::path(u8"音乐 ramp.wav");
        const auto file = storage::FileBytes::Open(path, 16 * 1024 * 1024);
        media::AudioDecoder reference(file);
        std::vector<float> decoded;
        while (auto block = reference.Read())
            decoded.insert(decoded.end(), block->samples_.begin(), block->samples_.end());
        const assets::AssetId id{std::string(64, 'a')};
        media::AudioClip first{1, "Left", id};
        first.timing_ = {0.1, 0.8, 0.2, 0.5, 1, parameters::ClipEnd::kLoop, 0.1, 0.1, false};
        first.gain_ = 0.5;
        first.pan_ = -1;
        auto second = first;
        second.id_ = 2;
        second.title_ = "Right";
        second.timing_.start_ = 0.4;
        second.pan_ = 1;
        media::AudioArrangementSource source{media::AudioArrangement({first, second}),
                                             {{id, {}, file}}};
        media::AudioMixer mixer(source, 7);
        std::vector<float> mixed;
        while (auto block = mixer.Read()) {
            check(block->first_sample_ == mixed.size() / 2 && block->generation_ == 7);
            mixed.insert(mixed.end(), block->samples_.begin(), block->samples_.end());
        }
        check(mixed.size() == 57600 * 2 && !mixer.Read());
        for (std::uint64_t frame = 0; frame < 57600; ++frame) {
            for (std::size_t channel = 0; channel < 2; ++channel) {
                const std::uint64_t start = channel ? 19200 : 4800;
                double expected = 0;
                if (frame >= start && frame < start + 38400) {
                    const auto local = frame - start;
                    const auto source_frame = 9600 + local % 14400;
                    expected =
                            decoded[source_frame * 2 + channel] * 0.5 *
                            parameters::EnvelopeGain(double(local) / 48000, 0.8, 0.1, 0.1, false);
                }
                check(std::abs(mixed[frame * 2 + channel] - expected) < 1e-6);
            }
        }
        mixer.Seek(31000, 8);
        const auto seek = mixer.Read();
        check(seek && seek->first_sample_ == 31000 && seek->generation_ == 8);
        for (std::size_t index = 0; index < seek->samples_.size(); ++index)
            check(std::abs(seek->samples_[index] - mixed[62000 + index]) < 1e-6);
        std::stop_source cancel;
        cancel.request_stop();
        bool rejected = false;
        try {
            mixer.Seek(0, 9, cancel.get_token());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        check(rejected && mixer.Read()->generation_ == 8);
        media::AudioMixer from_files(
                media::AudioArrangementFiles{source.arrangement_, {{id, path}}}, 10);
        from_files.Seek(31000, 11);
        check(from_files.Read()->samples_ == seek->samples_);
        first.muted_ = true;
        media::AudioMixer muted({media::AudioArrangement({first}), {{id, {}, file}}});
        while (auto block = muted.Read())
            for (const auto sample : block->samples_) check(sample == 0);
        first.muted_ = false;
        first.pan_ = 0;
        first.gain_ = 1;
        first.timing_ = {0, 0.2, 0.2, 0.4};
        std::vector<media::AudioClip> summed;
        for (std::uint64_t clip_id = 1; clip_id <= 4; ++clip_id) {
            first.id_ = clip_id;
            summed.push_back(first);
        }
        media::AudioMixer saturated({media::AudioArrangement(summed), {{id, {}, file}}});
        while (auto block = saturated.Read())
            for (std::size_t index = 0; index < block->samples_.size(); ++index) {
                const auto source_sample = 19200 + block->first_sample_ * 2 + index;
                const auto expected = std::clamp(decoded[source_sample] * 4, -1.0F, 1.0F);
                check(block->samples_[index] == expected);
            }
        std::cout << "FFmpeg mixer: exact PCM, trimmed loops, overlap, fades, balance, mute and "
                     "seek passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
