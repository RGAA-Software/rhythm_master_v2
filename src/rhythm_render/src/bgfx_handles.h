#pragma once

#include <bgfx/bgfx.h>

#include <stdexcept>

namespace rhythm::render::detail {
// External integer handles are move-only RAII values, private to the backend
// and its native validation probes. Final release runs on the device thread.
template <typename Handle>
class GpuHandle final {
   public:
    GpuHandle() = default;
    explicit GpuHandle(Handle handle) : handle_(handle) {
        if (!bgfx::isValid(handle)) throw std::runtime_error("render.gpu_allocation");
    }
    ~GpuHandle() { Reset(); }
    GpuHandle(GpuHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = BGFX_INVALID_HANDLE;
    }
    GpuHandle& operator=(GpuHandle&& other) noexcept {
        if (this != &other) {
            Reset();
            handle_ = other.handle_;
            other.handle_ = BGFX_INVALID_HANDLE;
        }
        return *this;
    }
    GpuHandle(const GpuHandle&) = delete;
    GpuHandle& operator=(const GpuHandle&) = delete;
    Handle Get() const { return handle_; }

   private:
    void Reset() noexcept {
        if (bgfx::isValid(handle_)) bgfx::destroy(handle_);
        handle_ = BGFX_INVALID_HANDLE;
    }
    Handle handle_ = BGFX_INVALID_HANDLE;
};
}  // namespace rhythm::render::detail
