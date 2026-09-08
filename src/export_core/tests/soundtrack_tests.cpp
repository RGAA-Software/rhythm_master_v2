#include <cmath>
#include <iostream>
#include <stdexcept>

#include "soundtrack.h"

int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("export.mix_contract");
        };
        check(argc == 2);
        const assets::AssetId id{std::string(64, 'a')};
        media::AudioClip clip{1, "First", id};
        clip.timing_ = {0.1, 0.4, 0.2, 0.6};
        auto other = clip;
        other.id_ = 2;
        other.title_ = "Second";
        other.timing_.start_ = 0.3;
        other.gain_ = 0.25F;
        const media::AudioArrangementSource source{
                media::AudioArrangement({clip, other}),
                {{id, {}, storage::FileBytes::Open(argv[1], 16 * 1024 * 1024)}}};
        media::AudioMixer mixer(source);
        std::vector<float> reference;
        while (const auto block = mixer.Read())
            reference.insert(reference.end(), block->samples_.begin(), block->samples_.end());
        reference.resize(48000 * 2, 0);
        exporting::detail::Soundtrack stream({}, 0.5F, {}, source);
        audio::Analyzer analyzer;
        check(analyzer.Reset(48000, 1));
        for (std::uint64_t sample = 0; sample < 48000; sample += 1600) {
            const auto output = stream.Next(1600, {});
            for (std::size_t index = 0; index < output.size(); ++index)
                check(std::abs(output[index] - reference[sample * 2 + index] * 0.5F) < 1e-7);
            check(analyzer.Push(std::span<const float>(reference).subspan(sample * 2, 3200), 2,
                                sample));
            const auto expected = analyzer.Snapshot();
            if (expected.valid_) {
                const auto actual = stream.Features();
                check(actual && actual->rms_ == expected.rms_ &&
                      actual->mono_bands_ == expected.mono_bands_);
            }
        }
        std::cout << "Export: shared mixed PCM, exact audio-frame slicing, canonical FFT and "
                     "silent tail passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
