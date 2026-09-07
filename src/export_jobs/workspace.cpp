#include "workspace.h"

#include <array>
#include <chrono>
#include <fstream>
#include <random>

namespace rhythm::exporting::detail {
Workspace::Workspace(const std::filesystem::path& destination) {
    const auto parent = std::filesystem::absolute(destination).parent_path();
    std::filesystem::create_directories(parent);
    if (std::filesystem::exists(destination)) throw std::runtime_error("export.destination_exists");
    std::random_device random;
    for (int attempt = 0; attempt < 16; ++attempt) {
        const auto candidate = parent / (".rhythm-export-" + std::to_string(random()) + "-" +
                                         std::to_string(random()));
        if (std::filesystem::create_directory(candidate)) {
            directory_ = candidate;
            return;
        }
    }
    throw std::runtime_error("export.workspace_unavailable");
}
Workspace::~Workspace() {
    if (!directory_.empty()) {
        std::error_code ignored;
        // Only the exact directory successfully created by this lease is owned.
        std::filesystem::remove_all(directory_, ignored);
    }
}
void Workspace::CopyMusic(const std::filesystem::path& source, std::stop_token stop) const {
    if (!std::filesystem::is_regular_file(source)) throw std::runtime_error("export.music_missing");
    const auto size = std::filesystem::file_size(source);
    const auto modified = std::filesystem::last_write_time(source);
    if (!size || size > 2ULL * 1024 * 1024 * 1024) throw std::runtime_error("export.music_size");
    std::ifstream input(source, std::ios::binary);
    input.exceptions(std::ios::failbit | std::ios::badbit);
    std::ofstream output(directory_ / "music.source", std::ios::binary);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    std::array<char, 65536> buffer{};
    std::uintmax_t copied = 0;
    while (copied < size) {
        if (stop.stop_requested()) throw std::runtime_error("export.canceled");
        const auto count = static_cast<std::streamsize>(
                std::min<std::uintmax_t>(buffer.size(), size - copied));
        input.read(buffer.data(), count);
        output.write(buffer.data(), count);
        copied += static_cast<std::uintmax_t>(count);
    }
    output.flush();
    if (std::filesystem::file_size(source) != size ||
        std::filesystem::last_write_time(source) != modified)
        throw std::runtime_error("export.music_changed");
}
}  // namespace rhythm::exporting::detail
