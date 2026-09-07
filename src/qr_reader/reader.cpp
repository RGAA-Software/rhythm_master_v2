#include "rhythm/qr/reader.h"

#include <ReadBarcode.h>

#include <exception>

#include "frame_validation.h"

namespace rhythm::qr {
ScanResult ReadLuminance(const LuminanceView& frame) {
    if (!detail::ValidLuminance(frame)) return {ScanStatus::kInvalidFrame, {}};
    try {
        const auto options = ZXing::ReaderOptions()
                                     .setFormats(ZXing::BarcodeFormat::QRCode)
                                     .setTryHarder(true)
                                     .setTryRotate(true)
                                     .setTryInvert(true)
                                     .setMaxNumberOfSymbols(1);
        // ZXing's borrowed image address exists only during this synchronous
        // adapter call; public consumers supply a checked span, never a raw pointer.
        const auto result = ZXing::ReadBarcode(
                ZXing::ImageView(frame.bytes_.data(), static_cast<int>(frame.bytes_.size()),
                                 static_cast<int>(frame.width_), static_cast<int>(frame.height_),
                                 ZXing::ImageFormat::Lum, static_cast<int>(frame.row_stride_)),
                options);
        if (!result.isValid() || result.format() != ZXing::BarcodeFormat::QRCode)
            return {ScanStatus::kNotFound, {}};
        const auto& bytes = result.bytes();
        if (bytes.empty()) return {ScanStatus::kNotFound, {}};
        if (bytes.size() > 1024) return {ScanStatus::kPayloadTooLarge, {}};
        return {ScanStatus::kFound, {bytes.begin(), bytes.end()}};
    } catch (const std::exception&) {
        return {ScanStatus::kFailed, {}};
    }
}
}  // namespace rhythm::qr
