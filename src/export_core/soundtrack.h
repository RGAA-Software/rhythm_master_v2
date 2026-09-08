#pragma once

#include "rhythm/audio/analyzer.h"
#include "rhythm/media/audio_mixer.h"

namespace rhythm::exporting::detail {
// Sequential soundtrack cursor. Analyzer consumes source PCM at sample cadence;
// encoded blocks may be scaled, and EOF pads silence without retaining the track.
class Soundtrack final {
   public:
    Soundtrack(const std::optional<std::filesystem::path>& path, float gain, std::stop_token stop,
               std::optional<media::AudioArrangementSource> arrangement = {});
    std::optional<audio::Features> Features() const;
    std::vector<float> Next(std::uint32_t frames, std::stop_token stop);

   private:
    std::optional<media::AudioDecoder> decoder_{};
    std::optional<media::AudioMixer> mixer_{};
    audio::Analyzer analyzer_{};
    std::optional<media::AudioBlock> block_{};
    std::size_t offset_ = 0;
    std::uint64_t sample_ = 0;
    float gain_ = 1;
    bool eof_ = false;
};
}  // namespace rhythm::exporting::detail
