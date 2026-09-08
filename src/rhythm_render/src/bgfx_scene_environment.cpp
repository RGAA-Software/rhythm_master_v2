#include "bgfx_scene_environment.h"

#include <cmath>
#include <numbers>

namespace rhythm::render::detail {
BgfxSceneEnvironment::BgfxSceneEnvironment() {
    sampler_ = GpuHandle(bgfx::createUniform("s_scene_environment", bgfx::UniformType::Sampler));
    settings_ = GpuHandle(bgfx::createUniform("u_scene_environment", bgfx::UniformType::Vec4));
    constexpr std::uint32_t kBlack = 0xff000000;
    fallback_ = GpuHandle(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0,
                                                bgfx::copy(&kBlack, sizeof(kBlack))));
}
void BgfxSceneEnvironment::Bind(const std::optional<SceneEnvironment>& environment,
                                const SceneTextureResolver& resolve) const {
    bgfx::setTexture(5, sampler_.Get(),
                     environment ? resolve(environment->atlas_) : fallback_.Get(),
                     BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    const auto data = environment.value_or(SceneEnvironment{});
    const auto angle = data.rotation_ * std::numbers::pi_v<float> / 180;
    const std::array settings{environment ? data.energy_ : 0.0f, std::cos(angle), std::sin(angle),
                              0.0f};
    bgfx::setUniform(settings_.Get(), settings.data());
}
}  // namespace rhythm::render::detail
