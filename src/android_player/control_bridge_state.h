#pragma once

#include <mutex>

#include "rhythm/player/scene_deck.h"

namespace rhythm::android_host::detail {
struct ControlEdits {
    parameters::ControlValues values_{};
    bool follow_ = false;
    std::optional<std::uint64_t> recall_{};
    parameters::Quantization recall_mode_ = parameters::Quantization::kImmediate;
    std::optional<std::uint64_t> cancel_{};
    bool grid_changed_ = false;
    std::optional<parameters::BeatSettings> grid_{};
};
// Adapter-only mutex protects UI/render exchange. JNI threads edit bounded value
// commands; only the render thread owns and mutates the actual SceneDeck.
struct ControlBridgeState {
    std::mutex mutex_{};
    parameters::ControlBank bank_{};
    parameters::ControlValues values_{};
    std::optional<parameters::ControlSequence> sequence_{};
    double seconds_ = 0;
    std::uint64_t generation_ = 0;
    bool overridden_ = false;
    std::optional<parameters::BeatSettings> grid_{};
    parameters::Quantization mode_ = parameters::Quantization::kImmediate;
    parameters::TapTempo taps_{};
    std::array<player::PerformanceAction, 2> actions_{};
    ControlEdits edits_{};
};
ControlBridgeState& ControlBridge();
}  // namespace rhythm::android_host::detail
