#include "music_playback.h"

#include <utility>

namespace rhythm::android_host {
MusicPlayback::FileLease::~FileLease() {
    if (!path_.empty()) {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
}
MusicPlayback::FileLease::FileLease(FileLease&& other) noexcept
    : path_(std::exchange(other.path_, {})) {}
MusicPlayback::MusicPlayback(std::filesystem::path cache)
    : cache_(std::filesystem::canonical(cache)) {}
void MusicPlayback::Collect() {
    const auto snapshot = file_.Snapshot();
    if (snapshot.generation_ >= generation_ && snapshot.state_ != audio::PlaybackState::kLoading)
        retired_.reset();
}
bool MusicPlayback::Open(std::filesystem::path path) {
    try {
        if (std::filesystem::symlink_status(path).type() != std::filesystem::file_type::regular ||
            std::filesystem::canonical(path.parent_path()) != cache_ ||
            !path.filename().string().starts_with("music-") || path.extension() != ".media")
            return false;
        const auto owned = cache_ / path.filename();
        if ((active_ && active_->path_ == owned) || (retired_ && retired_->path_ == owned))
            return false;
        FileLease incoming(owned);
        if (std::filesystem::file_size(owned) > 64 * 1024 * 1024) return false;
        Collect();
        if (retired_) return false;
        if (active_) retired_.emplace(std::move(*active_));
        active_.emplace(std::move(incoming));
        file_.Load(active_->path_);
        generation_ = file_.Snapshot().generation_;
        if (suspended_) {
            resume_ = true;
            file_.Pause(true);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
void MusicPlayback::Apply(const runtime::PlaybackCommand& command) {
    if (!active_) return;
    const auto state = file_.Snapshot().state_;
    if (state == audio::PlaybackState::kEnded && (command.seek_ || command.paused_ == false)) {
        file_.Load(active_->path_);
        file_.Pause(command.paused_.value_or(true));
    }
    if (command.seek_) file_.Seek(*command.seek_);
    if (command.paused_) {
        if (suspended_) resume_ = !*command.paused_;
        file_.Pause(suspended_ || *command.paused_);
    }
}
void MusicPlayback::SetSuspended(bool suspended) {
    if (suspended_ == suspended) return;
    suspended_ = suspended;
    if (suspended) {
        const auto snapshot = file_.Snapshot();
        resume_ = !snapshot.paused_ && (snapshot.state_ == audio::PlaybackState::kPlaying ||
                                        snapshot.state_ == audio::PlaybackState::kLoading);
        if (resume_) file_.Pause(true);
    } else if (resume_) {
        resume_ = false;
        file_.Pause(false);
    }
}
MusicFrame MusicPlayback::Frame() {
    if (!active_) return {};
    Collect();
    const auto snapshot = file_.Snapshot();
    MusicFrame frame;
    frame.inputs_.audio_ = snapshot.features_;
    frame.playback_ = runtime::PlaybackSample{
            snapshot.position_seconds_, snapshot.generation_,
            snapshot.paused_ || snapshot.state_ != audio::PlaybackState::kPlaying,
            snapshot.duration_seconds_};
    if (frame.playback_->duration_ && *frame.playback_->duration_ <= 0)
        frame.playback_->duration_.reset();
    frame.failed_ = snapshot.state_ == audio::PlaybackState::kFailed;
    return frame;
}
}  // namespace rhythm::android_host
