#include "bgfx_texture_programs.h"

#include <array>
#include <cmath>

#include "color_adjust_shader.h"
#include "fs_ocornut_imgui.bin.h"
#include "texture_filter_shader.h"
#include "texture_mapping_shader.h"
#include "texture_noise_shader.h"
#include "vs_ocornut_imgui.bin.h"

namespace rhythm::render::detail {
BgfxTexturePrograms::BgfxTexturePrograms() {
#if BX_PLATFORM_ANDROID
    GpuHandle vertex(
            bgfx::createShader(bgfx::copy(vs_ocornut_imgui_essl, sizeof(vs_ocornut_imgui_essl))));
    GpuHandle fragment(
            bgfx::createShader(bgfx::copy(fs_ocornut_imgui_essl, sizeof(fs_ocornut_imgui_essl))));
#else
    GpuHandle vertex(
            bgfx::createShader(bgfx::copy(vs_ocornut_imgui_dxbc, sizeof(vs_ocornut_imgui_dxbc))));
    GpuHandle fragment(
            bgfx::createShader(bgfx::copy(fs_ocornut_imgui_dxbc, sizeof(fs_ocornut_imgui_dxbc))));
#endif
    program_ = GpuHandle(bgfx::createProgram(vertex.Get(), fragment.Get(), false));
    GpuHandle color_fragment(
            bgfx::createShader(bgfx::copy(kColorAdjustShader, sizeof(kColorAdjustShader))));
    color_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), color_fragment.Get(), false));
    color_uniform_ = GpuHandle(bgfx::createUniform("u_color_adjust", bgfx::UniformType::Vec4));
    GpuHandle filter_fragment(
            bgfx::createShader(bgfx::copy(kTextureFilterShader, sizeof(kTextureFilterShader))));
    filter_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), filter_fragment.Get(), false));
    filter_uniform_ = GpuHandle(bgfx::createUniform("u_texture_filter", bgfx::UniformType::Vec4));
    GpuHandle noise_fragment(
            bgfx::createShader(bgfx::copy(kTextureNoiseShader, sizeof(kTextureNoiseShader))));
    noise_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), noise_fragment.Get(), false));
    noise_settings_ = GpuHandle(bgfx::createUniform("u_noise_settings", bgfx::UniformType::Vec4));
    noise_color_a_ = GpuHandle(bgfx::createUniform("u_noise_color_a", bgfx::UniformType::Vec4));
    noise_color_b_ = GpuHandle(bgfx::createUniform("u_noise_color_b", bgfx::UniformType::Vec4));
    noise_domain_ = GpuHandle(bgfx::createUniform("u_noise_domain", bgfx::UniformType::Vec4));
    GpuHandle mapping_fragment(
            bgfx::createShader(bgfx::copy(kTextureMappingShader, sizeof(kTextureMappingShader))));
    mapping_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), mapping_fragment.Get(), false));
    mapping_settings_ =
            GpuHandle(bgfx::createUniform("u_mapping_settings", bgfx::UniformType::Vec4));
    mapping_domain_ = GpuHandle(bgfx::createUniform("u_mapping_domain", bgfx::UniformType::Vec4));
    GpuHandle contours_fragment(
            bgfx::createShader(bgfx::copy(kTextureContoursShader, sizeof(kTextureContoursShader))));
    contours_program_ =
            GpuHandle(bgfx::createProgram(vertex.Get(), contours_fragment.Get(), false));
    contours_settings_ =
            GpuHandle(bgfx::createUniform("u_contours_settings", bgfx::UniformType::Vec4));
    contours_color_a_ =
            GpuHandle(bgfx::createUniform("u_contours_color_a", bgfx::UniformType::Vec4));
    contours_color_b_ =
            GpuHandle(bgfx::createUniform("u_contours_color_b", bgfx::UniformType::Vec4));
    sampler_ = GpuHandle(bgfx::createUniform("s_tex", bgfx::UniformType::Sampler));
}
void BgfxTexturePrograms::Submit(std::uint16_t view, const DrawCommand& command, Extent source_size,
                                 float aspect, bgfx::TextureHandle source) const {
    bgfx::setTexture(0, sampler_.Get(), source);
    if (command.texture_mapping_) {
        const auto& mapping = *command.texture_mapping_;
        const std::array settings{mapping.scale_, mapping.rotation_, mapping.travel_,
                                  mapping.twist_};
        const std::array domain{aspect, mapping.sectors_, mapping.radial_power_,
                                mapping.kind_ == TextureMappingKind::kPolar ? 1.0f : 0.0f};
        bgfx::setUniform(mapping_settings_.Get(), settings.data());
        bgfx::setUniform(mapping_domain_.Get(), domain.data());
        bgfx::submit(view, mapping_program_.Get());
    } else if (command.texture_contours_) {
        const auto& contours = *command.texture_contours_;
        const std::array settings{contours.count_, contours.width_, contours.phase_, 0.0f};
        bgfx::setUniform(contours_settings_.Get(), settings.data());
        bgfx::setUniform(contours_color_a_.Get(), contours.color_a_.data());
        bgfx::setUniform(contours_color_b_.Get(), contours.color_b_.data());
        bgfx::submit(view, contours_program_.Get());
    } else if (command.texture_noise_) {
        const auto& noise = *command.texture_noise_;
        const std::array settings{noise.scale_, noise.phase_, noise.contrast_, noise.seed_};
        const std::array domain{aspect, 0.0f, 0.0f, 0.0f};
        bgfx::setUniform(noise_settings_.Get(), settings.data());
        bgfx::setUniform(noise_color_a_.Get(), noise.color_a_.data());
        bgfx::setUniform(noise_color_b_.Get(), noise.color_b_.data());
        bgfx::setUniform(noise_domain_.Get(), domain.data());
        bgfx::submit(view, noise_program_.Get());
    } else if (command.texture_filter_) {
        const auto& filter = *command.texture_filter_;
        const std::array values{filter.step_x_ / source_size.width_,
                                filter.step_y_ / source_size.height_,
                                filter.kind_ == TextureFilterKind::kDownsample ? 1.0f : 0.0f, 0.0f};
        bgfx::setUniform(filter_uniform_.Get(), values.data());
        bgfx::submit(view, filter_program_.Get());
    } else if (command.color_adjustment_) {
        const auto& adjustment = *command.color_adjustment_;
        const std::array values{std::exp2(adjustment.exposure_), adjustment.contrast_,
                                adjustment.saturation_, adjustment.invert_};
        bgfx::setUniform(color_uniform_.Get(), values.data());
        bgfx::submit(view, color_program_.Get());
    } else {
        bgfx::submit(view, program_.Get());
    }
}
}  // namespace rhythm::render::detail
