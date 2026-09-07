#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace rhythm::qr {
struct LuminanceView {
    std::span<const std::uint8_t> bytes_{};
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t row_stride_ = 0;
};
enum class ScanStatus { kFound, kNotFound, kInvalidFrame, kPayloadTooLarge, kFailed };
struct ScanResult {
    ScanStatus status_ = ScanStatus::kNotFound;
    std::string payload_{};
};
// Synchronous worker-side decode, no camera ownership or retained image borrows.
// One 8-bit luminance plane, full pitched rows, <= 1,048,576 logical pixels,
// sides <= 2048, stride <= 4096, buffer <= 4 MiB. Native camera adapters normalize
// incomplete/pixel-interleaved Y planes before calling. QR payload <= 1024 bytes;
// payload is opaque (possibly a credential), never logged or opened as a URL here.
ScanResult ReadLuminance(const LuminanceView& frame);
}  // namespace rhythm::qr
