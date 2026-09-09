#include "rhythm/project/performance_store.h"

#include <initializer_list>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <utility>

#include "rhythm/storage/atomic_file.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::project {
namespace {
using Json = nlohmann::json;
constexpr std::size_t kMaximumBytes = 64 * 1024;
void Keys(const Json& object, std::initializer_list<std::string_view> keys) {
    if (!object.is_object() || object.size() != keys.size())
        throw std::invalid_argument("performance.fields");
    for (const auto key : keys)
        if (!object.contains(key)) throw std::invalid_argument("performance.fields");
}
std::uint64_t Unsigned(const Json& value) {
    if (!value.is_number_unsigned()) throw std::invalid_argument("performance.integer");
    return value.get<std::uint64_t>();
}
std::string Read(const std::filesystem::path& path) {
    const auto file = storage::FileBytes::Open(path, kMaximumBytes);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file.Size()));
    if (file.Read(0, bytes) != bytes.size()) throw std::runtime_error("performance.short_read");
    return {bytes.begin(), bytes.end()};
}
void CheckPath(const std::filesystem::path& directory) {
    for (const auto& path :
         {directory, directory / ".writer", directory / "list.json", directory / "list.pending"})
        if (std::filesystem::is_symlink(path)) throw std::invalid_argument("performance.symlink");
}
class StagedList final {
   public:
    explicit StagedList(std::filesystem::path path) : path_(std::move(path)) {}
    ~StagedList() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    StagedList(const StagedList&) = delete;
    StagedList& operator=(const StagedList&) = delete;
    const std::filesystem::path& Path() const { return path_; }

   private:
    std::filesystem::path path_{};
};
void Fault(PerformanceCommitStep current, PerformanceCommitStep requested) {
    if (current == requested) throw std::runtime_error("performance.injected_failure");
}
}  // namespace
std::string EncodePerformanceList(const performance::List& list) {
    Json entries = Json::array();
    for (const auto& entry : list.Entries()) {
        const auto& work = entry.work_;
        entries.push_back(
                {{"id", entry.id_},
                 {"title", entry.title_},
                 {"transition_seconds", entry.transition_seconds_},
                 {"quantization", entry.quantization_ == parameters::Quantization::kImmediate
                                          ? "immediate"
                                  : entry.quantization_ == parameters::Quantization::kBeat ? "beat"
                                                                                           : "bar"},
                 {"work",
                  {{"source",
                    work.source_ == performance::WorkSource::kBuiltin ? "builtin" : "managed"},
                   {"content_id", work.content_id_},
                   {"version", work.version_},
                   {"package_sha256", work.package_.sha256_},
                   {"policy", work.policy_ == performance::VersionPolicy::kExact
                                      ? "exact"
                                      : "current_builtin"}}}});
    }
    const auto bytes = Json{{"format", "rhythm.performance"},
                            {"version", 1},
                            {"title", list.Title()},
                            {"last_id", list.LastId()},
                            {"entries", std::move(entries)}}
                               .dump(4);
    if (bytes.size() > kMaximumBytes) throw std::length_error("performance.bytes");
    return bytes;
}
performance::List DecodePerformanceList(std::string_view bytes) {
    if (bytes.size() > kMaximumBytes) throw std::length_error("performance.bytes");
    std::vector<std::set<std::string>> keys;
    const auto root = Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 8) throw std::length_error("performance.depth");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::key &&
            !keys.back().insert(value.get<std::string>()).second)
            throw std::invalid_argument("performance.duplicate_key");
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        return true;
    });
    Keys(root, {"format", "version", "title", "last_id", "entries"});
    if (root.at("format") != "rhythm.performance" || Unsigned(root.at("version")) != 1)
        throw std::invalid_argument("performance.version");
    const auto& records = root.at("entries");
    if (!records.is_array() || records.size() > performance::List::kMaximumItems)
        throw std::length_error("performance.entry_count");
    std::vector<performance::ListEntry> entries;
    for (const auto& record : records) {
        Keys(record, {"id", "title", "transition_seconds", "quantization", "work"});
        const auto& work = record.at("work");
        Keys(work, {"source", "content_id", "version", "package_sha256", "policy"});
        performance::ListEntry entry;
        entry.id_ = Unsigned(record.at("id"));
        entry.title_ = record.at("title").get<std::string>();
        if (!record.at("transition_seconds").is_number())
            throw std::invalid_argument("performance.duration");
        entry.transition_seconds_ = record.at("transition_seconds").get<double>();
        const auto mode = record.at("quantization").get<std::string>();
        if (mode == "immediate")
            entry.quantization_ = parameters::Quantization::kImmediate;
        else if (mode == "beat")
            entry.quantization_ = parameters::Quantization::kBeat;
        else if (mode == "bar")
            entry.quantization_ = parameters::Quantization::kBar;
        else
            throw std::invalid_argument("performance.quantization");
        const auto source = work.at("source").get<std::string>();
        if (source == "builtin")
            entry.work_.source_ = performance::WorkSource::kBuiltin;
        else if (source == "managed")
            entry.work_.source_ = performance::WorkSource::kManaged;
        else
            throw std::invalid_argument("performance.source");
        const auto policy = work.at("policy").get<std::string>();
        if (policy == "exact")
            entry.work_.policy_ = performance::VersionPolicy::kExact;
        else if (policy == "current_builtin")
            entry.work_.policy_ = performance::VersionPolicy::kCurrentBuiltin;
        else
            throw std::invalid_argument("performance.policy");
        entry.work_.content_id_ = work.at("content_id").get<std::string>();
        entry.work_.version_ = work.at("version").get<std::string>();
        entry.work_.package_.sha256_ = work.at("package_sha256").get<std::string>();
        entries.push_back(std::move(entry));
    }
    return performance::List(root.at("title").get<std::string>(), std::move(entries),
                             Unsigned(root.at("last_id")));
}
performance::List LoadPerformanceList(const std::filesystem::path& directory) {
    CheckPath(directory);
    return DecodePerformanceList(Read(directory / "list.json"));
}
void SavePerformanceList(const std::filesystem::path& directory, const performance::List& list,
                         PerformanceCommitStep fail_after) {
    const auto bytes = EncodePerformanceList(list);
    std::filesystem::create_directories(directory);
    CheckPath(directory);
    const storage::WriteGuard guard(directory);
    const StagedList staged(directory / "list.pending");
    storage::WriteDurable(staged.Path(), bytes);
    Fault(PerformanceCommitStep::kWritten, fail_after);
    if (DecodePerformanceList(Read(staged.Path())) != list)
        throw std::runtime_error("performance.reopen_validation");
    Fault(PerformanceCommitStep::kValidated, fail_after);
    storage::Replace(staged.Path(), directory / "list.json");
}
}  // namespace rhythm::project
