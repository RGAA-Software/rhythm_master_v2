#include <iostream>
#include <stdexcept>

#include "rhythm/project/package.h"
#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "fxaa.publication";
        document.nodes_ = {
                registry.MakeNode(1, "scalar.constant"), registry.MakeNode(2, "texture.gradient"),
                registry.MakeNode(3, "texture.fxaa"), registry.MakeNode(4, "output.texture")};
        document.edges_ = {{1, 2, 3, "source"}, {2, 1, 3, "fxaa_strength"}, {3, 3, 4, "source"}};
        document.output_ = 4;
        document.nodes_[0].properties_["value"] = 1.0;
        document.nodes_[2].properties_["fxaa_span"] = 6.0;
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        for (const auto extent : {render::Extent{64, 32}, render::Extent{32, 64}}) {
            const auto package = project::DecodePackage(project::EncodePackage(document, "FXAA"));
            Check(package.program_.instructions_.size() == 4, "fxaa.publication_count");
            Check(package.program_.instructions_[2].operation_ == graph::Operation::kTextureFxaa &&
                          graph::Scalar(package.program_.instructions_[2].node_, "fxaa_span", 0) ==
                                  6,
                  "fxaa.published_configuration");
            runtime.Reset();
            std::uint64_t stable_bytes = 0;
            for (int frame = 0; frame < 12; ++frame) {
                runtime::FrameContext context{frame / 60.0, 0, extent};
                context.retained_textures_ = std::vector<graph::NodeId>{};
                renderer.BeginFrame();
                const auto result = runtime.Evaluate(package.program_, context, renderer);
                renderer.EndFrame();
                Check(renderer.IsValid(result.final_) && renderer.Precision(result.final_) ==
                                                                 render::TexturePrecision::kUnorm8,
                      "fxaa.runtime_output");
                if (!frame) stable_bytes = renderer.Stats().texture_bytes_;
                Check(renderer.Stats().texture_bytes_ == stable_bytes, "fxaa.texture_growth");
                Check(renderer.Stats().passes_ == (frame ? 0u : 2u), "fxaa.static_cache");
            }
            // Wired signals may exceed the user property's range. The adapter
            // clamps them before submitting the renderer's strict contract.
            for (const auto strength : {-2.0, 2.0}) {
                document.nodes_[0].properties_["value"] = strength;
                ++document.revision_;
                const auto modified =
                        project::DecodePackage(project::EncodePackage(document, "FXAA"));
                renderer.BeginFrame();
                const auto result = runtime.Evaluate(modified.program_, {1, 0, extent}, renderer);
                renderer.EndFrame();
                Check(renderer.IsValid(result.final_), "fxaa.wired_strength_clamp");
            }
        }
        runtime.Reset();
        Check(renderer.Stats().texture_bytes_ == 0, "fxaa.texture_release");
        document.nodes_[2].properties_["fxaa_reduce_minimum"] = 0.0;
        Check(std::holds_alternative<std::vector<graph::Diagnostic>>(
                      graph::Compile(document, registry)),
              "fxaa.invalid_property_rejected");
        std::cout << "FXAA graph: publication, wired strength, static cache, resize and release "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
