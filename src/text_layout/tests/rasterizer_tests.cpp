#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

#include "rhythm/text/rasterizer.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <class Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid text input accepted");
}
unsigned FirstX(const rhythm::text::Mask& mask) {
    unsigned result = mask.width_;
    for (unsigned y = 0; y < mask.height_; ++y)
        for (unsigned x = 0; x < mask.width_; ++x)
            if (mask.coverage_[std::size_t(y) * mask.width_ + x]) result = std::min(result, x);
    return result;
}
}  // namespace
int main(int argc, char** argv) {
    try {
        Check(argc == 2 || argc == 3, "font path required");
        std::ifstream file(argv[1], std::ios::binary);
        Check(bool(file), "font fixture missing");
        std::vector<std::uint8_t> font{std::istreambuf_iterator<char>(file), {}};
        rhythm::text::Rasterizer rasterizer(font);
        rhythm::text::Layout layout;
        layout.text_ = "棱镜星莲 / Rhythm\n音乐：Pulse，2026！";
        const auto mask = rasterizer.Render(layout);
        Check(mask.missing_glyphs_ == 0 && mask.lines_ == 2 && !mask.clipped_, "mixed CJK layout");
        Check(std::count_if(mask.coverage_.begin(), mask.coverage_.end(),
                            [](auto value) { return value > 0; }) > 1000,
              "text rendered blank");
        const auto rasterizations = rasterizer.Stats().rasterizations_;
        Check(rasterizer.Render(layout).coverage_ == mask.coverage_, "unstable repeated render");
        Check(rasterizer.Stats().rasterizations_ == rasterizations, "glyph cache missed");
        if (argc == 3) {
            std::ofstream image(argv[2], std::ios::binary);
            image << "P5\n" << mask.width_ << ' ' << mask.height_ << "\n255\n";
            for (const auto value : mask.coverage_) image.put(static_cast<char>(value));
            Check(bool(image), "capture write failed");
        }
        layout.text_ = "棱镜星莲";
        layout.width_ = 512;
        const auto left = FirstX(rasterizer.Render(layout));
        layout.alignment_ = rhythm::text::Alignment::kCenter;
        const auto center = FirstX(rasterizer.Render(layout));
        layout.alignment_ = rhythm::text::Alignment::kRight;
        const auto right = FirstX(rasterizer.Render(layout));
        Check(center > left + 100 && right > center + 100, "alignment ignored");
        layout.alignment_ = rhythm::text::Alignment::kLeft;
        layout.width_ = 96;
        Check(rasterizer.Render(layout).lines_ == 2, "CJK wrap");
        layout.wrap_ = false;
        Check(rasterizer.Render(layout).clipped_, "overflow not reported");
        layout.width_ = 512;
        layout.text_ = "\xf4\x8f\xbf\xbf";
        Check(rasterizer.Render(layout).missing_glyphs_ == 1, "missing glyph not reported");
        layout.text_ = "\xc0\xaf";
        Reject([&] { rasterizer.Render(layout); });
        layout.text_ = std::string(4097, 'A');
        Reject([&] { rasterizer.Render(layout); });
        layout.text_ = "A";
        layout.line_spacing_ = std::numeric_limits<float>::quiet_NaN();
        Reject([&] { rasterizer.Render(layout); });
        layout.line_spacing_ = 1.2f;
        layout.width_ = 2049;
        Reject([&] { rasterizer.Render(layout); });
        Reject([&] { rhythm::text::Rasterizer invalid(std::vector<std::uint8_t>{1, 2, 3}); });
        std::cout << "Mixed CJK/Latin glyphs, wrapping, alignment, cache, missing glyph and budget "
                     "checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
