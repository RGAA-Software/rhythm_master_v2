#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace rhythm::android_host {
struct Commands {
    bool toggle_pause_ = false;
    bool restart_ = false;
    std::string package_path_{};
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
