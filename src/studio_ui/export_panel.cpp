#include "export_panel.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rhythm::studio {
namespace {
bool Busy(exporting::JobState state) {
    return state == exporting::JobState::kPreparing || state == exporting::JobState::kRendering ||
           state == exporting::JobState::kPublishing;
}
std::filesystem::path Path(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}
}  // namespace
void ExportPanel::SetPath(std::array<char, 4096>& buffer, const std::filesystem::path& path) {
    buffer.fill(0);
    const auto text = path.u8string();
    if (text.size() >= buffer.size()) {
        error_ = "export.path_too_long";
        return;
    }
    std::memcpy(buffer.data(), text.data(), text.size());
}
void ExportPanel::Open(const std::filesystem::path& destination,
                       const std::optional<std::filesystem::path>& music, double duration,
                       float gain, graph::Canvas canvas, bool arranged) {
    open_ = true;
    if (Busy(Snapshot().state_)) return;
    error_.clear();
    SetPath(destination_, destination);
    SetPath(music_, music.value_or(std::filesystem::path{}));
    duration_ = std::isfinite(duration) && duration > 0
                        ? static_cast<float>(std::min(duration, 3600.0))
                        : 10;
    gain_ = std::clamp(gain, 0.0f, 1.0f);
    canvas_ = canvas;
    arranged_ = arranged;
}
exporting::JobSnapshot ExportPanel::Snapshot() const {
    return jobs_ ? jobs_->Snapshot() : exporting::JobSnapshot{};
}
void ExportPanel::Start(const std::filesystem::path& executable, editor::Snapshot snapshot,
                        const std::filesystem::path& assets, ExportRequest request) {
    error_.clear();
    try {
        if (!jobs_) jobs_.emplace();
        if (!jobs_->Start(executable, std::move(snapshot), assets, std::move(request.settings_),
                          std::move(request.destination_)))
            error_ = "export.worker_unavailable";
    } catch (const std::exception& error) {
        error_ = error.what();
    }
}
std::optional<ExportRequest> ExportPanel::Draw(const std::map<std::string, std::string>& text) {
    if (!open_) return std::nullopt;
    const auto translate = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    const auto label = [&](const std::string& key) { return translate(key) + "###" + key; };
    const auto area = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(680.0f, area.x), std::min(480.0f, area.y)},
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({std::min(480.0f, area.x), std::min(320.0f, area.y)}, area);
    std::optional<ExportRequest> request;
    if (ImGui::Begin(label("export.open").c_str(), &open_, ImGuiWindowFlags_NoDocking)) {
        const auto state = Snapshot();
        const auto busy = Busy(state.state_);
        ImGui::TextWrapped("%s", translate("export.help").c_str());
        if (arranged_) ImGui::TextWrapped("%s", translate("export.arrangement").c_str());
        ImGui::BeginDisabled(busy);
        ImGui::SetNextItemWidth(-180);
        if (ImGui::InputText(label("export.destination").c_str(), destination_.data(),
                             destination_.size()))
            error_.clear();
        ImGui::SetNextItemWidth(-180);
        if (ImGui::InputText(label("export.music").c_str(), music_.data(), music_.size()))
            error_.clear();
        ImGui::SetNextItemWidth(180);
        ImGui::DragFloat(label("export.duration").c_str(), &duration_, 0.1f, 0.05f, 3600, "%.2f");
        const std::array<std::string, 4> rate_names{"30", "60", "24", "25"};
        const std::array<std::uint32_t, 4> rates{30, 60, 24, 25};
        const std::array<std::string, 3> scales{translate("export.original"), "75%", "50%"};
        const std::array<std::string, 2> codecs{"H.264", "MPEG-4"};
        const std::array<std::string, 3> qualities{translate("export.standard"),
                                                   translate("export.high"),
                                                   translate("export.maximum")};
        const auto combo = [&](const std::string& key, auto& selected, const auto& names) {
            ImGui::SetNextItemWidth(180);
            if (ImGui::BeginCombo(label(key).c_str(), names.at(selected).c_str())) {
                for (std::size_t index = 0; index < names.size(); ++index)
                    if (ImGui::Selectable(names[index].c_str(),
                                          selected == static_cast<int>(index)))
                        selected = static_cast<int>(index);
                ImGui::EndCombo();
            }
        };
        combo("export.fps", fps_, rate_names);
        combo("export.resolution", scale_, scales);
        combo("export.codec", codec_, codecs);
        combo("export.quality", quality_, qualities);
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat(label("export.volume").c_str(), &gain_, 0, 1, "%.2f");
        const auto factor = std::array{1.0, 0.75, 0.5}.at(scale_);
        const auto width =
                std::max(16U, static_cast<std::uint32_t>(canvas_.width_ * factor) / 2 * 2);
        const auto height =
                std::max(16U, static_cast<std::uint32_t>(canvas_.height_ * factor) / 2 * 2);
        const bool valid =
                std::isfinite(duration_) && duration_ > 0 && duration_ <= 3600 && destination_[0];
        const auto frames = valid ? static_cast<std::uint64_t>(
                                            std::ceil(static_cast<double>(duration_) * rates[fps_]))
                                  : 0;
        ImGui::Text("%u x %u | %llu %s | %.3f s", width, height,
                    static_cast<unsigned long long>(frames), translate("export.frames").c_str(),
                    static_cast<double>(frames) / rates[fps_]);
        ImGui::BeginDisabled(!valid || !error_.empty());
        if (ImGui::Button(label("export.start").c_str())) {
            request.emplace();
            request->settings_.encoding_ = {
                    width,
                    height,
                    rates[fps_],
                    std::array{4000000U, 8000000U, 16000000U}.at(quality_),
                    codec_ == 0 ? media::VideoCodec::kH264 : media::VideoCodec::kMpeg4,
                    music_[0] != 0 || arranged_};
            request->settings_.frames_ = frames;
            request->settings_.gain_ = gain_;
            if (music_[0]) request->settings_.music_ = Path(music_.data());
            request->destination_ = Path(destination_.data());
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (busy) {
            const auto fraction = state.progress_.total_frames_
                                          ? static_cast<float>(state.progress_.completed_frames_) /
                                                    state.progress_.total_frames_
                                          : 0;
            ImGui::ProgressBar(fraction, {-1, 0});
            ImGui::BeginDisabled(state.state_ == exporting::JobState::kPublishing);
            if (ImGui::Button(label("export.cancel").c_str())) jobs_->Cancel();
            ImGui::EndDisabled();
        }
        static constexpr std::array kStates{
                "export.idle",     "export.preparing", "export.rendering", "export.publishing",
                "export.complete", "export.canceled",  "export.failed"};
        ImGui::TextWrapped("%s",
                           translate(kStates.at(static_cast<std::size_t>(state.state_))).c_str());
        if (state.state_ == exporting::JobState::kComplete) {
            ImGui::TextWrapped("%s", Utf8(state.output_).c_str());
            if (ImGui::Button(label("export.copy_path").c_str()))
                ImGui::SetClipboardText(Utf8(state.output_).c_str());
        }
        const auto error = error_.empty() ? state.error_ : error_;
        if (!error.empty()) ImGui::TextWrapped("%s", translate(error).c_str());
    }
    ImGui::End();
    return request;
}
}  // namespace rhythm::studio
