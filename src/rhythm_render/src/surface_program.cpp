#include <utility>

#include "backend.h"

namespace rhythm::render {
SurfaceProgram::SurfaceProgram(std::shared_ptr<detail::Backend> backend,
                               SurfaceProgramHandle handle)
    : backend_(std::move(backend)), handle_(handle) {}
SurfaceProgram::~SurfaceProgram() { Reset(); }
void SurfaceProgram::Reset() noexcept {
    if (backend_) backend_->ReleaseSurfaceProgram(handle_);
    handle_ = {};
    backend_.reset();
}
SurfaceProgram::SurfaceProgram(SurfaceProgram&& other) noexcept
    : backend_(std::move(other.backend_)), handle_(std::exchange(other.handle_, {})) {}
SurfaceProgram& SurfaceProgram::operator=(SurfaceProgram&& other) noexcept {
    if (this != &other) {
        Reset();
        backend_ = std::move(other.backend_);
        handle_ = std::exchange(other.handle_, {});
    }
    return *this;
}
SurfaceProgramTarget Renderer::SurfaceTarget() const {
    if (!backend_) throw std::logic_error("render.moved_from");
    return backend_->ImageTarget() == ImageProgramTarget::kGles300
                   ? SurfaceProgramTarget::kGles300
                   : SurfaceProgramTarget::kWindowsSm5;
}
SurfaceProgram Renderer::CreateSurfaceProgram(std::span<const std::uint8_t> artifact) {
    if (!backend_) throw std::logic_error("render.moved_from");
    return SurfaceProgram(backend_, backend_->CreateSurfaceProgram(artifact));
}
bool Renderer::IsValid(SurfaceProgramHandle handle) const {
    return backend_ && backend_->IsValid(handle);
}
}  // namespace rhythm::render
