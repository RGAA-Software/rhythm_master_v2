#include "control_bridge.h"

#include <android/log.h>
#include <jni.h>

#include <nlohmann/json.hpp>
#include <utility>

#include "control_bridge_state.h"

namespace rhythm::android_host {
namespace detail {
ControlBridgeState& ControlBridge() {
    static ControlBridgeState state;
    return state;
}
}  // namespace detail
namespace {
void FillFrame(detail::ControlBridgeState& state, const player::SceneDeck& deck) {
    state.seconds_ = deck.Current().Seconds();
    state.values_ = deck.Current().CurrentControls();
    state.overridden_ = deck.HasControlOverrides();
    state.grid_ = deck.BeatGrid();
    const std::array actions{deck.ActionStatus(player::PerformanceActionKind::kSnapshot),
                             deck.ActionStatus(player::PerformanceActionKind::kNextScene)};
    for (std::size_t index = 0; index < actions.size(); ++index)
        if (actions[index] != state.actions_[index]) {
            const auto& action = actions[index];
            __android_log_print(
                    ANDROID_LOG_INFO, "RhythmPerformance",
                    "id=%llu kind=%d target=%llu state=%d reason=%d due=%.6f seconds=%.6f",
                    static_cast<unsigned long long>(action.id_), static_cast<int>(action.kind_),
                    static_cast<unsigned long long>(action.target_),
                    static_cast<int>(action.state_), static_cast<int>(action.reason_),
                    action.due_seconds_.value_or(0), state.seconds_);
        }
    state.actions_ = actions;
}
}  // namespace
void PublishControls(const player::SceneDeck& deck) {
    auto& state = detail::ControlBridge();
    {
        std::lock_guard lock(state.mutex_);
        state.bank_ = deck.Current().Controls();
        state.sequence_ = deck.Current().ControlSequence();
        state.mode_ = parameters::Quantization::kImmediate;
        state.taps_.Reset();
        state.edits_ = {};
        ++state.generation_;
        FillFrame(state, deck);
    }
}
void PublishControlFrame(const player::SceneDeck& deck) {
    auto& state = detail::ControlBridge();
    std::lock_guard lock(state.mutex_);
    FillFrame(state, deck);
}
void ApplyControlCommands(player::SceneDeck& deck) {
    auto& state = detail::ControlBridge();
    const auto edits = [&] {
        std::lock_guard lock(state.mutex_);
        return std::exchange(state.edits_, {});
    }();
    if (edits.grid_changed_) deck.SetBeatGrid(edits.grid_);
    if (edits.follow_) deck.FollowCues();
    if (!edits.values_.empty()) deck.EditControls(edits.values_);
    if (edits.recall_) deck.RequestSnapshot(*edits.recall_, edits.recall_mode_);
    if (edits.cancel_) deck.CancelAction(*edits.cancel_);
}
parameters::Quantization CurrentQuantization() {
    auto& state = detail::ControlBridge();
    std::lock_guard lock(state.mutex_);
    return state.mode_;
}
// JNI arguments/local references are synchronous borrowed boundary values.
extern "C" JNIEXPORT jstring JNICALL
Java_org_rhythmmaster_player_PerformanceControls_nativeDescribe(JNIEnv* env, jclass) {
    try {
        auto& state = detail::ControlBridge();
        std::lock_guard lock(state.mutex_);
        const auto values = state.bank_.Resolve(state.values_);
        nlohmann::json result{
                {"generation", state.generation_},     {"automated", state.sequence_.has_value()},
                {"overridden", state.overridden_},     {"cue", ""},
                {"seconds", state.seconds_},           {"mode", static_cast<int>(state.mode_)},
                {"taps", state.taps_.Count()},         {"grid", nullptr},
                {"actions", nlohmann::json::array()},  {"controls", nlohmann::json::array()},
                {"snapshots", nlohmann::json::array()}};
        if (state.grid_) {
            const auto& grid = *state.grid_;
            const auto position = parameters::BeatGrid(grid).Position(state.seconds_);
            result["grid"] = {{"bpm", grid.bpm_},         {"beats", grid.beats_per_bar_},
                              {"unit", grid.beat_unit_},  {"origin", grid.origin_seconds_},
                              {"bar", position.bar_ + 1}, {"beat", position.beat_in_bar_ + 1}};
        }
        for (const auto& action : state.actions_)
            result["actions"].push_back({{"id", std::to_string(action.id_)},
                                         {"target", std::to_string(action.target_)},
                                         {"kind", static_cast<int>(action.kind_)},
                                         {"state", static_cast<int>(action.state_)},
                                         {"reason", static_cast<int>(action.reason_)},
                                         {"due", action.due_seconds_.value_or(0)}});
        for (const auto& control : state.bank_.Definitions())
            result["controls"].push_back({{"id", std::to_string(control.id_)},
                                          {"title", control.title_},
                                          {"minimum", control.minimum_},
                                          {"maximum", control.maximum_},
                                          {"value", values.at(control.id_)}});
        if (state.sequence_)
            if (const auto active = state.sequence_->Active(state.seconds_))
                for (const auto& cue : state.sequence_->Cues())
                    if (cue.id_ == *active) result["cue"] = cue.title_;
        for (const auto& snapshot : state.bank_.Snapshots())
            result["snapshots"].push_back(
                    {{"id", std::to_string(snapshot.id_)}, {"title", snapshot.title_}});
        const auto json = result.dump(-1, ' ', true);
        return env->NewStringUTF(json.c_str());
    } catch (const std::exception&) {
        return env->NewStringUTF("{}");
    }
}
extern "C" JNIEXPORT jboolean JNICALL Java_org_rhythmmaster_player_PerformanceControls_nativeValue(
        JNIEnv*, jclass, jlong version, jlong id, jdouble value) {
    try {
        auto& state = detail::ControlBridge();
        std::lock_guard lock(state.mutex_);
        if (static_cast<std::uint64_t>(version) != state.generation_) return false;
        auto changes = state.edits_.values_;
        changes[static_cast<parameters::ControlId>(id)] = value;
        (void)state.bank_.Resolve(changes);
        state.edits_.values_ = std::move(changes);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
extern "C" JNIEXPORT jboolean JNICALL Java_org_rhythmmaster_player_PerformanceControls_nativeBlend(
        JNIEnv*, jclass, jlong version, jlong first, jlong second, jdouble amount) {
    try {
        auto& state = detail::ControlBridge();
        std::lock_guard lock(state.mutex_);
        if (static_cast<std::uint64_t>(version) != state.generation_) return false;
        state.edits_.values_ =
                state.bank_.Blend(state.bank_.Snapshot(static_cast<std::uint64_t>(first)),
                                  state.bank_.Snapshot(static_cast<std::uint64_t>(second)), amount);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
extern "C" JNIEXPORT jboolean JNICALL Java_org_rhythmmaster_player_PerformanceControls_nativeRecall(
        JNIEnv*, jclass, jlong version, jlong id) {
    try {
        auto& state = detail::ControlBridge();
        std::lock_guard lock(state.mutex_);
        if (static_cast<std::uint64_t>(version) != state.generation_) return false;
        (void)state.bank_.Snapshot(static_cast<std::uint64_t>(id));
        state.edits_.recall_ = static_cast<std::uint64_t>(id);
        state.edits_.recall_mode_ = state.mode_;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_PerformanceControls_nativeFollow(JNIEnv*, jclass, jlong version) {
    auto& state = detail::ControlBridge();
    std::lock_guard lock(state.mutex_);
    if (static_cast<std::uint64_t>(version) != state.generation_) return false;
    state.edits_.follow_ = true;
    state.edits_.values_.clear();
    state.edits_.recall_.reset();
    return true;
}
}  // namespace rhythm::android_host
