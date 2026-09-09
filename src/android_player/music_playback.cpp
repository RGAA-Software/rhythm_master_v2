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
    if (snapshot.source_generation_ >= generation_) {
        retired_.reset();
        if (embedded_ || streamed_.Valid() || arrangement_ || !selected_) active_.reset();
    }
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
        embedded_.reset();
        streamed_ = {};
        arrangement_.reset();
        selected_ = true;
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
void MusicPlayback::Open(const media::SoundtrackSource& source) {
    if (!media::ValidSoundtrackSource(source))
        throw std::invalid_argument("project.soundtrack_invalid");
    file_.LoadSoundtrack(source);
    SelectSoundtrack(source);
    generation_ = file_.Snapshot().generation_;
    if (suspended_) {
        resume_ = true;
        file_.Pause(true);
    }
}
void MusicPlayback::SelectSoundtrack(const media::SoundtrackSource& source) {
    embedded_ = source.bytes_;
    streamed_ = source.file_bytes_;
    arrangement_ = source.arrangement_;
    selected_ = true;
    loop_ = source.binding_.loop_;
}
std::uint64_t MusicPlayback::BeginSoundtrackTransition(const media::SoundtrackSource& source,
                                                       double duration, bool paused) {
    file_.Pause(suspended_ || paused);
    const auto id = file_.BeginTransition(source, duration);
    if (suspended_) resume_ = !paused;
    selected_ = true;
    transition_id_ = id;
    return id;
}
bool MusicPlayback::CancelSoundtrackTransition(std::uint64_t id) {
    return file_.CancelTransition(id);
}
void MusicPlayback::AdoptSoundtrack(const media::SoundtrackSource& source, std::uint64_t id) {
    const auto snapshot = file_.Snapshot();
    if (!id || id != transition_id_ || snapshot.transition_.id_ != id ||
        snapshot.transition_.state_ != audio::AudioTransitionState::kCompleted ||
        !media::ValidSoundtrackSource(source))
        throw std::logic_error("audio.transition_not_committed");
    SelectSoundtrack(source);
    // A continuous handoff changes analysis generations without a load epoch.
    // Consumed confirmation already released the old decoder's file lease.
    generation_ = snapshot.source_generation_;
    transition_id_ = 0;
    Collect();
}
void MusicPlayback::Clear() {
    file_.Stop();
    embedded_.reset();
    streamed_ = {};
    arrangement_.reset();
    selected_ = false;
    resume_ = false;
    generation_ = file_.Snapshot().generation_;
}
void MusicPlayback::Apply(const runtime::PlaybackCommand& command) {
    if (!selected_) return;
    const auto state = file_.Snapshot().state_;
    if (command.seek_)
        file_.Seek(*command.seek_);
    else if (state == audio::PlaybackState::kEnded && command.paused_ == false)
        file_.Seek(0);
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
    Collect();
    MusicFrame frame;
    frame.audio_ = file_.Snapshot();
    if (!selected_) return frame;
    const auto& snapshot = frame.audio_;
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
