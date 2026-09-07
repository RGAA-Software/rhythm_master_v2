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
    Commit(package.Take(), std::move(resources));
    playback_offset_seconds_ = initial_seconds;
    seconds_ = initial_seconds;
}
void Session::Open(const std::filesystem::path& path) {
    auto package = project::LoadPackage(path);
    auto resources = prepared_assets::Prepare(package.program_, package.assets_);
    Commit(std::move(package), std::move(resources));
}
void Session::Commit(project::RuntimePackage package,
                     std::shared_ptr<const prepared_assets::Resources> resources) {
    package_ = std::move(package);
    resources_ = std::move(resources);
    videos_.Reset();
    paused_ = false;
    Restart();
}
const std::string& Session::Title() const {
    static const std::string kEmpty;
    return package_ ? package_->title_ : kEmpty;
}
render::Extent Session::Canvas() const {
    const auto canvas = package_ ? package_->program_.canvas_ : graph::Canvas{};
    return {static_cast<std::uint16_t>(canvas.width_), static_cast<std::uint16_t>(canvas.height_)};
}
void Session::Restart() {
    clock_ = {};
    seconds_ = 0;
    playback_offset_seconds_ = 0;
    external_ = {};
    ReleaseGraphics();
}
void Session::ReleaseGraphics() {
    frame_ = {};
    runtime_.Reset();
    extent_ = {};
    ++generation_;
}
runtime::FrameResult Session::Tick(double monotonic_seconds, bool suspended, render::Extent extent,
                                   render::Renderer& renderer,
                                   const runtime::ExternalInputs& inputs) {
    if (!runtime::ValidExternalInputs(inputs))
        throw std::invalid_argument("runtime.external_inputs");
    seconds_ = playback_offset_seconds_ + clock_.Advance(monotonic_seconds, suspended || paused_);
    if (!package_ || suspended || !extent.width_ || !extent.height_) return {};
    if (!paused_ || extent != extent_ || !renderer.IsValid(frame_.final_) ||
        !resources_->videos_.empty()) {
        runtime::FrameContext context{seconds_, generation_, extent, false};
        context.resources_ = resources_->models_;
        context.images_ = resources_->images_;
        context.videos_ = videos_.Update(package_->program_, *resources_, seconds_, generation_);
        context.external_ = paused_ ? external_ : inputs;
        frame_ = runtime_.Evaluate(package_->program_, context, renderer);
        external_ = context.external_;
        extent_ = extent;
    }
    return frame_;
}
}  // namespace rhythm::player
