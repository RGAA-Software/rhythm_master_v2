#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>

#include "rhythm/qr/generator.h"

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("qr.contract:" + std::to_string(location.line()));
}
template <class Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Check(rejected);
}
void CheckImage(const rhythm::qr::Image& image, std::uint32_t maximum_extent) {
    Check(image.width_ <= maximum_extent && image.pixels_per_module_ >= 2 &&
          image.quiet_modules_ == 4 && image.symbol_modules_ >= 21 &&
          (image.symbol_modules_ - 17) % 4 == 0 &&
          image.rgba_.size() == static_cast<std::size_t>(image.width_) * image.width_ * 4);
    const auto border = image.quiet_modules_ * image.pixels_per_module_;
    for (std::uint32_t y = 0; y < image.width_; ++y) {
        for (std::uint32_t x = 0; x < image.width_; ++x) {
            const auto index = (static_cast<std::size_t>(y) * image.width_ + x) * 4;
            const auto value = image.rgba_[index];
            Check((value == 0 || value == 255) && image.rgba_[index + 1] == value &&
                  image.rgba_[index + 2] == value && image.rgba_[index + 3] == 255);
            if (x < border || y < border || x >= image.width_ - border ||
                y >= image.width_ - border)
                Check(value == 255);
            // Every module is a uniform square, even at non-divisible requested extents.
            const auto origin_x = x / image.pixels_per_module_ * image.pixels_per_module_;
            const auto origin_y = y / image.pixels_per_module_ * image.pixels_per_module_;
            Check(value ==
                  image.rgba_[(static_cast<std::size_t>(origin_y) * image.width_ + origin_x) * 4]);
        }
    }
    // Top-left finder: black ring, white ring, solid black 3x3 center.
    for (std::uint32_t y = 0; y < 7; ++y)
        for (std::uint32_t x = 0; x < 7; ++x) {
            const auto index = ((border + y * image.pixels_per_module_) * image.width_ + border +
                                x * image.pixels_per_module_) *
                               4;
            const bool black =
                    x == 0 || y == 0 || x == 6 || y == 6 || (x >= 2 && x <= 4 && y >= 2 && y <= 4);
            Check(image.rgba_[index] == (black ? 0 : 255));
        }
}
void WriteFixture(const std::filesystem::path& directory, const std::string& name,
                  const std::string& payload, std::uint32_t extent) {
    const auto image = rhythm::qr::Generate(payload, extent);
    CheckImage(image, extent);
    std::filesystem::create_directories(directory);
    std::ofstream pixels(directory / (name + ".pgm"), std::ios::binary);
    pixels.exceptions(std::ios::failbit | std::ios::badbit);
    pixels << "P5\n" << image.width_ << ' ' << image.width_ << "\n255\n";
    for (std::size_t index = 0; index < image.rgba_.size(); index += 4)
        pixels.put(static_cast<char>(image.rgba_[index]));
    std::ofstream expected(directory / (name + ".payload"), std::ios::binary);
    expected.exceptions(std::ios::failbit | std::ios::badbit);
    expected << payload;
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        using rhythm::qr::Generate;
        const auto directory = argc == 2 ? std::filesystem::path(argv[1]) : "qr-fixtures";
        Reject([] { Generate(""); });
        Reject([] { Generate(std::string(1025, 'x')); });
        Reject([] { Generate("x", 0); });
        Reject([] { Generate("x", 2049); });
        Reject([] { Generate(std::string(1024, 'x'), 64); });
        const auto first = Generate("portable QR");
        Check(first.rgba_ == Generate("portable QR").rgba_);
        Check(first.rgba_ != Generate("portable Qr").rgba_);
        CheckImage(Generate("x", 2048), 2048);
        WriteFixture(directory, "short", "rhythm://test-fixture/123", 257);
        WriteFixture(directory, "medium", std::string(256, 'A') + "0123456789", 511);
        WriteFixture(directory, "long", std::string(1024, 'z'), 1024);
        WriteFixture(directory, "binary", std::string("prefix\0suffix", 13), 512);
        std::cout << "QR contracts passed: bounded payload/pixels, quiet zone, whole modules, "
                     "binary payload, deterministic output\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
