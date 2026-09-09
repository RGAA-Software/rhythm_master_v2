#pragma once

#include "builtin_works.h"
#include "import_file.h"

namespace rhythm::android_host {
struct ProgramCommand {
    int action_ = 0;
    std::uint64_t id_ = 0;
    std::string content_id_{};
    std::string title_{};
    std::string path_{};
    double duration_ = 1;
    parameters::Quantization mode_ = parameters::Quantization::kImmediate;
    bool follow_ = true;
};
ProgramCommand TakeProgramCommand();
void PublishProgram(const player::PerformanceProgram& program, std::string_view request_error);
bool ApplyProgramCommand(const ProgramCommand& command, player::PerformanceProgram& program,
                         std::span<const BuiltinWork> works,
                         const std::filesystem::path& canonical_cache,
                         std::optional<ImportFile>& owned_import);
}  // namespace rhythm::android_host
