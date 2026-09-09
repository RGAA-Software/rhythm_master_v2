#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "rhythm/render/color_pipeline.h"
#include "rhythm/render/gpu_points.h"
#include "rhythm/render/image_program.h"
#include "rhythm/render/scene.h"
#include "rhythm/render/surface_program.h"

namespace rhythm::platform {
class Host;
}

namespace rhythm::render {
namespace detail {
class Backend;
}

struct Extent {
    std::uint16_t width_ = 0;
    std::uint16_t height_ = 0;
    bool operator==(const Extent&) const = default;
};

// Float targets retain low-amplitude history through repeated temporal filtering.
// Uploads remain RGBA8; kFloat16 is for render targets only, at eight bytes/pixel.
enum class TexturePrecision : std::uint8_t { kUnorm8, kFloat16 };

struct Vertex {
    // Positions and normalized texture UVs use the logical top-left origin.
    // Uploaded RGBA row zero and render-target row zero address the same edge.
    float x_ = 0;
    float y_ = 0;
    float u_ = 0;
    float v_ = 0;
    std::uint32_t color_ = 0xffffffff;
};

struct ClipRect {
    float x_ = 0;
    float y_ = 0;
    float width_ = 0;
    float height_ = 0;
};

enum class BlendMode : std::uint8_t { kSourceOver, kAdd, kAlphaMask, kInverseAlphaMask };

struct ColorAdjustment {
    float exposure_ = 0;
    float contrast_ = 1;
    float saturation_ = 1;
    float invert_ = 0;
};

enum class TextureFilterKind : std::uint8_t { kGaussian, kDownsample };
// Sampling offsets are in source-image pixels. Filtering operates on internal
// premultiplied RGBA, preserving transparent coverage through repeated passes.
struct TextureFilter {
    TextureFilterKind kind_ = TextureFilterKind::kGaussian;
    float step_x_ = 1;
    float step_y_ = 0;
};

struct TextureNoise {
    float scale_ = 4;
    float phase_ = 0;
    float contrast_ = 1;
    float seed_ = 0;
    std::array<float, 4> color_a_{0.015f, 0.03f, 0.12f, 1};
    std::array<float, 4> color_b_{0.12f, 0.65f, 0.8f, 1};
};

// Coordinates use image-height units; angles are degrees. Texture edge addressing mirrors.
enum class TextureMappingKind : std::uint8_t { kKaleidoscope, kPolar };
struct TextureMapping {
    TextureMappingKind kind_ = TextureMappingKind::kKaleidoscope;
    float scale_ = 1;
    float rotation_ = 0;
    float travel_ = 0;
    float twist_ = 0;
    float sectors_ = 8;
    float radial_power_ = 1;
};
// Luminance isolines. Palette is straight RGBA; output remains premultiplied.
struct TextureContours {
    float count_ = 12;
    float width_ = 0.12f;
    float phase_ = 0;
    std::array<float, 4> color_a_{0.02f, 0.6f, 1, 1};
    std::array<float, 4> color_b_{1, 0.12f, 0.35f, 1};
};
enum class TextureDisplaceKind : std::uint8_t { kGradient, kVectorRg };
// Strength is in image-height units; radius is in map pixels, rotation in degrees.
// RG vectors use 0.5 as neutral; transparent map pixels carry no vector.
// Both map and source must be valid and distinct from the destination.
struct TextureDisplace {
    TextureHandle map_{};
    TextureDisplaceKind kind_ = TextureDisplaceKind::kGradient;
    float strength_ = 0.05f;
    float radius_ = 2;
    float rotation_ = 0;
};
// Per-channel peak envelope, preserving fresh highlights and decaying history.
struct TextureTrail {
    TextureHandle history_{};
    float retention_ = 0;
    float scale_ = 1;
    float rotation_ = 0;
};
// Window depth is [0,1], increasing away from the camera. Distances are positive
// view-space units; normalized output maps near/far to 0/1 for inspection.
struct DepthLinearization {
    float near_ = 0.05f;
    float far_ = 1000;
    bool orthographic_ = false;
    bool normalize_ = false;
};
struct DepthOfField {
    TextureHandle depth_{};
    DepthLinearization projection_{};
    float focus_ = 3;
    float focus_scale_ = 4;
    float radius_ = 12;
    std::uint32_t samples_ = 32;
};
// Equirectangular source, +Y up. Produces the fixed linear RGBA16F IBL atlas.
struct EnvironmentFilter {
    bool source_srgb_ = true;
};
// Spatial antialiasing for display-referred color. Alpha and premultiplied RGB
// are filtered together. No temporal state or implicit color-space conversion.
struct TextureFxaa {
    float span_ = 8;
    float reduce_multiplier_ = 0.125f;
    float reduce_minimum_ = 0.0078125f;
    float strength_ = 1;
};
inline constexpr Extent kEnvironmentAtlasExtent{780, 66};
struct DrawCommand {
    TextureHandle texture_{};
    std::uint32_t first_index_ = 0;
    std::uint32_t index_count_ = 0;
    ClipRect clip_{};
    BlendMode blend_ = BlendMode::kSourceOver;
    std::optional<ColorAdjustment> color_adjustment_{};
    std::optional<TextureFilter> texture_filter_{};
    std::optional<TextureNoise> texture_noise_{};
    std::optional<TextureMapping> texture_mapping_{};
    std::optional<TextureContours> texture_contours_{};
    std::optional<TextureDisplace> texture_displace_{};
    std::optional<TextureTrail> texture_trail_{};
    std::optional<ColorPipeline> color_pipeline_{};
    std::optional<DepthLinearization> depth_linearization_{};
    std::optional<DepthOfField> depth_of_field_{};
    std::optional<EnvironmentFilter> environment_filter_{};
    std::optional<ImageProgramInput> image_program_{};
    std::optional<TextureFxaa> texture_fxaa_{};
};

// Owned frame data; third-party draw buffers never survive their boundary call.
struct DrawList {
    std::vector<Vertex> vertices_{};
    std::vector<std::uint32_t> indices_{};
    std::vector<DrawCommand> commands_{};
    float width_ = 0;
    float height_ = 0;
};

struct FrameStats {
    std::uint64_t frame_ = 0;
    std::uint32_t passes_ = 0;
    std::uint32_t draws_ = 0;
    std::uint32_t live_textures_ = 0;
    std::uint64_t texture_bytes_ = 0;
    std::uint32_t live_meshes_ = 0;
    std::uint64_t mesh_bytes_ = 0;
    std::uint32_t gpu_point_buffers_ = 0;
    std::uint32_t gpu_point_capacity_ = 0;
    std::uint64_t gpu_point_bytes_ = 0;
    // Surface resets can discard submissions made earlier in that frame.
    // Cached producers must redraw when this generation changes.
    std::uint64_t presentation_generation_ = 0;
    std::uint32_t surface_programs_ = 0;
    std::uint64_t surface_program_bytes_ = 0;
    std::uint32_t image_programs_ = 0;
    std::uint64_t image_program_bytes_ = 0;  // Compiled payload, not driver allocation size.
};

// Move-only resource ownership. Shared backend lifetime ensures destruction order;
// handles alone are observers. All calls and final resource release are host-thread only.
class Texture final {
   public:
    Texture() = default;
    ~Texture();
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    [[nodiscard]] TextureHandle Handle() const { return handle_; }

