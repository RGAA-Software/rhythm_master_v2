#include "bgfx_depth_of_field.h"

#include <bx/math.h>

#include <algorithm>
#include <array>
#include <stdexcept>

#include "depth_shader.h"
#include "vs_ocornut_imgui.bin.h"

namespace rhythm::render::detail {
namespace {
constexpr auto kSamplerFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
bool IsHalfSize(const DepthOfField& dof) {
    return dof.shape_ == DepthOfFieldShape::kCircle || dof.quality_ <= DepthOfFieldQuality::kLow;
}
Extent Half(Extent extent) {
    return {static_cast<std::uint16_t>(std::max(1, extent.width_ / 2)),
            static_cast<std::uint16_t>(std::max(1, extent.height_ / 2))};
}
GpuHandle<bgfx::TextureHandle> CreateTarget(Extent extent, bgfx::TextureFormat::Enum format) {
    return GpuHandle(bgfx::createTexture2D(extent.width_, extent.height_, false, 1, format,
                                           BGFX_TEXTURE_RT | kSamplerFlags));
}
GpuHandle<bgfx::FrameBufferHandle> CreateFramebuffer(
        std::initializer_list<bgfx::TextureHandle> attachments) {
    return GpuHandle(bgfx::createFrameBuffer(static_cast<std::uint8_t>(attachments.size()),
                                             attachments.begin(), false));
}
}  // namespace

struct BgfxDepthOfField::Buffers {
    explicit Buffers(Extent requested)
        : extent_(requested),
          original_weight_(CreateTarget(extent_, bgfx::TextureFormat::R16F)),
          full_color_(CreateTarget(extent_, bgfx::TextureFormat::RGBA16F)),
          full_weight_(CreateTarget(extent_, bgfx::TextureFormat::R16F)),
          half_color_a_(CreateTarget(Half(extent_), bgfx::TextureFormat::RGBA16F)),
          half_weight_a_(CreateTarget(Half(extent_), bgfx::TextureFormat::R16F)),
          half_color_b_(CreateTarget(Half(extent_), bgfx::TextureFormat::RGBA16F)),
          half_weight_b_(CreateTarget(Half(extent_), bgfx::TextureFormat::R16F)),
          weight_framebuffer_(CreateFramebuffer({original_weight_.Get()})),
          full_framebuffer_(CreateFramebuffer({full_color_.Get(), full_weight_.Get()})),
          half_framebuffer_a_(CreateFramebuffer({half_color_a_.Get(), half_weight_a_.Get()})),
          half_framebuffer_b_(CreateFramebuffer({half_color_b_.Get(), half_weight_b_.Get()})) {}

