#include <array>
#include <atomic>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "rhythm/storage/atomic_file.h"
#include "rhythm/storage/file_bytes.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template <typename Action>
void Reject(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid file range or source accepted");
}
void Run(const std::filesystem::path& directory) {
    using rhythm::storage::FileBytes;
    std::filesystem::create_directories(directory);
    const auto path = directory / "file-bytes.bin";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        std::array<char, 65536> block{};
        for (int index = 0; index < 512; ++index) {
            block.fill(static_cast<char>(index % 251));
            output.write(block.data(), block.size());
        }
    }
    Reject([&] { FileBytes::Open(path, 1024); });
    auto file = FileBytes::Open(path, 32 * 1024 * 1024);
    Check(file.Size() == 32 * 1024 * 1024, "large file size");
    const auto range = file.Slice(17 * 65536 + 123, 131072);
    const auto nested = range.Slice(0, 1024);
    file = {};
    std::array<std::uint8_t, 2048> bytes{};
    Check(nested.Read(0, bytes) == 1024 && bytes[0] == 17 && bytes[1023] == 17,
          "slice must keep the open source alive and stop at its own end");
    Check(nested.Read(1024, bytes) == 0, "range EOF");
    Reject([&] { nested.Read(1025, bytes); });
    Reject([&] { nested.Slice(1000, 25); });
    Reject([&] { nested.Slice(std::numeric_limits<std::uint64_t>::max(), 2); });
    Reject([&] { FileBytes{}.Read(0, bytes); });
    std::atomic<bool> valid{true};
    const auto read = [&](std::uint64_t offset, std::uint8_t expected) {
        try {
            std::array<std::uint8_t, 1024> block{};
            for (int index = 0; index < 1000; ++index) {
                if (range.Read(offset, block) != block.size()) valid = false;
                for (auto byte : block)
                    if (byte != expected) valid = false;
            }
        } catch (const std::exception&) {
            valid = false;
        }
    };
    {
        std::jthread first([&] { read(0, 17); });
        std::jthread second([&] { read(65536, 18); });
    }
    Check(valid, "shared readers must not race the native file cursor");
    const auto replacement = directory / "file-bytes-replacement.bin";
    rhythm::storage::WriteDurable(replacement, "replacement");
    try {
        rhythm::storage::Replace(replacement, path);
    } catch (const std::exception&) {
#ifdef _WIN32
        std::cerr << "native replacement error=" << GetLastError() << '\n';
#endif
        throw;
    }
    Check(nested.Read(0, bytes) == 1024 && bytes[0] == 17,
          "atomic replacement must preserve the running reader's old file lease");
    Check(FileBytes::Open(path, 1024).Size() == 11, "subsequent readers must open replacement");
    Reject([&] { rhythm::storage::Replace(directory / "missing-source", path); });
    Check(FileBytes::Open(path, 1024).Size() == 11 && nested.Read(0, bytes) == 1024 &&
                  bytes[0] == 17,
          "failed replacement must preserve published content and existing readers");
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) throw std::invalid_argument("file_bytes_tests output");
        Run(std::filesystem::path(argv[1]));
        std::cout << "File ranges: 32 MiB, shared lifetime, EOF, bounds and concurrent reads "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
