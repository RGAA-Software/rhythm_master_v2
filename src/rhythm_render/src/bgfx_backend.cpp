#include "bgfx_backend.h"

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <optional>
#include <stdexcept>

#include "bgfx_gpu_points.h"
#include "bgfx_handles.h"
#include "bgfx_image_programs.h"
#include "bgfx_readbacks.h"
#include "bgfx_scene.h"
#include "bgfx_texture_programs.h"
#include "resource_table.h"
#include "rhythm/render/budget.h"

namespace rhythm::render::detail {
namespace {
std::atomic_flag device_in_use = ATOMIC_FLAG_INIT;
std::uint32_t PremultiplyAbgr(std::uint32_t color) {
    const auto alpha = color >> 24;
    const auto channel = [&](std::uint32_t shift) {
        return (((color >> shift) & 255) * alpha + 127) / 255;
    };
    return channel(0) | channel(8) << 8 | channel(16) << 16 | alpha << 24;
}
class DeviceLease final {
   public:
    DeviceLease() {
        if (device_in_use.test_and_set()) throw std::logic_error("render.device_exists");
    }
    ~DeviceLease() { device_in_use.clear(); }
    DeviceLease(const DeviceLease&) = delete;
    DeviceLease& operator=(const DeviceLease&) = delete;
};
class DeviceLifetime final {
   public:
    DeviceLifetime(std::uintptr_t window, Extent size, std::shared_ptr<void> owner,
                   std::uintptr_t context)
        : surface_owner_(std::move(owner)) {
        bgfx::Init init;
#if BX_PLATFORM_ANDROID
        init.type = bgfx::RendererType::OpenGLES;
#else
        init.type = bgfx::RendererType::Direct3D11;
#endif
        // Borrowed native window remains valid through surface_owner_; never propagated to
        // consumers.
        init.platformData.nwh = reinterpret_cast<void*>(window);
        init.platformData.context = reinterpret_cast<void*>(context);
        init.resolution.width = size.width_;
        init.resolution.height = size.height_;
        init.resolution.reset = BGFX_RESET_VSYNC;
        init.limits.maxTransientVbSize = 32 * 1024 * 1024;
        init.limits.maxTransientIbSize = 12 * 1024 * 1024;
        if (!bgfx::init(init)) throw std::runtime_error("render.bgfx_init");
    }
    ~DeviceLifetime() { bgfx::shutdown(); }
    DeviceLifetime(const DeviceLifetime&) = delete;
    DeviceLifetime& operator=(const DeviceLifetime&) = delete;

