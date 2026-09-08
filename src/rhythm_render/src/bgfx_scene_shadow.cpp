#include "bgfx_scene_shadow.h"

namespace rhythm::render::detail {
BgfxSceneShadow::BgfxSceneShadow() {
    sampler_ = GpuHandle(bgfx::createUniform("s_scene_shadow", bgfx::UniformType::Sampler));
    matrix_ = GpuHandle(bgfx::createUniform("u_scene_shadow_matrix", bgfx::UniformType::Mat4));
    settings_ = GpuHandle(bgfx::createUniform("u_scene_shadow_settings", bgfx::UniformType::Vec4));
    filtering_ = GpuHandle(bgfx::createUniform("u_scene_shadow_filter", bgfx::UniformType::Vec4));
    constexpr std::uint32_t kWhite = 0xffffffff;
    white_ = GpuHandle(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0,
                                             bgfx::copy(&kWhite, sizeof(kWhite))));
}
void BgfxSceneShadow::Bind(const std::optional<SceneShadow>& shadow,
                           const SceneTextureResolver& resolve) const {
    constexpr auto flags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT |
                           BGFX_SAMPLER_MAG_POINT;
    bgfx::setTexture(4, sampler_.Get(), shadow ? resolve(shadow->depth_) : white_.Get(), flags);
    const auto data = shadow.value_or(SceneShadow{});
    const std::array settings{shadow ? float(data.light_) : -1.0f, data.depth_bias_,
                              data.normal_bias_, 1.0f / data.resolution_};
    const std::array filtering{data.filter_ ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(matrix_.Get(), data.world_to_clip_.data());
    bgfx::setUniform(settings_.Get(), settings.data());
    bgfx::setUniform(filtering_.Get(), filtering.data());
}
}  // namespace rhythm::render::detail
