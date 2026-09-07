#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "rhythm/render/renderer.h"

namespace rhythm::platform {
class Host final {
   public:
    Host();
    ~Host();
    bool Poll();
    [[nodiscard]] bool Suspended() const;
    [[nodiscard]] std::uint64_t SurfaceGeneration() const;
    [[nodiscard]] render::Extent Size() const;
    render::Renderer CreateRenderer();
    std::string ReadAsset(const std::string& name) const;
    std::filesystem::path DataDirectory() const;
    std::filesystem::path CacheDirectory() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::platform