   private:
    DeviceLease lease_{};
    std::shared_ptr<void> surface_owner_{};
};

class BgfxBackend final : public Backend {
   public:
    BgfxBackend(std::uintptr_t window, Extent size, std::shared_ptr<void> owner,
                std::uintptr_t context)
        : size_(size) {
        lifetime_.emplace(window, size, std::move(owner), context);
        // Texture UVs address the logical top-left on every backend. Inverting
        // GLES render targets makes row zero match uploaded image row zero.
        const auto caps = bgfx::getCaps();
        if (!caps) throw std::runtime_error("render.no_capabilities");
        invert_targets_ = caps->originBottomLeft;
        homogeneous_depth_ = caps->homogeneousDepth;
        scenes_supported_ = (caps->supported & BGFX_CAPS_INDEX32) != 0 &&
                            bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8,
                                                 BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY);
        layout_.begin()
                .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
                .end();
        texture_programs_.emplace();
    }
    TextureHandle Create(Extent extent, std::span<const std::uint8_t> rgba,
                         TexturePrecision precision) override {
        const auto format = precision == TexturePrecision::kFloat16 ? bgfx::TextureFormat::RGBA16F
                                                                    : bgfx::TextureFormat::RGBA8;
        if (!bgfx::isTextureValid(0, false, 1, format, rgba.empty() ? BGFX_TEXTURE_RT : 0))
            throw std::invalid_argument("render.unsupported_texture_precision");
        const auto handle = resources_.Allocate(extent, rgba, precision);
        try {
            if (textures_.size() <= handle.slot_) textures_.resize(handle.slot_ + 1);
            auto& entry = textures_[handle.slot_];
            // Public pixels/colors are straight RGBA. Store all GPU images in
            // premultiplied form so translucent targets survive further passes.
            std::vector<std::uint8_t> pixels(rgba.begin(), rgba.end());
            for (std::size_t index = 0; index < pixels.size(); index += 4)
                for (std::size_t channel = 0; channel < 3; ++channel)
                    pixels[index + channel] = static_cast<std::uint8_t>(
                            (pixels[index + channel] * pixels[index + 3] + 127) / 255);
            // Initial-data textures become immutable in bgfx's D3D11 backend.
            // Allocate without data, then copy the initial pixels so subsequent
            // host uploads preserve this texture's handle on D3D11 and GLES.
            entry.texture_ =
                    GpuHandle(bgfx::createTexture2D(extent.width_, extent.height_, false, 1, format,
                                                    BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
                                                            (rgba.empty() ? BGFX_TEXTURE_RT : 0),
                                                    nullptr));
            if (!rgba.empty())
                bgfx::updateTexture2D(
                        entry.texture_.Get(), 0, 0, 0, 0, extent.width_, extent.height_,
                        bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
            if (rgba.empty()) {
                const auto native = entry.texture_.Get();
                entry.framebuffer_ = GpuHandle(bgfx::createFrameBuffer(1, &native, false));
            }
            return handle;
        } catch (...) {
            if (handle.slot_ < textures_.size()) {
                textures_[handle.slot_].framebuffer_ = {};
                textures_[handle.slot_].texture_ = {};
            }
            resources_.Release(handle);
            throw;
        }
    }
    void Update(TextureHandle handle, std::span<const std::uint8_t> rgba) override {
        resources_.ValidateUpload(handle, rgba);
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        const auto extent = resources_.Size(handle);
        std::vector<std::uint8_t> pixels(rgba.begin(), rgba.end());
        for (std::size_t index = 0; index < pixels.size(); index += 4)
            for (std::size_t channel = 0; channel < 3; ++channel)
                pixels[index + channel] = static_cast<std::uint8_t>(
                        (pixels[index + channel] * pixels[index + 3] + 127) / 255);
        // bgfx copies the upload before returning; no application pixels remain borrowed.
        bgfx::updateTexture2D(textures_.at(handle.slot_).texture_.Get(), 0, 0, 0, 0, extent.width_,
                              extent.height_,
                              bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
    }
    bool SupportsSampleableDepth() const override {
        resources_.CheckReady();
        const auto& caps = *bgfx::getCaps();
        return (caps.formats[bgfx::TextureFormat::D24S8] & BGFX_CAPS_FORMAT_TEXTURE_2D) != 0 &&
               bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT);
    }
    TextureHandle CreateDepth(Extent extent) override {
        if (!SupportsSampleableDepth())
            throw std::logic_error("render.sampleable_depth_unsupported");
        const auto handle = resources_.AllocateDepth(extent);
        try {
            if (textures_.size() <= handle.slot_) textures_.resize(handle.slot_ + 1);
            textures_[handle.slot_].texture_ = GpuHandle(bgfx::createTexture2D(
                    extent.width_, extent.height_, false, 1, bgfx::TextureFormat::D24S8,
                    BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
                            BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT));
            return handle;
        } catch (...) {
            resources_.Release(handle);
            throw;
        }
    }
    void SubmitSceneDepth(TextureHandle color, TextureHandle depth, const SceneDrawList& list,
                          std::uint32_t clear) override {
        resources_.ValidateSceneDepth(color, depth);
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        if (!scene_) scene_ = std::make_unique<BgfxScene>(resources_.DeviceId());
        scene_->Validate(list);
        resources_.ValidateSceneMaterials(color, list, depth);
        resources_.RecordSceneSamples(list);
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        const auto extent = resources_.Size(color);
        const auto framebuffer =
                scene_->Target(color, textures_[color.slot_].texture_.Get(), extent, depth,
                               textures_[depth.slot_].texture_.Get());
        resources_.DropDepth(color);
        draws_ += scene_->Draw({static_cast<bgfx::ViewId>(passes_++), framebuffer, extent,
                                invert_targets_, homogeneous_depth_},
                               list, clear, [this](TextureHandle handle) {
                                   return textures_.at(handle.slot_).texture_.Get();
                               });
    }
    void Release(TextureHandle handle) noexcept override {
        if (!resources_.Owns(handle)) return;
        if (scene_) scene_->ReleaseTarget(handle);
        textures_[handle.slot_].framebuffer_ = {};
        textures_[handle.slot_].texture_ = {};
        resources_.Release(handle);
    }
    bool IsValid(TextureHandle handle) const override { return resources_.IsValid(handle); }
    TexturePrecision Precision(TextureHandle handle) const override {
        return resources_.Precision(handle);
    }
    bool SupportsReadback() const override {
        resources_.CheckReady();
        return BgfxReadbacks::Supported();
    }
    std::uint64_t RequestReadback(TextureHandle handle) override {
        resources_.CheckReady();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        if (!SupportsReadback()) throw std::logic_error("render.readback_unsupported");
        if (resources_.Precision(handle) != TexturePrecision::kUnorm8)
            throw std::invalid_argument("render.readback_precision");
        if (!resources_.IsRenderTarget(handle))
            throw std::invalid_argument("render.readback_target");
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        const auto ticket = readbacks_.Request(textures_.at(handle.slot_).texture_.Get(),
                                               resources_.Size(handle),
                                               static_cast<bgfx::ViewId>(passes_), resources_);
        ++passes_;
        return ticket;
    }
    std::optional<ReadbackImage> PollReadback(std::uint64_t ticket) override {
        resources_.CheckReady();
        return readbacks_.Poll(ticket, resources_);
    }
    void CancelReadback(std::uint64_t ticket) noexcept override {
        readbacks_.Cancel(ticket, resources_);
    }
    bool SupportsGpuPoints() const override {
        resources_.CheckReady();
        return BgfxGpuPoints::Supported();
    }
    GpuPointHandle CreateGpuPoints(std::uint32_t capacity) override {
        resources_.CheckReady();
        if (!gpu_points_) gpu_points_ = std::make_unique<BgfxGpuPoints>(resources_.DeviceId());
        return gpu_points_->Create(capacity);
    }
    ImageProgramTarget ImageTarget() const override {
#if BX_PLATFORM_ANDROID
        return ImageProgramTarget::kGles300;
#else
        return ImageProgramTarget::kWindowsSm5;
#endif
    }
    ImageProgramHandle CreateImageProgram(std::span<const std::uint8_t> artifact) override {
        resources_.CheckReady();
        if (!image_programs_)
            image_programs_ = std::make_unique<BgfxImagePrograms>(resources_.DeviceId());
        return image_programs_->Create(artifact);
    }
    void ReleaseImageProgram(ImageProgramHandle handle) noexcept override {
        resources_.CheckThread();
        if (image_programs_) image_programs_->Release(handle);
    }
    bool IsValid(ImageProgramHandle handle) const override {
        resources_.CheckThread();
        return image_programs_ && image_programs_->IsValid(handle);
    }
    void ReleaseGpuPoints(GpuPointHandle handle) noexcept override {
        resources_.CheckThread();
        if (gpu_points_) gpu_points_->Release(handle);
    }
    bool IsValid(GpuPointHandle handle) const override {
        resources_.CheckThread();
        return gpu_points_ && gpu_points_->IsValid(handle);
    }
    void UpdateGpuParticles(GpuPointHandle handle, const GpuParticleStep& step) override {
        resources_.CheckReady();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        if (!gpu_points_) throw std::invalid_argument("render.invalid_gpu_points");
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        gpu_points_->Update(static_cast<bgfx::ViewId>(passes_), handle, step);
        ++passes_;
    }
    void SubmitGpuPoints(TextureHandle target, GpuPointHandle handle,
                         const GpuPointStyle& style) override {
        resources_.CheckReady();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        if (!resources_.IsRenderTarget(target))
            throw std::invalid_argument("render.gpu_point_target");
        if (!gpu_points_) throw std::invalid_argument("render.invalid_gpu_points");
        gpu_points_->ValidateDraw(handle, style);
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        gpu_points_->Draw(static_cast<bgfx::ViewId>(passes_),
                          textures_.at(target.slot_).framebuffer_.Get(), resources_.Size(target),
                          invert_targets_, handle, style,
                          resources_.Precision(target) == TexturePrecision::kFloat16);
        ++passes_;
        ++draws_;
    }
    bool SupportsScenes() const override {
        resources_.CheckReady();
        return scenes_supported_;
    }
    MeshHandle CreateMesh(std::span<const MeshVertex> vertices,
                          std::span<const std::uint32_t> indices, std::span<const SkinWeights> skin,
                          std::span<const MorphTarget> morphs) override {
        resources_.CheckReady();
        if (!scene_) scene_ = std::make_unique<BgfxScene>(resources_.DeviceId());
        return scene_->Create(vertices, indices, skin, morphs);
    }
    void ReleaseMesh(MeshHandle mesh) noexcept override {
        resources_.CheckThread();
        if (scene_) scene_->Release(mesh);
    }
    bool IsValid(MeshHandle mesh) const override {
        resources_.CheckThread();
        return scene_ && scene_->IsValid(mesh);
    }
    void SubmitScene(TextureHandle target, const SceneDrawList& list,
                     std::uint32_t clear) override {
        resources_.CheckReady();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        if (!resources_.IsRenderTarget(target)) throw std::invalid_argument("render.scene_target");
        if (!scene_) scene_ = std::make_unique<BgfxScene>(resources_.DeviceId());
        scene_->Validate(list);
        resources_.ValidateSceneMaterials(target, list);
        resources_.RecordSceneSamples(list);
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        const auto extent = resources_.Size(target);
        const bool allocated = resources_.ReserveDepth(target);
        bgfx::FrameBufferHandle framebuffer = BGFX_INVALID_HANDLE;
        try {
            framebuffer = scene_->Target(target, textures_[target.slot_].texture_.Get(), extent);
        } catch (...) {
            if (allocated) resources_.DropDepth(target);
            throw;
        }
        const auto view = static_cast<bgfx::ViewId>(passes_++);
        draws_ += scene_->Draw(
                {view, framebuffer, extent, invert_targets_, homogeneous_depth_}, list, clear,
                [this](TextureHandle handle) { return textures_.at(handle.slot_).texture_.Get(); });
    }
    void BeginFrame() override {
        resources_.CheckReady();
        if (in_frame_) throw std::logic_error("render.frame_already_open");
        resources_.BeginFrame();
        in_frame_ = true;
        passes_ = 0;
        draws_ = 0;
    }
    void Submit(TextureHandle target, const DrawList& list, std::uint32_t clear) override {
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        resources_.Validate(target, list);
        for (const auto& command : list.commands_) {
            if (!command.image_program_) continue;
            if (!image_programs_) throw std::invalid_argument("render.image_program_input");
            image_programs_->Validate(*command.image_program_);
        }
        resources_.RecordSamples(list);
        // Reserve the last 16 views for host/UI presentation after graph admission fails.
        if (passes_ >= (target == TextureHandle{} ? 256U : 240U))
            throw BudgetExceeded(Budget::kPasses);
        const auto view = static_cast<bgfx::ViewId>(passes_++);
        auto extent = size_;
        bgfx::FrameBufferHandle framebuffer = BGFX_INVALID_HANDLE;
        if (target != TextureHandle{}) {
            extent = resources_.Size(target);
            framebuffer = textures_[target.slot_].framebuffer_.Get();
            if (!bgfx::isValid(framebuffer))
                throw std::invalid_argument("render.not_render_target");
        } else {
            const Extent requested{static_cast<std::uint16_t>(list.width_),
                                   static_cast<std::uint16_t>(list.height_)};
            if (requested != size_) {
                size_ = requested;
                extent = size_;
                bgfx::reset(size_.width_, size_.height_, BGFX_RESET_VSYNC);
                ++presentation_generation_;
            }
        }
        bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
        bgfx::setViewFrameBuffer(view, framebuffer);
        bgfx::setViewRect(view, 0, 0, extent.width_, extent.height_);
        const auto alpha = clear & 255;
        const auto clear_channel = [&](std::uint32_t shift) {
            return (((clear >> shift) & 255) * alpha + 127) / 255;
        };
        bgfx::setViewClear(
                view, BGFX_CLEAR_COLOR,
                clear_channel(24) << 24 | clear_channel(16) << 16 | clear_channel(8) << 8 | alpha);
        float projection[16]{};
        const bool invert = target != TextureHandle{} && invert_targets_;
        bx::mtxOrtho(projection, 0, list.width_, invert ? 0 : list.height_,
                     invert ? list.height_ : 0, 0, 100, 0, homogeneous_depth_);
        bgfx::setViewTransform(view, nullptr, projection);
        bgfx::touch(view);
        if (list.commands_.empty()) return;
        bgfx::TransientVertexBuffer vertices{};
        bgfx::TransientIndexBuffer indices{};
        const auto vertex_count = static_cast<std::uint32_t>(list.vertices_.size());
        const auto index_count = static_cast<std::uint32_t>(list.indices_.size());
        if (!bgfx::allocTransientBuffers(&vertices, layout_, vertex_count, &indices, index_count,
                                         true))
            throw std::length_error("render.transient_budget");
        for (std::size_t index = 0; index < list.vertices_.size(); ++index) {
            auto vertex = list.vertices_[index];
            vertex.color_ = PremultiplyAbgr(vertex.color_);
            std::memcpy(vertices.data + index * sizeof(Vertex), &vertex, sizeof(Vertex));
        }
        std::memcpy(indices.data, list.indices_.data(),
                    list.indices_.size() * sizeof(std::uint32_t));
        for (const auto& command : list.commands_) {
            const auto scale_x = extent.width_ / list.width_;
            const auto scale_y = extent.height_ / list.height_;
            const auto left = std::clamp(command.clip_.x_ * scale_x, 0.0f, float(extent.width_));
            const auto top = std::clamp(command.clip_.y_ * scale_y, 0.0f, float(extent.height_));
            const auto right = std::clamp((command.clip_.x_ + command.clip_.width_) * scale_x, left,
                                          float(extent.width_));
            const auto bottom = std::clamp((command.clip_.y_ + command.clip_.height_) * scale_y,
                                           top, float(extent.height_));
            if (right <= left || bottom <= top || !command.index_count_) continue;
            bgfx::setScissor(static_cast<std::uint16_t>(left),
                             static_cast<std::uint16_t>(invert ? extent.height_ - bottom : top),
                             static_cast<std::uint16_t>(right - left),
                             static_cast<std::uint16_t>(bottom - top));
            bgfx::setVertexBuffer(0, &vertices);
            bgfx::setIndexBuffer(&indices, command.first_index_, command.index_count_);
            auto blending =
                    BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            switch (command.blend_) {
                case BlendMode::kSourceOver:
                    break;
                case BlendMode::kAdd:
                    blending =
                            IsValid(target) && resources_.Precision(target) ==
                                                       TexturePrecision::kFloat16
                                    ? BGFX_STATE_BLEND_FUNC_SEPARATE(
                                              BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE,
                                              BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                                    : BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                                            BGFX_STATE_BLEND_ONE);
                    break;
                case BlendMode::kAlphaMask:
                    blending = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO,
                                                     BGFX_STATE_BLEND_SRC_ALPHA);
                    break;
                case BlendMode::kInverseAlphaMask:
                    blending = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO,
                                                     BGFX_STATE_BLEND_INV_SRC_ALPHA);
                    break;
            }
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | blending | BGFX_STATE_MSAA);
            const auto map = command.depth_of_field_     ? command.depth_of_field_->depth_
                             : command.texture_trail_    ? command.texture_trail_->history_
                             : command.texture_displace_ ? command.texture_displace_->map_
                                                         : command.texture_;
            if (command.image_program_)
                image_programs_->Submit(view, *command.image_program_,
                                        textures_[command.texture_.slot_].texture_.Get(), extent);
            else
                texture_programs_->Submit(view, command, resources_.Size(command.texture_),
                                          list.width_ / list.height_,
                                          textures_[command.texture_.slot_].texture_.Get(),
                                          resources_.Size(map), textures_[map.slot_].texture_.Get(),
                                          IsValid(target) && resources_.Precision(target) ==
                                                                     TexturePrecision::kFloat16);
            ++draws_;
        }
    }
    void EndFrame() override {
        resources_.CheckThread();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        const auto completed = bgfx::frame();
        readbacks_.Advance(completed, resources_);
        in_frame_ = false;
        ++frame_;
    }
    FrameStats Stats() const override {
        auto stats = resources_.Stats();
        if (scene_) scene_->AddStats(stats);
        if (gpu_points_) gpu_points_->AddStats(stats);
        if (image_programs_) image_programs_->AddStats(stats);
        stats.frame_ = frame_;
        stats.passes_ = passes_;
        stats.draws_ = draws_;
        stats.presentation_generation_ = presentation_generation_;
        return stats;
    }
    void Invalidate() override {
        if (in_frame_) throw std::logic_error("render.frame_still_open");
        readbacks_.Invalidate(resources_);
        resources_.Invalidate();
        if (scene_) scene_->Invalidate();
        if (gpu_points_) gpu_points_->Invalidate();
        if (image_programs_) image_programs_->Invalidate();
    }

   private:
    struct Entry {
        GpuHandle<bgfx::TextureHandle> texture_{};
        GpuHandle<bgfx::FrameBufferHandle> framebuffer_{};
    };
    // Destruction order: staging GPU handles, then bgfx shutdown, then pixels.
    // Pending read destinations therefore survive cancellation and device loss.
    std::shared_ptr<ReadbackMemory> readback_memory_ = std::make_shared<ReadbackMemory>();
    std::optional<DeviceLifetime> lifetime_{};
    Extent size_{};
    ResourceTable resources_{};
    BgfxReadbacks readbacks_{readback_memory_};
    std::vector<Entry> textures_{};
    std::unique_ptr<BgfxScene> scene_{};
    std::unique_ptr<BgfxGpuPoints> gpu_points_{};
    std::unique_ptr<BgfxImagePrograms> image_programs_{};
    bgfx::VertexLayout layout_{};
    std::optional<BgfxTexturePrograms> texture_programs_{};
    bool in_frame_ = false;
    bool invert_targets_ = false;
    bool homogeneous_depth_ = false;
    bool scenes_supported_ = false;
    std::uint32_t passes_ = 0;
    std::uint32_t draws_ = 0;
    std::uint64_t frame_ = 0;
    std::uint64_t presentation_generation_ = 0;
};
}  // namespace
std::shared_ptr<Backend> CreateBgfxBackend(std::uintptr_t window, Extent size,
                                           std::shared_ptr<void> owner, std::uintptr_t context) {
    return std::make_shared<BgfxBackend>(window, size, std::move(owner), context);
}
}  // namespace rhythm::render::detail
