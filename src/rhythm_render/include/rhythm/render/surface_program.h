#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace rhythm::render {
namespace detail {
class Backend;
}
enum class SurfaceProgramTarget : std::uint8_t { kWindowsSm5, kGles300 };
struct SurfaceProgramHandle {
    std::uint64_t device_ = 0;
    std::uint32_t slot_ = 0;
    std::uint32_t generation_ = 0;
    bool operator==(const SurfaceProgramHandle&) const = default;
};
struct SurfaceProgramInput {
    SurfaceProgramHandle program_{};
    std::array<float, 4> parameters_{};
    float seconds_ = 0;
    bool operator==(const SurfaceProgramInput&) const = default;
};
// Owns one compiled surface-expression program. Handles are borrowed values;
// creation, use and final release are confined to the renderer's host thread.
class SurfaceProgram final {
   public:
    SurfaceProgram() = default;
    ~SurfaceProgram();
    SurfaceProgram(SurfaceProgram&& other) noexcept;
    SurfaceProgram& operator=(SurfaceProgram&& other) noexcept;
    SurfaceProgram(const SurfaceProgram&) = delete;
    SurfaceProgram& operator=(const SurfaceProgram&) = delete;
    SurfaceProgramHandle Handle() const { return handle_; }

   private:
    friend class Renderer;
    SurfaceProgram(std::shared_ptr<detail::Backend> backend, SurfaceProgramHandle handle);
    void Reset() noexcept;
    std::shared_ptr<detail::Backend> backend_{};
    SurfaceProgramHandle handle_{};
};
}  // namespace rhythm::render
