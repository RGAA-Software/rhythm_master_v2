#include "bgfx_scene_lights.h"

#include <cmath>
#include <numbers>
namespace rhythm::render::detail {
BgfxSceneLights::BgfxSceneLights() {
    constexpr std::array names{"u_scene_light_directions", "u_scene_light_colors",
                               "u_scene_light_positions", "u_scene_spot_directions",
                               "u_scene_light_ranges"};
    for (std::size_t i = 0; i < names.size(); ++i)
        uniforms_[i] = GpuHandle(bgfx::createUniform(names[i], bgfx::UniformType::Vec4, 4));
}
void BgfxSceneLights::Set(const SceneDrawList& list) {
    directions_ = {};
    colors_ = {};
    positions_ = {};
    spot_directions_ = {};
    ranges_ = {};
    std::size_t i = 0;
    for (const auto& light : list.lights_) {
        directions_[i] = {light.direction_[0], light.direction_[1], light.direction_[2], 0};
        colors_[i] = {light.radiance_[0], light.radiance_[1], light.radiance_[2], 0};
        ++i;
    }
    for (const auto& light : list.positional_lights_) {
        colors_[i] = {light.radiance_[0], light.radiance_[1], light.radiance_[2], 0};
        positions_[i] = {light.position_[0], light.position_[1], light.position_[2],
                         light.spot_ ? 2.0f : 1.0f};
        spot_directions_[i] = {light.direction_[0], light.direction_[1], light.direction_[2],
                               std::cos(light.cone_angle_ * std::numbers::pi_v<float> / 180)};
        ranges_[i] = {1 / light.range_, light.decay_, light.cone_decay_, 0};
        ++i;
    }
}
void BgfxSceneLights::Bind() const {
    bgfx::setUniform(uniforms_[0].Get(), directions_.data(), 4);
    bgfx::setUniform(uniforms_[1].Get(), colors_.data(), 4);
    bgfx::setUniform(uniforms_[2].Get(), positions_.data(), 4);
    bgfx::setUniform(uniforms_[3].Get(), spot_directions_.data(), 4);
    bgfx::setUniform(uniforms_[4].Get(), ranges_.data(), 4);
}
}  // namespace rhythm::render::detail
