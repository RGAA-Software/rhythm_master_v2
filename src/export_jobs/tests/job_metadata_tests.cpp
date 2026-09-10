#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "job_files.h"
#include "rhythm/storage/atomic_file.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
#ifdef _WIN32
// Native ownership stays in this synchronous test boundary. This reproduces
// the access held by an atomic publisher, without relying on a timing race.
struct HandleClose {
    void operator()(void* handle) const { CloseHandle(handle); }
};
void VerifyPublisherSharing(const std::filesystem::path& directory) {
    const auto path = directory / "progress.json";
    const auto opened = CreateFileW(path.c_str(), DELETE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(opened != INVALID_HANDLE_VALUE, "publisher handle unavailable");
    const std::unique_ptr<void, HandleClose> publisher(opened);
    {
        std::ifstream legacy(path, std::ios::binary);
        Require(!legacy.is_open(), "legacy stream unexpectedly permits publisher DELETE access");
    }
    const auto progress = rhythm::exporting::detail::ReadProgress(directory);
    Require(progress && progress->completed_frames_ == 3,
            "progress reader conflicts with the atomic publisher");
}
#endif
}  // namespace

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Require(argc == 2, "metadata_tests output");
        const auto root =
                std::filesystem::path(argv[1]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        Require(!exporting::detail::ReadProgress(root), "missing progress must remain advisory");
        exporting::detail::WriteProgress(root, {3, 480, 1024});
#ifdef _WIN32
        VerifyPublisherSharing(root);
#endif
        exporting::detail::WriteProgress(root, {480, 480, 1024});
        const auto completed = exporting::detail::ReadProgress(root);
        Require(completed && completed->completed_frames_ == 480, "final progress was lost");
        exporting::detail::WriteResult(root, {false, "worker.failure"});
        const auto result = exporting::detail::ReadResult(root);
        Require(!result.success_ && result.error_ == "worker.failure", "worker failure was hidden");
        for (const auto& invalid :
             {std::string("{"), std::string(8193, 'x'),
              std::string(R"({"completed":481,"total":480,"texture_bytes":0})")}) {
            storage::WriteDurable(root / "progress.json", invalid);
            bool rejected = false;
            try {
                static_cast<void>(exporting::detail::ReadProgress(root));
            } catch (const std::exception&) {
                rejected = true;
            }
            Require(rejected, "invalid progress was silently accepted");
        }
        std::cout << "Publisher sharing, completion, worker failures and metadata bounds passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
