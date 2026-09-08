#include <picosha2.h>

#include <array>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "morph_fixture.h"
#include "rhythm/model_assets/prepare.h"
#include "rhythm/runtime/runtime.h"

namespace rhythm::validation {
void VerifyModelMorph(render::Renderer& renderer) {
    const auto fixture = model_import::test::MorphFixture();
    const auto binary = model_import::test::Glb(fixture.json_, fixture.extra_);
    const std::string bytes(binary.begin(), binary.end());
    const project::PackagedAsset asset{{{picosha2::hash256_hex_string(bytes.begin(), bytes.end())},
                                        bytes.size(),
                                        "model/gltf-binary"},
                                       bytes};
    graph::Registry registry;
    graph::Document document;
    document.id_ = "morph.package.gpu";
    document.nodes_ = {
            registry.MakeNode(1, "geometry.glb"),   registry.MakeNode(2, "geometry.animate"),
            registry.MakeNode(3, "scene.instance"), registry.MakeNode(4, "scene.render"),
            registry.MakeNode(5, "output.texture"), registry.MakeNode(6, "material.unlit"),
            registry.MakeNode(7, "geometry.morph"), registry.MakeNode(8, "audio.feature")};
    document.nodes_[0].properties_["asset"] = asset.record_.id_;
    document.nodes_[5].properties_["color_a"] = graph::Color{1, 0, 0, 1};
    document.edges_ = {{1, 1, 2, "geometry"},      {2, 2, 3, "geometry"}, {3, 3, 4, "scene"},
                       {4, 4, 5, "source"},        {5, 6, 3, "material"}, {6, 2, 7, "geometry"},
                       {7, 8, 7, "morph_weight_1"}};
    document.output_ = 5;
    const std::array assets{asset};
    std::array<double, 3> origins{};
    for (int mode = 0; mode < 3; ++mode) {
        document.edges_[1].from_ = mode == 0 ? 1 : mode == 1 ? 2 : 7;
        document.nodes_[1].properties_["animation_clip"] = mode == 2 ? 1.0 : 0.0;
        const auto package =
                project::DecodePackage(project::EncodePackage(document, "Morph fixture", assets));
        runtime::FrameContext context{0, 0, {64, 64}};
        context.resources_ = model_assets::Prepare(package.program_, package.assets_);
        context.external_.audio_.emplace();
        context.external_.audio_->valid_ = true;
        context.external_.audio_->sample_rate_ = 48000;
        context.external_.audio_->generation_ = 1;
        runtime::Runtime runtime;
        std::array<render::ReadbackImage, 4> images;
        for (std::size_t frame = 0; frame < images.size(); ++frame) {
            context.seconds_ = mode == 2 ? 0.25 : double(frame % 2);
            context.external_.audio_->loudness_ = float(frame % 2);
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
                throw std::runtime_error("morph.package_resource");
        }
        const auto centroid = [](const render::ReadbackImage& image) {
            double sum = 0, count = 0;
            for (std::size_t i = 0; i < image.rgba_.size(); i += 4)
                if (image.rgba_[i] > 200 && image.rgba_[i + 1] < 20) {
                    sum += (i / 4) % 64;
                    ++count;
                }
            if (count < 50) throw std::runtime_error("morph.package_empty");
            return sum / count;
        };
        origins[mode] = centroid(images[0]);
        const auto shift = centroid(images[1]) - origins[mode];
        if ((mode == 0 ? shift != 0 : shift < 3) || images[0].rgba_ != images[2].rgba_ ||
            images[1].rgba_ != images[3].rgba_)
            throw std::runtime_error("morph.package_seek");
        std::cout << "GLB morph: mode=" << mode << " shift=" << shift
                  << ", package, skin, defaults/animation/music and repeated seeks passed\n";
    }
    if (origins[0] - origins[1] < 1 || origins[2] - origins[1] < 3)
        throw std::runtime_error("morph.package_defaults_or_skin_pose");
}
}  // namespace rhythm::validation
