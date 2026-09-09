#pragma once

#include <map>

#include "rhythm/player/performance_program.h"

namespace rhythm::android_host {
struct BuiltinWork {
    performance::WorkReference reference_{};
    std::string asset_{};
};
std::vector<BuiltinWork> ReadBuiltinWorks(std::string_view catalog);
// Reader owns only values. Its SDL stream, staged file and asset-store resources
// remain inside the synchronous worker callback, independent of the Host lifetime.
player::BuiltinWorkReader BuiltinReader(std::filesystem::path directory,
                                        std::vector<BuiltinWork> works);
}  // namespace rhythm::android_host
