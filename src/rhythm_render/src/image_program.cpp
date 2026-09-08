#include <utility>

#include "backend.h"

namespace rhythm::render {
ImageProgram::ImageProgram(std::shared_ptr<detail::Backend> backend, ImageProgramHandle handle)
    : backend_(std::move(backend)), handle_(handle) {}
ImageProgram::~ImageProgram() { Reset(); }
void ImageProgram::Reset() noexcept {
    if (backend_) backend_->ReleaseImageProgram(handle_);
    handle_ = {};
    backend_.reset();
}
ImageProgram::ImageProgram(ImageProgram&& other) noexcept
    : backend_(std::move(other.backend_)), handle_(std::exchange(other.handle_, {})) {}
ImageProgram& ImageProgram::operator=(ImageProgram&& other) noexcept {
    if (this != &other) {
        Reset();
        backend_ = std::move(other.backend_);
        handle_ = std::exchange(other.handle_, {});
    }
    return *this;
}
ImageProgramTarget Renderer::ImageTarget() const {
    if (!backend_) throw std::logic_error("render.moved_from");
    return backend_->ImageTarget();
}
ImageProgram Renderer::CreateImageProgram(std::span<const std::uint8_t> artifact) {
    if (!backend_) throw std::logic_error("render.moved_from");
    return ImageProgram(backend_, backend_->CreateImageProgram(artifact));
}
bool Renderer::IsValid(ImageProgramHandle handle) const {
    return backend_ && backend_->IsValid(handle);
}
}  // namespace rhythm::render
