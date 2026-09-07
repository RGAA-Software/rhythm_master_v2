#pragma once

#include <functional>
#include <optional>

#include "rhythm/media/av_writer.h"
#include "rhythm/project/package.h"
#include "rhythm/render/renderer.h"

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
// Dedicated offline worker with its own host/device, never the live Studio
// presentation thread. Reuses the shared graph runtime and media adapters.
// Three GPU tickets and at most one frame's PCM per ticket; no whole-track decode.
// The caller owns staging cleanup and publication after this function succeeds.
void RenderExport(const project::RuntimePackage& package, const ExportSettings& settings,
                  const std::filesystem::path& staging, render::Renderer& renderer,
                  const std::function<void(ExportProgress)>& progress = {},
                  std::stop_token stop = {});
}  // namespace rhythm::exporting
