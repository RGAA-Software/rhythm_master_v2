#pragma once

#include "rhythm/media/audio_arrangement.h"
#include "rhythm/media/audio_cursor_budget.h"
#include "rhythm/media/audio_decoder.h"

namespace rhythm::media {
// Host-side file selection crosses into the decoder worker as values. These
// paths never enter a published graph or the prepared package source contract.
struct AudioFileSource {
    assets::AssetId id_{};
    std::filesystem::path path_{};
};
struct AudioArrangementFiles {
    AudioArrangement arrangement_{};
    std::vector<AudioFileSource> files_{};
};
// Serialized worker-only PCM cursor. Reuses FFmpeg AudioDecoder per active
// source; owns no thread, audio device or clock. Output is stereo/48 kHz and
// clips once after summation. The same cursor serves playback and export.
class AudioMixer final {
   public:
    explicit AudioMixer(AudioArrangementSource source, std::uint64_t generation = 1,
                        AudioCursorBudget budget = {});
    explicit AudioMixer(AudioArrangementFiles files, std::uint64_t generation = 1,
                        std::stop_token stop = {}, AudioCursorBudget budget = {});
    ~AudioMixer();
    AudioMixer(AudioMixer&&) noexcept;
    AudioMixer& operator=(AudioMixer&&) noexcept;
    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;
    AudioInfo Info() const;
    std::optional<AudioBlock> Read(std::stop_token stop = {});
    // Resetting the sample cursor drops active decoders. I/O is deferred to Read;
    // a failed Read requires Seek before another read can be attempted.
    void Seek(std::uint64_t first_sample, std::uint64_t generation, std::stop_token stop = {});

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::media
