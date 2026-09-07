#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "point_ops.h"

namespace rhythm::runtime::detail {
render::Texture CreatePointSprite(render::Renderer& renderer) {
    std::array<std::uint8_t, 64 * 64 * 4> pixels{};
    for (std::size_t y = 0; y < 64; ++y)
        for (std::size_t x = 0; x < 64; ++x) {
            const auto radius = std::hypot((static_cast<double>(x) + 0.5 - 32) / 32,
                                           (static_cast<double>(y) + 0.5 - 32) / 32);
            const auto alpha = std::clamp((1 - radius) * 5, 0.0, 1.0);
            const auto index = (y * 64 + x) * 4;
            pixels[index] = pixels[index + 1] = pixels[index + 2] = 255;
            pixels[index + 3] = static_cast<std::uint8_t>(alpha * 255 + 0.5);
        }
    return renderer.CreateTexture({64, 64}, pixels);
}
void DrawPoints(std::span<const particles::Point> points, render::TextureHandle sprite,
                render::BlendMode blend, render::DrawList& list) {
    if (points.size() > particles::kMaximumPoints) throw std::length_error("runtime.points");
    list.vertices_.reserve(list.vertices_.size() + points.size() * 4);
    list.indices_.reserve(list.indices_.size() + points.size() * 6);
    const auto first_index = static_cast<std::uint32_t>(list.indices_.size());
    const auto channel = [](float value) {
        return static_cast<std::uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255 + 0.5f);
    };
    for (const auto& point : points) {
        if (point.size_ <= 0 || point.color_.alpha_ <= 0) continue;
        if (!std::isfinite(point.x_) || !std::isfinite(point.y_) || !std::isfinite(point.size_) ||
            !std::isfinite(point.rotation_))
            throw std::invalid_argument("runtime.points");
        const float x = point.x_ * list.width_;
        const float y = point.y_ * list.height_;
        const float half = point.size_ * list.height_ * 0.5f;
        if (x + half * 1.5f < 0 || y + half * 1.5f < 0 || x - half * 1.5f > list.width_ ||
            y - half * 1.5f > list.height_)
            continue;
        const auto cosine = std::cos(point.rotation_);
        const auto sine = std::sin(point.rotation_);
        const auto color = channel(point.color_.red_) | (channel(point.color_.green_) << 8) |
                           (channel(point.color_.blue_) << 16) |
                           (channel(point.color_.alpha_) << 24);
        const auto base = static_cast<std::uint32_t>(list.vertices_.size());
        for (const auto uv :
             std::array<std::array<float, 2>, 4>{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}}) {
            const auto dx = (uv[0] * 2 - 1) * half;
            const auto dy = (uv[1] * 2 - 1) * half;
            list.vertices_.push_back({x + cosine * dx - sine * dy, y + sine * dx + cosine * dy,
                                      uv[0], uv[1], color});
        }
        list.indices_.insert(list.indices_.end(),
                             {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    const auto count = static_cast<std::uint32_t>(list.indices_.size()) - first_index;
    if (count)
        list.commands_.push_back(
                {sprite, first_index, count, {0, 0, list.width_, list.height_}, blend});
}
}  // namespace rhythm::runtime::detail
