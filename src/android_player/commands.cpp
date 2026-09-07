#include "commands.h"

#include <android/native_window_jni.h>
#include <jni.h>

#include <cmath>
#include <mutex>
#include <utility>

namespace rhythm::android_host {
namespace {
std::mutex mutex;
Commands pending;
std::string status = "Starting";
Surface surface;
double playback_seconds = 0;
double playback_duration = 0;
struct NativeDeleter {
    void operator()(ANativeWindow* window) const {
        if (window) ANativeWindow_release(window);
    }
};
struct Utf8Deleter {
    // Borrowed JNI call context and local reference; this lease never escapes
    // the synchronous native entry point that acquired the UTF-8 characters.
    JNIEnv* environment_ = nullptr;
    jstring string_ = nullptr;
    void operator()(const char* bytes) const {
        environment_->ReleaseStringUTFChars(string_, bytes);
    }
};
bool QueuePath(JNIEnv& environment, jstring path, std::string& target) {
    if (!path) return false;
    std::unique_ptr<const char, Utf8Deleter> bytes(environment.GetStringUTFChars(path, nullptr),
                                                   Utf8Deleter{&environment, path});
    if (!bytes) return false;
    try {
        std::lock_guard lock(mutex);
        if (!target.empty()) return false;
        target = bytes.get();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
}  // namespace
Commands TakeCommands() {
    std::lock_guard lock(mutex);
    return std::exchange(pending, {});
}
void PublishStatus(std::string value) {
    std::lock_guard lock(mutex);
    status = std::move(value);
}
void PublishPlayback(double seconds, std::optional<double> duration) {
    std::lock_guard lock(mutex);
    playback_seconds = seconds;
    playback_duration = duration.value_or(0);
}
Surface CurrentSurface() {
    std::lock_guard lock(mutex);
    return surface;
}
extern "C" JNIEXPORT void JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeSurface(JNIEnv* env, jclass, jobject value) {
    std::lock_guard lock(mutex);
    ++surface.generation_;
    surface.window_ = 0;
    surface.owner_.reset();
    if (value) {
        // JNI transfers an acquired reference; renderer owners keep it alive.
        std::shared_ptr<ANativeWindow> window(ANativeWindow_fromSurface(env, value),
                                              NativeDeleter{});
        surface.window_ = reinterpret_cast<std::uintptr_t>(window.get());
        surface.owner_ = std::move(window);
    }
}
// JNI borrows arguments for the synchronous call. No Java objects are retained.
extern "C" JNIEXPORT void JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeCommand(JNIEnv*, jclass, jint command) {
    std::lock_guard lock(mutex);
    if (command == 1) pending.toggle_pause_ = !pending.toggle_pause_;
    if (command == 2) pending.restart_ = true;
    if (command == 3) pending.focus_pause_ = true;
    if (command == 20 || command == 21) pending.music_loop_ = command == 21;
    if (command >= 10 && command <= 12)
        pending.render_quality_ = static_cast<player::RenderQuality>(command - 10);
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeOpen(JNIEnv* env, jclass, jstring path) {
    // Paths are generated under app-private storage and are ASCII.
    return QueuePath(*env, path, pending.package_path_);
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeStatus(JNIEnv* env, jclass) {
    std::lock_guard lock(mutex);
    return env->NewStringUTF(status.c_str());
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeMusic(JNIEnv* env, jclass, jstring path) {
    return QueuePath(*env, path, pending.music_path_);
}
extern "C" JNIEXPORT void JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeSeek(JNIEnv*, jclass, jdouble seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400 * 7) return;
    std::lock_guard lock(mutex);
    pending.seek_seconds_ = seconds;
}
extern "C" JNIEXPORT jdouble JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativePosition(JNIEnv*, jclass) {
    std::lock_guard lock(mutex);
    return playback_seconds;
}
extern "C" JNIEXPORT jdouble JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeDuration(JNIEnv*, jclass) {
    std::lock_guard lock(mutex);
    return playback_duration;
}
}  // namespace rhythm::android_host
