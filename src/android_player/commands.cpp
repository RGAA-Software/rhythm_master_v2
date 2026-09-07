#include "commands.h"

#include <android/native_window_jni.h>
#include <jni.h>

#include <mutex>
#include <utility>

namespace rhythm::android_host {
namespace {
std::mutex mutex;
Commands pending;
std::string status = "Starting";
Surface surface;
struct NativeDeleter {
    void operator()(ANativeWindow* window) const {
        if (window) ANativeWindow_release(window);
    }
};
}  // namespace
Commands TakeCommands() {
    std::lock_guard lock(mutex);
    return std::exchange(pending, {});
}
void PublishStatus(std::string value) {
    std::lock_guard lock(mutex);
    status = std::move(value);
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
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeOpen(JNIEnv* env, jclass, jstring path) {
    // Paths are generated under app-private storage and are ASCII.
    const auto bytes = env->GetStringUTFChars(path, nullptr);
    if (!bytes) return false;
    bool accepted = false;
    {
        std::lock_guard lock(mutex);
        if (pending.package_path_.empty()) {
            pending.package_path_ = bytes;
            accepted = true;
        }
    }
    env->ReleaseStringUTFChars(path, bytes);
    return accepted;
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_rhythmmaster_player_PlayerActivity_nativeStatus(JNIEnv* env, jclass) {
    std::lock_guard lock(mutex);
    return env->NewStringUTF(status.c_str());
}
}  // namespace rhythm::android_host
