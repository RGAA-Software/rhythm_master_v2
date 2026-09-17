#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

#include "rhythm/render/texture_handle.h"

namespace rhythm::render {
namespace detail {
class Backend;
}
struct GpuPointHandle {
    std::uint64_t device_ = 0;
    std::uint32_t slot_ = 0;
    std::uint32_t generation_ = 0;
    bool operator==(const GpuPointHandle&) const = default;
};
inline constexpr std::uint32_t kMaximumGpuPoints = 262144;
inline constexpr std::uint32_t kMaximumGpuPointTotal = 1048576;
// Fixed typed buffer: position/age, velocity/lifetime, straight RGBA,
// size/rotation/spawn-random. Position is canvas-normalized, size a fraction of
// canvas height, time in seconds. The third shape component carries a stable
// per-particle random in [0,1) assigned at spawn; renderers may use it for
// atlas cell or shape variation and the simulation copies it unchanged.
// No CPU memory mapping or native binding is exposed. Mutations run on the device
// thread in submitted frame order; the first update must initialize every record.
struct GpuParticleStep {
    float seconds_ = 0;
    bool reset_ = false;
    std::uint32_t spawn_start_ = 0;
    std::uint32_t spawn_count_ = 0;
    std::uint32_t sequence_ = 0;
    std::uint32_t seed_ = 1;
    // Center xy are canvas-normalized; z is the view-space spawn depth used by
    // soft depth intersection, accumulated per particle by velocity z.
    std::array<float, 3> center_{0.5f, 0.5f, 0};
    float radius_ = 0.3f;
    float speed_ = 0.08f;
    float drag_ = 0.1f;
    float lifetime_ = 4;
    float size_ = 0.004f;
    std::array<float, 2> gravity_{0, 0.01f};
    float flow_ = 0.1f;
    float frequency_ = 8;
    float phase_ = 0;
    std::array<float, 4> color_a_{0.05f, 0.6f, 1, 1};
    std::array<float, 4> color_b_{1, 0.1f, 0.4f, 1};
};
// Independent whole-buffer copy/map; source and destination must be distinct,
// valid, equal-capacity handles and source must already be initialized. Device
// thread and open frame only; each successful call consumes one ordered pass.
// Column-major affine position transform, finite coefficients in [-16,16].
// Position output clamps to +/-10000, size to [0,1]; color factors are [0,1].
// Age, velocity, lifetime, rotation and reserved fields are copied unchanged.
struct GpuPointMapping {
    std::array<float, 16> transform_{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::array<float, 4> color_{1, 1, 1, 1};
    float size_ = 1;  // Multiplier [0,16].
};
// A lazy attribute view: sample the current premultiplied image at each point's
// canvas-normalized center. Source point records remain unchanged. UVs clamp at
// image edges, and luminance-based size uses premultiplied RGB (transparent = 0).
struct GpuPointSampling {
    TextureHandle texture_{};
    float color_amount_ = 1;
    float size_amount_ = 0;
    bool operator==(const GpuPointSampling&) const = default;
};
// Optional sprite atlas for the point renderer. Each particle picks one stable
// cell from its spawn random; the sampled shape and tint modulate the analytic
// soft envelope, so edges stay smooth. Columns and rows are in [1,64].
struct GpuPointAtlas {
    TextureHandle texture_{};
    std::uint32_t columns_ = 1;
    std::uint32_t rows_ = 1;
    bool operator==(const GpuPointAtlas&) const = default;
};
// Optional soft depth intersection (Godot proximity fade semantics). The depth
// texture must be a scene depth attachment of the same extent as the target;
// near/far are positive view-space distances matching the projection that wrote
// the depth. Each fragment reconstructs the scene distance and fades the sprite
// alpha across distance_ view units as it approaches the scene surface.
struct GpuPointSoftDepth {
    TextureHandle texture_{};
    float near_ = 0;
    float far_ = 1;
    bool orthographic_ = false;
    float distance_ = 0.1f;
    bool operator==(const GpuPointSoftDepth&) const = default;
};
struct GpuPointStyle {
    float opacity_ = 1;
    bool additive_ = true;
    std::optional<GpuPointSampling> sampling_{};
    // Expands the same analytic sprite for a local halo; no fullscreen blur pass.
    float glow_radius_ = 1;
    std::optional<GpuPointAtlas> atlas_{};
    std::optional<GpuPointSoftDepth> soft_depth_{};
};
// Move-only owner retains the backend through final release, like Texture/Mesh.
// Handles are generation-checked observers; all destruction is device-thread only.
class GpuPoints final {
   public:
    GpuPoints() = default;
    ~GpuPoints();
    GpuPoints(GpuPoints&& other) noexcept;
    GpuPoints& operator=(GpuPoints&& other) noexcept;
    GpuPoints(const GpuPoints&) = delete;
    GpuPoints& operator=(const GpuPoints&) = delete;
    GpuPointHandle Handle() const { return handle_; }

   private:
    friend class Renderer;
    GpuPoints(std::shared_ptr<detail::Backend> backend, GpuPointHandle handle);
    void Reset() noexcept;
    std::shared_ptr<detail::Backend> backend_{};
    GpuPointHandle handle_{};
};
}  // namespace rhythm::render