    Extent extent_{};
    GpuHandle<bgfx::TextureHandle> original_weight_{};
    GpuHandle<bgfx::TextureHandle> full_color_{};
    GpuHandle<bgfx::TextureHandle> full_weight_{};
    GpuHandle<bgfx::TextureHandle> half_color_a_{};
    GpuHandle<bgfx::TextureHandle> half_weight_a_{};
    GpuHandle<bgfx::TextureHandle> half_color_b_{};
    GpuHandle<bgfx::TextureHandle> half_weight_b_{};
    GpuHandle<bgfx::FrameBufferHandle> weight_framebuffer_{};
    GpuHandle<bgfx::FrameBufferHandle> full_framebuffer_{};
    GpuHandle<bgfx::FrameBufferHandle> half_framebuffer_a_{};
    GpuHandle<bgfx::FrameBufferHandle> half_framebuffer_b_{};
};

BgfxDepthOfField::~BgfxDepthOfField() = default;

BgfxDepthOfField::BgfxDepthOfField() {
#if BX_PLATFORM_ANDROID
    GpuHandle vertex(
            bgfx::createShader(bgfx::copy(vs_ocornut_imgui_essl, sizeof(vs_ocornut_imgui_essl))));
#else
    GpuHandle vertex(
            bgfx::createShader(bgfx::copy(vs_ocornut_imgui_dxbc, sizeof(vs_ocornut_imgui_dxbc))));
#endif
    const auto create_program = [&](const auto& bytes) {
        GpuHandle fragment(bgfx::createShader(
                bgfx::copy(bytes.data(), static_cast<std::uint32_t>(bytes.size()))));
        return GpuHandle(bgfx::createProgram(vertex.Get(), fragment.Get(), false));
    };
    weight_program_ = create_program(std::span(kDepthOfFieldWeightShader));
    filter_program_ = create_program(std::span(kDepthOfFieldFilterShader));
    final_filter_program_ = create_program(std::span(kDepthOfFieldFinalFilterShader));
    composite_program_ = create_program(std::span(kDepthOfFieldCompositeShader));
    sampler_ = GpuHandle(bgfx::createUniform("s_tex", bgfx::UniformType::Sampler));
    weight_sampler_ = GpuHandle(bgfx::createUniform("s_displace_map", bgfx::UniformType::Sampler));
    original_weight_sampler_ =
            GpuHandle(bgfx::createUniform("s_dof_original_weight", bgfx::UniformType::Sampler));
    original_color_sampler_ =
            GpuHandle(bgfx::createUniform("s_dof_original_color", bgfx::UniformType::Sampler));
    depth_settings_ = GpuHandle(bgfx::createUniform("u_depth_settings", bgfx::UniformType::Vec4));
    dof_settings_ = GpuHandle(bgfx::createUniform("u_dof_settings", bgfx::UniformType::Vec4));
    filter_settings_ = GpuHandle(bgfx::createUniform("u_dof_filter", bgfx::UniformType::Vec4));
    domain_ = GpuHandle(bgfx::createUniform("u_dof_domain", bgfx::UniformType::Vec4));
}

std::uint32_t BgfxDepthOfField::PreparationPasses(const DepthOfField& dof) const {
    return IsHalfSize(dof) && dof.shape_ != DepthOfFieldShape::kCircle ? 3 : 2;
}

std::uint64_t BgfxDepthOfField::AdditionalTextureBytes(Extent extent) const {
    for (const auto& buffers : buffers_)
        if (buffers->extent_ == extent) return 0;
    const auto pixels = std::uint64_t{extent.width_} * extent.height_;
    const auto half = Half(extent);
    return pixels * 12 + std::uint64_t{half.width_} * half.height_ * 20;
}

BgfxDepthOfField::Buffers& BgfxDepthOfField::GetBuffers(Extent extent) {
    for (const auto& buffers : buffers_)
        if (buffers->extent_ == extent) return *buffers;
    buffers_.push_back(std::make_unique<Buffers>(extent));
    return *buffers_.back();
}

void BgfxDepthOfField::SetFilterUniforms(const DepthOfField& dof, Extent sampling_extent,
                                         bool second_pass) const {
    static constexpr std::array<float, 4> kCircleScale{8, 4, 1, 0.5f};
    static constexpr std::array<float, 4> kShapeSteps{6, 12, 12, 24};
    const auto quality = static_cast<std::size_t>(dof.quality_);
    const float parameter =
            dof.shape_ == DepthOfFieldShape::kCircle ? kCircleScale[quality] : kShapeSteps[quality];
    const std::array filter{
            static_cast<float>(dof.shape_), second_pass ? 1.0f : 0.0f,
            dof.radius_ *
                    (IsHalfSize(dof) && dof.shape_ != DepthOfFieldShape::kCircle ? 0.5f : 1.0f),
            parameter};
    const std::array domain{1.0f / sampling_extent.width_, 1.0f / sampling_extent.height_, 0.0f,
                            0.0f};
    bgfx::setUniform(filter_settings_.Get(), filter.data());
    bgfx::setUniform(domain_.Get(), domain.data());
}

void BgfxDepthOfField::DrawPass(bgfx::ViewId view, const char* name,
                                bgfx::FrameBufferHandle framebuffer, Extent extent,
                                float logical_width, float logical_height, bool invert,
                                bool homogeneous_depth, const DrawCommand& command,
                                const bgfx::TransientVertexBuffer& vertices,
                                const bgfx::TransientIndexBuffer& indices,
                                bgfx::ProgramHandle program) const {
    bgfx::setViewName(view, name);
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(view, framebuffer);
    bgfx::setViewRect(view, 0, 0, extent.width_, extent.height_);
    bgfx::setViewClear(view, BGFX_CLEAR_COLOR, 0);
    float projection[16]{};
    bx::mtxOrtho(projection, 0, logical_width, invert ? 0 : logical_height,
                 invert ? logical_height : 0, 0, 100, 0, homogeneous_depth);
    bgfx::setViewTransform(view, nullptr, projection);
    const auto scale_x = extent.width_ / logical_width;
    const auto scale_y = extent.height_ / logical_height;
    const auto left = std::clamp(command.clip_.x_ * scale_x, 0.0f, float(extent.width_));
    const auto top = std::clamp(command.clip_.y_ * scale_y, 0.0f, float(extent.height_));
    const auto right = std::clamp((command.clip_.x_ + command.clip_.width_) * scale_x, left,
                                  float(extent.width_));
    const auto bottom = std::clamp((command.clip_.y_ + command.clip_.height_) * scale_y, top,
                                   float(extent.height_));
    bgfx::setScissor(static_cast<std::uint16_t>(left),
                     static_cast<std::uint16_t>(invert ? extent.height_ - bottom : top),
                     static_cast<std::uint16_t>(right - left),
                     static_cast<std::uint16_t>(bottom - top));
    bgfx::setVertexBuffer(0, &vertices);
    bgfx::setIndexBuffer(&indices, command.first_index_, command.index_count_);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view, program);
}

