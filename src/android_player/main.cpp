#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <optional>
#include <sstream>

#include "commands.h"
#include "host.h"
#include "package_imports.h"
#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "music_playback.h"
#endif
#include "rhythm/player/session.h"
#include "rhythm/render/layout.h"

namespace {
rhythm::render::DrawList Present(rhythm::render::TextureHandle texture, rhythm::render::Extent size,
                                 rhythm::render::Extent canvas) {
    const float width = size.width_;
    const float height = size.height_;
    rhythm::render::DrawList list;
    list.width_ = width;
    list.height_ = height;
    if (!texture.device_) return list;
    const auto fit = rhythm::render::AspectFit(canvas, {0, 0, width, height});
    list.vertices_ = {{fit.x_, fit.y_, 0, 0},
                      {fit.x_ + fit.width_, fit.y_, 1, 0},
                      {fit.x_ + fit.width_, fit.y_ + fit.height_, 1, 1},
                      {fit.x_, fit.y_ + fit.height_, 0, 1}};
    list.indices_ = {0, 1, 2, 0, 2, 3};
    list.commands_ = {{texture, 0, 6, {0, 0, width, height}}};
    return list;
}
}  // namespace
// SDL mandates this process-entry signature; arguments are not retained.
int main(int, char**) {
    using namespace rhythm;
    try {
        platform::Host host;
        std::optional<render::Renderer> renderer;
        player::Session session;
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        android_host::MusicPlayback music(host.CacheDirectory());
#endif
        auto render_quality = player::RenderQuality::kBalanced;
        session.Load(host.ReadAsset("signal_texture.rhythmpack"));
        const auto installed = host.DataDirectory() / "selected.rhythmpack";
        android_host::PackageImports imports(installed, host.CacheDirectory());
        if (std::filesystem::exists(installed)) {
            try {
                session.Open(installed);
            } catch (const std::exception&) {
                SDL_Log("player stored package rejected; using builtin");
            }
        }
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        const auto apply_soundtrack = [&] {
            if (const auto track = session.Soundtrack())
                music.Open(*track);
            else
                music.Clear();
        };
        apply_soundtrack();
#endif
        android_host::PublishScene(session.Canvas(), session.Title());
        std::uint64_t frames = 0;
        std::uint64_t devices = 0;
        std::uint64_t surface_generation = 0;
        std::string error;
        while (host.Poll()) {
            const double seconds = SDL_GetTicksNS() / 1.0e9;
#ifdef RHYTHM_HAS_LOCAL_MEDIA
            if (host.Suspended()) music.SetSuspended(true);
#endif
            if (renderer && surface_generation != host.SurfaceGeneration()) {
                session.Tick(seconds, true, {}, *renderer);
                session.ReleaseGraphics();
                renderer.reset();
            }
            if (host.Suspended()) {
                if (renderer) {
                    session.Tick(seconds, true, {}, *renderer);
                    session.ReleaseGraphics();
                    renderer.reset();
                    SDL_Log("player suspended frames=%llu",
                            static_cast<unsigned long long>(frames));
                }
                SDL_Delay(16);
                continue;
            }
            if (!renderer) {
                surface_generation = host.SurfaceGeneration();
                renderer.emplace(host.CreateRenderer());
                ++devices;
                SDL_Log("player device created generation=%llu",
                        static_cast<unsigned long long>(devices));
            }
            const auto commands = android_host::TakeCommands();
            runtime::PlaybackCommand playback;
            if (commands.toggle_pause_) playback.paused_ = !session.Paused();
            if (commands.focus_pause_) playback.paused_ = true;
            if (commands.restart_) playback.seek_ = 0;
            if (commands.seek_seconds_) playback.seek_ = commands.seek_seconds_;
            if (playback.paused_) session.SetPaused(*playback.paused_);
            if (playback.seek_) session.Seek(*playback.seek_);
#ifdef RHYTHM_HAS_LOCAL_MEDIA
            if (!commands.music_path_.empty()) {
                if (music.Open(commands.music_path_))
                    error.clear();
                else
                    error = "audio_error";
            }
            music.Apply(playback);
            music.SetSuspended(false);
            if (commands.music_loop_) music.SetLoop(*commands.music_loop_);
#endif
            if (commands.render_quality_) render_quality = *commands.render_quality_;
            if (!commands.package_path_.empty()) {
                if (!imports.Request(commands.package_path_)) error = "package_error";
            }
            if (auto loaded = imports.Take()) {
                if (loaded->package_) {
                    const bool paused = session.Paused();
                    session.LoadPrepared(std::move(*loaded->package_));
                    session.SetPaused(paused);
                    android_host::PublishScene(session.Canvas(), session.Title());
#ifdef RHYTHM_HAS_LOCAL_MEDIA
                    // Visual-only effects keep the current music and its clock.
                    // A published work with a bound soundtrack replaces it.
                    if (const auto track = session.Soundtrack()) music.Open(*track);
#endif
                    error.clear();
                } else {
                    error = "package_error";
                }
            }
            const auto size = host.Size();
            if (!size.width_ || !size.height_) {
                SDL_Delay(16);
                continue;
            }
            renderer->BeginFrame();
            const auto extent = player::PlaybackExtent(session.Canvas(), render_quality);
#ifdef RHYTHM_HAS_LOCAL_MEDIA
            const auto music_frame = music.Frame();
            const auto output = session.Tick(seconds, false, extent, *renderer, music_frame.inputs_,
                                             music_frame.playback_);
            if (music_frame.failed_) error = "audio_error";
            android_host::PublishPlayback(
                    session.Seconds(),
                    music_frame.playback_ ? music_frame.playback_->duration_ : std::nullopt);
#else
            const auto output = session.Tick(seconds, false, extent, *renderer);
#endif
            renderer->Submit({}, Present(output.final_, size, session.Canvas()), 0x111822ff);
            renderer->EndFrame();
            ++frames;
            if (frames % 30 == 0) {
                std::ostringstream status;
                status << (session.Paused() ? "paused" : "playing") << " " << session.Seconds()
                       << " s | frames=" << frames << " devices=" << devices << " " << error;
                if (imports.Busy()) status << " loading";
                if (output.budget_) status << " render.resource_budget";
#ifdef RHYTHM_HAS_LOCAL_MEDIA
                if (music_frame.inputs_.audio_)
                    status << " rms=" << music_frame.inputs_.audio_->rms_;
#endif
                android_host::PublishStatus(status.str());
            }
            if (frames % 300 == 0)
                SDL_Log("player frames=%llu time=%.3f devices=%llu",
                        static_cast<unsigned long long>(frames), session.Seconds(),
                        static_cast<unsigned long long>(devices));
        }
        session.ReleaseGraphics();
        renderer.reset();
    } catch (const std::exception& error) {
        android_host::PublishStatus(std::string("error: ") + error.what());
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "player failed: %s", error.what());
        return 1;
    }
    return 0;
}
