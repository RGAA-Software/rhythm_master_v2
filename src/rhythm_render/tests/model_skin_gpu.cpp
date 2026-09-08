#include <picosha2.h>

#include <array>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/model_assets/prepare.h"
#include "rhythm/runtime/runtime.h"
#include "skin_fixture.h"

namespace rhythm::validation {
void VerifyModelSkin(render::Renderer& renderer) {
    const auto fixture = model_import::test::SkinFixture();
    const auto binary = model_import::test::Glb(fixture.json_, fixture.extra_);
    const std::string bytes(binary.begin(), binary.end());
    const project::PackagedAsset asset{{{picosha2::hash256_hex_string(bytes.begin(), bytes.end())},
                                        bytes.size(),
                                        "model/gltf-binary"},
                                       bytes};
    graph::Registry registry;
    graph::Document document;
    document.id_ = "skin.package.gpu";
    document.nodes_ = {
            registry.MakeNode(1, "geometry.glb"),   registry.MakeNode(2, "geometry.animate"),
            registry.MakeNode(3, "scene.instance"), registry.MakeNode(4, "scene.render"),
            registry.MakeNode(5, "output.texture"), registry.MakeNode(6, "material.unlit")};
    document.nodes_[0].properties_["asset"] = asset.record_.id_;
    document.nodes_[5].properties_["color_a"] = graph::Color{1, 0, 0, 1};
    document.edges_ = {{1, 1, 2, "geometry"},
                       {2, 2, 3, "geometry"},
                       {3, 3, 4, "scene"},
                       {4, 4, 5, "source"},
                       {5, 6, 3, "material"}};
    document.output_ = 5;
    const std::array assets{asset};
    const auto package =
            project::DecodePackage(project::EncodePackage(document, "Skin fixture", assets));
    runtime::FrameContext context{0, 0, {64, 64}};
    context.resources_ = model_assets::Prepare(package.program_, package.assets_);
    runtime::Runtime runtime;
    std::array<render::ReadbackImage, 4> images;
    const std::array times{0.0, 0.25, 0.0, 0.25};
    for (std::size_t frame = 0; frame < times.size(); ++frame) {
        context.seconds_ = times[frame];
        renderer.BeginFrame();
        const auto output = runtime.Evaluate(package.program_, context, renderer);
        auto ticket = renderer.RequestReadback(output.final_);
        renderer.EndFrame();
        bool complete = false;
        for (int i = 0; i < 32; ++i) {
            if (auto image = ticket.Poll()) {
                images[frame] = std::move(*image);
                complete = true;
                break;
            }
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        if (!complete || renderer.Stats().live_meshes_ != 1)
            throw std::runtime_error("skin.package_resource");
    }
    const auto centroid = [](const render::ReadbackImage& image) {
        double sum = 0, count = 0;
        for (std::size_t i = 0; i < image.rgba_.size(); i += 4)
            if (image.rgba_[i] > 200 && image.rgba_[i + 1] < 20) {
                sum += (i / 4) % 64;
                ++count;
            }
        if (count < 50) throw std::runtime_error("skin.package_empty");
        return sum / count;
    };
    const auto shift = centroid(images[1]) - centroid(images[0]);
    if (shift < 4 || images[0].rgba_ != images[2].rgba_ || images[1].rgba_ != images[3].rgba_)
        throw std::runtime_error("skin.package_seek");
    std::cout << "GLB skin: package -> bind hierarchy -> animation -> GPU, seek shift=" << shift
              << ", one shared mesh passed\n";
}
}  // namespace rhythm::validation
