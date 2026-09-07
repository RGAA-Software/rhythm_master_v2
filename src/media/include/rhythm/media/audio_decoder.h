#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace rhythm::media {
inline constexpr std::uint32_t kAudioSampleRate = 48000;
inline constexpr std::uint32_t kAudioChannels = 2;
inline constexpr std::uint32_t kAudioBlockFrames = 4096;

struct AudioInfo {
    std::uint32_t source_sample_rate_ = 0;
    std::uint32_t source_channels_ = 0;
    std::optional<double> duration_seconds_{};
};

struct AudioBlock {
    // Interleaved stereo float PCM, at most kAudioBlockFrames at 48 kHz.
    // The sample index is relative to the decoded start, after codec padding.
    std::vector<float> samples_{};
    std::uint64_t first_sample_ = 0;
    std::uint64_t generation_ = 0;
};

// Exclusive, serialized worker access. Construction, Read and Seek perform I/O;
// never call them on a render/UI/device callback thread. Only local regular files
// are accepted. EOF returns nullopt; cancellation and malformed media throw.
// A failure after reading starts requires Seek/reopen before another Read.
class AudioDecoder final {
   public:
    explicit AudioDecoder(const std::filesystem::path& path, std::uint64_t generation = 1,
                          std::stop_token stop = {});
    // Immutable compressed asset bytes share lifetime with package playback.
    // Uses the same bounded custom I/O adapter as embedded video (16 MiB limit).
    explicit AudioDecoder(std::shared_ptr<const std::vector<std::uint8_t>> bytes,
                          std::uint64_t generation = 1, std::stop_token stop = {});
    ~AudioDecoder();
    AudioDecoder(AudioDecoder&&) noexcept;
    AudioDecoder& operator=(AudioDecoder&&) noexcept;
    AudioDecoder(const AudioDecoder&) = delete;
    AudioDecoder& operator=(const AudioDecoder&) = delete;
    AudioInfo Info() const;
    std::optional<AudioBlock> Read(std::stop_token stop = {});
    // Exact decoded-sample seek currently replays from the start, with bounded
    // memory and cancellation. Failed/canceled seek preserves the old decoder.
    void Seek(std::uint64_t first_sample, std::uint64_t generation, std::stop_token stop = {});

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::media
