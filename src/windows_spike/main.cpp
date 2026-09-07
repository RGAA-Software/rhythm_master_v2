#include <chrono>
#include <iostream>
#include <optional>

#include "rhythm/platform/host.h"
#include "rhythm/runtime/frame_clock.h"
#include "rhythm/studio/studio.h"
#ifdef RHYTHM_HAS_EXPORT
#include "rhythm/export/jobs.h"
#endif

// The process-entry argv pointers are mandated by the C++ host ABI and are not stored.
#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    try {
        bool smoke = false;
        std::optional<std::filesystem::path> requested_project;
        std::optional<std::filesystem::path> requested_audio;
        std::optional<std::filesystem::path> export_job;
        for (int index = 1; index < argc; ++index) {
            const std::filesystem::path argument(argv[index]);
            if (argument == "--smoke")
                smoke = true;
            else if (argument == "--project" && index + 1 < argc)
                requested_project = std::filesystem::path(argv[++index]);
            else if (argument == "--audio" && index + 1 < argc)
                requested_audio = std::filesystem::path(argv[++index]);
#ifdef RHYTHM_HAS_EXPORT
            else if (argument == "--export-job" && index + 1 < argc)
                export_job = std::filesystem::path(argv[++index]);
#endif
            else
                throw std::invalid_argument(
                        "Usage: rhythm_master [--project directory] [--audio file] [--smoke]");
        }
#ifdef RHYTHM_HAS_EXPORT
        if (export_job) {
            if (argc != 3)
                throw std::invalid_argument("export job requires an exclusive entry branch");
            return rhythm::exporting::RunExportJob(*export_job);
        }
#endif
        rhythm::platform::Host host(smoke);
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        const auto project = requested_project.value_or(
                smoke ? host.ResourceDirectory() / "smoke-project.rhythmproj"
                      : host.DataDirectory() / "Projects" / "Untitled.rhythmproj");
        if (requested_project && !std::filesystem::exists(project / "CURRENT"))
            throw std::invalid_argument("project.current_missing");
        rhythm::studio::Studio studio(host.ResourceDirectory(), project);
        if (requested_audio) studio.LoadAudioFile(*requested_audio, smoke ? 0.0f : 1.0f);
        const auto start = std::chrono::steady_clock::now();
        rhythm::runtime::FrameClock clock;
        for (int frame = 0; host.Poll() && (!smoke || frame < 30); ++frame) {
            const auto elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const auto playback_seconds = clock.Advance(elapsed, host.IsSuspended());
            studio.SetSuspended(host.IsSuspended());
            if (host.IsSuspended()) continue;
            host.BeginUi();
            renderer.BeginFrame();
            const auto seconds = smoke ? frame / 60.0 : playback_seconds;
            studio.Frame(host, renderer, seconds);
            auto draw = host.EndUi();
            if (draw.width_ > 0 && draw.height_ > 0) renderer.Submit({}, draw, 0x111822ff);
            renderer.EndFrame();
        }
        if (smoke && (!studio.HasValidPlan() || renderer.Stats().passes_ < 2))
            throw std::runtime_error("studio.smoke_no_graph");
        const auto status = studio.Status();
        std::cout << "visible_nodes=" << status.visible_nodes_ << "/" << status.authored_nodes_
                  << " viewers=" << status.viewers_
                  << " inline_previews=" << status.inline_previews_ << '\n';
        if (smoke && !requested_project &&
            (status.visible_nodes_ != status.authored_nodes_ || status.viewers_ != 5 ||
             status.inline_previews_ != 5))
            throw std::runtime_error("studio.smoke_layout");
        std::cout << "gpu_frames=" << renderer.Stats().frame_ << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
