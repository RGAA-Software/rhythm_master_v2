#include <picosha2.h>

#include <array>
#include <iostream>
#include <stdexcept>

#include "fixtures.h"
#include "gpu_execution_probe.h"
#include "rhythm/model_assets/prepare.h"
#include "rhythm/runtime/runtime.h"

namespace rhythm::validation {
void VerifyModelImages(render::Renderer& renderer) {
    using namespace model_import::test;
    auto fixture = EmbeddedFixture();
    fixture.json_ = Replace(fixture.json_, "[1,0,0]", "[0,0,0]");
    fixture.json_ = Replace(fixture.json_, "[0,2,0]", "[0,0,0]");
    fixture.json_ = Replace(fixture.json_, "[0.2,0.4,0.6,1]", "[1,1,1,1]");
    const auto binary = Glb(fixture.json_, fixture.extra_);
    const std::string bytes(binary.begin(), binary.end());
    project::PackagedAsset asset{{{picosha2::hash256_hex_string(bytes.begin(), bytes.end())},
                                  bytes.size(),
                                  "model/gltf-binary"},
                                 bytes};
    graph::Registry registry;
    graph::Document document;
    document.id_ = "model.image.gpu";
    document.nodes_ = {registry.MakeNode(1, "geometry.glb"), registry.MakeNode(2, "scene.instance"),
                       registry.MakeNode(3, "scene.render"),
                       registry.MakeNode(4, "output.texture")};
    document.nodes_[0].properties_["asset"] = asset.record_.id_;
    document.edges_ = {{1, 1, 2, "geometry"}, {2, 2, 3, "scene"}, {3, 3, 4, "source"}};
    document.output_ = 4;
    const std::array assets{asset};
    const auto package =
            project::DecodePackage(project::EncodePackage(document, "Image fixture", assets));
    const auto resources = model_assets::Prepare(package.program_, package.assets_);
    runtime::FrameContext frame{0, 0, {32, 32}};
    frame.resources_ = resources;
    runtime::Runtime runtime;
    renderer.BeginFrame();
    const auto result = runtime.Evaluate(package.program_, frame, renderer);
    auto ticket = renderer.RequestReadback(result.final_);
    renderer.EndFrame();
    std::optional<render::ReadbackImage> image;
    for (int i = 0; i < 32; ++i) {
        image = ticket.Poll();
        if (image) break;
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    if (!image) throw std::runtime_error("model_image.readback_timeout");
    constexpr auto kCenter = (16 * 32 + 16) * 4;
    const auto& pixels = image->rgba_;
    // The four colored image corners interpolate through UV0 at the triangle center.
    // Missing textures produce white; missing hierarchy/mesh produces transparent black.
    if (pixels[kCenter] < 45 || pixels[kCenter] > 85 || pixels[kCenter + 1] < 50 ||
        pixels[kCenter + 1] > 90 || pixels[kCenter + 2] < 65 || pixels[kCenter + 2] > 110 ||
        pixels[kCenter + 3] != 255)
        throw std::runtime_error(
                "model_image.packaged_texture_pixel." + std::to_string(pixels[kCenter]) + "." +
                std::to_string(pixels[kCenter + 1]) + "." + std::to_string(pixels[kCenter + 2]));
    std::cout << "GLB images: packaged asset -> FFmpeg -> runtime -> GPU pixel "
              << int(pixels[kCenter]) << ',' << int(pixels[kCenter + 1]) << ','
              << int(pixels[kCenter + 2]) << " passed\n";
}
}  // namespace rhythm::validation
