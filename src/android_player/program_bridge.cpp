#include "program_bridge.h"

#include <jni.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <nlohmann/json.hpp>
#include <utility>

#include "rhythm/project/performance_store.h"

namespace rhythm::android_host {
namespace {
std::mutex program_mutex;
ProgramCommand pending_program;
std::string program_description = "{}";
bool ValidText(const std::string& text, std::size_t maximum) {
    return text.size() <= maximum && text.find('\0') == std::string::npos;
}
}  // namespace
ProgramCommand TakeProgramCommand() {
    const std::lock_guard lock(program_mutex);
    return std::exchange(pending_program, {});
}
void PublishProgram(const player::PerformanceProgram& program, std::string_view request_error) {
    auto root = nlohmann::json::parse(project::EncodePerformanceList(program.Draft()));
    for (auto& entry : root["entries"])
        entry["id"] = std::to_string(entry["id"].get<std::uint64_t>());
    root["busy"] = program.Busy();
    root["dirty"] = program.Status().dirty_;
    root["saved"] = program.Status().saved_;
    root["error"] = request_error.empty() ? program.Status().error_ : std::string(request_error);
    const auto text = root.dump(-1, ' ', true);
    const std::lock_guard lock(program_mutex);
    program_description = text;
}
// JNI pointers are borrowed only at this synchronous boundary. Requests use
// UTF-8 byte arrays; responses are ASCII-escaped JSON for NewStringUTF.
extern "C" JNIEXPORT jstring JNICALL
Java_org_rhythmmaster_player_PerformanceProgramDialog_nativeDescribe(JNIEnv* env, jclass) {
    const std::lock_guard lock(program_mutex);
    return env->NewStringUTF(program_description.c_str());
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_rhythmmaster_player_PerformanceProgramDialog_nativeAction(JNIEnv* env, jclass,
                                                                   jbyteArray bytes) {
    try {
        if (!bytes) return false;
        const auto size = env->GetArrayLength(bytes);
        if (size < 1 || size > 8192) return false;
        std::vector<jbyte> buffer(static_cast<std::size_t>(size));
        env->GetByteArrayRegion(bytes, 0, size, buffer.data());
        if (env->ExceptionCheck()) return false;
        const auto root = nlohmann::json::parse(std::string(buffer.begin(), buffer.end()));
        if (!root.at("action").is_number_integer()) return false;
        ProgramCommand command;
        command.action_ = root.at("action").get<int>();
        if (command.action_ < 1 || command.action_ > 11) return false;
        const auto id = root.value("id", std::string("0"));
        if (id.empty() || id.size() > 20 || !std::all_of(id.begin(), id.end(), [](char value) {
                return value >= '0' && value <= '9';
            }))
            return false;
        command.id_ = std::stoull(id);
        command.content_id_ = root.value("content_id", std::string{});
        command.title_ = root.value("title", std::string{});
        command.path_ = root.value("path", std::string{});
        if (root.contains("duration") && !root.at("duration").is_number()) return false;
        command.duration_ = root.value("duration", 1.0);
        if (root.contains("mode") && !root.at("mode").is_number_integer()) return false;
        const auto mode = root.value("mode", 0);
        if (mode < 0 || mode > 2 || !std::isfinite(command.duration_) || command.duration_ < 0 ||
            command.duration_ > 5 || !ValidText(command.content_id_, 128) ||
            !ValidText(command.title_, 512) || !ValidText(command.path_, 4096))
            return false;
        command.mode_ = static_cast<parameters::Quantization>(mode);
        command.follow_ = root.value("follow", true);
        const std::lock_guard lock(program_mutex);
        if (pending_program.action_) return false;
        pending_program = std::move(command);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
}  // namespace rhythm::android_host
