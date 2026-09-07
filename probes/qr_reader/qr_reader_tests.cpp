#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "qrcodegen.hpp"
#include "rhythm/qr/async_reader.h"
#include "rhythm/qr/generator.h"
#include "rhythm/qr/reader.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("qr.reader_contract");
}
}  // namespace
int main() {
    using namespace rhythm::qr;
    try {
        const auto start = std::chrono::steady_clock::now();
        const std::string payload = "rhythmmaster://join/1/" + std::string(187, 'A');
        // Decoder treats this as opaque text; protocol validation is separate.
        const auto image = Generate(payload, 512);
        const auto size = image.width_;
        std::vector<std::uint8_t> luminance(size * size);
        for (std::size_t index = 0; index < luminance.size(); ++index)
            luminance[index] = image.rgba_[index * 4];
        const auto validate = [&](const std::vector<std::uint8_t>& bytes, std::uint32_t stride) {
            const auto decoded = ReadLuminance({bytes, size, size, stride});
            Check(decoded.status_ == ScanStatus::kFound && decoded.payload_ == payload);
        };
        validate(luminance, size);
        auto changed = luminance;
        std::transform(changed.begin(), changed.end(), changed.begin(),
                       [](auto byte) { return static_cast<std::uint8_t>(255 - byte); });
        validate(changed, size);
        std::transform(luminance.begin(), luminance.end(), changed.begin(),
                       [](auto byte) { return static_cast<std::uint8_t>(byte ? 96 : 16); });
        validate(changed, size);
        for (std::uint32_t y = 0; y < size; ++y)
            for (std::uint32_t x = 0; x < size; ++x)
                changed[y * size + x] = luminance[(size - x - 1) * size + y];
        validate(changed, size);
        const auto stride = size + 32;
        std::vector<std::uint8_t> padded(stride * size, 81);
        for (std::uint32_t y = 0; y < size; ++y)
            std::copy_n(luminance.begin() + y * size, size, padded.begin() + y * stride);
        validate(padded, stride);
        Check(ReadLuminance({padded, size, size, size - 1}).status_ == ScanStatus::kInvalidFrame);
        padded.pop_back();
        Check(ReadLuminance({padded, size, size, stride}).status_ == ScanStatus::kInvalidFrame);
        Check(ReadLuminance({}).status_ == ScanStatus::kInvalidFrame);
        Check(ReadLuminance({luminance, 2048, 2048, 2048}).status_ == ScanStatus::kInvalidFrame);
        changed.assign(luminance.size(), 255);
        Check(ReadLuminance({changed, size, size, size}).status_ == ScanStatus::kNotFound);
        // Independent encoder boundary: our public generator deliberately rejects
        // payloads over 1024, so construct this untrusted input in the test only.
        const auto oversized = qrcodegen::QrCode::encodeBinary(std::vector<std::uint8_t>(1025, 'x'),
                                                               qrcodegen::QrCode::Ecc::MEDIUM);
        const auto oversized_size = static_cast<std::uint32_t>((oversized.getSize() + 8) * 3);
        std::vector<std::uint8_t> oversized_pixels(oversized_size * oversized_size, 255);
        for (int y = 0; y < oversized.getSize(); ++y)
            for (int x = 0; x < oversized.getSize(); ++x)
                if (oversized.getModule(x, y))
                    for (int dy = 0; dy < 3; ++dy)
                        for (int dx = 0; dx < 3; ++dx)
                            oversized_pixels[static_cast<std::size_t>((y + 4) * 3 + dy) *
                                                     oversized_size +
                                             static_cast<std::size_t>((x + 4) * 3 + dx)] = 0;
        Check(ReadLuminance({oversized_pixels, oversized_size, oversized_size, oversized_size})
                      .status_ == ScanStatus::kPayloadTooLarge);
        AsyncReader scanner;
        Check(scanner.Submit({luminance, size, size, size}, 0) == ScanSubmit::kInactive);
        Check(scanner.Begin(1) && !scanner.Begin(1));
        Check(scanner.Submit({luminance, size, size, size}, 0) == ScanSubmit::kAccepted);
        Check(scanner.Submit({luminance, size, size, size}, 0) == ScanSubmit::kBusy);
        // Caller may release/change its camera buffer immediately after Submit.
        auto saved = luminance;
        std::fill(luminance.begin(), luminance.end(), std::uint8_t{255});
        const auto drain = [&] {
            std::optional<CompletedScan> result;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (scanner.Busy() && std::chrono::steady_clock::now() < deadline) {
                if (auto ready = scanner.Take()) result = std::move(ready);
                if (scanner.Busy()) std::this_thread::yield();
            }
            Check(!scanner.Busy());
            return result;
        };
        const auto first = drain().value();
        Check(first.generation_ == 1 && first.result_.payload_ == payload);
        Check(scanner.Submit({saved, size, size, size}, 199'999) == ScanSubmit::kRateLimited);
        Check(scanner.Submit({saved, size, size, size}, 200'000) == ScanSubmit::kAccepted);
        Check(scanner.Begin(2));
        Check(!drain());  // An old generation cannot publish into the new scan UI.
        Check(scanner.Submit({saved, size, size, size}, 200'000) == ScanSubmit::kRateLimited);
        Check(scanner.Submit({saved, size, size, size}, 199'999) == ScanSubmit::kInvalidTime);
        Check(scanner.Submit({saved, size, size, size}, 400'000) == ScanSubmit::kAccepted);
        const auto second = drain().value();
        Check(second.generation_ == 2 && second.result_.payload_ == payload);
        for (std::uint64_t generation = 3; generation <= 12; ++generation) {
            Check(scanner.Begin(generation));
            Check(scanner.Submit({saved, size, size, size},
                                 static_cast<std::int64_t>(generation) * 200'000) ==
                  ScanSubmit::kAccepted);
            scanner.Cancel();
            Check(!drain());
        }
        {
            AsyncReader exiting;
            Check(exiting.Begin(1));
            Check(exiting.Submit({saved, size, size, size}, 0) == ScanSubmit::kAccepted);
        }  // Destruction joins in-flight native decoding before buffers/executor are released.
        const auto elapsed =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                        .count();
        std::cout
                << "QR_READER variants=5 bounded_frame_checks=passed async_lifecycle=passed pixels="
                << size * size << " elapsed_ms=" << elapsed << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
