#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <optional>
#include <sstream>

#include "commands.h"
#include "control_bridge.h"
#include "host.h"
#include "package_imports.h"
#include "program_bridge.h"
#include "queued_imports.h"
#include "scene_bridge.h"
#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "music_playback.h"
#include "rhythm/player_audio/scene_audio_bridge.h"
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
        const auto program_directory = host.DataDirectory() / "performance";
        const auto program_cache = std::filesystem::canonical(host.CacheDirectory());
        const auto builtin_works =
                android_host::ReadBuiltinWorks(host.ReadAsset("effects/catalog.json"));
        std::vector<performance::WorkReference> catalog;
        for (const auto& work : builtin_works) catalog.push_back(work.reference_);
        // Declared before the program: its worker joins before removing an import copy.
        std::optional<android_host::ImportFile> program_import;
        player::PerformanceProgram program(
                program_directory, std::move(catalog),
                android_host::BuiltinReader(program_directory, builtin_works));
        program.Load();
        std::string program_error;
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        android_host::MusicPlayback music(host.CacheDirectory());
        player_audio::SceneAudioBridge scene_audio;
        std::pair<std::uint64_t, audio::AudioTransitionState> audio_transition_status{};
        deck.EnableAudioTransitions(true);
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
                    } else if (const auto sample = music.Frame().playback_)
                        deck.AnchorMedia(*sample);
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
            program.Pump();
            if (!program.Busy()) program_import.reset();
            if (const auto command = android_host::TakeProgramCommand(); command.action_) {
                program_error = android_host::ApplyProgramCommand(command, program, builtin_works,
                                                                  program_cache, program_import)
                                        ? ""
                                        : "performance.request_rejected";
            }
            if (deck.CanPrepareNext()) {
                if (auto resolved = program.TakeResolved())
                    scene_queue.ReplacePerformance(*resolved);
            }
            android_host::PublishProgram(program, program_error);
            if (scene_commands.action_ == 1 && deck.CanPrepareNext() &&
                !scene_queue.Items().empty() &&
                scene_queue.Items().front().id_ == scene_commands.id_) {
                const auto entry = scene_queue.Items().front().entry_;
                deck.RequestNextScene(scene_commands.id_,
                                      entry ? entry->transition_seconds_ : scene_commands.duration_,
                                      entry ? entry->quantization_ : scene_commands.mode_);
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
            const auto& transition = music_frame.audio_.transition_;
            const auto status = std::pair{transition.id_, transition.state_};
            if (status != audio_transition_status) {
                audio_transition_status = status;
                // Borrowed SDL name is checked and used only in this host log call.
                const auto* driver = SDL_GetCurrentAudioDriver();
                SDL_Log("scene audio id=%llu phase=%u consumed=%llu elapsed=%llu previous=%.4f "
                        "incoming=%.4f driver=%s error=%s",
                        static_cast<unsigned long long>(transition.id_),
                        static_cast<unsigned>(transition.state_),
                        static_cast<unsigned long long>(music_frame.audio_.consumed_frames_),
                        static_cast<unsigned long long>(transition.elapsed_frames_),
                        transition.previous_seconds_, transition.incoming_seconds_,
                        driver ? driver : "unknown", transition.error_.c_str());
            }
            const auto scene_audio_sample = scene_audio.Poll(
                    deck, music_frame.audio_,
                    [&](const auto& source, double duration, bool paused) {
                        return music.BeginSoundtrackTransition(source, duration, paused);
                    },
                    [&](std::uint64_t id) { return music.CancelSoundtrackTransition(id); });
            const auto frame =
                    deck.Tick(seconds, false, render_quality, *renderer, music_frame.inputs_,
                              music_frame.playback_, scene_queue, scene_audio_sample);
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
                    if (frame.audio_synchronized_)
                        music.AdoptSoundtrack(*track, music_frame.audio_.transition_.id_);
                    else {
                        music.Open(*track);
                        music.Apply({deck.Current().Paused(), frame.entry_seconds_});
                        if (const auto sample = music.Frame().playback_) deck.AdoptMedia(*sample);
                    }
                }
                SDL_Log("scene committed transition=%llu audio_synchronized=%d width=%u height=%u",
                        static_cast<unsigned long long>(frame.transition_id_),
                        frame.audio_synchronized_, deck.Current().Canvas().width_,
                        deck.Current().Canvas().height_);
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
                if (output.rejected_event_total_)
                    status << " event_rejections=" << output.rejected_event_total_;
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