   private:
    friend class Renderer;
    Texture(std::shared_ptr<detail::Backend> backend, TextureHandle handle);
    void Reset() noexcept;
    std::shared_ptr<detail::Backend> backend_{};
    TextureHandle handle_{};
};

struct ReadbackImage {
    Extent extent_{};
    // Tightly packed top-left RGBA8, premultiplied alpha (SDR over black).
    std::vector<std::uint8_t> rgba_{};
};

// Host-thread ticket. Poll never waits; EndFrame advances completion. Destruction
// cancels delivery while the backend retains native buffers until GPU completion.
// A completed image is an owned value that may be moved to an encoding worker.
class Readback final {
   public:
    Readback() = default;
    ~Readback();
    Readback(Readback&& other) noexcept;
    Readback& operator=(Readback&& other) noexcept;
    Readback(const Readback&) = delete;
    Readback& operator=(const Readback&) = delete;
    std::optional<ReadbackImage> Poll();

   private:
    friend class Renderer;
    Readback(std::shared_ptr<detail::Backend> backend, std::uint64_t ticket);
    void Reset() noexcept;
    std::shared_ptr<detail::Backend> backend_{};
    std::uint64_t ticket_ = 0;
};

class Mesh final {
   public:
    Mesh() = default;
    ~Mesh();
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    [[nodiscard]] MeshHandle Handle() const { return handle_; }

