#pragma once
#include <cstdint>
namespace rhythm::render {
// Explicit transfer conversions operate on straight RGB inside the adapter;
// textures remain premultiplied and alpha never receives a gamma transform.
enum class ColorTransfer : std::uint8_t { kLinear, kSrgb };
enum class ToneMapping : std::uint8_t { kNone, kReinhard, kFilmic, kAces, kAgx };
struct ColorPipeline {
    ColorTransfer input_ = ColorTransfer::kLinear;
    ColorTransfer output_ = ColorTransfer::kLinear;
    ToneMapping tone_mapping_ = ToneMapping::kNone;
    float exposure_ = 0;
    float white_ = 32;
    float agx_contrast_ = 1.25f;
};
}  // namespace rhythm::render
