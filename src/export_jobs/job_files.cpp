#include "job_files.h"

#include <nlohmann/json.hpp>
#include <vector>

#include "rhythm/storage/atomic_file.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::exporting::detail {
namespace {
nlohmann::json Read(const std::filesystem::path& path) {
    // The child atomically replaces progress.json. A reader must permit the
    // publisher's DELETE handle and retain one immutable file revision.
    const auto file = storage::FileBytes::Open(path, 8192);
    if (!file.Size()) throw std::runtime_error("export.job_metadata_size");
    std::vector<std::uint8_t> text(static_cast<std::size_t>(file.Size()));
    file.Read(0, text);
    return nlohmann::json::parse(text,
                                 [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
                                     if (depth > 4)
                                         throw std::runtime_error("export.job_metadata_depth");
                                     return true;
                                 });
}
void Write(const std::filesystem::path& path, const nlohmann::json& json) {
    auto temporary = path;
    temporary += ".tmp";
    storage::WriteDurable(temporary, json.dump());
    storage::Replace(temporary, path);
}
}  // namespace
void WriteRequest(const std::filesystem::path& directory, const ExportSettings& settings) {
    const auto& encoding = settings.encoding_;
    if (encoding.codec_ != media::VideoCodec::kH264 && encoding.codec_ != media::VideoCodec::kMpeg4)
        throw std::invalid_argument("export.job_codec");
    Write(directory / "request.json",
          {{"version", 1},
           {"width", encoding.width_},
           {"height", encoding.height_},
           {"fps", encoding.fps_},
           {"bitrate", encoding.bitrate_},
           {"audio", encoding.audio_},
           {"codec", encoding.codec_ == media::VideoCodec::kH264 ? "h264" : "mpeg4"},
           {"frames", settings.frames_},
           {"music", settings.music_.has_value()},
           {"gain", settings.gain_}});
}
ExportSettings ReadRequest(const std::filesystem::path& directory) {
    const auto json = Read(directory / "request.json");
    if (json.at("version") != 1) throw std::runtime_error("export.job_version");
    ExportSettings settings;
    auto& encoding = settings.encoding_;
    encoding.width_ = json.at("width").get<std::uint32_t>();
    encoding.height_ = json.at("height").get<std::uint32_t>();
    encoding.fps_ = json.at("fps").get<std::uint32_t>();
    encoding.bitrate_ = json.at("bitrate").get<std::uint32_t>();
    encoding.audio_ = json.at("audio").get<bool>();
    const auto codec = json.at("codec").get<std::string>();
    if (codec != "h264" && codec != "mpeg4") throw std::runtime_error("export.job_codec");
    encoding.codec_ = codec == "h264" ? media::VideoCodec::kH264 : media::VideoCodec::kMpeg4;
    settings.frames_ = json.at("frames").get<std::uint64_t>();
    settings.gain_ = json.at("gain").get<float>();
    if (json.at("music").get<bool>()) settings.music_ = directory / "music.source";
    return settings;
}
void WriteProgress(const std::filesystem::path& directory, ExportProgress progress) {
    try {
        Write(directory / "progress.json", {{"completed", progress.completed_frames_},
                                            {"total", progress.total_frames_},
                                            {"texture_bytes", progress.texture_bytes_}});
    } catch (const std::exception&) {
        // Progress is advisory. A Windows reader can briefly prevent replacement;
        // the next update retries. Final result and media errors remain mandatory.
    }
}
std::optional<ExportProgress> ReadProgress(const std::filesystem::path& directory) {
    const auto path = directory / "progress.json";
    if (!std::filesystem::exists(path)) return std::nullopt;
    const auto json = Read(path);
    ExportProgress progress{json.at("completed").get<std::uint64_t>(),
                            json.at("total").get<std::uint64_t>(),
                            json.at("texture_bytes").get<std::uint64_t>()};
    if (progress.completed_frames_ > progress.total_frames_ || !progress.total_frames_ ||
        progress.total_frames_ > 60 * 3600 || progress.texture_bytes_ > 256ULL * 1024 * 1024)
        throw std::runtime_error("export.job_progress");
    return progress;
}
void WriteResult(const std::filesystem::path& directory, const JobResult& result) {
    Write(directory / "result.json",
          {{"success", result.success_}, {"error", result.error_.substr(0, 2048)}});
}
JobResult ReadResult(const std::filesystem::path& directory) {
    const auto json = Read(directory / "result.json");
    return {json.at("success").get<bool>(), json.at("error").get<std::string>()};
}
}  // namespace rhythm::exporting::detail
