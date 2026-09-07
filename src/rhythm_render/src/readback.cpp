#include <stdexcept>
#include <utility>

#include "backend.h"
#include "rhythm/render/renderer.h"

namespace rhythm::render {
Readback::Readback(std::shared_ptr<detail::Backend> backend, std::uint64_t ticket)
    : backend_(std::move(backend)), ticket_(ticket) {}
Readback::~Readback() { Reset(); }
Readback::Readback(Readback&& other) noexcept
    : backend_(std::move(other.backend_)), ticket_(std::exchange(other.ticket_, 0)) {}
Readback& Readback::operator=(Readback&& other) noexcept {
    if (this != &other) {
        Reset();
        backend_ = std::move(other.backend_);
        ticket_ = std::exchange(other.ticket_, 0);
    }
    return *this;
}
void Readback::Reset() noexcept {
    if (backend_) backend_->CancelReadback(ticket_);
    ticket_ = 0;
    backend_.reset();
}
std::optional<ReadbackImage> Readback::Poll() {
    if (!backend_ || !ticket_) throw std::logic_error("render.readback_closed");
    auto result = backend_->PollReadback(ticket_);
    if (result) Reset();
    return result;
}
bool Renderer::SupportsReadback() const {
    if (!backend_) throw std::logic_error("render.moved_from");
    return backend_->SupportsReadback();
}
Readback Renderer::RequestReadback(TextureHandle texture) {
    if (!backend_) throw std::logic_error("render.moved_from");
    return Readback(backend_, backend_->RequestReadback(texture));
}
}  // namespace rhythm::render
