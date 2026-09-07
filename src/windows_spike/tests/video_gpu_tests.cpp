#include <bgfx/bgfx.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/platform/host.h"
#include "rhythm/runtime/runtime.h"
#include "rhythm/video_sources/streams.h"

int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        if (argc != 3) throw std::invalid_argument("video fixtures and capture path required");
        const std::filesystem::path output(argv[2]);
        assets::Store store(output / "assets");
        const auto asset =
                store.Import(std::filesystem::path(argv[1]) / std::filesystem::path(u8"视频.mkv"),
                             "video/x-matroska");
        const std::vector<project::PackagedAsset> packaged{{asset, store.Read(asset)}};
        graph::Registry registry;
        graph::Document document;
        document.id_ = "video.gpu";
        document.canvas_ = {128, 128};
        document.nodes_ = {
                registry.MakeNode(1, "texture.video"), registry.MakeNode(2, "texture.video"),
                registry.MakeNode(3, "texture.blend"), registry.MakeNode(4, "output.texture")};
        for (std::size_t index = 0; index < 2; ++index) {
            document.nodes_[index].properties_["asset"] = asset.id_;
            document.nodes_[index].properties_["image_fill"] = 1.0;
        }
        document.nodes_[1].properties_["video_speed"] = 0.0;
        document.nodes_[1].properties_["video_offset"] = 0.8;
        document.edges_ = {{1, 1, 3, "a"}, {2, 2, 3, "b"}, {3, 3, 4, "source"}};
        document.output_ = 4;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        const auto resources = prepared_assets::Prepare(plan, packaged);
        platform::Host host(true);
        auto renderer = host.CreateRenderer();
        video_sources::Streams streams;
        runtime::Runtime runtime;
        constexpr std::array kTimes{0.0, 0.4, 1.2, 2.4, 0.2};
        for (std::size_t scenario = 0; scenario < kTimes.size(); ++scenario) {
            const auto seconds = kTimes[scenario];
            const std::uint64_t generation = scenario == 4 ? 2 : 1;
            runtime::FrameContext context{seconds, generation, {128, 128}, true};
            context.videos_ = streams.Resolve(plan, *resources, seconds, generation);
            if (context.videos_.size() != 2)
                throw std::runtime_error("offline video demand omitted a source");
            for (const auto& sample : context.videos_) {
                const auto expected = sample.node_ == 1 ? std::fmod(seconds, 2.0) : 0.8;
                if (std::abs(sample.frame_->seconds_ - expected) >= 0.001)
                    throw std::runtime_error("offline video demand returned a stale timestamp");
            }
            const auto capture = (output / ("video-" + std::to_string(scenario))).string();
            for (int frame = 0; frame < 8; ++frame) {
                renderer.BeginFrame();
                // A separate preview may already have submitted another pass.
                // Unrelated texture uploads must still be allowed in this frame.
                render::DrawList empty;
                empty.width_ = empty.height_ = 128;
                renderer.Submit({}, empty);
                const auto rendered = runtime.Evaluate(plan, context, renderer);
                render::DrawList draw;
                draw.width_ = draw.height_ = 128;
                draw.vertices_ = {{0, 0, 0, 0}, {128, 0, 1, 0}, {128, 128, 1, 1}, {0, 128, 0, 1}};
                draw.indices_ = {0, 1, 2, 0, 2, 3};
                draw.commands_ = {{rendered.final_, 0, 6, {0, 0, 128, 128}}};
                renderer.Submit({}, draw);
                if (frame == 3) bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
                renderer.EndFrame();
            }
        }
        std::cout << "Video D3D11 capture: independent times, loop and seek\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
