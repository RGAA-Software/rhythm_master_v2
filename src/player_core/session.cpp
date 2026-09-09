#include "rhythm/player/session.h"

#include <cmath>
#include <stdexcept>

#include "rhythm/prepared_assets/prepare.h"

namespace rhythm::player {
void Session::Load(std::string_view package_bytes) { LoadPrepared(PreparedPackage(package_bytes)); }
void Session::LoadPrepared(PreparedPackage package, double initial_seconds) {
    if (!std::isfinite(initial_seconds) || initial_seconds < 0 ||
        initial_seconds > double(std::uint64_t{1} << 52) / 1000000 ||
        (initial_seconds > 0 && !package.SupportsAnalyticSeek()))
        throw std::invalid_argument("player.unsupported_seek");
    auto resources = std::move(package.resources_);
    auto soundtrack = std::move(package.soundtrack_);
    Commit(package.Take(), std::move(resources), std::move(soundtrack));
    clock_.Seek(initial_seconds);
}
void Session::Open(const std::filesystem::path& path) {
    auto package = project::LoadPackage(path);
    auto resources = prepared_assets::Prepare(package.program_, package.assets_);
    auto soundtrack = prepared_assets::PrepareSoundtrack(package);
    Commit(std::move(package), std::move(resources), std::move(soundtrack));
}
void Session::Commit(project::RuntimePackage package,
                     std::shared_ptr<const prepared_assets::Resources> resources,
                     std::optional<media::SoundtrackSource> soundtrack) {
    package_ = std::move(package);
    resources_ = std::move(resources);
    soundtrack_ = std::move(soundtrack);
    videos_.Reset();
    clock_.SetPaused(false);
    Restart();
}
const std::string& Session::Title() const {
    static const std::string kEmpty;
    return package_ ? package_->title_ : kEmpty;
}
const parameters::ControlBank& Session::Controls() const {
    static const parameters::ControlBank kEmpty;
    return package_ ? package_->program_.controls_ : kEmpty;
}
render::Extent Session::Canvas() const {
    const auto canvas = package_ ? package_->program_.canvas_ : graph::Canvas{};
    return {static_cast<std::uint16_t>(canvas.width_), static_cast<std::uint16_t>(canvas.height_)};
}
void Session::Restart() {
    const bool paused = Paused();
    clock_ = {};
    clock_.SetPaused(paused);
    clock_generation_ = clock_.Generation();
    external_ = {};
    ReleaseGraphics();
}
const std::optional<parameters::ControlSequence>& Session::ControlSequence() const {
    static const std::optional<parameters::ControlSequence> kEmpty;
    return package_ ? package_->program_.control_sequence_ : kEmpty;
}
std::optional<parameters::BeatSettings> Session::BeatGrid() const {
    return package_ ? package_->program_.beat_grid_ : std::nullopt;
}
parameters::ControlValues Session::CurrentControls() const {
    return external_.controls_.empty()
                   ? parameters::EvaluateControls(Controls(), ControlSequence(), Seconds())
                   : external_.controls_;
}
void Session::ReleaseGraphics() {
    preparation_context_.reset();
    preparation_progress_ = {};
    preparation_started_ = false;
    frame_ = {};
    runtime_.Reset();
    extent_ = {};
    ++generation_;
}
runtime::FrameResult Session::Tick(double monotonic_seconds, bool suspended, render::Extent extent,
                                   render::Renderer& renderer,
                                   const runtime::ExternalInputs& inputs,
                                   const std::optional<runtime::PlaybackSample>& playback) {
    if (!runtime::ValidExternalInputs(inputs))
        throw std::invalid_argument("runtime.external_inputs");
    if (preparation_context_ && preparation_progress_.state_ != runtime::PreparationState::kReady)
        throw std::logic_error("player.preparation_pending");
    preparation_context_.reset();
    const auto previous_seconds = Seconds();
    clock_.Advance(monotonic_seconds, suspended, playback);
    const bool media_position_changed = playback && Seconds() != previous_seconds;
    const bool discontinuity = clock_generation_ != clock_.Generation();
    if (discontinuity) {
        clock_generation_ = clock_.Generation();
        ++generation_;
        frame_ = {};
    }
    if (!package_ || suspended || !extent.width_ || !extent.height_) return {};
    const auto controls = parameters::EvaluateControls(Controls(), ControlSequence(), Seconds(),
                                                       inputs.controls_);
    const auto controls_changed = controls != external_.controls_;
    if (!Paused() || controls_changed || media_position_changed || extent != extent_ ||
        !renderer.IsValid(frame_.final_) || !resources_->videos_.empty()) {
        runtime::FrameContext context{Seconds(), generation_, extent, false};
        context.resources_ = resources_->models_;
        context.images_ = resources_->images_;
        context.shaders_ = resources_->shaders_;
        context.surfaces_ = resources_->surfaces_;
        context.videos_ = videos_.Update(package_->program_, *resources_, Seconds(), generation_);
        context.external_ =
                Paused() && !discontinuity && !media_position_changed ? external_ : inputs;
        context.external_.controls_ = controls;
        context.advance_state_ = !Paused() && (!playback || Seconds() != previous_seconds);
        context.retained_textures_ = std::vector<graph::NodeId>{};
        frame_ = runtime_.EvaluateSafely(package_->program_, context, renderer);
        external_ = context.external_;
        extent_ = extent;
    }
    return frame_;
}
}  // namespace rhythm::player
