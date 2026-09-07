#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "rhythm/runtime/runtime.h"

namespace rhythm::validation {
void VerifyTextureReusePixels(render::Renderer& renderer) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = "gpu.texture_reuse";
    document.nodes_ = {registry.MakeNode(1, "core.time"), registry.MakeNode(2, "texture.gradient"),
                       registry.MakeNode(3, "texture.blur")};
    document.nodes_[2].properties_["blur_radius"] = 0.0;
    document.edges_ = {{1, 1, 2, "amount"}, {2, 2, 3, "source"}};
    for (graph::NodeId id = 4; id <= 23; ++id) {
        auto node = registry.MakeNode(id, "texture.color_adjust");
        node.properties_["exposure"] = id % 2 ? -0.15 : 0.15;
        document.nodes_.push_back(std::move(node));
        document.edges_.push_back({id, id - 1, id, "source"});
    }
    document.nodes_.push_back(registry.MakeNode(24, "texture.feedback"));
    document.nodes_.push_back(registry.MakeNode(25, "texture.blend"));
    document.nodes_.push_back(registry.MakeNode(26, "output.texture"));
    document.edges_.insert(
            document.edges_.end(),
            {{24, 23, 25, "a"}, {25, 24, 25, "b"}, {26, 25, 24, "source"}, {27, 25, 26, "source"}});
    document.output_ = 26;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    std::vector<std::array<std::uint8_t, 16 * 16 * 4>> baseline;
    std::array<std::uint64_t, 2> bytes{};
    for (int mode = 0; mode < 2; ++mode) {
        runtime::Runtime runtime;
        for (int index = 0; index < 32; ++index) {
            runtime::FrameContext frame;
            frame.extent_ = {16, 16};
            frame.seconds_ = std::min(index, 16) / 60.0;
            frame.advance_state_ = index < 16;
            if (mode) frame.retained_textures_ = std::vector<graph::NodeId>{};
            renderer.BeginFrame();
            const auto output = runtime.Evaluate(plan, frame, renderer);
            render::DrawList draw;
            draw.width_ = draw.height_ = 16;
            draw.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
            draw.indices_ = {0, 1, 2, 0, 2, 3};
            draw.commands_ = {{output.final_, 0, 6, {0, 0, 16, 16}}};
            renderer.Submit({}, draw);
            renderer.EndFrame();
            bytes[mode] = std::max(bytes[mode], renderer.Stats().texture_bytes_);
            std::array<std::uint8_t, 16 * 16 * 4> pixels{};
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("reuse.gles_readback");
            if (!mode)
                baseline.push_back(pixels);
            else if (baseline[index] != pixels)
                throw std::runtime_error("reuse.gles_pixels");
        }
    }
    if (bytes[1] >= bytes[0] / 2) throw std::runtime_error("reuse.gles_memory");
    std::cout << "GLES texture reuse: 32 identical frames including feedback, zero blur and pause; "
              << bytes[0] << " -> " << bytes[1] << " bytes\n";
}
}  // namespace rhythm::validation