void BgfxDepthOfField::Prepare(bgfx::ViewId first_view, const DrawCommand& command, Extent extent,
                               float logical_width, float logical_height, bool invert,
                               bool homogeneous_depth, const bgfx::TransientVertexBuffer& vertices,
                               const bgfx::TransientIndexBuffer& indices,
                               bgfx::TextureHandle source, bgfx::TextureHandle depth) {
    if (!command.depth_of_field_) throw std::invalid_argument("render.depth_of_field_missing");
    const auto& dof = *command.depth_of_field_;
    auto& buffers = GetBuffers(extent);
    const auto half = Half(extent);
    const std::array projection{dof.projection_.near_, dof.projection_.far_,
                                dof.projection_.orthographic_ ? 1.0f : 0.0f, 0.0f};
    const std::array settings{dof.focus_, dof.focus_scale_, dof.radius_, 0.0f};
    bgfx::setTexture(1, weight_sampler_.Get(), depth, kSamplerFlags);
    bgfx::setUniform(depth_settings_.Get(), projection.data());
    bgfx::setUniform(dof_settings_.Get(), settings.data());
    DrawPass(first_view, "DOF signed weight", buffers.weight_framebuffer_.Get(), extent,
             logical_width, logical_height, invert, homogeneous_depth, command, vertices, indices,
             weight_program_.Get());

    const bool half_size = IsHalfSize(dof);
    const auto working = half_size ? half : extent;
    bgfx::setTexture(0, sampler_.Get(), source, kSamplerFlags);
    bgfx::setTexture(1, weight_sampler_.Get(), buffers.original_weight_.Get(), kSamplerFlags);
    SetFilterUniforms(dof, dof.shape_ == DepthOfFieldShape::kCircle ? extent : working, false);
    DrawPass(static_cast<bgfx::ViewId>(first_view + 1), "DOF bokeh pass 1",
             half_size ? buffers.half_framebuffer_a_.Get() : buffers.full_framebuffer_.Get(),
             working, logical_width, logical_height, invert, homogeneous_depth, command, vertices,
             indices, filter_program_.Get());

    if (half_size && dof.shape_ != DepthOfFieldShape::kCircle) {
        bgfx::setTexture(0, sampler_.Get(), buffers.half_color_a_.Get(), kSamplerFlags);
        bgfx::setTexture(1, weight_sampler_.Get(), buffers.half_weight_a_.Get(), kSamplerFlags);
        SetFilterUniforms(dof, working, true);
        DrawPass(static_cast<bgfx::ViewId>(first_view + 2), "DOF bokeh pass 2",
                 buffers.half_framebuffer_b_.Get(), working, logical_width, logical_height, invert,
                 homogeneous_depth, command, vertices, indices, filter_program_.Get());
    }
}

void BgfxDepthOfField::SubmitFinal(bgfx::ViewId view, const DrawCommand& command, Extent extent,
                                   bgfx::TextureHandle source) {
    const auto& dof = *command.depth_of_field_;
    auto& buffers = GetBuffers(extent);
    if (IsHalfSize(dof)) {
        const bool circle = dof.shape_ == DepthOfFieldShape::kCircle;
        bgfx::setTexture(0, sampler_.Get(),
                         circle ? buffers.half_color_a_.Get() : buffers.half_color_b_.Get(),
                         kSamplerFlags);
        bgfx::setTexture(1, weight_sampler_.Get(),
                         circle ? buffers.half_weight_a_.Get() : buffers.half_weight_b_.Get(),
                         kSamplerFlags);
        bgfx::setTexture(2, original_weight_sampler_.Get(), buffers.original_weight_.Get(),
                         kSamplerFlags);
        bgfx::setTexture(3, original_color_sampler_.Get(), source, kSamplerFlags);
        bgfx::submit(view, composite_program_.Get());
        return;
    }
    bgfx::setTexture(0, sampler_.Get(), buffers.full_color_.Get(), kSamplerFlags);
    bgfx::setTexture(1, weight_sampler_.Get(), buffers.full_weight_.Get(), kSamplerFlags);
    SetFilterUniforms(dof, extent, true);
    bgfx::submit(view, final_filter_program_.Get());
}

void BgfxDepthOfField::AddStats(FrameStats& stats) const {
    for (const auto& buffers : buffers_) {
        const auto pixels = std::uint64_t{buffers->extent_.width_} * buffers->extent_.height_;
        const auto half = Half(buffers->extent_);
        stats.live_textures_ += 7;
        stats.texture_bytes_ += pixels * 12 + std::uint64_t{half.width_} * half.height_ * 20;
    }
}
}  // namespace rhythm::render::detail
