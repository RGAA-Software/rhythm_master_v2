#include "rhythm/audio_ui/audio_panel.h"

#include <imgui.h>

#include <algorithm>

namespace rhythm::audio_ui {
void AudioPanel::SetSuspended(bool suspended) {
    if (suspended_ == suspended) return;
    suspended_ = suspended;
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    if (suspended) {
        const auto state = file_.Snapshot().state_;
        resume_file_ =
                state == audio::PlaybackState::kPlaying || state == audio::PlaybackState::kLoading;
        if (resume_file_) file_.Pause(true);
    } else if (resume_file_) {
        resume_file_ = false;
        file_.Pause(false);
    }
#endif
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
std::optional<audio::Features> AudioPanel::Snapshot() const { return Frame().features_; }
AudioInputFrame AudioPanel::Frame() const {
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    if (media_selected_) {
        const auto file = file_.Snapshot();
        runtime::PlaybackSample playback{
                file.position_seconds_, file.generation_,
                file.paused_ || file.state_ != audio::PlaybackState::kPlaying,
                file.duration_seconds_};
        if (playback.duration_ && *playback.duration_ <= 0) playback.duration_.reset();
        return {file.features_, playback};
    }
#endif
    const auto snapshot = capture_.Snapshot();
    if (snapshot.state_ != audio::CaptureState::kRunning || !snapshot.features_.valid_) return {};
    return {snapshot.features_, {}};
}
void AudioPanel::ApplyPlayback(const runtime::PlaybackCommand& command) {
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    if (!media_selected_) return;
    const auto state = file_.Snapshot().state_;
    if ((state == audio::PlaybackState::kStopped || state == audio::PlaybackState::kEnded) &&
        (command.seek_ || command.paused_ == false)) {
        if (streamed_.Valid())
            file_.Load(streamed_);
        else if (embedded_)
            file_.Load(embedded_);
        else
            file_.Load(loaded_file_);
        file_.Pause(command.paused_.value_or(true));
    }
    if (command.seek_) file_.Seek(*command.seek_);
    if (command.paused_) file_.Pause(*command.paused_);
#else
    (void)command;
#endif
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
        else {
#ifdef RHYTHM_HAS_LOCAL_MEDIA
            file_.Stop();
            media_selected_ = false;
#endif
            capture_.Start();
        }
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
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    DrawFile(text);
#endif
}

#ifdef RHYTHM_HAS_LOCAL_MEDIA
std::optional<std::filesystem::path> AudioPanel::SelectedFile() const {
    if (media_selected_ && !loaded_file_.empty()) return loaded_file_;
    return std::nullopt;
}
void AudioPanel::LoadFile(const std::filesystem::path& path) {
    capture_.Stop();
    embedded_.reset();
    streamed_ = {};
    loaded_file_ = path;
    media_selected_ = true;
    const auto utf8 = path.u8string();
    if (utf8.size() < file_path_.size()) {
        file_path_.fill('\0');
        std::transform(utf8.begin(), utf8.end(), file_path_.begin(),
                       [](char8_t value) { return static_cast<char>(value); });
    }
    file_.Load(path);
    if (suspended_) {
        resume_file_ = true;
        file_.Pause(true);
    }
}
void AudioPanel::LoadSoundtrack(const media::SoundtrackSource& source) {
    if (bool(source.bytes_) == source.file_bytes_.Valid())
        throw std::invalid_argument("project.soundtrack_invalid");
    if (source.file_bytes_.Valid())
        file_.Load(source.file_bytes_);
    else
        file_.Load(source.bytes_);
    capture_.Stop();
    embedded_ = source.bytes_;
    streamed_ = source.file_bytes_;
    loaded_file_.clear();
    file_path_.fill(0);
    media_selected_ = true;
    SetVolume(source.binding_.gain_);
    SetLoop(source.binding_.loop_);
    if (suspended_) {
        resume_file_ = true;
        file_.Pause(true);
    }
}
void AudioPanel::ClearFile() {
    file_.Stop();
    embedded_.reset();
    streamed_ = {};
    loaded_file_.clear();
    file_path_.fill(0);
    media_selected_ = false;
    resume_file_ = false;
}
void AudioPanel::SetLoop(bool loop) {
    loop_ = loop;
    file_.SetLoop(loop);
}
void AudioPanel::SetVolume(float volume) {
    file_.SetVolume(volume);
    volume_ = volume;
}
void AudioPanel::SetDemoFile(std::filesystem::path path) { demo_file_ = std::move(path); }
void AudioPanel::DrawFile(const std::map<std::string, std::string>& text) {
    ImGui::Separator();
    if (!demo_file_.empty() && ImGui::Button((text.at("audio.demo") + "###audio.demo").c_str())) {
        LoadFile(demo_file_);
        loop_ = true;
        file_.SetLoop(true);
    }
    ImGui::InputTextWithHint("###audio.file_path", text.at("audio.file_path").c_str(),
                             file_path_.data(), file_path_.size());
    if (ImGui::Button((text.at("audio.file_open") + "###audio.file_open").c_str())) {
        const std::string path(file_path_.data());
        LoadFile(std::filesystem::path(std::u8string(path.begin(), path.end())));
    }
    const auto snapshot = file_.Snapshot();
    const bool paused = snapshot.state_ == audio::PlaybackState::kPaused;
    const bool active = snapshot.state_ == audio::PlaybackState::kPlaying || paused;
    if (active) {
        ImGui::SameLine();
        if (ImGui::Button((text.at(paused ? "audio.file_resume" : "audio.file_pause") +
                           "###audio.file_pause")
                                  .c_str()))
            file_.Pause(!paused);
    }
    if (snapshot.state_ != audio::PlaybackState::kStopped) {
        ImGui::SameLine();
        if (ImGui::Button((text.at("audio.stop") + "###audio.file_stop").c_str())) file_.Stop();
    }
    if (ImGui::SliderFloat((text.at("audio.volume") + "###audio.volume").c_str(), &volume_, 0, 1,
                           "%.2f"))
        file_.SetVolume(volume_);
    if (ImGui::Checkbox((text.at("audio.loop") + "###audio.loop").c_str(), &loop_))
        file_.SetLoop(loop_);
    if (snapshot.duration_seconds_ && *snapshot.duration_seconds_ > 0) {
        if (!seeking_) seek_seconds_ = static_cast<float>(snapshot.position_seconds_);
        ImGui::SliderFloat((text.at("audio.position") + "###audio.position").c_str(),
                           &seek_seconds_, 0, static_cast<float>(*snapshot.duration_seconds_),
                           "%.2f s");
        if (ImGui::IsItemActivated()) seeking_ = true;
        if (ImGui::IsItemDeactivated()) {
            if (ImGui::IsItemDeactivatedAfterEdit()) file_.Seek(seek_seconds_);
            seeking_ = false;
        }
    }
    if (snapshot.state_ == audio::PlaybackState::kFailed)
        ImGui::TextWrapped("%s", text.at("audio.file_failed").c_str());
    if (snapshot.state_ == audio::PlaybackState::kLoading)
        ImGui::TextUnformatted(text.at("audio.file_loading").c_str());
    if (snapshot.state_ == audio::PlaybackState::kEnded)
        ImGui::TextUnformatted(text.at("audio.file_ended").c_str());
    if (snapshot.features_ && snapshot.features_->valid_)
        ImGui::PlotHistogram("###audio.file_spectrum", snapshot.features_->mono_bands_.data(),
                             static_cast<int>(audio::kBandCount), 0, nullptr, 0, 1, ImVec2(0, 64));
}
#endif
}  // namespace rhythm::audio_ui
