#include <utility>

#include "backend.h"
#include "rhythm/render/renderer.h"

namespace rhythm::render {
GpuPoints::GpuPoints(std::shared_ptr<detail::Backend> backend, GpuPointHandle handle)
    : backend_(std::move(backend)), handle_(handle) {}
GpuPoints::~GpuPoints() { Reset(); }
void GpuPoints::Reset() noexcept {
    if (backend_) backend_->ReleaseGpuPoints(handle_);
    handle_ = {};
    backend_.reset();
}
GpuPoints::GpuPoints(GpuPoints&& other) noexcept
    : backend_(std::move(other.backend_)), handle_(std::exchange(other.handle_, {})) {}
GpuPoints& GpuPoints::operator=(GpuPoints&& other) noexcept {
    if (this != &other) {
        Reset();
        backend_ = std::move(other.backend_);
        handle_ = std::exchange(other.handle_, {});
    }
    return *this;
}
bool Renderer::SupportsGpuPoints() const { return backend_ && backend_->SupportsGpuPoints(); }
GpuPoints Renderer::CreateGpuPoints(std::uint32_t capacity) {
    if (!backend_) throw std::logic_error("render.moved_from");
    return GpuPoints(backend_, backend_->CreateGpuPoints(capacity));
}
bool Renderer::IsValid(GpuPointHandle handle) const {
    return backend_ && backend_->IsValid(handle);
}
void Renderer::UpdateGpuParticles(GpuPointHandle handle, const GpuParticleStep& step) {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->UpdateGpuParticles(handle, step);
}
void Renderer::MapGpuPoints(GpuPointHandle source, GpuPointHandle destination,
                            const GpuPointMapping& mapping) {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->MapGpuPoints(source, destination, mapping);
}
void Renderer::SubmitGpuPoints(TextureHandle target, GpuPointHandle points,
                               const GpuPointStyle& style) {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->SubmitGpuPoints(target, points, style);
}
}  // namespace rhythm::render
