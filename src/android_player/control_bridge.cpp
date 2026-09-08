#include "control_bridge.h"

#include <jni.h>

#include <mutex>
#include <nlohmann/json.hpp>

namespace rhythm::android_host {
namespace {
std::mutex control_mutex;
parameters::ControlBank control_bank;
parameters::ControlValues control_values;
std::optional<parameters::ControlSequence> control_sequence;
double control_seconds = 0;
std::uint64_t generation = 0;
}  // namespace
void PublishControls(const parameters::ControlBank& bank,
                     const std::optional<parameters::ControlSequence>& sequence) {
    std::lock_guard lock(control_mutex);
    control_bank = bank;
    control_values.clear();
    control_sequence = sequence;
    control_seconds = 0;
    ++generation;
}
void PublishControlTime(double seconds) {
    std::lock_guard lock(control_mutex);
    control_seconds = seconds;
}
parameters::ControlValues CurrentControls() {
    std::lock_guard lock(control_mutex);
    return control_values;
}
// JNI arguments/local references are synchronous borrowed boundary values.
extern "C" JNIEXPORT jstring JNICALL
Java_org_rhythmmaster_player_PerformanceControls_nativeDescribe(JNIEnv* env, jclass) {
    try {
        std::lock_guard lock(control_mutex);
        const auto values = parameters::EvaluateControls(control_bank, control_sequence,
                                                         control_seconds, control_values);
        nlohmann::json result{{"generation", generation},
                              {"automated", control_sequence.has_value()},
                              {"overridden", !control_values.empty()},
                              {"cue", ""},
                              {"controls", nlohmann::json::array()},
                              {"snapshots", nlohmann::json::array()}};
        for (const auto& control : control_bank.Definitions())
            result["controls"].push_back({{"id", std::to_string(control.id_)},
                                          {"title", control.title_},
                                          {"minimum", control.minimum_},
                                          {"maximum", control.maximum_},
                                          {"value", values.at(control.id_)}});
        if (control_sequence)
            if (const auto active = control_sequence->Active(control_seconds))
                for (const auto& cue : control_sequence->Cues())
                    if (cue.id_ == *active) result["cue"] = cue.title_;
        for (const auto& snapshot : control_bank.Snapshots())
            result["snapshots"].push_back(
                    {{"id", std::to_string(snapshot.id_)}, {"title", snapshot.title_}});
        // JSON ASCII escapes also avoid modified-UTF8 ambiguities in JNI.
        const auto json = result.dump(-1, ' ', true);
        return env->NewStringUTF(json.c_str());
    } catch (const std::exception&) {
        return env->NewStringUTF("{}");
    }
}
extern "C" JNIEXPORT jboolean JNICALL Java_org_rhythmmaster_player_PerformanceControls_nativeValue(
        JNIEnv*, jclass, jlong version, jlong id, jdouble value) {
    try {
        std::lock_guard lock(control_mutex);
        if (static_cast<std::uint64_t>(version) != generation) return false;
        auto next = control_values;
        next[static_cast<parameters::ControlId>(id)] = value;
        (void)control_bank.Resolve(next);
        control_values = std::move(next);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
extern "C" JNIEXPORT jboolean JNICALL Java_org_rhythmmaster_player_PerformanceControls_nativeBlend(
        JNIEnv*, jclass, jlong version, jlong first, jlong second, jdouble amount) {
    try {
        std::lock_guard lock(control_mutex);
        if (static_cast<std::uint64_t>(version) != generation) return false;
        control_values = control_bank.Blend(
                control_bank.Snapshot(static_cast<std::uint64_t>(first)),
                control_bank.Snapshot(static_cast<std::uint64_t>(second)), amount);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_PerformanceControls_nativeFollow(JNIEnv*, jclass, jlong version) {
    std::lock_guard lock(control_mutex);
    if (static_cast<std::uint64_t>(version) != generation) return false;
    control_values.clear();
    return true;
}
}  // namespace rhythm::android_host
