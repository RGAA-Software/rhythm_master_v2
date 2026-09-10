#pragma once

#include <filesystem>
#include <functional>

#include "rhythm/studio/studio.h"

namespace rhythm::testing {
// Drives the existing Studio sliders with mouse events, then checks authored
// defaults through save/reopen/publication. Cue playback is a separate contract.
void CheckControlDelivery(studio::Studio& studio, const std::filesystem::path& project,
                          const std::filesystem::path& package,
                          const std::filesystem::path& captures,
                          const std::function<void()>& frame);
}  // namespace rhythm::testing
