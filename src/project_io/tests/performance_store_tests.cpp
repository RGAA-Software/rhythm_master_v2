#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "rhythm/project/performance_store.h"
#include "rhythm/storage/atomic_file.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid performance document accepted");
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::invalid_argument("test directory required");
        const std::filesystem::path directory(argv[1]);
        performance::List list("舞台 / Stage");
        const performance::WorkReference work{performance::WorkSource::kBuiltin,
                                              "official.templates.aureate_vortex",
                                              "0.4.0",
                                              {std::string(64, 'a')},
                                              performance::VersionPolicy::kCurrentBuiltin};
        Check(list.Append({0, work, "鎏光流涡", 2, parameters::Quantization::kBar}) == 1 &&
                      list.Append({0, work, "Encore", 0, parameters::Quantization::kBeat}) == 2,
              "cannot construct test list");
        Check(list.Move(2, 0), "cannot reorder");
        project::SavePerformanceList(directory, list);
        Check(project::LoadPerformanceList(directory) == list, "list round trip lost settings");
        auto edited = list;
        Check(edited.Remove(2), "cannot remove");
        for (const auto phase : {project::PerformanceCommitStep::kWritten,
                                 project::PerformanceCommitStep::kValidated}) {
            Reject([&] { project::SavePerformanceList(directory, edited, phase); });
            Check(project::LoadPerformanceList(directory) == list &&
                          !std::filesystem::exists(directory / "list.pending"),
                  "failed save changed committed document or leaked staging");
        }
        {
            const storage::WriteGuard guard(directory);
            Reject([&] { project::SavePerformanceList(directory, edited); });
        }
        Check(project::LoadPerformanceList(directory) == list, "writer conflict lost old list");
        project::SavePerformanceList(directory, edited);
        auto reopened = project::LoadPerformanceList(directory);
        Check(reopened.LastId() == 2 && reopened.Append(list.Entries().front()) == 3,
              "persistent allocation watermark reused deleted ID");
        using Json = nlohmann::json;
        const auto encoded = project::EncodePerformanceList(list);
        const auto original = Json::parse(encoded);
        auto invalid = original;
        invalid["version"] = 2;
        Reject([&] { project::DecodePerformanceList(invalid.dump()); });
        invalid = original;
        invalid["future_setting"] = true;
        Reject([&] { project::DecodePerformanceList(invalid.dump()); });
        for (const auto& value : {Json(-1), Json(1.5), Json("1"), Json(true)}) {
            invalid = original;
            invalid["entries"][0]["id"] = value;
            Reject([&] { project::DecodePerformanceList(invalid.dump()); });
        }
        invalid = original;
        invalid["entries"][1]["id"] = invalid["entries"][0]["id"];
        Reject([&] { project::DecodePerformanceList(invalid.dump()); });
        invalid = original;
        invalid["entries"][0]["work"]["source"] = "cache";
        Reject([&] { project::DecodePerformanceList(invalid.dump()); });
        invalid = original;
        invalid["entries"][0]["work"]["content_id"] = "/data/cache/temp.rhythmpack";
        Reject([&] { project::DecodePerformanceList(invalid.dump()); });
        Reject([&] { project::DecodePerformanceList("{\"version\":1," + encoded.substr(1)); });
        Reject([&] { project::DecodePerformanceList(std::string(65537, ' ')); });
        storage::WriteDurable(directory / "list.json", "truncated{");
        Reject([&] { project::LoadPerformanceList(directory); });
        project::SavePerformanceList(directory, list);
        Check(project::LoadPerformanceList(directory) == list, "explicit recovery failed");
        std::cout << "performance persistence, identity, strict decoding and atomic recovery "
                     "passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
