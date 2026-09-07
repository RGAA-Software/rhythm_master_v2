#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "rhythm/platform/host.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"

namespace {
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}
void Report(std::string_view name, std::vector<double> values) {
    std::sort(values.begin(), values.end());
    std::cout << name << "_p50_ms=" << values[values.size() / 2] << ' ' << name
              << "_p95_ms=" << values[values.size() * 95 / 100] << '\n';
}
}  // namespace

// An opt-in benchmark, with the real editor and inline previews enabled. It
// never opens the user's project or starts audio capture. Run without builds
// or other benchmarks in parallel; timings are evidence, not CI assertions.
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc < 4 || argc > 6 || (argc == 6 && std::string_view(argv[5]) != "--retain-textures"))
            throw std::invalid_argument(
                    "editor_benchmark resources template output [audio [--retain-textures]]");
        const std::filesystem::path resources(argv[1]), source(argv[2]), output(argv[3]);
        if (std::filesystem::exists(output))
            throw std::invalid_argument("editor_benchmark.output_must_be_new");
        const auto prepared = project::PrepareTemplate(source, output / "assets");
        project::Save(output, prepared.snapshot_);
        platform::Host host(true);
        host.Resize({1920, 1080});
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(resources, output);
        if (argc == 6) studio.SetTextureReuse(false);
        if (argc >= 5) studio.LoadAudioFile(argv[4], 0);
        std::vector<double> total_times, editor_times, submit_times;
        bool observed_previews = false;
        float peak_rms = 0;
        std::size_t peak_previews = 0, peak_visible = 0, pan_frames = 0;
        std::uint64_t peak_texture_bytes = 0;
        std::uint32_t peak_recycled = 0;
        for (int frame = 0; frame < 720; ++frame) {
            const auto start = Clock::now();
            if (!host.Poll()) throw std::runtime_error("editor_benchmark.closed");
            // Move the pointer over the node canvas to exercise hover processing.
            auto& io = ImGui::GetIO();
            io.AddMousePosEvent(400.0f + (frame >= 300 && frame < 340 ? float(frame - 300) : 0),
                                300.0f);
            if (frame >= 140 && frame < 220 && frame % 8 == 0) io.AddMouseWheelEvent(0, 1);
            if (frame == 300 || frame == 340)
                io.AddMouseButtonEvent(ImGuiMouseButton_Right, frame == 300);
            host.BeginUi();
            renderer.BeginFrame();
            const auto editor_start = Clock::now();
            studio.Frame(host, renderer, frame / 60.0);
            const auto submit_start = Clock::now();
            auto draw = host.EndUi();
            if (draw.width_ && draw.height_) renderer.Submit({}, draw, 0x111822ff);
            renderer.EndFrame();
            const auto end = Clock::now();
            if (frame >= 120) {
                total_times.push_back(Milliseconds(start, end));
                editor_times.push_back(Milliseconds(editor_start, submit_start));
                submit_times.push_back(Milliseconds(submit_start, end));
                peak_recycled = std::max(peak_recycled, studio.Status().recycled_textures_);
                observed_previews |= studio.Status().inline_previews_ != 0;
                peak_rms = std::max(peak_rms, studio.Status().audio_rms_);
                peak_previews = std::max(peak_previews, studio.Status().inline_previews_);
                peak_visible = std::max(peak_visible, studio.Status().visible_nodes_);
                peak_texture_bytes = std::max(peak_texture_bytes, renderer.Stats().texture_bytes_);
                if (frame > 300 && frame < 340 && ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand)
                    ++pan_frames;
            }
        }
        if (!studio.HasValidPlan() || !observed_previews)
            throw std::runtime_error("editor_benchmark.missing_graph_or_previews");
        if (argc >= 5 && peak_rms < 0.01f)
            throw std::runtime_error("editor_benchmark.missing_decoded_audio");
        if (!pan_frames) throw std::runtime_error("editor_benchmark.pan_not_exercised");
        std::cout << "measured_frames=600 window=1920x1080 synthetic_time_hz=60 audio="
                  << (argc >= 5 ? "decoded_file_muted_device" : "silent") << '\n';
        Report("host", total_times);
        Report("editor_and_graph", editor_times);
        Report("ui_submit_and_present", submit_times);
        std::cout << "authored_nodes=" << studio.Status().authored_nodes_
                  << " peak_visible=" << peak_visible << " peak_inline_previews=" << peak_previews
                  << " peak_audio_rms=" << peak_rms << " pan_hand_frames=" << pan_frames
                  << " peak_recycled_targets=" << peak_recycled
                  << " peak_texture_bytes=" << peak_texture_bytes << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
