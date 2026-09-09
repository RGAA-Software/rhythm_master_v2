#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <optional>
#include <sstream>

#include "commands.h"
#include "control_bridge.h"
#include "host.h"
#include "package_imports.h"
#include "queued_imports.h"
#include "scene_bridge.h"
#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "music_playback.h"
#endif
#include "rhythm/player/scene_deck.h"
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
        player::SceneDeck deck;
        android_host::QueuedImports queued_imports(host.CacheDirectory());
        auto& scene_queue = queued_imports.Queue();
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        android_host::MusicPlayback music(host.CacheDirectory());
#endif
        auto render_quality = player::RenderQuality::kBalanced;
        deck.LoadPrepared(player::PreparedPackage(host.ReadAsset("signal_texture.rhythmpack")));
        const auto installed = host.DataDirectory() / "selected.rhythmpack";
        android_host::PackageImports imports(installed, host.CacheDirectory());
        if (std::filesystem::exists(installed)) {
            try {
                deck.Open(installed);
            } catch (const std::exception&) {
                SDL_Log("player stored package rejected; using builtin");
            }
        }
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        const auto apply_soundtrack = [&] {
            if (const auto track = deck.Current().Soundtrack())
                music.Open(*track);
            else
                music.Clear();
        };
        apply_soundtrack();
#endif
        android_host::PublishScene(deck.Current().Canvas(), deck.Current().Title());
        android_host::PublishControls(deck);
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
                deck.Tick(seconds, true, render_quality, *renderer);
                deck.ReleaseGraphics();
                renderer.reset();
            }
            if (host.Suspended()) {
                if (renderer) {
                    deck.Tick(seconds, true, render_quality, *renderer);
                    deck.ReleaseGraphics();
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
            if (commands.toggle_pause_) playback.paused_ = !deck.Current().Paused();
            if (commands.focus_pause_) playback.paused_ = true;
            if (commands.restart_) playback.seek_ = 0;
            if (commands.seek_seconds_) playback.seek_ = commands.seek_seconds_;
            if (playback.paused_) deck.SetPaused(*playback.paused_);
            if (playback.seek_) deck.Seek(*playback.seek_);
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
                    scene_queue.Clear();
                    const bool paused = deck.Current().Paused();
                    deck.LoadPrepared(std::move(*loaded->package_));
                    android_host::PublishControls(deck);
                    deck.SetPaused(paused);
                    android_host::PublishScene(deck.Current().Canvas(), deck.Current().Title());
#ifdef RHYTHM_HAS_LOCAL_MEDIA
                    // Visual-only effects keep the current music and its clock.
                    // A published work with a bound soundtrack replaces it.
                    if (const auto track = deck.Current().Soundtrack()) {
                        music.Open(*track);
                        music.Apply({paused, {}});
                    }
#endif
                    error.clear();
                } else {
                    error = "package_error";
                }
            }
            const auto scene_commands = android_host::TakeSceneCommands();
            android_host::ApplyControlCommands(deck);
            if (!scene_commands.path_.empty() &&
                !queued_imports.Request(scene_commands.path_, scene_commands.title_))
                error = "package_error";
            queued_imports.Pump(deck.CanPrepareNext());
            if (scene_commands.action_ == 1 && deck.CanPrepareNext() &&
                !scene_queue.Items().empty() &&
                scene_queue.Items().front().id_ == scene_commands.id_) {
                deck.RequestNextScene(scene_commands.id_, scene_commands.duration_,
                                      scene_commands.mode_);
            }
            if (scene_commands.action_ == 2) scene_queue.Remove(scene_commands.id_);
            if (scene_commands.action_ == 3) scene_queue.Clear();
            if (scene_commands.action_ == 4) scene_queue.Retry();
            if (scene_commands.action_ == 5) deck.CancelTransition();
            const auto size = host.Size();
            if (!size.width_ || !size.height_) {
                SDL_Delay(16);
                continue;
            }
            renderer->BeginFrame();
#ifdef RHYTHM_HAS_LOCAL_MEDIA
            auto music_frame = music.Frame();
            const auto frame = deck.Tick(seconds, false, render_quality, *renderer,
                                         music_frame.inputs_, music_frame.playback_, scene_queue);
            if (music_frame.failed_) error = "audio_error";
            android_host::PublishPlayback(
                    music_frame.playback_ ? music_frame.playback_->seconds_
                                          : deck.Current().Seconds(),
                    music_frame.playback_ ? music_frame.playback_->duration_ : std::nullopt,
                    music.Loop());
#else
            runtime::ExternalInputs inputs;
            const auto frame =
                    deck.Tick(seconds, false, render_quality, *renderer, inputs, {}, scene_queue);
#endif
            if (frame.switched_) {
                android_host::PublishControls(deck);
                android_host::PublishScene(deck.Current().Canvas(), deck.Current().Title());
#ifdef RHYTHM_HAS_LOCAL_MEDIA
                if (const auto track = deck.Current().Soundtrack()) {
                    music.Open(*track);
                    music.Apply({deck.Current().Paused(), frame.entry_seconds_});
                    if (const auto sample = music.Frame().playback_) deck.AdoptMedia(*sample);
                }
#endif
            }
            const auto& output = frame.output_;
            if (frames % 12 == 0) android_host::PublishSceneQueue(scene_queue, deck);
            renderer->Submit({}, Present(output.final_, size, deck.Current().Canvas()), 0x111822ff);
            android_host::PublishControlFrame(deck);
            renderer->EndFrame();
            ++frames;
            if (frames % 30 == 0) {
                std::ostringstream status;
                status << (deck.Current().Paused() ? "paused" : "playing") << " "
                       << deck.Current().Seconds() << " s | frames=" << frames
                       << " devices=" << devices << " " << error;
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
                        static_cast<unsigned long long>(frames), deck.Current().Seconds(),
                        static_cast<unsigned long long>(devices));
        }
        deck.ReleaseGraphics();
        renderer.reset();
    } catch (const std::exception& error) {
        android_host::PublishStatus(std::string("error: ") + error.what());
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "player failed: %s", error.what());
        return 1;
    }
    return 0;
}
