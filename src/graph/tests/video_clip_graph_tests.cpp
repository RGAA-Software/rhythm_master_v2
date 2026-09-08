#include <iostream>
#include <stdexcept>

#include "rhythm/graph/compiler.h"
#include "rhythm/graph/video_clip.h"

int main() {
    using namespace rhythm;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("clip.graph_contract");
        };
        graph::Registry registry;
        graph::Document document;
        document.id_ = "video.clips";
        document.nodes_ = {registry.MakeNode(1, "texture.video_clip"),
                           registry.MakeNode(2, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        auto& node = document.nodes_[0];
        node.properties_["asset"] = assets::AssetId{std::string(64, 'a')};
        node.properties_["clip_start"] = 2.0;
        node.properties_["clip_duration"] = 4.0;
        node.properties_["source_in"] = 0.2;
        node.properties_["source_out"] = 0.8;
        node.properties_["clip_end"] = 2.0;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        check(plan.instructions_[0].operation_ == graph::Operation::kTextureVideo);
        check(graph::DescribeVideoClip(node).Sample(2).source_seconds_ == 0.2);
        node.properties_["source_in"] = 0.9;
        check(!registry.ValidateNode(node).empty());
        check(std::holds_alternative<std::vector<graph::Diagnostic>>(
                graph::Compile(document, registry)));
        node.properties_["source_in"] = graph::Color{};
        check(!registry.ValidateNode(node).empty());
        node = registry.MakeNode(1, "texture.video_clip");
        node.properties_["clip_start"] = 604800.0;
        check(!registry.ValidateNode(node).empty());
        check(registry.ValidateNode(registry.MakeNode(3, "texture.video")).empty());
        std::cout << "Video clips: graph trim validation, compile and legacy source passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
