#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "rhythm/player/render_quality.h"

namespace rhythm::android_host {
struct Commands {
    bool toggle_pause_ = false;
    bool restart_ = false;
    std::string package_path_{};
    std::optional<player::RenderQuality> render_quality_{};
};
Commands TakeCommands();
void PublishStatus(std::string status);
struct Surface {
    std::uintptr_t window_ = 0;
    std::shared_ptr<void> owner_{};
    std::uint64_t generation_ = 0;
};
Surface CurrentSurface();
}  // namespace rhythm::android_host
