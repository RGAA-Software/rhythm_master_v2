#include "soundtrack_panel.h"

#include <imgui.h>

#include <algorithm>
#include <set>

#include "rhythm/media/audio_mixer.h"

namespace rhythm::studio {
namespace {
std::filesystem::path BlobPath(const std::filesystem::path& assets, const assets::AssetId& id) {
    if (!rhythm::assets::ValidId(id)) throw std::invalid_argument("asset.id");
    // The project loader/importer validates the immutable store and this ID.
    // Resolving its deterministic name performs no disk access on the UI thread.
    return assets / "sha256" / id.sha256_.substr(0, 2) / id.sha256_;
}
void LoadBinding(const media::Soundtrack& binding, const std::filesystem::path& assets,
                 audio_ui::AudioPanel& audio) {
    if (binding.clips_.empty()) {
        audio.LoadFile(BlobPath(assets, binding.asset_));
    } else {
        media::AudioArrangementFiles files{media::AudioArrangement(binding.clips_), {}};
        std::set<std::string> loaded;
        for (const auto& clip : binding.clips_)
            if (loaded.insert(clip.asset_.sha256_).second)
                files.files_.push_back({clip.asset_, BlobPath(assets, clip.asset_)});
        audio.LoadArrangement(std::move(files));
    }
    audio.SetVolume(binding.gain_);
    audio.SetLoop(binding.loop_);
}
}  // namespace
std::optional<editor::Snapshot> SoundtrackPanel::Take(const editor::Snapshot& current,
                                                      bool pending_edit,
                                                      const audio_ui::AudioPanel& audio) {
    if (!importer_) return {};
    auto result = importer_->Take();
    if (!result) return {};
    if (!result->snapshot_) {
        status_ = result->error_;
        return {};
    }
    if (pending_edit || audio.SelectedFile() != import_source_ || audio.Volume() != import_gain_ ||
        audio.Loop() != import_loop_ || current.document_.id_ != result->document_ ||
        current.document_.revision_ != result->revision_) {
        status_ = "load_conflict";
        return {};
    }
    status_ = "music.bound";
    return std::move(result->snapshot_);
}
void SoundtrackPanel::Sync(const editor::Snapshot& snapshot, const std::filesystem::path& assets,
                           audio_ui::AudioPanel& audio,
                           std::span<const assets::AssetId> unavailable) {
    const bool blocked =
            snapshot.soundtrack_ &&
            std::any_of(unavailable.begin(), unavailable.end(), [&](const auto& id) {
                return snapshot.soundtrack_->asset_ == id ||
                       std::any_of(snapshot.soundtrack_->clips_.begin(),
                                   snapshot.soundtrack_->clips_.end(),
                                   [&](const auto& clip) { return clip.asset_ == id; });
            });
    if (blocked) {
        if (!blocked_) audio.ClearFile();
        blocked_ = true;
        status_ = "asset.music_unavailable";
        return;
    }
    if (blocked_) {
        blocked_ = false;
        active_.reset();
        document_.clear();
    }
    if (document_ == snapshot.document_.id_ && active_ == snapshot.soundtrack_) return;
    if (snapshot.soundtrack_) {
        const auto& next = *snapshot.soundtrack_;
        if (document_ != snapshot.document_.id_ || !active_ || active_->asset_ != next.asset_ ||
            active_->clips_ != next.clips_) {
            const auto playback = audio.Frame().playback_;
            const bool preserve = document_ == snapshot.document_.id_ && active_ &&
                                  active_->asset_ == next.asset_;
            LoadBinding(next, assets, audio);
            if (preserve && playback) audio.ApplyPlayback({playback->paused_, playback->seconds_});
        }
        audio.SetVolume(next.gain_);
        audio.SetLoop(next.loop_);
    } else if (active_) {
        audio.ClearFile();
    }
    document_ = snapshot.document_.id_;
    active_ = snapshot.soundtrack_;
    status_ = active_ ? "music.bound" : "";
}
std::optional<SoundtrackAction> SoundtrackPanel::Draw(
        const editor::Snapshot& snapshot, bool selected,
        const std::map<std::string, std::string>& text) {
    std::optional<SoundtrackAction> action;
    if (!ImGui::CollapsingHeader((text.at("music.work") + "###music.work").c_str(),
                                 ImGuiTreeNodeFlags_DefaultOpen))
        return {};
    ImGui::TextWrapped("%s", text.at("music.binding_help").c_str());
    if (snapshot.soundtrack_)
        ImGui::TextWrapped("%s", snapshot.soundtrack_->title_.c_str());
    else
        ImGui::TextUnformatted(text.at("music.unbound").c_str());
    ImGui::BeginDisabled(Busy());
    ImGui::BeginDisabled(!selected);
    if (ImGui::Button((text.at("music.bind") + "###music.bind").c_str()))
        action = SoundtrackAction::kBind;
    ImGui::SameLine();
    if (ImGui::Button((text.at("music.append") + "###music.append").c_str()))
        action = SoundtrackAction::kAppend;
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!snapshot.soundtrack_);
    ImGui::BeginDisabled(blocked_);
    if (ImGui::Button((text.at("music.load") + "###music.load").c_str()) && !blocked_)
        action = SoundtrackAction::kLoad;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button((text.at("music.clear") + "###music.clear").c_str()))
        action = SoundtrackAction::kClear;
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (Busy() && ImGui::Button((text.at("asset.cancel") + "###music.cancel").c_str()))
        importer_->Cancel();
    if (!status_.empty())
        ImGui::TextWrapped(
                "%s", text.at(text.contains(status_) ? status_ : "music.binding_failed").c_str());
    return action;
}
std::optional<editor::Snapshot> SoundtrackPanel::Start(SoundtrackAction action,
                                                       const editor::Snapshot& snapshot,
                                                       const std::filesystem::path& assets,
                                                       audio_ui::AudioPanel& audio) {
    try {
        if (Busy()) return {};
        if (action == SoundtrackAction::kClear) {
            status_ = "music.unbound";
            return content::UnbindSoundtrack(snapshot);
        }
        if (action == SoundtrackAction::kLoad) {
            if (blocked_) return {};
            if (snapshot.soundtrack_) {
                LoadBinding(*snapshot.soundtrack_, assets, audio);
            }
            return {};
        }
        const auto source = audio.SelectedFile();
        if (!source) return {};
        if (action == SoundtrackAction::kBind && snapshot.soundtrack_ &&
            snapshot.soundtrack_->clips_.empty() &&
            *source == BlobPath(assets, snapshot.soundtrack_->asset_)) {
            auto next = snapshot;
            next.soundtrack_->gain_ = audio.Volume();
            next.soundtrack_->loop_ = audio.Loop();
            status_ = "music.bound";
            return next;
        }
        if (!importer_) importer_.emplace();
        import_source_ = source;
        import_gain_ = audio.Volume();
        import_loop_ = audio.Loop();
        status_ = importer_->Start(snapshot, assets, *source, audio.Volume(), audio.Loop(),
                                   action == SoundtrackAction::kAppend)
                          ? "music.binding"
                          : "music.binding_failed";
    } catch (const std::exception&) {
        status_ = "music.binding_failed";
    }
    return {};
}
}  // namespace rhythm::studio
