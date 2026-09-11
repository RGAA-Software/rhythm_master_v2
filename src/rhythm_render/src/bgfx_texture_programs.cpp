#include "bgfx_texture_programs.h"

#include <bx/platform.h>

#include <array>
#include <cmath>

#include "color_adjust_shader.h"
#include "color_pipeline_shader.h"
#include "depth_shader.h"
#include "environment_shader.h"
#include "fs_ocornut_imgui.bin.h"
#include "texture_displace_shader.h"
#include "texture_filter_shader.h"
#include "texture_fxaa_shader.h"
#include "texture_glow_shader.h"
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
    GpuHandle fxaa_fragment(
            bgfx::createShader(bgfx::copy(kTextureFxaaShader, sizeof(kTextureFxaaShader))));
    fxaa_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), fxaa_fragment.Get(), false));
    fxaa_settings_ = GpuHandle(bgfx::createUniform("u_fxaa_settings", bgfx::UniformType::Vec4));
    fxaa_domain_ = GpuHandle(bgfx::createUniform("u_fxaa_domain", bgfx::UniformType::Vec4));
    GpuHandle environment_fragment(bgfx::createShader(
            bgfx::copy(kEnvironmentFilterShader, sizeof(kEnvironmentFilterShader))));
    environment_program_ =
            GpuHandle(bgfx::createProgram(vertex.Get(), environment_fragment.Get(), false));
    environment_settings_ =
            GpuHandle(bgfx::createUniform("u_environment_filter", bgfx::UniformType::Vec4));
    GpuHandle depth_fragment(
            bgfx::createShader(bgfx::copy(kDepthLinearShader, sizeof(kDepthLinearShader))));
    depth_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), depth_fragment.Get(), false));
    depth_settings_ = GpuHandle(bgfx::createUniform("u_depth_settings", bgfx::UniformType::Vec4));
    GpuHandle dof_fragment(
            bgfx::createShader(bgfx::copy(kDepthOfFieldShader, sizeof(kDepthOfFieldShader))));
    dof_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), dof_fragment.Get(), false));
    dof_settings_ = GpuHandle(bgfx::createUniform("u_dof_settings", bgfx::UniformType::Vec4));
    dof_domain_ = GpuHandle(bgfx::createUniform("u_dof_domain", bgfx::UniformType::Vec4));
    GpuHandle color_fragment(
            bgfx::createShader(bgfx::copy(kColorAdjustShader, sizeof(kColorAdjustShader))));
    color_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), color_fragment.Get(), false));
    GpuHandle pipeline_fragment(
            bgfx::createShader(bgfx::copy(kColorPipelineShader, sizeof(kColorPipelineShader))));
    pipeline_program_ =
            GpuHandle(bgfx::createProgram(vertex.Get(), pipeline_fragment.Get(), false));
    pipeline_settings_ =
            GpuHandle(bgfx::createUniform("u_color_pipeline", bgfx::UniformType::Vec4));
    color_limits_ = GpuHandle(bgfx::createUniform("u_color_limits", bgfx::UniformType::Vec4));
    color_uniform_ = GpuHandle(bgfx::createUniform("u_color_adjust", bgfx::UniformType::Vec4));
    GpuHandle filter_fragment(
            bgfx::createShader(bgfx::copy(kTextureFilterShader, sizeof(kTextureFilterShader))));
    filter_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), filter_fragment.Get(), false));
    filter_uniform_ = GpuHandle(bgfx::createUniform("u_texture_filter", bgfx::UniformType::Vec4));
    const std::array<std::span<const std::uint8_t>, 4> glow_fragments{
            kTextureGlowFilterShader, kTextureGlowDownsampleShader, kTextureGlowUpsampleShader,
            kTextureGlowCompositeShader};
    for (std::size_t index = 0; index < glow_fragments.size(); ++index) {
        GpuHandle fragment_shader(bgfx::createShader(
                bgfx::copy(glow_fragments[index].data(),
                           static_cast<std::uint32_t>(glow_fragments[index].size_bytes()))));
        glow_programs_[index] =
                GpuHandle(bgfx::createProgram(vertex.Get(), fragment_shader.Get(), false));
    }
    glow_settings_ = GpuHandle(bgfx::createUniform("u_glow_settings", bgfx::UniformType::Vec4));
    glow_domain_ = GpuHandle(bgfx::createUniform("u_glow_domain", bgfx::UniformType::Vec4));
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
    GpuHandle trail_fragment(
            bgfx::createShader(bgfx::copy(kTextureTrailShader, sizeof(kTextureTrailShader))));
    trail_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), trail_fragment.Get(), false));
    trail_settings_ = GpuHandle(bgfx::createUniform("u_trail_settings", bgfx::UniformType::Vec4));
    GpuHandle displace_fragment(
            bgfx::createShader(bgfx::copy(kTextureDisplaceShader, sizeof(kTextureDisplaceShader))));
    displace_program_ =
            GpuHandle(bgfx::createProgram(vertex.Get(), displace_fragment.Get(), false));
    displace_settings_ =
            GpuHandle(bgfx::createUniform("u_displace_settings", bgfx::UniformType::Vec4));
    displace_domain_ = GpuHandle(bgfx::createUniform("u_displace_domain", bgfx::UniformType::Vec4));
    map_sampler_ = GpuHandle(bgfx::createUniform("s_displace_map", bgfx::UniformType::Sampler));
    sampler_ = GpuHandle(bgfx::createUniform("s_tex", bgfx::UniformType::Sampler));
}
void BgfxTexturePrograms::Submit(std::uint16_t view, const DrawCommand& command, Extent source_size,
                                 Extent target_size, float aspect, bgfx::TextureHandle source,
                                 Extent map_size, bgfx::TextureHandle map,
                                 bool float_target) const {
    bgfx::setTexture(0, sampler_.Get(), source);
    if (command.texture_fxaa_) {
        const auto& fxaa = *command.texture_fxaa_;
        const std::array settings{fxaa.span_, fxaa.reduce_multiplier_, fxaa.reduce_minimum_,
                                  fxaa.strength_};
        const std::array domain{1.0f / source_size.width_, 1.0f / source_size.height_, 0.0f, 0.0f};
        bgfx::setTexture(0, sampler_.Get(), source, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::setUniform(fxaa_settings_.Get(), settings.data());
        bgfx::setUniform(fxaa_domain_.Get(), domain.data());
        bgfx::submit(view, fxaa_program_.Get());
    } else if (command.environment_filter_) {
        const std::array settings{command.environment_filter_->source_srgb_ ? 1.0f : 0.0f,
                                  0.5f / source_size.height_, 0.0f, 0.0f};
        bgfx::setTexture(0, sampler_.Get(), source, BGFX_SAMPLER_V_CLAMP);
        bgfx::setUniform(environment_settings_.Get(), settings.data());
        bgfx::submit(view, environment_program_.Get());
    } else if (command.depth_of_field_) {
        const auto& dof = *command.depth_of_field_;
        const auto& depth = dof.projection_;
        const std::array projection{depth.near_, depth.far_, depth.orthographic_ ? 1.0f : 0.0f,
                                    0.0f};
        const std::array settings{dof.focus_, dof.focus_scale_, dof.radius_, float(dof.samples_)};
        const std::array domain{1.0f / source_size.width_, 1.0f / source_size.height_,
                                std::max(0.5f, dof.radius_ * dof.radius_ / (2 * dof.samples_)),
                                0.0f};
        bgfx::setTexture(1, map_sampler_.Get(), map);
        bgfx::setUniform(depth_settings_.Get(), projection.data());
        bgfx::setUniform(dof_settings_.Get(), settings.data());
        bgfx::setUniform(dof_domain_.Get(), domain.data());
        bgfx::submit(view, dof_program_.Get());
    } else if (command.depth_linearization_) {
        const auto& depth = *command.depth_linearization_;
        const std::array settings{depth.near_, depth.far_, depth.orthographic_ ? 1.0f : 0.0f,
                                  depth.normalize_ ? 1.0f : 0.0f};
        bgfx::setUniform(depth_settings_.Get(), settings.data());
        bgfx::submit(view, depth_program_.Get());
    } else if (command.color_pipeline_) {
        const auto& c = *command.color_pipeline_;
        const std::array settings{c.input_ == ColorTransfer::kSrgb ? 1.0f : 0.0f,
                                  c.output_ == ColorTransfer::kSrgb ? 1.0f : 0.0f,
                                  c.tone_mapping_ == ToneMapping::kReinhard ? 1.0f : 0.0f,
                                  std::exp2(c.exposure_)};
        bgfx::setUniform(pipeline_settings_.Get(), settings.data());
        bgfx::submit(view, pipeline_program_.Get());
    } else if (command.texture_trail_) {
        const auto& trail = *command.texture_trail_;
        const std::array settings{trail.retention_, trail.scale_, trail.rotation_, aspect};
        bgfx::setTexture(1, map_sampler_.Get(), map);
        bgfx::setUniform(trail_settings_.Get(), settings.data());
        bgfx::submit(view, trail_program_.Get());
    } else if (command.texture_displace_) {
        const auto& displace = *command.texture_displace_;
        const std::array settings{displace.strength_, displace.rotation_,
                                  displace.kind_ == TextureDisplaceKind::kVectorRg ? 1.0f : 0.0f,
                                  0.0f};
        const std::array domain{displace.radius_ / map_size.width_,
                                displace.radius_ / map_size.height_, aspect, 0.0f};
        bgfx::setTexture(1, map_sampler_.Get(), map);
        bgfx::setUniform(displace_settings_.Get(), settings.data());
        bgfx::setUniform(displace_domain_.Get(), domain.data());
        bgfx::submit(view, displace_program_.Get());
    } else if (command.texture_mapping_) {
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
        const std::array domain{aspect, noise.offset_x_, noise.offset_y_, 0.0f};
        bgfx::setUniform(noise_settings_.Get(), settings.data());
        bgfx::setUniform(noise_color_a_.Get(), noise.color_a_.data());
        bgfx::setUniform(noise_color_b_.Get(), noise.color_b_.data());
        bgfx::setUniform(noise_domain_.Get(), domain.data());
        bgfx::submit(view, noise_program_.Get());
    } else if (command.texture_glow_) {
        const auto& glow = *command.texture_glow_;
        const std::array settings{
                glow.kind_ == TextureGlowKind::kComposite ? glow.strength_ : glow.threshold_,
                glow.threshold_scale_, glow.bloom_floor_, glow.luminance_cap_};
        const std::array domain{1.0f / target_size.width_, 1.0f / target_size.height_, 0.0f, 0.0f};
        bgfx::setTexture(0, sampler_.Get(), source, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::setUniform(glow_settings_.Get(), settings.data());
        bgfx::setUniform(glow_domain_.Get(), domain.data());
        bgfx::submit(view, glow_programs_[static_cast<std::size_t>(glow.kind_)].Get());
    } else if (command.texture_filter_) {
        const auto& filter = *command.texture_filter_;
        const std::array values{filter.step_x_ / source_size.width_,
                                filter.step_y_ / source_size.height_, 0.0f, 0.0f};
        bgfx::setTexture(0, sampler_.Get(), source, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::setUniform(filter_uniform_.Get(), values.data());
        bgfx::submit(view, filter_program_.Get());
    } else if (command.color_adjustment_) {
        const auto& adjustment = *command.color_adjustment_;
        const std::array values{std::exp2(adjustment.exposure_), adjustment.contrast_,
                                adjustment.saturation_, adjustment.invert_};
        const std::array limits{float_target ? 65504.0f : 1.0f, 0.0f, 0.0f, 0.0f};
        bgfx::setUniform(color_limits_.Get(), limits.data());
        bgfx::setUniform(color_uniform_.Get(), values.data());
        bgfx::submit(view, color_program_.Get());
    } else {
        bgfx::submit(view, program_.Get());
    }
}
}  // namespace rhythm::render::detail
