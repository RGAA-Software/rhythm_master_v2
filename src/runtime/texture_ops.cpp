#include "texture_ops.h"

#include <algorithm>
#include <stdexcept>

#include "affine.h"
#include "audio_spectrum.h"
#include "shapes.h"

namespace rhythm::runtime::detail {
namespace {
std::uint32_t Pack(graph::Color color) {
    const auto channel = [](double value) {
        return static_cast<std::uint32_t>(std::clamp(value, 0.0, 1.0) * 255 + 0.5);
    };
    return channel(color.r_) | (channel(color.g_) << 8) | (channel(color.b_) << 16) |
           (channel(color.a_) << 24);
}
}  // namespace
void AppendTextureQuad(render::DrawList& list, render::TextureHandle source, std::uint32_t top,
                       std::uint32_t bottom, double scale) {
    const auto x = list.width_ * static_cast<float>((1 - scale) * 0.5);
    const auto y = list.height_ * static_cast<float>((1 - scale) * 0.5);
    const auto base = static_cast<std::uint32_t>(list.vertices_.size());
    const auto first = static_cast<std::uint32_t>(list.indices_.size());
    list.vertices_.insert(list.vertices_.end(), {{x, y, 0, 0, top},
                                                 {list.width_ - x, y, 1, 0, top},
                                                 {list.width_ - x, list.height_ - y, 1, 1, bottom},
                                                 {x, list.height_ - y, 0, 1, bottom}});
    list.indices_.insert(list.indices_.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    list.commands_.push_back({source, first, 6, {0, 0, list.width_, list.height_}});
}

std::uint32_t DrawTexture(const graph::Instruction& instruction,
                          std::span<const NodeOutput> outputs, const ExternalInputs& external,
                          render::TextureHandle white, render::DrawList& list) {
    const auto& node = instruction.node_;
    const auto operation = instruction.operation_;
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        return outputs[instruction.inputs_.at(port).value()];
    };
    if (operation == graph::Operation::kTextureMapping) {
        AppendTextureQuad(list, input(0).texture_, 0xffffffff, 0xffffffff);
        const auto bound = [&](std::size_t port, std::string_view key, double fallback,
                               double minimum, double maximum) {
            return static_cast<float>(std::clamp(instruction.inputs_[port]
                                                         ? input(port).scalar_
                                                         : graph::Scalar(node, key, fallback),
                                                 minimum, maximum));
        };
        render::TextureMapping mapping;
        mapping.kind_ = graph::Scalar(node, "mapping_mode", 0) == 0
                                ? render::TextureMappingKind::kKaleidoscope
                                : render::TextureMappingKind::kPolar;
        mapping.rotation_ = bound(1, "rotation", 0, -36000, 36000);
        mapping.travel_ = bound(2, "travel", 0, -4096, 4096);
        mapping.twist_ = bound(3, "twist", 0, -16, 16);
        mapping.scale_ = bound(4, "scale", 1, 0.125, 16);
        mapping.sectors_ = static_cast<float>(graph::Scalar(node, "sectors", 8));
        mapping.radial_power_ = static_cast<float>(graph::Scalar(node, "radial_power", 1));
        list.commands_.back().texture_mapping_ = mapping;
    } else if (operation == graph::Operation::kTextureContours) {
        AppendTextureQuad(list, input(0).texture_, 0xffffffff, 0xffffffff);
        render::TextureContours contours;
        contours.count_ = static_cast<float>(graph::Scalar(node, "contour_count", 12));
        contours.width_ = static_cast<float>(graph::Scalar(node, "line_width", 0.12));
        contours.phase_ = static_cast<float>(std::clamp(
                instruction.inputs_[1] ? input(1).scalar_ : graph::Scalar(node, "phase", 0),
                -4096.0, 4096.0));
        const auto color = [&](std::string_view key, graph::Color fallback) {
            const auto value = graph::ColorValue(node, key, fallback);
            return std::array{static_cast<float>(value.r_), static_cast<float>(value.g_),
                              static_cast<float>(value.b_), static_cast<float>(value.a_)};
        };
        contours.color_a_ = color("color_a", {0.02, 0.6, 1, 1});
        contours.color_b_ = color("color_b", {1, 0.12, 0.35, 1});
        list.commands_.back().texture_contours_ = contours;
    } else if (operation == graph::Operation::kTextureNoise) {
        AppendTextureQuad(list, white, 0xffffffff, 0xffffffff);
        const auto color = [&](std::string_view key, graph::Color fallback) {
            const auto value = graph::ColorValue(node, key, fallback);
            return std::array{static_cast<float>(value.r_), static_cast<float>(value.g_),
                              static_cast<float>(value.b_), static_cast<float>(value.a_)};
        };
        render::TextureNoise noise;
        noise.scale_ = static_cast<float>(graph::Scalar(node, "noise_scale", 4));
        noise.phase_ = static_cast<float>(std::clamp(
                instruction.inputs_[0] ? input(0).scalar_ : graph::Scalar(node, "phase", 0), 0.0,
                4096.0));
        noise.contrast_ = static_cast<float>(graph::Scalar(node, "contrast", 1));
        noise.seed_ = static_cast<float>(graph::Scalar(node, "seed", 0));
        noise.color_a_ = color("color_a", {0.015, 0.03, 0.12, 1});
        noise.color_b_ = color("color_b", {0.12, 0.65, 0.8, 1});
        list.commands_.back().texture_noise_ = noise;
    } else if (operation == graph::Operation::kGradient) {
        auto first = graph::ColorValue(node, "color_a", {0, 0.5, 1, 1});
        auto second = graph::ColorValue(node, "color_b", {1, 0, 0.5, 1});
        const auto amount = instruction.inputs_[0] ? std::clamp(input(0).scalar_, 0.0, 1.0) : 1.0;
        const auto brightness = 0.25 + 0.75 * amount;
        first.r_ *= brightness;
        first.g_ *= brightness;
        first.b_ *= brightness;
        second.r_ *= brightness;
        second.g_ *= brightness;
        second.b_ *= brightness;
        AppendTextureQuad(list, white, Pack(first), Pack(second));
    } else if (operation == graph::Operation::kAudioSpectrum) {
        DrawSpectrum(node, SpectrumBands(node, external), white, list);
    } else if (operation == graph::Operation::kShape) {
        DrawShape(node, white, list);
    } else if (operation == graph::Operation::kMask) {
        AppendTextureQuad(list, input(0).texture_, 0xffffffff, 0xffffffff);
        AppendTextureQuad(list, input(1).texture_, 0xffffffff, 0xffffffff);
        list.commands_.back().blend_ = graph::Scalar(node, "mask_mode", 0) == 0
                                               ? render::BlendMode::kAlphaMask
                                               : render::BlendMode::kInverseAlphaMask;
    } else if (operation == graph::Operation::kComposite) {
        AppendTextureQuad(list, input(0).texture_, 0xffffffff, 0xffffffff);
        const auto amount =
                instruction.inputs_[2] ? input(2).scalar_ : graph::Scalar(node, "amount", 1);
        const auto tint = Pack({1, 1, 1, std::clamp(amount, 0.0, 1.0)});
        AppendTextureQuad(list, input(1).texture_, tint, tint);
        if (graph::Scalar(node, "composite_mode", 0) == 1)
            list.commands_.back().blend_ = render::BlendMode::kAdd;
    } else if (operation == graph::Operation::kColorAdjust) {
        AppendTextureQuad(list, input(0).texture_, 0xffffffff, 0xffffffff);
        const auto scalar = [&](std::size_t port, std::string_view key, double fallback,
                                double minimum, double maximum) {
            return static_cast<float>(std::clamp(instruction.inputs_[port]
                                                         ? input(port).scalar_
                                                         : graph::Scalar(node, key, fallback),
                                                 minimum, maximum));
        };
        list.commands_.back().color_adjustment_ = render::ColorAdjustment{
                scalar(1, "exposure", 0, -8, 8), scalar(2, "contrast", 1, 0, 4),
                scalar(3, "saturation", 1, 0, 4), scalar(4, "invert", 0, 0, 1)};
    } else if (operation == graph::Operation::kAffine) {
        DrawAffine(instruction, outputs, list);
    } else if (operation == graph::Operation::kTransform) {
        AppendTextureQuad(list, input(0).texture_, 0xffffffff, 0xffffffff,
                          graph::Scalar(node, "scale", 0.95));
    } else if (operation == graph::Operation::kBlend) {
        AppendTextureQuad(list, input(0).texture_, 0xffffffff, 0xffffffff);
        const auto tint = Pack({1, 1, 1, graph::Scalar(node, "amount", 0.25)});
        AppendTextureQuad(list, input(1).texture_, tint, tint);
    } else {
        throw std::invalid_argument("runtime.texture_operation");
    }
    return operation == graph::Operation::kGradient || operation == graph::Operation::kTransform ||
                           operation == graph::Operation::kBlend
                   ? 0x000000ff
                   : 0;
}
}  // namespace rhythm::runtime::detail
