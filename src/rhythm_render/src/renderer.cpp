#include "rhythm/render/renderer.h"

#include <stdexcept>
#include <utility>

#include "backend.h"

namespace rhythm::render {
Texture::Texture(std::shared_ptr<detail::Backend> backend, TextureHandle handle)
    : backend_(std::move(backend)), handle_(handle) {}
Texture::~Texture() { Reset(); }
void Texture::Reset() noexcept {
    if (backend_) backend_->Release(handle_);
    handle_ = {};
    backend_.reset();
}
Texture::Texture(Texture&& other) noexcept
    : backend_(std::move(other.backend_)), handle_(std::exchange(other.handle_, {})) {}
Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        Reset();
        backend_ = std::move(other.backend_);
        handle_ = std::exchange(other.handle_, {});
    }
    return *this;
}
Renderer::Renderer(std::shared_ptr<detail::Backend> backend) : backend_(std::move(backend)) {}
Renderer Renderer::CreateNull() { return Renderer(detail::CreateNullBackend()); }
Texture Renderer::CreateTexture(Extent extent, std::span<const std::uint8_t> rgba,
                                TexturePrecision precision) {
    if (!backend_) throw std::logic_error("render.moved_from");
    return Texture(backend_, backend_->Create(extent, rgba, precision));
}
bool Renderer::IsValid(TextureHandle handle) const { return backend_ && backend_->IsValid(handle); }
Texture Renderer::RetainTexture(TextureHandle texture) {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->Retain(texture);
    return Texture(backend_, texture);
}
TexturePrecision Renderer::Precision(TextureHandle handle) const {
    if (!backend_) throw std::logic_error("render.moved_from");
    return backend_->Precision(handle);
}
void Renderer::UpdateTexture(TextureHandle texture, std::span<const std::uint8_t> rgba) {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->Update(texture, rgba);
}
void Renderer::BeginFrame() {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->BeginFrame();
}
void Renderer::Submit(TextureHandle target, const DrawList& list, std::uint32_t clear_rgba) {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->Submit(target, list, clear_rgba);
}
void Renderer::EndFrame() {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->EndFrame();
}
FrameStats Renderer::Stats() const {
    if (!backend_) throw std::logic_error("render.moved_from");
    return backend_->Stats();
}
void Renderer::Invalidate() {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->Invalidate();
}
}  // namespace rhythm::render
