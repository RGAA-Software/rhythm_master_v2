#include <android/log.h>
#include <jni.h>

#include "control_bridge_state.h"

namespace rhythm::android_host {
// JNI primitive arguments are synchronous borrowed boundary inputs only.
extern "C" JNIEXPORT jboolean JNICALL Java_org_rhythmmaster_player_BeatControls_nativeGrid(
        JNIEnv*, jclass, jlong version, jboolean enabled, jdouble bpm, jint beats, jint unit,
        jdouble origin) {
    const parameters::BeatSettings settings{bpm, static_cast<std::uint32_t>(beats),
                                            static_cast<std::uint32_t>(unit), origin};
    if (enabled && !parameters::ValidBeatSettings(settings)) return false;
    auto& state = detail::ControlBridge();
    std::lock_guard lock(state.mutex_);
    if (static_cast<std::uint64_t>(version) != state.generation_) return false;
    state.edits_.grid_changed_ = true;
    state.edits_.grid_ = enabled ? std::optional(settings) : std::nullopt;
    if (!enabled) state.mode_ = parameters::Quantization::kImmediate;
    state.taps_.Reset();
    return true;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_BeatControls_nativeMode(JNIEnv*, jclass, jlong version, jint mode) {
    if (mode < 0 || mode > 2) return false;
    auto& state = detail::ControlBridge();
    std::lock_guard lock(state.mutex_);
    if (static_cast<std::uint64_t>(version) != state.generation_) return false;
    const auto& grid = state.edits_.grid_changed_ ? state.edits_.grid_ : state.grid_;
    if (mode != 0 && !grid) return false;
    state.mode_ = static_cast<parameters::Quantization>(mode);
    __android_log_print(ANDROID_LOG_INFO, "RhythmPerformance", "mode=%d generation=%llu", mode,
                        static_cast<unsigned long long>(state.generation_));
    return true;
}
extern "C" JNIEXPORT jdouble JNICALL Java_org_rhythmmaster_player_BeatControls_nativeTap(
        JNIEnv*, jclass, jlong version, jdouble seconds) {
    try {
        auto& state = detail::ControlBridge();
        std::lock_guard lock(state.mutex_);
        if (static_cast<std::uint64_t>(version) != state.generation_) return -1;
        auto grid = state.edits_.grid_changed_ ? state.edits_.grid_ : state.grid_;
        if (!grid) return -1;
        const auto bpm = state.taps_.Tap(seconds, grid->beat_unit_);
        if (!bpm) return 0;
        grid->bpm_ = *bpm;
        state.edits_.grid_changed_ = true;
        state.edits_.grid_ = grid;
        return *bpm;
    } catch (const std::exception&) {
        return -1;
    }
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_BeatControls_nativeCancel(JNIEnv*, jclass, jlong version, jlong id) {
    auto& state = detail::ControlBridge();
    std::lock_guard lock(state.mutex_);
    if (static_cast<std::uint64_t>(version) != state.generation_) return false;
    for (const auto& action : state.actions_)
        if (action.id_ == static_cast<std::uint64_t>(id) &&
            (action.state_ == player::PerformanceActionState::kPending ||
             action.state_ == player::PerformanceActionState::kDispatched)) {
            state.edits_.cancel_ = action.id_;
            return true;
        }
    return false;
}
}  // namespace rhythm::android_host
