#include <GLES3/gl3.h>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace rhythm::validation {
void VerifyVideoUploadPixels(render::Renderer& renderer) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "video.upload.gles";
    document.canvas_ = {16, 16};
    const assets::AssetId id{std::string(64, 'a')};
    document.nodes_ = {registry.MakeNode(1, "texture.video"),
                       registry.MakeNode(2, "output.texture")};
    document.nodes_[0].properties_["asset"] = id;
    document.edges_ = {{1, 1, 2, "source"}};
    document.output_ = 2;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    runtime::Runtime runtime;
    std::uint64_t texture_bytes = 0;
    for (int sample = 0; sample < 5; ++sample) {
        const int source_frame = sample < 3 ? sample : 2;
        const double gain = sample < 3 ? 1.0 : sample == 3 ? 0.5 : 0.25;
        auto frame = std::make_shared<media::VideoFrame>();
        frame->info_.width_ = frame->info_.height_ = 1;
        frame->rgba_ = {static_cast<std::uint8_t>(source_frame * 60), 160, 80, 128};
        frame->seconds_ = sample;
        runtime::FrameContext context{double(sample), 1, {16, 16}, true};
        context.videos_ = {{1, id, frame, static_cast<std::uint64_t>(source_frame + 1), 1, gain}};
        for (int repeat = 0; repeat < 4; ++repeat) {
            renderer.BeginFrame();
            const auto result = runtime.Evaluate(plan, context, renderer);
            render::DrawList draw;
            draw.width_ = draw.height_ = 16;
            draw.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
            draw.indices_ = {0, 1, 2, 0, 2, 3};
            draw.commands_ = {{result.final_, 0, 6, {0, 0, 16, 16}}};
            renderer.Submit({}, draw, 0x000000ff);
            renderer.EndFrame();
        }
        std::array<std::uint8_t, 16 * 16 * 4> pixels{};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("video upload GLES readback");
        for (std::size_t index = 0; index < pixels.size(); index += 4)
            if (std::abs(int(pixels[index]) - source_frame * 30 * gain) > 2 ||
                std::abs(int(pixels[index + 1]) - 80 * gain) > 2 ||
                std::abs(int(pixels[index + 2]) - 40 * gain) > 2)
                throw std::runtime_error("video upload GLES pixels or alpha");
        if (sample == 0) texture_bytes = renderer.Stats().texture_bytes_;
        if (texture_bytes != renderer.Stats().texture_bytes_)
            throw std::runtime_error("video upload texture allocation changed");
    }
    std::cout << "Video upload: GLES frame replacement, alpha, clip fades and stable allocation "
                 "passed\n";
}
}  // namespace rhythm::validation
