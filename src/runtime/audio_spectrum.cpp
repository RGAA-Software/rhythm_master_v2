#include "audio_spectrum.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "rhythm/graph/registry.h"

namespace rhythm::runtime::detail {
namespace {
std::uint32_t Color(graph::Color color) {
    const auto byte = [](double value) {
        return static_cast<std::uint32_t>(std::clamp(value, 0.0, 1.0) * 255 + 0.5);
    };
    return byte(color.r_) | byte(color.g_) << 8 | byte(color.b_) << 16 | byte(color.a_) << 24;
}
}  // namespace
std::span<const float> SpectrumBands(const graph::Node& node, const ExternalInputs& inputs) {
    if (!inputs.audio_ || !inputs.audio_->valid_) return {};
    const auto channel = graph::Scalar(node, "audio_channel", 0);
    const auto& frame = *inputs.audio_;
    return channel == 1   ? std::span(frame.left_bands_)
           : channel == 2 ? std::span(frame.right_bands_)
                          : std::span(frame.mono_bands_);
}
float SpectrumSample(std::span<const float> bands, double index) {
    if (bands.empty() || !std::isfinite(index)) return 0;
    const auto sample = std::clamp(index, 0.0, static_cast<double>(bands.size() - 1));
    const auto lower = static_cast<std::size_t>(sample);
    const auto upper = std::min(lower + 1, bands.size() - 1);
    const auto value =
            bands[lower] +
            (bands[upper] - bands[lower]) * static_cast<float>(sample - static_cast<double>(lower));
    return std::isfinite(value) ? value : 0;
}
void DrawSpectrum(const graph::Node& node, std::span<const float> bands,
                  render::TextureHandle white, render::DrawList& list) {
    if (bands.empty()) return;
    const auto count = static_cast<std::uint32_t>(graph::Scalar(node, "bar_count", 63));
    const auto gain = static_cast<float>(graph::Scalar(node, "spectrum_gain", 2));
    const auto gap = static_cast<float>(graph::Scalar(node, "bar_gap", 0.2));
    const auto radius = static_cast<float>(graph::Scalar(node, "spectrum_radius", 0.2));
    const bool radial = graph::Scalar(node, "spectrum_layout", 0) == 1;
    const auto top = Color(graph::ColorValue(node, "color_a", {0.05, 0.9, 0.8, 1}));
    const auto bottom = Color(graph::ColorValue(node, "color_b", {0.2, 0.1, 0.8, 1}));
    const auto first_index = static_cast<std::uint32_t>(list.indices_.size());
    list.vertices_.reserve(list.vertices_.size() + count * 4);
    list.indices_.reserve(list.indices_.size() + count * 6);
    for (std::uint32_t bar = 0; bar < count; ++bar) {
        const double sample =
                static_cast<double>(bar) * static_cast<double>(bands.size() - 1) / (count - 1);
        const float amount = std::clamp(SpectrumSample(bands, sample) * gain, 0.0f, 1.0f);
        if (amount == 0) continue;
        const auto base = static_cast<std::uint32_t>(list.vertices_.size());
        if (!radial) {
            const float cell = list.width_ / static_cast<float>(count);
            const float left = cell * (static_cast<float>(bar) + gap * 0.5f);
            const float right = left + cell * (1 - gap);
            const float upper_y = list.height_ * (1 - amount * 0.9f);
            list.vertices_.insert(list.vertices_.end(), {{left, upper_y, 0, 0, top},
                                                         {right, upper_y, 1, 0, top},
                                                         {right, list.height_, 1, 1, bottom},
                                                         {left, list.height_, 0, 1, bottom}});
        } else {
            const float size = std::min(list.width_, list.height_);
            const float inner = size * radius;
            const float outer = inner + size * (0.48f - radius) * amount;
            const float angle = static_cast<float>(bar) * 2 * std::numbers::pi_v<float> /
                                static_cast<float>(count);
            const float half_angle =
                    std::numbers::pi_v<float> / static_cast<float>(count) * (1 - gap);
            const float c1 = std::cos(angle - half_angle), s1 = std::sin(angle - half_angle);
            const float c2 = std::cos(angle + half_angle), s2 = std::sin(angle + half_angle);
            const float x = list.width_ * 0.5f, y = list.height_ * 0.5f;
            list.vertices_.insert(list.vertices_.end(),
                                  {{x + c1 * outer, y + s1 * outer, 0, 0, top},
                                   {x + c2 * outer, y + s2 * outer, 1, 0, top},
                                   {x + c2 * inner, y + s2 * inner, 1, 1, bottom},
                                   {x + c1 * inner, y + s1 * inner, 0, 1, bottom}});
        }
        list.indices_.insert(list.indices_.end(),
                             {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    const auto indices = static_cast<std::uint32_t>(list.indices_.size()) - first_index;
    if (indices)
        list.commands_.push_back({white, first_index, indices, {0, 0, list.width_, list.height_}});
}
}  // namespace rhythm::runtime::detail
