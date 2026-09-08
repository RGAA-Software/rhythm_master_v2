#include <stdexcept>
#include <utility>

#include "backend.h"
#include "rhythm/render/renderer.h"

namespace rhythm::render {
Mesh::Mesh(std::shared_ptr<detail::Backend> backend, MeshHandle handle)
    : backend_(std::move(backend)), handle_(handle) {}
Mesh::~Mesh() { Reset(); }
void Mesh::Reset() noexcept {
    if (backend_) backend_->ReleaseMesh(handle_);
    handle_ = {};
    backend_.reset();
}
Mesh::Mesh(Mesh&& other) noexcept
    : backend_(std::move(other.backend_)), handle_(std::exchange(other.handle_, {})) {}
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        Reset();
        backend_ = std::move(other.backend_);
        handle_ = std::exchange(other.handle_, {});
    }
    return *this;
}
Mesh Renderer::CreateMesh(std::span<const MeshVertex> vertices,
                          std::span<const std::uint32_t> indices, std::span<const SkinWeights> skin,
                          std::span<const MorphTarget> morphs) {
    if (!backend_) throw std::logic_error("render.moved_from");
    return Mesh(backend_, backend_->CreateMesh(vertices, indices, skin, morphs));
}
bool Renderer::IsValid(MeshHandle handle) const { return backend_ && backend_->IsValid(handle); }
bool Renderer::SupportsScenes() const { return backend_ && backend_->SupportsScenes(); }
bool Renderer::SupportsSampleableDepth() const {
    return backend_ && backend_->SupportsSampleableDepth();
}
Texture Renderer::CreateDepthTexture(Extent extent) {
    if (!backend_) throw std::logic_error("render.no_backend");
    return Texture(backend_, backend_->CreateDepth(extent));
}
void Renderer::SubmitSceneDepth(TextureHandle color, TextureHandle depth, const SceneDrawList& list,
                                std::uint32_t clear) {
    if (!backend_) throw std::logic_error("render.no_backend");
    backend_->SubmitSceneDepth(color, depth, list, clear);
}
void Renderer::SubmitScene(TextureHandle target, const SceneDrawList& list, std::uint32_t clear) {
    if (!backend_) throw std::logic_error("render.moved_from");
    backend_->SubmitScene(target, list, clear);
}
}  // namespace rhythm::render
