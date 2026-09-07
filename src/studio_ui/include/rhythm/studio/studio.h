#pragma once
#include <filesystem>
#include <memory>

#include "rhythm/platform/host.h"
namespace rhythm::studio {
struct FrameStatus {
    std::size_t authored_nodes_ = 0;
    std::size_t visible_nodes_ = 0;
    std::size_t viewers_ = 0;
    std::size_t inline_previews_ = 0;
    float audio_rms_ = 0;
};
class Studio final {
   public:
    Studio(const std::filesystem::path& resources, const std::filesystem::path& project);
    ~Studio();
    Studio(const Studio&) = delete;
    Studio& operator=(const Studio&) = delete;
    void Frame(platform::Host& host, render::Renderer& renderer, double seconds);
    void SetSuspended(bool suspended);
    void LoadAudioFile(const std::filesystem::path& path, float volume = 1);
    bool HasValidPlan() const;
    FrameStatus Status() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::studio
