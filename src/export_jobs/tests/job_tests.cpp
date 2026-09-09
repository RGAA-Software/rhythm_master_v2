#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/export/jobs.h"
#include "rhythm/media/video_decoder.h"
#include "rhythm/platform/host.h"
#include "rhythm/project/store.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
bool Busy(rhythm::exporting::JobState state) {
    using rhythm::exporting::JobState;
    return state == JobState::kPreparing || state == JobState::kRendering ||
           state == JobState::kPublishing;
}
std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    using exporting::JobState;
    try {
        Require(argc == 5, "job_tests executable template music output");
        const auto root = std::filesystem::path(argv[4]) /
                          (std::filesystem::path(u8"音画 导出 ") += std::to_string(
                                   std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        const auto project = project::PrepareTemplate(argv[2], root / "assets").snapshot_;
        exporting::ExportSettings settings;
        settings.encoding_ = {320, 180, 30, 4000000, media::VideoCodec::kH264, true};
        settings.frames_ = 120;
        settings.music_ = argv[3];
        platform::Host host(true);
        auto renderer = host.CreateRenderer();
        exporting::ExportJobs jobs;
        std::uint64_t responsive_frames = 0;
        const auto wait = [&](bool cancel) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
            bool canceled = false;
            for (;;) {
                const auto state = jobs.Snapshot();
                if (!Busy(state.state_)) return state;
                if (cancel && state.progress_.completed_frames_ > 0 && !canceled) {
                    jobs.Cancel();
                    canceled = true;
                }
                renderer.BeginFrame();
                render::DrawList draw;
                draw.width_ = 1280;
                draw.height_ = 800;
                renderer.Submit({}, draw, 0x132335ff);
                renderer.EndFrame();
                ++responsive_frames;
                if (std::chrono::steady_clock::now() >= deadline)
                    throw std::runtime_error("job test timeout");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        };
        const auto output = root / std::filesystem::path(u8"完成作品.mp4");
        Require(jobs.Start(argv[1], project, root / "assets", settings, output), "submit export");
        Require(!jobs.Start(argv[1], project, root / "assets", settings, root / "duplicate.mp4"),
                "busy export admits another job");
        const auto complete = wait(false);
        if (complete.state_ != JobState::kComplete) throw std::runtime_error(complete.error_);
        Require(complete.phase_ == exporting::JobPhase::kComplete &&
                        complete.progress_.completed_frames_ == 120 && responsive_frames > 5,
                "background export did not preserve parent frame progress");
        {
            media::VideoDecoder decoder(output);
            int frames = 0;
            while (decoder.Read()) ++frames;
            Require(frames == 120, "published export is incomplete");
        }
        const auto original = Read(output);
        Require(jobs.Start(argv[1], project, root / "assets", settings, output),
                "resubmit existing path");
        const auto existing = wait(false);
        Require(existing.state_ == JobState::kFailed &&
                        existing.phase_ == exporting::JobPhase::kPreparing &&
                        Read(output) == original,
                "existing output was overwritten");
        settings.frames_ = 108000;
        const auto canceled_path = root / "canceled.mp4";
        Require(jobs.Start(argv[1], project, root / "assets", settings, canceled_path),
                "submit canceled job");
        Require(wait(true).state_ == JobState::kCanceled && !std::filesystem::exists(canceled_path),
                "canceled export was published");
        settings.frames_ = 120;
        settings.encoding_.bitrate_ = 0;
        const auto failed_path = root / "failed.mp4";
        Require(jobs.Start(argv[1], project, root / "assets", settings, failed_path),
                "submit failed worker");
        const auto failed = wait(false);
        Require(failed.state_ == JobState::kFailed && !failed.error_.empty() &&
                        failed.phase_ == exporting::JobPhase::kRendering &&
                        !std::filesystem::exists(failed_path),
                "failed child was reported as success");
        for (const auto& entry : std::filesystem::directory_iterator(root))
            Require(!entry.path().filename().u8string().starts_with(u8".rhythm-export-"),
                    "job staging leaked");
        std::cout << "hidden export worker, parent frames=" << responsive_frames
                  << ", Unicode publication, no overwrite, cancellation and child errors passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
