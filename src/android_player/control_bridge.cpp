#include "control_bridge.h"

#include <jni.h>

#include <mutex>
#include <nlohmann/json.hpp>

namespace rhythm::android_host {
namespace {
std::mutex control_mutex;
parameters::ControlBank control_bank;
parameters::ControlValues control_values;
std::uint64_t generation = 0;
}  // namespace
void PublishControls(const parameters::ControlBank& bank) {
    std::lock_guard lock(control_mutex);
    control_bank = bank;
    control_values = bank.Resolve();
    ++generation;
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
        nlohmann::json result{{"generation", generation},
                              {"controls", nlohmann::json::array()},
                              {"snapshots", nlohmann::json::array()}};
        for (const auto& control : control_bank.Definitions())
            result["controls"].push_back({{"id", std::to_string(control.id_)},
                                          {"title", control.title_},
                                          {"minimum", control.minimum_},
                                          {"maximum", control.maximum_},
                                          {"value", control_values.at(control.id_)}});
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
        control_values = control_bank.Resolve(next);
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
}  // namespace rhythm::android_host
