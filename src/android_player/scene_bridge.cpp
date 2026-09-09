#include "scene_bridge.h"

#include <jni.h>

#include <cmath>
#include <mutex>
#include <nlohmann/json.hpp>
#include <utility>

#include "control_bridge.h"

namespace rhythm::android_host {
namespace {
std::mutex scene_mutex;
SceneCommands pending;
std::string description = "{}";
}  // namespace
SceneCommands TakeSceneCommands() {
    std::lock_guard lock(scene_mutex);
    return std::exchange(pending, {});
}
void PublishSceneQueue(const player::SceneQueue& queue, const player::SceneDeck& deck) {
    nlohmann::json result{
            {"items", nlohmann::json::array()},
            {"transitioning", deck.Transitioning()},
            {"progress", deck.Progress()},
            {"incoming", deck.IncomingTitle()},
            {"error", static_cast<int>(deck.Error())},
            {"can_go", deck.CanPrepareNext() && !queue.Items().empty() &&
                               queue.Items().front().state_ == player::ScenePreparation::kReady}};
    for (const auto& item : queue.Items())
        result["items"].push_back({{"id", std::to_string(item.id_)},
                                   {"title", item.title_},
                                   {"state", static_cast<int>(item.state_)}});
    auto text = result.dump(-1, ' ', true);
    std::lock_guard lock(scene_mutex);
    description = std::move(text);
}
// JNI values are borrowed only during the synchronous entry call. JSON arrives
// as standard UTF-8 bytes, avoiding modified-UTF8 conversion of authored titles.
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_SceneQueueDialog_nativeEnqueue(JNIEnv* env, jclass, jbyteArray bytes) {
    try {
        if (!bytes) return false;
        const auto length = env->GetArrayLength(bytes);
        if (length <= 0 || length > 8192) return false;
        std::vector<jbyte> buffer(static_cast<std::size_t>(length));
        env->GetByteArrayRegion(bytes, 0, length, buffer.data());
        if (env->ExceptionCheck()) return false;
        const std::string utf8(buffer.begin(), buffer.end());
        const auto request = nlohmann::json::parse(utf8);
        auto path = request.at("path").get<std::string>();
        auto title = request.at("title").get<std::string>();
        if (path.empty() || path.size() > 4096 || path.find('\0') != std::string::npos ||
            title.empty() || title.size() > 512 || title.find('\0') != std::string::npos)
            return false;
        std::lock_guard lock(scene_mutex);
        if (!pending.path_.empty()) return false;
        pending.path_ = std::move(path);
        pending.title_ = std::move(title);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_rhythmmaster_player_SceneQueueDialog_nativeDescribe(JNIEnv* env, jclass) {
    std::lock_guard lock(scene_mutex);
    return env->NewStringUTF(description.c_str());
}
extern "C" JNIEXPORT void JNICALL Java_org_rhythmmaster_player_SceneQueueDialog_nativeAction(
        JNIEnv*, jclass, jint action, jlong id, jdouble duration) {
    if (action < 1 || action > 5 || id < 0 || !std::isfinite(duration) || duration < 0 ||
        duration > 5)
        return;
    const auto mode = CurrentQuantization();
    std::lock_guard lock(scene_mutex);
    // One pending gesture; UI refreshes from the host snapshot after it applies.
    pending.action_ = action;
    pending.id_ = static_cast<std::uint64_t>(id);
    pending.duration_ = duration;
    pending.mode_ = mode;
}
}  // namespace rhythm::android_host
