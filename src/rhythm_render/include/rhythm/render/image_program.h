#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace rhythm::render {
namespace detail {
class Backend;
}
enum class ImageProgramTarget : std::uint8_t { kWindowsSm5, kGles300 };
struct ImageProgramHandle {
    std::uint64_t device_ = 0;
    std::uint32_t slot_ = 0;
    std::uint32_t generation_ = 0;
    bool operator==(const ImageProgramHandle&) const = default;
};
struct ImageProgramInput {
    ImageProgramHandle program_{};
    std::array<float, 4> parameters_{};
    float seconds_ = 0;
};
// Owns one compiled image-expression program. Handles are borrowed values;
// creation, use and final release are confined to the renderer's host thread.
class ImageProgram final {
   public:
    ImageProgram() = default;
    ~ImageProgram();
    ImageProgram(ImageProgram&& other) noexcept;
    ImageProgram& operator=(ImageProgram&& other) noexcept;
    ImageProgram(const ImageProgram&) = delete;
    ImageProgram& operator=(const ImageProgram&) = delete;
    ImageProgramHandle Handle() const { return handle_; }

   private:
    friend class Renderer;
    ImageProgram(std::shared_ptr<detail::Backend> backend, ImageProgramHandle handle);
    void Reset() noexcept;
    std::shared_ptr<detail::Backend> backend_{};
    ImageProgramHandle handle_{};
};
}  // namespace rhythm::render
