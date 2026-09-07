#pragma once

#include <filesystem>
#include <memory>

#include "rhythm/render/renderer.h"

namespace rhythm::platform {
class Host final {
   public:
    explicit Host(bool hidden = false);
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    render::Renderer CreateRenderer();
    bool Poll();
    bool IsSuspended() const;
    void Resize(render::Extent logical_size);
    void BeginUi();
    render::DrawList EndUi();
    render::Texture CreateFontTexture(render::Renderer& renderer);
    std::uint64_t RegisterTexture(render::TextureHandle texture);
    void ClearViewerTextures();
    std::filesystem::path DataDirectory() const;
    std::filesystem::path ResourceDirectory() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::platform