   private:
    friend class Renderer;
    Mesh(std::shared_ptr<detail::Backend> backend, MeshHandle handle);
    void Reset() noexcept;
    std::shared_ptr<detail::Backend> backend_{};
    MeshHandle handle_{};
};

class Renderer final {
   public:
    static Renderer CreateNull();
    Renderer(Renderer&&) noexcept = default;
    Renderer& operator=(Renderer&&) noexcept = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    // Input pixels and vertex colors use straight alpha. Internal target storage
    // and compositing preserve coverage across repeated render passes.
    Texture CreateTexture(Extent extent, std::span<const std::uint8_t> rgba = {},
                          TexturePrecision precision = TexturePrecision::kUnorm8);
    // Host-thread owning lease on an existing texture. Does not copy pixels or
    // reserve the bytes twice. The last lease releases the resource. Callers
    // must stop writes separately when retaining a frozen presentation frame.
    Texture RetainTexture(TextureHandle texture);
    // Host thread, after BeginFrame and before this texture is sampled. Replaces all
    // straight-alpha pixels of an uploaded RGBA8 texture, preserving its handle
    // and extent. Render targets cannot be overwritten through this operation.
    void UpdateTexture(TextureHandle texture, std::span<const std::uint8_t> rgba);
    [[nodiscard]] bool IsValid(TextureHandle handle) const;
    TexturePrecision Precision(TextureHandle handle) const;
    Mesh CreateMesh(std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices,
                    std::span<const SkinWeights> skin = {},
                    std::span<const MorphTarget> morphs = {});
    [[nodiscard]] bool IsValid(MeshHandle handle) const;
    [[nodiscard]] bool SupportsScenes() const;
    [[nodiscard]] bool SupportsGpuPoints() const;
    [[nodiscard]] SurfaceProgramTarget SurfaceTarget() const;
    SurfaceProgram CreateSurfaceProgram(std::span<const std::uint8_t> artifact);
    [[nodiscard]] bool IsValid(SurfaceProgramHandle handle) const;
    [[nodiscard]] ImageProgramTarget ImageTarget() const;
    ImageProgram CreateImageProgram(std::span<const std::uint8_t> artifact);
    [[nodiscard]] bool IsValid(ImageProgramHandle handle) const;
    GpuPoints CreateGpuPoints(std::uint32_t capacity);
    [[nodiscard]] bool IsValid(GpuPointHandle handle) const;
    void UpdateGpuParticles(GpuPointHandle handle, const GpuParticleStep& step);
    void SubmitGpuPoints(TextureHandle target, GpuPointHandle points,
                         const GpuPointStyle& style = {});
    [[nodiscard]] bool SupportsReadback() const;
    // Inside an open frame, after source rendering. RGBA8 render targets only.
    // Three outstanding
    // images maximum, each <= 1080p pixels; staging counts toward texture budgets.
    // Float targets require an explicit SDR conversion pass before requesting.
    Readback RequestReadback(TextureHandle texture);
    void BeginFrame();
    // Default target means the host surface; texture targets are explicit resources.
    void Submit(TextureHandle target, const DrawList& list, std::uint32_t clear_rgba = 0);
    // Scene output requires an offscreen render target and clears its depth.
    void SubmitScene(TextureHandle target, const SceneDrawList& list, std::uint32_t clear_rgba = 0);
    [[nodiscard]] bool SupportsSampleableDepth() const;
    // Depth owns a sampleable attachment. Only scene rendering writes it; use a
    // depth conversion command to inspect it. Ordinary color drawing is rejected.
    Texture CreateDepthTexture(Extent extent);
    void SubmitSceneDepth(TextureHandle color, TextureHandle depth, const SceneDrawList& list,
                          std::uint32_t clear_rgba = 0);
    void EndFrame();
    [[nodiscard]] FrameStats Stats() const;
    // Host-thread loss notification at a completed-frame boundary. All observer
    // handles become invalid; release owners before creating a replacement device.
    void Invalidate();

   private:
    friend class rhythm::platform::Host;
    explicit Renderer(std::shared_ptr<detail::Backend> backend);
    std::shared_ptr<detail::Backend> backend_{};
};
}  // namespace rhythm::render
