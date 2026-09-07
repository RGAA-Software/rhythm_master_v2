#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "rhythm/render/scene.h"

namespace rhythm::platform {
class Host;
}

namespace rhythm::render {
namespace detail {
class Backend;
}

struct TextureHandle {
    std::uint64_t device_ = 0;
    std::uint32_t slot_ = 0;
    std::uint32_t generation_ = 0;
    bool operator==(const TextureHandle&) const = default;
};

struct Extent {
    std::uint16_t width_ = 0;
    std::uint16_t height_ = 0;
    bool operator==(const Extent&) const = default;
};

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
    // Surface resets can discard submissions made earlier in that frame.
    // Cached producers must redraw when this generation changes.
    std::uint64_t presentation_generation_ = 0;
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
    Texture CreateTexture(Extent extent, std::span<const std::uint8_t> rgba = {});
    [[nodiscard]] bool IsValid(TextureHandle handle) const;
    Mesh CreateMesh(std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices);
    [[nodiscard]] bool IsValid(MeshHandle handle) const;
    [[nodiscard]] bool SupportsScenes() const;
    void BeginFrame();
    // Default target means the host surface; texture targets are explicit resources.
    void Submit(TextureHandle target, const DrawList& list, std::uint32_t clear_rgba = 0);
    // Scene output requires an offscreen render target and clears its depth.
    void SubmitScene(TextureHandle target, const SceneDrawList& list, std::uint32_t clear_rgba = 0);
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
