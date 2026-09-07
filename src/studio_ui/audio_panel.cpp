#include "audio_panel.h"

#include <imgui.h>

namespace rhythm::studio {
void AudioPanel::SetSuspended(bool suspended) {
    if (suspended_ == suspended) return;
    suspended_ = suspended;
    if (suspended) {
        const auto state = capture_.Snapshot().state_;
        resume_capture_ =
                state == audio::CaptureState::kStarting || state == audio::CaptureState::kRunning;
        capture_.Stop();
    } else if (resume_capture_) {
        resume_capture_ = false;
        capture_.Start();
    }
}
std::optional<audio::Features> AudioPanel::Snapshot() const {
    const auto snapshot = capture_.Snapshot();
    if (snapshot.state_ != audio::CaptureState::kRunning || !snapshot.features_.valid_) return {};
    return snapshot.features_;
}
void AudioPanel::Draw(const std::map<std::string, std::string>& text) {
    if (!ImGui::CollapsingHeader((text.at("audio.input") + "###audio.input").c_str(),
                                 ImGuiTreeNodeFlags_DefaultOpen))
        return;
    ImGui::TextWrapped("%s", text.at("audio.help").c_str());
    const auto snapshot = capture_.Snapshot();
    const bool active = snapshot.state_ == audio::CaptureState::kStarting ||
                        snapshot.state_ == audio::CaptureState::kRunning;
    if (ImGui::Button((text.at(active ? "audio.stop" : "audio.start_system") + "###audio.toggle")
                              .c_str())) {
        if (active)
            capture_.Stop();
        else
            capture_.Start();
    }
    const auto key = snapshot.state_ == audio::CaptureState::kRunning    ? "audio.running"
                     : snapshot.state_ == audio::CaptureState::kStarting ? "audio.starting"
                     : snapshot.state_ == audio::CaptureState::kFailed   ? "audio.failed"
                                                                         : "audio.idle";
    ImGui::TextWrapped("%s", text.at(key).c_str());
    if (snapshot.features_.valid_) {
        ImGui::ProgressBar(snapshot.features_.loudness_, ImVec2(-1, 0),
                           text.at("audio.loudness").c_str());
        ImGui::PlotHistogram((text.at("audio.spectrum") + "###audio.spectrum").c_str(),
                             snapshot.features_.mono_bands_.data(),
                             static_cast<int>(audio::kBandCount), 0, nullptr, 0, 1, ImVec2(0, 64));
        ImGui::Text("%s: %.1f", text.at("audio.bpm").c_str(), snapshot.features_.bpm_);
    } else if (active)
        ImGui::TextUnformatted(text.at("audio.no_signal").c_str());
}
}  // namespace rhythm::studio
