#pragma once

#include <optional>

#include "rhythm/media/av_writer.h"

namespace rhythm::exporting {
struct ExportSettings {
    media::EncodingSettings encoding_{};
    std::uint64_t frames_ = 300;
    std::optional<std::filesystem::path> music_{};
    // A short soundtrack is followed by silence; animation continues to frames_.
    // Gain affects encoded audio; visual features use source PCM, as in Player.
    float gain_ = 1;
};
struct ExportProgress {
    std::uint64_t completed_frames_ = 0;
    std::uint64_t total_frames_ = 0;
    std::uint64_t texture_bytes_ = 0;
};
}  // namespace rhythm::exporting
