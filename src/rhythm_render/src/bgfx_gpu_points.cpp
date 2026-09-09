#include "bgfx_gpu_points.h"

#include "gpu_point_shader.h"
namespace rhythm::render::detail {
bool BgfxGpuPoints::Supported() {
    const auto caps = bgfx::getCaps();
    constexpr auto required = BGFX_CAPS_COMPUTE | BGFX_CAPS_INSTANCING;
    return caps && (caps->supported & required) == required;
}
BgfxGpuPoints::BgfxGpuPoints(std::uint64_t device) : store_(device) {
    if (!Supported()) throw std::logic_error("render.gpu_points_unsupported");
    GpuHandle cs(bgfx::createShader(
            bgfx::copy(kGpuParticleComputeShader, sizeof(kGpuParticleComputeShader))));
    compute_ = GpuHandle(bgfx::createProgram(cs.Get(), false));
    GpuHandle vs(
            bgfx::createShader(bgfx::copy(kGpuPointVertexShader, sizeof(kGpuPointVertexShader))));
    GpuHandle fs(bgfx::createShader(
            bgfx::copy(kGpuPointFragmentShader, sizeof(kGpuPointFragmentShader))));
    render_ = GpuHandle(bgfx::createProgram(vs.Get(), fs.Get(), false));
    constexpr std::array<float, 8> vertices{-1, -1, 1, -1, 1, 1, -1, 1};
    constexpr std::array<std::uint16_t, 6> indices{0, 1, 2, 0, 2, 3};
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float).end();
    quad_ = GpuHandle(
            bgfx::createVertexBuffer(bgfx::copy(vertices.data(), sizeof(vertices)), layout));
    indices_ = GpuHandle(bgfx::createIndexBuffer(bgfx::copy(indices.data(), sizeof(indices))));
    constexpr std::array names{"u_gpu_step0",   "u_gpu_step1",   "u_gpu_emit",   "u_gpu_dynamics",
                               "u_gpu_gravity", "u_gpu_color_a", "u_gpu_color_b"};
    for (std::size_t i = 0; i < names.size(); ++i)
        uniforms_[i] = GpuHandle(bgfx::createUniform(names[i], bgfx::UniformType::Vec4));
    view_ = GpuHandle(bgfx::createUniform("u_gpu_view", bgfx::UniformType::Vec4));
    sampling_ = GpuHandle(bgfx::createUniform("u_gpu_sample", bgfx::UniformType::Vec4));
    sampler_ = GpuHandle(bgfx::createUniform("s_gpu_sample", bgfx::UniformType::Sampler));
    constexpr std::array<std::uint8_t, 4> kWhite{255, 255, 255, 255};
    white_ = GpuHandle(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
                                             BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                                             bgfx::copy(kWhite.data(), sizeof(kWhite))));
}
GpuPointHandle BgfxGpuPoints::Create(std::uint32_t capacity) {
    auto handle = store_.Allocate(capacity);
    try {
        if (buffers_.size() <= handle.slot_) buffers_.resize(handle.slot_ + 1);
        bgfx::VertexLayout layout;
        layout.begin()
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
                .end();
        buffers_[handle.slot_] = GpuHandle(
                bgfx::createDynamicVertexBuffer(capacity, layout, BGFX_BUFFER_COMPUTE_READ_WRITE));
        return handle;
    } catch (...) {
        store_.Release(handle);
        throw;
    }
}
void BgfxGpuPoints::Release(GpuPointHandle handle) noexcept {
    if (!store_.Owns(handle)) return;
    buffers_[handle.slot_] = {};
    store_.Release(handle);
}
void BgfxGpuPoints::Update(bgfx::ViewId view, GpuPointHandle handle, const GpuParticleStep& s) {
    store_.Validate(handle, s);
    const auto capacity = store_.Capacity(handle);
    const std::array<std::array<float, 4>, 7> values{
            {{s.seconds_, s.reset_ ? 1.0f : 0.0f, float(s.spawn_start_), float(s.spawn_count_)},
             {float(capacity), float(s.seed_), float(s.sequence_), s.phase_},
             {s.center_[0], s.center_[1], s.radius_, s.speed_},
             {s.drag_, s.lifetime_, s.size_, s.flow_},
             {s.gravity_[0], s.gravity_[1], s.frequency_, s.center_[2]},
             s.color_a_,
             s.color_b_}};
    // View IDs are reused across frames. A compute pass must not inherit the
    // framebuffer/clear state of an earlier graphics pass at the same index.
    bgfx::resetView(view);
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewName(view, "GPU particle update");
    for (std::size_t i = 0; i < values.size(); ++i)
        bgfx::setUniform(uniforms_[i].Get(), values[i].data());
    bgfx::setBuffer(0, buffers_[handle.slot_].Get(), bgfx::Access::ReadWrite);
    bgfx::dispatch(view, compute_.Get(), (capacity + 63) / 64);
    store_.Updated(handle);
}
void BgfxGpuPoints::Map(bgfx::ViewId view, GpuPointHandle source, GpuPointHandle destination,
                        const GpuPointMapping& mapping) {
    store_.ValidateMap(source, destination, mapping);
    // Lazily own mapping resources so existing particle-only works pay no cost.
    if (!bgfx::isValid(map_.Get())) {
        GpuHandle shader(
                bgfx::createShader(bgfx::copy(kGpuPointMapShader, sizeof(kGpuPointMapShader))));
        auto program = GpuHandle(bgfx::createProgram(shader.Get(), false));
        auto transform =
                GpuHandle(bgfx::createUniform("u_attribute_transform", bgfx::UniformType::Mat4));
        auto info = GpuHandle(bgfx::createUniform("u_attribute_info", bgfx::UniformType::Vec4));
        auto color = GpuHandle(bgfx::createUniform("u_attribute_color", bgfx::UniformType::Vec4));
        map_transform_ = std::move(transform);
        map_info_ = std::move(info);
        map_color_ = std::move(color);
        map_ = std::move(program);
    }
    const auto count = store_.Capacity(source);
    const std::array<float, 4> info{float(count), mapping.size_, 0, 0};
    bgfx::resetView(view);
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewName(view, "GPU point attribute mapping");
    bgfx::setUniform(map_transform_.Get(), mapping.transform_.data());
    bgfx::setUniform(map_info_.Get(), info.data());
    bgfx::setUniform(map_color_.Get(), mapping.color_.data());
    bgfx::setBuffer(0, buffers_[source.slot_].Get(), bgfx::Access::Read);
    bgfx::setBuffer(1, buffers_[destination.slot_].Get(), bgfx::Access::Write);
    bgfx::dispatch(view, map_.Get(), (count + 63) / 64);
    store_.Updated(destination);
}
void BgfxGpuPoints::Draw(bgfx::ViewId view, bgfx::FrameBufferHandle target, Extent extent,
                         bool invert, GpuPointHandle handle, const GpuPointStyle& style,
                         bool float_target, bgfx::TextureHandle sampling_texture) {
    store_.ValidateDraw(handle, style);
    bgfx::setViewName(view, "GPU point rendering");
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(view, target);
    bgfx::setViewRect(view, 0, 0, extent.width_, extent.height_);
    bgfx::setViewClear(view, BGFX_CLEAR_COLOR, 0);
    bgfx::setViewTransform(view, nullptr, nullptr);
    const std::array<float, 4> values{float(extent.height_) / extent.width_, invert ? -1.0f : 1.0f,
                                      style.opacity_, 0};
    bgfx::setUniform(view_.Get(), values.data());
    const std::array<float, 4> sampling{style.sampling_ ? style.sampling_->color_amount_ : 0,
                                        style.sampling_ ? style.sampling_->size_amount_ : 0, 0, 0};
    bgfx::setUniform(sampling_.Get(), sampling.data());
    bgfx::setTexture(0, sampler_.Get(),
                     bgfx::isValid(sampling_texture) ? sampling_texture : white_.Get(),
                     BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    bgfx::setVertexBuffer(0, quad_.Get());
    bgfx::setIndexBuffer(indices_.Get());
    bgfx::setInstanceDataBuffer(buffers_[handle.slot_].Get(), 0, store_.Capacity(handle));
    bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
            (style.additive_
                     ? (float_target
                                ? BGFX_STATE_BLEND_FUNC_SEPARATE(
                                          BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE,
                                          BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                                : BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE))
                     : BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                             BGFX_STATE_BLEND_INV_SRC_ALPHA)));
    bgfx::submit(view, render_.Get());
}
}  // namespace rhythm::render::detail
