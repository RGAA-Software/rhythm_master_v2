#include "rhythm/export/render_export.h"

#include <chrono>
#include <cmath>
#include <deque>
#include <stdexcept>

#include "encoding_queue.h"
#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/runtime/runtime.h"
#include "rhythm/video_sources/streams.h"
#include "soundtrack.h"

namespace rhythm::exporting {
namespace {
render::DrawList OutputPass(render::TextureHandle source, render::Extent extent) {
    render::DrawList draw;
    draw.width_ = extent.width_;
    draw.height_ = extent.height_;
    draw.vertices_ = {{0, 0, 0, 0},
                      {draw.width_, 0, 1, 0},
                      {draw.width_, draw.height_, 1, 1},
                      {0, draw.height_, 0, 1}};
    draw.indices_ = {0, 1, 2, 0, 2, 3};
    draw.commands_ = {{source, 0, 6, {0, 0, draw.width_, draw.height_}}};
    return draw;
}
struct PendingFrame {
    render::Readback readback_{};
    std::vector<float> audio_{};
};
}  // namespace
void RenderExport(const project::RuntimePackage& package, const ExportSettings& settings,
                  const std::filesystem::path& staging, render::Renderer& renderer,
                  const std::function<void(ExportProgress)>& progress, std::stop_token stop) {
    const auto& encoding = settings.encoding_;
    if (stop.stop_requested()) throw std::runtime_error("export.canceled");
    if (!settings.frames_ || settings.frames_ > encoding.fps_ * 3600ULL ||
        !graph::ValidCanvas({encoding.width_, encoding.height_}) ||
        (encoding.fps_ != 24 && encoding.fps_ != 25 && encoding.fps_ != 30 &&
         encoding.fps_ != 60) ||
        encoding.width_ % 2 || encoding.height_ % 2 || !std::isfinite(settings.gain_) ||
        settings.gain_ < 0 || settings.gain_ > 1 || (!encoding.audio_ && settings.music_))
        throw std::invalid_argument("export.settings");
    if (!renderer.SupportsReadback()) throw std::runtime_error("export.readback_unavailable");
    const render::Extent extent{static_cast<std::uint16_t>(encoding.width_),
                                static_cast<std::uint16_t>(encoding.height_)};
    const auto resources = prepared_assets::Prepare(package.program_, package.assets_, stop);
    detail::Soundtrack soundtrack(settings.music_, settings.gain_, stop);
    video_sources::Streams videos;
    runtime::Runtime runtime;
    auto target = renderer.CreateTexture(extent);
    detail::EncodingQueue writer(staging, encoding, stop);
    std::deque<PendingFrame> pending;
    std::uint64_t rendered = 0, submitted = 0, completed = 0;
    auto last_progress = std::chrono::steady_clock::now();
    const auto notify = [&] {
        if (progress) progress({completed, settings.frames_, renderer.Stats().texture_bytes_});
    };
    const auto report = [&] {
        const auto encoded = writer.Completed();
        while (completed < encoded) {
            if (stop.stop_requested()) throw std::runtime_error("export.canceled");
            ++completed;
            notify();
        }
    };
    notify();
    while (submitted < settings.frames_) {
        if (stop.stop_requested()) throw std::runtime_error("export.canceled");
        // The common runtime evaluates exactly once per output time. Frames
        // used only to advance GPU completion never advance simulation/audio.
        if (rendered < settings.frames_ && pending.size() < 3) {
            const auto seconds = static_cast<double>(rendered) / encoding.fps_;
            runtime::FrameContext context{seconds, 1, extent, false};
            context.resources_ = resources->models_;
            context.images_ = resources->images_;
            context.videos_ = videos.Resolve(package.program_, *resources, seconds, 1, stop);
            context.external_.audio_ = soundtrack.Features();
            context.retained_textures_ = std::vector<graph::NodeId>{};
            renderer.BeginFrame();
            render::Readback readback;
            try {
                const auto result =
                        runtime.Evaluate(package.program_, std::move(context), renderer);
                renderer.Submit(target.Handle(), OutputPass(result.final_, extent), 0x000000ff);
                readback = renderer.RequestReadback(target.Handle());
            } catch (...) {
                renderer.EndFrame();
                throw;
            }
            renderer.EndFrame();
            auto audio = encoding.audio_ ? soundtrack.Next(48000 / encoding.fps_, stop)
                                         : std::vector<float>{};
            pending.push_back({std::move(readback), std::move(audio)});
            ++rendered;
        } else {
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        while (!pending.empty()) {
            if (stop.stop_requested()) throw std::runtime_error("export.canceled");
            auto image = pending.front().readback_.Poll();
            if (!image) break;
            writer.Push(std::move(*image), std::move(pending.front().audio_));
            pending.pop_front();
            ++submitted;
            last_progress = std::chrono::steady_clock::now();
            report();
        }
        if (std::chrono::steady_clock::now() - last_progress > std::chrono::seconds(30))
            throw std::runtime_error("export.gpu_timeout");
    }
    writer.Finish();
    report();
}
}  // namespace rhythm::exporting
