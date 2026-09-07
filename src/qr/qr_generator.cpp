// Adapted from GammaRay's QR wrapper: Created by RGAA on 2024/4/8.
// First-party provenance: provenance/gammaray_qr.json. Nayuki remains MIT.
#include <stdexcept>

#include "qrcodegen.hpp"
#include "rhythm/qr/generator.h"

namespace rhythm::qr {
Image Generate(std::string_view payload, std::uint32_t maximum_extent) {
    if (payload.empty() || payload.size() > 1024) throw std::invalid_argument("qr.payload_limit");
    if (maximum_extent < 64 || maximum_extent > 2048)
        throw std::invalid_argument("qr.extent_limit");
    const std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
    const auto code = qrcodegen::QrCode::encodeBinary(bytes, qrcodegen::QrCode::Ecc::MEDIUM);
    Image image;
    image.symbol_modules_ = static_cast<std::uint32_t>(code.getSize());
    const auto modules = image.symbol_modules_ + 2 * image.quiet_modules_;
    image.pixels_per_module_ = maximum_extent / modules;
    if (image.pixels_per_module_ < 2) throw std::invalid_argument("qr.extent_too_small");
    image.width_ = modules * image.pixels_per_module_;
    image.rgba_.resize(static_cast<std::size_t>(image.width_) * image.width_ * 4, 255);
    for (std::uint32_t y = 0; y < image.symbol_modules_; ++y) {
        for (std::uint32_t x = 0; x < image.symbol_modules_; ++x) {
            if (!code.getModule(static_cast<int>(x), static_cast<int>(y))) continue;
            for (std::uint32_t dy = 0; dy < image.pixels_per_module_; ++dy) {
                const auto row = (y + image.quiet_modules_) * image.pixels_per_module_ + dy;
                for (std::uint32_t dx = 0; dx < image.pixels_per_module_; ++dx) {
                    const auto column = (x + image.quiet_modules_) * image.pixels_per_module_ + dx;
                    const auto index = (static_cast<std::size_t>(row) * image.width_ + column) * 4;
                    image.rgba_[index] = 0;
                    image.rgba_[index + 1] = 0;
                    image.rgba_[index + 2] = 0;
                }
            }
        }
    }
    return image;
}
}  // namespace rhythm::qr
