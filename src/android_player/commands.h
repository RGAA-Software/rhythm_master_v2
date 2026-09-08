#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "rhythm/player/render_quality.h"
#include "rhythm/render/renderer.h"

namespace rhythm::android_host {
struct Commands {
    bool toggle_pause_ = false;
    bool restart_ = false;
    std::string package_path_{};
    std::string music_path_{};
    std::optional<double> seek_seconds_{};
    std::optional<bool> music_loop_{};
    bool focus_pause_ = false;
    std::optional<player::RenderQuality> render_quality_{};
};
Commands TakeCommands();
void PublishStatus(std::string status);
void PublishPlayback(double seconds, std::optional<double> duration, bool loop = false);
// Publish only a successfully loaded work's authored canvas, never the surface size.
void PublishScene(render::Extent canvas, std::string title);
struct Surface {
    std::uintptr_t window_ = 0;
    std::shared_ptr<void> owner_{};
    std::uint64_t generation_ = 0;
};
Surface CurrentSurface();
}  // namespace rhythm::android_host
