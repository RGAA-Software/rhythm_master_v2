#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "rhythm/platform/host.h"
namespace rhythm::studio {
struct FrameStatus {
    std::size_t authored_nodes_ = 0;
    std::size_t visible_nodes_ = 0;
    std::size_t viewers_ = 0;
    std::size_t inline_previews_ = 0;
    float audio_rms_ = 0;
    bool budget_limited_ = false;
    std::uint32_t recycled_textures_ = 0;
    std::size_t profiled_nodes_ = 0;
    std::size_t component_inline_previews_ = 0;
    std::size_t signal_previews_ = 0;
    std::size_t waveform_bins_ = 0;
    std::size_t preview_group_ = 0;
    std::size_t preview_groups_ = 0;
    std::size_t clip_waveform_sources_ = 0;
};
// UI-thread value snapshot for support/acceptance evidence. No backend objects,
// generated schema or media resources cross this inspection boundary.
struct WorkflowStatus {
    std::uint64_t selected_author_node_ = 0;
    std::uint64_t requested_generation_ = 0;
    std::uint64_t installed_generation_ = 0;
    std::vector<std::string> graph_errors_{};
    bool shader_busy_ = false;
    std::string shader_error_{};
    std::string export_state_ = "unavailable";
    std::string export_phase_ = "idle";
    std::uint64_t exported_frames_ = 0;
    std::uint64_t export_total_frames_ = 0;
    std::string export_error_{};
    bool operator==(const WorkflowStatus&) const = default;
};
class Studio final {
   public:
    Studio(const std::filesystem::path& resources, const std::filesystem::path& project);
    ~Studio();
    Studio(const Studio&) = delete;
    Studio& operator=(const Studio&) = delete;
    void Frame(platform::Host& host, render::Renderer& renderer, double seconds);
    void SetSuspended(bool suspended);
    void SetTextureReuse(bool enabled);
    void LoadAudioFile(const std::filesystem::path& path, float volume = 1);
    // True only after the current compilation request has succeeded. A retained
    // previous output while loading or diagnosing a new graph is not success.
    bool HasValidPlan() const;
    FrameStatus Status() const;
    WorkflowStatus Workflow() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::studio
