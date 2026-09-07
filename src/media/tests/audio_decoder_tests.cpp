#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "compressed_fixture.h"
#include "rhythm/media/audio_decoder.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
void WriteInteger(std::ostream& output, std::uint32_t value, int bytes) {
    for (int index = 0; index < bytes; ++index) {
        output.put(static_cast<char>((value >> (index * 8)) & 255U));
    }
}
float Ramp(std::size_t frame) {
    return static_cast<float>(static_cast<int>(frame % 32000) - 16000) / 32768.0F;
}
void WriteWave(const std::filesystem::path& path, std::uint32_t rate, std::uint32_t frames,
               bool tone) {
    std::ofstream file(path, std::ios::binary);
    file.write("RIFF", 4);
    WriteInteger(file, 36 + frames * 4, 4);
    file.write("WAVEfmt ", 8);
    WriteInteger(file, 16, 4);
    WriteInteger(file, 1, 2);
    WriteInteger(file, 2, 2);
    WriteInteger(file, rate, 4);
    WriteInteger(file, rate * 4, 4);
    WriteInteger(file, 4, 2);
    WriteInteger(file, 16, 2);
    file.write("data", 4);
    WriteInteger(file, frames * 4, 4);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const auto sample =
                tone ? static_cast<int>(12000 * std::sin(2 * std::numbers::pi * 440 * frame / rate))
                     : static_cast<int>(frame % 32000) - 16000;
        WriteInteger(file, static_cast<std::uint16_t>(sample), 2);
        WriteInteger(file, static_cast<std::uint16_t>(-sample), 2);
    }
    Require(static_cast<bool>(file), "write wave fixture");
}
std::vector<float> Decode(rhythm::media::AudioDecoder& decoder, std::uint64_t generation) {
    std::vector<float> output;
    while (auto block = decoder.Read()) {
        Require(block->generation_ == generation, "decoded generation");
        Require(block->first_sample_ == output.size() / 2, "continuous sample index");
        Require(block->samples_.size() <= 8192 && block->samples_.size() % 2 == 0,
                "bounded stereo block");
        output.insert(output.end(), block->samples_.begin(), block->samples_.end());
    }
    Require(!decoder.Read(), "stable EOF");
    return output;
}
void Run(const std::filesystem::path& directory) {
    namespace media = rhythm::media;
    std::filesystem::create_directories(directory);
    const auto ramp_path = directory / std::filesystem::path(u8"音乐 ramp.wav");
    const auto tone_path = directory / "tone44100.wav";
    WriteWave(ramp_path, 48000, 120031, false);
    WriteWave(tone_path, 44100, 44100, true);
    media::AudioDecoder ramp(ramp_path, 9);
    Require(ramp.Info().source_sample_rate_ == 48000 && ramp.Info().source_channels_ == 2,
            "source metadata");
    const auto samples = Decode(ramp, 9);
    {
        std::ifstream input(ramp_path, std::ios::binary);
        auto bytes = std::make_shared<const std::vector<std::uint8_t>>(
                std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        const std::weak_ptr<const std::vector<std::uint8_t>> lifetime = bytes;
        media::AudioDecoder embedded(bytes, 31);
        bytes.reset();
        Require(!lifetime.expired(), "embedded source retained by decoder");
        Require(Decode(embedded, 31) == samples, "embedded PCM equals file decoding");
        embedded.Seek(4173, 32);
        const auto block = embedded.Read();
        Require(block && block->first_sample_ == 4173 && block->generation_ == 32,
                "embedded seek position");
        Require(std::equal(block->samples_.begin(), block->samples_.end(),
                           samples.begin() + 4173 * 2),
                "embedded seek exact PCM");
        std::stop_source stop;
        stop.request_stop();
        bool canceled = false;
        try {
            embedded.Seek(0, 33, stop.get_token());
        } catch (const std::exception&) {
            canceled = true;
        }
        Require(canceled, "embedded canceled seek rejects");
        const auto retained = embedded.Read();
        Require(retained && retained->first_sample_ == 4173 + 4096 && retained->generation_ == 32,
                "embedded canceled seek preserves source and decoder");
    }
    Require(samples.size() == 120031 * 2, "exact PCM sample count including final partial block");
    for (std::size_t index = 0; index < samples.size() / 2; ++index) {
        Require(samples[index * 2] == Ramp(index) && samples[index * 2 + 1] == -Ramp(index),
                "exact PCM channels");
    }
    for (const std::uint64_t target :
         {0ULL, 1ULL, 4095ULL, 4096ULL, 90001ULL, 120030ULL, 120031ULL}) {
        ramp.Seek(target, 11);
        const auto block = ramp.Read();
        if (target == 120031) {
            Require(!block, "seek exact EOF");
        } else {
            Require(block && block->first_sample_ == target && block->generation_ == 11,
                    "seek position/generation");
            for (std::size_t sample = 0; sample < block->samples_.size(); ++sample) {
                Require(block->samples_[sample] == samples[target * 2 + sample],
                        "exact seek samples");
            }
        }
    }
    media::AudioDecoder tone(tone_path);
    const auto converted = Decode(tone, 1);
    Require(converted.size() == 48000 * 2, "resampler drain retains all samples");
    double squared_error = 0;
    for (std::size_t frame = 100; frame < 47900; ++frame) {
        const double expected =
                (12000.0 / 32768) * std::sin(2 * std::numbers::pi * 440 * frame / 48000);
        const double error = converted[frame * 2] - expected;
        squared_error += error * error;
        Require(std::abs(converted[frame * 2] + converted[frame * 2 + 1]) < 0.00001F,
                "resampled stereo phase");
    }
    Require(std::sqrt(squared_error / 47800) < 0.0001, "resample frequency/amplitude accuracy");
    const auto flac_path = directory / "tone.flac";
    media::test::WriteFlac(flac_path);
    media::AudioDecoder flac(flac_path, 19);
    const auto lossless = Decode(flac, 19);
    Require(lossless.size() == converted.size(), "compressed audio resampler drain");
    for (std::size_t sample = 0; sample < converted.size(); ++sample) {
        Require(std::abs(lossless[sample] - converted[sample]) < 0.000002F,
                "lossless decode across different packet sizes");
    }
    tone.Seek(12345, 22);
    const auto sought = tone.Read();
    Require(sought && sought->generation_ == 22, "resampled seek generation");
    for (std::size_t sample = 0; sample < sought->samples_.size(); ++sample) {
        Require(sought->samples_[sample] == converted[12345 * 2 + sample],
                "resampled seek exactness");
    }
    std::stop_source canceled;
    canceled.request_stop();
    bool rejected = false;
    try {
        tone.Seek(0, 23, canceled.get_token());
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected, "canceled seek rejects");
    auto retained = tone.Read();
    Require(retained && retained->generation_ == 22 && retained->first_sample_ == 12345 + 4096,
            "canceled seek preserves old session");
    rejected = false;
    try {
        tone.Read(canceled.get_token());
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected, "canceled read rejects");
    const auto invalid_path = directory / "invalid.wav";
    {
        std::ofstream invalid(invalid_path);
        invalid << "not a media file";
    }
    for (const auto& path : {invalid_path, directory / "missing.wav", directory}) {
        rejected = false;
        try {
            media::AudioDecoder invalid(path);
        } catch (const std::exception&) {
            rejected = true;
        }
        Require(rejected, "invalid/non-file source rejects");
    }
    std::cout << "audio decode: exact PCM, Unicode path, 44.1/48 kHz conversion, bounded blocks, "
                 "EOF drain, exact seek, cancellation and invalid input passed\n";
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            throw std::runtime_error("usage: media_audio_tests <writable fixture directory>");
        }
        const std::string argument(argv[1]);
        Run(std::filesystem::path(std::u8string(argument.begin(), argument.end())));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
